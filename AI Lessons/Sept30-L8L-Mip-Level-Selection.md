# L8L — Mip Level Selection: LOD, UV Derivatives, and LOD Bias
**Theme A1: UV Mapping + Texture Fundamentals | Stage 4 of 5**

---

## The Problem Without This Concept

The mipmap chain exists. The GPU must now choose which level to use for each fragment. Too low a level (too sharp): aliasing. Too high a level (too blurry): unnecessary blurring. The GPU must match the mip level to the texel footprint size precisely. This lesson derives how the GPU does this, how UV derivatives work, and how LOD bias lets you override the automatic selection.

---

## Part A — Theory

### The Goal of LOD Selection

The correct mip level k for a given fragment satisfies: one texel at level k corresponds to approximately one screen pixel. At this scale, bilinear sampling within the level is sufficient — the texel size matches the pixel footprint.

This is equivalent to: at level k, the footprint size in the level's texel space is approximately 1. Since level k has 2^k times fewer texels per axis than level 0, and the footprint in level-0 texels is F, the footprint in level-k texels is F/2^k. Setting this equal to 1:

```
F / 2^k = 1
2^k = F
k = log2(F)
```

This is the LOD formula: LOD = log2(footprint in level-0 texels).

### UV Derivatives: How the GPU Measures Footprint

The GPU processes fragments in **2×2 quads** — groups of 4 adjacent screen pixels. Even if only some pixels in the quad actually cover the triangle (e.g., near a triangle edge), all 4 fragments are computed so that derivatives can be calculated.

The **derivative** of a quantity Q with respect to screen position is approximated by subtraction:

```
dFdx(Q) ≈ Q(x+1, y) - Q(x, y)   // Q change from left pixel to right pixel
dFdy(Q) ≈ Q(x, y+1) - Q(x, y)   // Q change from bottom pixel to top pixel
```

For UV coordinates, this gives:

```
dFdx(uv) = UV at pixel (x+1,y) - UV at pixel (x,y)   // UV change per pixel rightward
dFdy(uv) = UV at pixel (x,y+1) - UV at pixel (x,y)   // UV change per pixel upward
```

These are 2D vectors (since uv is a vec2). Each component tells you how fast the UV is changing in screen space.

In GLSL, these are available as built-in functions in the fragment shader:

```glsl
vec2 duvdx = dFdx(vTexCoord);   // how UV changes as you move 1 pixel right
vec2 duvdy = dFdy(vTexCoord);   // how UV changes as you move 1 pixel up
```

### The LOD Formula

The footprint in texel space is found by scaling the UV derivatives by the texture dimensions:

```
// duvdx and duvdy are in UV space (0 to 1 per axis)
// Scale to texel space (0 to W per axis in U, 0 to H in V):

dx = vec2(duvdx.x * W, duvdx.y * H)   // UV gradient in X screen direction, in texels
dy = vec2(duvdy.x * W, duvdy.y * H)   // UV gradient in Y screen direction, in texels
```

The footprint is approximately the larger of the two gradient magnitudes:

```
px = sqrt(dx.x*dx.x + dx.y*dx.y)   // magnitude of gradient in X
py = sqrt(dy.x*dy.x + dy.y*dy.y)   // magnitude of gradient in Y

p = max(px, py)    // use the larger = more conservative (prevents aliasing at cost of some blur)

LOD = log2(p)
```

The `max` here selects the worst-case axis (most texels per pixel). This ensures the chosen mip level is sufficient for the direction that changes fastest. Using `min` instead would under-blur and cause aliasing along the fast axis.

An alternative formula that accounts for anisotropy:

```
p = sqrt(max(dot(dx,dx), dot(dy,dy)))
```

This is equivalent but more numerically stable.

### What Each Gradient Tells You

`dFdx(uv)` at a fragment tells you: "if I move one screen pixel to the right, the UV changes by this vector."

If `dFdx(uv).x = 0.01` on a 512-wide texture: moving one pixel right changes U by 0.01, which spans `0.01 × 512 = 5.12 texels`. The footprint in the X direction is about 5 texels per pixel.

If `dFdx(uv)` is large: the texture is compressed (many texels per pixel → distant surface → high mip level needed).
If `dFdx(uv)` is small: the texture is stretched (few texels per pixel → close surface → use level 0 or low level).

### Integer vs Float LOD

The GPU computes LOD as a float. For trilinear, it uses both floor(LOD) and ceil(LOD) with fractional blending (as derived in L8I). For bilinear-only (`GL_LINEAR_MIPMAP_NEAREST`), it rounds to the nearest integer level.

Both the computed LOD and the rounded level can be clamped:

```c
glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 0.0f);   // clamp to level >= 0
glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 7.0f);   // clamp to level <= 7
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);   // first level available
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 9);    // last level available
```

