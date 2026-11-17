# SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
# Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
# Complex matrices as pairs ⟨re, im⟩ of equal-shaped real arrays. STBUF complex buffers are n×n×2.
a ← •Import "assert.bqn"
cv ← •Import "covariance.bqn"
MP ← cv.MP

C ⇐ {𝕨‿𝕩}
FromStbuf ⇐ {"FromStbuf: last axis must be 2" ! 2=¯1⊑≢𝕩 ⋄ ⟨⊏⎉1 𝕩, 1⊸⊏⎉1 𝕩⟩}
ToStbuf ⇐ {r‿i: r≍⎉0 i}
Check ← {𝕨 a.Sh ⟨≢⊑𝕩, 1⊑𝕩⟩ ⋄ 𝕨 a.Fin ∾⥊¨𝕩 ⋄ 𝕩}
Add ⇐ {⟨ar,ai⟩‿⟨br,bi⟩ ← 𝕨‿𝕩 ⋄ ⟨ar+br, ai+bi⟩}
Sub ⇐ {⟨ar,ai⟩‿⟨br,bi⟩ ← 𝕨‿𝕩 ⋄ ⟨ar-br, ai-bi⟩}
Scale ⇐ {s 𝕊 r‿i: ⟨s×r, s×i⟩}
Mul ⇐ {⟨ar,ai⟩‿⟨br,bi⟩ ← 𝕨‿𝕩 ⋄ "complex Mul" Check ⟨(ar MP br)-ai MP bi, (ar MP bi)+ai MP br⟩}
Dagger ⇐ {r‿i: ⟨⍉r, -⍉i⟩}
Id ⇐ {⟨cv.Id 𝕩, 0×cv.Id 𝕩⟩}
Abs ⇐ {r‿i: √(r×r)+i×i}
MaxErr ⇐ {0⌈´⥊Abs 𝕨 Sub 𝕩}
Trace ⇐ {r‿i: n←≠r ⋄ ⟨+´(cv.Id n)/○⥊r, +´(cv.Id n)/○⥊i⟩}
# Kronecker product of complex matrices (n×n) ⊗ (m×m) → nm×nm.
Kron ⇐ {⟨ar,ai⟩‿⟨br,bi⟩ ← 𝕨‿𝕩
  K ← {n←≠𝕨 ⋄ m←≠𝕩 ⋄ ((n×m)‿(n×m))⥊0‿2‿1‿3⍉𝕨×⌜𝕩}
  ⟨(ar K br)-ai K bi, (ar K bi)+ai K br⟩
}
Exp ⇐ {⟨•math.Cos 𝕩, •math.Sin 𝕩⟩}   # e^{iθ} elementwise
Unitarity ⇐ {(Id ≠⊑𝕩) MaxErr 𝕩 Mul Dagger 𝕩}
HermErr ⇐ {𝕩 MaxErr Dagger 𝕩}
# Eigenvalues of a Hermitian matrix: real embedding [[R,−I],[I,R]] has each eigenvalue twice.
HermEig ⇐ {r‿i:
  "HermEig: E_NOT_HERMITIAN" ! (HermErr r‿i)≤1e¯12×1⌈⌈´⥊Abs r‿i
  e ← (r∾˘-i)∾i∾˘r
  w ← ⊑cv.Jacobi e
  "HermEig: embedding eigenvalues are not paired" ! (1e¯10×1⌈⌈´|w)≥⌈´|(⊢´˘w2)-⊣´˘w2←((≠w)÷2)‿2⥊w
  ⊣´˘w2
}
