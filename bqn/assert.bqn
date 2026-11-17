# Shape and numerical assertions. Every public transformation in the BQN engine routes its inputs and
# outputs through these, so a wrong shape or a NaN/Inf aborts with a message naming the transformation
# instead of propagating. (CBQN: NaN=NaN is 0, so 𝕩=𝕩 detects NaN.)

Finite ⇐ {∧´⥊(𝕩=𝕩)∧∞>|𝕩}

# name Fin x: x must be numeric and finite. Returns x.
Fin ⇐ {
  (𝕨∾": non-numeric data") ! ∧´⥊1=•Type¨𝕩
  (𝕨∾": non-finite value") ! Finite 𝕩
  𝕩
}

# name Sh ⟨shape, x⟩: exact shape check. Returns x.
Sh ⇐ {name 𝕊 s‿x:
  (name∾": expected shape "∾(•Fmt s)∾", got "∾•Fmt ≢x) ! s≡≢x
  x
}

# name Rk ⟨rank, x⟩: rank check. Returns x.
Rk ⇐ {name 𝕊 r‿x:
  (name∾": expected rank "∾(•Fmt r)∾", got "∾•Fmt =x) ! r=≠≢x
  x
}

# name Square x: x is a square matrix. Returns x.
Square ⇐ {
  (𝕨∾": expected a square matrix, got shape "∾•Fmt ≢𝕩) ! (2=≠≢𝕩)∧=´2↑(≢𝕩)∾0
  𝕩
}

# Maximum absolute deviation of two equal-shaped arrays.
MaxDev ⇐ {
  "MaxDev: shape mismatch "∾(•Fmt ≢𝕨)∾" vs "∾•Fmt ≢𝕩 ! (≢𝕨)≡≢𝕩
  0⌈´⥊|𝕨-𝕩
}

# ⟨name, tol⟩ Near ⟨a, b⟩: asserts max|a-b| ≤ tol, returns the deviation.
Near ⇐ {name‿tol 𝕊 a‿b:
  d ← a MaxDev b
  (name∾": max deviation "∾(•Fmt d)∾" exceeds tolerance "∾•Fmt tol) ! d≤tol
  d
}
