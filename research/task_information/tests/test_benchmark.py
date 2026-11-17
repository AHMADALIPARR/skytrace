# SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
# Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
import sys,unittest,math
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[3]))
from research.task_information.benchmark import generate,canonical_origins,representation,benchmark

class Tests(unittest.TestCase):
    def test_dup(self):
        row=generate(2)[0][1][0]
        self.assertEqual(representation([row])['fingerprint'],representation([row,row])['fingerprint'])
    def test_mutated_content_same_origin_not_lost(self):
        row=generate(2)[0][1][0]; changed=dict(row); changed['angles']=row['angles'][:];changed['angles'][1]+=.01
        self.assertEqual(len(canonical_origins([row,changed])),2)
    def test_shuffled_order(self):
        a,b=generate(2)[0][1][0],generate(2)[1][1][0]
        self.assertEqual(representation([a,b])['fingerprint'],representation([b,a])['fingerprint'])
    def test_no_speed_from_range_unknown(self):
        self.assertIsNone(generate(2)[0][1][0]['range_m'])
    def test_time_invalid(self):
        row=dict(generate(2)[0][1][0]);row['times']=[0]*9
        with self.assertRaises(ValueError):representation([row])
    def test_reproducible(self): self.assertEqual(benchmark(),benchmark())
    def test_comparison(self):
        result=benchmark()
        self.assertTrue(result['duplication_invariant'])
        self.assertTrue(result['different_measurement_retained'])
        self.assertGreaterEqual(result['models']['temporal_signed_rate_baseline'],result['models']['lossy_absolute_magnitude_baseline'])
    def test_braid_unitary_entropy(self):
        from research.speculative_math.core import braid_word, dagger, matmul
        import numpy as np
        u=np.array(braid_word([1,2,-1,2]), dtype=np.complex128)
        rho=np.diag([.7,.3]).astype(np.complex128)
        rotated=u@rho@u.conj().T
        eig=np.linalg.eigvalsh(rotated)
        entropy=lambda p: -sum(float(x)*math.log2(float(x)) for x in p if x>1e-12)
        self.assertAlmostEqual(entropy(eig),entropy([.7,.3]),places=12)
if __name__=='__main__': unittest.main()
