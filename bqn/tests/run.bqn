# BQN engine tests. Usage: bqn bqn/tests/run.bqn <interchange-dir>
# The interchange directory is produced by `skytrace export <dir>` (C engine); every buffer read from it is
# recomputed independently here and compared, so this is a cross-implementation check, not a self-check.
a  ← •Import "../assert.bqn"
ix ← •Import "../interchange.bqn"
tm ← •Import "../temporal.bqn"
g  ← •Import "../geometry.bqn"
k  ← •Import "../kinematics.bqn"
cv ← •Import "../covariance.bqn"
sp ← •Import "../spectral.bqn"
cx ← •Import "../complex.bqn"
b  ← •Import "../braid.bqn"
e  ← •Import "../entropy.bqn"

"usage: bqn run.bqn <interchange-dir>" ! 1=≠•args
dir ← ⊑•args
Path ← {dir∾"/"∾𝕩∾".stbuf"}

nchecks ← 0 ⋄ nfail ← 0
Fail ← {nfail +↩ 1 ⋄ •Out "FAIL "∾𝕩}
# name Ok value: value must be exactly 1.
Ok ← {nchecks +↩ 1 ⋄ 𝕩≡1 ? @ ; Fail 𝕨}
# name _t: run 𝔽 @; it must return 1. Errors count as failures and are printed.
_t ← {F ← 𝔽 ⋄ r ← {𝕤 ⋄ F @}⎊{𝕤 ⋄ •Out "  error: "∾•CurrentError @ ⋄ 0} @ ⋄ 𝕨 Ok r}
# substring _raises: 𝔽 @ must fail with an assertion whose message contains `substring`.
_raises ← {F ← 𝔽 ⋄ sub ← 𝕨
  r ← {𝕤 ⋄ F @ ⋄ 0}⎊{𝕤 ⋄ msg ← •CurrentError @ ⋄ ∨´sub⍷msg} @
  ("raises "∾sub) Ok r
}
Near ← {tol 𝕊 x‿y: (≢x)≡≢y ? tol≥0⌈´⥊|x-y ; 0}
Show ← {•Out 𝕨∾" = "∾•Fmt 𝕩 ⋄ 𝕩}

# ---------------------------------------------------------------- interchange
bufs ← ⟨"sigma1"‿⟨2,2,2⟩, "sigma2"‿⟨2,2,2⟩, "word_1_2_2_m1_2"‿⟨2,2,2⟩, "bell_rho_ab"‿⟨4,4,2⟩,
        "bell_rho_a"‿⟨2,2,2⟩, "synthetic_graph_adjacency"‿⟨4,4⟩, "constant_turn_track"‿⟨25,3⟩⟩
data ← {n‿s: x ← ix.Read Path n ⋄ ("shape of "∾n) Ok s≡≢x ⋄ x}¨bufs
s1x‿s2x‿wx‿rabx‿rax‿adj‿track ← data
"roundtrip encode/decode" {𝕤 ⋄ ∧´{𝕩≡ix.Decode ix.Encode 𝕩}¨data}_t @
"bad magic" {𝕤 ⋄ ix.Decode "XTBUF"∾5↓ix.Encode adj} _raises @
"payload length" {𝕤 ⋄ ix.Decode ¯1↓ix.Encode adj} _raises @
"truncated header" {𝕤 ⋄ ix.Decode 10↑ix.Encode adj} _raises @
"non-finite" {𝕤 ⋄ ix.Encode 1‿(0÷0)} _raises @
"non-finite" {𝕤 ⋄ ix.Encode 1‿∞} _raises @

