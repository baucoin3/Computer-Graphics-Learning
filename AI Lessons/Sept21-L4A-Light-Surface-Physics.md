# Lesson 4A — Light, Surfaces, and the Physics Foundation

**Date:** Sept 21, 2026  
**Prerequisites:** L1D (Normal Vectors), L1E (Fragment Pipeline)  
**Goal:** Understand what a lighting model is trying to do, where it comes from physically, and master the math of Lambert's cosine law and light attenuation before touching any shading model.

---

## Why Lighting Models Exist

A GPU has no concept of light. It executes math on numbers. When you write a fragment shader that computes a color, you are writing a function that approximates what a physicist would call *radiometric transport* — how energy from light sources travels through a scene, interacts with surfaces, and reaches a camera sensor.

The problem is that the real physics is impossible to compute in real time. A single photon bounces off multiple surfaces before reaching your eye. Film renderers (Pixar's RenderMan, Weta's Arnold) simulate millions of photon paths per pixel — each frame takes minutes to hours. Games run at 60–120 frames per second.

So real-time graphics uses **local lighting models** — simplified approximations that only consider the direct path from a light source to a surface to the camera. No bounced light, no inter-object shadows (without extra work), no spectral wavelengths. Just: "how much of this light source's energy reaches this surface point, and how much of that reflects toward the camera?"

That approximation, done well, produces convincing results. Done badly, it produces plastic-looking, obviously fake objects. Understanding the physics behind the approximation tells you *why* models like Phong fail in certain conditions and *why* PBR replaced them.

---

## The Two Things Light Does at a Surface

When light hits an opaque surface, two things happen:

**Diffuse reflection:** Light penetrates slightly into the surface, bounces around among the material's particles (called subsurface scattering at a micro scale), and exits in all directions roughly equally. This is what gives a surface its **color**. Matte paint, paper, chalk — these are mostly diffuse. Direction to the camera does not matter for diffuse; the surface looks the same brightness from any angle.

**Specular reflection:** Light bounces off the top surface layer at an angle equal to the incoming angle (like a mirror). This produces **highlights** — bright spots that move as you move the camera. Mirrors, polished metal, wet surfaces — these are mostly specular.

Real materials blend both. A painted car door is mostly diffuse (its color) with specular highlights on top. A chrome bumper is almost entirely specular.

There is also a third term added by Phong for bookkeeping: **ambient**, which represents all the scattered light in the environment that has no specific direction. It is not physically derived — it is a hack to prevent surfaces facing away from all lights from being completely black. Modern renderers replace ambient with proper global illumination (ambient occlusion, image-based lighting, or path-traced indirect light).

---

## Surface Normals in Lighting

You already know normals from L1D. For lighting, the key fact is:

**A normal at a surface point tells you which direction "out" is.** That direction determines how much of a light source's energy the surface can absorb.

For now, assume normals are unit vectors (length = 1). You must normalize them before lighting calculations. Non-unit normals produce wrong results — lighting values will be too bright or too dim by a factor equal to the normal's actual length.

---

## Lambert's Cosine Law — The Foundation of Diffuse Lighting

### The Problem It Solves

A flat surface receives more energy from a light source when the light hits it head-on than when it hits at a shallow angle. Think of sunlight at noon (directly overhead) vs. at sunset (near-horizontal). The same amount of light energy is spread over a larger area at shallow angles.

### The Math

Let:
- **N** = surface normal at the point (unit vector, pointing away from surface)
- **L** = unit vector from the surface point toward the light source

Lambert's law says the diffuse intensity is proportional to the **cosine of the angle between N and L**:

```
diffuse_factor = cos(θ) = N · L
```

This follows directly from the dot product definition:

```
N · L = |N| |L| cos(θ) = (1)(1) cos(θ) = cos(θ)
```

Since both are unit vectors, the dot product *is* the cosine.

### Why Cosine Is the Right Measure

Imagine a unit square of light beam hitting a surface. When the beam is perpendicular (θ = 0°), it illuminates exactly one unit square of surface area. When the beam hits at angle θ, the same beam illuminates 1/cos(θ) units of area — more surface, same energy, so each point gets less. Energy per unit area scales by cos(θ). Lambert's law captures this exactly.

### The Clamp

When the light is behind the surface (θ > 90°), the dot product is negative. A negative diffuse contribution makes no physical sense — light behind the surface contributes zero, not negative light. Always clamp:

```
diffuse_factor = max(0.0, N · L)
```

### Worked Example

```
N = (0, 1, 0)   — surface pointing straight up
L = (0.577, 0.577, 0.577)   — light at 45° elevation, normalized

N · L = (0)(0.577) + (1)(0.577) + (0)(0.577) = 0.577

cos(45°) = 0.707... wait, that's not 0.577. Let me recheck.

If L = (0.707, 0.707, 0)  — light at 45° in the XY plane, unit vector
N · L = (0)(0.707) + (1)(0.707) + (0)(0) = 0.707

diffuse_factor = max(0.0, 0.707) ≈ 0.707 = cos(45°) ✓
```

