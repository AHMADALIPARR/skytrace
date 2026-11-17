#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
# Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
"""SKYTRACE deterministic graph/sensor numerical kernel; JSON-lines in/out.
No external dependencies. Inputs are synthetic or externally sourced only when labeled.
"""
import hashlib
import json
import math
import sys
from collections import defaultdict

class GraphError(ValueError):
    pass


def canonical(x):
    return json.dumps(x, sort_keys=True, separators=(',', ':'), ensure_ascii=False, allow_nan=False)


def digest(x):
    return hashlib.sha256(canonical(x).encode('utf-8')).hexdigest()


def finite(x):
    return isinstance(x, (int, float)) and not isinstance(x, bool) and math.isfinite(x)


def require(v, label):
    if not v:
        raise GraphError(label)


def bearing_track(obs):
    """Compute finite differences of unwrapped 2D angular observations (rad, s).
    Values are angular rates, NOT target velocity. Times are UTC-relative seconds.
    """
    samples = obs.get('samples', [])
    require(len(samples) >= 2, 'E_SAMPLES_MIN_TWO')
    times, az, el = [], [], []
    for s in samples:
        t, a, e = (s.get(k) for k in ('t_s', 'az_rad', 'el_rad'))
        require(all(finite(v) for v in (t, a, e)), 'E_NONFINITE')
        require(-math.pi/2 <= e <= math.pi/2, 'E_ELEVATION')
        require(-math.pi <= a <= math.pi, 'E_AZIMUTH')
        times.append(t); az.append(a); el.append(e)
    require(all(b > a for a, b in zip(times, times[1:])), 'E_TIME_ORDER')
    # wrap angle differences to (-pi, pi], but reject ambiguous 180-degree steps
    rates = []
    for i in range(1, len(samples)):
        da = (az[i] - az[i-1] + math.pi) % (2*math.pi) - math.pi
        require(abs(abs(da) - math.pi) > 1e-10, 'E_AZ_UNWRAP_AMBIGUOUS')
        de = el[i] - el[i-1]
        dt = times[i] - times[i-1]
        rates.append({'dt_s': dt, 'az_rad_s': da/dt, 'el_rad_s': de/dt,
                      'angular_norm_rad_s': math.hypot(da/dt, de/dt)})
    return rates


def sensor_signature(obs):
    require(isinstance(obs.get('observation_id'), str) and obs['observation_id'], 'E_OBSERVATION_ID')
    require(isinstance(obs.get('recording_origin_id'), str) and obs['recording_origin_id'], 'E_ORIGIN')
    require(isinstance(obs.get('modality'), str) and obs['modality'], 'E_MODALITY')
    require('calibration_sha256' in obs, 'E_CALIBRATION_STATE')
    calib = obs['calibration_sha256']
    require(calib is None or (isinstance(calib,str) and len(calib)==64 and all(c in '0123456789abcdef' for c in calib)), 'E_CALIBRATION_HASH')
    require(obs.get('range_m') is None or (finite(obs['range_m']) and obs['range_m'] > 0), 'E_RANGE')
    rates = bearing_track(obs)
    # Hash raw measurement tuple and sensor semantics, not rounded derivative values.
    exact = {k: obs[k] for k in ('observation_id','recording_origin_id','modality','calibration_sha256','range_m','samples')}
    return {'observation_id':obs['observation_id'], 'recording_origin_id':obs['recording_origin_id'],
            'semantic_sha256':digest(exact), 'angular_rates':rates,
            'metric_speed_identifiable': False if obs['range_m'] is None else None,
            'identifiability_note': 'unknown range blocks metric speed' if obs['range_m'] is None else 'range alone is insufficient without sensor-platform motion and calibration'}


