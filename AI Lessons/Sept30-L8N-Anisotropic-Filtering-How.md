# L8N — How Anisotropic Filtering Works
**Theme A1: UV Mapping + Texture Fundamentals | Stage 5 of 5**

---

## Introduction: We Know the Problem — Now Understand the Machine

L8M established the diagnosis: an oblique surface produces an elongated elliptical footprint in texture space. Isotropic mip selection picks a mip level based on the long axis, which catastrophically over-blurs the short axis. The correct filter should use a mip level appropriate for the short axis and take multiple samples along the long axis.

This lesson answers the engineering questions: how does the GPU know which direction is the long axis? How many samples does it take, and exactly where are they positioned? What mip level does each sample use? And what did early hardware try before arriving at the modern algorithm?

Understanding this at the algorithm level will let you reason about when AF helps, when it falls short, what the hardware cost actually is, and how to expose this capability correctly through both OpenGL and Vulkan APIs.

---

## Part A: Theory

### Section 1 — Why We Need to Understand the Algorithm (Not Just the Result)

You could enable AF by calling two lines of OpenGL and moving on. For a tutorial project, that would be fine. For a graphics engineering role, it is not. Hiring engineers ask: "Why does 16× AF cost less than you'd expect from 16 texture reads?" "Why does AF not help shadow maps?" "How does the hardware know which direction is the major axis?" These questions require understanding the algorithm, not just the API knob.

More importantly, understanding the algorithm teaches you how derivatives work — and texture space derivatives are used everywhere: in parallax occlusion mapping, in screen-space ambient occlusion, in temporal anti-aliasing, in the mip selection of environment maps. Every technique that asks "how fast is this UV changing across the screen?" uses the same machinery.

---

### Section 2 — Historical Context: RIP Mapping

Before explaining what modern hardware does, it helps to understand what was tried first and why it was abandoned.

The original observation was: mipmaps fail for anisotropic footprints because each mip level reduces both U and V by the same factor. What if we had a prefiltered version that reduces only U by 2? Only U by 4? Only V by 2? Only V by 4? A full table of every combination?

This is called **RIP mapping** (Rectangular In Pyramid mapping). "Rectangular" because the filter kernels are rectangles, not squares.

For a 256×256 texture, the RIP map contains every combination of U and V downscaling:

```
Level (1,1):   256 × 256   (no reduction)
Level (2,1):   128 × 256   (halved in U only)
Level (4,1):    64 × 256   (quartered in U only)
Level (8,1):    32 × 256   (eighth in U only)
...
Level (1,2):   256 × 128   (halved in V only)
Level (1,4):   256 ×  64   (quartered in V only)
Level (2,2):   128 × 128   (halved in both — standard mip level 1)
Level (2,4):   128 ×  64   (halved in U, quartered in V)
...
```

The full table for an N×M texture has log2(N) × log2(M) entries. For 256×256: 8 × 8 = 64 levels. Compare to 9 levels for a standard mip chain.

**RIP mapping cost:** For a 256×256 RGBA texture (256 KB at level 0), the mip chain adds ~1/3 overhead (85 KB). The RIP map adds 64 levels — approximately log2(W) × log2(H) / ((W × H) / (W × H - 1)) ≈ 64 × 256 KB / 1 = dramatically more. In practice, RIP maps increase memory by a factor of roughly log2(max(W, H))² / 1.33 compared to a mip chain — for a 4K texture, this is catastrophic.

**RIP mapping quality:** With the right level selected for a given anisotropy ratio and direction, RIP mapping produces excellent anisotropic filtering because each prefiltered level is exactly a rectangular box filter of the appropriate dimensions. The filter is exact.

**Why modern hardware abandoned RIP mapping:** The memory cost is prohibitive. A game with 500 textures averaging 1 MB each already uses 500 MB of VRAM. If RIP maps multiply this by 5–10×, VRAM runs out. Texture bandwidth — which is already often the bottleneck — explodes. No modern GPU uses RIP mapping for general texture filtering.

The question became: can we achieve most of the quality benefit of RIP mapping while using the standard mip chain (which costs only 1/3 extra memory)?

