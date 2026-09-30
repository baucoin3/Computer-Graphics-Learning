# L8M — Why Isotropic Filters Fail on Oblique Surfaces
**Theme A1: UV Mapping + Texture Fundamentals | Stage 5 of 5**

---

## Introduction: The Problem We Did Not Know We Had

By the end of Stage 4 (L8J–L8L), you had a complete, functioning texture pipeline: UV coordinates flowing from CPU to GPU through the VBO, bilinear filtering eliminating the blocky nearest-neighbor artifact, mipmaps eliminating the shimmering aliasing at distance, and trilinear filtering blending between mip levels to remove the abrupt mip transition seam. Every aliasing problem we set out to solve was solved.

Now walk your textured scene over to a floor. The floor is a checkerboard or brick tile pattern — anything with repeating detail. Position the camera at eye height, roughly 1.7 meters, and look toward the horizon. Directly beneath your feet, the texture is sharp — you can read every brick seam, every grout line. Then your eye travels forward. Somewhere around 3 to 5 meters ahead, the sharpness collapses. The floor becomes a muddy smear. The brick pattern dissolves. By the time you're looking at the floor 20 meters ahead, there is nothing but a uniform gray-brown wash, even though the original texture has perfectly sharp high-frequency detail that should be visible at that distance.

You have trilinear filtering enabled. You have mipmaps. You did everything right. The aliasing is gone. But the blur is wrong — far more aggressive than it needs to be. The floor looks like it was painted with a soft brush, not tiled with crisp masonry.

This is not a bug in your implementation. This is the fundamental limitation of isotropic filtering. Mipmaps and trilinear filtering solve the problem they were designed to solve — and that problem is not the one you are looking at now.

This lesson diagnoses exactly what is happening and why. The next two lessons (L8N, L8O) will show how anisotropic filtering fixes it and how to enable it in code.

---

## Part A: Theory

### Section 1 — Isotropic vs. Anisotropic: Definitions

The word *isotropic* comes from Greek: *iso* (equal) + *tropos* (direction). An isotropic process behaves the same in every direction. A circle is isotropic — its radius is equal in all directions. A sphere is isotropic. A Gaussian blur with equal sigma in X and Y is isotropic.

The word *anisotropic* means the opposite: the process behaves differently in different directions. An ellipse is anisotropic — its extent in one axis is greater than in the other. A Gaussian blur with sigma_x = 1 and sigma_y = 10 is anisotropic.

Mipmaps are isotropic filters. When you generate a mip chain, you halve both width and height at each level. Level 0: 256×256. Level 1: 128×128. Level 2: 64×64. Every level is a uniformly blurred, uniformly downscaled version of the previous — equal reduction in both texture axes. When the GPU selects a mip level and samples from it, it is applying a filter that is equal in both U and V directions.

This is correct behavior for a texture viewed head-on: if a screen pixel maps to a 4×4 region in texture space, you need to average 4 texels in U and 4 texels in V. A mip level that has reduced the texture by a factor of 4 in each direction gives you exactly that — one sample in the downscaled texture corresponds to a 4×4 average in the original. Isotropic filter, isotropic footprint, perfect match.

The problem is that the pixel's footprint in texture space is not always square.

---

### Section 2 — The Pixel Footprint in Texture Space

Every screen pixel corresponds to a small region of the world surface. When a textured surface is rasterized, the GPU needs to know: what region of the texture does this pixel cover? That region is called the **pixel footprint** in texture space.

The shape of that footprint depends on two things: the geometry of the surface and the angle at which the camera views it.

**Case 1: Surface viewed head-on (perpendicular incidence).**

The camera looks straight at a flat wall. The wall fills the screen approximately uniformly. Each screen pixel maps to a roughly square patch of the wall, and that wall patch maps to a roughly square region in texture space. The U and V axes of the texture are both foreshortened by the same amount. The footprint is approximately square (or at least has a low aspect ratio). An isotropic mip level is the correct filter.

**Case 2: Surface viewed at a grazing angle (oblique incidence).**

