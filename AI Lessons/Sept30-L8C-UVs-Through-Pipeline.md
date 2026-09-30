# L8C — UVs Through the Pipeline
**Theme A1: UV Mapping + Texture Fundamentals | Stage 1 of 5**

---

## The Problem Without This Concept

UVs are assigned to vertices. But fragments (screen pixels covered by a triangle) are not vertices — there are potentially millions of fragments generated from 3 vertices. The GPU must somehow give each fragment its own UV coordinate so the fragment shader can sample the correct part of the texture.

How does a UV value "travel" from a vertex all the way to a fragment shader? What does the rasterizer do with it? And why does naive interpolation produce wrong results for geometry viewed at an angle?

This lesson traces the complete journey: VBO → vertex shader → rasterizer → fragment shader.

---

## Part A — Theory

### UV as a Vertex Attribute in the VBO

In the VBO, UV coordinates live alongside position and normal as an **interleaved attribute**. A common layout is:

```
[x  y  z  |  nx  ny  nz  |  u  v]  per vertex
```

Each group is one vertex's data. The total size per vertex is 8 floats = 8 × 4 bytes = 32 bytes. This is the **stride** — how many bytes to advance to get to the next vertex's data.

The UV attribute starts at byte offset 24 (after 6 floats × 4 bytes = 24 bytes for position + normal).

The `glVertexAttribPointer` call for UV:

```c
// Assuming attribute location 2 for UV
glVertexAttribPointer(
    2,              // attribute location (matches layout(location=2) in vertex shader)
    2,              // 2 components (u, v)
    GL_FLOAT,       // type
    GL_FALSE,       // not normalized
    32,             // stride in bytes: 8 floats × 4 bytes
    (void*)24       // offset in bytes: 6 floats × 4 bytes
);
glEnableVertexAttribArray(2);
```

Getting stride and offset wrong is the single most common VBO bug. The result is garbled attribute data — the GPU reads the wrong bytes as UV coordinates.

### The Vertex Shader Receives UV and Passes It Forward

In the vertex shader:

```glsl
#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;   // UV attribute from VBO

out vec2 vTexCoord;   // will be interpolated across the triangle

uniform mat4 uMVP;

void main() {
    gl_Position = uMVP * vec4(aPosition, 1.0);
    vTexCoord = aTexCoord;   // pass UV to rasterizer for interpolation
}
```

The `out vec2 vTexCoord` is a **varying variable** — a signal that the rasterizer will interpolate between the three vertex values and deliver a per-fragment value to the fragment shader.

You can optionally transform the UV here: `vTexCoord = aTexCoord * uTilingFactor + uOffset;`. This is common for animated texture scrolling or atlas sub-region selection.

### What the Rasterizer Does: Barycentric Interpolation

The rasterizer takes each triangle's three output vertices and determines which screen pixels are covered by the triangle. For each covered pixel, it generates a **fragment** and assigns interpolated values to all `out` variables from the vertex shader.

The interpolation is **barycentric**. For a point P inside triangle (A, B, C):

```
P = λ_A · A + λ_B · B + λ_C · C
```

where λ_A, λ_B, λ_C are the **barycentric coordinates** — three non-negative weights that sum to 1:

```
λ_A + λ_B + λ_C = 1
λ_A ≥ 0,  λ_B ≥ 0,  λ_C ≥ 0
```

λ_A is 1 at vertex A and decreases to 0 at the opposite edge BC. λ_B is 1 at B, 0 on edge AC. λ_C is 1 at C, 0 on edge AB.

The UV at a fragment is:

```
UV_fragment = λ_A · UV_A + λ_B · UV_B + λ_C · UV_C
```

This is what the rasterizer computes for every fragment inside the triangle.

### The Perspective Problem: Why Naive Interpolation Is Wrong

Here is a subtle but critical issue. The rasterizer works in **screen space** — the 2D image coordinates. Barycentric coordinates computed in screen space are based on the projected (perspective-divided) vertex positions, not the original 3D positions.

For a triangle viewed straight-on (camera looking perpendicular to the triangle), screen-space barycentric interpolation gives correct UV values. The texture appears correct.

For a triangle viewed at an angle — especially geometry that recedes into the distance (a floor, a road, a wall corner) — the screen-space midpoint of the triangle does NOT correspond to the 3D midpoint of the triangle. The further vertex is compressed by perspective projection.

**Example**: three colinear points in 3D at depths z = 2, z = 2, z = 4 (two points at the near depth, one far). After perspective division (divide by z), the far point projects to half its original screen position. The screen-space centroid of the three projected points is shifted toward the near points. If you naively interpolate UV based on screen-space barycentric coordinates, the UV at the "center" fragment corresponds to the center of the projected triangle — which is NOT the center of the 3D triangle. The texture slides and kinks at oblique angles.

This artifact is visible as a "tent fold" or diagonal crease in the texture when a quad or large triangle is viewed at a steep angle.

### Perspective-Correct Interpolation

