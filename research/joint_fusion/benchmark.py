# SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
# Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
"""SKYTRACE 2.1: joint sensor/provenance calibration benchmark (synthetic only).
Run: python3 research/joint_fusion/benchmark.py
"""
import json, math, time, hashlib, tracemalloc
from pathlib import Path
import numpy as np
from scipy.optimize import least_squares
import importlib.util, sys
_prior_path = Path(__file__).resolve().parents[1]/'parameter_recovery'/'benchmark.py'
_prior_spec = importlib.util.spec_from_file_location('skytrace_parameter_recovery_ref', _prior_path)
_prior = importlib.util.module_from_spec(_prior_spec)
sys.modules[_prior_spec.name] = _prior
_prior_spec.loader.exec_module(_prior)
fit_ridge, BUDGET_ALPHAS, metrics = _prior.fit_ridge, _prior.BUDGET_ALPHAS, _prior.metrics
ROOT=Path(__file__).resolve().parent
SEED=20261117

def canon(records):
    seen={}
    for r in records:
        key=hashlib.sha256(json.dumps(r,sort_keys=True,separators=(',',':')).encode()).hexdigest()
        seen[key]=r
    return list(seen.values())

def xy_path(t,x0,y0,v,psi,w):
    if abs(w)<1e-10: return np.column_stack([x0+v*t*np.cos(psi),y0+v*t*np.sin(psi)])
    return np.column_stack([x0+v*t*np.sinc(w*t/(2*np.pi))*np.cos(psi+w*t/2),y0+v*t*np.sinc(w*t/(2*np.pi))*np.sin(psi+w*t/2)])

def simulate(i,rng,n=16,noise=0.008,missing=0.25):
    theta=float(rng.uniform(-0.15,0.15)); v=float(rng.uniform(2.5,5.0)); psi=float(rng.uniform(-np.pi,np.pi))
    x0=float(rng.uniform(-3,3));y0=float(rng.uniform(-3,3));step=float(rng.uniform(.38,.6))
    t=np.arange(n)*step; sensors=np.array([[-22.,-12.],[24.,-9.],[-7.,27.]])
    records=[]
    for s,origin in enumerate(['optical-A','radar-B','optical-C']):
        offset=float(rng.uniform(-.12,.12)); tr=t+offset
        position=xy_path(tr,x0,y0,v,psi,theta)
        vec=position-sensors[s];r=np.linalg.norm(vec,axis=1)
        bearing=np.arctan2(vec[:,1],vec[:,0])+rng.normal(0,noise,size=n)
        range_sigma=.22
        ranges=r+rng.normal(0,range_sigma,size=n)
        valid=np.ones(n,dtype=bool) if s==1 else rng.random(n)>missing
        rows=[{'frame':int(k),'time_s':float(tr[k]),'bearing_rad':float(bearing[k]),
               'range_m':float(ranges[k]) if (s==1 and valid[k]) else None} for k in range(n) if valid[k]]
        records.append({'origin_id':f'{i}:{origin}','recording_origin':f'{i}:{origin}',
          'provenance':{'archive':'synthetic','canonical_id':f'SYNTH-{i}-{s}','parent_id':f'SYNTH-{i}',
                        'calibration_id':f'CAL-{s}','clock_offset_s':offset},
          'sensor_xy_m':sensors[s].tolist(),'bearing_sigma_rad':noise,'range_sigma_m':range_sigma,
          'samples':rows})
    return {'id':i,'omega_rad_s':theta,'records':records,'true_xy':xy_path(t,x0,y0,v,psi,theta).tolist()}

def validate(records):
    records=canon(records)
    origins=[r['origin_id'] for r in records]
    if len(origins)!=len(set(origins)):raise ValueError('conflicting recording origin')
    for r in records:
        ts=[x['time_s'] for x in r['samples']]
        if len(ts)<5 or any(b<=a for a,b in zip(ts,ts[1:])):raise ValueError('invalid time')
        if len(r['sensor_xy_m'])!=2 or not all(np.isfinite(r['sensor_xy_m'])):raise ValueError('invalid geometry')
        if not r['provenance']['canonical_id']:raise ValueError('missing provenance')
    return records

def points(records, use_range=True, use_multisensor=True):
    recs=validate(records)
    if not use_multisensor: recs=recs[:1]
    # Earliest to latest common sampling domain, do not silently extrapolate.
    t0=max(r['samples'][0]['time_s'] for r in recs)
    t1=min(r['samples'][-1]['time_s'] for r in recs)
    grid=np.linspace(t0,t1,12)
    p=[]; effective=[]
    for t in grid:
        A=[];b=[];weights=[]
        for r in recs:
            samples=r['samples'];times=np.array([o['time_s'] for o in samples]); bearings=np.unwrap([o['bearing_rad'] for o in samples]);
            angle=float(np.interp(t,times,bearings));sx,sy=r['sensor_xy_m']
            # normal to bearing line, n dot (target - sensor) = 0
            nx,ny=-math.sin(angle),math.cos(angle)
            A.append([nx,ny]);b.append(nx*sx+ny*sy);weights.append(1/max(r['bearing_sigma_rad'],1e-6)**2)
            if use_range and any(o['range_m'] is not None for o in samples):
                available=[o for o in samples if o['range_m'] is not None]
                times_r=np.array([o['time_s'] for o in available]);values=np.array([o['range_m'] for o in available])
                rr=float(np.interp(t,times_r,values));ux,uy=math.cos(angle),math.sin(angle)
                A.append([ux,uy]);b.append(ux*sx+uy*sy+rr);weights.append(1/max(r['range_sigma_m'],.01)**2)
        AA=np.asarray(A); bb=np.asarray(b); ww=np.sqrt(np.asarray(weights));
        solution=np.linalg.lstsq(AA*ww[:,None],bb*ww,rcond=None)[0]
        p.append(solution);effective.append(np.linalg.matrix_rank(AA))
    return grid,np.asarray(p),min(effective)