# ---------------------------------------------------------------- temporal
"wrap range" {𝕤 ⋄ w ← tm.Wrap ¯20+0.37×↕120 ⋄ ∧´(w≥-π)∧w<π} _t @
"wrap is identity on [-π,π)" {𝕤 ⋄ x ← ¯3+0.5×↕12 ⋄ 0 Near ⟨x, tm.Wrap x⟩} _t @
"unwrap recovers ramp" {𝕤 ⋄ r ← 0.4+0.9×↕40 ⋄ 1e¯12 Near ⟨r, tm.Unwrap tm.Wrap r⟩} _t @
"E_TIME_ORDER" {𝕤 ⋄ 0‿1‿1‿2 tm.Deriv 4⥊0} _raises @
"deriv of linear" {𝕤 ⋄ 1e¯12 Near ⟨3⥊2, 0‿1‿3‿4 tm.Deriv 1+2×0‿1‿3‿4⟩} _t @
"central of quadratic exact" {𝕤 ⋄ t←0.5×↕9 ⋄ 1e¯12 Near ⟨2×1↓¯1↓t, t tm.Central t×t⟩} _t @
"interp" {𝕤 ⋄ 1e¯12 Near ⟨5‿22.5, ⟨0‿1‿2‿3, 10×0‿1‿2‿3⟩ tm.Interp 0.5‿2.25⟩} _t @
"E_BOUND" {𝕤 ⋄ ⟨0‿1‿2, 0‿1‿2⟩ tm.Interp ⟨2.5⟩} _raises @
"to reference clock" {𝕤 ⋄ ⟨10,9.5,10.5⟩ ≡ ⟨0,0.5⟩ tm.ToReference ⟨0‿1‿1, 10‿10‿11⟩} _t @
"reference sensor 0" {𝕤 ⋄ ⟨0.1,0.5⟩ tm.ToReference ⟨0‿1, 1‿2⟩} _raises @
"overlap" {𝕤 ⋄ (1.5‿2 ≡ 1‿2 tm.Overlap 1.5‿4) ∧ ⟨⟩ ≡ 1‿2 tm.Overlap 3‿4} _t @

# ---------------------------------------------------------------- geometry and kinematics
p5 ← 1.5‿¯2‿4‿0.3‿0.08
tt ← ⊏˘track ⋄ xy ← 1↓˘track
"time axis of export" {𝕤 ⋄ tt ≡ 0.25×↕25} _t @
dct ← "CTPos vs C st_ct_position" Show ⌈´⥊|xy-p5 g.CTPos tt
"constant-turn closed form agrees with C (1e-12)" Ok dct≤1e¯12
"CTPos at 0 is p0" {𝕤 ⋄ 0 Near ⟨1‿2⥊1.5‿¯2, p5 g.CTPos ⟨0⟩⟩} _t @
"straight line limit" {𝕤 ⋄ ref ← (3×↕4)×⌜(•math.Cos 0.2)‿(•math.Sin 0.2) ⋄ 1e¯12 Near ⟨ref, 0‿0‿3‿0.2‿0 g.CTPos ↕4⟩} _t @
"range/bearing round trip" {𝕤 ⋄ s←5‿¯1 ⋄ 1e¯12 Near ⟨xy, s g.FromRangeBearing ⟨s g.Range xy, s g.Bearing xy⟩⟩} _t @
"E_RANGE" {𝕤 ⋄ 0‿0 g.FromRangeBearing ⟨⟨¯1⟩, ⟨0⟩⟩} _raises @
"bearing quadrants" {𝕤 ⋄ 1e¯15 Near ⟨0‿(π÷2)‿(-π)‿(-π÷2), 0‿0 g.Bearing 4‿2⥊1‿0‿0‿1‿¯1‿0‿0‿¯1⟩} _t @
"rotation preserves range" {𝕤 ⋄ 1e¯12 Near ⟨0‿0 g.Range xy, 0‿0 g.Range 0.7 g.Rot xy⟩} _t @
u ← 0.08×0.125
ds ← "chord speed deviation from v·sinc(ωΔt/2)" Show ⌈´|(k.Speed tt k.ChordVel xy)-4×(•math.Sin u)÷u
"chord speed exact (1e-12)" Ok ds≤1e¯12
dw ← "turn rate deviation from ω" Show ⌈´|0.08-tt k.TurnRate xy
"turn rate = ω (1e-10)" Ok dw≤1e¯10
sensor ← 10‿5
ti‿wb‿ta‿ab ← sensor k.BearingRates tt‿xy
r ← sensor g.Rel p5 g.CTPos ti
v ← p5 g.CTVel ti
wtrue ← (((⊏˘r)×1⊏˘v)-(1⊏˘r)×⊏˘v)÷+˝˘×˜r
dwb ← "bearing rate deviation from analytic (central differences, Δt=0.25)" Show ⌈´|wb-wtrue
"bearing angular velocity O(Δt²) (1e-3)" Ok dwb≤1e¯3
# Both bearing-rate estimators (analytic relation with central velocities, and central derivative of the
# unwrapped bearing) must converge to the analytic rate at second order: halving Δt divides the error by ≈4.
BrErr ← {𝕊 dt: t ← dt×↕1+⌊0.5+6÷dt ⋄ x ← p5 g.CTPos t ⋄ ti‿wq‿·‿· ← sensor k.BearingRates t‿x
  rr ← sensor g.Rel p5 g.CTPos ti ⋄ vv ← p5 g.CTVel ti ⋄ w ← (((⊏˘rr)×1⊏˘vv)-(1⊏˘rr)×⊏˘vv)÷+˝˘×˜rr
  ⟨⌈´|wq-w, ⌈´|w-t tm.Central tm.Unwrap sensor g.Bearing x⟩}
