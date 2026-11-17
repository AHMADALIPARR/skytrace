"""SKYTRACE 2.2 - distinct comparisons, synthetic scientific fixtures only.
Run: python3 research/release_22/benchmark.py
"""
import hashlib, importlib.util, json, math, os, platform, resource, statistics, sys, time
from pathlib import Path
import numpy as np
from scipy.optimize import least_squares
ROOT=Path(__file__).resolve().parent
UP=ROOT.parent/'joint_fusion'/'benchmark.py'
spec=importlib.util.spec_from_file_location('skytrace_21_frozen',UP)
v21=importlib.util.module_from_spec(spec);sys.modules[spec.name]=v21;spec.loader.exec_module(v21)
SENSORS=np.array([[-22.,-12.],[24.,-9.],[-7.,27.]])
PHI=(1+5**.5)/2
F=np.array([[1/PHI,1/PHI**.5],[1/PHI**.5,-1/PHI]],complex)
R=np.diag(np.exp(1j*np.array([-4*np.pi/5,3*np.pi/5])))
U1=R;U2=F@R@F

def braid_matrix_features(records):
    """Unitary Fibonacci representation of same fused angular-rate word.
    No augmented measurements are introduced; compare with exact feature input.
    """
    grid,xy,_=v21.points(records)
    d=np.diff(xy,axis=0);heading=np.unwrap(np.arctan2(d[:,1],d[:,0]));tm=(grid[1:]+grid[:-1])/2
    rates=np.diff(heading)/np.diff(tm)
    U=np.eye(2,dtype=complex)
    for k,r in enumerate(rates):
        gen=U1 if k%2==0 else U2
        U=U@(gen if r>=0 else gen.conj().T)
    # Unitary checks: spectral invariants cannot recover sign or rate magnitude alone.
    if np.max(np.abs(U.conj().T@U-np.eye(2)))>1e-10:raise ValueError('invalid braid unitary')
    return np.r_[U.real.flatten(),U.imag.flatten()]

def physical_fixture(seed=0,noise=.003,n=20):
    rng=np.random.default_rng(seed)
    truth=np.array([rng.uniform(-2,2),rng.uniform(-2,2),rng.uniform(3,4.5),
        rng.uniform(-1.2,1.2),rng.uniform(-.11,.11)])
    delta=np.array([0.,rng.uniform(-.18,.18),rng.uniform(-.18,.18)])
    t=np.linspace(0,6,n)
    rows=[]
    for i,s in enumerate(SENSORS):
        xy=v21.xy_path(t,*truth)
        dv=xy-s
        b=np.arctan2(dv[:,1],dv[:,0])+rng.normal(0,noise,n)
        range_vals=np.linalg.norm(dv,axis=1)+rng.normal(0,.10,n)
        for k in range(n):
            rows.append((i,float(t[k]+delta[i]),float(b[k]),float(range_vals[k])))
    return {'truth':truth,'offsets':delta,'rows':np.array(rows,float),'seed':seed}

def physical_residual(x,rows,joint,ref_clock=False):
    params=x[:5]
    ds=np.array([0.,x[5],x[6]]) if joint else np.zeros(3)
    idx=rows[:,0].astype(int);phys_t=rows[:,1]-ds[idx]
    xy=v21.xy_path(phys_t,*params)
    diff=xy-SENSORS[idx]
    br=np.arctan2(diff[:,1],diff[:,0]); rr=np.linalg.norm(diff,axis=1)
    b_err=np.arctan2(np.sin(br-rows[:,2]),np.cos(br-rows[:,2]))/.003
    r_err=(rr-rows[:,3])/.10
    return np.r_[b_err,r_err]

def fit_physics(fixture,known_clock=False):
    rows=fixture['rows']
    # Initial geometric estimates via interpolated range/bearing for reference sensor.
    r0=rows[rows[:,0]==0]; t=r0[:,1]
    xy=SENSORS[0]+np.column_stack([r0[:,3]*np.cos(r0[:,2]),r0[:,3]*np.sin(r0[:,2])])
    vel=(xy[4]-xy[0])/(t[4]-t[0]);speed=np.linalg.norm(vel)
    psi=np.arctan2(vel[1],vel[0]);h=np.unwrap(np.arctan2(np.diff(xy[:,1]),np.diff(xy[:,0])))
    w0=np.polyfit((t[1:]+t[:-1])/2,h,1)[0]
    init=[xy[0,0],xy[0,1],float(np.clip(speed,1,6)),psi,float(np.clip(w0,-.2,.2))]
    if known_clock:
        # Known-clock reference: subtract the injected offsets only for the reference comparison.
        adjusted=rows.copy();adjusted[:,1]-=fixture['offsets'][rows[:,0].astype(int)]
        res=least_squares(physical_residual,init,args=(adjusted,False),max_nfev=160,
            bounds=([-10,-10,1,-5,-.25],[10,10,7,5,.25]))
        estimated_offsets=fixture['offsets'].copy()
    else:
        init += [0.,0.]
        res=least_squares(physical_residual,init,args=(rows,True),max_nfev=220,
            bounds=([-10,-10,1,-5,-.25,-.3,-.3],[10,10,7,5,.25,.3,.3]))
        estimated_offsets=np.array([0.,*res.x[5:]])
    return {'omega_error':abs(float(res.x[4]-fixture['truth'][4])),
        'clock_mae_s':float(np.mean(abs(estimated_offsets[1:]-fixture['offsets'][1:]))),
        'trajectory_rmse_m':float(np.sqrt(np.mean(np.sum((v21.xy_path(np.linspace(0,6,20),*res.x[:5])-v21.xy_path(np.linspace(0,6,20),*fixture['truth']))**2,axis=1)))),
        'converged':bool(res.success and np.linalg.norm(res.fun)<250),'nfev':int(res.nfev)}