def features(records, branch):
    if branch=='bearing_only':
        # One sensor's angular history, no geometry or range.
        r=validate(records)[0]; samples=r['samples']; t=np.array([x['time_s'] for x in samples]); a=np.unwrap([x['bearing_rad'] for x in samples]);
        deriv=np.diff(a)/np.diff(t);return np.array([np.polyfit(t,a,1)[0],np.mean(deriv),np.std(deriv),np.median(deriv),t[-1]-t[0],len(t)],dtype=float)
    t,xy,rank=points(records,use_range=(branch!='multibearing'),use_multisensor=True)
    disp=np.diff(xy,axis=0);heading=np.unwrap(np.arctan2(disp[:,1],disp[:,0])); tm=(t[1:]+t[:-1])/2
    rates=np.diff(heading)/np.diff(tm)
    base=np.array([np.polyfit(tm,heading,1)[0],np.mean(rates),np.median(rates),np.std(rates),np.mean(np.linalg.norm(disp,axis=1)/np.diff(t)),t[-1]-t[0],rank],float)
    if branch!='fused_braid':return base
    # Explicitly additional crossing-sign features, never substitute for metric features.
    signs=np.sign(rates);words=signs*np.where(np.arange(len(signs))%2==0,1,2)
    return np.r_[base,np.mean(signs),np.mean(np.diff(signs)!=0),np.sum(words),np.sum(abs(words)),np.mean(signs[:-1]*signs[1:])]

def fit_branch(train,val,test,branch):
    tic=time.perf_counter()
    extract=lambda samples:features(samples['records'],branch)
    Xtr=np.array([extract(r) for r in train]);Xv=np.array([extract(r) for r in val]);Xt=np.array([extract(r) for r in test]); t_feature=time.perf_counter()-tic
    ytr=np.array([r['omega_rad_s'] for r in train]);yv=np.array([r['omega_rad_s'] for r in val]);yt=np.array([r['omega_rad_s'] for r in test]);
    tic=time.perf_counter(); models=[fit_ridge(Xtr,ytr,a) for a in BUDGET_ALPHAS];mod=min(models,key=lambda m:np.mean((m.predict(Xv)-yv)**2)); train_s=time.perf_counter()-tic
    pred=mod.predict(Xt);pv=mod.predict(Xv);q=float(np.quantile(np.abs(pv-yv),.9,method='higher'))
    o=metrics(yt,pred);o.update(alpha=mod.alpha,coverage90=float(np.mean(abs(pred-yt)<=q)),interval90_width=2*q,feature_count=Xtr.shape[1],
      construction_all_s=t_feature,training_s=train_s,total_s=t_feature+train_s)
    return o

def evaluate(n=180,seed=SEED):
    rng=np.random.default_rng(seed); rows=[simulate(i,rng) for i in range(n)];ids=rng.permutation(n)
    tr=[rows[i] for i in ids[:100]]; val=[rows[i] for i in ids[100:140]];test=[rows[i] for i in ids[140:]]
    tracemalloc.start(); tic=time.perf_counter()
    output={'experiment':'SKYTRACE 2.1 joint metadata/sensor fusion','seed':seed,'synthetic':True,'split':{'train':100,'validation':40,'test':40},'branches':{}}
    for b in ['bearing_only','multibearing','fused','fused_braid']:
        output['branches'][b]=fit_branch(tr,val,test,b)
    output['wall_s']=time.perf_counter()-tic;output['peak_python_allocation_bytes']=tracemalloc.get_traced_memory()[1];tracemalloc.stop()
    # perturb raw input, preserving exact duplicate invariance and sensitivity to telemetry
    rec=test[0]['records'];dupe=rec+[json.loads(json.dumps(rec[0]))]
    output['controls']={'duplicate_invariant':bool(np.allclose(features(rec,'fused_braid'),features(dupe,'fused_braid'))),
      'distinct_origins':len(set(r['origin_id'] for r in validate(rec))),
      'calibrated_multisensor_rank':int(points(rec)[2]),
      'braid_delta_features_norm':float(np.linalg.norm(features(rec,'fused_braid')[7:])),
      'source_id_sample':rec[0]['provenance']['canonical_id']}
    return output

if __name__=='__main__':
    out=evaluate();(ROOT/'results.synthetic.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out,indent=2))
