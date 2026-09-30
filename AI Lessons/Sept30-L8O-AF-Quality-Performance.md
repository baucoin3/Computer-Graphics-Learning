# L8O — AF Quality Levels, Performance Tradeoffs, and API Setup
**Theme A1: UV Mapping + Texture Fundamentals | Stage 5 of 5**

---

## Introduction: From Theory to Production Decision

L8M diagnosed the isotropic over-blur problem. L8N derived the AF algorithm — multiple samples along the major axis at a mip level chosen for the minor axis. Now you need to make real-time production decisions: which AF quality level to set in a given context, how much it actually costs, how to write the API calls correctly in both OpenGL and Vulkan, and — critically — when AF is the wrong tool for the job entirely.

This is the engineering lesson. Theory is done. Here you learn what a shipping graphics programmer actually configures, and why.

---

## Part A: Theory

### Section 1 — What the AF Level Number Actually Means

Hardware anisotropic filtering is specified as a maximum level: 2×, 4×, 8×, or 16×. The number is the **maximum anisotropy ratio** the hardware will attempt to handle. It is a cap on N (the sample count derived from the footprint anisotropy ratio).

When you set `GL_TEXTURE_MAX_ANISOTROPY_EXT = 4.0f`, you are telling the hardware: "Do not take more than 4 samples for anisotropic filtering on this texture, regardless of how anisotropic the footprint is."

**What happens when the scene ratio exceeds your AF level:**

Suppose a floor fragment has a footprint ratio of 20:1 (major axis = 20 texels, minor axis = 1 texel) and you have 4× AF enabled.

- Hardware computes N_raw = 20.
- Hardware clamps: N = min(20, 4) = 4.
- Takes 4 samples along the major axis at a mip level appropriate for the 1-texel minor axis.
- 4 samples cover 4 out of 20 texels along the major axis.
- The remaining 16/20 = 80% of the major axis footprint is not covered by these 4 samples. The LOD selection still uses the minor axis (1 texel → level 0), but the effective filter only sees 4 texels in the major direction when 20 are needed.
- Result: better than trilinear (which would have selected LOD = log2(20) ≈ 4.3, causing severe over-blur in the minor axis), but still slightly blurry along the major axis where the 4-sample coverage runs out.

**The accuracy/cost curve across AF levels:**

For a fragment with footprint ratio R:
- If R ≤ AF_level: perfect coverage. The hardware takes exactly R samples and fully covers the footprint.
- If R > AF_level: partial coverage. Coverage fraction = AF_level / R. The uncovered portion of the major axis blurs into the selected mip level.
- If R = 1 (square footprint): AF takes 1 sample regardless of AF level. Cost is identical to trilinear.

---

### Section 2 — When Each Level Is Sufficient

**1× (trilinear only, AF disabled):**

Correct for surfaces where the anisotropy ratio is always near 1: ceiling textures in a top-down game, walls in a first-person game where the camera is always roughly perpendicular to wall surfaces, screen-space UI elements. For any surface that the camera always faces nearly head-on, AF provides no benefit and costs nothing to skip.

**2× AF:**

Handles up to 2:1 anisotropy correctly. Covers mild oblique viewing: walls at 45° to the camera, ceiling viewed from an angle in a 3D platformer. For most vertical surfaces in a typical FPS, 2× is mathematically adequate — the footprint rarely exceeds 2:1 anisotropy for walls the player faces.

**4× AF:**

Industry-standard minimum for floor and ground textures in most games. Handles anisotropy ratios up to 4:1. For a player camera at eye height viewing a floor with elevation angle 20° or more, the footprint ratio is typically 2:1 to 4:1. 4× AF is sufficient for mid-distance ground. Default AF level in Unreal Engine 5.

**8× AF:**

Noticeably better for ground textures viewed at shallower angles (10–20° elevation). Handles footprint ratios up to 8:1. Most open-world ground textures at 50–100m distance from a standing player will have ratios in the 6:1 to 12:1 range — 8× AF handles the majority of this range correctly. Recommended for any game with visible terrain extending to the horizon.

**16× AF:**

Maximum hardware quality. Handles up to 16:1 anisotropy exactly. Provides the best visual results for:
- Racing games with long straights viewed at near-horizontal angles
- Flight simulators approaching a runway
- Open-world games with very flat ground planes viewed toward the horizon
- Any scene where floor texture quality at extreme distances matters

