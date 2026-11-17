import importlib.util
import unittest
from pathlib import Path
P=Path(__file__).resolve().parents[1]/'analyze.py'
spec=importlib.util.spec_from_file_location('skytrace_numeric',P)
m=importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
class Analysis(unittest.TestCase):
    def test_ternary_domain(self):
        t=m.truth_table()
        self.assertEqual(t['ternary_combinations'],27)
        self.assertEqual(t['policy_mismatch_combinations'],8)
    def test_unknown_resolution(self):
        self.assertTrue(m.kernel_active(-1,-1,-1))
        self.assertFalse(m.strict_active(-1,-1,-1))
    def test_resolved_never_active(self):
        for r in (-1,0,1):
            for c in (-1,0,1): self.assertFalse(m.kernel_active(r,c,1))
    def test_cycle_detected(self):
        with self.assertRaises(ValueError): m.metrics([{'canonical_id':'a','parent_id':'b'}, {'canonical_id':'b','parent_id':'a'}])
    def test_braid_numerical(self):
        self.assertLess(m.braid_audit()['braid_relation_max_abs_error'],1e-12)
if __name__=='__main__': unittest.main()
