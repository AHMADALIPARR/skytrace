# SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
# Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
import json
import pathlib
import unittest

HERE = pathlib.Path(__file__).resolve().parents[1]

class HypothesesContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.document = json.loads((HERE / 'hypotheses.json').read_text())
    def test_unique_models(self):
        ids = [m['id'] for m in self.document['models']]
        self.assertEqual(len(ids), len(set(ids)))
        self.assertEqual(len(ids), 5)
    def test_models_have_falsifiers(self):
        for model in self.document['models']:
            self.assertTrue(model['predictions'])
            self.assertTrue(model['falsifiers'])
    def test_telemetry_not_conflated(self):
        states = self.document['telemetry_states']
        self.assertIn('obscured_confirmed', states)
        self.assertIn('not_in_public_video', states)
        self.assertNotEqual(states.index('obscured_confirmed'),states.index('not_in_public_video'))
    def test_expected_cases(self):
        ids = {c['id'] for c in self.document['case_targets']}
        self.assertTrue({'GIMBAL','DOW-UAP-PR113','DOW-UAP-PR149'}.issubset(ids))
    def test_no_gimbal_range_assumption(self):
        gimbal = next(c for c in self.document['case_targets'] if c['id']=='GIMBAL')
        self.assertEqual(gimbal['range'], 'unknown_public_record')
        self.assertEqual(gimbal['redaction_of_range'], 'not_established')

if __name__ == '__main__': unittest.main()
