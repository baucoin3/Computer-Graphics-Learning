# L8I — Trilinear Filtering
**Theme A1: UV Mapping + Texture Fundamentals | Stage 3 of 5**

---

## The Problem Without This Concept

Bilinear filtering + mipmaps selects the nearest mip level and applies bilinear filtering within it. This works, but introduces a visual artifact: as an object moves away from the camera, the LOD crosses integer boundaries, causing the mip level to suddenly jump from level 2 to level 3. The result is a visible pop — a sudden change in blurriness, clearly visible on surfaces that recede smoothly into the distance.

Trilinear filtering eliminates this pop by blending between the two adjacent mip levels proportionally to the fractional LOD.

---

## Part A — Theory

### Prerequisite: The Mipmap Chain

Trilinear filtering requires mipmaps. A mipmap chain is a sequence of precomputed, pre-filtered versions of a texture at progressively halved resolution:

- Level 0: full resolution (e.g. 512×512)
- Level 1: half resolution (256×256)
- Level 2: quarter resolution (128×128)
- ...
- Level k: 512/2^k × 512/2^k
- Final level: 1×1

Each level is a correctly blurred version of the previous. The GPU can select any level based on the current viewing geometry. Mipmap generation is covered fully in L8K.

### Bilinear with Mip Selection: GL_LINEAR_MIPMAP_NEAREST

Before trilinear, there is an intermediate mode: bilinear filtering + nearest mip level selection. The GPU computes the correct LOD level as a float (e.g., 2.7), rounds to the nearest integer (level 3), and applies bilinear filtering within that level.

This is `GL_LINEAR_MIPMAP_NEAREST` in OpenGL. It is an improvement over GL_LINEAR alone (aliasing is mostly fixed) but produces mip popping at mip boundaries.

**What mip popping looks like**: a floor tile viewed at gradually increasing distance. At a specific distance, the displayed texture suddenly becomes noticeably blurrier — as if someone swapped a sharp version for a blurry one. This distance corresponds to where LOD crosses an integer boundary and the GPU switches from level 2 to level 3.

### Trilinear Filtering: Interpolating Between Mip Levels

Trilinear filtering takes the fractional LOD and uses it to blend between TWO adjacent mip levels.

**LOD computation** (simplified — full derivation in L8L):

The GPU computes a float LOD `d` based on how fast the UV coordinates change in screen space. If the texture footprint is 4 texels wide per pixel, d ≈ log2(4) = 2.

**Trilinear blending**:

```
d_low  = floor(d)      // lower mip level (higher resolution)
d_high = ceil(d)       // upper mip level (lower resolution)
frac   = d - d_low     // fractional part: 0.0 = use only lower, 1.0 = use only upper
```

For d = 2.7: d_low = 2, d_high = 3, frac = 0.7.

```
color_low  = bilinear(mip[d_low],  uv)   // bilinear sample from mip level 2
color_high = bilinear(mip[d_high], uv)   // bilinear sample from mip level 3

result = lerp(color_low, color_high, frac)
       = color_low × (1 - frac) + color_high × frac
       = color_low × 0.3 + color_high × 0.7
```

This means: at LOD = 2.7, the result is 30% from the sharper level 2 and 70% from the blurrier level 3.

**Why "trilinear"**: three dimensions of linear interpolation.
- Two horizontal bilinear blends (one within mip level d_low, one within d_high) → each is a 2D bilinear operation
- One vertical blend between those two results → the third linear interpolation

Together: 3D linear interpolation in the space (u, v, mip_level).

### Texel Cost: 8 Reads Per Sample

Bilinear: 4 texel reads (2×2 grid from one mip level).
Trilinear: 4 reads from level d_low + 4 reads from level d_high = 8 texel reads per sample.

In theory this is 2× the cost of bilinear. In practice on modern GPUs:
- Texture hardware is heavily pipelined and optimized for exactly this access pattern
- The 4 reads per level are spatially adjacent → high cache hit rate
- The two levels occupy different memory regions but mip data is compact
- On NVIDIA Turing / AMD RDNA2, trilinear cost over bilinear is near-zero for typical scenes

### OpenGL Setup

```c
// After binding texture and uploading level 0:
glGenerateMipmap(GL_TEXTURE_2D);

glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);  // trilinear
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);                // bilinear mag
```

`GL_LINEAR_MIPMAP_LINEAR` = linear filtering within each mip level + linear interpolation between mip levels = trilinear.

`GL_LINEAR_MIPMAP_NEAREST` = linear within a level + nearest level selection = bilinear+mips (no pop smoothing).