The camera is at eye height, looking toward the horizon. The floor stretches away from you. A screen pixel near the horizon covers a long thin strip of the floor surface — narrow in the left-right direction (across the receding direction), long in the forward direction (along the receding direction). That long thin strip maps to a long thin region in texture space: the footprint is an elongated ellipse.

The U and V axes of the texture are foreshortened very differently. In the across direction (say, U), the pixel covers a small number of texels — perhaps 2. In the along direction (say, V), the pixel covers a large number of texels — perhaps 64.

The footprint aspect ratio is 1:32 or worse. This is massively anisotropic.

---

### Section 3 — How the LOD Formula Works (and Where It Breaks)

You derived this in L8L, but let us revisit it carefully because the failure mode is in the formula itself.

The GPU computes the texture-space derivatives of the UV coordinates per fragment. In GLSL, these are accessible as `dFdx(vTexCoord)` and `dFdy(vTexCoord)`. More precisely:

```
dFdx(uv) = the rate of change of uv as you step one pixel to the RIGHT in screen space
dFdy(uv) = the rate of change of uv as you step one pixel DOWN in screen space
```

If you multiply by the texture dimensions (W, H) to get these in texel units rather than normalized UV units, you get:

```
dpdx = dFdx(uv) * vec2(W, H)   — change in texels per screen pixel in X
dpdy = dFdy(uv) * vec2(H, H)   — change in texels per screen pixel in Y
```

Each of these is a 2D vector. The length of `dpdx` tells you how many texels the footprint spans as you move one pixel right on screen. The length of `dpdy` tells you how many texels the footprint spans as you move one pixel down on screen.

The standard LOD formula is:

```
p = max(||dpdx||, ||dpdy||)
LOD = log2(p)
```

The max is critical. The LOD is chosen based on the **maximum** of the two footprint extents. This ensures that the mip level is large enough to avoid aliasing along the worst axis.

For a square footprint with ||dpdx|| = 4 and ||dpdy|| = 4: max = 4, LOD = 2, use mip level 2 (which reduces 256×256 to 64×64). Four texels become one sample in each direction. Correct.

For an elongated footprint with ||dpdx|| = 2 and ||dpdy|| = 64: max = 64, LOD = log2(64) = 6. Use mip level 6 (256×256 reduces to 4×4). Now examine what happens to each axis:

**Along the V axis (length 64):** at level 6, 64 texels have become 1 texel. The filter correctly averages 64 texels of detail along V. This is what you want.

**Along the U axis (length 2):** at level 6, those 2 texels have been downscaled by a factor of 64. Two texels at full resolution become 2/64 = 0.031 texels at level 6. The footprint is 0.031 texels wide in U. Sampling at this level in the U direction is wildly over-blurred — you are applying 64× more blurring than the 2-texel footprint requires. You needed to average 2 texels in U; you averaged 64.

The result: the floor texture is blurred 32× more than necessary in the across-floor direction. All the fine horizontal detail — grout lines, tile edges, pebble patterns — is obliterated by a filter that was only supposed to average 2 texels but averaged 64 instead.

---

### Section 4 — Why "Max" Was the Right Rule for the Wrong Problem

The max rule is not a mistake. It is the correct conservative choice for an isotropic filter. If you have only one mip level to pick, and you must avoid aliasing along both axes simultaneously, you must pick the level appropriate for the worst axis. Picking a smaller level would correctly filter U but leave V under-filtered, causing aliasing (shimmering, Moire patterns) along V. The max rule prevents that.

The real problem is that an isotropic filter is being applied to an anisotropic footprint. A single mip level cannot simultaneously be correct for a 2-texel axis and a 64-texel axis. The only correct solution is to use a filter that can operate differently in different directions — an anisotropic filter.

---

### Section 5 — What the Correct Filter Would Do

Think about the anisotropic footprint geometrically. You have a screen pixel whose texture-space footprint is a thin ellipse: 2 texels wide in U, 64 texels long in V.

The correct filter should:

