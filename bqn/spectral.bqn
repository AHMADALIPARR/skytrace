# SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
# Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
# Graph spectral features of an undirected sensor graph given by a symmetric non-negative adjacency matrix.
a ← •Import "assert.bqn"
c ← •Import "covariance.bqn"

Adjacency ⇐ {
  m ← "Adjacency" a.Fin "Adjacency" a.Square 𝕩
  "Adjacency: E_NOT_SYMMETRIC" ! 0=c.SymErr m
  "Adjacency: E_NEGATIVE_WEIGHT" ! ∧´⥊m≥0
  "Adjacency: E_SELF_LOOP diagonal must be zero" ! ∧´0=(c.Id ≠m)/○⥊m
  m
}
Degree ⇐ {+´˘Adjacency 𝕩}
Laplacian ⇐ {m ← Adjacency 𝕩 ⋄ ((+´˘m)×c.Id ≠m)-m}
# Symmetric normalized Laplacian I − D^-1/2 A D^-1/2; an isolated vertex has no normalized row.
NormalizedLaplacian ⇐ {
  m ← Adjacency 𝕩 ⋄ d ← +´˘m
  "NormalizedLaplacian: E_ISOLATED_VERTEX" ! ∧´d>0
  s ← ÷√d
  (c.Id ≠m)-s×s×⎉1 m
}
# ⟨eigenvalues, connected components, algebraic connectivity, spectral radius of A⟩
Spectrum ⇐ {
  m ← Adjacency 𝕩
  w ← ⊑c.Jacobi Laplacian m
  "Spectrum: E_NOT_PSD Laplacian eigenvalue below zero" ! ∧´w≥¯1e¯9
  ncomp ← +´w<1e¯9
  fiedler ← {1<≠w ? 1⊑w ; 0}
  rho ← ⌈´|⊑c.Jacobi m
  ⟨w, ncomp, fiedler, rho⟩
}