---

### Section 3 — Modern Hardware AF: The EWA Approximation

Modern hardware anisotropic filtering is an approximation to a filter called **EWA: Elliptical Weighted Average**. EWA is the mathematically ideal anisotropic filter. Hardware AF approximates it with a simpler algorithm that is accurate enough for real-time use.

The EWA ideal: given the footprint ellipse defined by dFdx(uv) and dFdy(uv), trace every texel within the ellipse in texture space, weight each texel by a Gaussian function of its distance from the ellipse center, and average. This is exact but expensive — thousands of texel reads for a large ellipse.

The hardware approximation:

**Step a: Compute the footprint vectors in texture space.**

The GPU has already computed, per fragment:
```
dpdx = dFdx(uv) × vec2(W, H)    (in texels)
dpdy = dFdy(uv) × vec2(W, H)    (in texels)
```

These are two 2D vectors. Together they define a parallelogram (the pixel footprint approximated as a parallelogram rather than an ellipse).

**Step b: Identify the major and minor axes.**

```
||dpdx|| = length of dpdx vector
||dpdy|| = length of dpdy vector

If ||dpdx|| >= ||dpdy||:
    major axis = dpdx (the longer gradient)
    minor axis = dpdy (the shorter gradient)
Else:
    major axis = dpdy
    minor axis = dpdx
```

The major axis direction is the direction in texture space along which the footprint is most elongated. This is the direction along which AF must probe.

**Step c: Select the mip level based on the MINOR axis.**

```
LOD = log2(||minor axis||)
LOD = clamp(LOD, 0, max_mip_level)
```

Using the minor axis for LOD selection is the key insight. The minor axis represents the footprint's extent in the direction perpendicular to elongation. Selecting a mip level appropriate for the minor axis means the texture is downscaled just enough to avoid aliasing in the non-elongated direction, while preserving as much resolution as possible — which the major-axis sampling will then use.

**Step d: Compute the anisotropy ratio and clamp to hardware limit.**

```
N_raw = ||major axis|| / ||minor axis||
N_clamped = min(N_raw, GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT)
N = ceil(N_clamped)    (round up to nearest integer for sample count)
```

**Step e: Take N bilinear samples along the major axis.**

The center sample is at the fragment's UV coordinate. The remaining samples are distributed evenly along the major axis direction, centered on the fragment UV.

```
major_direction = major_axis_vector / ||major axis||     (unit vector in major axis direction)
step = major_axis_vector / N                              (step size in UV space between samples)

samples:
for i in range(N):
    offset = (i - (N-1)/2.0) × step
    sample_UV = fragment_UV + offset
    color[i] = bilinearSample(texture, sample_UV, LOD)

result = average(color[0], color[1], ..., color[N-1])
```

Each `bilinearSample` reads 4 texels at the specified LOD level and linearly interpolates between them. Trilinear AF is also possible: each sample blends between adjacent mip levels (LOD and LOD+1), though some hardware implementations use only bilinear at the selected LOD level for AF samples.

**Step f: Return the averaged color.**

The N sample colors are averaged with equal weight. This is not a Gaussian-weighted average (which EWA would use) — it is a box filter along the major axis. A box filter has weaker anti-aliasing properties than Gaussian (it can exhibit ringing), but the difference is subtle in practice and the computational saving is significant.

---

### Section 4 — Why the Algorithm Works: Intuitive Explanation

The fundamental correctness argument:

At the selected LOD (based on minor axis), one sample in the downscaled mip level covers approximately `2^LOD` texels in U and `2^LOD` texels in V. The minor axis footprint is approximately `2^LOD` texels — so one sample correctly covers the footprint extent in the minor direction.

The major axis footprint is approximately `N × 2^LOD` texels. One sample covers `2^LOD` texels in that direction — insufficient. N samples, each spaced `2^LOD` texels apart, collectively cover `N × 2^LOD` texels — exactly the major axis footprint. Each sample handles its own `2^LOD`-texel region along the major axis; averaging the N samples correctly integrates the texture over the full footprint.

