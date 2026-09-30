# L8H — Nearest-Neighbor and Bilinear Filtering
**Theme A1: UV Mapping + Texture Fundamentals | Stage 3 of 5**

---

## The Problem Without This Concept

L8G established that a continuous UV coordinate must be converted to a discrete texel value, and that just picking the nearest texel produces aliasing under minification and blockiness under magnification. We need better reconstruction filters. This lesson derives the two fundamental ones: nearest-neighbor (the trivial case) and bilinear (the standard case), with full mathematical derivation of the bilinear interpolation formula.

---

## Part A — Theory

### Nearest-Neighbor Filtering

Given a fractional texel coordinate (tx, ty), nearest-neighbor rounds to the nearest integer:

```
i = floor(tx + 0.5)    // equivalent to round(tx)
j = floor(ty + 0.5)    // equivalent to round(ty)
result = texel[j][i]
```

Every fractional position within a 1×1 texel region returns the same color — the center texel. Adjacent screen pixels that map to the same texel region all get identical colors.

**Magnification appearance**: square blocks of identical color. The texture looks like it is made of large colored squares — pixelated. The block size matches the texel size in screen pixels.

**Minification appearance**: one texel chosen from potentially hundreds. The choice is essentially arbitrary from frame to frame → shimmering.

**When nearest-neighbor is correct:**
- **Pixel art and retro games**: the blocky, pixelated look is intentional. Each texel should render as a distinct colored block. Bilinear blurring would destroy the crisp pixel aesthetic.
- **Data textures**: lookup tables, integer index maps, classification textures where interpolation between values is meaningless (e.g., a texture where each color represents a material type, not a gradient).
- **Shadow maps** (depth comparison): the depth value should not be interpolated — you compare against exact stored depths. Use `GL_NEAREST` for shadow map sampling.

OpenGL:
```c
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
```

In Unreal Engine 5, for pixel art sprites: Texture → Filter → Nearest.

### Bilinear Filtering: The Idea

Instead of rounding to one texel, bilinear filtering looks at the **4 texels surrounding the fractional sample point** and blends between them in proportion to distance.

The name "bilinear" means: linear interpolation in both the horizontal and vertical direction — two linear interpolations combined.

### Bilinear Filtering: Full Derivation

Given fractional texel coordinate (tx, ty):

**Step 1: Find the surrounding 4 texels**

```
i = floor(tx)       // left column index
j = floor(ty)       // bottom row index
fx = tx - i         // fractional part in x: how far right from left texel center
fy = ty - j         // fractional part in y: how far up from bottom texel center
```

So fx ∈ [0,1), fy ∈ [0,1). The 4 surrounding texels:

```
T[j  ][i  ] = bottom-left  (weight depends on distance to this corner)
T[j  ][i+1] = bottom-right
T[j+1][i  ] = top-left
T[j+1][i+1] = top-right
```

**Step 2: Interpolate horizontally across the bottom row**

At the bottom row (row j), interpolate between left and right based on fx:

```
bottom = T[j][i] × (1 - fx) + T[j][i+1] × fx
```

When fx = 0: all weight on T[j][i] (left). When fx = 1: all weight on T[j][i+1] (right). When fx = 0.5: equal blend of both.

**Step 3: Interpolate horizontally across the top row**

```
top = T[j+1][i] × (1 - fx) + T[j+1][i+1] × fx
```

**Step 4: Interpolate vertically between bottom and top**

```
result = bottom × (1 - fy) + top × fy
```

**Step 5: Substitute (the full bilinear formula)**

```
result = T[j][i]   × (1-fx) × (1-fy)
       + T[j][i+1] × fx     × (1-fy)
       + T[j+1][i] × (1-fx) × fy
       + T[j+1][i+1] × fx   × fy
```

The four weights are: `(1-fx)(1-fy)`, `fx(1-fy)`, `(1-fx)fy`, `fx·fy`.

**Verify the weights sum to 1:**

```
(1-fx)(1-fy) + fx(1-fy) + (1-fx)fy + fx·fy
= (1-fy)[(1-fx) + fx] + fy[(1-fx) + fx]
= (1-fy)(1) + fy(1)
= 1 - fy + fy
= 1
```

The weights always sum to 1 — a properly normalized weighted average.

### What the Weights Mean Geometrically

The weight for T[j][i] (bottom-left) is `(1-fx)(1-fy)`. This is the area of the rectangle from the sample point to the top-right corner of the texel grid cell — the area opposite to that corner. The farther the sample point is from a texel center, the smaller that texel's contribution.

At the exact center of a texel (fx=0, fy=0): T[j][i] gets weight 1, all others get 0. At the midpoint between 4 texels (fx=0.5, fy=0.5): all 4 get equal weight 0.25.

### Bilinear vs Unweighted 2×2 Average

Some explanations describe bilinear as "averaging the surrounding 4 texels." This is WRONG. An unweighted average gives equal 0.25 weight to all 4. That is only correct at fx=0.5, fy=0.5. At all other positions, the weights differ.

