# SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
# Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
import sys, unittest
from pathlib import Path
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import benchmark as b

class Checks(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tr,cls.va,cls.te=b.generate()
    def test_split_sizes(self):
        self.assertEqual([len(self.tr),len(self.va),len(self.te)],[220,70,70])
    def test_disjoint_trajectory_ids(self):
        ids=[[r['id'] for r in g] for g in (self.tr,self.va,self.te)]
        self.assertEqual(len(set(sum(ids,[]))),360)
    def test_invariant_exact_duplicate(self):
        records=self.tr[0]['records'];dupe=records+[dict(records[0])]
        for fn in (b.graph_features,b.temporal_features,lambda r:b.braid_features(r,'word'),lambda r:b.braid_features(r,'timed'),lambda r:b.braid_features(r,'metric')):
            np.testing.assert_array_equal(fn(records),fn(dupe))
    def test_changed_measurement_not_merged(self):
        records=self.tr[0]['records'];copy=dict(records[0]);copy['positions_m']=[list(x) for x in copy['positions_m']]
        copy['positions_m'][2][0]+=.1
        self.assertEqual(len(b.canonicalize(records+[copy])),2)
    def test_word_collision(self):
        q=b.collision_example();self.assertTrue(q['same_braid_word']);self.assertFalse(q['same_temporal_features'])
    def test_zero_noise_reference(self):
        rng=np.random.default_rng(233)
        row=b.simulate(9,rng,n=18,sigma=0)
        self.assertLess(abs(b.direct_fit(row['records'])-row['theta_rad_s']),1e-5)
    def test_invalid_timestamps(self):
        r=dict(self.tr[0]['records'][0]);r['times_s']=list(r['times_s']);r['times_s'][4]=r['times_s'][3]
        with self.assertRaises(ValueError):b.temporal_features([r])
    def test_branch_feature_lengths(self):
        rec=self.tr[0]['records']
        self.assertEqual(len(b.braid_features(rec,'metric')),len(b.braid_features(rec,'word'))+len(b.temporal_features(rec)))
    def test_quantities_finite(self):
        for item in self.te[:5]:
            self.assertTrue(np.isfinite(b.temporal_features(item['records'])).all())
    def test_output(self):
        report=b.benchmark()
        self.assertTrue(all(report['duplication_invariance'].values()))
        self.assertTrue(report['word_collision_control']['same_braid_word'])
        self.assertEqual(report['split']['test'],70)
        for r in report['branches'].values():
            self.assertTrue(0<=r['interval_90_coverage']<=1)
            self.assertGreater(r['interval_90_width_rad_s'],0)

if __name__=='__main__':unittest.main()