The fix is mathematically elegant. Instead of interpolating UV linearly in screen space, the GPU interpolates:

```
u/w,  v/w,  1/w
```

linearly in screen space (where w is the clip-space w coordinate, which is the pre-division depth). Then, for each fragment, it recovers the correct UV by dividing:

```
u = (u/w interpolated) / (1/w interpolated)
v = (v/w interpolated) / (1/w interpolated)
```

Why does this work? Division by w "undoes" the perspective projection for the interpolation weights. This is equivalent to interpolating in 3D space before projecting, which gives the geometrically correct result.

Modern OpenGL (core profile 3.3+) performs perspective-correct interpolation **automatically** for all `out` variables from the vertex shader. You do not write any code for this — it just works correctly.

In legacy OpenGL fixed-function pipeline, `glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST)` enabled perspective-correct interpolation. `GL_FASTEST` used affine (incorrect) interpolation. The default was implementation-defined — on some old hardware, textures visibly warped at angles because the driver chose the fast path.

In modern GLSL, you can explicitly disable perspective correction with the `noperspective` qualifier:

```glsl
noperspective out vec2 vTexCoord;  // affine (screen-space linear) interpolation
```

`noperspective` is correct for screen-space effects (motion blur vectors, SSAO sample positions) where you intentionally want screen-space interpolation. It is wrong for surface texture UVs.

### The Fragment Shader Receives UV and Samples the Texture

In the fragment shader:

```glsl
#version 330 core

in vec2 vTexCoord;           // perspective-correct UV from rasterizer

out vec4 fragColor;

uniform sampler2D uAlbedo;   // texture unit to sample

void main() {
    vec4 color = texture(uAlbedo, vTexCoord);
    fragColor = color;
}
```

`texture(sampler, uv)` is the standard GLSL 3.30 texture sampling function. It automatically:
- Applies the active filter mode (nearest, bilinear, trilinear)
- Applies the wrap mode for UVs outside [0,1]
- Selects the appropriate mip level (if mipmaps are enabled)

**Binding a texture to a sampler uniform**: a `sampler2D` uniform represents a texture unit slot (0–15 typically). You bind the texture to the slot and tell the shader which slot:

```c
glActiveTexture(GL_TEXTURE0);         // activate texture unit 0
glBindTexture(GL_TEXTURE_2D, texID);  // bind texture to unit 0

GLint loc = glGetUniformLocation(program, "uAlbedo");
glUniform1i(loc, 0);   // tell sampler "use texture unit 0"
```

### Complete Minimal Example: Textured Quad

**Vertex shader:**

```glsl
#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 2) in vec2 aTexCoord;

out vec2 vTexCoord;

uniform mat4 uMVP;

void main() {
    gl_Position = uMVP * vec4(aPosition, 1.0);
    vTexCoord = aTexCoord;
}
```

**Fragment shader:**

```glsl
#version 330 core

in vec2 vTexCoord;
out vec4 fragColor;

uniform sampler2D uAlbedo;

void main() {
    fragColor = texture(uAlbedo, vTexCoord);
}
```

**CPU setup (relevant parts):**

```c
// VBO: position (3 floats) + UV (2 floats) per vertex, stride = 20 bytes
float vertices[] = {
//  x      y     z     u     v
   -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
    0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
    0.5f,  0.5f, 0.0f, 1.0f, 1.0f,
   -0.5f,  0.5f, 0.0f, 0.0f, 1.0f,
};

// Position attribute
glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 20, (void*)0);
glEnableVertexAttribArray(0);

// UV attribute — offset 12 bytes (3 floats × 4 bytes)
glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 20, (void*)12);
glEnableVertexAttribArray(2);
```

---

## Part B — Numeric Worked Example

### Why Affine Interpolation Fails

Triangle with three vertices (simplified 2D perspective for clarity):

- V0: 3D position (-1, 0, -2), UV = (0.0, 0.5)  → projected screen x = -1/-2 = 0.5, so screen pos = (-0.5, 0)
- V1: 3D position ( 1, 0, -2), UV = (1.0, 0.5)  → projected screen x = 1/-2 = -0.5, so screen pos = (0.5, 0)
- V2: 3D position ( 0, 0, -4), UV = (0.5, 0.5)  → projected screen x = 0/-4 = 0, so screen pos = (0, 0)

(Using pinhole camera: screen_x = world_x / (-world_z). All y = 0 for simplicity, looking at UV.x only.)

The true 3D midpoint of the triangle edge V0–V2 (ignoring V1) is at 3D position (-0.5, 0, -3), with UV.x = 0.25.

After projection, this 3D midpoint projects to screen_x = -0.5 / -3 = 0.167.

Now look at what screen-space affine interpolation gives at screen_x = 0.167 between V0 (screen_x = -0.5) and V2 (screen_x = 0.0):

The parameter t along the projected edge = (0.167 - (-0.5)) / (0.0 - (-0.5)) = 0.667 / 0.5 = 1.334

