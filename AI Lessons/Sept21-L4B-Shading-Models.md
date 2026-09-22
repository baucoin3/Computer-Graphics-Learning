# Lesson 4B — Shading Models: Flat, Gouraud, Lambert, Phong, Blinn-Phong

**Date:** Sept 21, 2026  
**Prerequisites:** L4A (Light, Surface Physics)  
**Goal:** Understand the five core shading models: what each one computes, where it runs in the pipeline, what artifacts it produces, and why the field moved from one to the next.

---

## Two Things That Are Often Confused

**Lighting model** = the formula for computing reflected light intensity (ambient + diffuse + specular, Lambert, etc.). This is the *physics approximation* you learned in L4A.

**Shading model** = *where* and *how often* you evaluate the lighting formula across a surface. The same Phong lighting formula can produce different visual results depending on whether you evaluate it once per face, once per vertex, or once per pixel.

These two axes are independent. You can evaluate Phong lighting per-vertex (Gouraud-style execution) or per-fragment (true Phong shading). The name "Phong shading" refers specifically to per-fragment evaluation of a lighting model — and historically the lighting model evaluated was the Phong model, so the names blurred together.

This lesson separates them cleanly.

---

## Area 1: Flat Shading

### What It Does

Compute one normal per **face** (triangle). Evaluate the lighting formula once using that face normal. Apply that one color uniformly across the entire face.

### Where It Runs

In a modern pipeline: computed once per primitive in the geometry shader, or by reading a face normal from a texture/buffer, and output as a flat (non-interpolated) varying to the fragment shader. In legacy OpenGL fixed-function: `glShadeModel(GL_FLAT)`.

In a modern vertex shader without a geometry shader: you can approximate it by using the same normal for all three vertices of a triangle — the face normal. This gives flat shading because all three vertices produce the same lighting value, and the rasterizer interpolating identical values still produces a uniform color.

### How to Compute a Face Normal

Given three vertices V0, V1, V2 (in order):
```
edge1 = V1 - V0
edge2 = V2 - V0
face_normal = normalize(cross(edge1, edge2))
```

The cross product is perpendicular to both edges, so it points away from the face. Winding order (clockwise vs counter-clockwise) determines which side it points toward.

### Visual Result

Each face is a solid color. You see the polygon mesh clearly. Sharp silhouettes between adjacent faces with different orientations. Low-polygon models look faceted — like origami or a low-poly game asset.

### When It Was / Is Used

- Legacy hardware (pre-1970s) where interpolation was expensive
- Stylistic choice: low-poly art style in indie games (Monument Valley, Superhot)
- Debugging — if you want to verify face normals are correct, flat shading makes them directly visible as color
- CAD / solid modeling visualization where seeing facets is useful

### What It Cannot Do

Smooth curved surfaces look wrong. A sphere made of 100 triangles looks like 100 flat faces. You see every polygon edge because each face has a slightly different normal and therefore a slightly different color.

---

## Area 2: Lambert Shading (Diffuse-Only)

### What It Does

Evaluate only the diffuse term of the Phong lighting model. No ambient, no specular:

```
I = k_d * I_d * max(0, N · L)
```

Often ambient is added as a small constant to prevent pure-black shadows, making it:

```
I = k_a * I_a + k_d * I_d * max(0, N · L)
```

### Why It Exists as a Separate Model

Lambert shading captures only diffuse reflection, which makes it physically accurate for **perfectly matte, non-shiny surfaces** — chalk, concrete, clay. Adding specular on top of Lambert is exactly what Phong does. Lambert is Phong without the specular term.

### Pipeline Placement

Lambert can run per-vertex (Gouraud execution, see below) or per-fragment. When run per-fragment, Lambert produces smooth matte surfaces without specular artifacts.

### Visual Result

Smooth brightness gradient across surfaces. No highlights. Surfaces facing the light are bright; surfaces facing away are dark (plus ambient). Looks correct for matte materials. Incorrect for anything with gloss or shine.

### Industry Use

Lambert diffuse is still used in PBR workflows as the diffuse component. The specular component is replaced with a more physically accurate model (Cook-Torrance), but the diffuse term is still `k_d * (N · L)` in many implementations. Lambert diffuse is the diffuse component of Unreal's default material.

---

## Area 3: Gouraud Shading (Per-Vertex Interpolation)