MAG_FILTER does not support `_MIPMAP_*` modes — magnification always uses level 0 (the full-resolution level), so mip interpolation is irrelevant. Using `GL_LINEAR_MIPMAP_LINEAR` for MAG is invalid and produces undefined behavior (usually silently falls back to GL_LINEAR).

### LOD Bias

The GPU's automatic LOD selection can be overridden by a constant bias:

```c
glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, bias);
```

- Positive bias: adds to the computed LOD → selects a higher (blurrier) mip level than geometry would indicate. Effect: softer, dreamier look. Useful in cinematics.
- Negative bias: selects a lower (sharper) mip level. Effect: crisper appearance at the cost of more aliasing.

LOD bias in GLSL: `textureLod(sampler, uv, computedLOD + bias)` — manual LOD with offset.

### The Remaining Limitation: Isotropic Footprint

Trilinear filtering selects a mip level based on the MAXIMUM texel footprint size in either axis. For a floor at a grazing angle, the footprint might be 1 texel wide and 64 texels long. Trilinear selects level log2(64) = 6 — appropriate for the long axis, but massively over-blurring the short axis (1 texel gets 64× more blur than it should).

This is the problem anisotropic filtering (L8M–L8O) solves. Trilinear + anisotropic is the standard configuration for all real-world game textures.

---

## Part B — Numeric Worked Example

### Trilinear Sample: Complete Trace

Texture at level 0: 4×4 (indexed as T0[row][col]). Values:
```
Row 3: [0.9, 0.7, 0.5, 0.3]
Row 2: [0.8, 0.6, 0.4, 0.2]
Row 1: [0.7, 0.5, 0.3, 0.1]
Row 0: [0.6, 0.4, 0.2, 0.0]
```

Level 1 (2×2), precomputed as box-filter averages of 2×2 blocks from level 0:

```
T1[1][0] = avg(T0[2][0], T0[3][0], T0[2][1], T0[3][1])
         = (0.8 + 0.9 + 0.6 + 0.7) / 4
         = 3.0 / 4 = 0.750

T1[1][1] = avg(T0[2][2], T0[3][2], T0[2][3], T0[3][3])
         = (0.4 + 0.5 + 0.2 + 0.3) / 4
         = 1.4 / 4 = 0.350

T1[0][0] = avg(T0[0][0], T0[1][0], T0[0][1], T0[1][1])
         = (0.6 + 0.7 + 0.4 + 0.5) / 4
         = 2.2 / 4 = 0.550

T1[0][1] = avg(T0[0][2], T0[1][2], T0[0][3], T0[1][3])
         = (0.2 + 0.3 + 0.0 + 0.1) / 4
         = 0.6 / 4 = 0.150
```

Level 1:
```
Row 1: [0.750, 0.350]
Row 0: [0.550, 0.150]
```

**Sample UV = (0.4, 0.3), LOD = 0.6**

d_low = 0, d_high = 1, frac = 0.6

**Bilinear from level 0 at UV (0.4, 0.3):**

```
tx = 0.4 × 4 = 1.6,  ty = 0.3 × 4 = 1.2
i = 1, j = 1, fx = 0.6, fy = 0.2
```

Surrounding texels:
```
T0[1][1] = 0.5   (bottom-left)
T0[1][2] = 0.3   (bottom-right)
T0[2][1] = 0.6   (top-left)
T0[2][2] = 0.4   (top-right)
```

Bottom row: `0.5 × (1-0.6) + 0.3 × 0.6 = 0.5 × 0.4 + 0.3 × 0.6 = 0.20 + 0.18 = 0.38`
Top row: `0.6 × (1-0.6) + 0.4 × 0.6 = 0.6 × 0.4 + 0.4 × 0.6 = 0.24 + 0.24 = 0.48`
Vertical: `0.38 × (1-0.2) + 0.48 × 0.2 = 0.38 × 0.8 + 0.48 × 0.2 = 0.304 + 0.096 = 0.400`

color_low = **0.400**

**Bilinear from level 1 at UV (0.4, 0.3):**

Level 1 is 2×2.
```
tx = 0.4 × 2 = 0.8,  ty = 0.3 × 2 = 0.6
i = 0, j = 0, fx = 0.8, fy = 0.6
```

Surrounding texels (clamping at boundary — only 2×2 exists so j+1 = 1 = valid):
```
T1[0][0] = 0.550  (bottom-left)
T1[0][1] = 0.150  (bottom-right)
T1[1][0] = 0.750  (top-left)
T1[1][1] = 0.350  (top-right)
```