1. Pick a mip level appropriate for the **short axis** (2 texels → level 1, which gives you 1-texel coverage in U). This preserves sharpness in U.
2. Recognize that the long axis (64 texels) spans many more texels than the mip level can cover in a single sample. A single bilinear sample at level 1 covers roughly a 2×2 texel region — nowhere near the 64 texels needed along V.
3. Take multiple samples distributed along the long axis, at the mip level selected in step 1. Each sample covers ~2 texels in V (correct for level 1). To cover 64 texels, you need about 32 samples. With a hardware cap of 16 samples, you take 16 samples spaced 64/16 = 4 texels apart along V.
4. Average those 16 samples.

The result: U axis is filtered correctly (2 texels → 1 sample, no over-blur). V axis is filtered over 64 texels via 16 samples (some residual aliasing from under-sampling along V, but far better than the alternative). The floor texture remains sharp in the across direction while correctly blurring detail along the receding direction.

This is the essence of anisotropic filtering: **probe along the major axis at a mip level appropriate for the minor axis, averaging multiple samples.**

---

### Section 6 — The Anisotropy Ratio

The anisotropy ratio quantifies how elongated the footprint is:

```
N = max(||dpdx||, ||dpdy||) / min(||dpdx||, ||dpdy||)
```

For the floor example above: N = 64 / 2 = 32.

N = 1 means a perfectly square footprint. No anisotropy. Isotropic filtering is correct.
N = 2 means the footprint is twice as long as it is wide. Mild anisotropy.
N = 16 means 16:1. Severe anisotropy. Maximum hardware AF is needed.
N = 64 means 64:1. Beyond what hardware can perfectly handle at 16× AF.

Hardware anisotropic filtering specifies its level in terms of this ratio: 2× AF handles up to N = 2. 4× AF handles up to N = 4. 16× AF handles up to N = 16. Beyond the hardware limit, the filter still falls back to the max-axis mip level rather than clamping to the hardware limit and continuing — the anisotropy ratio is clamped, and the mip level selection partially degrades.

The anisotropy ratio also gives you the improved LOD formula that AF uses internally:

```
LOD_af = log2(min(||dpdx||, ||dpdy||))     ← use the SHORT axis for LOD
N      = max / min                          ← how many samples to take along the long axis
```

This is the complete AF algorithm, summarized. The LOD is chosen for the short axis (preserving quality), and the long axis is handled by multiple samples.

---

### Section 7 — Visual Manifestation and Diagnostic

**Symptom of isotropic over-blur on an oblique surface:**

- Floor texture is sharp directly beneath the camera.
- Sharpness collapses rapidly as distance increases along the receding direction.
- Even with trilinear mipmaps enabled, the floor looks blurry past a few meters.
- The blur is directional — the across-floor direction (perpendicular to viewing direction, in the near field) looks sharp, but the along-floor direction (the receding direction) looks smeared.
- Checkerboard patterns show this most clearly: near tiles are distinct, far tiles merge into a uniform color.

**With anisotropic filtering enabled:**

- The floor remains sharp well into the distance.
- The distinct texture pattern (bricks, checkers, stone) is recognizable much further from the camera.
- The transition to blurriness is pushed far toward the horizon.
- Sharp detail in the across-floor direction is preserved at all distances where the texture has that detail.

**Diagnostic test:** Enable GL_TEXTURE_MAX_ANISOTROPY_EXT = 1 (effectively disabling AF, keeping trilinear). Then enable GL_TEXTURE_MAX_ANISOTROPY_EXT = 16. Compare a floor texture at a shallow viewing angle. The difference is dramatic and immediately visible.

---

### Section 8 — Industry Relevance: Where Anisotropic Footprints Appear

Any surface viewed at a non-perpendicular angle has some degree of anisotropic footprint. The cases where this matters most are the ones with the highest anisotropy ratio:

**Floors and terrain in games:** The single most common case. Any third-person or first-person game with a visible floor exhibits this. Ground textures in open-world games (grass, dirt, stone) are viewed at extreme grazing angles from player eye height. Without AF, the terrain looks muddy and low-resolution past a few meters. With 16× AF, the terrain retains crispness to the horizon. This is why AF is a standard setting in PC game graphics options — its visual impact on floors and terrain is immediately perceptible.

**Roads in racing and driving games:** The road recedes to the horizon directly ahead of the camera. The anisotropy ratio at 100m distance can easily exceed 100:1. AF is especially impactful here. Classic Moto GP and racing games used AF to maintain road texture quality at high speeds.