That is t > 1, which would be outside the segment — indicating the midpoint is not at the screen-space center. The screen-space midpoint between V0 and V2 is at screen_x = (-0.5 + 0.0)/2 = -0.25, which corresponds to t = 0.5 in screen space.

Naïve affine interpolation at t = 0.5 gives UV.x = 0.0 + 0.5 × (0.5 - 0.0) = 0.25. That happens to match here — but let us check where the screen-space center ACTUALLY lies in 3D:

Screen_x = -0.25 corresponds to world_x / (-world_z) = -0.25. On the edge from V0 to V2 in 3D space: the parametric point at t = 0.5 along the 3D edge is ((-1 + 0)/2, 0, (-2 + -4)/2) = (-0.5, 0, -3). Projected: -0.5 / 3 = -0.167 (taking abs value for sign convention). That does not match screen_x = -0.25.

So the screen-space center (t=0.5 in screen space) does NOT correspond to the 3D midpoint. The correct UV at screen_x = -0.25 is NOT UV.x = 0.25 — it corresponds to the 3D point that projects there, which is a different point on the triangle with a different UV.

**Perspective-correct interpolation** recovers the correct answer by working with (u/w) quantities. It correctly maps each screen pixel to its true 3D UV. Modern OpenGL does this automatically.

---

## Pitfalls

**Pitfall 1: Wrong stride or offset in glVertexAttribPointer**

Mistake: VBO layout is position (3 floats) + UV (2 floats), stride = 20 bytes. You accidentally write stride = 12 (only counting the position) or offset = 8 (only 2 floats instead of 3).

Symptom: UV appears wildly incorrect — the texture is stretched, randomly offset, or sampled from a chaotic pattern of coordinates. In extreme cases, the GPU reads positions as UV values, and you get a "marble" effect where UV is proportional to world position.

Fix: count bytes exactly. Stride = (number of floats per vertex) × 4. Offset = (number of floats before this attribute) × 4. Write it on paper before coding it.

**Pitfall 2: Forgetting to call glEnableVertexAttribArray**

Mistake: calling glVertexAttribPointer to define the UV attribute but not calling glEnableVertexAttribArray(2) to enable it.

Symptom: UV attribute reads as (0, 0) for every vertex — the default value for disabled attributes. The fragment shader samples from UV (0,0) everywhere → solid color from the bottom-left texel.

Fix: always call `glEnableVertexAttribArray(location)` for every attribute location you define.

**Pitfall 3: Sampler2D uniform not bound to correct texture unit**

Mistake: two textures (albedo + normal map) are active on units 0 and 1. The sampler uniforms both get set to the same unit, or neither gets set.

Symptom: texture appears black (sampler2D defaults to 0 but texture was bound to unit 1), or both samplers read from the same texture.

Fix: always explicitly set every sampler uniform: `glUniform1i(glGetUniformLocation(prog, "uAlbedo"), 0)` and `glUniform1i(glGetUniformLocation(prog, "uNormal"), 1)`. Set them after linking the program, before drawing.

**Pitfall 4: Using noperspective on surface UV**

Mistake: adding `noperspective` to the `out vec2 vTexCoord` declaration in the vertex shader.

Symptom: for flat or head-on geometry, no visible difference. For geometry at an angle (a receding floor), the texture visibly kinks or tears along the diagonal where affine and perspective-correct interpolation diverge most.

Fix: remove `noperspective` from all texture coordinate varyings. Use it only for screen-space quantities that intentionally should not be perspective-corrected.

---

## What to Build

**Exercise 1: Interleaved VBO with position + normal + UV**

Write the complete VBO setup for a quad (2 triangles, 4 vertices, 6 indices) with interleaved vertex data: position (3 floats) + normal (3 floats) + UV (2 floats). Include:

1. The `vertices[]` float array for all 4 vertices (position, normal, UV values for a flat quad facing +Z at z=0)
2. The `indices[]` for the two triangles
3. All three `glVertexAttribPointer` calls with correct stride and offset
4. All three `glEnableVertexAttribArray` calls

Compute stride and offset in bytes, showing your arithmetic.

**Exercise 2: Complete textured quad program**

Write the complete vertex and fragment shader pair for a textured quad that:
- Takes MVP transform as a uniform mat4
- Takes a sampler2D uniform for the texture
- Applies a UV scale uniform (vec2 uTilingFactor) to allow tiling
- Passes perspective-correct UVs (no noperspective qualifier)
- Samples the texture and outputs the result

Include all `in`, `out`, and `uniform` declarations.

**Exercise 3: Identify the stride/offset bug**

Given this (incorrect) VBO setup code, identify every bug and explain what symptom each produces:

```c
float verts[] = {
//  x      y     z     u     v
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
};

glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, (void*)0);   // position
glEnableVertexAttribArray(0);

glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 12, (void*)12);  // UV
glEnableVertexAttribArray(2);
```

The VBO contains 5 floats per vertex (3 position + 2 UV). Find and fix all errors.