Use case: texture streaming. If only mip levels 3–9 are loaded (coarse resolution for distant objects), set `GL_TEXTURE_BASE_LEVEL = 3` to prevent sampling from levels 0–2 (not in memory yet).

### LOD Bias

LOD bias shifts the computed LOD before mip level selection:

```
effective_LOD = computed_LOD + bias
```

**Positive bias** (e.g., +1.0): selects a blurrier mip level than geometry dictates. The texture looks softer. Use case: cinematic soft-focus look, or intentionally blurring distant textures to reduce texture detail overloading the composition.

**Negative bias** (e.g., -1.0): selects a sharper mip level. The texture looks crisper, but aliasing may return. Use case: sharpening effect, or compensating for TAA's temporal blurring in some pipelines.

In OpenGL, per-texture LOD bias:
```c
glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, bias);
```

In GLSL, per-sample LOD bias in the texture call:
```glsl
vec4 color = texture(uTex, uv, bias);   // texture() with 3rd arg = LOD bias
```

Or explicit LOD with textureLod:
```glsl
vec4 color = textureLod(uTex, uv, explicitLOD);   // bypass automatic LOD entirely
```

### The 2×2 Quad Constraint

UV derivatives are computed by differencing adjacent pixels in the 2×2 fragment quad. This means:

1. **Derivatives are only available in fragment shaders.** Not in vertex, geometry, or compute shaders.

2. **Derivatives are undefined inside non-uniform flow control.** If your fragment shader branches (`if/else`) and different fragments in the same 2×2 quad take different branches, the derivative computation is undefined (implementation-dependent, often gives 0 or garbage). This causes the GPU to select LOD=0 (no mip), which leads to aliasing at distance.

```glsl
// WRONG: LOD computation happens inside a branch — potentially divergent
void main() {
    if (someCondition) {
        // dFdx(uv) here may be wrong if someCondition differs within the 2x2 quad
        vec4 c = texture(uTex, uv);  // LOD undefined
    }
}

// CORRECT: compute the UV (and let the driver compute the derivative) before branching
void main() {
    vec4 c = texture(uTex, uv);     // derivative computed correctly here, before any branch
    if (someCondition) {
        // use c
    }
}
```

3. **textureLod() bypasses this limitation.** When you need to sample a texture inside a loop or branch with correct LOD, pre-compute the LOD outside the branch using `textureQueryLod()`:

```glsl
vec2 queriedLOD = textureQueryLod(uTex, uv);   // returns (computed_lod, used_lod)
float lod = queriedLOD.x;
// Now use lod inside the branch safely:
if (someCondition) {
    vec4 c = textureLod(uTex, uv, lod);
}
```

### Anisotropy and LOD

The formula `p = max(px, py)` is **isotropic** — it takes the larger footprint dimension and blurs both dimensions to that level. For a surface where px = 1 and py = 16 (oblique floor), p = 16 → LOD = log2(16) = 4. The GPU uses mip level 4, which blurs the 1-texel axis to only 1/16 of a texel at level 4 — massively over-blurred.

Anisotropic filtering modifies this: use LOD based on the **minor axis** (px = 1 → LOD = 0), then take N = major/minor = 16 samples along the major axis direction. This is the AF algorithm (L8N).

---

## Part B — Numeric Worked Example

### LOD Calculation: Step by Step

Fragment at screen position (100, 200).

Adjacent fragments:
- Right neighbor (101, 200): UV = (0.410, 0.600)
- Top neighbor  (100, 201): UV = (0.400, 0.620)
- Current fragment (100, 200): UV = (0.400, 0.600)

Texture dimensions: 512×512 (W = 512, H = 512).

**Step 1: Compute UV derivatives**

```
dFdx(uv) = UV(101,200) - UV(100,200) = (0.410 - 0.400, 0.600 - 0.600) = (0.010, 0.000)
dFdy(uv) = UV(100,201) - UV(100,200) = (0.400 - 0.400, 0.620 - 0.600) = (0.000, 0.020)
```

**Step 2: Scale to texel space**

```
dx = (0.010 × 512, 0.000 × 512) = (5.12, 0.00)   texels per pixel in X direction
dy = (0.000 × 512, 0.020 × 512) = (0.00, 10.24)  texels per pixel in Y direction
```

**Step 3: Compute footprint magnitudes**

```
px = sqrt(5.12² + 0.00²) = sqrt(26.21 + 0) = sqrt(26.21) = 5.12
py = sqrt(0.00² + 10.24²) = sqrt(0 + 104.86) = sqrt(104.86) = 10.24
```

sqrt(26.21): 5² = 25, 5.1² = 26.01, 5.12² = 26.21. So sqrt(26.21) = 5.12 exactly.
sqrt(104.86): 10² = 100, 10.2² = 104.04, 10.24² = 104.86. So sqrt(104.86) = 10.24 exactly.

**Step 4: Compute LOD**