Surface at 45° incidence receives ~70.7% of maximum illumination. Straight overhead (N · L = 1.0) receives 100%.

### Practice Problem

```
N = (0, 0, 1)   — surface normal pointing toward camera (Z+)
L = (-0.5, 0.5, 0.707)   (normalize this first)

Step 1: Compute |L| = sqrt(0.25 + 0.25 + 0.5) = sqrt(1.0) = 1.0  (already unit)
Step 2: N · L = (0)(-0.5) + (0)(0.5) + (1)(0.707) = 0.707
Step 3: max(0.0, 0.707) = 0.707
```

---

## The Ambient + Diffuse + Specular Decomposition

The Phong *lighting model* (not Phong *shading* — these are different things, covered in L4B) decomposes reflected light into three additive terms:

```
I_total = I_ambient + I_diffuse + I_specular
```

Each term has:
- A **material coefficient** (how much of this type the material reflects): k_a, k_d, k_s
- A **light intensity** for that component: I_a, I_d, I_s
- A **geometric factor** (only diffuse and specular depend on geometry)

### Ambient Term

```
I_ambient = k_a * I_a
```

No geometry. Every point on the surface gets this equally regardless of light direction or viewer angle. Prevents absolute-black shadows. Physically wrong but pragmatically necessary until you have global illumination.

### Diffuse Term

```
I_diffuse = k_d * I_d * max(0, N · L)
```

k_d is the diffuse reflectivity — for a red surface, k_d might be (0.8, 0.1, 0.1) in RGB. The material absorbs most green and blue, reflects most red.

### Specular Term

```
I_specular = k_s * I_s * max(0, R · V)^n
```

Where:
- **R** = reflection of L about N (the mirror-reflected light direction)
- **V** = unit vector from surface point toward the camera (view vector)
- **n** = shininess exponent (higher = smaller, tighter highlight)

The formula for R given L and N:

```
R = 2(N · L)N - L
```

Derivation: You project L onto N (that gives you the component of L along N), double it (to get across the normal), then subtract L. This is a mirror reflection.

The `(R · V)^n` factor measures how closely the reflected light direction aligns with the view direction. When they align perfectly, R · V = 1 and specular = k_s * I_s. As the camera moves away from the reflection angle, R · V drops, and raising it to power n makes the falloff steeper. High n (128, 256) = tight metallic highlight. Low n (4, 8) = broad soft highlight.

### Worked Example — Full Phong Lighting Calculation

Setup:
```
N = (0, 1, 0)           — surface pointing up
L = (0, 1, 0)           — light directly above (straight up)
V = (0, 1, 0)           — camera directly above too (viewing straight down)
k_a = 0.1, k_d = 0.6, k_s = 0.3
I_a = I_d = I_s = 1.0   — white light, full intensity
n = 32
```

Step 1 — ambient:
```
I_ambient = 0.1 * 1.0 = 0.1
```

Step 2 — diffuse:
```
N · L = (0)(0) + (1)(1) + (0)(0) = 1.0
I_diffuse = 0.6 * 1.0 * max(0, 1.0) = 0.6
```

Step 3 — specular:
```
R = 2(N · L)N - L = 2(1)(0,1,0) - (0,1,0) = (0,2,0) - (0,1,0) = (0,1,0)
R · V = (0)(0) + (1)(1) + (0)(0) = 1.0
I_specular = 0.3 * 1.0 * (1.0)^32 = 0.3
```

Total: `0.1 + 0.6 + 0.3 = 1.0` — maximum brightness. Light is directly above and camera is directly above, perfect alignment.

Now move the camera to V = (0.707, 0.707, 0) — 45° angle from vertical:
```
R · V = (0)(0.707) + (1)(0.707) + (0)(0) = 0.707
(0.707)^32 = 0.707^32
```

Compute 0.707^32 step by step:
```
0.707^2 = 0.500
0.707^4 = 0.500^2 = 0.250
0.707^8 = 0.250^2 = 0.0625
0.707^16 = 0.0625^2 = 0.00391
0.707^32 = 0.00391^2 = 0.0000153
```

`I_specular = 0.3 * 0.0000153 ≈ 0.0000046` — nearly zero. Camera at 45° from the mirror reflection = almost no specular highlight. This is the specular falloff in action.

---

## Light Types

### Directional Light

Models a light source so far away that all rays are parallel — the sun is the canonical example. Has **no position**, only a **direction**.

```
L = normalize(-light_direction)   — negate because L points FROM surface TO light
```

No attenuation. Every surface point in the scene uses the same L vector. This is why sunlight at noon and sunlight at sunset look different for a surface — only the direction changes, not the intensity.

**Pseudocode placement:** In vertex or fragment shader, L is a uniform (set once per draw call for the whole scene). Compute `N · L` per vertex (Gouraud) or per fragment (Phong).

### Point Light

A light at a specific position in world space. Different surface points receive light from different directions.