The algorithm correctly handles the footprint in both axes simultaneously, using only the standard mip chain (no extra memory) and N bilinear samples (bounded computation).

---

### Section 5 — Sample Count and Hardware Cost

At 16× AF: up to 16 bilinear samples are taken per fragment. Each bilinear sample reads 4 texels (the 2×2 neighborhood for bilinear interpolation). Total texel reads: 16 × 4 = 64 texels per fragment.

Compare to:
- Nearest filtering: 1 texel per fragment
- Bilinear filtering: 4 texels per fragment
- Trilinear filtering: 8 texels per fragment (4 from each of two mip levels, blended)
- 16× AF: 64 texels per fragment (worst case)

This sounds expensive. In practice, the cost is much lower than raw texel counts suggest, for two reasons:

**Reason 1: Spatial coherence.** The 16 AF sample positions are evenly distributed along the major axis. Adjacent samples are `||major axis|| / 16` apart in texture space. For a typical floor footprint with major axis of 10 texels and texture size 256×256, the step is 10/16 ≈ 0.6 texels. Adjacent samples are 0.6 texels apart — they are reading nearly adjacent memory locations in the texture cache. Cache hits are very high. The effective bandwidth cost of 16 AF samples is much closer to 2–3 bilinear samples than to 16 independent accesses.

**Reason 2: Most fragments don't hit N=16.** AF is adaptive. When the anisotropy ratio is 1.5 (nearly square footprint), N = 2. When the ratio is 3, N = 3. Only fragments with extreme grazing angles reach N = 16. The average across a typical scene is often N = 3–5 effective samples. The 16× limit is a maximum, not an average.

**Real-world cost benchmarks (rough, GPU-dependent):**
- Trilinear → 4× AF: +5 to +10% frame time in texture-heavy scenes on modern GPUs (Nvidia Ampere, AMD RDNA2)
- 4× AF → 16× AF: +3 to +8% additional, depending on scene (floors, terrain, runways most affected)
- On NVIDIA Turing and later: dedicated texture unit hardware for AF makes 16× AF nearly free vs. 8× AF in most scenes

The takeaway: AF is not free, but it is cheap relative to its visual quality gain. A 5% frame time cost for dramatically sharper floors is an easy trade in any real-time application.

---

### Section 6 — Automatic Degradation to Trilinear

AF gracefully degrades for isotropic footprints. If ||dpdx|| ≈ ||dpdy||, then N_raw = major/minor ≈ 1. The hardware rounds to N = 1 — one bilinear sample at the LOD selected from the minor axis. This is exactly trilinear filtering (if blending between mip levels is also done).

Enabling AF on a surface that is rarely viewed at oblique angles incurs no quality penalty (AF gives you the same result as trilinear) and minimal performance overhead (N = 1, no extra samples). This means you can safely enable maximum AF on all textures in a scene and only pay the cost where it matters.

---

### Section 7 — Legacy OpenGL: AF as an Extension

Anisotropic filtering was not part of OpenGL's original specification. It was added as an extension: `GL_EXT_texture_filter_anisotropic`. Understanding this distinction matters because it affects how you query and set it.

In legacy OpenGL (pre-3.3, fixed-function pipeline): there was no way to do AF at all without this extension. The fixed-function texture pipeline had GL_LINEAR, GL_NEAREST, and the mip variants — and that was it. The extension added one new texture parameter: `GL_TEXTURE_MAX_ANISOTROPY_EXT`.

In modern OpenGL (3.3 core profile and later): the extension still exists as `GL_EXT_texture_filter_anisotropic` because AF was never promoted to core. However, this extension is universally supported on all desktop hardware since approximately 2001. In practice, you can assume it is available on any OpenGL 3.3+ context. OpenGL 4.6 added `GL_ARB_texture_filter_anisotropic` (without the EXT suffix), which is equivalent and is part of the core specification starting in 4.6.