```
p = max(5.12, 10.24) = 10.24

LOD = log2(10.24)
    = log2(8 × 1.28)
    = log2(8) + log2(1.28)
    = 3 + log2(1.28)
```

log2(1.28): `1.28 = 2^x → x = ln(1.28)/ln(2) = 0.247/0.693 = 0.357`

```
LOD = 3 + 0.357 = 3.357
```

**Step 5: Trilinear mip selection**

```
d_low  = floor(3.357) = 3   → level 3: 64×64
d_high = ceil(3.357)  = 4   → level 4: 32×32
frac   = 3.357 - 3 = 0.357
```

Trilinear result: 35.7% from level 4 + 64.3% from level 3.

**Physical interpretation**: this fragment sees 10.24 original texels per pixel in the Y direction (5.12 in X). The GPU correctly uses mostly level 3 (64×64) with a blend toward level 4 (32×32). Level 3 of the 512×512 texture represents every 8×8 block averaged to one texel. Since the footprint is ~10 texels, this is about right.

**Anisotropy ratio**: py/px = 10.24/5.12 = 2.0. This fragment has a 2:1 anisotropy — the texture stretches twice as fast in the Y direction as X. At 2× AF, this would be handled with 2 samples along the Y axis. At 1× AF (no anisotropic filtering), the X direction is over-blurred by a factor of 2.

---

## Pitfalls

**Pitfall 1: Using textureLod with fixed LOD=0 everywhere**

Mistake: writing `textureLod(uTex, uv, 0.0)` for surface textures to "ensure sharpness."

Symptom: textures are sharp everywhere, including at great distances → severe aliasing on distant surfaces. This disables all mip selection and always reads from the full-resolution level.

Fix: use `texture(uTex, uv)` for surface textures — let the GPU compute the correct LOD automatically. Only use `textureLod` when you have a specific, explicit reason (shadow maps, custom LOD algorithms, or sampling inside a divergent branch with pre-computed LOD).

**Pitfall 2: dFdx/dFdy inside non-uniform flow control**

Mistake:

```glsl
void main() {
    if (vFaceID == 0) {
        vec4 c = texture(uTex, vTexCoord);   // derivatives undefined here
        fragColor = c;
    } else {
        fragColor = vec4(1.0);
    }
}
```

Symptom: on surfaces where the branch is not uniform within a 2×2 quad (near the boundary of condition `vFaceID == 0`), texture LOD is undefined. May produce incorrect mip selection (usually LOD=0 → no aliasing fix, or LOD=max → everything blurry).

Fix: move texture samples outside the branch, or pre-compute LOD with `textureQueryLod` and use `textureLod` inside the branch.

**Pitfall 3: Wrong LOD in compute shaders for texture access**

Mistake: sampling a texture inside a compute shader with `texture(sampler, uv)`.

Symptom: OpenGL generates an error or undefined behavior — `texture()` with automatic LOD is invalid in compute shaders (no fragment quads → no derivative computation).

Fix: always use `textureLod(sampler, uv, 0.0)` (or the desired level) in compute shaders. There is no automatic LOD in compute — you must specify it manually.

---

## What to Build

**Exercise 1: LOD heat map shader**

Write a fragment shader that visualizes the computed LOD as a color:
- LOD ≤ 0: pure blue (0,0,1) — surface is magnified or at 1:1
- LOD = 1: cyan (0,1,1)
- LOD = 2: green (0,1,0)
- LOD = 3: yellow (1,1,0)
- LOD ≥ 4: red (1,0,0) — heavy minification

Compute LOD using dFdx/dFdy directly. Use `mix()` to blend between these colors based on the LOD value. Apply to your receding floor scene. The result is a visual mip-level heat map of the entire scene — an invaluable debugging tool.

**Exercise 2: Explicit LOD calculation**

In a fragment shader for a 512×512 texture:
1. Compute dFdx(vTexCoord) and dFdy(vTexCoord)
2. Scale to texel space (multiply by 512)
3. Compute px, py (magnitudes)
4. Compute p = max(px, py)
5. Compute LOD = log2(p) using GLSL's built-in `log2()`
6. Sample the texture with `textureLod(uTex, vTexCoord, LOD)`

This manually reimplements what the GPU does automatically. Compare the result visually to using `texture()` — they should be nearly identical.

**Exercise 3: Identify the derivative branch bug**

Given this shader:

```glsl
uniform bool uUseSpecialUV;
in vec2 vTexCoord;
in vec2 vSpecialTexCoord;

void main() {
    vec4 color;
    if (uUseSpecialUV) {
        color = texture(uTex, vSpecialTexCoord);  // samples with special UVs
    } else {
        color = texture(uTex, vTexCoord);
    }
    fragColor = color;
}
```

Explain: under what condition is the LOD selection correct in this shader? Under what condition is it wrong? What is the GPU doing differently at the LOD selection step when the branch is non-uniform within a 2×2 quad? How would you fix it?
