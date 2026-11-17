# SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
# Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
# Temporal operations on sampled series. Times are seconds on one declared clock; sensor clocks are
# mapped to the reference clock with explicit per-sensor offsets (reference sensor offset is 0).
a ← •Import "assert.bqn"

# Angle wrap to [-π, π) with floor-mod semantics (matches st_wrap_angle / Python's %).
Wrap ⇐ {(-π)+(2×π)|𝕩+π}

Diff ⇐ {"Diff" a.Fin (1↓𝕩)-¯1↓𝕩}
Mid ⇐ {"Mid" a.Fin 0.5×(1↓𝕩)+¯1↓𝕩}

# Strictly increasing time axis; a repeated or reversed timestamp is a machine-readable failure.
Increasing ⇐ {
  "temporal: E_TIME_ORDER time axis must be strictly increasing" ! ∧´0<Diff "temporal time" a.Fin 𝕩
  𝕩
}

# Continuous angle from wrapped samples: integrates wrapped first differences.
Unwrap ⇐ {"Unwrap" a.Fin (⊑𝕩)+0∾+`Wrap Diff 𝕩}

# t Deriv y: forward-difference derivative, sampled at Mid t. y may be a list or an n×k matrix.
Deriv ⇐ {t 𝕊 y:
  "Deriv" a.Sh⟨(≠t)∾1↓≢y, y⟩
  dt ← Diff Increasing t
  "Deriv" a.Fin (Diff y)÷dt
}

# t Central y: second-order central derivative at interior samples 1..n-2.
Central ⇐ {t 𝕊 y:
  "Central: need at least 3 samples" ! 3≤≠t
  "Central" a.Sh⟨(≠t)∾1↓≢y, y⟩
  dt ← (2↓Increasing t)-¯2↓t
  "Central" a.Fin ((2↓y)-¯2↓y)÷dt
}

# t‿y Interp tq: piecewise-linear interpolation; extrapolation is refused.
Interp ⇐ {t‿y 𝕊 tq:
  "Interp" a.Sh⟨≢t, y⟩
  "temporal: E_BOUND query time outside the sampled interval" ! ∧´((⊑t)≤tq)∧tq≤¯1⊑Increasing t
  i ← 1⌈(¯1+≠t)⌊t⍋tq
  t0 ← (i-1)⊏t ⋄ t1 ← i⊏t ⋄ y0 ← (i-1)⊏y ⋄ y1 ← i⊏y
  "Interp" a.Fin y0+(y1-y0)×(tq-t0)÷t1-t0
}

# offsets ToReference ⟨sensor index per sample, local time per sample⟩ → reference-clock time.
ToReference ⇐ {off 𝕊 s‿t:
  "ToReference: reference sensor 0 must have offset 0" ! 0=⊑off
  "ToReference: sensor index out of range" ! ∧´(s≥0)∧s<≠off
  "ToReference" a.Fin t-s⊏off
}

# Overlap of two closed intervals ⟨lo, hi⟩; returns ⟨⟩ when they do not intersect.
Overlap ⇐ {l‿h ← (⌈´⊑¨𝕨‿𝕩)‿(⌊´1⊑¨𝕨‿𝕩) ⋄ l≤h ? l‿h ; ⟨⟩}
