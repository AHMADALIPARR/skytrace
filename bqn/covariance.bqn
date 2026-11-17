# SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
# Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
# Sample statistics and a cyclic Jacobi eigensolver for real symmetric matrices.
a ← •Import "assert.bqn"

MP ⇐ +˝∘×⎉1‿∞
Id ⇐ {=⌜˜↕𝕩}

Mean ⇐ {"Mean: need at least 1 row" ! 1≤≠𝕩 ⋄ "Mean" a.Fin (+˝𝕩)÷≠𝕩}
Center ⇐ {𝕩-⎉1 Mean 𝕩}
# Unbiased sample covariance of an n×k matrix (rows = samples).
Cov ⇐ {
  x ← "Cov" a.Fin "Cov" a.Rk⟨2, 𝕩⟩
  "Cov: E_TOO_FEW need at least 2 samples" ! 2≤≠x
  c ← Center x
  "Cov" a.Fin ((⍉c) MP c)÷¯1+≠x
}
SymErr ⇐ {0⌈´⥊|𝕩-⍉𝕩}

# Jacobi ⟨eigenvalues ascending, eigenvectors as columns⟩. Bounded: at most 100 sweeps, and a matrix that
# has not converged by then is a failure, not a result.
Jacobi ⇐ {
  a0 ← "Jacobi" a.Fin "Jacobi" a.Square 𝕩
  n ← ≠a0
  scale ← 1⌈⌈´⥊|a0
  "Jacobi: E_NOT_SYMMETRIC" ! (SymErr a0)≤1e¯12×scale
  s0 ← 0.5×a0+⍉a0
  Off ← {+´⥊×˜𝕩×¬Id≠𝕩}
  Rot ← {p‿q 𝕊 m‿v:
    apq ← ⟨p,q⟩⊑m
    {(|apq)≤1e¯300 ? m‿v ;
      th ← ((⟨q,q⟩⊑m)-⟨p,p⟩⊑m)÷2×apq
      t ← (¯1+2×th≥0)÷(|th)+√1+th×th
      c ← ÷√1+t×t ⋄ s ← t×c
      j ← (c‿c‿s‿(-s))⌾((⟨p,p⟩‿⟨q,q⟩‿⟨p,q⟩‿⟨q,p⟩)⊸⊑) Id n
      ⟨(⍉j) MP m MP j, v MP j⟩}
  }
  pairs ← ⌽∾{p←𝕩 ⋄ {p‿𝕩}¨(p+1)↓↕n}¨↕n
  Sweep ← {m‿v‿k: m2‿v2 ← m‿v Rot´ pairs ⋄ ⟨0.5×m2+⍉m2, v2, k+1⟩}
  tol ← 1e¯28×1⌈+´⥊×˜s0
  m‿v‿k ← Sweep •_while_ {m‿·‿k: (k<100)∧tol<Off m} ⟨s0, Id n, 0⟩
  "Jacobi: E_NO_CONVERGENCE" ! tol≥Off m
  vals ← {⟨𝕩,𝕩⟩⊑m}¨↕n
  o ← ⍋vals
  ⟨o⊏vals, o⊸⊏˘v⟩
}

# x Mahalanobis² against mean mu and covariance c (via the eigendecomposition; c must be SPD).
Mahalanobis2 ⇐ {mu‿c 𝕊 x:
  w‿v ← Jacobi c
  "Mahalanobis2: E_NOT_POSITIVE covariance is not positive definite" ! ∧´w>1e¯12×1⌈⌈´|w
  z ← (x-⎉1 mu) MP v
  "Mahalanobis2" a.Fin +˝˘(×˜z)÷⎉1 w
}
