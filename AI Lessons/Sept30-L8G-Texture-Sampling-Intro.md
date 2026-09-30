# L8G — Texture Sampling: The Aliasing Problem
**Theme A1: UV Mapping + Texture Fundamentals | Stage 3 of 5**

---

## The Problem Without This Concept

You have a UV coordinate — a continuous float pair. You have a texture — a grid of discrete pixels. You need a color. How do you go from a float to a pixel?

The naive answer is: round to the nearest pixel. That works at full size. But when the textured surface is small on screen — when 1000 texels must contribute to a single screen pixel — rounding picks one texel and ignores the other 999. The result shimmers, crawls, and flickers as the camera moves.

This lesson diagnoses WHY this happens from first principles. The next two lessons give the solutions.

---

## Part A — Theory

### The Core Tension: Continuous vs Discrete

A texture is a **discrete signal** — a grid of values sampled at regular integer positions. When you sample it at a continuous UV coordinate, you are asking the grid to represent a value at a position that may fall between grid points.

This is a **signal reconstruction problem**. Reconstructing a continuous signal from discrete samples is only possible if the signal meets certain conditions. When those conditions are violated, you get aliasing.

### What the Texture Contains: Frequencies

Think of a texture image not as a collection of pixels, but as a **signal** — a function that maps 2D position to color. Like any signal, it has a frequency spectrum:

- **Low frequencies**: large-scale, slowly-varying regions — a gradient, a solid color area, a broad shadow
- **High frequencies**: rapidly-varying detail — thin lines, sharp edges, fine checkerboard patterns, wood grain

A checkerboard with 2-pixel-wide squares alternates black and white every 2 texels. That is the highest frequency a texture can represent: one full cycle per 2 texels (1 cycle = one black + one white stripe). A gradient across the entire texture has one cycle over its full width — extremely low frequency.

Real textures contain a mix of all frequencies.

### The Nyquist-Shannon Sampling Theorem

**Statement**: a continuous signal can be perfectly reconstructed from discrete samples if and only if the sampling frequency is at least twice the highest frequency component in the signal.

In other words: if a signal has features that repeat at most every 2 units, you need at least 1 sample per unit to capture them correctly. More generally: sample rate ≥ 2 × max signal frequency.

The threshold frequency (half the sample rate) is called the **Nyquist frequency**. Signal content at or below Nyquist can be perfectly reconstructed. Signal content above Nyquist **cannot** — it aliases.

### Applying the Theorem to Texture Rendering

When you render a textured surface:

- **The signal**: the texture image, with its frequency content
- **The sample rate**: one sample per screen pixel

The "sample rate" in texture space is determined by how many texels correspond to one screen pixel.

**If the surface is large on screen** (close to camera): one screen pixel covers a small region of texture — perhaps 0.5 × 0.5 texels. The sample rate (pixels per texel) is high. The Nyquist condition is satisfied for all frequencies in the texture. No aliasing. The texture can look sharp.

**If the surface is small on screen** (far from camera): one screen pixel covers a large region of texture — perhaps 32 × 32 texels. The sample rate (pixels per texel) is 1/32. The Nyquist condition requires: sample rate ≥ 2 × max frequency. For a checkerboard changing every 2 texels, max frequency = 0.5 cycles/texel. Nyquist requires sample rate ≥ 1 sample/texel. We have 1/32. Far below. Aliasing occurs.

### Magnification vs Minification

**Magnification** (zoomed in): one screen pixel covers less than one texel. The display resolution exceeds the texture resolution. We are "stretching" the texture.

Under nearest-neighbor sampling: a group of adjacent screen pixels all round to the same texel → they all get the same color → the texture looks blocky, pixelated, like an old video game at low resolution.

Under bilinear filtering: the fractional texel position is interpolated → smooth gradients between texels → looks blurry rather than blocky.

**Minification** (zoomed out): one screen pixel covers more than one texel. We need to "compress" many texels into one color. This is where aliasing strikes.

Under nearest-neighbor: pick ONE texel from the many that cover the pixel → all others are ignored → the chosen texel changes frame-to-frame as the camera moves → shimmering, crawling texture pattern.

