# STBUF typed interchange buffers shared with the C engine (include/skytrace/core.h):
#   bytes 0..7   magic "STBUF" 0 0 1
#   bytes 8..11  u32 element type: 1 = f64, 2 = i64
#   bytes 12..15 u32 rank (≤ 8)
#   then rank × u64 shape, then ×´shape elements, all little-endian.
# Read validates every header field and the exact payload length and returns the array with its shape.
# CBQN's •bit._cast uses host byte order; Read asserts the host is little-endian before casting.
a ← •Import "assert.bqn"

magic ← "STBUF"∾@+0‿0‿1
two32 ← 2⋆32
le ← 1≡⊑⟨8‿'c',32‿'i'⟩•bit._cast @+1‿0‿0‿0

U32s ← {two32|⟨8‿'c',32‿'i'⟩•bit._cast 𝕩}
U64s ← {m ← ((≠𝕩)÷8)‿2⥊U32s 𝕩 ⋄ (⊏˘m)+two32×1⊏˘m}

Decode ⇐ {
  "STBUF: host is not little-endian" ! le
  b ← 𝕩
  "STBUF: truncated header" ! 16≤≠b
  "STBUF: bad magic" ! magic≡8↑b
  et‿rk ← U32s 8↓16↑b
  "STBUF: unknown element type" ! ∨´et=1‿2
  "STBUF: rank exceeds 8" ! rk≤8
  h ← 16+8×rk
  "STBUF: truncated shape" ! h≤≠b
  s ← U64s 16↓h↑b
  n ← ×´s
  "STBUF: payload length does not match shape" ! (8×n)=(≠b)-h
  d ← h↓b
  v ← {et=1 ? "STBUF f64 payload" a.Fin ⟨8‿'c',64‿'f'⟩•bit._cast d ;
       m ← n‿2⥊⟨8‿'c',32‿'i'⟩•bit._cast d
       "STBUF: i64 value outside the exactly representable range" ! ∧´(|1⊏˘m)<2⋆20
       (two32|⊏˘m)+two32×1⊏˘m}
  s⥊v
}
Read ⇐ {Decode •file.Bytes 𝕩}

# Encode an f64 array (rank ≤ 8) as STBUF bytes.
Encode ⇐ {
  x ← "STBUF encode" a.Fin 𝕩
  "STBUF: rank exceeds 8" ! 8≥=x
  W32 ← {⟨32‿'i',8‿'c'⟩•bit._cast 𝕩-two32×𝕩≥2⋆31}
  "STBUF: dimension too large" ! ∧´(≢x)<2⋆31
  magic∾(W32 1‿(=x))∾(W32 ⥊(≢x)≍˘0)∾⟨64‿'f',8‿'c'⟩•bit._cast ⥊x+0
}
Write ⇐ {𝕨 •file.Bytes Encode 𝕩}