def braid_control(seed=20261117,n=90):
    from sklearn.linear_model import Ridge
    rng=np.random.default_rng(seed);all_rows=[v21.simulate(i,rng) for i in range(n)]
    indexes=rng.permutation(n)
    train=[all_rows[k] for k in indexes[:50]];val=[all_rows[k] for k in indexes[50:70]];test=[all_rows[k] for k in indexes[70:]]
    def mat(branch,arr):
        return np.array([v21.features(r['records'],'fused') if branch=='temporal' else
              np.r_[v21.features(r['records'],'fused'),braid_matrix_features(r['records'])] for r in arr])
    ytr=np.array([r['omega_rad_s'] for r in train]);yv=np.array([r['omega_rad_s'] for r in val]);yt=np.array([r['omega_rad_s'] for r in test])
    out={}
    for name in ('temporal','fibonacci_matrix'):
        tic=time.perf_counter();a=mat(name,train);b=mat(name,val);c=mat(name,test);build=time.perf_counter()-tic
        tic=time.perf_counter();fits=[v21.fit_ridge(a,ytr,alpha) for alpha in v21.BUDGET_ALPHAS]
        best=min(fits,key=lambda mod:np.mean((mod.predict(b)-yv)**2));train_t=time.perf_counter()-tic
        tic=time.perf_counter();pred=best.predict(c);infer=time.perf_counter()-tic
        out[name]={'mae_rad_s':float(np.mean(abs(pred-yt))),'alpha':float(best.alpha),
             'construction_s':build,'training_s':train_t,'prediction_s':infer,'features':int(a.shape[1])}
    return out

def run(seeds=range(5),physical_seeds=range(12)):
    begun=time.perf_counter()
    # Fully frozen 2.1 fixture: leave original module unchanged and compare outputs.
    original=json.loads((UP.parent/'results.synthetic.json').read_text())
    frozen=v21.evaluate()
    differences={k:float(frozen['branches'][k]['mae_rad_s']-original['branches'][k]['mae_rad_s']) for k in frozen['branches']}
    per_seed=[]
    for seed in seeds:
        output=v21.evaluate(seed=20261117+seed)
        per_seed.append({'seed':20261117+seed,'methods':{name:{'mae_rad_s':r['mae_rad_s'],
            'coverage90':r['coverage90'],'interval90_width_rad_s':r['interval90_width']} for name,r in output['branches'].items()}})
    aggregate={}
    for branch in per_seed[0]['methods']:
        rows=[p['methods'][branch] for p in per_seed]
        values=np.array([r['mae_rad_s'] for r in rows]);
        aggregate[branch]={'mean_seed_mae_rad_s':float(values.mean()),'sd_seed_mae_rad_s':float(values.std(ddof=1)),
            'approx_95ci_mean_mae_rad_s':[float(values.mean()-2.776*values.std(ddof=1)/np.sqrt(len(values))),float(values.mean()+2.776*values.std(ddof=1)/np.sqrt(len(values)))],
            'mean_coverage90':float(np.mean([r['coverage90'] for r in rows])),
            'mean_interval90_width_rad_s':float(np.mean([r['interval90_width_rad_s'] for r in rows]))}
    physical=[]
    for seed in physical_seeds:
        case=physical_fixture(seed)
        physical.append({'seed':seed,'known':fit_physics(case,True),'unknown':fit_physics(case,False)})
    physical_agg={key:{metric:float(np.mean([row[key][metric] for row in physical])) for metric in ['omega_error','clock_mae_s','trajectory_rmse_m','nfev']}|
         {'failed':sum(not row[key]['converged'] for row in physical)} for key in ['known','unknown']}
    braid=braid_control()
    return {'version':'2.2','synthetic':True,'frozen_21_mae_differences':differences,
        'frozen_tolerance':1e-10,'seeds':per_seed,'aggregate':aggregate,
        'physical_fit':physical,'physical_aggregate':physical_agg,'braid_ablation':braid,
        'environment':{'python':sys.version.split()[0],'numpy':np.__version__,
          'platform':platform.platform(),'source_sha256':hashlib.sha256(UP.read_bytes()).hexdigest()},
        'wall_seconds':time.perf_counter()-begun,'peak_process_rss_kib':resource.getrusage(resource.RUSAGE_SELF).ru_maxrss}

if __name__=='__main__':
    results=run()
    (ROOT/'results.synthetic.json').write_text(json.dumps(results,indent=2)+'\n')
    print(json.dumps({'frozen_differences':results['frozen_21_mae_differences'],
        'aggregate':results['aggregate'],'physical':results['physical_aggregate'],
        'braid':results['braid_ablation'],'wall_seconds':results['wall_seconds']},indent=2))
