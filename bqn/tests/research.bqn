# SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
# Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
# Cross-implementation checks of the research ports. Usage: bqn bqn/tests/research.bqn <dir>
# <dir> is written by `skytrace research-export <dir>` (C engine). Every buffer is recomputed here from
# raw inputs (tracks, design matrices, samples) and compared; tolerances are printed with each check.
a  ← •Import "../assert.bqn"
ix ← •Import "../interchange.bqn"
rs ← •Import "../research.bqn"

"usage: bqn research.bqn <research-export-dir>" ! 1=≠•args
dir ← ⊑•args
R ← {ix.Read dir∾"/"∾𝕩∾".stbuf"}
nchecks ← 0 ⋄ nfail ← 0
Check ← {name‿tol 𝕊 x‿y:
  nchecks +↩ 1
  d ← {(≢x)≡≢y ? 0⌈´⥊|x-y ; ∞}
  •Out name∾": max |BQN − C| = "∾(•Fmt d)∾" (tol "∾(•Fmt tol)∾")"
  {d≤tol ? @ ; nfail +↩ 1 ⋄ •Out "FAIL "∾name}
}
Raises ← {name 𝕊 f: nchecks +↩ 1 ⋄ {𝕤 ⋄ F @ ⋄ nfail +↩ 1 ⋄ •Out "FAIL (no error) "∾name}⎊{𝕤 ⋄ •Out name∾": raised as expected"} @}

# ---------------------------------------------------------------- parameter recovery feature maps
trk ← R "pr_track"
"graph features (10)"‿1e¯12 Check ⟨rs.Graph trk, R "pr_graph"⟩
"temporal features (10)"‿1e¯12 Check ⟨rs.Temporal trk, R "pr_temporal"⟩
"braid word features (9)"‿0 Check ⟨rs.BraidBase trk, R "pr_braid_word"⟩
"timed braid features (14)"‿1e¯12 Check ⟨rs.BraidTimed trk, R "pr_braid_timed"⟩
"equal timestamps rejected" Raises {𝕤 ⋄ rs.Temporal (⊏trk)⌾(1⊸⊏) trk}
"word collision: rates of equal sign, different size" Raises {𝕤 ⋄ ! (rs.Word 0.1‿¯0.2‿0.3) ≢ rs.Word 5‿¯1‿0.01}

# ---------------------------------------------------------------- ridge, interval, metrics
xtr‿xv‿xt ← R¨"pr_Xtr"‿"pr_Xv"‿"pr_Xt" ⋄ ytr‿yv‿yt ← R¨"pr_ytr"‿"pr_yv"‿"pr_yt"
"temporal design matrix rows = held-out features"‿1e¯12 Check ⟨⊏xt, R "pr_temporal"⟩
model ← rs.Select xtr‿ytr‿xv‿yv
cm ← R "pr_model"
"selected alpha"‿0 Check ⟨⊑model, ⊑cm⟩
"ridge coefficients"‿1e¯9 Check ⟨3⊑model, 1↓cm⟩
pred ← model rs.Predict xt
"held-out predictions"‿1e¯10 Check ⟨pred, R "pr_pred"⟩
q ← 0.9 rs.QuantileHigher |yv-model rs.Predict xv
sm ← R "pr_summary"
"MAE, RMSE, bias"‿1e¯12 Check ⟨yt rs.Metrics pred, 3↑sm⟩
"90% half-width (quantile 'higher')"‿1e¯12 Check ⟨q, 3⊑sm⟩
"interval coverage"‿0 Check ⟨rs.Mean q≥|pred-yt, 4⊑sm⟩

# ---------------------------------------------------------------- joint fusion and release 2.2
smp ← R "jf_samples" ⋄ sen ← R "jf_sensors" ⋄ cpts ← R "jf_points"
grid‿xy ← smp rs.Points sen
"fusion grid"‿1e¯12 Check ⟨grid, ⊏˘cpts⟩
"triangulated positions (normal equations vs Householder QR)"‿1e¯9 Check ⟨xy, 1↓˘cpts⟩
"Fibonacci braid-matrix features from BQN positions"‿1e¯9 Check ⟨rs.Fib grid‿xy, R "r22_fib"⟩
"Fibonacci braid-matrix features from C positions"‿1e¯12 Check ⟨rs.Fib (⊏˘cpts)‿(1↓˘cpts), R "r22_fib"⟩

# ---------------------------------------------------------------- task information
"task representation (agnostic, temporal, braid sign)"‿1e¯15 Check ⟨rs.TiRepr R "ti_angles", R "ti_repr"⟩

# ---------------------------------------------------------------- solver sanity
"Solve on a pivoting system"‿1e¯12 Check ⟨1‿2‿3, (3‿3⥊0‿2‿1‿1‿0‿0‿3‿1‿5) rs.Solve (3‿3⥊0‿2‿1‿1‿0‿0‿3‿1‿5) +˝∘×⎉1‿∞ 1‿2‿3⟩

•Out "research BQN checks: "∾(•Fmt nchecks)∾" run, "∾(•Fmt nfail)∾" failed"
•Exit 0<nfail
