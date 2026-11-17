# SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
# Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
# Density matrices, partial trace, von Neumann / Shannon entropy and two-qubit correlations.
# Complex matrices are ⟨re, im⟩ pairs (complex.bqn). Tolerance for physical checks: 1e¯12.
a ← •Import "assert.bqn"
cx ← •Import "complex.bqn"
cv ← •Import "covariance.bqn"
tol ⇐ 1e¯12

# |ψ⟩ as ⟨re list, im list⟩ → ρ = |ψ⟩⟨ψ|. The state must already be normalized (no silent rescaling).
Density ⇐ {r‿i:
  "Density" a.Fin r∾i
  n2 ← +´(r×r)+i×i
  "Density: E_TOLERANCE state is not normalized" ! tol≥|1-n2
  ⟨(r×⌜r)+i×⌜i, (i×⌜r)-r×⌜i⟩
}
BellPhiPlus ⇐ {𝕤 ⋄ ⟨(÷√2)×1‿0‿0‿1, 4⥊0⟩}

# Validate a density matrix: Hermitian, unit trace, positive semidefinite. Returns its eigenvalues.
Validate ⇐ {
  "Validate" a.Square ⊑𝕩
  "Validate: E_NOT_HERMITIAN" ! tol≥cx.HermErr 𝕩
  tr ← cx.Trace 𝕩
  "Validate: E_TOLERANCE trace is not 1" ! tol≥(|1-⊑tr)⌈|1⊑tr
  w ← cx.HermEig 𝕩
  "Validate: E_NOT_POSITIVE negative eigenvalue" ! ∧´w≥-tol
  w
}

# dA‿dB‿keep PartialTrace ρ: keep = 0 returns ρ_A (traces out B), keep = 1 returns ρ_B.
PartialTrace ⇐ {dA‿dB‿keep 𝕊 r‿i:
  "PartialTrace: E_SHAPE" ! (dA×dB)=≠r
  "PartialTrace: keep must be 0 or 1" ! ∨´keep=0‿1
  # ρ[(i,j),(k,l)] as m[i;j;k;l]; repeated transpose axes take the diagonal over the traced factor.
  T ← {m ← dA‿dB‿dA‿dB⥊𝕩 ⋄ +´⎉1 (keep⊑⟨0‿2‿1‿2, 2‿0‿2‿1⟩)⍉m}
  ⟨T r, T i⟩
}

Log2 ← {(⋆⁼𝕩)÷⋆⁼2}
# Shannon entropy in bits of a probability vector (0 log 0 = 0). Refuses unnormalized input.
Shannon ⇐ {
  "Shannon: E_NEGATIVE probability" ! ∧´𝕩≥-tol
  "Shannon: E_TOLERANCE probabilities do not sum to 1" ! tol≥|1-+´𝕩
  p ← 0⌈𝕩
  -+´p×Log2 p+p=0
}
# von Neumann entropy S(ρ) = −Σ λ log₂ λ over the validated spectrum (eigenvalues clipped at 0).
VonNeumann ⇐ {w ← 0⌈Validate 𝕩 ⋄ -+´w×Log2 w+w=0}
Purity ⇐ {⊑cx.Trace 𝕩 cx.Mul 𝕩}

# Observable cos θ Z + sin θ X and two-qubit correlation E(al,be) = Tr ρ (A(al)⊗B(be)).
Obs ⇐ {c←•math.Cos 𝕩 ⋄ s←•math.Sin 𝕩 ⋄ ⟨2‿2⥊c‿s‿s‿(-c), 2‿2⥊0⟩}
Correlation ⇐ {al‿be 𝕊 rho:
  Validate rho
  ⊑cx.Trace rho cx.Mul (Obs al) cx.Kron Obs be
}
# Joint outcome probabilities P(x,y), x,y ∈ {+1,−1}, via projectors (1 ± A)/2.
Joint ⇐ {al‿be 𝕊 rho:
  P ← {s 𝕊 th: (0.5) cx.Scale (cx.Id 2) cx.Add s cx.Scale Obs th}
  {⊑cx.Trace rho cx.Mul (𝕨 P al) cx.Kron 𝕩 P be}⌜˜ 1‿¯1
}
CHSH ⇐ {a0‿a1‿b0‿b1 𝕊 rho:
  E ← {𝕨‿𝕩 Correlation rho}
  +´⟨a0 E b0, -a0 E b1, a1 E b0, a1 E b1⟩   # E(a,b) − E(a,b′) + E(a′,b) + E(a′,b′), as st_chsh
}