Visual difference from 8× to 16× is subtle in most first/third-person scenes because the camera elevation angle typically stays above 10°, keeping the anisotropy ratio under 8:1. The difference becomes clear in racing and flight simulation where near-zero elevation angles are sustained for long durations.

---

### Section 3 — Performance Cost: Real Numbers and the Reasoning Behind Them

**The theoretical cost model:**

AF at N× takes N bilinear samples per fragment. Bilinear = 4 texel reads. Trilinear = 8 texel reads. 16× AF = up to 64 texel reads.

If texture reads were the only cost and there were no cache effects, 16× AF would be 8× more expensive than trilinear. This would be completely untenable.

**Why the actual cost is far lower:**

First: cache coherence. The N samples in AF are spatially adjacent in texture space. For a 512×512 texture with 16 samples spaced ~0.5 texels apart, the 16 sample positions span ~8 texels along the major axis. Modern GPU texture caches hold a tile of texture data — typically 4KB to 16KB. Eight adjacent texels at any given mip level are extremely likely to reside in the same cache tile. Once the first sample pulls the cache tile into L1 texture cache (paid for by the first sample), the remaining 15 samples are cache hits — they are nearly free in terms of memory bandwidth. The effective memory bandwidth cost of 16× AF over trilinear in a coherent case is perhaps 2–4× bilinear, not 8×.

Second: adaptive N. The maximum is 16 but the average is much lower. Fragments on near-perpendicular surfaces (N ≈ 1) cost no more than trilinear. Fragments at moderate oblique angles (N ≈ 4) cost 2× bilinear in additional work over trilinear. Only fragments at extreme grazing angles reach N = 16. In a typical indoor first-person scene, the average N across all fragments is probably 2–5. In an open-world game with large terrain, it is higher — perhaps 6–10.

Third: texture unit parallelism. GPU texture units are deeply pipelined and massively parallel. Issuing 16 texture reads in a tight loop (which is what AF does internally) maps well to the pipeline because the texture fetch unit can overlap multiple outstanding requests. This is fundamentally different from 16 independent texture reads to arbitrary locations (like a voxel raycast), which would destroy latency hiding.

**Published benchmarks (approximate, GPU-architecture-dependent):**

| Transition | Typical frame time impact | Notes |
|---|---|---|
| Nearest → Bilinear | 0–2% | Nearly free on modern hardware |
| Bilinear → Trilinear | 0–5% | Texture hardware optimized for this |
| Trilinear → 4× AF | 3–8% | Floor/terrain most affected |
| 4× AF → 8× AF | 2–5% | Diminishing per-step cost |
| 8× AF → 16× AF | 1–4% | Cache coherence makes high N cheap |
| Trilinear → 16× AF total | 5–15% | Scene-dependent, worst case texture-heavy open world |

On NVIDIA Ampere (RTX 3000 series) and AMD RDNA2 (RX 6000 series), AF hardware is mature enough that 16× AF over 8× AF is often within noise in frame timing measurements. The bigger variable is texture bandwidth pressure from the scene overall.

**When AF cost is most noticeable:**

- Very high triangle count with many small textured triangles (many fragments, many unique footprints)
- Large render targets (4K, VR dual-eye render)
- Textures with poor spatial locality (large textures where adjacent fragments sample widely different regions — unusual for most surfaces)

**When AF cost is negligible:**

- Indoor scenes with walls and ceilings predominantly perpendicular to camera
- Scenes dominated by shading cost rather than texture fetch cost
- GPU-memory-bandwidth-limited scenes where AF's coherent accesses barely add to existing bandwidth pressure

---

### Section 4 — Complete OpenGL Setup Code

The full correct sequence for setting up a texture with mipmaps and maximum anisotropic filtering:

```c
GLuint texture;
glGenTextures(1, &texture);
glBindTexture(GL_TEXTURE_2D, texture);

// 1. Upload the image data (your stb_image loaded data, for example):
//    width, height, channels come from stbi_load()
glTexImage2D(GL_TEXTURE_2D,
             0,                  // mip level 0
             GL_RGBA8,           // internal format — explicit sized format, not GL_RGBA
             width, height,
             0,                  // legacy border param, always 0
             GL_RGBA,            // source format
             GL_UNSIGNED_BYTE,   // source type
             imageData);

// 2. Generate the mip chain. Must be done AFTER uploading level 0.
glGenerateMipmap(GL_TEXTURE_2D);

// 3. Set filter modes.
// MIN_FILTER: trilinear — linear interpolation between two mip levels,
//             each level bilinearly filtered. This is the prerequisite for AF.
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
// MAG_FILTER: bilinear — when the texture is magnified (zoomed in),
//             no mip levels are needed, just bilinear within level 0.
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

// 4. Set anisotropic filtering.
// Always query the hardware maximum first.
float maxAniso = 1.0f;
glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);

// Set to hardware maximum. To cap at 8×: min(maxAniso, 8.0f)
glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAniso);

// Done. This texture now has:
// - Full mip chain
// - Trilinear mip filtering (prerequisite for AF)
// - Maximum anisotropic filtering
glBindTexture(GL_TEXTURE_2D, 0);
```

**Using sampler objects (modern OpenGL, preferred):**

Sampler objects separate sampling state from texture data. You can bind the same texture with different sampler configurations at different texture units. This is the Vulkan model brought into OpenGL 3.3+:

```c
GLuint sampler;
glGenSamplers(1, &sampler);

glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

float maxAniso = 1.0f;
glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
glSamplerParameterf(sampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAniso);

// Wrap modes on the sampler, not the texture:
glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_REPEAT);
glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_REPEAT);

// At render time:
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_2D, myTexture);
glBindSampler(0, sampler);    // sampler overrides texture's own parameters
// draw...
glBindSampler(0, 0);          // unbind sampler (revert to texture's parameters)
```

The sampler object is Vulkan-equivalent: all sampling decisions (filter, wrap, LOD clamp, anisotropy) live in the sampler, not in the texture. For production code, prefer sampler objects. You can have one "high quality floor" sampler, one "UI (no mipmaps)" sampler, one "shadow map (compare mode)" sampler, and reuse them across many textures.

---

### Section 5 — Complete Vulkan Setup

```c
// Device creation (do this once at startup):
VkPhysicalDeviceFeatures deviceFeatures{};
deviceFeatures.samplerAnisotropy = VK_TRUE;
// ... pass deviceFeatures to VkDeviceCreateInfo.pEnabledFeatures

// Query the hardware limit:
VkPhysicalDeviceProperties deviceProperties;
vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
float maxAniso = deviceProperties.limits.maxSamplerAnisotropy;
// Typical value: 16.0f on modern desktop GPUs

// Sampler creation (create once, reuse for all floor/terrain textures):
VkSamplerCreateInfo samplerInfo{};
samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
samplerInfo.magFilter               = VK_FILTER_LINEAR;
samplerInfo.minFilter               = VK_FILTER_LINEAR;
samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR; // trilinear
samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
samplerInfo.mipLodBias              = 0.0f;
samplerInfo.anisotropyEnable        = VK_TRUE;
samplerInfo.maxAnisotropy           = maxAniso;  // use full hardware maximum
samplerInfo.compareEnable           = VK_FALSE;  // not a depth comparison sampler
samplerInfo.minLod                  = 0.0f;
samplerInfo.maxLod                  = VK_LOD_CLAMP_NONE; // access all mip levels
samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
samplerInfo.unnormalizedCoordinates = VK_FALSE;  // use normalized [0,1] UV

VkSampler sampler;
vkCreateSampler(device, &samplerInfo, nullptr, &sampler);
```

Note `VK_LOD_CLAMP_NONE` (a large float, typically 1000.0f): allows access to all mip levels. If you set `maxLod = 0`, you disable mipmapping — the sampler will only ever read level 0. This is a common mistake when adapting tutorial code.

**Key Vulkan architecture point:** in Vulkan, ALL texture filtering decisions — filter mode, mipmap mode, wrap mode, anisotropy, LOD bias, LOD clamp, border color, comparison mode — live in the `VkSampler` object. There are no per-texture filtering parameters. The `VkImageView` tells Vulkan what the image is (format, aspect, mip range). The `VkSampler` tells Vulkan how to sample it. They are bound together in `VkDescriptorImageInfo`. This separation is intentional: same image, different samplers, different filtering behavior — no state duplication.