def analyze(payload):
    require(isinstance(payload, dict), 'E_PAYLOAD_TYPE')
    nodes = payload.get('nodes'); edges = payload.get('edges'); obs = payload.get('observations')
    require(isinstance(nodes,list) and isinstance(edges,list) and isinstance(obs,list), 'E_INPUT_ARRAYS')
    require(len(nodes) <= 20000 and len(edges) <= 100000 and len(obs) <= 20000, 'E_SIZE_LIMIT')
    ids = [n.get('id') for n in nodes]
    require(all(isinstance(i,str) and i for i in ids) and len(ids)==len(set(ids)), 'E_NODE_IDS')
    idset = set(ids); children=defaultdict(list); parents=defaultdict(list)
    allowed={'contains','derived_from','depicts','calibrated_by'}
    edgekeys=set()
    for e in edges:
        p,c,t=e.get('from'),e.get('to'),e.get('type')
        require(p in idset and c in idset and t in allowed, 'E_EDGE_REF')
        require(p != c, 'E_SELF_CYCLE')
        key=(p,c,t)
        require(key not in edgekeys, 'E_DUP_EDGE')
        edgekeys.add(key); children[p].append(c); parents[c].append(p)
    # DAG invariant using Kahn topological sort: robust on joins/diamonds.
    indegree={i:0 for i in ids}
    for p,c,t in edgekeys: indegree[c]+=1
    available=sorted(i for i in ids if indegree[i]==0)
    ordering=[]
    import heapq
    heapq.heapify(available)
    while available:
        u=heapq.heappop(available); ordering.append(u)
        for v in children[u]:
            indegree[v]-=1
            if indegree[v]==0: heapq.heappush(available,v)
    require(len(ordering)==len(ids),'E_CYCLE')
    # Parent-count restriction is only for containment edges, not full provenance DAG.
    containment=defaultdict(set)
    for e in edges:
        if e['type']=='contains': containment[e['to']].add(e['from'])
    require(all(len(s)<=1 for s in containment.values()),'E_MULTIPLE_CONTAINMENT_PARENTS')
    obs_ids=set(); signatures=[]
    for o in obs:
        require(o.get('observation_id') not in obs_ids,'E_DUP_OBSERVATION')
        obs_ids.add(o.get('observation_id'))
        require(o.get('node_id') in idset,'E_OBSERVATION_NODE')
        sig=sensor_signature(o)
        sig['node_id']=o['node_id']; signatures.append(sig)
    # Number of independent recording origins, not number of syndicated reports.
    origins={s['recording_origin_id'] for s in signatures}
    # Recursive DAG dynamic programming: descendant observation IDs, with joins deduplicated.
    local=defaultdict(set)
    for s in signatures: local[s['node_id']].add(s['observation_id'])
    reachable={}
    for u in reversed(ordering):
        seen=set(local[u])
        for v in children[u]: seen.update(reachable[v])
        reachable[u]=seen
    # If same graph relabeled or edge-order shuffled, sensor semantic multiset stays stable.
    sensor_hashes=sorted(s['semantic_sha256'] for s in signatures)
    sensor_summary={'sensor_multiset_sha256':digest(sensor_hashes),
                    'observation_count':len(signatures),
                    'independent_recording_origins':len(origins),
                    'missing_range_count':sum(s['metric_speed_identifiable'] is False for s in signatures),
                    'calibrated_count':sum(o['calibration_sha256'] is not None for o in obs)}
    roots=sorted(u for u in ids if not parents[u])
    return {'status':'ok','graph':{'nodes':len(ids),'edges':len(edges),'roots':roots,
            'root_reachable_observations':{r:len(reachable[r]) for r in roots},
            'acyclic':True,'containment_unique_parent':True},
            'sensor':sensor_summary,'tracks':sorted(signatures,key=lambda x:x['observation_id'])}


def main():
    for line in sys.stdin:
        try:
            response=analyze(json.loads(line))
        except (ValueError,TypeError,KeyError,OverflowError) as ex:
            response={'status':'error','reason':str(ex)}
        print(canonical(response),flush=True)

if __name__=='__main__': main()
