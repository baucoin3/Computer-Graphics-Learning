# L8J — Why Aliasing Happens at Distance
**Theme A1: UV Mapping + Texture Fundamentals | Stage 4 of 5**

---

## The Problem Without This Concept

L8G introduced aliasing as a sampling rate problem. This lesson goes deeper: why does distance cause the sampling rate to drop? What is the mathematical relationship between camera distance and texel footprint size? And why does this violate Nyquist specifically for high-frequency texture content but not for low-frequency content?

Understanding this precisely tells you exactly when mipmaps help, when they don't, and what level of filtering is required at a given distance.

---

## Part A — Theory

### The Texel Footprint

When a fragment is rasterized, it corresponds to one screen pixel. That pixel "covers" a certain region of the texture — the area of texture that, when rendered correctly, contributes to this pixel's color. This region is called the **texel footprint** or **texture footprint**.

The footprint is not a perfect square in texture space. For a surface viewed straight-on, it approximates a square. For a surface at an angle, it becomes a parallelogram or even an elongated ellipse (the anisotropic case, L8M). For now, treat it as approximately square.

The footprint side length in texels = (texture width in texels) × (UV change per screen pixel).

For a 512×512 texture on a surface that occupies 512 screen pixels horizontally: each screen pixel covers exactly 1 texel. Footprint = 1×1 texel.

For the same texture on a surface that occupies 64 screen pixels: each pixel covers 512/64 = 8 texels per axis = 64 texels per pixel. Footprint = 8×8 texels.

### How Distance Affects Footprint Size

Perspective projection compresses distant geometry. A 1-meter surface patch at distance d meters subtends an angle of approximately 1/d radians (for d >> 1). At a screen resolution of W pixels across a horizontal FOV of θ radians, each pixel subtends θ/W radians.

The number of screen pixels covering that 1-meter patch:

```
pixels_per_meter = W / (d × tan(θ/2) × 2)
                ≈ W / (d × θ)    for small θ (approximation)
```

As d increases, pixels_per_meter decreases proportionally to 1/d. If a surface has a texture at T texels per meter, the texels per screen pixel is:

```
texels_per_pixel = T / pixels_per_meter = T × d × θ / W
```

This grows linearly with distance d. At distance d:
- d = 1m: 1 texel/pixel (roughly, at typical values)
- d = 4m: 4 texels/pixel
- d = 16m: 16 texels/pixel
- d = 64m: 64 texels/pixel

The footprint grows proportionally to distance.

### The Nyquist Violation at Distance

The texture has a maximum frequency content. For a checkerboard with 2-texel-wide stripes, the maximum frequency is 0.5 cycles per texel (one full cycle = one black stripe + one white stripe = 2 texels).

The Nyquist condition in screen space: we need at least 2 samples per cycle of the highest texture frequency as it appears in screen space.

One cycle in texture space = 2 texels. At footprint F texels/pixel, those 2 texels occupy:

```
2 texels / F texels_per_pixel = 2/F screen pixels per cycle
```

For the sample rate to satisfy Nyquist: the rate (1 sample/pixel) must be ≥ 2× the frequency in screen space. The frequency in screen space (cycles per screen pixel) = F/2. Nyquist requires sample rate ≥ 2 × F/2 = F.

But the sample rate is 1 sample/pixel. The requirement is 1 sample/pixel ≥ F. This is violated when F > 1, i.e., when more than 1 texel fits in each pixel.

**Conclusion**: aliasing from the maximum texture frequency occurs as soon as the footprint exceeds 1 texel/pixel. For lower-frequency content, aliasing begins at a larger footprint threshold:
- A stripe pattern with 8-texel stripes (frequency = 1/16 cycles/texel): aliasing begins when footprint > 8 texels/pixel

This is why aliasing is progressive: the highest frequencies alias first (at small footprints), and lower frequencies alias only at much larger footprints. A texture on a distant object first loses its finest detail, then gradually looks more blurred as distance increases.

### Area Averaging as the Correct Solution

The correct color for a pixel with footprint F×F texels is the average of all F² texels in the footprint:

```
correct_color = (1/F²) × Σ texel(i,j) for all (i,j) in footprint
```

This removes all frequency content above 1 cycle per 2 texels of footprint — exactly the Nyquist limit for this resolution. No aliasing.

The problem: F changes continuously with depth, view angle, and surface orientation. Computing a different-size average for every pixel every frame is too expensive.

### Mipmaps as Pre-Computed Averages

The mipmap at level k is the texture pre-averaged over 2^k × 2^k texel blocks. This corresponds to the correct area-averaged color for a footprint of 2^k texels.

For a footprint of F texels, the correct mip level is:

```
k = log2(F)
```

At this level, the mip texel size matches the footprint size — one mip texel corresponds to approximately one screen pixel. The aliasing-free condition is met.

**Example calculation**: footprint F = 8 texels/pixel.
```
k = log2(8) = 3
```
Use mip level 3 (64×64 for a 512×512 base). At this level, each texel represents an 8×8 block of original texels — averaged. Bilinear filtering within this level gives the correct anti-aliased result.

### What Pre-filtering Means

"Pre-filtering" means applying the low-pass filter BEFORE the signal is sampled at the screen resolution. Mipmaps are pre-filtered textures — they have already been blurred by the correct amount for each scale factor. The GPU selects the appropriate pre-blurred version at render time.

This is fundamentally different from post-processing: blurring the final rendered image does not recover aliased information — it just blurs the already-aliased result. Pre-filtering must happen before sampling.

### Industry Application

