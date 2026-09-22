# Lesson 4C — Modern Shading: PBR, Pipeline Placement, and Industry Context

**Date:** Sept 21, 2026  
**Prerequisites:** L4B (Shading Models)  
**Goal:** Understand why Blinn-Phong was replaced, what PBR actually means, where shading lives in a modern pipeline, and how the concepts from L4A and L4B appear in real production systems.

---

## Why Blinn-Phong Was Not Good Enough

Blinn-Phong looks convincing for many materials but fails in specific ways that became increasingly visible as rendering quality improved:

### Problem 1: Energy Is Not Conserved

Physical law: a surface cannot reflect more light than it receives. The Phong/Blinn-Phong specular formula `k_s * (N·H)^n` has no constraint ensuring that total reflected energy (diffuse + specular) ≤ incident energy. In practice, for certain material parameters and at grazing angles, the model can output more light than went in. This produces overbright highlights on surfaces viewed at a glance angle.

### Problem 2: Metals Look Wrong

Real metals (copper, gold, chrome) have no diffuse component. All light either reflects (specular) or is absorbed. The specular color of a metal is the metal's actual color (gold specular = golden highlights). In Blinn-Phong:
- k_d is the material's diffuse color
- k_s is a separate specular color, usually grey/white

For metal, you need k_d = 0 and k_s = the metal's tint. Artists had to manually tweak three separate parameters (k_a, k_d, k_s, n) to approximate this. The results were inconsistent across different lighting conditions.

### Problem 3: Highlights Change Shape With Viewing Angle

In Blinn-Phong, as you rotate the camera around a surface, the specular highlight shape and intensity change in ways that are not physically consistent. It is an ad-hoc approximation with no grounding in actual surface physics.

### Problem 4: Not Portable Across Lighting Conditions

A Blinn-Phong material tuned to look correct under one set of lights looks wrong under different lighting conditions. Artists had to tweak material parameters separately for outdoor scenes, indoor scenes, etc. Physical materials look correct under any lighting.

---

## What PBR Actually Is

**Physically Based Rendering (PBR)** is not a single formula — it is a set of constraints and a class of models that satisfy them:

1. **Energy conservation:** Total reflected energy ≤ incident energy.
2. **Reciprocity (Helmholtz):** The BRDF (see below) gives the same value if you swap the light direction and view direction.
3. **Physically plausible parameters:** Material properties correspond to measurable real-world quantities (roughness, metalness, base color).

### The Rendering Equation

The theoretical foundation is the rendering equation (Kajiya, 1986):

```
L_o(x, ω_o) = L_e(x, ω_o) + ∫ f_r(x, ω_i, ω_o) * L_i(x, ω_i) * (N · ω_i) dω_i
```

Where:
- L_o = outgoing radiance at point x in direction ω_o (toward camera)
- L_e = emitted radiance (for emissive surfaces like light bulbs)
- f_r = **BRDF** — the bidirectional reflectance distribution function
- L_i = incoming radiance from direction ω_i
- (N · ω_i) = Lambert's cosine factor
- ∫ dω_i = integral over all incoming directions (the entire upper hemisphere)

The integral over the hemisphere is what makes this hard. Film renderers evaluate it by Monte Carlo sampling (path tracing — shoot many rays, average the results). Real-time renderers approximate it using only direct light sources (no integral over all directions) plus pre-computed terms for indirect light (environment maps, precomputed irradiance).

You do not need to implement the full rendering equation. But understanding it tells you:
- Lambert's N·L is just the (N · ω_i) factor in the equation — it comes from geometry, not from the BRDF
- The BRDF f_r is what describes the material; swapping f_r gives you a different material model
- Phong shading = using an empirical BRDF (not physically derived); PBR = using a physically derived BRDF

### The BRDF

A BRDF takes two directions (incoming light direction, outgoing view direction) and returns a scalar (or RGB): how much of the incoming light in direction ω_i is reflected toward ω_o.

For diffuse surfaces: BRDF = `k_d / π`  
(Lambertian diffuse — constant across all directions, divided by π for energy conservation)