To check for extension availability:
```c
// GLFW approach
if (!glfwExtensionSupported("GL_EXT_texture_filter_anisotropic")) {
    // Fall back to trilinear — AF not available
    // On modern desktop hardware, this branch is essentially unreachable
}

// GLAD approach (glad2)
// GL_EXT_texture_filter_anisotropic is exposed as a glad extension flag
// Check GLAD_GL_EXT_texture_filter_anisotropic
```

**Legacy vs. Modern conceptual comparison:**

Legacy fixed-function: texture filtering was configured via the texture matrix and fixed parameters. You could not write a shader to influence how the texture was sampled. The GPU applied its fixed sampler logic and you had no visibility into the derivative computation or LOD selection.

Modern programmable pipeline: you can access `dFdx(vTexCoord)` and `dFdy(vTexCoord)` directly in the fragment shader. You can compute the anisotropy ratio yourself and visualize it (as in the exercises). The GPU's AF hardware runs in parallel with your shader logic, using the same derivatives that the shader can access. The shader does not control AF directly — the sampler state controls it — but you can observe and reason about the same quantities the AF hardware uses.

---

### Section 8 — Vulkan: AF as an Explicit Feature

Vulkan treats anisotropy as a physical device feature that must be explicitly enabled. This design is intentional: Vulkan makes no assumptions about hardware capabilities. Every non-universal feature must be queried, reported, and opted into.

**Query support:**
```c
VkPhysicalDeviceFeatures supportedFeatures;
vkGetPhysicalDeviceFeatures(physicalDevice, &supportedFeatures);
if (!supportedFeatures.samplerAnisotropy) {
    // This GPU does not support AF — fall back to linear
    // Essentially never happens on modern desktop GPUs
}
```

**Enable the feature at device creation:**
```c
VkPhysicalDeviceFeatures enabledFeatures{};
enabledFeatures.samplerAnisotropy = VK_TRUE;

VkDeviceCreateInfo deviceCreateInfo{};
deviceCreateInfo.pEnabledFeatures = &enabledFeatures;
// ... rest of device creation
```

**Configure the sampler:**
```c
VkSamplerCreateInfo samplerInfo{};
samplerInfo.sType         = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
samplerInfo.magFilter     = VK_FILTER_LINEAR;
samplerInfo.minFilter     = VK_FILTER_LINEAR;
samplerInfo.mipmapMode    = VK_SAMPLER_MIPMAP_MODE_LINEAR;   // trilinear between mips
samplerInfo.anisotropyEnable = VK_TRUE;
samplerInfo.maxAnisotropy    = physicalDeviceLimits.maxSamplerAnisotropy;  // use max
```

The key difference from OpenGL: in Vulkan, ALL sampling parameters live in the `VkSampler` object — separate from the image and image view. You create samplers once and reuse them. This matches the OpenGL sampler object pattern (`glGenSamplers`), which is the modern OpenGL equivalent. The old OpenGL approach of setting filter state directly on a texture (`glTexParameteri`) is the legacy path.

**DirectX 11 / HLSL:**
```c
D3D11_SAMPLER_DESC samplerDesc{};
samplerDesc.Filter         = D3D11_FILTER_ANISOTROPIC;
samplerDesc.MaxAnisotropy  = 16;
samplerDesc.AddressU       = D3D11_TEXTURE_ADDRESS_WRAP;
samplerDesc.AddressV       = D3D11_TEXTURE_ADDRESS_WRAP;
samplerDesc.AddressW       = D3D11_TEXTURE_ADDRESS_WRAP;
ID3D11SamplerState* sampler;
device->CreateSamplerState(&samplerDesc, &sampler);
context->PSSetSamplers(0, 1, &sampler);
```

---

### Section 9 — Unreal Engine 5: AF in the Asset Pipeline

Unreal Engine 5 exposes AF through its texture asset system and global rendering settings.

**Global setting (affects all textures):**
Project Settings → Rendering → Textures → Anisotropic Filtering: None / 2× / 4× / 8× / 16×. Defaults to 4×.

**Per-texture override:**
In the Texture Editor for a given texture asset: Details panel → Level of Detail → Mip Gen Settings, and the LOD Group dropdown. The LOD Group maps to a preset that includes the anisotropy level. Common LOD Groups: `World` (4×), `WorldNormalMap` (4×), `Terrain` (16×), `UI` (no mipmaps, no AF).

