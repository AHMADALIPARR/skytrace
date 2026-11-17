# Planar coordinates and observation geometry. Points are n×2 matrices (x, y) in metres; bearings are
# radians measured counter-clockwise from +x, wrapped to [-π, π). CBQN's  x •math.Atan2 y  = atan2(y, x).
a ← •Import "assert.bqn"
tm ← •Import "temporal.bqn"

Pts ← {"geometry points" a.Fin "geometry points" a.Rk ⟨2, 𝕩⟩ ⋄ "geometry: points need 2 columns" ! 2=1⊑≢𝕩 ⋄ 𝕩}

# sensor Rel pts: offsets of points from a sensor position (2-vector).
Rel ⇐ {(Pts 𝕩)-⎉1 "sensor" a.Sh⟨⟨2⟩, 𝕨⟩}
Range ⇐ {"Range" a.Fin √+˝˘×˜ 𝕨 Rel 𝕩}
Bearing ⇐ {r ← 𝕨 Rel 𝕩 ⋄ "Bearing" a.Fin tm.Wrap (⊏˘r) •math.Atan2 1⊏˘r}
# sensor FromRangeBearing ⟨range, bearing⟩ → points
FromRangeBearing ⇐ {s 𝕊 r‿b:
  "FromRangeBearing" a.Sh⟨≢r, b⟩
  "FromRangeBearing: E_RANGE range must be non-negative" ! ∧´r≥0
  "FromRangeBearing" a.Fin s+⎉1 r×(•math.Cos b)≍˘•math.Sin b
}
Rot ⇐ {c←•math.Cos 𝕨 ⋄ s←•math.Sin 𝕨 ⋄ "Rot" a.Fin (Pts 𝕩)+˝∘×⎉1‿∞ (c‿s)≍(-s)‿c}

Sinc ⇐ {z←𝕩=0 ⋄ z+(¬z)×(•math.Sin 𝕩)÷𝕩+z}
# Constant-turn closed form, parameters x0 y0 v psi om (same order as ST_CT_X0..ST_CT_OMEGA; psi = ψ, om = ω):
#   p(tau) = p0 + v tau sinc(omtau/2) [cos(psi+omtau/2), sin(psi+omtau/2)]
CTPos ⇐ {p 𝕊 tau:
  x0‿y0‿v‿psi‿om ← "CTPos params" a.Fin "CTPos params" a.Sh⟨⟨5⟩, p⟩
  u ← om×tau÷2 ⋄ k ← v×tau×Sinc u
  "CTPos" a.Fin (x0+k×•math.Cos psi+u)≍˘y0+k×•math.Sin psi+u
}
CTVel ⇐ {p 𝕊 tau: ·‿·‿v‿psi‿om ← p ⋄ h ← psi+om×tau ⋄ "CTVel" a.Fin (v×•math.Cos h)≍˘v×•math.Sin h}