For specular surfaces, the standard real-time PBR BRDF is **Cook-Torrance**:

```
f_r_specular = (D * F * G) / (4 * (N·L) * (N·V))
```

Where:
- **D** = Distribution function — how many microfacets on the surface are oriented to reflect light toward the camera (uses roughness)
- **F** = Fresnel term — how much light reflects vs refracts at the surface based on viewing angle
- **G** = Geometry/shadowing-masking function — accounts for microfacets blocking each other

This looks complicated. The key ideas:

---

## The Three PBR Terms Explained

### D — Normal Distribution Function (Roughness)

Real surfaces are not perfectly smooth. At a microscopic level, they have tiny bumps and grooves (microfacets). The surface "roughness" parameter describes the statistical distribution of microfacet orientations.

- **Low roughness (0.0–0.2):** Most microfacets are aligned — surface is mirror-like. Tight, sharp specular highlight.
- **High roughness (0.8–1.0):** Microfacets point in many directions — surface is matte. Broad, diffuse-looking specular spread.

The normal distribution function D gives the fraction of microfacets with their normal aligned with H (the half-vector). High D when the surface normal and H are well-aligned (which happens when roughness is low and geometry is right). The most common D function in real-time PBR is **GGX (Trowbridge-Reitz)**:

```
D_GGX = α² / (π * ((N·H)² * (α² - 1) + 1)²)
```

Where α = roughness^2 (squaring gives more perceptually linear artistic control).

You do not need to implement this from scratch yet — just understand that D controls how "spread out" the specular highlight is based on roughness.

### F — Fresnel Term (Angle-Dependent Reflectivity)

The Fresnel effect: at grazing angles, every surface becomes highly reflective, regardless of material. Look at water straight down — you see the bottom. Look at water at a near-horizontal angle — you see a perfect mirror reflection of the sky. This is the Fresnel effect.

The Schlick approximation (used in all real-time PBR):
```
F = F0 + (1 - F0) * (1 - N·V)^5
```

Where:
- **F0** = reflectivity at normal incidence (looking straight at the surface)
- **(1 - N·V)^5** = increases toward 1 as the viewing angle becomes more grazing

For dielectrics (non-metals): F0 ≈ 0.04 (very low base reflectivity — non-metals are dark in specular at head-on angles).  
For metals: F0 = the metal's actual color — gold = (1.0, 0.86, 0.57), copper = (0.95, 0.64, 0.54).

This is why metals look colored in their reflections and dielectrics look grey/white — the Fresnel F0 is the specular color.

### G — Geometry / Shadowing-Masking

Microfacets can shadow and mask each other. A rough surface has many microfacets in shadow (blocked from the light) or masked (blocked from the camera). G accounts for this, reducing specular intensity on rough surfaces where self-shadowing is significant.

---

## The Metallic-Roughness Material Model

All three terms (D, F, G) are derived from just **two artistic parameters**:

- **Roughness:** 0.0 (mirror) to 1.0 (fully rough)
- **Metalness (metallic):** 0.0 (dielectric/non-metal) to 1.0 (pure metal)

Plus:
- **Base Color (albedo):** The RGB color of the surface

The metallic parameter blends between two behaviors:
- metallic = 0: F0 = 0.04, diffuse = base_color, specular = grey/white tint
- metallic = 1: F0 = base_color, diffuse = 0 (no diffuse for metals), specular = base_color tint

This two-parameter system replaced the k_a/k_d/k_s/n four-parameter Blinn-Phong system and produces more consistent, physically correct results across lighting conditions.

Unreal Engine 4's material system exposes exactly these parameters: Base Color, Metallic, Roughness. Unity's Standard Shader (HDRP and legacy URP) exposes the same.

---

## Where Shading Lives in the Modern Pipeline

This maps directly to what you have been learning in L1 and L2.

### Legacy (Pre-2000)

Gouraud shading. Lighting computed in the **vertex shader** (or the fixed-function T&L unit). Smooth shading was an upgrade from flat, and it was cheap.

### 2004–2010: Phong/Blinn-Phong Per-Fragment

