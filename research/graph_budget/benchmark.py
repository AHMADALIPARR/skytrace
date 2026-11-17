#!/usr/bin/env python3
"""SKYTRACE 1.9: task-relevant graph baseline with transparent compute accounting.
Synthetic data only. Reuses frozen SKYTRACE 1.8 simulation and split.
"""
import json, os, sys, time, tracemalloc, platform
from pathlib import Path
import numpy as np
from research.parameter_recovery import benchmark as prev

SEED=prev.SEED
ALPHAS=prev.BUDGET_ALPHAS


def full_graph_features(records):
    """All 18 node timestamps and 2D positions, calibrated origin/scale invariance.
    Ordered by timestamps; starting point removed, displacement scale retained.
    Reject variable length rather than silently pad or interpolate.
    Canonical graph semantics: each sample is attributed measurement vertex,
    successive measurements linked by directed temporal edges.
    """
    t,xy=prev.measurements(records)
    if len(t)!=18: raise ValueError('full graph fixed-grid fixture requires 18 nodes')
    return np.r_[t-t[0], (xy-xy[0]).reshape(-1),
                 [records[0]['sigma_m']]]


def temporal_features(records): return prev.temporal_features(records)
def braid_features(records): return prev.braid_features(records,'word')
def braid_timing(records): return prev.braid_features(records,'timed')
def braid_metrics(records): return prev.braid_features(records,'metric')

BRANCHES={'full_attributed_graph':full_graph_features,
          'temporal':temporal_features,
          'braid_word':braid_features,
          'braid_timing':braid_timing,
          'braid_metrics':braid_metrics}


def assess(train,val,test,fn,cap_ms=None):
    """Measure construction, tuning, inference; prohibit inference over cap.
    Training cost disclosed and separately evaluated, not hidden in the cap.
    """
    t0=time.perf_counter_ns()
    Xt=np.array([fn(r['records']) for r in train]); Xv=np.array([fn(r['records']) for r in val]); Xh=np.array([fn(r['records']) for r in test])
    representation_ns=time.perf_counter_ns()-t0
    yt=np.array([r['theta_rad_s'] for r in train]); yv=np.array([r['theta_rad_s'] for r in val]); yh=np.array([r['theta_rad_s'] for r in test])
    t1=time.perf_counter_ns()
    models=[prev.fit_ridge(Xt,yt,a) for a in ALPHAS]
    selected=models[int(np.argmin([np.mean((m.predict(Xv)-yv)**2) for m in models]))]
    q=float(np.quantile(abs(selected.predict(Xv)-yv),.9,method='higher'))
    training_ns=time.perf_counter_ns()-t1
    latency=[];preds=[]
    for row in test:
        s=time.perf_counter_ns()
        x=fn(row['records']).reshape(1,-1)
        preds.append(float(selected.predict(x)[0]))
        latency.append(time.perf_counter_ns()-s)
    p=np.array(preds)
    stats=prev.metrics(yh,p)
    median_ms=float(np.median(latency))/1e6
    p95_ms=float(np.percentile(latency,95))/1e6
    return {**stats,'interval_90_coverage':float(np.mean(abs(yh-p)<=q)),
            'interval_90_width_rad_s':2*q,'alpha':selected.alpha,
            'feature_count':int(Xt.shape[1]), 'representation_s_all_splits':representation_ns/1e9,
            'training_tuning_s':training_ns/1e9,
            'per_track_full_pipeline_median_ms':median_ms,
            'per_track_full_pipeline_p95_ms':p95_ms,
            'per_track_budget_cap_ms':cap_ms,
            'cap_pass_p95':None if cap_ms is None else p95_ms<=cap_ms}


def run():
    train,val,test=prev.generate()
    results={}
    for name,fn in BRANCHES.items():
        results[name]=assess(train,val,test,fn,cap_ms=50.)
    # Reference model timing: model fitting includes its own feature extraction.
    t=time.perf_counter_ns(); ref=np.array([prev.direct_fit(r['records']) for r in test]); elapsed=(time.perf_counter_ns()-t)/1e9
    reference={**prev.metrics(np.array([r['theta_rad_s'] for r in test]),ref),
               'per_track_mean_ms':1000*elapsed/len(test),'total_inference_s':elapsed}
    copy=json.loads(json.dumps(test[0]['records'][0])); orig=test[0]['records']; duplication=orig+[copy]
    invariance={k:bool(np.array_equal(fn(orig),fn(duplication))) for k,fn in BRANCHES.items()}
    collision=prev.collision_example()
    return {'version':'SKYTRACE 1.9','dataset':'synthetic direct-position constant-turn',
            'seed':SEED,'split':{'training':len(train),'validation':len(val),'held_out':len(test)},
            'reference':reference,'branches':results,
            'duplicate_invariance':invariance,'collision_control':collision,
            'budget_rule':'50 ms p95 single-trajectory construction plus prediction (not training); training cost reported separately; runs on shared host so no hardware-equality claim',
            'machine':{'python':platform.python_version(),'numpy':np.__version__,'platform':platform.platform()},
            'interpretation':'Full attributed graph = fixed 18-node ordered vertex attribute vector with ridge regression; not graph neural network or exhaustive graph estimator. Full-graph vector is a higher-capacity baseline; 1.8 graph summary remains explicitly lossy.'}

if __name__=='__main__':
    print(json.dumps(run(),indent=2,sort_keys=True))
