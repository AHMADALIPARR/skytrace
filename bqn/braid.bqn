# Fibonacci anyon braid representation of B₃ on the two-dimensional fusion space of three τ anyons.
#   φ = (1+√5)/2,  F = [[φ⁻¹, φ^-½], [φ^-½, −φ⁻¹]],  R = diag(e^{−4πi/5}, e^{3πi/5}),
#   σ₁ = R,  σ₂ = F R F  (F is real, symmetric and an involution).
# A braid word is a list of generators ±1, ±2; its matrix is the left-to-right product.
# Equal matrices do NOT imply equal braids (σ₁¹⁰ = I here), and equal braids do not imply equal
# trajectories: a word is a coarse symbolic feature of a declared feature mode, never an identity.
a ← •Import "assert.bqn"
cx ← •Import "complex.bqn"

phi ⇐ 0.5×1+√5
fmat ⇐ ⟨2‿2⥊(÷phi)‿(÷√phi)‿(÷√phi)‿(-÷phi), 2‿2⥊0⟩
theta ← π×¯4‿3÷5
id2 ← ⊑cx.Id 2
rmat ⇐ ⟨(•math.Cos theta)×id2, (•math.Sin theta)×id2⟩
sigma1 ⇐ rmat
sigma2 ⇐ fmat cx.Mul rmat cx.Mul fmat

Gen ⇐ {
  "braid: generator must be ±1 or ±2" ! ∨´𝕩=¯2‿¯1‿1‿2
  g ← (1=|𝕩)⊑sigma2‿sigma1
  {𝕩>0 ? g ; cx.Dagger g} 𝕩
}
maxword ⇐ 4096
Word ⇐ {
  "braid: E_BOUND word longer than 4096" ! maxword≥≠𝕩
  (cx.Id 2) cx.Mul´ Gen¨𝕩
}
# Free reduction (cancel adjacent g g⁻¹) — the only word rewriting that is always valid.
FreeReduce ⇐ {⟨⟩ {g 𝕊 acc: {0=≠acc ? ⟨g⟩ ; (¯1⊑acc)=-g ? ¯1↓acc ; acc∾g}}´ ⌽𝕩}
Checks ⇐ {𝕤
  ⟨"unitarity_sigma1"‿(cx.Unitarity sigma1), "unitarity_sigma2"‿(cx.Unitarity sigma2),
   "unitarity_F"‿(cx.Unitarity fmat), "F_involution"‿((cx.Id 2) cx.MaxErr fmat cx.Mul fmat),
   "braid_relation"‿((Word 1‿2‿1) cx.MaxErr Word 2‿1‿2),
   "inverse_sigma1"‿((cx.Id 2) cx.MaxErr Word 1‿¯1), "inverse_sigma2"‿((cx.Id 2) cx.MaxErr Word 2‿¯2)⟩
}
# Matrix collision: two words with (numerically) equal representation matrices up to tol.
Collide ⇐ {tol 𝕊 w1‿w2: tol ≥ (Word w1) cx.MaxErr Word w2}

# Fusion-space dimension for n τ anyons with total charge `total` (0 = vacuum, 1 = τ):
# fusion rules 1×b = b, τ×τ = 1+τ. Gives vacuum = fib(n−1), total τ = fib(n).
FusionDim ⇐ {n 𝕊 total:
  "FusionDim: need n ≥ 1 and total ∈ {0,1}" ! (n≥1)∧∨´total=0‿1
  total⊑ {v‿t: ⟨t, v+t⟩}⍟(n-1) 0‿1
}