---

### Section 6 — Per-Texture vs. Global AF: Engine Architecture

**Raw OpenGL / Vulkan:** AF is configured per-texture (OpenGL legacy) or per-sampler (OpenGL sampler objects, Vulkan). There is no global AF setting. You control every texture individually.

**Unreal Engine 5:**

Global minimum AF level: Project Settings → Rendering → Textures → Anisotropic Filtering. This sets the minimum applied engine-wide, overriding lower-quality per-texture settings. Individual textures can set a *higher* AF level but cannot go *below* the global minimum.

Per-texture: Texture Editor → Details → Level of Detail → LOD Group. Each LOD group has an associated texture filter quality preset. `TEXTUREGROUP_World`: default (4× AF). `TEXTUREGROUP_Terrain`: typically 16× AF. `TEXTUREGROUP_UI`: no mipmaps, no AF. `TEXTUREGROUP_Skybox`: trilinear, no AF (sky dome is viewed nearly head-on, not obliquely). You can create custom LOD groups in `DefaultDeviceProfiles.ini`.

Scalability: UE5's scalability system (Low/Medium/High/Epic) maps to AF quality levels. "Low" may set 1× AF globally, "Epic" sets 16× AF. This is how players with weaker GPUs automatically get less AF without you writing per-quality-level shader variants.

**Unity HDRP:**

Global AF in HDRP: managed via HDRP Asset → Rendering → Anisotropic Filtering Level. Per-material: texture sampler settings in ShaderLab or via `Texture2D.filterMode` and `Texture2D.anisoLevel` in C# (range 1–16). Unity exposes `anisoLevel = 0` as "forced off regardless of quality settings", `anisoLevel = 1` as "use quality settings", and `2–16` as explicit levels.

---

### Section 7 — When NOT to Use Anisotropic Filtering

AF is the default for most surface textures, but several texture types should not use it:

**Shadow map depth textures:**

Shadow maps use depth comparison sampling. The sampler returns 0 or 1 (in shadow / not in shadow) based on comparing the stored depth against the fragment's depth. Averaging multiple comparison results with AF samples does not produce a meaningful anisotropic blur of the shadow — it produces an irregular PCF kernel with anisotropic sample placement. PCF is the correct technique for soft shadows: regular grid of comparison taps. Do not enable AF on shadow samplers.

In OpenGL: shadow samplers use `GL_TEXTURE_COMPARE_MODE = GL_COMPARE_R_TO_TEXTURE`. AF on such a sampler has driver-defined behavior — some drivers ignore AF for comparison samplers, others apply it incorrectly.

In Vulkan: `VkSamplerCreateInfo::compareEnable = VK_TRUE` and `anisotropyEnable = VK_TRUE` simultaneously is technically valid but the interaction is implementation-defined. Specification says: when comparison is enabled, the filtering applies to the comparison results. On some GPUs this works unexpectedly. Best practice: never enable both simultaneously.

**Normal maps:**

Normal maps store surface normal vectors encoded as (R, G, B) = (Nx*0.5+0.5, Ny*0.5+0.5, Nz*0.5+0.5). When AF filters a normal map, it averages multiple normal samples. The average of two unit vectors is NOT a unit vector — its length is less than 1.

Example: if two adjacent samples are (1, 0, 0) and (0, 1, 0) (90° apart), their average is (0.5, 0.5, 0) with length sqrt(0.5) ≈ 0.707, not 1.0. A non-unit normal in a dot product with the light direction produces a darkened result — `dot(n, l)` will be smaller than for the correct unit normal.

The fix is simple: renormalize after sampling.

```glsl
vec3 sampledNormal = texture(uNormalMap, vTexCoord).xyz * 2.0 - 1.0;
vec3 N = normalize(sampledNormal);   // ← always renormalize
```

In practice, modern engines (UE5, Unity HDRP) still use AF on normal maps because the renormalization fix is standard practice and the visual difference without AF is worse (over-blurred normals → incorrect lighting angle) than the renormalization error. But you must always renormalize.

**Lookup table (LUT) textures:**