### What It Does

1. Compute vertex normals (one per vertex, typically averaged from adjacent face normals)
2. Evaluate the **full lighting formula** (ambient + diffuse + specular) at each **vertex**
3. Output the computed color as a `varying` / `out` from the vertex shader
4. The rasterizer linearly interpolates the color values across the triangle
5. Fragment shader receives the interpolated color — no lighting math runs in the fragment shader

### Where It Runs

Lighting computation: **vertex shader**. This is Gouraud shading in modern terms.

Pseudocode (vertex shader logic):
```
// vertex shader
vec3 N = normalize(normal_in_world_space);
vec3 L = normalize(light_pos - vertex_pos_world);
vec3 V = normalize(camera_pos - vertex_pos_world);
vec3 R = reflect(-L, N);   // or: 2*(dot(N,L))*N - L

float ambient  = k_a;
float diffuse  = k_d * max(0.0, dot(N, L));
float specular = k_s * pow(max(0.0, dot(R, V)), shininess);

color_out = (ambient + diffuse + specular) * light_color * material_color;
// color_out gets interpolated and passed to fragment shader
```

```
// fragment shader
out_color = color_in;   // just output the interpolated color, no lighting math
```

### Why Vertex Normals (vs Face Normals)

For Gouraud and Phong shading, you want smooth surfaces. You assign each vertex a normal that is the **average of all face normals of adjacent faces**. This averaged normal points "outward" from the surface in a smooth way, so lighting transitions smoothly across the model.

For a sphere approximated by triangles, the averaged vertex normal at each vertex points radially outward from the sphere center — as if the sphere were truly round. This is how you fake smooth curvature with a triangulated mesh.

### The Gouraud Artifact — Specular Highlight Interpolation Problem

Gouraud shading fails badly at specular highlights. The highlight must fall somewhere on a face to be visible. If the highlight falls in the *middle* of a face but not near any vertex, all three vertex colors are dark (no highlight was detected at the vertices), and interpolation produces no highlight at all. The highlight disappears.

If the highlight falls near a vertex, it appears at that vertex and then smears/stretches as the rasterizer interpolates it toward adjacent vertices. Highlights look wrong — they elongate, flicker as geometry moves, or disappear entirely.

This is not a correctness problem with low-polygon models. It is a fundamental limitation: specular highlights are a high-frequency phenomenon (sharp, small, position-sensitive), but Gouraud shading evaluates them at a low sampling rate (one sample per vertex, then blurs them via interpolation).

### Visual Result

Smooth diffuse shading on curved surfaces — much better than flat. Specular highlights are incorrect on anything but very high-polygon models.

### When It Was Used

Default shading in OpenGL 1.x fixed-function (`glShadeModel(GL_SMOOTH)`). Standard in games through the mid-1990s. Still used today where fragment shaders are expensive (embedded/mobile hardware with tight budgets, or intentionally stylized low-fi rendering).

---

## Area 4: Phong Shading (Per-Fragment)

### The Key Insight

Instead of interpolating **colors** (Gouraud), interpolate **normals**. Evaluate the lighting formula per fragment using the interpolated normal.

### Where It Runs

Lighting computation: **fragment shader**.

Pseudocode:
```
// vertex shader
normal_out = normalize(normal_matrix * normal_in);     // transform normal to world/eye space
pos_out    = (model_matrix * vec4(pos_in, 1.0)).xyz;   // position in world space
// These get interpolated by the rasterizer

// fragment shader
vec3 N = normalize(normal_in);     // interpolated normal — renormalize! (see pitfalls)
vec3 L = normalize(light_pos - pos_in);
vec3 V = normalize(camera_pos - pos_in);
vec3 R = 2.0 * dot(N, L) * N - L;   // or: reflect(-L, N)

float ambient  = k_a;
float diffuse  = k_d * max(0.0, dot(N, L));
float specular = k_s * pow(max(0.0, dot(R, V)), shininess);

out_color = (ambient + diffuse + specular) * light_color * material_color;
```

### Why This Fixes Gouraud's Specular Problem

With Gouraud, you are interpolating a scalar or color (the *result* of lighting). Interpolated results smear highlights.