Bottom: `0.550 × (1-0.8) + 0.150 × 0.8 = 0.550 × 0.2 + 0.150 × 0.8 = 0.110 + 0.120 = 0.230`
Top: `0.750 × (1-0.8) + 0.350 × 0.8 = 0.750 × 0.2 + 0.350 × 0.8 = 0.150 + 0.280 = 0.430`
Vertical: `0.230 × (1-0.6) + 0.430 × 0.6 = 0.230 × 0.4 + 0.430 × 0.6 = 0.092 + 0.258 = 0.350`

color_high = **0.350**

**Trilinear blend (frac = 0.6):**

```
result = color_low × (1 - frac) + color_high × frac
       = 0.400 × (1 - 0.6) + 0.350 × 0.6
       = 0.400 × 0.4 + 0.350 × 0.6
       = 0.160 + 0.210
       = 0.370
```

**Summary:**
- Pure level 0 (bilinear only): 0.400
- Pure level 1 (lower resolution): 0.350
- Trilinear at LOD 0.6: 0.370 (60% toward level 1, 40% toward level 0)

At LOD = 0.6, the GPU is 60% of the way to needing the blurrier level — the result correctly interpolates between the two.

---

## Pitfalls

**Pitfall 1: GL_LINEAR_MIPMAP_LINEAR without glGenerateMipmap**

Mistake: setting MIN_FILTER to GL_LINEAR_MIPMAP_LINEAR but forgetting to call glGenerateMipmap.

Symptom: texture appears black at any distance that triggers mip lookup (which is any distance with minification). Up close (magnification → level 0), texture may appear correct. At distance, black.

This is the most common "texture disappears at distance" bug.

Fix: always call `glGenerateMipmap(GL_TEXTURE_2D)` after `glTexImage2D` and before rendering. Call it while the texture is still bound.

**Pitfall 2: Setting MAG_FILTER to GL_LINEAR_MIPMAP_LINEAR**

Mistake: using `GL_LINEAR_MIPMAP_LINEAR` for MAG_FILTER.

Symptom: on some drivers, silently falls back to GL_LINEAR. On others, may produce GL_INVALID_ENUM. Always an error in strict OpenGL.

Fix: MAG_FILTER must be `GL_NEAREST` or `GL_LINEAR` only. The `_MIPMAP_*` variants are only valid for MIN_FILTER.

**Pitfall 3: LOD_BIAS set on a shadow map texture**

Mistake: applying LOD_BIAS to a shadow map texture that uses depth comparison.

Symptom: the LOD bias shifts which mip level the depth value is read from. Shadow maps should always read from level 0 (full resolution) for accurate depth comparison. A positive bias causes the shadow map to sample from a blurred (lower-resolution) depth — which changes the effective depth value and can cause large areas to incorrectly appear in shadow (Peter-Panning).

Fix: for shadow map textures, explicitly set `GL_TEXTURE_MAX_LEVEL = 0` to prevent any mip lookup beyond level 0. Use `textureLod(shadowMap, uv, 0.0)` in the shader.

---

## What to Build

**Exercise 1: Add mipmaps to existing textured quad**

Take the textured quad project from L8C exercises. Add:
1. `glGenerateMipmap(GL_TEXTURE_2D)` after the texture upload
2. Set MIN_FILTER to `GL_LINEAR_MIPMAP_LINEAR`
3. Set MAG_FILTER to `GL_LINEAR`

Then tilt the quad so it recedes into the distance (rotate it around its X axis by ~60°). Look at the distant end without mipmaps (remove the above lines) vs with mipmaps. Note: without mipmaps, the far end shimmers. With trilinear, it is smoothly blurred.

**Exercise 2: LOD bias experiment**

Set `GL_TEXTURE_LOD_BIAS` to +2.0 on a textured floor. Render. Take a screenshot. Then set it to -2.0. Render. Take a screenshot. Compare the three states (no bias, +2.0, -2.0). Describe:
- What does +2.0 do to the sharpness?
- What does -2.0 do? What artifact appears?
- Can you see the mip level boundaries as color bands if you use a texture that has each mip level colored differently (red for level 0, green for level 1, etc.)?

**Exercise 3: Mip level visualization texture**

Create a texture manually in code where:
- Level 0: all texels (1.0, 0.0, 0.0) red
- Level 1: all texels (0.0, 1.0, 0.0) green
- Level 2: all texels (0.0, 0.0, 1.0) blue
- Level 3: all texels (1.0, 1.0, 0.0) yellow
- Levels 4+: white

Upload each level separately with `glTexImage2D(..., level, ...)`. Apply to a floor quad. At different distances, different colors appear. This gives a direct visual of which mip level the GPU is using at each point on the surface.