errs ← "bearing-rate max errors at Δt = 0.25, 0.125, 0.0625 (relation, unwrapped)" Show >BrErr¨0.25‿0.125‿0.0625
ratios ← "error ratios per halving" Show (¯1↓errs)÷1↓errs
"bearing-rate estimators are second order (ratio in [3.5, 4.5])" Ok ∧´⥊(ratios≥3.5)∧ratios≤4.5
"bearing angular acceleration shape and finiteness" {𝕤 ⋄ ((≠ab)=21) ∧ a.Finite ab} _t @
"E_SINGULAR" {𝕤 ⋄ 0‿0 k.BearingRates ⟨↕3, 3‿2⥊0⟩} _raises @
feat ← tt k.Features xy
"features" {𝕤 ⋄ f ← 1⊑¨feat ⋄ (1e¯12 Near ⟨0.08, 3⊑f⟩) ∧ (1>2⊑f) ∧ 1e¯9 Near ⟨24×(•math.Sin u)÷u, ⊑f⟩} _t @
"residuals vanish on exact track" {𝕤 ⋄ 1e¯12 ≥ k.RMS p5 k.Residuals tt‿xy} _t @
"residuals detect wrong turn rate" {𝕤 ⋄ 0.01 < k.RMS 1.5‿¯2‿4‿0.3‿0.09 k.Residuals tt‿xy} _t @

# ---------------------------------------------------------------- covariance
"cov matches definition" {𝕤 ⋄ c ← cv.Cov xy ⋄ m ← cv.Mean xy ⋄ d ← xy-⎉1 m
  ref ← {+´(𝕨⊏⍉d)×𝕩⊏⍉d}⌜˜↕2 ⋄ 1e¯12 Near ⟨c, ref÷24⟩} _t @
"E_TOO_FEW" {𝕤 ⋄ cv.Cov 1‿2⥊0} _raises @
mm ← 4‿4⥊4‿1‿0.5‿0‿1‿3‿0.2‿0.1‿0.5‿0.2‿2‿0.3‿0‿0.1‿0.3‿1
"jacobi reconstructs" {𝕤 ⋄ w‿q ← cv.Jacobi mm ⋄ 1e¯12 Near ⟨mm, (q×⎉1 w) cv.MP ⍉q⟩} _t @
"jacobi orthonormal" {𝕤 ⋄ w‿q ← cv.Jacobi mm ⋄ 1e¯12 Near ⟨cv.Id 4, (⍉q) cv.MP q⟩} _t @
"jacobi ascending, trace" {𝕤 ⋄ w‿q ← cv.Jacobi mm ⋄ (∧´0≤tm.Diff w) ∧ 1e¯12 Near ⟨10, +´w⟩} _t @
"E_NOT_SYMMETRIC" {𝕤 ⋄ cv.Jacobi 2‿2⥊1‿2‿3‿4} _raises @
"mahalanobis" {𝕤 ⋄ 1e¯12 Near ⟨25‿1, ⟨0‿0, 2‿2⥊1‿0‿0‿1⟩ cv.Mahalanobis2 2‿2⥊3‿4‿1‿0⟩} _t @
"E_NOT_POSITIVE" {𝕤 ⋄ ⟨0‿0, 2‿2⥊1‿2‿2‿1⟩ cv.Mahalanobis2 1‿2⥊0} _raises @

