#!/usr/bin/env python3
"""SKYTRACE 2.0 source-attribute ablation on frozen *synthetic* tracks.
Source registry is real public source catalog metadata, but all track/source
associations are SYNTHETIC and NEVER describe archived UAP recordings.
"""
import json, time, hashlib, sys
from pathlib import Path
import numpy as np
from research.parameter_recovery import benchmark as prior

ROOT=Path(__file__).resolve().parents[2]
REGISTRY=json.loads((ROOT/'data/sources.json').read_text())
SOURCES={r['id']:r for r in REGISTRY['sources']}
SOURCE_ORDER=('pursue','aaro','nara')

def attached_metadata(row):
    """Deterministic source assignment independent of target turn-rate and pixels."""
    idx=int(row['id'])
    source_id=SOURCE_ORDER[idx % len(SOURCE_ORDER)]
    source=SOURCES[source_id]
    return {
      'source_id':source_id,
      'source_url':source['url'],
      'source_tier':source['tier'],
      'record_id':f'SYNTHETIC-ONLY-{idx:04d}',
      'recording_origin_id':row['records'][0]['origin'],
      'source_link_status':'synthetic_association',
      'range_telemetry':'unknown',
      'corroboration':'unknown',
      'resolution':'unknown',
      'sensor_modality':'synthetic_position_meter_v1',
      'calibration_id':row['records'][0]['calibration'],
      'sigma_m':row['records'][0]['sigma_m']}

def validate_metadata(m):
    if m['source_id'] not in SOURCES:raise ValueError('unknown source')
    if m['source_url']!=SOURCES[m['source_id']]['url']:raise ValueError('source URL mismatch')
    if m['source_link_status']!='synthetic_association':raise ValueError('unexpected source binding')
    if m['range_telemetry'] not in ('unknown','present','absent'):raise ValueError('invalid range state')
    if m['corroboration'] not in ('unknown','yes','no'):raise ValueError('invalid corroboration')
    return True

def metadata_features(row):
    m=attached_metadata(row);validate_metadata(m)
    # Do not include source record ID, source URL, origin ID, or target leakage.
    return np.array([float(m['source_id']==s) for s in SOURCE_ORDER]+[
      float(m['range_telemetry']=='present'),float(m['range_telemetry']=='absent'),
      float(m['corroboration']=='yes'),float(m['corroboration']=='no'),
      float(m['sigma_m'])],dtype=float)

def features(row, branch):
    records=row['records']; t=prior.temporal_features(records)
    b=prior.braid_features(records,'word')
    m=metadata_features(row)
    if branch=='temporal':return t
    if branch=='temporal_source':return np.r_[t,m]
    if branch=='temporal_braid':return np.r_[t,b]
    if branch=='temporal_source_braid':return np.r_[t,m,b]
    if branch=='source_only':return m
    raise ValueError(branch)

def fit_eval(tr,va,te,branch):
    start=time.perf_counter_ns()
    xt=np.asarray([features(r,branch) for r in tr]);xv=np.asarray([features(r,branch) for r in va])
    xh=np.asarray([features(r,branch) for r in te]);construction_ms=(time.perf_counter_ns()-start)/1e6
    yt=np.array([r['theta_rad_s'] for r in tr]);yv=np.array([r['theta_rad_s'] for r in va]);yh=np.array([r['theta_rad_s'] for r in te])
    start=time.perf_counter_ns()
    fits=[prior.fit_ridge(xt,yt,a) for a in prior.BUDGET_ALPHAS]
    chosen=fits[int(np.argmin([np.mean((f.predict(xv)-yv)**2) for f in fits]))]
    q=float(np.quantile(np.abs(chosen.predict(xv)-yv),.9,method='higher'))
    training_ms=(time.perf_counter_ns()-start)/1e6
    times=[];pred=[]
    for row in te:
        tick=time.perf_counter_ns()
        pred.append(float(chosen.predict(features(row,branch).reshape(1,-1))[0]))
        times.append((time.perf_counter_ns()-tick)/1e6)
    pred=np.asarray(pred)
    return {**prior.metrics(yh,pred), 'interval_90_coverage':float(np.mean(abs(pred-yh)<=q)),
            'interval_90_width_rad_s':2*q,'feature_count':int(xt.shape[1]),
            'alpha':chosen.alpha,'construction_ms_all_splits':construction_ms,
            'training_tuning_ms':training_ms,'p95_inference_ms':float(np.percentile(times,95)),
            'total_measured_ms':construction_ms+training_ms+sum(times)}

def run():
    tr,va,te=prior.generate()
    branches=('temporal','temporal_source','temporal_braid','temporal_source_braid','source_only')
    result={b:fit_eval(tr,va,te,b) for b in branches}
    example=attached_metadata(te[0])
    return {'study':'SKYTRACE 2.0 synthetic source-attribute fusion',
      'source_registry_version':REGISTRY['registry_version'],
      'source_ids_used':list(SOURCE_ORDER),'source_association':'synthetic, independent of omega',
      'real_UAP_sensor_traces_loaded':0,'synthetic_trajectories':360,
      'split':{'train':len(tr),'validation':len(va),'held_out':len(te)},
      'example_binding':example,'branches':result,
      'braid_incremental_mae_delta_rad_s':result['temporal_braid']['mae_rad_s']-result['temporal']['mae_rad_s'],
      'source_incremental_mae_delta_rad_s':result['temporal_source']['mae_rad_s']-result['temporal']['mae_rad_s'],
      'validation_note':'Archive metadata describe public source catalogs only. Associations with generated tracks are controlled simulations, not evidence of real videos.'}

if __name__=='__main__':print(json.dumps(run(),indent=2,sort_keys=True))