### Why Nearest-Neighbor at Minification Is Catastrophically Bad

A screen pixel that covers a 16×16 texel region contains 256 texels. Nearest-neighbor picks 1 and ignores 255.

If the texture has a checkerboard pattern (alternating black and white), and the single sampled texel happens to be black, the pixel shows black. Next frame, the camera has moved slightly — the same pixel now samples a different texel, which happens to be white. The pixel flips to white. The checkerboard pattern across the surface shimmers — flickering between black and white at every pixel, every frame.

This is **temporal aliasing** — not just a wrong color in a single frame, but an incorrect color that CHANGES frame to frame, creating visible animation in a static surface.

### The Correct Solution: Pre-filtering (Area Averaging)

The mathematically correct answer for a pixel covering a texel region is to **average all the texels in that region**. This is called **area averaging** or **box filtering**.

If a pixel covers a 16×16 = 256-texel region of a checkerboard with exactly half black and half white texels:

```
average = (128 × 0.0 + 128 × 1.0) / 256 = 0.5
```

The pixel correctly shows medium gray — a blurred representation of the high-frequency detail that cannot be represented at this resolution.

This is correct: it removes frequency content above the Nyquist limit for this pixel's resolution. No aliasing.

**The problem**: the texel region covered by each pixel changes continuously with depth and viewing angle. Every pixel at every frame would need a differently-sized average. Computing this dynamically is too expensive for real-time rendering.

**The solution**: pre-compute the average at multiple fixed scales. **Mipmaps** are exactly this: a sequence of pre-blurred texture versions at power-of-two sizes. The GPU picks the mip level whose resolution matches the current pixel's footprint size. Mipmaps are covered in L8J–L8L.

### Bilinear Filtering: A Weak Pre-filter

Bilinear filtering does not fix the aliasing problem for minification. It samples 4 texels instead of 1, but 4 texels out of 256 is still a wildly inadequate sample of the footprint. Bilinear only helps at magnification (and mildly at mild minification).

Trilinear filtering (bilinear + mip level interpolation) is the standard solution. Anisotropic filtering extends this to oblique surfaces.

### The Signal Processing View

Aliasing is high-frequency signal content "folding back" into the visible frequency range because the sampling rate is insufficient. When you sample a 10 Hz signal at 6 Hz (below Nyquist of 20 Hz), the energy at 10 Hz appears as if it were at 10 - 6 = 4 Hz — a lower frequency. The high-frequency content masquerades as low-frequency noise. In textures: the fine checkerboard appears as coarse random blotches at distance.

The standard signal-processing solution is to **low-pass filter** the signal before sampling — remove all frequency content above the Nyquist limit. For texture rendering, the Nyquist limit at a given distance is one cycle per 2 screen pixels. The pre-filter must remove all texture frequencies above that. Mipmaps apply this pre-filter at a discrete set of scales.

---

## Part B — Numeric Worked Example

### Checkerboard Aliasing

Texture: 8×8, checkerboard. Texel (i,j) is black if (i+j) is even, white if (i+j) is odd.

```
Row 7: W B W B W B W B
Row 6: B W B W B W B W
Row 5: W B W B W B W B
Row 4: B W B W B W B W
Row 3: W B W B W B W B
Row 2: B W B W B W B W
Row 1: W B W B W B W B
Row 0: B W B W B W B W    (B=black=0.0, W=white=1.0)
```

Scenario: surface appears as 2×2 pixels on screen. Each pixel covers a 4×4 texel region.

**Pixel A**: covers texels at columns 0–3, rows 0–3.

Nearest-neighbor: sample center falls at texel (1,1) approximately. Texel (1,1): i+j = 2, even → BLACK.

**Pixel B**: covers columns 4–7, rows 0–3.

Nearest-neighbor sample center at texel (5,1): i+j = 6, even → BLACK.

**Pixel C**: covers columns 0–3, rows 4–7.

Nearest-neighbor sample center at texel (1,5): i+j = 6, even → BLACK.

**Pixel D**: covers columns 4–7, rows 4–7.

Nearest-neighbor sample center at texel (5,5): i+j = 10, even → BLACK.