Color grading LUTs (3D textures mapping input color → output color), tone mapping curves (1D or 2D textures), and procedural lookup tables should use `GL_LINEAR` only, no mipmaps, no AF. These textures are accessed at known UV coordinates that are derived from color values, not from surface geometry. There is no screen-space derivative in the meaningful sense — you are not sampling a surface footprint. AF would produce meaningless samples spread along an artificial "major axis" with no geometric interpretation.

**Skybox / cubemap environment maps:**

Skyboxes are viewed from inside the cube — effectively perpendicular to the face normal. The anisotropy ratio is always near 1:1. AF provides no benefit. Use trilinear (or bilinear if no reflection detail is needed).

Environment maps (IBL reflection probes) are used for specular reflections. These are sampled via a reflection direction, not a surface UV. The footprint concept does not apply the same way. AF on cubemaps has limited benefit in most cases; filtering is handled by the mip chain selected by roughness level.

---

## Part B: Numeric Worked Example

### Scene Analysis: Open-World Game AF Budget

Consider an open-world third-person action game. Camera is 2.0m above ground, tilts forward at 30° below horizontal (standard over-shoulder camera). Player looks toward the horizon.

**Surface 1: Ground / grass texture (4096×4096)**

At a point 50m ahead, using small-angle approximation for the grazing angle. Elevation angle from horizontal: 30° (the camera tilt). The screen-pixel-to-ground footprint in the receding direction (V) vs. across direction (U):

```
Camera height: h = 2.0m
Distance along ground: d = 50m
Elevation angle from horizontal: θ = 30°

The ground pixel at 50m sees the ground at angle θ = 30°.
Footprint along V (receding, V direction):
    dV ≈ 1 pixel height / (sin(θ) × d/pixel_density)

For a rough estimate — from a known-good empirical rule:
Anisotropy ratio at distance d from a camera at height h with elevation θ:
    N ≈ d / (h / tan(θ)) = d × tan(θ) / h
    
At d = 50m, h = 2.0m, θ = 30°:
    tan(30°) = 1/√3 ≈ 0.5774
    N ≈ 50 × 0.5774 / 2.0 = 28.87 / 2.0 = 14.4

N ≈ 14.4 at 50m.

At d = 100m:
    N ≈ 100 × 0.5774 / 2.0 = 28.9

N ≈ 28.9 at 100m — beyond 16× hardware limit.

At d = 200m:
    N ≈ 200 × 0.5774 / 2.0 = 57.7 — well beyond 16× limit.
```

Verdict: at 50m, 16× AF covers the ratio exactly. At 100m and beyond, 16× AF is insufficient — the ratio exceeds the hardware maximum. The ground at 100m+ will still exhibit some minor over-blur in the receding direction even with maximum AF. This is mitigated in practice by the terrain having sufficient texel density (4096×4096 tiled at high frequency) and by atmospheric haze/fog hiding the far horizon.

Required AF level: **16× AF**.

**Surface 2: Vertical walls (brick texture, 2048×2048)**

Camera facing a wall at 90°. Typical wall viewing in a 3D game: player looks at the wall somewhere between straight-on (0° horizontal deviation) and angled (60° deviation from perpendicular).

```
At 60° deviation from perpendicular (steep viewing angle):
Anisotropy ratio for a wall:
The wall recedes in one direction. At 60° deviation, the foreshortening in the depth direction vs. across direction:
    N ≈ 1 / cos(60°) = 1 / 0.5 = 2.0
```

Anisotropy ratio: 2:1 at the most extreme typical wall viewing angle.

Verdict: 2× AF is mathematically sufficient. 4× AF provides full coverage and is imperceptibly better. 16× AF degrades gracefully to 2 samples for N=2.

Required AF level: **2× AF** (4× for comfort).

**Surface 3: Ceiling (plaster texture, 1024×1024)**

Camera looks up at the ceiling. For a first-person camera at eye height with max upward tilt:

```
At 70° upward tilt (nearly straight up):
    The footprint is nearly square — the across and along components of the ceiling projection are roughly equal.
    N ≈ 1 / cos(70°) ≈ 1 / 0.342 ≈ 2.9
```

Anisotropy ratio: ~3:1 at extreme tilt.

Required AF level: **4× AF**.

**Surface 4: Road texture (asphalt, 2048×2048)**

Racing game camera: 1.0m above road, 10° elevation angle from horizontal. Road stretches 500m to the visible horizon.