With Phong shading, you are interpolating **normals** — the *input* to the lighting formula. A smooth curved surface has normals that vary smoothly across it. Interpolating normals gives you a reasonable normal at every pixel, and the lighting formula evaluated at each pixel correctly detects and places highlights wherever the geometry says they should appear.

The highlight is computed at the fragment level, not averaged from vertices. It appears at the correct position and has the correct shape.

### Why Interpolated Normals Must Be Renormalized

The rasterizer interpolates the three vertex normals linearly across the triangle. Linear interpolation of unit vectors does not produce unit vectors — the interpolated vector has length less than 1.0 except exactly at the vertices. If you use the non-unit interpolated normal in the dot product, the diffuse and specular values are wrong (too dark). Always normalize the interpolated normal in the fragment shader.

### Worked Example — Detecting the Difference

Consider a sphere with a single point light above it. The highlight should appear at the top of the sphere.

**Gouraud:** The top vertex is lit brightly. Adjacent vertices are dimmer. The rasterizer interpolates from bright-at-top to dim-at-neighbor. The highlight is a broad smeared gradient across many triangles around the top. It does not look sharp or physically correct.

**Phong:** Every fragment at the top of the sphere has an interpolated normal pointing upward, and L also points upward, so N·L is high and the full specular formula fires. Moving one triangle down, the normals interpolate to point slightly sideways, N·L drops, and (R·V)^n drops faster still. The highlight is sharp, physically sized according to the shininess exponent, and appears exactly where the geometry says it should.

### Phong vs Gouraud — When They Look the Same

On a very high-polygon model (normals per vertex very densely packed), Gouraud and Phong produce near-identical results for diffuse shading. Specular still diverges. For diffuse-only (Lambert) materials, Gouraud is often acceptable even at moderate polygon counts.

---

## Area 5: Blinn-Phong (The Standard Specular Model Until PBR)

### The Problem with Phong's R · V

Computing the reflection vector R is moderately expensive (multiple multiplications and a subtraction). More importantly, when the camera is near the surface plane (grazing angles), the angle between R and V can exceed 90°, making R · V negative — the specular term goes to zero abruptly. This produces an unrealistic hard cutoff in the specular highlight at grazing angles.

### Blinn's Half-Vector Fix

Jim Blinn (1977) proposed replacing R · V with a half-vector:

```
H = normalize(L + V)
```

H is the unit vector halfway between L and V. Then use N · H instead of R · V:

```
I_specular = k_s * I_s * max(0, N · H)^n
```

### Why This Is Better

1. **Cheaper to compute.** One normalization instead of a reflection formula.
2. **No grazing cutoff.** N · H never goes negative when the light and viewer are on the same side of the surface. The highlight fades smoothly to zero instead of cutting off.
3. **Better artistic behavior.** Blinn-Phong highlights behave more like real materials over a wide range of viewing angles. The Phong model's highlights change shape as the view angle changes; Blinn-Phong's shape is more stable.
4. **Closer to physical.** For certain material models (particularly microfacet distributions), N · H is exactly the correct quantity to compute. Blinn-Phong is a special case of the microfacet specular model with a simple distribution.

### The Shininess Exponent Adjustment

Because N · H is always larger than R · V for equivalent configurations, you need a higher exponent to get an equivalent highlight size. A common rule of thumb: Blinn-Phong shininess ≈ 4× Phong shininess for a visually similar highlight.

### Worked Example — Blinn-Phong vs Phong

Setup (Phong):
```
N = (0, 1, 0)
L = (0.707, 0.707, 0)    — light at 45°
V = (-0.707, 0.707, 0)   — camera at 45° other side (symmetric)
```

Phong: Compute R:
```
N · L = 0.707
R = 2(0.707)(0,1,0) - (0.707, 0.707, 0)
  = (0, 1.414, 0) - (0.707, 0.707, 0)
  = (-0.707, 0.707, 0)
R · V = (-0.707)(-0.707) + (0.707)(0.707) = 0.5 + 0.5 = 1.0
I_specular = k_s * (1.0)^n = k_s
```

Blinn-Phong: Compute H:
```
L + V = (0.707 + (-0.707), 0.707 + 0.707, 0) = (0, 1.414, 0)
H = normalize(0, 1.414, 0) = (0, 1, 0)
N · H = (0)(0) + (1)(1) + (0)(0) = 1.0
I_specular = k_s * (1.0)^n = k_s
```