**Result**: all 4 pixels are BLACK. The checkerboard appears as solid black. Every white texel is invisible.

**Area averaging for the same 4 pixels:**

Each 4×4 region contains: look at columns 0–3, rows 0–3:
```
(0,0)=B (1,0)=W (2,0)=B (3,0)=W
(0,1)=W (1,1)=B (2,1)=W (3,1)=B
(0,2)=B (1,2)=W (2,2)=B (3,2)=W
(0,3)=W (1,3)=B (2,3)=W (3,3)=B
```

Black count: (0,0),(2,0),(1,1),(3,1),(0,2),(2,2),(1,3),(3,3) = 8 black texels.
White count: 16 - 8 = 8 white texels.

Average = (8 × 0.0 + 8 × 1.0) / 16 = 8.0 / 16.0 = 0.5.

All 4 pixels → 0.5 gray. A correct, aliasing-free representation of the checkerboard at this resolution.

**Next frame**: camera moved 0.5 texels to the right. Nearest-neighbor sample centers shift slightly → some now land on white texels. Those pixels flip to WHITE. The surface flickers between frames — temporal aliasing. Area averaging: still 8 black, 8 white, still 0.5 → no flicker. Stable across frames.

---

## Pitfalls

**Pitfall 1: Thinking texture aliasing is a shader bug**

Mistake: a texture shimmers at distance. You try to fix it by adjusting shader math — multiplying colors, tweaking the lighting, adjusting UV scale.

Symptom: nothing helps. The shimmer persists because it is a sampling rate problem, not a computation error.

Fix: enable mipmaps (`glGenerateMipmap`) and trilinear filtering (`GL_LINEAR_MIPMAP_LINEAR`). If the shimmer is on an oblique surface, also enable anisotropic filtering. No shader changes needed.

**Pitfall 2: Confusing MSAA with texture anti-aliasing**

Mistake: enabling MSAA (4× or 8×) expecting texture aliasing to disappear.

Symptom: MSAA eliminates jagged geometry edges but the texture still shimmers on a distant receding floor. MSAA samples the fragment multiple times per pixel at the subpixel level, but in most implementations, each fragment still takes only ONE texture sample (not one per MSAA subsample). Texture aliasing is unaffected.

Fix: mipmaps + trilinear + anisotropic filtering for texture aliasing. MSAA for geometry edge aliasing. They solve different problems.

**Pitfall 3: Assuming bilinear filtering fixes minification aliasing**

Mistake: setting GL_LINEAR and expecting the shimmer to disappear at distance.

Symptom: slight improvement at mild distances, but still shimmers badly at medium and far distances. Bilinear samples 4 texels — 4 out of potentially hundreds. It is not enough of an area average to prevent aliasing.

Fix: bilinear alone is not sufficient for minification. Always pair it with mipmaps.

---

## What to Build

**Exercise 1: Visual aliasing demonstration**

Create a checkerboard texture programmatically: 64×64 texels, alternating black and white every 4 texels. Upload to OpenGL WITHOUT mipmaps. Set GL_NEAREST for both MIN and MAG filter. Render it on a flat quad that recedes into the distance at a 45° angle (one end close, one end far). Observe the aliasing: near region looks correct, far region shimmers.

Capture a screenshot. Note exactly where the aliasing begins as a function of distance.

**Exercise 2: Frequency analysis by eye**

Look at three textures (brick, wood grain, gradient sky). Rank them by "highest frequency content" — which one has the finest, most rapidly-changing detail? Which has the lowest? Explain your ranking in terms of: period of the repeating pattern in texels, and expected severity of aliasing at 4× minification.

**Exercise 3: Compute the aliasing distance**

A 512×512 texture is mapped to a 1m×1m quad. The quad is perpendicular to the camera. At what viewing distance does 1 screen pixel begin to cover more than 1 texel (where aliasing can begin)? Assume screen resolution is 1920×1080 and horizontal FOV is 90°.

Hint: compute screen pixels per meter at a given distance d, then find d such that pixels/m = 512/1 = 512 texels/m, then aliasing begins when pixels/m < 512.

Show all arithmetic.