**Runways in flight simulators:** Runway surface textures viewed at shallow approach angles. The runway stretches away at 2–3 degrees above horizontal. Anisotropy ratio is extreme. Pre-AF flight simulators had notoriously blurry runways during approach. AF fixed this.

**Terrain in real-time strategy games:** Camera is typically at a 45–60 degree elevation angle. Terrain footprints are moderately anisotropic. AF at 4× or 8× is sufficient.

**Decals on horizontal surfaces:** Blood pools, tire marks, explosion scorch marks placed on floors. These are flat textured quads that share the floor's anisotropy. Same problem, same fix.

**Film and VFX (offline rendering):** Offline renderers (Arnold, RenderMan, Cycles) use EWA (Elliptical Weighted Average) filtering, which is the mathematically exact anisotropic filter. EWA traces the ellipse of the pixel footprint through texture space and correctly weights texels by their distance from the ellipse center. Hardware AF is a fast approximation to EWA. Arnold's `EWA` texture filter setting is the film industry standard for eliminating anisotropic blur artifacts.

---

## Part B: Numeric Worked Example

**Setup:** A flat floor textured with a 256×256 tile texture. Camera at eye height 1.7m, looking nearly horizontally — elevation angle 10° above horizontal (nearly grazing). We examine the floor at a point 10 meters ahead of the camera.

At this point, the partial derivatives of the UV coordinates have been computed by the GPU's rasterizer (via screen-space differences between adjacent fragments). We are given these values in normalized UV space and convert to texel space by multiplying by the texture dimensions (256, 256).

**Given derivatives in UV space:**
```
dFdx(uv) = (0.002, 0.000)     ← stepping one pixel right in screen space changes U by 0.002, V by 0.000
dFdy(uv) = (0.000, 0.040)     ← stepping one pixel down in screen space changes U by 0.000, V by 0.040
```

**Convert to texel space** (multiply each component by 256):
```
dpdx = dFdx(uv) × 256 = (0.002 × 256, 0.000 × 256) = (0.512, 0.000) texels per screen pixel
dpdy = dFdy(uv) × 256 = (0.000 × 256, 0.040 × 256) = (0.000, 10.24) texels per screen pixel
```

**Compute lengths:**
```
||dpdx|| = sqrt(0.512² + 0.000²) = sqrt(0.2621) = 0.512 texels
||dpdy|| = sqrt(0.000² + 10.24²) = sqrt(104.8576) = 10.24 texels
```

The footprint is 0.512 texels wide (in U, across the floor) and 10.24 texels tall (in V, along the receding direction).

---

### Isotropic LOD Selection (Standard Trilinear):

```
p = max(||dpdx||, ||dpdy||) = max(0.512, 10.24) = 10.24
LOD = log2(10.24)
```

Computing log2(10.24):
```
log2(8)  = 3        (since 2³ = 8)
log2(16) = 4        (since 2⁴ = 16)
10.24 is between 8 and 16, closer to 8.
10.24 / 8 = 1.28
log2(1.28) = log2(1 + 0.28) ≈ 0.28 / ln(2) ≈ 0.28 / 0.693 ≈ 0.364

Wait — let's be precise:
log2(10.24) = ln(10.24) / ln(2) = 2.3263 / 0.6931 = 3.357
```

So LOD ≈ 3.36. Trilinear blends between mip level 3 and mip level 4.

**At mip level 3**, the 256×256 texture has been halved 3 times:
```
Level 0: 256 × 256
Level 1: 128 × 128   (factor of 2 reduction)
Level 2:  64 ×  64   (factor of 4 reduction)
Level 3:  32 ×  32   (factor of 8 reduction)
```

Each texel at level 3 represents an 8×8 block of level-0 texels.

**Now check what the filter actually covers at level 3:**

U axis (short axis, ||dpdx|| = 0.512):
```
The footprint is 0.512 texels wide at level 0.
At level 3, every texel is 8× larger than at level 0.
The footprint width in level-3 texels = 0.512 / 8 = 0.064 level-3 texels.
```
This footprint is 0.064 texels wide at level 3. It is far less than 1 texel. The bilinear sample at level 3 covers a full texel (interpolating between adjacent texels), which spans approximately 8 level-0 texels in U. The filter is covering 8 level-0 texels in U when the actual footprint only spans 0.512. **The filter is 8 / 0.512 = 15.6× more blurry in U than it should be.**