Both give 1.0 here because this is a perfectly symmetric configuration. The difference appears at asymmetric viewing angles and grazing conditions.

### Industry Status

Blinn-Phong replaced Phong as the standard specular model for real-time graphics through the 2000s. It was the default in DirectX 9 / OpenGL 2.x era games. It is still used in lightweight or mobile rendering pipelines where PBR is too expensive. OpenGL's built-in lighting (deprecated) used Blinn-Phong.

---

## Side-by-Side Summary

| Model | Where computed | Interpolates | Specular quality | Cost |
|---|---|---|---|---|
| Flat | Once per face | Nothing (uniform face color) | None | Lowest |
| Lambert | Per vertex or fragment | Color (vertex) or nothing (fragment) | None | Low |
| Gouraud | Per vertex (vertex shader) | Color (output of lighting formula) | Poor — smearing | Low |
| Phong | Per fragment (fragment shader) | Normals | Correct | Medium |
| Blinn-Phong | Per fragment (fragment shader) | Normals | Better + cheaper | Medium |

"Medium cost" for Phong/Blinn-Phong is still very fast on modern hardware. A fragment shader doing Blinn-Phong is a few dozen instructions. PBR is 200–500 instructions but still real-time at 60fps on modern GPUs.

---

## The Progression — Why Each Model Was Necessary

**Flat → Gouraud:** Flat shading makes meshes look angular. Gouraud adds smooth shading across faces by interpolating. The first practical improvement for curved-surface rendering.

**Gouraud → Phong shading:** As hardware got faster, you could afford to move lighting into the fragment shader. This fixed the specular smearing problem and allowed correct highlights on curved surfaces.

**Phong → Blinn-Phong:** A minor refinement (same cost, better behavior) that became the standard almost immediately after Blinn published it.

**Blinn-Phong → PBR:** By the 2010s, artists found Blinn-Phong frustrating. Metallic surfaces required careful manual tweaking. Energy was not conserved (a surface could reflect more light than it received at extreme angles). PBR replaced the ad-hoc k_a/k_d/k_s parameters with physically derived BRDF parameters. Covered in L4C.

---

## Pitfalls

**Confusing shading model with lighting model.** Phong lighting = the k_a + k_d * N·L + k_s * (R·V)^n formula. Phong shading = evaluating that (or any) lighting formula per fragment. These are orthogonal.

**Not renormalizing interpolated normals.** The most common Phong shading bug. Normals lose their unit-length property after interpolation. Add `normalize()` in the fragment shader.

**Using model-space normals without transforming them.** Normals must be in the same space as your light positions and vertex positions (usually world space). Use the inverse transpose of the model matrix, not the model matrix itself, to transform normals (see L1D for the derivation).

**High shininess on flat surfaces.** Large n (n > 128) combined with very flat geometry can make specular highlights appear only on a single fragment, flickering. Either reduce n or increase polygon density around the highlight area.

**Ambient too high in Phong.** Same pitfall as L4A. k_a * I_a washes out the diffuse gradient that makes surfaces look 3D.

---

## Industry Context

**Games (2000–2010):** Blinn-Phong per-fragment in the fragment shader was the state of the art. Unreal Engine 3 used Blinn-Phong with additional environment maps for reflections.

**Games (2010–present):** PBR replaced Blinn-Phong in AAA titles starting around 2013. Unreal Engine 4 shipped with a full PBR material system. Unity 5's Standard Shader is PBR. The exact BRDF varies by engine.

**Mobile/embedded (2026):** Blinn-Phong still dominates because PBR is too expensive for low-power hardware. Mobile games running on Qualcomm Adreno or Apple A-series chips often use simplified Blinn-Phong or Lambert with specular maps.

**DCC tools (Maya, Blender):** Viewport display uses Phong/Blinn-Phong for interactive feedback. Final render (Cycles, Arnold) uses full path-traced BRDFs.

**Film:** Phong and Blinn-Phong are legacy models not used in production for 20+ years. Every major renderer uses physically based BRDFs. However, technical artists still think in terms of "diffuse color", "specular intensity", and "roughness" — concepts that descend directly from the Phong decomposition.

Next: L4C covers why Blinn-Phong fails physically, what PBR replaces it with, and how this whole chain of shading models maps to where shading lives in a modern rendering pipeline.
