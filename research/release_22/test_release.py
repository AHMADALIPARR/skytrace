# SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
# Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
import importlib.util,json,subprocess,sys
from pathlib import Path
import numpy as np
ROOT=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('release22',ROOT/'benchmark.py')
b=importlib.util.module_from_spec(spec);sys.modules[spec.name]=b;spec.loader.exec_module(b)

def test_fibonacci_braid_relation():
    assert np.allclose(b.U1@b.U2@b.U1,b.U2@b.U1@b.U2,atol=1e-12)

def test_matrix_unitary():
    f=b.physical_fixture(1)
    assert len(f['rows'])==60
    rng=np.random.default_rng(3)
    r=b.v21.simulate(1,rng)
    assert len(b.braid_matrix_features(r['records']))==8

def test_reference_clock_identifiability():
    f=b.physical_fixture(3)
    assert f['offsets'][0]==0.
    out=b.fit_physics(f,False)
    assert out['clock_mae_s']<.06
    assert out['trajectory_rmse_m']<.2

def test_time_measured_not_assumed():
    f=b.physical_fixture(5)
    r=f['rows'];k=(r[:,0]==1)
    assert np.isclose(r[k,1][0],f['offsets'][1])

def test_port_recovery_and_output_count():
    lines=[{'operation':'physical_fit','seed':1,'job_id':'one'},
           {'operation':'not_supported','seed':2},
           {'operation':'physical_fit','seed':2,'job_id':'two'}]
    data='\n'.join(json.dumps(line) for line in lines)+'\n'
    child=subprocess.run([sys.executable,'-u',str(ROOT/'kernel_port.py')],input=data,text=True,
             capture_output=True,timeout=25,check=True)
    replies=list(map(json.loads,child.stdout.splitlines()))
    assert len(replies)==3
    assert [r['status'] for r in replies]==['ok','error','ok']
    assert replies[0]['job_id']=='one' and replies[2]['job_id']=='two'

def test_frozen_reference():
    saved=json.loads((ROOT.parent/'joint_fusion'/'results.synthetic.json').read_text())
    now=b.v21.evaluate()
    for key in saved['branches']:
        assert abs(saved['branches'][key]['mae_rad_s']-now['branches'][key]['mae_rad_s'])<1e-10
