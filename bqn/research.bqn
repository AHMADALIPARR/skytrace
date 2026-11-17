# SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
# Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
# Array-level reimplementation of the research feature maps (formerly research/*/benchmark.py, now
# src/research/*.c): track derivatives, graph / temporal / braid-word features, ridge regression with
# validation-selected penalty, the 'higher' quantile interval, weighted bearing/range triangulation, the
# Fibonacci braid-matrix features of release 2.2, and the task-information representation.
# bqn/tests/research.bqn compares every result with buffers exported by `skytrace research-export`.
a  ← •Import "assert.bqn"
tm ← •Import "temporal.bqn"
cv ← •Import "covariance.bqn"
cx ← •Import "complex.bqn"
b  ← •Import "braid.bqn"

Mean ⇐ {(+´𝕩)÷≠𝕩}
Std ⇐ {√Mean ×˜𝕩-Mean 𝕩}                       # ddof 0
Median ⇐ {s ← ∧𝕩 ⋄ n ← ≠s ⋄ 0.5×((⌊(n-1)÷2)⊑s)+(⌊n÷2)⊑s}
Slope ⇐ {x 𝕊 y: dx ← x-Mean x ⋄ (+´dx×y-Mean y)÷+´×˜dx}   # degree-1 least-squares slope
# np.unwrap: add multiples of 2π so successive differences lie in [-π, π)
Unwrap ⇐ {d ← tm.Diff 𝕩 ⋄ m ← (-π)+(2×π)|d+π ⋄ m +↩ (2×π)×(m=-π)∧d>0 ⋄ c ← (m-d)×π≤|d ⋄ 𝕩+0∾+`c}

# track (n×3: t, x, y) → ⟨t, xy, headings, midpoints, rates, dt⟩
# CBQN: 𝕨 •math.Atan2 𝕩 is the angle of the point (𝕨, 𝕩), as in geometry.bqn
Derivs ⇐ {
  t ← ⊏˘𝕩 ⋄ xy ← 1↓˘𝕩
  "research: E_TIME timestamps must increase" ! ∧´0<tm.Diff t
  d ← (1↓xy)-¯1↓xy
  h ← Unwrap (⊏˘d) •math.Atan2 1⊏˘d
  mid ← tm.Mid t ⋄ dt ← tm.Diff mid
  ⟨t, xy, h, mid, (tm.Diff h)÷dt, dt⟩
}
Graph ⇐ {t‿xy‿·‿·‿·‿· ← Derivs 𝕩 ⋄ d ← (1↓xy)-¯1↓xy
  ⟨≠t, (¯1⊑t)-⊑t⟩∾(Mean¨ <˘⍉xy)∾(Std¨ <˘⍉xy)∾(Mean¨ <˘⍉d)∾Std¨ <˘⍉d}
Temporal ⇐ {t‿xy‿h‿mid‿r‿dt ← Derivs 𝕩 ⋄ d ← (1↓xy)-¯1↓xy
  ⟨mid Slope h, Mean r, +´r×dt÷+´dt, Median r, Std r, (¯1⊑h)-⊑h, (¯1⊑t)-⊑t, ≠t, Mean √+˝˘×˜d, Median dt⟩}
Word ⇐ {(¯1+2×0≤𝕩)×1+2|↕≠𝕩}                  # crossing signs on alternating generators
BraidBase ⇐ {w ← Word 4⊑Derivs 𝕩 ⋄ s ← ×w
  ⟨≠w, +´s, Mean s, Mean (1↓s)×¯1↓s, Mean 0≠tm.Diff s⟩∾+´¨w⊸=¨1‿¯1‿2‿¯2}
BraidTimed ⇐ {t‿·‿·‿·‿r‿dt ← Derivs 𝕩 ⋄ s ← ×Word r
  (BraidBase 𝕩)∾⟨(+´s×dt)÷+´dt, +´s×dt, Mean dt, Std dt, (¯1⊑t)-⊑t⟩}

# Gauss–Jordan elimination with partial pivoting.
Solve ⇐ {m 𝕊 v:
  n ← ≠m ⋄ ab ← m∾˘v
  {𝕊 k: p ← k+⊑⍒|k↓k⊏˘ab ⋄ ab ↩ (p‿k⊏ab)⌾(k‿p⊸⊏)ab
      r ← k⊏ab ⋄ ab ↩ ab-((k≠↕n)×(k⊏˘ab)÷k⊑r)×⌜r
      "research: singular normal equations" ! 0≠k⊑r}¨↕n
  (¯1⊏˘ab)÷(⋈˜¨↕n)⊑ab
}
alphas ⇐ 0.01‿0.1‿1‿10‿100
# x‿y Ridge α → ⟨α, mean, sd, β⟩; standardised columns, unpenalised intercept.
Ridge ⇐ {x‿y 𝕊 al:
  mu ← (+˝x)÷≠x ⋄ sd ← √(+˝×˜x-⎉1 mu)÷≠x ⋄ sd +↩ (1-sd)×sd<1e¯9
  am ← 1∾˘(x-⎉1 mu)÷⎉1 sd ⋄ k ← 1+≠mu
  ⟨al, mu, sd, ((al×(0<↕k)×cv.Id k)+(⍉am) cv.MP am) Solve (⍉am) cv.MP y⟩
}
Predict ⇐ {·‿mu‿sd‿beta 𝕊 x: (1∾˘(x-⎉1 mu)÷⎉1 sd) cv.MP beta}
# ⟨xtr, ytr, xv, yv⟩ → the first model of minimum validation MSE
Select ⇐ {xtr‿ytr‿xv‿yv: fits ← (xtr‿ytr)⊸Ridge¨alphas ⋄ (⊑⍋{Mean ×˜yv-𝕩 Predict xv}¨fits)⊑fits}
QuantileHigher ⇐ {q 𝕊 v: (⌈q×¯1+≠v)⊑∧v}
Metrics ⇐ {truth 𝕊 pred: e ← pred-truth ⋄ ⟨Mean |e, √Mean ×˜e, Mean e⟩}

# np.interp on a list of query points: piecewise linear, clamped to the end values outside the sampled interval.
NpInterp ⇐ {xp‿fp 𝕊 x: x ↩ (⊑xp)⌈x⌊¯1⊑xp ⋄ i ← 1⌈(¯1+≠xp)⌊xp⍋x
  x0 ← (i-1)⊏xp ⋄ y0 ← (i-1)⊏fp ⋄ y0+((i⊏fp)-y0)×(x-x0)÷(i⊏xp)-x0}
# Weighted bearing (+ range) triangulation on a 12-point common grid.
# samples: rows (sensor, t, bearing, range or ¯1); sensors: rows (x, y, bearing σ, range σ).
Points ⇐ {samples 𝕊 sensors:
  per ← {s ← 𝕩=⊏˘samples ⋄ s/samples}¨↕≠sensors
  t0 ← ⌈´{⊑1⊏˘𝕩}¨per ⋄ t1 ← ⌊´{¯1⊑1⊏˘𝕩}¨per
  grid ← t1⌾(¯1⊸⊑) t0+(↕12)×(t1-t0)÷11   # np.linspace: last point exact
  Rows ← {tq 𝕊 smp‿sen:
    ts ← 1⊏˘smp ⋄ ang ← ⊑⟨ts, Unwrap 2⊏˘smp⟩ NpInterp ⟨tq⟩
    sx‿sy‿bs‿rs ← sen ⋄ nx ← -•math.Sin ang ⋄ ny ← •math.Cos ang
    rb ← ⟨⟨nx, ny, (nx×sx)+ny×sy, ÷×˜1e¯6⌈bs⟩⟩
    have ← 0≤3⊏˘smp
    {∨´have ? av ← have/smp ⋄ rr ← ⊑⟨1⊏˘av, 3⊏˘av⟩ NpInterp ⟨tq⟩ ⋄ ux ← •math.Cos ang ⋄ uy ← •math.Sin ang
              rb∾⟨⟨ux, uy, (ux×sx)+(uy×sy)+rr, ÷×˜0.01⌈rs⟩⟩ ; rb}
  }
  Solve1 ← {𝕊 tq: m ← >∾(tq⊸Rows)¨per⋈¨<˘sensors ⋄ w ← √3⊏˘m ⋄ aw ← w×2↑˘m ⋄ bw ← w×2⊏˘m
    ((⍉aw) cv.MP aw) Solve (⍉aw) cv.MP bw}
  grid‿(>Solve1¨grid)
}
# Release 2.2 unitary Fibonacci features of the fused heading-rate word: Re U, Im U (row-major).
Fib ⇐ {grid‿xy: d ← (1↓xy)-¯1↓xy ⋄ h ← Unwrap (⊏˘d) •math.Atan2 1⊏˘d
  u ← b.Word Word (tm.Diff h)÷tm.Diff tm.Mid grid
  "research: braid matrix not unitary" ! 1e¯10≥cx.Unitarity u
  (⥊⊑u)∾⥊1⊑u}
# Task-information representation of one track (times 0…8): ⟨mean |v|, mean v², mean v, mean sign⟩
TiRepr ⇐ {v ← tm.Diff 𝕩 ⋄ ⟨Mean |v, Mean ×˜v, Mean v, Mean ¯1+2×0≤v⟩}