**Film VFX**: uses Elliptical Weighted Average (EWA) filtering — a mathematically optimal filter that handles both isotropic and anisotropic footprints. Arnold renderer, Renderman, and Blender Cycles all use EWA or similar. EWA is too expensive for real-time.

**Real-time games**: mipmaps + trilinear + anisotropic filtering (OpenGL, DX12, Vulkan). This is a good approximation of EWA for isotropic footprints, with the anisotropic case handled by AF.

**OptiX ray tracing**: `rtTextureSampler` with `RT_FILTER_LINEAR` + `RT_FILTER_TRILINEAR` for device-side textures in NVIDIA OptiX. Same principle applied to ray-traced texture access.

---

## Part B — Numeric Worked Example

### Aliasing Threshold at Distance

Setup:
- Texture: 512×512, containing a stripe pattern with 4-texel-wide stripes (horizontal frequency = 1/8 cycles per texel)
- Surface: 2m × 2m square, perpendicular to camera
- Screen: 1920×1080, horizontal FOV 60°, rendering at full resolution
- Texture maps uniformly: U goes 0 to 1 across the 2m width

**Pixels per meter at distance d:**

With FOV 60°, at distance d, the view width at d is:
```
view_width = 2 × d × tan(30°) = 2 × d × 0.577 = 1.155 × d  meters
```

Screen pixels per meter:
```
px/m = 1920 / (1.155 × d) = 1661.9 / d
```

**Texels per pixel at distance d:**

Texture has 512 texels over 2m of surface → 256 texels/m.
```
texels_per_pixel = 256 / (px/m) = 256 × d / 1661.9 = 0.1541 × d
```

**Aliasing threshold for 4-texel stripes:**

Stripes have period 8 texels → frequency 0.125 cycles/texel. Nyquist violation when texels_per_pixel > 8 (footprint > 8 texels means the stripe period is smaller than the footprint → undersampled).

```
0.1541 × d > 8
d > 8 / 0.1541
d > 51.9 meters
```

At distances beyond ~52 meters, the 4-texel stripes begin to alias. Below 52m, trilinear filtering is not needed for this frequency — the sampling rate is sufficient.

**Aliasing threshold for 2-texel stripes (maximum texture frequency):**

```
0.1541 × d > 2
d > 2 / 0.1541
d > 13.0 meters
```

The finest details (2-texel stripes) start aliasing at only 13 meters. A player standing 13m from a wall would see the finest texture details shimmer without mipmaps.

**What mip level is needed at d = 52m?**

Footprint = 0.1541 × 52 = 8.01 texels. Level needed: log2(8.01) ≈ 3.

At 52m, the GPU should use mip level 3 (the 64×64 version of the 512×512 texture). This version has averaged out all detail below the 8-texel scale. Correct.

Show the arithmetic:
```
footprint = texels_per_pixel = 256 texels/m × (1/px_per_m) = 256 × d / 1661.9

At d = 52:
footprint = 256 × 52 / 1661.9 = 13312 / 1661.9 = 8.01 texels/pixel

log2(8.01) = log2(8) + log2(1.00125) ≈ 3 + 0.0018 ≈ 3.002

Mip level ≈ 3.
```

---

## Pitfalls

**Pitfall 1: Thinking aliasing is a rendering pipeline bug**

Mistake: when a texture shimmers at distance, debugging shader code, checking UV coordinates, or adjusting light values.

Symptom: no change from shader debugging. The shimmer is not in the shader — it is in the texture sampling.

Fix: aliasing is always a sampling rate problem. Check: are mipmaps enabled? Is MIN_FILTER set to GL_LINEAR_MIPMAP_LINEAR? Is anisotropic filtering set for oblique surfaces? These three things fix 99% of texture aliasing.

**Pitfall 2: Thinking MSAA fixes texture aliasing**

Mistake: enabling 4× MSAA expecting texture shimmering to stop.

Symptom: geometry edges look smoother (correct — MSAA helps there) but the floor texture still shimmers at distance (MSAA does not take multiple texture samples).

Explanation: MSAA takes multiple geometry samples per pixel to resolve sub-pixel geometry. But each fragment still takes one texture sample from the same UV. Texture aliasing is unaffected by MSAA sample count.

Fix: mipmaps + trilinear + anisotropic filtering. These specifically address texture aliasing.

---

## What to Build

**Exercise 1: Visual distance aliasing**

Create a programmatic stripe texture: 64×64 texels, alternating black/white every 2 texels (horizontal stripes). Upload WITHOUT mipmaps, GL_NEAREST filter. Render on a flat quad that fills the screen at z=0 and recedes to z=-50. Observe: at what depth does the shimmering begin? Is the threshold consistent with the formula from Part B (scaled for your screen/FOV settings)?

**Exercise 2: Nyquist frequency calculation**

For a 256×256 texture on a 4m×4m surface:
1. Compute texels per meter: 256/4 = 64 texels/m
2. At screen resolution 1920×1080, FOV 90°, compute pixels/m at distances d = 2m, 10m, 50m
3. Compute texels/pixel at each distance
4. At which distance does texels/pixel exceed 1? (first aliasing of highest frequency)
5. At which distance does texels/pixel exceed 8? (aliasing of 8-texel pattern)

Show all arithmetic.

**Exercise 3: Explain without "aliasing"**

Write 3–4 sentences explaining to a non-technical person why a texture on a distant wall appears to "shimmer and crawl" when the camera moves. Do not use the word "aliasing", "Nyquist", or "frequency". Use only visual and intuitive language.

This exercise forces you to develop genuine understanding rather than vocabulary.
