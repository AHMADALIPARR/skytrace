# Angular velocity / acceleration and trajectory features from sampled positions (t list, xy n×2).
a ← •Import "assert.bqn"
tm ← •Import "temporal.bqn"
g ← •Import "geometry.bqn"

# Chord velocities between consecutive samples (n-1)×2, located at Mid t.
ChordVel ⇐ {t 𝕊 xy: t tm.Deriv "ChordVel" a.Sh⟨(≠t)‿2, xy⟩}
Speed ⇐ {"Speed" a.Fin √+˝˘×˜𝕩}
Heading ⇐ {"Heading" a.Fin tm.Wrap (⊏˘𝕩)•math.Atan2 1⊏˘𝕩}

# Turn rate (heading angular velocity): derivative of the unwrapped chord heading, at interior midpoints.
TurnRate ⇐ {t 𝕊 xy:
  "TurnRate: need at least 3 samples" ! 3≤≠t
  h ← tm.Unwrap Heading t ChordVel xy
  (tm.Mid t) tm.Deriv h
}

# Bearing angular velocity and acceleration as seen from a sensor, from the analytic relation
#   θ̇ = (rx vy − ry vx) / |r|²  with central-difference velocities at interior samples,
# then θ̈ as the central derivative of θ̇. Returns ⟨interior times, θ̇, θ̈ times, θ̈⟩.
BearingRates ⇐ {sensor 𝕊 t‿xy:
  r ← sensor g.Rel xy
  "BearingRates: E_SINGULAR target at the sensor position" ! ∧´0<+˝˘×˜r
  v ← t tm.Central xy
  ri ← 1↓¯1↓r ⋄ ti ← 1↓¯1↓t
  w ← "BearingRates" a.Fin (((⊏˘ri)×1⊏˘v)-(1⊏˘ri)×⊏˘v)÷+˝˘×˜ri
  ⟨ti, w, 1↓¯1↓ti, ti tm.Central w⟩
}

# Trajectory features. Curvature = turn rate / speed at the same midpoints (needs speed > 0).
Features ⇐ {t 𝕊 xy:
  cv ← t ChordVel xy
  sp ← Speed cv
  seg ← sp×tm.Diff t
  path ← +´seg
  disp ← √+´×˜(¯1⊏xy)-⊏xy
  tr ← t TurnRate xy
  spm ← 0.5×(1↓sp)+¯1↓sp
  "Features: E_SINGULAR zero speed makes curvature undefined" ! ∧´spm>0
  ⟨"path_length"‿path, "mean_speed"‿(path÷(¯1⊑t)-⊑t), "straightness"‿(disp÷path),
   "mean_turn_rate"‿((+´tr)÷≠tr), "max_abs_curvature"‿(⌈´|tr÷spm)⟩
}

# Residuals of samples against the constant-turn model with parameters p, and their RMS.
Residuals ⇐ {p 𝕊 t‿xy: "Residuals" a.Fin xy-p g.CTPos t}
RMS ⇐ {√(+´⥊×˜𝕩)÷1⌈≠⥊𝕩}