**In material blueprints:** Texture Sample node → right-click → Convert to Texture Sample Parameter → in the Sampler Type dropdown, the sampler type controls filtering. Anisotropy settings come from the texture asset's LOD group unless overridden in a sampler state object.

**In code (UE5 C++ / RHI level):** `FSamplerStateInitializerRHI` struct, `Filter` field. UE5's RHI abstraction maps its own filter enum to Vulkan/DX12/Metal sampler states.

---

## Part B: Numeric Worked Example

**Setup:** Fragment with UV coordinate (0.5, 0.5) on a 512×512 texture. Texture-space gradient vectors (already in texel space, not normalized UV) are given.

**Given:**
```
dFdx(uv) in texel space: (0.5, 0.1)     ← stepping one screen pixel right
dFdy(uv) in texel space: (0.1, 8.0)     ← stepping one screen pixel down
```

---

### Step 1: Compute gradient vector lengths.

```
||dpdx|| = sqrt(0.5² + 0.1²)
         = sqrt(0.25 + 0.01)
         = sqrt(0.26)

Computing sqrt(0.26):
    0.5² = 0.25  → sqrt(0.25) = 0.500
    0.51² = 0.2601 → too high by 0.0001
    0.509² = 0.259081 → close but slightly low
    0.510² = 0.260100 → matches 0.26 to 4 significant figures
    ∴ sqrt(0.26) ≈ 0.5099 ≈ 0.510

||dpdx|| ≈ 0.510 texels
```

```
||dpdy|| = sqrt(0.1² + 8.0²)
         = sqrt(0.01 + 64.00)
         = sqrt(64.01)

Computing sqrt(64.01):
    8.0² = 64.00 exactly
    8.001² = 64.016001 → slightly above 64.01
    8.0006² = 64.0096... → getting closer
    More precisely: sqrt(64.01) = 8 × sqrt(1 + 0.01/64) ≈ 8 × (1 + 0.01/128) ≈ 8 × (1 + 0.0000781) ≈ 8.000625

    ∴ sqrt(64.01) ≈ 8.001 (to 4 significant figures)

||dpdy|| ≈ 8.001 texels
```

---

### Step 2: Identify major and minor axes.

```
||dpdx|| = 0.510   ← MINOR (shorter)
||dpdy|| = 8.001   ← MAJOR (longer)

Major axis vector: (0.1, 8.0)   (the dFdy vector)
Minor axis vector: (0.5, 0.1)   (the dFdx vector)
```

---

### Step 3: Select mip level based on minor axis.

```
LOD = log2(||minor||) = log2(0.510)

Computing log2(0.510):
    log2(0.5) = -1 exactly (since 2^(-1) = 0.5)
    0.510 = 0.5 × 1.02
    log2(0.510) = log2(0.5) + log2(1.02)
                = -1 + log2(1.02)

    log2(1.02):
        = ln(1.02) / ln(2)
        ln(1.02) ≈ 0.02 - 0.02²/2 + 0.02³/3 - ...  (Taylor series ln(1+x) = x - x²/2 + ...)
                 ≈ 0.02 - 0.0002 + 0.0000027 ≈ 0.019803
        ln(2) = 0.693147
        log2(1.02) = 0.019803 / 0.693147 = 0.02858

    log2(0.510) = -1 + 0.02858 = -0.9714
```

LOD = -0.9714. Clamp to 0 (cannot go below mip level 0). Use mip level 0 (full resolution: 512×512).

---

### Step 4: Compute the anisotropy ratio.

```
N_raw = ||major|| / ||minor|| = 8.001 / 0.510

8.001 / 0.510:
    8 / 0.5 = 16 exactly
    Adjustment: 8.001 / 0.510 vs 8 / 0.5
    8.001 / 0.510 = 8001 / 510 = 15.6882...

    Let's compute:
    510 × 15 = 7650
    8001 - 7650 = 351
    351 / 510 = 0.6882

    N_raw = 15.688
```

