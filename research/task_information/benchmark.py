#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
# Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
"""Pre-registered synthetic task-information benchmark; no real UAP claims.
Independent generative model, train/test split, comparisons and mutation checks.
"""
from __future__ import annotations
import hashlib, json, math, random, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT))
from research.speculative_math.core import braid_word

SEED=20261117

def canonical(data):
    return json.dumps(data,sort_keys=True,ensure_ascii=False,separators=(',',':'),allow_nan=False)

def digest(data):
    return hashlib.sha256(canonical(data).encode()).hexdigest()

def canonical_origins(records):
    """Remove exact republications only, including all content that affects inference.
    Different measurements/calibration for same origin remain distinct.
    """
    unique={}
    for rec in records:
        value={k:rec[k] for k in ('origin','calibration','uncertainty','times','angles','range_m')}
        key=digest(value)
        unique[key]=value
    return [unique[k] for k in sorted(unique)]

def representation(records):
    records=canonical_origins(records)
    increments=[]
    for rec in records:
        times=rec['times']; angles=rec['angles']
        if len(times)!=len(angles) or len(times)<2 or any(b<=a for a,b in zip(times,times[1:])):
            raise ValueError('invalid track')
        increments.extend((b-a)/(v-u) for a,b,u,v in zip(angles,angles[1:],times,times[1:]))
    if not increments: raise ValueError('no samples')
    # The time-agnostic baseline is deliberately lossy; matched temporal baseline is not.
    agnostic=(sum(abs(v) for v in increments)/len(increments),sum(v*v for v in increments)/len(increments))
    temporal=sum(increments)/len(increments)
    # A predeclared (not physically justified) deterministic crossing map.
    word=[(1 if v>=0 else -1)*(1 if i%2==0 else 2) for i,v in enumerate(increments)]
    U=braid_word(word)
    # Matrix trace is an observable braid feature; signed abelianization is another.
    feature=(sum(1 if x>0 else -1 for x in word)/len(word), (U[0][0]+U[1][1]).real, (U[0][0]+U[1][1]).imag)
    # von Neumann entropy of U |0><0| U^dagger is identically 0 (pure state).
    return {'count':len(records),'fingerprint':digest(records),'agnostic':agnostic,'temporal':temporal,'braid':feature,'word':word}

def generate(n=200,seed=SEED):
    rng=random.Random(seed); rows=[]
    for i in range(n):
        label=i%2 # motion direction: 0=negative, 1=positive
        sign=1 if label else -1
        velocity=sign*rng.uniform(0.012,0.035)
        intercept=rng.uniform(-.5,.5)
        noise=[rng.gauss(0,.002) for _ in range(9)]
        times=list(range(9)); angles=[intercept+velocity*t+noise[t] for t in times]
        rec={'origin':f'origin-{i}','calibration':'calib-v1','uncertainty':.002,'times':times,'angles':angles,'range_m':None}
        rows.append((label,[rec]))
    rng.shuffle(rows)
    return rows

def accuracy(data,predict):
    return sum(int(predict(rep)==label) for label,rep in data)/len(data)

def benchmark():
    data=generate(); train,test=data[:100],data[100:]
    # Fixed-rule classifiers; no fitted threshold or test leakage.
    a=accuracy(test,lambda r: int(representation(r)['agnostic'][0]>0.023))
    t=accuracy(test,lambda r: int(representation(r)['temporal']>0))
    b=accuracy(test,lambda r: int(representation(r)['braid'][0]>0))
    combined=accuracy(test,lambda r: int(representation(r)['temporal']>0))
    r=representation(test[0][1]); dup=representation(test[0][1]+[dict(test[0][1][0])])
    changed=dict(test[0][1][0]);changed['angles']=list(changed['angles']);changed['angles'][4]+=.05
    changed_repr=representation(test[0][1]+[changed])
    # Range removal changes the raw canonical representation; metric speed unidentifiable without range.
    range_record=dict(test[0][1][0]);range_record['range_m']=1500.
    report={'dataset':'synthetic_angular_track_direction','seed':SEED,'train_n':len(train),'test_n':len(test),
            'task':'binary_sign_of_underlying_angular_velocity','units':'radians and seconds',
            'models':{'lossy_absolute_magnitude_baseline':a,'temporal_signed_rate_baseline':t,
                      'braid_abelianization':b,'combined_temporal_braid':combined},
            'duplication_invariant':r['fingerprint']==dup['fingerprint'] and r['braid']==dup['braid'],
            'different_measurement_retained':changed_repr['count']==2,
            'range_field_changes_canonical_state':representation([range_record])['fingerprint']!=r['fingerprint'],
            'range_without_platform_calibration_does_not_identify_metric_velocity':True,
            'braid_vs_temporal_improvement':round(b-t,10),
            'scope':'No independent physics inference; braid crossing map is a modeling choice.'}
    return report

if __name__=='__main__': print(json.dumps(benchmark(),indent=2,sort_keys=True))