With shader model 3.0 (DirectX 9, OpenGL 2.0), fragment shaders became powerful enough for per-pixel lighting. The industry moved lighting to the **fragment shader**. Blinn-Phong per-pixel became standard.

Pipeline placement (pseudocode):
```
// vertex shader: transform positions and normals, pass to fragment shader
out_position_world = model_matrix * in_position;
out_normal_world   = normalize(normal_matrix * in_normal);  // normal_matrix = transpose(inverse(model))
gl_Position        = projection * view * out_position_world;

// fragment shader: evaluate lighting at each pixel
N = normalize(in_normal_world);        // renormalize after interpolation
L = normalize(light_pos - in_pos);
V = normalize(camera_pos - in_pos);
H = normalize(L + V);                  // Blinn-Phong half-vector

diffuse  = k_d * max(0, dot(N, L));
specular = k_s * pow(max(0, dot(N, H)), shininess);
out_color = (k_a + diffuse + specular) * light_color * material_color;
```

This is exactly what you will implement in your upcoming shader work.

### 2010–present: PBR Per-Fragment

Same pipeline placement — fragment shader still runs the lighting math. The math is more complex (D, F, G terms) but the location is identical. You pass roughness, metallic, base_color as uniforms or sample them from textures.

```
// fragment shader (PBR, conceptual)
N = normalize(in_normal_world);
V = normalize(camera_pos - in_pos);
L = normalize(light_pos - in_pos);
H = normalize(L + V);

roughness = texture(roughness_map, in_uv).r;
metallic  = texture(metallic_map, in_uv).r;
albedo    = texture(albedo_map, in_uv).rgb;

F0 = mix(vec3(0.04), albedo, metallic);   // dielectric vs metal F0

D = GGX_distribution(N, H, roughness);
F = schlick_fresnel(V, H, F0);
G = schlick_geometry(N, V, L, roughness);

specular = (D * F * G) / (4.0 * max(dot(N,L),0) * max(dot(N,V),0) + 0.001);
diffuse  = (1.0 - F) * (1.0 - metallic) * albedo / PI;    // no diffuse for metals; energy-conserving

out_color = (diffuse + specular) * light_color * max(dot(N, L), 0);
```

Note: `(1.0 - F)` ensures the diffuse and specular terms together do not exceed incident energy. As F increases (more reflective), diffuse decreases. Energy conservation built in.

---

## Deferred Rendering — Why Multiple Lights Are Hard

In forward rendering (what you are learning), you run one fragment shader pass per light source. One point light = one pass. Twenty point lights = twenty passes, or one pass with a loop over 20 lights. Memory bandwidth and compute scale linearly with lights.

Deferred rendering separates geometry from lighting:

**Pass 1 — G-Buffer pass:** Render scene geometry, but instead of computing lighting, write material properties to textures (called the G-Buffer):
- Texture 0: Base color / albedo (RGB)
- Texture 1: World-space normals (RGB)
- Texture 2: Roughness, metallic, AO (packed into RGB channels)
- Texture 3: Depth (one value per pixel)

**Pass 2 — Lighting pass:** For each light source, render a screen-space shape covering that light's influence (sphere for point light, full screen quad for directional). Read the G-Buffer, evaluate lighting using the stored material data, accumulate into the output buffer.

**Result:** Geometry processed once regardless of light count. Adding 100 point lights adds 100 small lighting passes (touching only pixels the light reaches), not 100 full scene passes.

This is why modern games can have hundreds of dynamic lights. Unreal Engine 4/5, Unity HDRP — both use deferred rendering by default for complex scenes. Blinn-Phong and PBR both work in deferred pipelines.

Deferred has a cost: MSAA (anti-aliasing) becomes difficult because you are doing lighting from stored textures, not from geometry. Transparent objects do not fit the G-Buffer model. Modern engines use hybrid approaches (deferred for opaque, forward for transparent).

---

## Image-Based Lighting (IBL)

Phong/Blinn-Phong compute lighting only from explicit light sources. Real scenes have light coming from the entire environment — sky, walls, ground, bounce light. IBL replaces the ambient term with a precomputed texture representing the environment's contribution.