Hardware maximum supported: 16 (typical desktop GPU).

```
N_clamped = min(15.688, 16) = 15.688
N = ceil(15.688) = 16 samples
```

---

### Step 5: Compute the step vector along the major axis.

The major axis vector is dFdy in texel space: `(0.1, 8.0)`. We need to distribute 16 samples evenly along this vector, centered at the fragment UV.

We express the step in texel space, then convert back to normalized UV by dividing by texture dimensions (512, 512):

```
Total span in texel space: (0.1, 8.0)   (the full major axis vector)
Step between samples: major_vector / (N - 1) = (0.1, 8.0) / 15

Step_texel = (0.1/15, 8.0/15) = (0.00667, 0.5333) texels per step

Convert to normalized UV:
Step_UV = (0.00667/512, 0.5333/512) = (0.0000130, 0.001042) per step
```

Center UV: (0.5000, 0.5000).

Start position (sample 0): center - 7.5 × step_UV
```
7.5 × step_UV = 7.5 × (0.0000130, 0.001042) = (0.0000975, 0.007813)
Start = (0.5000 - 0.0000975, 0.5000 - 0.007813) = (0.499903, 0.492188)
End   = (0.5000 + 0.0000975, 0.5000 + 0.007813) = (0.500098, 0.507813)
```

**First 3 sample positions (normalized UV):**
```
Sample 0:  (0.499903, 0.492188)
Sample 1:  (0.499903 + 0.0000130, 0.492188 + 0.001042) = (0.499916, 0.493229)
Sample 2:  (0.499916 + 0.0000130, 0.493229 + 0.001042) = (0.499929, 0.494271)
```

**Last 3 sample positions (normalized UV):**
```
Sample 13: center + 5.5 × step_UV = (0.5000 + 5.5 × 0.0000130, 0.5000 + 5.5 × 0.001042)
         = (0.5000 + 0.0000715, 0.5000 + 0.005729) = (0.500072, 0.505729)
Sample 14: (0.500072 + 0.0000130, 0.505729 + 0.001042) = (0.500085, 0.506771)
Sample 15: (0.500085 + 0.0000130, 0.506771 + 0.001042) = (0.500098, 0.507813)
```

**Verification:** 
```
V span: 0.507813 - 0.492188 = 0.015625 in normalized UV
In texels: 0.015625 × 512 = 8.0 texels
Target major axis length: 8.001 texels ✓

U span: 0.500098 - 0.499903 = 0.000195 in normalized UV  
In texels: 0.000195 × 512 = 0.100 texels
Target U component of major axis: 0.1 texels ✓
```

---

### Step 6: Final result.

The GPU performs 16 bilinear lookups at these UV positions using mip level 0 (512×512 full resolution). Let each bilinear result be `color[i]`. The final fragment color is:

```
result = (color[0] + color[1] + ... + color[15]) / 16
```

This is a box filter with 16 equally weighted samples, distributed along the major axis (approximately the V direction in texture space) at the full resolution mip level.

The minor axis (approximately U direction) is handled by the bilinear interpolation within each sample, which covers about 1 texel at level 0 — appropriate for a minor axis footprint of 0.510 texels (slight over-blur, but far less than the 15.6× over-blur of the isotropic approach).

---

## Pitfalls

**Pitfall 1: Enabling AF with GL_NEAREST point filtering.**

Exact mistake: setting `GL_TEXTURE_MAX_ANISOTROPY_EXT` on a texture that uses `GL_NEAREST` or `GL_NEAREST_MIPMAP_NEAREST` filtering, expecting AF to improve quality.

Exact symptom: texture still shows pixel-perfect nearest-neighbor artifacts and the blocky appearance characteristic of point sampling. No visible improvement from enabling AF. The texel reads associated with AF produce sharp rectangular samples that get averaged, but the block artifact is visible in each sample.