V axis (long axis, ||dpdy|| = 10.24):
```
The footprint is 10.24 texels tall at level 0.
At level 3, every texel is 8× larger than at level 0.
The footprint height in level-3 texels = 10.24 / 8 = 1.28 level-3 texels.
```
This footprint is 1.28 texels tall at level 3. Bilinear filtering interpolates between two adjacent level-3 texels, covering about 2 level-3 texels (which span 16 level-0 texels). For a footprint of 1.28 level-3 texels, this is reasonable — slightly over-filtered in V but not dramatically so.

**Summary of isotropic failure:**
- U axis: over-blurred by factor of ~15.6. Crisp horizontal details (grout lines, tile edges) are destroyed.
- V axis: filtered approximately correctly.
- Visible result: floor looks muddy and blurry from left to right (across tiles), despite having full resolution texture detail available.

---

### Anisotropic Approach (What AF Does):

**Step 1: Identify the short and long axes.**
```
Short axis: ||dpdx|| = 0.512 texels (U direction)
Long axis:  ||dpdy|| = 10.24 texels (V direction)
```

**Step 2: Select mip level based on the SHORT axis.**
```
LOD = log2(0.512)
    = log2(1/2 × 1.024)
    = log2(1/2) + log2(1.024)
    = -1 + log2(1.024)

log2(1.024):
    = ln(1.024) / ln(2)
    = 0.02372 / 0.6931
    = 0.0342

LOD = -1 + 0.0342 = -0.9658
```

LOD = -0.966, which is below 0. Clamp to level 0. The short axis needs no downscaling at all — use the full-resolution mip level 0.

**Step 3: Compute the anisotropy ratio.**
```
N = max / min = 10.24 / 0.512 = 20.0
```

Hardware maximum is typically 16. Clamp N to 16.

**Step 4: Determine sample count and spacing.**

We take 16 bilinear samples along the V axis (the long axis direction), distributed evenly across the 10.24-texel extent of the footprint. Each sample uses mip level 0 (full resolution).

```
Total coverage along V: 10.24 texels
Number of samples: 16
Spacing between samples: 10.24 / 16 = 0.64 texels
```

Let the center of the footprint be at UV = (0.500, 0.500) (normalized). The major axis direction is the V axis: direction vector (0, 1) in UV space. The step in UV space is:

```
Step per sample = (0, 0.64) / 256 = (0, 0.0025) in normalized UV
Total span = 15 steps (from sample 0 to sample 15)
Start position = center - 7.5 × step = (0.500, 0.500) - 7.5 × (0, 0.0025)
               = (0.500, 0.500 - 0.01875)
               = (0.500, 0.48125)
```

**16 sample positions (in normalized UV):**
```
Sample 0:  (0.500, 0.48125)
Sample 1:  (0.500, 0.48375)
Sample 2:  (0.500, 0.48625)
Sample 3:  (0.500, 0.48875)
Sample 4:  (0.500, 0.49125)
Sample 5:  (0.500, 0.49375)
Sample 6:  (0.500, 0.49625)
Sample 7:  (0.500, 0.49875)    ← just below center
Sample 8:  (0.500, 0.50125)    ← just above center
Sample 9:  (0.500, 0.50375)
Sample 10: (0.500, 0.50625)
Sample 11: (0.500, 0.50875)
Sample 12: (0.500, 0.51125)
Sample 13: (0.500, 0.51375)
Sample 14: (0.500, 0.51625)
Sample 15: (0.500, 0.51875)
```

Each sample is bilinearly filtered at mip level 0. The 16 results are averaged with equal weight.

**Verification of coverage:**
```
Span from sample 0 to sample 15:
UV_V range: 0.51875 - 0.48125 = 0.0375 in normalized UV
In texels: 0.0375 × 256 = 9.6 texels

With bilinear interpolation, each sample covers ~1 texel width,
so effective coverage: 9.6 + ~0.5 texels on each side ≈ 10.6 texels.
Target was 10.24 texels. Close match. ✓
```

