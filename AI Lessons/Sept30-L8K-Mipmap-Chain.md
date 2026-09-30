# L8K — The Mipmap Chain
**Theme A1: UV Mapping + Texture Fundamentals | Stage 4 of 5**

---

## The Problem Without This Concept

L8J established that preventing texture aliasing requires pre-filtered (pre-blurred) versions of the texture at the appropriate resolution for each viewing distance. You cannot pre-blur at every possible distance — there are infinitely many. But you can pre-blur at a discrete set of scales. Mipmaps are exactly that: a pyramid of pre-blurred texture versions at power-of-two size steps, covering all viewing distances efficiently.

This lesson derives the structure of the mipmap chain, why it costs only 1/3 more memory than the base texture, and how each level is generated.

---

## Part A — Theory

### Name Origin

"MIP" is a Latin abbreviation for "multum in parvo" — "much in a small place." Each smaller level of the pyramid compactly represents the information from a larger region of the original texture.

### Structure of the Chain

Starting from a 512×512 base texture (level 0):

| Level | Size     | Notes                          |
|-------|----------|--------------------------------|
| 0     | 512×512  | Full resolution                |
| 1     | 256×256  | Half resolution                |
| 2     | 128×128  |                                |
| 3     | 64×64    |                                |
| 4     | 32×32    |                                |
| 5     | 16×16    |                                |
| 6     | 8×8      |                                |
| 7     | 4×4      |                                |
| 8     | 2×2      |                                |
| 9     | 1×1      | Terminal level                 |

Each level halves both dimensions. The chain terminates at 1×1.

**Number of levels**: `floor(log2(max(W, H))) + 1`

For 512×512: `floor(log2(512)) + 1 = floor(9) + 1 = 9 + 1 = 10` levels (0 through 9).

For a non-square texture, e.g., 512×128:
- Level 0: 512×128
- Level 1: 256×64
- Level 2: 128×32
- Level 3: 64×16
- Level 4: 32×8
- Level 5: 16×4
- Level 6: 8×2
- Level 7: 4×1
- Level 8: 2×1
- Level 9: 1×1

Number of levels: `floor(log2(max(512,128))) + 1 = floor(log2(512)) + 1 = 10` levels.

### Memory Cost: The 1/3 Rule

Total texels across all mip levels for a W×H base texture:

```
Total = W×H + W/2×H/2 + W/4×H/4 + ...
      = W×H × (1 + 1/4 + 1/16 + 1/64 + ...)
      = W×H × Σ (1/4)^k for k = 0 to ∞
```

This is a geometric series with first term 1, ratio 1/4:

```
Σ (1/4)^k = 1 / (1 - 1/4) = 1 / (3/4) = 4/3
```

Therefore:

```
Total = W×H × 4/3
```

The mipmap chain costs exactly **1/3 more** than the base texture alone. A 512×512 RGBA8 texture is 512×512×4 = 1,048,576 bytes = 1 MB. With mipmaps: 1 MB × 4/3 = 1.333 MB — 333 KB of overhead for the full mip chain.

This is the definitive answer: mipmaps always cost exactly 1/3 more memory.

**Verify with small example**: W=4, H=4 base.
- Level 0: 4×4 = 16 texels
- Level 1: 2×2 = 4 texels
- Level 2: 1×1 = 1 texel
- Total: 21 texels
- Without mipmaps: 16 texels
- Ratio: 21/16 = 1.3125 ≈ 4/3. (The approximation is exact only for infinite chains; for small textures the ratio converges to 4/3 quickly.)

Exact 4/3 for the geometric series: the residual error (1 - sum of finite terms) is (1/4)^n where n is the number of levels, which goes to 0 as n → ∞.

### How Each Level Is Generated: Box Filter

The simplest (and what `glGenerateMipmap` uses by default):

For each texel at level (k+1) at position (i, j):

```
T[k+1][j][i] = (T[k][2j][2i] + T[k][2j+1][2i] + T[k][2j][2i+1] + T[k][2j+1][2i+1]) / 4
```

This averages a 2×2 block of the parent level. It is a **box filter** — equal weight for all 4 parent texels.