Exact fix: AF requires `GL_LINEAR` filtering as the base interpolation mode. Set `GL_TEXTURE_MIN_FILTER` to `GL_LINEAR_MIPMAP_LINEAR` (trilinear) and `GL_TEXTURE_MAG_FILTER` to `GL_LINEAR` before enabling AF. The AF algorithm takes multiple bilinear samples — "bilinear" is the operative word. No bilinear → no benefit.

**Pitfall 2: Enabling Vulkan AF without requesting the device feature.**

Exact mistake: creating a `VkSampler` with `anisotropyEnable = VK_TRUE` and `maxAnisotropy = 16.0f` without setting `VkPhysicalDeviceFeatures::samplerAnisotropy = VK_TRUE` in `VkDeviceCreateInfo::pEnabledFeatures`.

Exact symptom: Vulkan validation layers report: `"VUID-VkSamplerCreateInfo-anisotropyEnable-01070: anisotropyEnable is VK_TRUE but the samplerAnisotropy feature is not enabled."` Without validation layers: undefined behavior, potentially crashes on some drivers or silently falls back to bilinear on others.

Exact fix: Before device creation, query `VkPhysicalDeviceFeatures` to verify support. Then in `VkDeviceCreateInfo`, set `enabledFeatures.samplerAnisotropy = VK_TRUE`. This is a Vulkan-wide pattern: every non-universal feature (geometry shaders, tessellation, multi-draw indirect, etc.) must be explicitly opted into at device creation. AF is one of the most commonly forgotten.

**Pitfall 3: Expecting AF to benefit shadow maps.**

Exact mistake: enabling AF on the shadow map's sampler/texture, expecting softer or higher-quality shadows.

Exact symptom: no change in shadow quality. Or — with certain sampler configurations — shadow comparison produces unexpected results because the AF sampler averages depth values rather than applying percentage-closer filtering (PCF) correctly.

Exact fix: shadow maps use depth comparison sampling (`GL_COMPARE_R_TO_TEXTURE` / `VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE` with `compareEnable = VK_TRUE`). The "color" value being filtered is a binary in-shadow / not-in-shadow result (0 or 1), or a depth value being compared against the fragment's depth. Averaging multiple binary comparison results IS PCF — but AF applies its samples along an anisotropic major axis, not as a regular kernel around the sample point. This does not produce the soft shadow kernel that PCF is designed for. For soft shadows: use a PCF kernel (multiple comparison taps in a regular 3×3 or 5×5 grid around the shadow map texel) instead of AF on the shadow sampler.

---

## What to Build

**Exercise 1: Query and print your GPU's maximum anisotropy.**

After OpenGL context initialization, add this diagnostic:
```c
float maxAniso = 0.0f;
glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
printf("Max anisotropy: %.0fx\n", maxAniso);
```

Run it. Most desktop GPUs report 16. Print the value, then enable it on your floor texture:
```c
glBindTexture(GL_TEXTURE_2D, floorTexture);
glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAniso);
```

Compare the floor texture before and after. The visual difference should be significant on a surface viewed at a shallow angle.

**Exercise 2: Visualize the major-axis direction in your fragment shader.**

Add this to your fragment shader (requires GLSL 400+ for `dFdx`/`dFdy`):
```glsl
vec2 dpdx = dFdx(vTexCoord) * vec2(textureSize(uTex, 0));
vec2 dpdy = dFdy(vTexCoord) * vec2(textureSize(uTex, 0));

float lenX = length(dpdx);
float lenY = length(dpdy);

vec2 majorAxis = (lenX >= lenY) ? dpdx : dpdy;
vec2 majorDir  = normalize(majorAxis);

// Encode direction as color: R = X component, G = Y component
// Remap from [-1,1] to [0,1]
fragColor = vec4(majorDir * 0.5 + 0.5, 0.0, 1.0);
```

Run this shader on your floor texture. Near the camera (where the floor is viewed more perpendicularly), the colors should vary. Far from the camera (grazing angle), the major axis should converge strongly to the V direction (the receding direction) and the color should be a consistent green (high G component). This is the exact direction along which AF is probing for each fragment.

Note: on older GLSL versions, `textureSize` requires passing the mip level. Use `textureSize(uTex, 0).xy` to get level-0 dimensions.
