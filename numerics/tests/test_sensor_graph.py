# SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
# Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
import copy
import json
import sys
import unittest
from pathlib import Path

BASE=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(BASE/'numerics'))
from sensor_graph import analyze, GraphError

class SensorGraphTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.fixture=json.loads((BASE/'fixtures/sensors/synthetic-graph.json').read_text())

    def test_sensor_track(self):
        r=analyze(copy.deepcopy(self.fixture))
        self.assertEqual(r['sensor']['observation_count'],2)
        self.assertEqual(r['sensor']['independent_recording_origins'],1)
        self.assertEqual(r['sensor']['missing_range_count'],2)
        self.assertEqual(r['graph']['root_reachable_observations'],{'archive':2})
        self.assertAlmostEqual(r['tracks'][0]['angular_rates'][0]['angular_norm_rad_s'],0.02)

    def test_shuffle_invariant(self):
        a=analyze(copy.deepcopy(self.fixture))
        data=copy.deepcopy(self.fixture)
        data['nodes'].reverse();data['edges'].reverse();data['observations'].reverse()
        b=analyze(data)
        self.assertEqual(a,b)

    def test_join_dag_deduplicates_reachability(self):
        r=analyze(copy.deepcopy(self.fixture))
        self.assertEqual(r['graph']['root_reachable_observations']['archive'],2)

    def test_forged_independent_origin_changes_semantic_fingerprint(self):
        data=copy.deepcopy(self.fixture)
        orig=analyze(data)
        data['observations'][1]['recording_origin_id']='fabricated-second-camera'
        changed=analyze(data)
        self.assertNotEqual(orig['sensor']['sensor_multiset_sha256'],changed['sensor']['sensor_multiset_sha256'])
        self.assertEqual(changed['sensor']['independent_recording_origins'],2)

    def test_drop_measurement_detected(self):
        data=copy.deepcopy(self.fixture)
        orig=analyze(data)
        data['observations'][0]['samples'].pop()
        changed=analyze(data)
        self.assertNotEqual(orig['sensor']['sensor_multiset_sha256'],changed['sensor']['sensor_multiset_sha256'])

    def test_two_node_cycle(self):
        data=copy.deepcopy(self.fixture)
        data['edges'].append({'from':'record-b','to':'case','type':'derived_from'})
        with self.assertRaisesRegex(GraphError,'E_CYCLE'): analyze(data)

    def test_duplicate_containment(self):
        data=copy.deepcopy(self.fixture)
        data['edges'].append({'from':'archive','to':'record-a','type':'contains'})
        with self.assertRaisesRegex(GraphError,'E_MULTIPLE_CONTAINMENT_PARENTS'): analyze(data)

    def test_time_reversal(self):
        data=copy.deepcopy(self.fixture)
        data['observations'][0]['samples'][1]['t_s']=-1
        with self.assertRaisesRegex(GraphError,'E_TIME_ORDER'): analyze(data)

    def test_missing_calibration_field(self):
        data=copy.deepcopy(self.fixture)
        del data['observations'][0]['calibration_sha256']
        with self.assertRaisesRegex(GraphError,'E_CALIBRATION_STATE'): analyze(data)

    def test_missing_range_never_metric_velocity(self):
        r=analyze(copy.deepcopy(self.fixture))
        self.assertTrue(all(t['metric_speed_identifiable'] is False for t in r['tracks']))

    def test_invalid_azimuth(self):
        data=copy.deepcopy(self.fixture)
        data['observations'][0]['samples'][1]['az_rad']=100
        with self.assertRaisesRegex(GraphError,'E_AZIMUTH'): analyze(data)

    def test_unknown_range_not_zero(self):
        data=copy.deepcopy(self.fixture)
        data['observations'][0]['range_m']=0
        with self.assertRaisesRegex(GraphError,'E_RANGE'): analyze(data)

if __name__=='__main__': unittest.main()
