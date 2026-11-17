import copy
import numpy as np
import pytest
from research.source_fusion import fusion
from research.parameter_recovery import benchmark as old

def sample():return old.generate()[0][0]

def test_source_provenance_registry():
 m=fusion.attached_metadata(sample());assert m['source_url']==fusion.SOURCES[m['source_id']]['url']

def test_synthetic_binding_explicit():
 assert fusion.attached_metadata(sample())['source_link_status']=='synthetic_association'

def test_target_not_in_metadata():
 r=sample();m=fusion.metadata_features(r);r['theta_rad_s']+=1;assert np.array_equal(m,fusion.metadata_features(r))

def test_exact_duplicate_observation_invariant():
 r=sample();a=fusion.features(r,'temporal_source_braid');r['records'].append(copy.deepcopy(r['records'][0]));assert np.array_equal(a,fusion.features(r,'temporal_source_braid'))

def test_change_measurement_detected():
 r=sample();a=fusion.features(r,'temporal_source');r['records'][0]['positions_m'][3][0]+=0.5;assert not np.array_equal(a,fusion.features(r,'temporal_source'))

def test_bad_binding_rejected():
 m=fusion.attached_metadata(sample());m['source_url']='https://fake.example';
 with pytest.raises(ValueError):fusion.validate_metadata(m)

def test_known_braid_collision_retained():assert old.collision_example()['same_braid_word']

def test_no_missing_as_false():
 m=fusion.attached_metadata(sample());assert m['range_telemetry']=='unknown' and m['corroboration']=='unknown'