```
At d = 30m, h = 1.0m, θ = 10°:
    tan(10°) = 0.1763
    N ≈ 30 × 0.1763 / 1.0 = 5.3

At d = 100m:
    N ≈ 100 × 0.1763 / 1.0 = 17.6 — just beyond 16× hardware limit
    
At d = 50m:
    N ≈ 50 × 0.1763 / 1.0 = 8.8 → requires 8× to 16× AF
```

Even at 50m, a 10° elevation angle gives N ≈ 8.8. At 100m, N exceeds 16. This is extreme anisotropy.

Required AF level: **16× AF** — and accept that beyond 100m, the road will still exhibit residual over-blur. Mitigate with high-resolution road textures or distance-based LOD geometry reducing the road to a flat color at extreme distance.

---

### Memory Cost Calculation

**Memory cost of AF itself: zero additional bytes.**

AF takes extra samples from existing mip levels. No additional textures are created. No additional mip levels are generated. The GL_TEXTURE_MAX_ANISOTROPY_EXT parameter changes only the runtime sampling behavior — not the stored data.

Compare:
- Mipmaps add 1/3 of the base texture size: a 4096×4096 RGBA texture is 64 MB at level 0. Mip chain adds ~21 MB. Total: ~85 MB.
- 16× AF adds: 0 bytes. Total remains ~85 MB.

This is a significant advantage of hardware AF over RIP mapping, which would have multiplied the 85 MB by ~8× for a similar quality result.

---

### GPU Computation Cost Example

**Setup:**

- Display: 1920×1080 pixels
- Scene: 60% of pixels are floor/terrain fragments (areas with high anisotropy)
- Target: understand the raw computation added by 16× AF vs. trilinear

Floor/terrain fragment count:
```
1920 × 1080 = 2,073,600 total pixels
60% on floor: 2,073,600 × 0.60 = 1,244,160 floor fragments
```

Assumption: average effective N for floor fragments = 12 (a reasonable estimate for a mix of near and far floor).

Additional bilinear samples from AF over trilinear baseline (trilinear = 1 sample in this model for simplicity):
```
Additional samples per floor fragment = 12 - 1 = 11
Total additional samples per frame = 1,244,160 × 11 = 13,685,760 additional bilinear lookups
```

Each bilinear lookup touches 4 texels. But assuming 80% cache hit rate (due to spatial coherence):
```
Cache misses requiring memory fetch = 13,685,760 × 0.20 = 2,737,152 actual texture cache misses
```

A modern GPU's texture cache processes misses at roughly one miss per ~100–200 cycles at 1–2 GHz texture throughput. But critically, texture units are massively parallel — modern GPUs have 8–16 or more texture units per compute cluster, each capable of issuing fetches in parallel.