**Diffuse IBL:** Pre-integrate the Lambert diffuse term over the entire environment into an irradiance cubemap. Sample it with the surface normal. Gives soft, natural ambient light that changes based on where you are in the environment.

**Specular IBL:** Pre-filter the environment map at multiple roughness levels (mip levels). Sample with the reflection vector and the roughness parameter to get the correct environment reflection for the surface's roughness. Sharp reflections from low roughness, blurry from high.

IBL is standard in all modern PBR pipelines. In Unreal Engine, the "Reflection Capture" actors sample the environment and generate IBL probes. In Unity HDRP, "Reflection Probes" do the same.

---

## Industry Mapping — Where Each Model Appears

| Context | Shading Model |
|---|---|
| Legacy OpenGL fixed-function | Gouraud (Phong lighting, vertex-computed) |
| DirectX 9 / OpenGL 2.x era games | Blinn-Phong per-fragment |
| Modern AAA games (Unreal, Unity HDRP) | Cook-Torrance PBR per-fragment + IBL |
| Mobile games (2026) | Blinn-Phong or simplified PBR (mobile BRDF approximations) |
| Film (Arnold, Cycles, RenderMan) | Physically accurate BRDFs + path-traced indirect light |
| Real-time ray tracing (DXR, RTX) | PBR BRDF + hardware-accelerated ray queries for reflections/shadows |
| Shader graphs in Unreal/Unity | PBR parameters (Base Color, Metallic, Roughness) piped into the same Cook-Torrance fragment shader — you are just authoring the inputs |

---

## Your Learning Path From Here

You now have the full conceptual stack:

1. **Physics foundation:** Lambert's cosine law, ambient/diffuse/specular decomposition, light attenuation — L4A
2. **Shading model progression:** Flat → Gouraud → Lambert → Phong → Blinn-Phong — L4B
3. **Modern context:** PBR constraints, BRDF concept, metallic/roughness parameters, pipeline placement, deferred rendering — L4C

When you move to shader implementation:
- Start with **Blinn-Phong per-fragment** — implement N, L, V, H, then ambient + diffuse + specular in the fragment shader
- Add a **directional light** first (uniform L vector), then extend to **point lights** (per-fragment L + attenuation)
- Once Blinn-Phong works and feels solid, PBR is a controlled extension: replace k_d/k_s/n with base_color/metallic/roughness and replace the specular formula with D*F*G

Everything you have written in your current OpenGL projects (VAO, VBO, vertex positions) is the geometry layer. Lighting sits entirely in the shaders on top of that geometry. You are not replacing the pipeline — you are adding math in the stages that already exist.

---

## Pitfalls

**Implementing diffuse without energy conservation.** If you use `k_d = 1.0` and `k_s = 1.0`, the surface reflects twice the incident light. Keep k_a + k_d + k_s ≤ 1.0 for any physically plausible result.

**Forgetting to gamma-correct output.** When you sample textures, albedo values are typically stored in sRGB (gamma-encoded) space. Lighting math must happen in linear space. Convert sRGB to linear before lighting, then convert back to sRGB for output. Skipping this makes colors look wrong especially in lit vs shadowed areas. In GLSL: `albedo = pow(albedo_srgb, vec3(2.2))` to convert to linear; `out_color = pow(lit_color, vec3(1.0/2.2))` to convert back. (Or use `GL_SRGB8_ALPHA8` textures and `GL_FRAMEBUFFER_SRGB`.)

**Confusing roughness^2 vs roughness.** The GGX formula takes α = roughness^2. Many implementations square the roughness going in so the artistic slider feels linear. Forgetting this makes roughness behave non-linearly in a jarring way.

**Dividing by zero in Cook-Torrance.** The denominator `4 * (N·V) * (N·L)` goes to zero at grazing angles. Add a small epsilon: `max(N·V, 0.001)`. A common production bug that shows up as bright spiking at silhouette edges.

**Treating deferred and forward as interchangeable.** Transparent objects cannot be deferred (no G-Buffer slot for blending). When you later add transparency to your project, you will need a forward pass on top of any deferred lighting pass — or just use forward rendering throughout, which is fine for a learning project.