```
L = normalize(light_position - surface_position)
```

You must compute L per vertex or per fragment because each point has a different vector to the light.

**Attenuation:** Intensity falls off with distance. Physical falloff is inverse-square:

```
attenuation = 1.0 / (d * d)
```

Where d = distance from surface to light. But pure inverse-square goes to infinity at d = 0 and drops very steeply. The standard real-time approximation uses three constants:

```
attenuation = 1.0 / (k_c + k_l * d + k_q * d^2)
```

- k_c: constant term (prevents infinity at zero distance)
- k_l: linear term (smooth close falloff)
- k_q: quadratic term (physically accurate far falloff)

Typical values for a "medium range" point light: k_c=1.0, k_l=0.09, k_q=0.032.

**Worked attenuation example:**
```
k_c=1.0, k_l=0.09, k_q=0.032

d=1:   1/(1.0 + 0.09 + 0.032) = 1/1.122 = 0.891
d=5:   1/(1.0 + 0.45 + 0.8)   = 1/2.25  = 0.444
d=10:  1/(1.0 + 0.9 + 3.2)    = 1/5.1   = 0.196
d=20:  1/(1.0 + 1.8 + 12.8)   = 1/15.6  = 0.064
```

At distance 20, the light contributes only 6.4% of its maximum. Use this to set a "range" cutoff — discard the light contribution when attenuation < some threshold (e.g., 0.01) to avoid computing lights that barely contribute.

### Spotlight

Point light with angular cutoff. Adds a direction vector (where the cone points) and an angle. Inside the cone, full (or attenuated) intensity. Outside, zero.

Computed using: `cos(angle between L_negated and spotlight_direction)` compared to `cos(cutoff_angle)`. Cosines are monotonically decreasing, so larger cosine = smaller angle = inside cone. Often includes an inner/outer cone for smooth falloff at edges (used in game engines as "inner/outer cone angle").

---

## What Light Intensity Actually Is

In these formulas, I_d and I_s are the light's color multiplied by intensity. For a white light at full intensity, I_d = (1,1,1). For a red light at 50%, I_d = (1,0,0) * 0.5 = (0.5, 0, 0). The material coefficients k_d are also RGB, representing how much of each wavelength channel the material reflects.

The final diffuse contribution for a red surface (k_d = (0.8, 0.1, 0.1)) under white light:
```
I_diffuse_R = 0.8 * 1.0 * (N·L)
I_diffuse_G = 0.1 * 1.0 * (N·L)
I_diffuse_B = 0.1 * 1.0 * (N·L)
```

Under blue light (I_d = (0,0,1)):
```
I_diffuse_R = 0.8 * 0 * (N·L) = 0
I_diffuse_G = 0.1 * 0 * (N·L) = 0
I_diffuse_B = 0.1 * 1 * (N·L) = 0.1 * (N·L)
```

Red surface under blue light appears dark/near-black — the red material has almost no blue to reflect.

---

## Pitfalls

**Forgetting to normalize.** After transforming normals by the model matrix (or inverse transpose — see L1D), they may no longer be unit length. Always normalize before the dot product, in both CPU and shader code.

**Computing L in the wrong space.** N and L must be in the same coordinate space. If N is in world space (after multiplying by the inverse transpose of the model matrix), L must also be in world space (light position in world space minus vertex position in world space). Mixing eye space and world space normals/lights is a common source of broken lighting.

**Not clamping N·L to [0,1].** Negative values mean the surface faces away from the light. They must be zeroed, not left as negative (which could subtract from the ambient term).

**Forgetting attenuation kills range.** Without attenuation, a point light illuminates everything in the scene equally regardless of distance. This is physically wrong and looks wrong. Always add attenuation for point lights.

**Ambient too high.** A common beginner mistake is setting k_a or I_a too high (e.g., 0.5) to avoid dark shadows. This washes out the diffuse term and makes the scene look flat and unlit. Keep ambient low (0.05–0.1) and accept that shadowed areas are dark.

---

## Industry Context

These fundamentals appear everywhere, just dressed differently:

- **Unreal Engine:** Directional light = "Directional Light Actor", point light = "Point Light Actor." The engine evaluates the same N·L math in its deferred shading pass, per-fragment, for all opaque objects.
- **Vulkan/DX12:** You write this math yourself in GLSL/HLSL. There is no "set light position" API call. You pass light data to the fragment shader as a uniform buffer object (UBO) and compute everything by hand.
- **Film (Arnold, Cycles):** Replace the direct illumination approximation with a Monte Carlo path integral that estimates the full rendering equation. Still starts from the same N·L geometry.
- **Automotive visualization (Unreal + real-time ray tracing):** PBR materials with physically accurate area lights, reflection captures, and ray-traced shadows — but the diffuse/specular decomposition and N·L is still the foundation.

Next: L4B covers how these ingredients combine into the shading models — flat, Gouraud, Lambert, Phong, Blinn-Phong — and exactly where in the pipeline each one runs.
