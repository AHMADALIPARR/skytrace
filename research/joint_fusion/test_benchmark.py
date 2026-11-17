# SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
# Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
import json
import numpy as np
import pytest
from benchmark import simulate,canon,features,points,validate,xy_path

def fixture():return simulate(2,np.random.default_rng(42))['records']

def test_perfect_identity():
 r=fixture(); assert len(canon(r+[json.loads(json.dumps(r[0]))]))==3

def test_duplicate_invariance():
 r=fixture(); assert np.allclose(features(r,'fused_braid'),features(r+[json.loads(json.dumps(r[0]))],'fused_braid'))

def test_independent_sensors():
 assert len({r['origin_id'] for r in fixture()})==3

def test_geometry_rank():
 assert points(fixture())[2]==2

def test_finite_features():
 for b in ['bearing_only','multibearing','fused','fused_braid']:
  assert np.all(np.isfinite(features(fixture(),b)))

def test_braid_adds_features():
 r=fixture();assert len(features(r,'fused_braid'))>len(features(r,'fused'))

def test_bad_time_rejected():
 r=fixture();r[0]['samples'][1]['time_s']=r[0]['samples'][0]['time_s']
 with pytest.raises(ValueError,match='time'):validate(r)

def test_missing_source_rejected():
 r=fixture();r[0]['provenance']['canonical_id']=''
 with pytest.raises(ValueError,match='provenance'):validate(r)

def test_constant_turn_zero_limit():
 t=np.arange(7);a=xy_path(t,1,2,3,.6,0);b=xy_path(t,1,2,3,.6,1e-9)
 assert np.allclose(a,b,atol=1e-7)

def test_missing_range_is_preserved():
 r=fixture();assert any(s['range_m'] is None for x in r for s in x['samples'])