Rather than derive a precise ms estimate (which requires knowing your GPU's specific texture unit count, clock speed, L1/L2 texture cache configuration, and memory subsystem bandwidth), the practical conclusion is:

**The 2.7 million cache misses from AF are distributed across:** all active warps (groups of 32 fragments executing in parallel), all texture units (8–32+ per GPU cluster), and the full frame time (~16ms at 60 FPS). Texture hardware is designed to hide latency with massive parallelism. In practice, this translates to the 5–10% frame time overhead measured empirically, not the 8× theoretical raw cost.

The lesson: GPU cost analysis cannot ignore parallelism and cache architecture. Raw texel count is the wrong metric. Cache coherence and hardware pipeline utilization determine actual cost. This is why AF performs far better than its texel read count suggests.

---

## Pitfalls

**Pitfall 1: Not querying GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT before setting it.**

Exact mistake: hardcoding `glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 16.0f)` without first querying the hardware maximum.

Exact symptom: on a GPU that only supports 8× AF (uncommon on modern hardware, but present on some integrated graphics or older mobile GPUs), this call may silently clamp to 8, or may produce a `GL_INVALID_VALUE` error on strict driver implementations. If you never check `glGetError()`, this fails silently. More seriously, on a system where the extension is not supported at all, `GL_TEXTURE_MAX_ANISOTROPY_EXT` is an undefined enum value — calling `glTexParameterf` with it produces a `GL_INVALID_ENUM` error.

Exact fix:
```c
float maxAniso = 1.0f;  // default to 1× (no AF)
if (GLAD_GL_EXT_texture_filter_anisotropic) {
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
    float desired = 16.0f;  // or your quality setting
    float clamped = (desired < maxAniso) ? desired : maxAniso;
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, clamped);
}
```

Always check extension availability, always query the maximum, always clamp the desired level to the hardware maximum.

**Pitfall 2: Setting maxAnisotropy = 0.0f in VkSamplerCreateInfo with anisotropyEnable = VK_TRUE.**

Exact mistake: constructing `VkSamplerCreateInfo` with `anisotropyEnable = VK_TRUE` and forgetting to set `maxAnisotropy`, leaving it at its zero-initialized value.

Exact symptom: Vulkan validation layer reports: `"VUID-VkSamplerCreateInfo-anisotropyEnable-01076: If anisotropyEnable is VK_TRUE, maxAnisotropy must be between 1.0 and VkPhysicalDeviceLimits::maxSamplerAnisotropy."` Without validation layers: undefined behavior. On some drivers, the sampler uses 0× AF (no filtering). On others, it may use maximum AF regardless. On others, it may crash during command buffer execution.

Exact fix:
```c
VkPhysicalDeviceProperties props;
vkGetPhysicalDeviceProperties(physicalDevice, &props);

samplerInfo.anisotropyEnable = VK_TRUE;
samplerInfo.maxAnisotropy    = props.limits.maxSamplerAnisotropy;
// Always set maxAnisotropy to a valid value (1.0f to maxSamplerAnisotropy)
// when anisotropyEnable is VK_TRUE.
```

Run with validation layers enabled during development. `VK_LAYER_KHRONOS_validation` catches this class of error immediately and verbosely.

**Pitfall 3: Using AF on a texture without mipmaps.**

Exact mistake: setting `GL_TEXTURE_MAX_ANISOTROPY_EXT` on a texture that has no mip levels (only level 0 was uploaded, `glGenerateMipmap` was never called).

Exact symptom: AF provides some improvement over bilinear alone — the multiple samples along the major axis are still taken from level 0 — but the benefit is reduced. The mip-level selection for the minor axis would yield a negative LOD (minor axis < 1 texel), which clamps to level 0. Since only level 0 exists, the hardware is effectively forced to use level 0 for all samples. The minor-axis dimension benefits from the bilinear interpolation within each sample, but the multiple samples don't provide the full anisotropic coverage that they would with a mip chain available. More importantly: without mipmaps, the mip selection for non-AF cases is broken — aliasing from the major axis at far distances (which trilinear would prevent) reappears.

Exact fix: always generate the mip chain before enabling AF. The correct sequence is: `glTexImage2D` → `glGenerateMipmap` → set filter modes (including AF). Never skip `glGenerateMipmap`. If you want to avoid mipmapping deliberately (e.g., for UI textures), do not enable AF either.

**Pitfall 4: Forgetting to renormalize normal maps after AF sampling.**

Exact mistake: sampling a normal map with AF enabled and using the result directly in lighting computations without renormalizing.

Exact symptom: specular highlights are subtly darker than they should be, particularly on surfaces viewed at oblique angles (exactly where AF is most active). The darkening is proportional to how non-unit the averaged normals are — for strongly anisotropic footprints with large angle differences between adjacent normals (detail-rich normal maps), the error can be 10–20% darkening of specular, visible as less shiny-looking surfaces at grazing angles.

Exact fix:
```glsl
// In the fragment shader:
vec3 rawNormal = texture(uNormalMap, vTexCoord).xyz;
vec3 tangentNormal = rawNormal * 2.0 - 1.0;  // remap from [0,1] to [-1,1]
vec3 N = normalize(tangentNormal);             // ← ALWAYS normalize
// Then transform N from tangent space to world space via TBN matrix...
```

The renormalization is a single `normalize()` call and is essentially free. There is no excuse for omitting it. This should be a reflexive part of any normal map sampling.

---

## What to Build

**Exercise 1: The Four-Way Filter Comparison Scene.**

Create a scene with four large floor quads extending from the camera into the distance, all using the same texture but different filter configurations:

```
Quad A: GL_NEAREST (nearest neighbor — no filtering)
Quad B: GL_LINEAR  (bilinear — no mipmaps)
Quad C: GL_LINEAR_MIPMAP_LINEAR, GL_TEXTURE_MAX_ANISOTROPY_EXT = 1.0  (trilinear only)
Quad D: GL_LINEAR_MIPMAP_LINEAR, GL_TEXTURE_MAX_ANISOTROPY_EXT = max  (trilinear + max AF)
```

Display all four quads simultaneously in a single frame, arranged side by side, all receding from the camera at a shallow angle. This is the canonical comparison that appears in every graphics textbook and every GPU vendor's texture filtering demo. Build it yourself. The visual difference is dramatically more instructive than any description. Write 3–4 sentences for each transition: A→B, B→C, C→D. Identify the exact visual artifact that each transition eliminates.

**Exercise 2: The Reusable Texture Setup Helper.**

Write a helper function that you will call for every surface texture in your portfolio projects from now on:

```c
// Sets up mipmaps, trilinear filtering, and AF on the currently bound texture.
// maxAniso: desired maximum AF level (e.g., 16.0f). Clamped to hardware limit.
// Call after glBindTexture and glTexImage2D but before any draw calls.
void setupSurfaceTexture(float desiredMaxAniso) {
    // Generate mip chain
    glGenerateMipmap(GL_TEXTURE_2D);
    
    // Trilinear filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // Clamp wrap (or GL_REPEAT — pass as parameter if needed)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    // Anisotropic filtering
    float maxAniso = 1.0f;
    if (GLAD_GL_EXT_texture_filter_anisotropic) {
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
        float clamped = (desiredMaxAniso < maxAniso) ? desiredMaxAniso : maxAniso;
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, clamped);
    }
}
```

This function encapsulates the correct setup sequence. Use it as your standard for every diffuse texture, every surface normal map (remember to renormalize in the shader), every specular map. You will not need to think about these parameters again — they are correct by default.

**Exercise 3: Frame Timing AF Measurement.**

Measure the real cost of AF on your GPU for your scene. Use CPU-side timing as a rough proxy (not GPU-precise, but adequate for detecting large differences):

```c
// Run 60 frames with trilinear only, record average frame time.
// Then run 60 frames with 16× AF, record average frame time.

auto start = std::chrono::high_resolution_clock::now();
for (int frame = 0; frame < 60; frame++) {
    render();
    glFinish();  // forces GPU to complete all commands before this call returns
}
auto end = std::chrono::high_resolution_clock::now();

double totalMs = std::chrono::duration<double, std::milli>(end - start).count();
double avgMs = totalMs / 60.0;
printf("Average frame time: %.2f ms\n", avgMs);
```

Note: `glFinish()` stalls the CPU until the GPU is idle. This makes frame timing accurate but adds overhead — do not use in production rendering, only for benchmarking. Compare the two averages. What is the difference in ms? Convert to percentage. Is the result consistent with the published 5–15% range, or is your scene texture-limited or not texture-limited?

**Exercise 4 (stretch): Per-Fragment AF Level Heatmap.**

Extend Exercise 2 from L8N (the major-axis direction visualizer) to also show the computed anisotropy ratio N as a color intensity:

```glsl
vec2 dpdx = dFdx(vTexCoord) * vec2(textureSize(uTex, 0));
vec2 dpdy = dFdy(vTexCoord) * vec2(textureSize(uTex, 0));

float lenX = length(dpdx);
float lenY = length(dpdy);
float N_raw = max(lenX, lenY) / max(min(lenX, lenY), 0.001);

// Map: N=1 → blue, N=4 → green, N=8 → yellow, N=16+ → red
float t = clamp((N_raw - 1.0) / 15.0, 0.0, 1.0);
vec3 heatColor = mix(vec3(0,0,1), mix(vec3(0,1,0), vec3(1,0,0), t*2.0 - 1.0), t);

fragColor = vec4(heatColor, 1.0);
```

This shader maps the per-fragment anisotropy ratio to a blue-green-red heat map. Blue means nearly isotropic (AF not needed). Red means N=16+ (maximum AF needed). Run this on your floor scene. You should see: a blue patch directly beneath the camera (near-perpendicular), transitioning through green (moderate AF needed) at mid-distance, and turning red at the far floor approaching the horizon. The red boundary marks exactly where 16× AF becomes insufficient and terrain quality must be handled by other means (fog, LOD, or higher-resolution textures).