Bilinear uses **distance-proportional weights** — a sample near the center of one texel gets almost all weight from that texel. A sample at the border between two texels gets approximately equal weight from each. This is correct reconstruction behavior.

### Bilinear for Magnification and Minification

**Magnification** (sample point in a large texel region): bilinear smoothly interpolates between texel centers → smooth gradient instead of blocky regions. The texture looks blurry rather than pixelated. This is usually correct for photographic textures — you would rather have smooth blur than jagged blocks.

**Minification** (sample point covers many texels): bilinear still samples only 4 texels regardless of how large the footprint is. If the footprint is 32×32 texels, bilinear picks 4 of those 32×32 = 1024 texels and interpolates. The result is better than nearest-neighbor's 1 texel, but still a wildly inadequate representation of the footprint. Aliasing persists.

Bilinear alone is not sufficient for minification. Bilinear + mipmaps (trilinear) is needed.

### OpenGL Setup for Bilinear

```c
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);    // bilinear mag
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);    // bilinear min (no mips)
```

`GL_LINEAR` for both — this is "bilinear" mode. No mipmaps used.

**Important**: setting `GL_TEXTURE_MIN_FILTER` to `GL_LINEAR` without generating mipmaps is valid in modern OpenGL (core profile). But the default MIN_FILTER in OpenGL is `GL_NEAREST_MIPMAP_LINEAR`, which requires mipmaps — if you don't generate them, the texture returns black or OpenGL reports an incomplete texture. Always set both filter modes explicitly.

---

## Part B — Numeric Worked Example

### Complete Bilinear Sample

Texture (4×4, grayscale float values, using row/column indexing where row 0 = bottom):

```
Row 3 (top):    [0.8,  0.6,  0.4,  0.2]
Row 2:          [0.7,  0.5,  0.3,  0.1]
Row 1:          [0.6,  0.4,  0.2,  0.0]
Row 0 (bottom): [0.5,  0.3,  0.1,  0.0]
```

So `T[row][col]`: T[0][0]=0.5, T[0][1]=0.3, T[1][0]=0.6, T[1][1]=0.4, etc.

**Sample at UV (0.6, 0.6):**

Texel coordinates:
```
tx = 0.6 × 4 = 2.4
ty = 0.6 × 4 = 2.4
```

Integer parts:
```
i = floor(2.4) = 2
j = floor(2.4) = 2
fx = 2.4 - 2 = 0.4
fy = 2.4 - 2 = 0.4
```

Four surrounding texels:
```
T[j  ][i  ] = T[2][2] = 0.3   (bottom-left of the 4)
T[j  ][i+1] = T[2][3] = 0.1   (bottom-right)
T[j+1][i  ] = T[3][2] = 0.4   (top-left)
T[j+1][i+1] = T[3][3] = 0.2   (top-right)
```

Weights:
```
w_BL = (1-fx)(1-fy) = (1-0.4)(1-0.4) = 0.6 × 0.6 = 0.36
w_BR = fx(1-fy)     = 0.4 × (1-0.4)  = 0.4 × 0.6 = 0.24
w_TL = (1-fx)(fy)   = (1-0.4) × 0.4  = 0.6 × 0.4 = 0.24
w_TR = fx·fy        = 0.4 × 0.4      = 0.16
```

Sum check: 0.36 + 0.24 + 0.24 + 0.16 = 1.00. ✓

Weighted sum:
```
result = T[2][2] × 0.36 + T[2][3] × 0.24 + T[3][2] × 0.24 + T[3][3] × 0.16
       = 0.3 × 0.36   + 0.1 × 0.24    + 0.4 × 0.24    + 0.2 × 0.16
       = 0.108         + 0.024         + 0.096         + 0.032
       = 0.260
```

**Alternative step-by-step (horizontal then vertical):**

Bottom row horizontal:
```
bottom = T[2][2] × (1-0.4) + T[2][3] × 0.4
       = 0.3 × 0.6 + 0.1 × 0.4
       = 0.18 + 0.04
       = 0.22
```

Top row horizontal:
```
top = T[3][2] × (1-0.4) + T[3][3] × 0.4
    = 0.4 × 0.6 + 0.2 × 0.4
    = 0.24 + 0.08
    = 0.32
```

Vertical blend:
```
result = bottom × (1-0.4) + top × 0.4
       = 0.22 × 0.6 + 0.32 × 0.4
       = 0.132 + 0.128
       = 0.260
```

Both methods give 0.260. ✓

**Compare to nearest-neighbor**: nearest rounds tx=2.4 → 2, ty=2.4 → 2. T[2][2] = 0.3. Bilinear gives 0.260, which is a blend weighted toward 0.3 (the closest texel, which has the largest weight 0.36).

### Bonus: Four-Texel Case with Color

Texture 2×2, full color:
```
T[0][0] = (1.0, 0.0, 0.0)   red         (bottom-left)
T[0][1] = (0.0, 1.0, 0.0)   green       (bottom-right)
T[1][0] = (0.0, 0.0, 1.0)   blue        (top-left)
T[1][1] = (1.0, 1.0, 0.0)   yellow      (top-right)
```

