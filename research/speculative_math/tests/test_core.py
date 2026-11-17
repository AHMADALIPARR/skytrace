# SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
# Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parents[1]))
import unittest
from core import *

class TestSpeculativeMath(unittest.TestCase):
    def test_artin_relation(self):
        self.assertLess(max_error(braid_word([1,2,1]),braid_word([2,1,2])),1e-12)
    def test_unitary(self):
        for s in fibonacci_braid_generators():
            self.assertLess(max_error(matmul(s,dagger(s)),identity()),1e-12)
    def test_inverse(self):
        self.assertLess(max_error(braid_word([1,-1,2,-2]),identity()),1e-12)
    def test_bad_generator(self):
        with self.assertRaises(ValueError): braid_word([3])
    def test_dag_paths(self):
        self.assertEqual(unfold_paths(['a','b','c','d'], [('a','b'),('a','c'),('b','d'),('c','d')],'a'), [('a','b','d'),('a','c','d')])
    def test_cycle(self):
        with self.assertRaises(ValueError): validate_dag(['a','b'],[('a','b'),('b','a')])
    def test_path_cap(self):
        with self.assertRaises(OverflowError): unfold_paths(['a','b','c'],[('a','b'),('a','c')],'a',1)
    def test_parity(self):
        original = parity_encode([1,0,1,1])
        self.assertEqual(parity_syndrome(original),0)
        original[2] ^= 1
        self.assertEqual(parity_syndrome(original),1)
    def test_parity_even_flips_limit(self):
        original = parity_encode([1,0,1])
        original[0] ^= 1
        original[1] ^= 1
        self.assertEqual(parity_syndrome(original),0)
    def test_probes_resource_bound(self):
        states = replicate_probes(1,7,2,2,6)
        self.assertEqual(states[-1].count,4)
        self.assertEqual(states[-1].resource_units,1)
        self.assertEqual(len(states),7)
    def test_bridge_is_not_physical_claim(self):
        self.assertEqual(hypothetical_bridge_edges(['earth','mars'],[('earth','mars')])['status'],'speculative_not_observed')

if __name__=='__main__': unittest.main()