# ---------------------------------------------------------------- graph spectral (synthetic sensor graph)
spec ← sp.Spectrum adj
"laplacian spectrum of exported sensor graph = 0 1 3 4" {𝕤 ⋄ 1e¯12 Near ⟨0‿1‿3‿4, ⊑spec⟩} _t @
"one connected component" {𝕤 ⋄ 1=1⊑spec} _t @
"algebraic connectivity" {𝕤 ⋄ 1e¯12 Near ⟨1, 2⊑spec⟩} _t @
"two components" {𝕤 ⋄ 2=1⊑sp.Spectrum 4‿4⥊0‿1‿0‿0‿1‿0‿0‿0‿0‿0‿0‿1‿0‿0‿1‿0} _t @
"normalized laplacian in [0,2]" {𝕤 ⋄ w ← ⊑cv.Jacobi sp.NormalizedLaplacian adj ⋄ (∧´w≥¯1e¯12)∧(∧´w≤2+1e¯12)∧1e¯12 Near ⟨0, ⊑w⟩} _t @
"E_SELF_LOOP" {𝕤 ⋄ sp.Laplacian 2‿2⥊1‿1‿1‿0} _raises @
"E_NOT_SYMMETRIC" {𝕤 ⋄ sp.Laplacian 2‿2⥊0‿1‿0‿0} _raises @
"E_ISOLATED_VERTEX" {𝕤 ⋄ sp.NormalizedLaplacian 3‿3⥊0‿1‿0‿1‿0‿0‿0‿0‿0} _raises @

# ---------------------------------------------------------------- braid
"golden ratio" {𝕤 ⋄ 1e¯15 Near ⟨b.phi×b.phi, 1+b.phi⟩} _t @
{n‿v: ("braid check "∾n) Ok v≤1e¯15 ⊣ n Show v}¨b.Checks @
"sigma1 equals C export" {𝕤 ⋄ 1e¯15 ≥ b.sigma1 cx.MaxErr cx.FromStbuf s1x} _t @
"sigma2 equals C export" {𝕤 ⋄ 1e¯15 ≥ b.sigma2 cx.MaxErr cx.FromStbuf s2x} _t @
"word 1 2 2 -1 2 equals C export" {𝕤 ⋄ 1e¯15 ≥ (b.Word 1‿2‿2‿¯1‿2) cx.MaxErr cx.FromStbuf wx} _t @
"free reduction" {𝕤 ⋄ (⟨2⟩ ≡ b.FreeReduce 1‿2‿¯2‿¯1‿2‿1‿¯1) ∧ ⟨⟩ ≡ b.FreeReduce ⟨⟩} _t @
"free reduction preserves matrix" {𝕤 ⋄ w ← 1‿2‿¯2‿¯1‿2‿1‿¯1‿2 ⋄ 1e¯14 ≥ (b.Word w) cx.MaxErr b.Word b.FreeReduce w} _t @
"collision: sigma1^10 has the identity matrix (representation is not faithful)" {𝕤 ⋄ 1e¯12 b.Collide ⟨10⥊1, ⟨⟩⟩} _t @
"no collision: 1 2 vs 2 1" {𝕤 ⋄ ¬1e¯9 b.Collide ⟨1‿2, 2‿1⟩} _t @
"E_BOUND word" {𝕤 ⋄ b.Word 4097⥊1} _raises @
"generator must be" {𝕤 ⋄ b.Word ⟨3⟩} _raises @
"fusion dims" {𝕤 ⋄ (0‿1‿1‿2‿3‿5‿8‿13 ≡ {𝕩 b.FusionDim 0}¨1+↕8) ∧ 1‿1‿2‿3‿5‿8‿13‿21 ≡ {𝕩 b.FusionDim 1}¨1+↕8} _t @

