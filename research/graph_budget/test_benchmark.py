# SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
# Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
import unittest, sys, json
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from research.graph_budget import benchmark as b
from research.parameter_recovery import benchmark as p
import numpy as np

class TestGraphBudget(unittest.TestCase):
    @classmethod
    def setUpClass(cls): cls.tr,cls.val,cls.te=p.generate()
    def test_fixed_graph_size(self): self.assertEqual(len(b.full_graph_features(self.tr[0]['records'])),55)
    def test_duplicate_invariance(self):
        records=self.te[0]['records']; clone=json.loads(json.dumps(records[0]));
        for name,fn in b.BRANCHES.items():
            with self.subTest(name=name): self.assertTrue(np.array_equal(fn(records),fn(records+[clone])))
    def test_braid_collision(self):
        c=p.collision_example(); self.assertTrue(c['same_braid_word']);self.assertFalse(c['same_temporal_features'])
    def test_samples_change_graph(self):
        rec=json.loads(json.dumps(self.te[0]['records']))
        orig=b.full_graph_features(rec);rec[0]['positions_m'][4][0]+=.1
        self.assertFalse(np.array_equal(orig,b.full_graph_features(rec)))
    def test_timestamp_order_rejected(self):
        rec=json.loads(json.dumps(self.te[0]['records']))
        rec[0]['times_s'][3]=rec[0]['times_s'][2]
        with self.assertRaises(ValueError): b.full_graph_features(rec)
    def test_single_track_all_branches_finite(self):
        for name,fn in b.BRANCHES.items():
            with self.subTest(name=name): self.assertTrue(np.isfinite(fn(self.te[0]['records'])).all())

if __name__=='__main__':unittest.main()
