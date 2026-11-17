#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
# Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
"""SKYTRACE parameter-recovery preregistered synthetic benchmark.
Only synthetic constant-turn motion; nothing here is evidence about UAPs.
"""
from __future__ import annotations
import hashlib, json, math, statistics, time, tracemalloc
from dataclasses import dataclass
from pathlib import Path
import numpy as np
from scipy.optimize import least_squares

SEED=20261117
BUDGET_ALPHAS=(0.01,0.1,1.,10.,100.)


def record_digest(rec):
    return hashlib.sha256(json.dumps(rec,sort_keys=True,separators=(',',':'),allow_nan=False).encode()).hexdigest()


def canonicalize(records):
    # Deduplicate only byte/metadata-identical records, not independent readings.
    unique={record_digest(r):r for r in records}
    return [unique[key] for key in sorted(unique)]


def simulate(index:int, rng:np.random.Generator, n:int=18, sigma:float=0.035, jitter:float=0., drop:float=0.):
    omega=float(rng.uniform(-.17,.17))
    v=float(rng.uniform(2.5,8.))
    psi=float(rng.uniform(-math.pi,math.pi))
    xy0=rng.uniform(-5,5,size=2)
    times=np.arange(n)*float(rng.uniform(.35,.7))
    if jitter:
        times=times+rng.normal(0,jitter,size=n)
        times[0]=0
        times=np.sort(times)
    # Continuous constant-turn solution, including omega=0 limit.
    if abs(omega)<1e-10:
        x=xy0[0]+v*times*np.cos(psi);y=xy0[1]+v*times*np.sin(psi)
    else:
        x=xy0[0]+v/omega*(np.sin(psi+omega*times)-np.sin(psi))
        y=xy0[1]-v/omega*(np.cos(psi+omega*times)-np.cos(psi))
    measured=np.column_stack([x,y])+rng.normal(0,sigma,size=(n,2))
    if drop:
        keep=rng.random(n)>=drop
        keep[0]=keep[-1]=True
        if sum(keep)<5: keep[:5]=True
        times=times[keep];measured=measured[keep]
    rec={'origin':f'origin-{index}','sensor':'synthetic_position_meter_v1','calibration':'wgs_local_cartesian_m_v1',
         'sigma_m':sigma,'times_s':times.tolist(),'positions_m':measured.tolist()}
    return {'id':index,'theta_rad_s':omega,'speed_m_s':v,'records':[rec]}


def generate(n=360, seed=SEED, **kwargs):
    rng=np.random.default_rng(seed)
    rows=[simulate(i,rng,**kwargs) for i in range(n)]
    order=rng.permutation(n)
    # Split by underlying trajectory, never its measurement copies
    return [rows[i] for i in order[:220]],[rows[i] for i in order[220:290]],[rows[i] for i in order[290:]]


def measurements(records):
    rs=canonicalize(records)
    if len(rs)!=1: raise ValueError('baseline experiment requires exactly one unique sensor track')
    rec=rs[0]
    t=np.asarray(rec['times_s'],dtype=float)
    xy=np.asarray(rec['positions_m'],dtype=float)
    if len(t)<5 or xy.shape!=(len(t),2) or np.any(np.diff(t)<=0):
        raise ValueError('invalid position track')
    return t,xy


def derivatives(records):
    t,xy=measurements(records)
    displacement=np.diff(xy,axis=0)
    headings=np.unwrap(np.arctan2(displacement[:,1],displacement[:,0]))
    midpoint=(t[1:]+t[:-1])/2
    dtheta=np.diff(headings)
    dt=np.diff(midpoint)
    rates=dtheta/dt
    return t,xy,headings,midpoint,rates,dt


def graph_features(records):
    t,xy,h,mid,r,dt=derivatives(records)
    return np.array([len(t),t[-1]-t[0],np.mean(xy[:,0]),np.mean(xy[:,1]),
                     np.std(xy[:,0]),np.std(xy[:,1]),np.mean(np.diff(xy[:,0])),
                     np.mean(np.diff(xy[:,1])),np.std(np.diff(xy[:,0])),np.std(np.diff(xy[:,1]))])


def temporal_features(records):
    t,xy,h,mid,r,dt=derivatives(records)
    # Linear fit of unwrapped displacement headings versus timestamps
    slope=float(np.polyfit(mid,h,1)[0]); weights=dt/dt.sum()
    return np.array([slope,np.mean(r),float(np.sum(weights*r)),np.median(r),
                     float(np.std(r)),h[-1]-h[0],t[-1]-t[0],len(t),
                     np.linalg.norm(np.diff(xy,axis=0),axis=1).mean(),np.median(dt)])


def braid_word(records):
    _,_,_,_,rates,_=derivatives(records)
    # Crossing signs; intentionally ignores metric magnitude and timestamps.
    # Alternating braid generators is a fixed design choice, not a physical law.
    return [(1 if r>=0 else -1)*(1 if i%2==0 else 2) for i,r in enumerate(rates)]