The box filter is simple and fast but not optimal:
- It passes some high frequencies (the box filter's frequency response is a sinc-like function with side lobes)
- It introduces slight ringing at sharp edges
- At each level, the accumulated filtering error grows

**Better alternatives** used in offline/production tools:
- **Lanczos** filter: much better frequency cutoff, sharper edges, minimal ringing
- **Kaiser window**: near-ideal low-pass filter, used by many GPU texture compressors
- **Mitchell-Netravali** (bicubic): good tradeoff between sharpness and ringing

These filters look at more than 4 parent texels (e.g., 6×6 kernel for Lanczos). They produce noticeably better-looking mipmaps, especially at levels 2–4 where the quality difference is most visible. Substance Painter, Photoshop, and offline texture tools use them.

### Power-of-Two Requirement (Legacy vs Modern)

**Legacy OpenGL** (pre-2.0): texture dimensions MUST be powers of two (1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, ...). Non-power-of-two (NPOT) textures were either not supported or had restricted functionality (no mipmapping, no wrapping — only clamping).

The reason: mipmap halving requires even dimensions at every level. If a dimension is odd (e.g., 5), halving gives 2.5 — not an integer. Power-of-two dimensions are always even after any number of halvings.

**Modern OpenGL core profile** (3.2+): NPOT textures are fully supported. For NPOT dimensions, the halving rounds down: 5 → 2 → 1. The mipmap chain is valid but slightly asymmetric.

**OpenGL ES 2.0 and WebGL 1.0**: NPOT restriction still applies. Mipmaps only work with power-of-two textures. Many mobile games keep textures at power-of-two sizes for this reason.

**Best practice**: use power-of-two dimensions unless you have a specific reason not to. Assets at 2048×2048, 1024×512, etc. are universally compatible.

### Uploading Mipmaps in OpenGL

**Option 1: Auto-generate with glGenerateMipmap (standard)**

```c
glGenTextures(1, &texID);
glBindTexture(GL_TEXTURE_2D, texID);

// Upload level 0
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

// Generate all other levels automatically
glGenerateMipmap(GL_TEXTURE_2D);

// Set filter modes
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
```

**Option 2: Manual upload (pre-generated mip chain)**

If you pre-generated high-quality mip levels (e.g., in Photoshop or a custom tool):

```c
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, data_L0);
glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, data_L1);
glTexImage2D(GL_TEXTURE_2D, 2, GL_RGBA8, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, data_L2);
// ... all the way to 1×1
```

No `glGenerateMipmap` call needed — you provide all levels yourself.

**Option 3: Control the mip chain range**

```c
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);   // first usable level
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL,  5);   // last usable level (0–5 only)
```

Useful for texture streaming: initially upload only levels 3–9 (low resolution), then stream in levels 0–2 as the object approaches. GPU uses the available levels and clamps at the finest available.

Unreal Engine 5's texture streaming system uses exactly this: on GPU memory pressure, it drops high-resolution mip levels (levels 0 and 1) and raises `TEXTURE_BASE_LEVEL` to 2 or 3. Streaming them back in as the player approaches the object.

### Compressed Textures and Mipmaps

In production, textures are stored compressed:
- **BC1** (DXT1): 4 bits/texel, RGB, no alpha, lossy
- **BC3** (DXT5): 8 bits/texel, RGBA, lossy
- **BC7**: 8 bits/texel, high-quality lossy for diffuse and normal maps
- **ASTC**: flexible block compression for mobile (3.56–8 bits/texel)

Each mip level is compressed separately. The GPU has on-chip texture decompression hardware — decompressing a 4:1 compressed texture costs the same GPU time as reading an uncompressed texture at 1/4 the bandwidth. Effectively free.

With BC7 (8 bits/texel instead of 32 for RGBA8): 4× memory reduction for the same visual quality. Combined with mipmaps (4/3 total storage): total = base_size × 4/3 × (1/4) = base_size × 1/3. A 2048×2048 BC7 texture with full mips costs the same memory as the original 1024×1024 uncompressed texture.

---

## Part B — Numeric Worked Example

### Full Mip Chain: 4×4 Grayscale

Base texture (level 0):

```
Row 3: [0.8,  0.6,  0.4,  0.2]
Row 2: [0.7,  0.5,  0.3,  0.1]
Row 1: [0.6,  0.4,  0.2,  0.0]
Row 0: [0.5,  0.3,  0.1,  0.0]
```

**Generate level 1 (2×2), box filter:**

Bottom-left 2×2 block (rows 0-1, cols 0-1):
```
T1[0][0] = (T0[0][0] + T0[1][0] + T0[0][1] + T0[1][1]) / 4
         = (0.5 + 0.6 + 0.3 + 0.4) / 4
         = 1.8 / 4
         = 0.450
```

Bottom-right 2×2 block (rows 0-1, cols 2-3):
```
T1[0][1] = (T0[0][2] + T0[1][2] + T0[0][3] + T0[1][3]) / 4
         = (0.1 + 0.2 + 0.0 + 0.0) / 4
         = 0.3 / 4
         = 0.075
```

Top-left 2×2 block (rows 2-3, cols 0-1):
```
T1[1][0] = (T0[2][0] + T0[3][0] + T0[2][1] + T0[3][1]) / 4
         = (0.7 + 0.8 + 0.5 + 0.6) / 4
         = 2.6 / 4
         = 0.650
```

Top-right 2×2 block (rows 2-3, cols 2-3):
```
T1[1][1] = (T0[2][2] + T0[3][2] + T0[2][3] + T0[3][3]) / 4
         = (0.3 + 0.4 + 0.1 + 0.2) / 4
         = 1.0 / 4
         = 0.250
```

Level 1:
```
Row 1: [0.650, 0.250]
Row 0: [0.450, 0.075]
```

**Generate level 2 (1×1):**

```
T2[0][0] = (T1[0][0] + T1[1][0] + T1[0][1] + T1[1][1]) / 4
         = (0.450 + 0.650 + 0.075 + 0.250) / 4
         = 1.425 / 4
         = 0.35625
```

**Verify against global average of all 16 level-0 texels:**

Sum = 0.8+0.6+0.4+0.2 + 0.7+0.5+0.3+0.1 + 0.6+0.4+0.2+0.0 + 0.5+0.3+0.1+0.0

```
Row 3: 0.8+0.6+0.4+0.2 = 2.0
Row 2: 0.7+0.5+0.3+0.1 = 1.6
Row 1: 0.6+0.4+0.2+0.0 = 1.2
Row 0: 0.5+0.3+0.1+0.0 = 0.9

Total = 2.0 + 1.6 + 1.2 + 0.9 = 5.7
Average = 5.7 / 16 = 0.35625
```

Matches exactly. The 1×1 level is the grand average of all original texels. ✓

**Storage cost verification:**

- Level 0: 4×4 = 16 texels
- Level 1: 2×2 = 4 texels
- Level 2: 1×1 = 1 texel
- Total: 21 texels
- Ratio: 21/16 = 1.3125

Theoretical limit: 4/3 = 1.3333. For a 3-level chain:
```
Sum = 1 + 1/4 + 1/16 = 16/16 + 4/16 + 1/16 = 21/16 = 1.3125
```

At 10 levels (512×512): `Sum = (1 - (1/4)^10) / (1 - 1/4) = (1 - 1/1048576) / 0.75 ≈ 1.3333`

---

## Pitfalls

**Pitfall 1: Calling glGenerateMipmap before uploading level 0**

Mistake: generating mipmaps before the texture data is uploaded.

Symptom: all mip levels are generated from uninitialized memory → garbage data at every mip level. Close-up samples work (level 0 is correct), but as the surface moves away, random pixels appear from the corrupted mip levels.

Fix: always call `glGenerateMipmap` after `glTexImage2D` while the texture is still bound.

**Pitfall 2: NPOT dimensions in WebGL 1.0 or OpenGL ES 2.0**

Mistake: uploading a 600×400 texture and trying to use mipmaps.

Symptom: texture appears correct at close range (level 0 used), disappears or renders black at distance (mip lookup fails). WebGL 1.0 silently treats the texture as incomplete for mip filters when dimensions are NPOT.

Fix: pad to power-of-two dimensions (use 1024×512 instead of 600×400), or use GL_CLAMP_TO_EDGE wrap mode with GL_LINEAR filter only (no mipmaps) for NPOT textures in WebGL 1.0.

**Pitfall 3: Expecting glGenerateMipmap to produce high-quality mips**

Mistake: relying on the box-filter mip generation for production assets.

Symptom: at mip levels 2–4, textures look slightly blurry or ringing artifacts appear near sharp edges. Subtle, but visible in stylized art styles where sharp textures are important.

Fix: pre-generate mip chains in an offline tool (Photoshop with plugin, Substance Painter, or custom code using a Lanczos/Kaiser filter). Upload all levels manually via multiple `glTexImage2D` calls with increasing level parameter.

---

## What to Build

**Exercise 1: Manual mipmap upload**

Write code to upload a 4×4 grayscale texture with manually computed mip levels (use the values from Part B). Upload level 0 with `glTexImage2D(GL_TEXTURE_2D, 0, ...)`, level 1, and level 2. Set `GL_TEXTURE_BASE_LEVEL = 0` and `GL_TEXTURE_MAX_LEVEL = 2`. Set MIN_FILTER to GL_LINEAR_MIPMAP_LINEAR. Render on a quad and verify the correct mip levels are used at different distances by checking the colors against the known per-level values.

**Exercise 2: Memory calculation**

A 2048×2048 texture with 4 bytes per texel (RGBA8):
1. Base texture size in bytes and MB
2. Total size with full mipmap chain (exact bytes, using 4/3 multiplier)
3. Same texture compressed as BC7 (4 bits per texel) without mipmaps
4. BC7 with mipmaps

Show all arithmetic. Which configuration fits in 2MB?

**Exercise 3: Mip level debugging texture**

Create a procedural texture where each mip level has a distinct solid color:
- Level 0: red (1,0,0)
- Level 1: green (0,1,0)
- Level 2: blue (0,0,1)
- Level 3: yellow (1,1,0)
- Level 4: cyan (0,1,1)
- Level 5+: white (1,1,1)

Upload all levels with `glTexImage2D`. Apply to a large quad that recedes into the distance. The surface should show distinct color bands at different distances, revealing exactly which mip level the GPU selects at each depth.

This texture is also a useful debugging tool for any future scene where you suspect incorrect mip selection.