**Step 5: U axis coverage check.**

Each bilinear sample at level 0 is taken at a fixed U = 0.500. The bilinear kernel at level 0 spans 1 texel in U (interpolating between adjacent texels at the U = 0.500 boundary). The footprint is 0.512 texels wide in U. Bilinear at level 0 covers ~1 texel in U. The footprint is 0.512 texels, so we are covering 1 texel / 0.512 texels ≈ 2× too much in U — a mild over-blur, not the 15.6× from the isotropic case.

**Conclusion of numeric example:**

| Axis | Isotropic blur factor | AF blur factor |
|------|----------------------|----------------|
| U (across floor) | 15.6× over-blurred | ~2× over-blurred (minor) |
| V (along floor)  | ~1.3× (approximately correct) | ~1.0× (correct) |

The floor texture, with AF enabled, retains sharpness in the U direction at a level 7–8× better than trilinear alone.

---

## Pitfalls

**Pitfall 1: Confusing AF with higher-resolution textures.**

Exact mistake: a developer enables 16× AF and expects blurry, low-resolution textures to become sharp. The floor looks blurry even with AF enabled.

Exact symptom: the texture still looks muddy at distance, but the blurriness seems "smoother" or "more detailed" than before. Some improvement is visible but the texture still lacks crispness.

Exact fix: AF preserves detail that already exists in the texture; it does not create new detail. If the texture source is a 256×256 image and the floor is 50 meters away subtending many screen pixels, no amount of AF will recover sub-texel detail. The fix is either a higher-resolution source texture, virtual texturing (megatextures), or a tiling strategy that provides adequate texel density at the required distances. Diagnose: check texel density. If you are below 1 texel per screen pixel even at level 0, the texture is simply too low resolution for the distance — AF is not the solution.

**Pitfall 2: Thinking AF eliminates all aliasing at all angles.**

Exact mistake: enabling 16× AF and expecting perfect, sharp floor textures even at nearly parallel (grazing) angles — camera nearly at floor level, looking almost horizontally.

Exact symptom: at extreme grazing angles (elevation < 3°), AF still produces blurring and mild shimmer. The floor near the horizon looks better than trilinear but still not crisp.

Exact fix: accept the limitation. At anisotropy ratios beyond 16:1, 16× AF still partially over-blurs the long axis because it cannot take enough samples to cover the full footprint length. The options are: (1) use a higher resolution texture to reduce the texel span per pixel, (2) apply a slight fog or depth fade that obscures the horizon anyway (most open-world games do this deliberately), (3) use virtual texturing / texture streaming with high-resolution terrain tiles at distance. There is no simple filter fix for ratios beyond hardware limits.

---

## What to Build

**Exercise 1: Side-by-side floor comparison (trilinear vs. AF).**

In your existing textured scene, place a textured floor plane extending from beneath the camera to 50+ meters ahead. Position the camera at eye height (Y ≈ 1.7) looking toward the horizon. Run the scene with `GL_TEXTURE_MAX_ANISOTROPY_EXT` set to 1.0 (trilinear only). Take a mental note or screenshot of the floor appearance at 10m, 20m, 30m. Then set `GL_TEXTURE_MAX_ANISOTROPY_EXT` to the hardware maximum (you will implement this API call in L8O — for now, note where the change would go). Compare. Write 2–3 sentences: which distance shows the most improvement? Which direction (across or along the floor) improves most?

**Exercise 2: Compute your scene's anisotropy ratio.**

From your L8L shader (which visualized dFdx/dFdy), add a uniform to display the anisotropy ratio N = max(||dpdx||, ||dpdy||) / min(||dpdx||, ||dpdy||) as a heat map on the floor: map N=1 to blue, N=8 to green, N=16+ to red. This shows you visually exactly where on the floor AF matters most. The near floor should be blue (low N, isotropic). The far floor should be red (high N, strongly anisotropic). Identify the screen-space boundary where N crosses 4 (where 4× AF first matters) and where it crosses 16 (where 16× AF is needed but hardware is at its limit).