def braid_features(records, variant='word'):
    word=braid_word(records)
    signs=np.array([np.sign(x) for x in word],dtype=float)
    transitions=np.diff(signs)
    base=np.array([len(word),signs.sum(),float(np.mean(signs)),float(np.mean(signs[:-1]*signs[1:])),
                   float(np.mean(transitions!=0)),sum(x==1 for x in word),sum(x==-1 for x in word),
                   sum(x==2 for x in word),sum(x==-2 for x in word)])
    if variant=='word':return base
    t,xy,h,mid,r,dt=derivatives(records)
    if variant=='timed':
        return np.r_[base,np.sum(signs*dt)/dt.sum(),
                     float(np.sum(signs*dt)),np.mean(dt),np.std(dt),t[-1]-t[0]]
    if variant=='metric':
        return np.r_[base,temporal_features(records)]
    raise ValueError(variant)


def direct_fit(records):
    t,xy=measurements(records)
    vxy=np.diff(xy,axis=0)/np.diff(t)[:,None]
    v0=float(np.median(np.linalg.norm(vxy,axis=1)))
    angle=float(np.arctan2(vxy[0,1],vxy[0,0]))
    om0=float(temporal_features(records)[0])
    def predicted(params):
        x0,y0,v,psi,w=params
        if abs(w)<1e-8:
            dx=v*t*np.cos(psi);dy=v*t*np.sin(psi)
        else:
            dx=v*np.sinc((w*t)/(2*math.pi))*t*np.cos(psi+w*t/2)
            dy=v*np.sinc((w*t)/(2*math.pi))*t*np.sin(psi+w*t/2)
        return np.column_stack([x0+dx,y0+dy])
    fit=least_squares(lambda p:(predicted(p)-xy).ravel(),[xy[0,0],xy[0,1],v0,angle,om0],
                      bounds=([-np.inf,-np.inf,.1,-np.inf,-.5],[np.inf,np.inf,30.,np.inf,.5]),
                      max_nfev=90,ftol=1e-8,xtol=1e-8)
    return float(fit.x[-1])


@dataclass
class Ridge:
    alpha:float
    mean:np.ndarray
    std:np.ndarray
    beta:np.ndarray
    def predict(self,X):
        A=(X-self.mean)/self.std
        return np.column_stack([np.ones(len(A)),A])@self.beta


def fit_ridge(X,y,alpha):
    mu=X.mean(axis=0);sd=X.std(axis=0)
    sd=np.where(sd<1e-9,1.,sd)
    A=np.column_stack([np.ones(len(X)),(X-mu)/sd]);penalty=np.eye(A.shape[1])*alpha;penalty[0,0]=0
    beta=np.linalg.solve(A.T@A+penalty,A.T@y)
    return Ridge(alpha,mu,sd,beta)


def metrics(truth,pred):
    error=pred-truth
    return {'mae_rad_s':float(np.mean(abs(error))),'rmse_rad_s':float(np.sqrt(np.mean(error**2))),
            'bias_rad_s':float(np.mean(error))}


def evaluate_branch(train,val,test,extractor):
    tick=time.perf_counter()
    Xtr=np.array([extractor(r['records']) for r in train]);Xv=np.array([extractor(r['records']) for r in val]);Xt=np.array([extractor(r['records']) for r in test])
    representation_s=time.perf_counter()-tick
    ytr=np.array([r['theta_rad_s'] for r in train]);yv=np.array([r['theta_rad_s'] for r in val]);yt=np.array([r['theta_rad_s'] for r in test])
    tick=time.perf_counter();fits=[fit_ridge(Xtr,ytr,a) for a in BUDGET_ALPHAS]
    scores=[np.mean((m.predict(Xv)-yv)**2) for m in fits]
    model=fits[int(np.argmin(scores))]
    pred_val=model.predict(Xv);pred_test=model.predict(Xt)
    tuning_s=time.perf_counter()-tick
    q=float(np.quantile(abs(pred_val-yv),.9,method='higher'))
    report=metrics(yt,pred_test)
    report.update({'alpha':model.alpha,'interval_90_coverage':float(np.mean(abs(pred_test-yt)<=q)),
                   'interval_90_width_rad_s':2*q,'representation_s_all_splits':representation_s,
                   'training_tuning_s':tuning_s,'inference_s_per_track':0.,'feature_count':len(Xtr[0])})
    tick=time.perf_counter()
    for x in Xt: model.predict(x.reshape(1,-1))
    report['inference_s_per_track']=(time.perf_counter()-tick)/len(Xt)
    report['total_s']=representation_s+tuning_s+report['inference_s_per_track']*len(Xt)
    return report