Sample at UV (0.3, 0.7):
```
tx = 0.3 × 2 = 0.6,  ty = 0.7 × 2 = 1.4
i = 0,  j = 1,  fx = 0.6,  fy = 0.4
```

Surrounding texels: T[1][0]=blue, T[1][1]=yellow, T[2][0], T[2][1] — but row 2 doesn't exist in a 2×2 texture. The GPU clamps or wraps. With GL_CLAMP_TO_EDGE, row 2 → row 1.

Actually fy = 0.4 with j=1 → top row = T[2][...]. Since texture has only rows 0–1, j+1=2 → clamped to 1. So:

```
T[j][i] = T[1][0] = (0,0,1)   blue
T[j][i+1] = T[1][1] = (1,1,0) yellow
T[j+1][i] = T[1][0] = (0,0,1) blue (clamped)
T[j+1][i+1] = T[1][1] = (1,1,0) yellow (clamped)
```

All top-row texels are same as bottom → result is purely the horizontal blend:

```
result = blue × (1-0.6) + yellow × 0.6
       = (0,0,1)×0.4 + (1,1,0)×0.6
       = (0, 0, 0.4) + (0.6, 0.6, 0)
       = (0.6, 0.6, 0.4)
```

A slightly blue yellow-green. Correct — we're at UV (0.3, 0.7) in a 2×2 texture, which maps into the top half and 30% from the left — the blue-yellow border.

---

## Pitfalls

**Pitfall 1: Setting GL_TEXTURE_MIN_FILTER to GL_LINEAR without mipmaps**

Mistake: no mipmap generation, but MIN_FILTER = GL_LINEAR.

Symptom: texture looks fine close up, but at distance still shimmers. Bilinear in MIN mode still only reads 4 texels — insufficient for large footprints. The improvement over GL_NEAREST at distance is marginal.

Fix: generate mipmaps and use GL_LINEAR_MIPMAP_LINEAR (trilinear). GL_LINEAR for MIN is only acceptable when the surface never appears minified (fills the screen at all viewing distances).

**Pitfall 2: Default MIN_FILTER is GL_NEAREST_MIPMAP_LINEAR — requires mipmaps**

Mistake: not setting MIN_FILTER explicitly. OpenGL defaults to `GL_NEAREST_MIPMAP_LINEAR`. Without mipmaps generated, this filter mode treats the texture as "mipmap incomplete" and returns black.

This is one of the most common reasons for a texture appearing black in a new OpenGL project.

Fix: after creating a texture, always set both MIN and MAG filter explicitly:
```c
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
glGenerateMipmap(GL_TEXTURE_2D);
```

**Pitfall 3: Bilinear filtering wrapping across texture boundary**

Mistake: surface uses GL_REPEAT, texture has non-seamless edges (left edge color differs from right edge color). Bilinear sampling at U near 1.0 blends the last column of texels with the first column (wrap). If the texture does not tile seamlessly, a visible seam appears.

Symptom: a thin stripe of wrong color where the texture wraps, especially visible at magnification.

Fix: either make the texture seamlessly tileable (ensure left edge = right edge, top edge = bottom edge) or use GL_CLAMP_TO_EDGE if tiling is not needed.

---

## What to Build

**Exercise 1: Two-texture filter comparison**

Load the same 512×512 brick texture into two GL texture objects. Set object A to GL_NEAREST for both MAG and MIN. Set object B to GL_LINEAR for both MAG and MIN (no mipmaps). Render two quads side by side. Move the camera close enough that the texture is magnified 4× on screen. Observe and describe the visual difference: what does blocky look like vs smooth look like at the same UV scale?

**Exercise 2: Manual bilinear calculation**

Texture 2×2, full RGB:
```
T[0][0] = (1.0, 0.0, 0.0)   red
T[0][1] = (0.0, 1.0, 0.0)   green
T[1][0] = (0.0, 0.0, 1.0)   blue
T[1][1] = (1.0, 1.0, 0.0)   yellow
```

Compute the bilinear sample at UV (0.3, 0.7) for this 2×2 texture (W=2, H=2). Show every step:
1. Texel coordinates tx, ty
2. i, j, fx, fy
3. Four surrounding texels (handle boundary with GL_CLAMP_TO_EDGE)
4. Four weights
5. Weight sum verification
6. Final RGB result

**Exercise 3: Identify where bilinear helps and where it does not**

For each scenario, state whether bilinear (without mipmaps) gives noticeably better results than nearest-neighbor, and explain why:
1. A 1024×1024 texture on a quad that fills the full 1920×1080 screen (slight magnification)
2. A 64×64 texture on a quad that appears 8×8 pixels on screen (8× minification)
3. A 64×64 checkerboard texture on a quad appearing 64×64 pixels exactly (1:1 mapping)
4. A 512×512 photo-realistic wood texture on a floor receding to the horizon
