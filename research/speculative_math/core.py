# SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
# Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
"""SKYTRACE speculative math: finite classical models, not physical probe detection."""
from __future__ import annotations
from collections import deque
from dataclasses import dataclass
from math import sqrt, pi
from cmath import exp

PHI = (1 + sqrt(5)) / 2
Matrix = tuple[tuple[complex, complex], tuple[complex, complex]]


def matmul(a: Matrix, b: Matrix) -> Matrix:
    return tuple(tuple(sum(a[i][k] * b[k][j] for k in range(2)) for j in range(2)) for i in range(2))  # type: ignore[return-value]


def dagger(a: Matrix) -> Matrix:
    return tuple(tuple(a[j][i].conjugate() for j in range(2)) for i in range(2))  # type: ignore[return-value]


def identity() -> Matrix:
    return ((1+0j, 0j), (0j, 1+0j))


def max_error(a: Matrix, b: Matrix) -> float:
    return max(abs(a[i][j]-b[i][j]) for i in range(2) for j in range(2))


def fibonacci_braid_generators() -> tuple[Matrix, Matrix]:
    """B3 representation on the two-dimensional total-tau fusion space."""
    a = 1 / PHI
    b = 1 / sqrt(PHI)
    F: Matrix = ((a, b), (b, -a))
    R: Matrix = ((exp(-4j*pi/5), 0j), (0j, exp(3j*pi/5)))
    return R, matmul(matmul(F, R), F)


def braid_word(word: list[int]) -> Matrix:
    """Signed generators: +1, +2, -1, -2; inverses via conjugate transpose."""
    s1, s2 = fibonacci_braid_generators()
    out = identity()
    for letter in word:
        if letter not in (-2, -1, 1, 2):
            raise ValueError('braid generator must be +1,+2,-1,-2')
        gen = s1 if abs(letter) == 1 else s2
        out = matmul(out, gen if letter > 0 else dagger(gen))
    return out


def validate_dag(nodes: list[str], edges: list[tuple[str, str]]) -> list[str]:
    """Deterministic topological ordering; duplicate edges rejected."""
    if len(set(nodes)) != len(nodes):
        raise ValueError('duplicate node')
    indegree = {n: 0 for n in nodes}
    adj = {n: [] for n in nodes}
    if len(set(edges)) != len(edges):
        raise ValueError('duplicate edge')
    for u,v in edges:
        if u not in adj or v not in adj:
            raise ValueError('dangling edge')
        adj[u].append(v)
        indegree[v] += 1
    available = sorted(n for n in nodes if indegree[n] == 0)
    order = []
    while available:
        u = available.pop(0)
        order.append(u)
        for v in sorted(adj[u]):
            indegree[v] -= 1
            if indegree[v] == 0:
                available.append(v)
                available.sort()
    if len(order) != len(nodes):
        raise ValueError('cycle')
    return order


def unfold_paths(nodes: list[str], edges: list[tuple[str, str]], root: str, max_paths: int = 10000) -> list[tuple[str, ...]]:
    """Unfold a DAG into root-to-leaf paths; branch structure is not a physical braid."""
    validate_dag(nodes, edges)
    if root not in nodes or max_paths < 1:
        raise ValueError('invalid root or path cap')
    adj = {n: [] for n in nodes}
    for u,v in edges:
        adj[u].append(v)
    pending = [(root,)]
    paths = []
    while pending:
        path = pending.pop()
        children = sorted(adj[path[-1]], reverse=True)
        if not children:
            paths.append(path)
            if len(paths) > max_paths:
                raise OverflowError('path cap exceeded')
        else:
            pending.extend(path + (child,) for child in children)
            if len(pending) + len(paths) > max_paths:
                raise OverflowError('path cap exceeded')
    return sorted(paths)


def parity_encode(bits: list[int]) -> list[int]:
    """Single-parity-check code: detects odd number of bit flips, not all errors."""
    if not bits or any(b not in (0, 1) for b in bits):
        raise ValueError('bits must be a nonempty binary sequence')
    return bits + [sum(bits) % 2]


def parity_syndrome(codeword: list[int]) -> int:
    if len(codeword) < 2 or any(b not in (0,1) for b in codeword):
        raise ValueError('invalid codeword')
    return sum(codeword) % 2


@dataclass(frozen=True)
class ProbeModel:
    generation: int
    count: int
    resource_units: int


def replicate_probes(initial: int, resources: int, cost_per_child: int, branching: int, generations: int) -> list[ProbeModel]:
    """Discrete resource-bounded branching toy model; no wormhole physics asserted."""
    if initial < 1 or resources < 0 or cost_per_child < 1 or branching < 0 or generations < 0:
        raise ValueError('invalid replication parameters')
    states = [ProbeModel(0,initial,resources)]
    count = initial
    remaining = resources
    for generation in range(1,generations+1):
        offspring = min(count * branching, remaining // cost_per_child)
        remaining -= offspring * cost_per_child
        count += offspring
        states.append(ProbeModel(generation, count, remaining))
    return states


def hypothetical_bridge_edges(locations: list[str], bridges: list[tuple[str,str]]) -> dict[str,object]:
    """Graph-layer scenario only: no claim of physical Einstein-Rosen traversal."""
    if len(set(locations)) != len(locations):
        raise ValueError('duplicate location')
    for u,v in bridges:
        if u not in locations or v not in locations or u == v:
            raise ValueError('invalid hypothetical bridge endpoint')
    return {'status':'speculative_not_observed','vertices':sorted(locations),'edges':sorted(bridges)}