# ---------------------------------------------------------------- density matrices, entropy, CHSH
rho ← e.Density e.BellPhiPlus @
ra ← 2‿2‿0 e.PartialTrace rho ⋄ rb ← 2‿2‿1 e.PartialTrace rho
"rho_AB equals C export" {𝕤 ⋄ 1e¯15 ≥ rho cx.MaxErr cx.FromStbuf rabx} _t @
"rho_A equals C export" {𝕤 ⋄ 1e¯15 ≥ ra cx.MaxErr cx.FromStbuf rax} _t @
sab ← "S(rho_AB) bits" Show e.VonNeumann rho
sa ← "S(rho_A) bits" Show e.VonNeumann ra
sb ← "S(rho_B) bits" Show e.VonNeumann rb
"S(rho_AB) = 0 (1e-12)" Ok e.tol ≥ |sab
"S(rho_A) = 1 (1e-12)" Ok e.tol ≥ |1-sa
"S(rho_B) = 1 (1e-12)" Ok e.tol ≥ |1-sb
chsh ← "CHSH" Show (0‿(π÷2)‿(π÷4)‿(3×π÷4)) e.CHSH rho
"CHSH = 2√2 (1e-12)" Ok e.tol ≥ |chsh-2×√2
"correlation = cos(a-b)" {𝕤 ⋄ ∧´⥊{1e¯12 ≥ |(•math.Cos 𝕨-𝕩)-𝕨‿𝕩 e.Correlation rho}⌜˜ ¯1+0.45×↕5} _t @
"joint probabilities normalized" {𝕤 ⋄ 1e¯12 ≥ |1-+´⥊0.3‿1.1 e.Joint rho} _t @
"no-signaling: Alice's marginal independent of Bob's setting" {𝕤 ⋄ m ← >{+´˘0.4‿𝕩 e.Joint rho}¨0‿0.7‿2 ⋄ 1e¯12 ≥ ⌈´⥊|m-⎉1 ⊑m} _t @
prod ← e.Density ⟨1‿0‿0‿0, 4⥊0⟩
"product state obeys CHSH ≤ 2" {𝕤 ⋄ s ← ⥊{⟨0,𝕨,𝕩,𝕨+𝕩⟩ e.CHSH prod}⌜˜ 0.3×↕11 ⋄ (2+1e¯12) ≥ ⌈´|s} _t @
"product state entropy 0" {𝕤 ⋄ e.tol ≥ |e.VonNeumann prod} _t @
"maximally mixed 2 qubits = 2 bits" {𝕤 ⋄ e.tol ≥ |2-e.VonNeumann ⟨0.25×cv.Id 4, 4‿4⥊0⟩} _t @
"purity of rho_A = 1/2" {𝕤 ⋄ e.tol ≥ |0.5-e.Purity ra} _t @
"shannon" {𝕤 ⋄ 1e¯15 ≥ |1.5-e.Shannon 0.25‿0.25‿0.5} _t @
"pauli-Y eigenvalues" {𝕤 ⋄ 1e¯14 Near ⟨¯1‿1, cx.HermEig ⟨2‿2⥊0, 2‿2⥊0‿¯1‿1‿0⟩⟩} _t @
"E_TOLERANCE" {𝕤 ⋄ e.Density ⟨1‿1‿0‿0, 4⥊0⟩} _raises @
"E_NOT_HERMITIAN" {𝕤 ⋄ e.Validate ⟨2‿2⥊0.5‿0.3‿0‿0.5, 2‿2⥊0⟩} _raises @
"E_NOT_POSITIVE" {𝕤 ⋄ e.Validate ⟨2‿2⥊1.5‿0‿0‿¯0.5, 2‿2⥊0⟩} _raises @
"E_SHAPE" {𝕤 ⋄ 3‿2‿0 e.PartialTrace rho} _raises @
# Braid operator on Alice's declared subsystem: local, so entanglement and Bob's state are unchanged.
"braid on subsystem A" {𝕤
  ok ← {𝕊 w: u ← (b.Word w) cx.Kron cx.Id 2 ⋄ r2 ← u cx.Mul rho cx.Mul cx.Dagger u
        (e.tol ≥ |1-e.VonNeumann 2‿2‿0 e.PartialTrace r2) ∧ e.tol ≥ rb cx.MaxErr 2‿2‿1 e.PartialTrace r2}¨⟨⟨1⟩, 1‿2‿1, 2‿¯1‿2‿2⟩
  ∧´ok} _t @

•Out "bqn: "∾(•Fmt nchecks)∾" checks, "∾(•Fmt nfail)∾" failures"
•Exit 0<nfail
