"""Numerical audit of archive closure + DAG + braid on explicitly synthetic records.
No observational UAP claims are derived from fixture-only data.
"""
from __future__ import annotations
import json
import sys
from collections import Counter
from itertools import product
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'research' / 'speculative_math'))
from core import braid_word, fibonacci_braid_generators, identity, matmul, max_error, unfold_paths, validate_dag


def bool3(x):
    if x is None: return -1
    if type(x) is not bool: raise ValueError('expected bool or null')
    return int(x)


def kernel_active(range_code: int, corroborated: int, resolution: int) -> bool:
    """Matches the existing st_active in kernel/src/skytrace.c: resolution=1 blocks; -1 does not."""
    return resolution != 1 and (range_code != 1 or corroborated != 1)


def strict_active(range_code: int, corroborated: int, resolution: int) -> bool:
    """Conservative publication policy: unknown resolution is never auto-admitted."""
    return resolution == 0 and (range_code != 1 or corroborated != 1)


def metrics(records):
    ids = [r['canonical_id'] for r in records]
    edges = [(r['parent_id'], r['canonical_id']) for r in records if r.get('parent_id')]
    order = validate_dag(ids, edges)
    roots = [n for n in order if not any(v == n for _,v in edges)]
    all_paths = [p for root in roots for p in unfold_paths(ids, edges, root)]
    counts = Counter()
    for rec in records:
        r = bool3(rec.get('has_range_telemetry'))
        c = bool3(rec.get('corroborated'))
        s = bool3(rec.get('resolved'))
        counts['kernel_active' if kernel_active(r,c,s) else 'kernel_inactive'] += 1
        counts['strict_active' if strict_active(r,c,s) else 'strict_inactive'] += 1
        if s == -1: counts['unknown_resolution'] += 1
        if r == -1: counts['unknown_range'] += 1
        if c == -1: counts['unknown_corroboration'] += 1
    return {'nodes':len(ids),'edges':len(edges),'roots':len(roots),'root_to_leaf_paths':len(all_paths),
            'max_path_edges':max((len(p)-1 for p in all_paths),default=0), 'topological_order':order,
            'closure':dict(sorted(counts.items()))}


def truth_table():
    combos = [ (r,c,s) for r,c,s in product((-1,0,1),repeat=3) ]
    mismatch = [ {'range':r,'corroborated':c,'resolution':s} for r,c,s in combos
                 if kernel_active(r,c,s) != strict_active(r,c,s) ]
    return {'ternary_combinations':len(combos),'kernel_active_combinations':sum(kernel_active(*x) for x in combos),
            'strict_active_combinations':sum(strict_active(*x) for x in combos),
            'policy_mismatch_combinations':len(mismatch),'mismatches':mismatch}


def braid_audit():
    a,b = fibonacci_braid_generators()
    left = matmul(matmul(a,b),a)
    right = matmul(matmul(b,a),b)
    return {'braid_relation_max_abs_error':max_error(left,right),
            'dag_word_example':[1,2,1], 'equivalent_word':[2,1,2],
            'word_matrix_max_abs_error':max_error(braid_word([1,2,1]),braid_word([2,1,2])),
            'mapping_status':'example braid words, not uniquely inferred from provenance'}


def main():
    raw = json.loads((ROOT/'fixtures/archives/synthetic-batch.json').read_text())
    result = {'fixture':'synthetic-batch.json','observational_data':False,
              'graph':metrics(raw['records']),'ternary_closure':truth_table(),'braid':braid_audit()}
    print(json.dumps(result,sort_keys=True,indent=2,allow_nan=False))

if __name__ == '__main__': main()