def collision_example():
    # Same crossing signs and timing, different omega. Constant turns at positive w.
    rng=np.random.default_rng(91)
    a=simulate(10,rng,n=24,sigma=0.)
    b=json.loads(json.dumps(a)); t=np.asarray(b['records'][0]['times_s'])
    x0,y0=b['records'][0]['positions_m'][0]
    psi=0.3;v=4.;omega=.12
    b['theta_rad_s']=omega
    b['records'][0]['positions_m']=[[float(x0+v/omega*(np.sin(psi+omega*ti)-np.sin(psi))),
                                       float(y0-v/omega*(np.cos(psi+omega*ti)-np.cos(psi)))] for ti in t]
    # Find second deterministic track with same sign-encoding but different omega.
    def pos(w):
        return [[float(x0+v/w*(np.sin(psi+w*ti)-np.sin(psi))),
                 float(y0-v/w*(np.cos(psi+w*ti)-np.cos(psi)))] for ti in t]
    c=json.loads(json.dumps(b));c['theta_rad_s']=.04;c['records'][0]['positions_m']=pos(.04)
    return {'omega_a_rad_s':.04,'omega_b_rad_s':omega,
            'same_braid_word':braid_word(b['records'])==braid_word(c['records']),
            'same_temporal_features':bool(np.allclose(temporal_features(b['records']),temporal_features(c['records'])))}


def benchmark():
    tr,val,te=generate()
    branches={'graph_summary':graph_features,'temporal':temporal_features,
              'braid_word':lambda x:braid_features(x,'word'),
              'braid_timed':lambda x:braid_features(x,'timed'),
              'braid_metric':lambda x:braid_features(x,'metric')}
    out={name:evaluate_branch(tr,val,te,feature) for name,feature in branches.items()}
    tick=time.perf_counter();p=np.array([direct_fit(r['records']) for r in te]);cost=time.perf_counter()-tick
    direct=metrics(np.array([r['theta_rad_s'] for r in te]),p)
    direct['inference_s_per_track']=cost/len(te)
    original=te[0]['records']; duplicate=original+[json.loads(json.dumps(original[0]))]
    invariant={name:bool(np.allclose(fn(original),fn(duplicate),atol=0,rtol=0)) for name,fn in branches.items()}
    return {'experiment':'synthetic_constant_turn_parameter_recovery','seed':SEED,
            'units':{'target':'rad/s','coordinates':'meters','time':'seconds'},
            'split':{'train':len(tr),'validation':len(val),'test':len(te),'group':'trajectory'},
            'noise_sigma_m':.035,'sensors':'direct_position',
            'branches':out,'direct_model_reference':direct,
            'duplication_invariance':invariant,'word_collision_control':collision_example(),
            'coverage_method':'split absolute-residual quantile on validation; marginal, not conditional guarantee',
            'runtime_note':'wall-clock measurements include feature construction, tuning and held-out prediction; exclude interpreter startup and data generation; repeat on fixed dedicated hardware for comparisons',
            'claim_boundary':'Synthetic model only; not a physical UAP inference or quantum-information claim.'}

if __name__=='__main__':
    print(json.dumps(benchmark(),indent=2,sort_keys=True))


def robustness():
    """Train on base data, then perturb exactly the held-out measurements.
    Independent perturbation generator leaves physical ground truth unchanged.
    """
    tr,val,te=generate()
    methods={'temporal':temporal_features,'braid_word':lambda x:braid_features(x,'word'),
             'braid_timed':lambda x:braid_features(x,'timed'),
             'braid_metric':lambda x:braid_features(x,'metric')}
    ytr=np.array([r['theta_rad_s'] for r in tr]);yv=np.array([r['theta_rad_s'] for r in val]);yt=np.array([r['theta_rad_s'] for r in te])
    trained={}
    for name,fn in methods.items():
        Xtr=np.array([fn(r['records']) for r in tr]);Xv=np.array([fn(r['records']) for r in val])
        fits=[fit_ridge(Xtr,ytr,a) for a in BUDGET_ALPHAS]
        trained[name]=fits[int(np.argmin([np.mean((f.predict(Xv)-yv)**2) for f in fits]))]
    scenarios={}
    for name in ('base','extra_noise_0p1m','time_jitter_0p03s','missing_every_fourth','half_duration'):
        perturbed=[]
        for r in te:
            copy=json.loads(json.dumps(r['records'][0]));rng=np.random.default_rng(SEED+r['id']*17)
            pos=np.array(copy['positions_m']);ts=np.array(copy['times_s'])
            if name=='extra_noise_0p1m':pos+=rng.normal(0,.1,size=pos.shape)
            elif name=='time_jitter_0p03s':
                ts=ts+rng.normal(0,.03,size=len(ts));ts[0]=0.;ts=np.sort(ts)
            elif name=='missing_every_fourth':
                keep=np.array([i%4!=0 or i==0 or i==len(ts)-1 for i in range(len(ts))]);ts=ts[keep];pos=pos[keep]
            elif name=='half_duration':
                n=max(5,len(ts)//2);ts=ts[:n];pos=pos[:n]
            copy['positions_m']=pos.tolist();copy['times_s']=ts.tolist()
            perturbed.append([copy])
        scenarios[name]={}
        for method,extractor in methods.items():
            X=np.array([extractor(records) for records in perturbed]);pred=trained[method].predict(X)
            scenarios[name][method]=metrics(yt,pred)
    return scenarios
