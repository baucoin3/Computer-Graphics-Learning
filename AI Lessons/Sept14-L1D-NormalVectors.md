# Normal Vectors — From Linear Algebra First Principles to Every Major Use in Graphics

**CPS 511 / Self-Study Module — Brendan Aucoin**
**Date: September 16, 2026**

---

## Preface

Before you write a single lighting equation, before you think about shaders, before you touch a GPU — you need to understand what a normal vector actually *is*. Not just "it points away from the surface." That answer gets you through the first homework and fails you on everything after it.

A normal vector is a fundamental geometric tool. It is used in lighting, culling, collision response, reflection, ambient occlusion, clipping, and shadow computation. It transforms differently from positions and most people get that wrong in production code. By the end of this note, you will understand normals deeply enough to use them correctly in every context you encounter them.

We start from linear algebra basics. No assumed recall from university — just build it up from scratch.

---

## 1. What Is a Normal Vector? (From Linear Algebra)

Start with something concrete: a flat floor.

```
        ^ n (normal — pointing up)
        |
        |
========+==============================  <-- the floor (a plane in 3D)
        |
    --> v1 (east)   --> v2 (north)   --> v3 (northeast)
```

Every vector that lies *in* the floor — east, north, northeast, any direction along the floor — is said to lie *in the plane*. The normal vector `n` is the vector that is **perpendicular** to all of them simultaneously.

From linear algebra: a vector `n` is normal to a plane P if and only if:

```
n · v = 0    for EVERY vector v that lies in P
```

Why? The dot product of two vectors is:

```
n · v = |n| |v| cos(θ)
```

When two vectors are perpendicular, `θ = 90°`, and `cos(90°) = 0`. So the dot product is zero. Conversely, if `n · v = 0` for every vector in the plane, then `n` is perpendicular to every direction in that plane — which is exactly the definition of a normal.

Let us verify with the floor example. Let `n = (0, 1, 0)` (pointing straight up). Let:
- `v1 = (1, 0, 0)` (east, lies in the floor)
- `v2 = (0, 0, 1)` (north, lies in the floor)
- `v3 = (1, 0, 1)` (northeast, lies in the floor)

Compute:
```
n · v1 = (0)(1) + (1)(0) + (0)(0) = 0  ✓
n · v2 = (0)(0) + (1)(0) + (0)(1) = 0  ✓
n · v3 = (0)(1) + (1)(0) + (0)(1) = 0  ✓
```

Every vector lying in the floor is perpendicular to `n`. The dot products are all zero. This is not a coincidence — it is the definition working exactly as expected.

**The key insight**: a plane in 3D is completely described by a normal vector (its orientation) plus one point on the plane (its position). The normal captures *which way* the plane faces. This is why normals appear everywhere in graphics — surfaces and planes are everywhere, and their orientation is critical information.

---

## 2. How to Compute a Surface Normal (Triangle)

In 3D graphics, all geometry is ultimately triangles. Given a triangle, how do you find its normal?

You have three vertices: A, B, C. Two vectors lie in the triangle's plane — any two edges will do.

**Step 1: Compute two edge vectors.**

```
e1 = B - A
e2 = C - A
```

Both `e1` and `e2` lie in the triangle's plane by construction (they connect vertices within the triangle).

**Step 2: Take the cross product.**

```
n = e1 × e2
```

Why does the cross product give the normal? The cross product of two vectors produces a vector that is perpendicular to *both* input vectors. Since `e1` and `e2` both lie in the triangle's plane, their cross product is perpendicular to both — and therefore perpendicular to the entire plane. That perpendicular direction is exactly the surface normal.

The cross product formula, expanded component by component:

```
If e1 = (e1x, e1y, e1z)  and  e2 = (e2x, e2y, e2z), then:

n = e1 × e2 = ( e1y*e2z - e1z*e2y,
                e1z*e2x - e1x*e2z,
                e1x*e2y - e1y*e2x )
```

A mnemonic: write it as a 3×3 determinant with unit vectors i, j, k in the first row.

---

### Worked Example 1 — Triangle in the XY Plane

Vertices: `A = (0,0,0)`, `B = (1,0,0)`, `C = (0,1,0)`

```
C (0,1,0)
|  \
|    \
|      \
A------B  (in the XY plane, Z=0)
(0,0,0) (1,0,0)
```

Edge vectors:
```
e1 = B - A = (1,0,0) - (0,0,0) = (1, 0, 0)
e2 = C - A = (0,1,0) - (0,0,0) = (0, 1, 0)
```

Cross product:
```
n = e1 × e2 = ( (0)(0) - (0)(1),   -->  0 - 0  = 0
                (0)(0) - (1)(0),   -->  0 - 0  = 0
                (1)(1) - (0)(0) )  -->  1 - 0  = 1

n = (0, 0, 1)
```

Result: `+Z`. The triangle lies in the XY plane, so the normal pointing straight up in Z is exactly right. The geometry confirms the algebra.

---

### Worked Example 2 — Tilted Triangle

Vertices: `A = (1,0,0)`, `B = (0,1,0)`, `C = (0,0,1)`

This triangle is the cross-section of a cube corner — it cuts diagonally across all three axes.

```
      C (0,0,1)
     /|
    / |
   /  |
  A------B
(1,0,0) (0,1,0)
```

Edge vectors:
```
e1 = B - A = (0,1,0) - (1,0,0) = (-1, 1, 0)
e2 = C - A = (0,0,1) - (1,0,0) = (-1, 0, 1)
```

Cross product, step by step:
```
nx = e1y*e2z - e1z*e2y = (1)(1) - (0)(0) = 1
ny = e1z*e2x - e1x*e2z = (0)(-1) - (-1)(1) = 0 + 1 = 1
nz = e1x*e2y - e1y*e2x = (-1)(0) - (1)(-1) = 0 + 1 = 1

n = (1, 1, 1)
```

This makes perfect sense. The triangle cuts equally across X, Y, and Z, so the normal points equally in all three positive directions — `(1,1,1)`, which when normalized is `(1/√3, 1/√3, 1/√3)`.

---

### Winding Order — Which Way Does the Normal Point?

The order you list A, B, C determines which direction the normal points. This is called *winding order*.

OpenGL convention: **counter-clockwise winding** (when viewed from outside the object) produces an **outward-facing normal**. This is not arbitrary — it is by convention, and the entire culling system depends on it.

Use the **right-hand rule**: point your fingers in the direction of `e1`, curl them toward `e2`, and your thumb points in the direction of `n = e1 × e2`.

```
         n (thumb points up)
         ^
         |
    e2   |
   <-----+
         |
         +-----> e1
         
Right hand: fingers point along e1, curl toward e2, thumb = normal.
```

If you reverse B and C (swapping winding order), `e1` and `e2` swap, and `n` flips sign. This produces a normal pointing *inward* instead of outward. A very common bug: if your lighting looks inverted (bright where it should be dark), check winding order.

---

## 3. Unit Normals — Why Length Matters

The raw cross product `n = e1 × e2` has magnitude equal to the *area of the parallelogram* formed by `e1` and `e2`:

```
|n| = |e1 × e2| = |e1| |e2| sin(θ)
```

This is not 1 in general. A large triangle produces a long normal. A tiny triangle produces a short one.

For all lighting calculations, you **must** normalize: `n̂ = n / |n|`

**Why this is not optional**: the dot product formula is:
```
n · l = |n| |l| cos(θ)
```

If `|n| ≠ 1` and `|l| ≠ 1`, then `n · l` is *not* the cosine of the angle — it is scaled by both magnitudes. Your lighting would be brighter on large triangles (bigger normal magnitude) and dimmer on small ones. The geometry of the mesh would corrupt the lighting. You would get nonsense.

Normalization is how you extract just the angular information, stripping out the magnitude:
```
n̂ · l̂ = cos(θ)    (only valid when both vectors are unit length)
```

In GLSL, always normalize in the shader:

```glsl
vec3 N = normalize(normal);   // do this in the fragment shader
vec3 L = normalize(lightDir);
float diff = max(dot(N, L), 0.0);
```

**Critical subtlety**: even if you normalize normals in C++ when building the mesh, once the vertex shader outputs them and the rasterizer interpolates them across the triangle surface, the interpolated normals are *no longer unit length*. Interpolating two unit vectors produces a shorter vector. You must renormalize in the fragment shader. Always. This is why `glEnable(GL_NORMALIZE)` existed in legacy OpenGL — it handled this automatically. In modern shader code, it is your responsibility.

---

## 4. The Dot Product as an Angle Probe

This is the single most important equation in real-time rendering:

```
n̂ · l̂ = cos(θ)      where θ is the angle between them
```

This works because both vectors are unit length. Let us walk through what this value means at key angles:

| θ (angle between normal and light) | cos(θ) | Physical meaning |
|---|---|---|
| 0° | 1.0 | Light hits surface dead-on. Maximum brightness. |
| 45° | ≈ 0.707 | Light at 45°. 70.7% brightness. |
| 60° | 0.5 | Light at 60°. Half brightness. |
| 90° | 0.0 | Light grazes the surface. Zero contribution. |
| > 90° | negative | Light is behind the surface. Clamp to 0. |

```
Light source (L)
      \         |  <- normal n
       \        |
        \   45  |
         \ deg  |
          \     |
           \    |
            \   |
             [surface]

At 45°: dot product = cos(45°) ≈ 0.707
Surface receives about 70% of maximum possible light.
```

```
Light source (L)           Light source (L)
      |                         \
      |  <- normal n             \      <- normal n (still points up)
      |                           \
      |                            \
  [surface]                     [surface]  <- tilted surface intercepts less light
  (perpendicular)                          cos(θ) is smaller
```

This is **Lambert's cosine law** — a physical law, not a hack. It says diffuse surface brightness is proportional to `cos(θ)`. The reason is geometric: a light beam of fixed width covers more surface area when hitting at an angle, so each unit of surface receives less light. The math and the physics match perfectly.

```
                  ^   ^   ^   ^        <- parallel light rays
                  |   |   |   |
                  |   |   |   |
    Width W: [====|===|===|===|====]   <- surface perpendicular to light: W wide
    
                  ^   ^   ^   ^
                   \   \   \   \
                    \   \   \   \
    Width W: [=======|===|===|===|===]  <- tilted surface: same W of light, more surface
                     <----- longer ---->
```

In the tilted case, the same "column" of light covers more surface — less intensity per unit area. That is exactly what `cos(θ)` captures. The dot product is not just a formula; it is a direct measurement of this geometric effect.

In GLSL:

```glsl
float diff = max(dot(N, L), 0.0);
// The max() clamps negative values (light behind surface) to zero.
// Without it, back-lit surfaces add negative light, which is physically wrong.
```

---

## 5. Every Place Normals Appear in Graphics

Normals are not "a lighting thing." They are a geometric measurement of surface orientation, and surface orientation matters in an enormous number of computations. Here is every major place you will use them.

---

### 5.1 Diffuse Lighting (Lambertian Reflectance)

The Phong model's diffuse term:

```
I_diffuse = k_d * L_d * max(n̂ · l̂, 0)
```

`n̂` is the surface normal (triangle/vertex normal, in eye space). `l̂` is the unit vector from the surface point toward the light source. The dot product gives Lambert's cosine. `k_d` scales by the material's diffuse color, `L_d` by the light intensity.

The normal tells the equation how the surface is oriented relative to the light. That is why diffuse lighting changes when you rotate an object but not when you move it (moving doesn't change the surface's orientation relative to the light direction).

---

### 5.2 Specular Lighting (Blinn-Phong)

The Blinn-Phong specular term:

```
I_specular = k_s * L_s * max(n̂ · ĥ, 0)^alpha
```

Where `ĥ = normalize(l̂ + v̂)` is the halfway vector between the light direction and the view direction.

The surface normal determines whether the halfway vector aligns with the normal — i.e., whether you are looking at the surface from the angle that would produce a specular highlight. When `n̂ · ĥ ≈ 1`, the surface is oriented to produce a strong highlight toward your eye. The normal is the reference: *am I oriented to send a specular spike toward the viewer?*

---

### 5.3 Backface Culling

Before the GPU rasterizes a triangle, it can check: does this triangle face the camera? If not, skip it.

```
Compute: n̂ · v̂

Where n̂ is the face normal, and v̂ is the unit vector from the triangle to the camera.

If n̂ · v̂ > 0  →  normal points toward camera  →  front face  →  draw it
If n̂ · v̂ < 0  →  normal points away from camera  →  back face  →  cull it
```

```
      Camera
        |
        v̂ (direction from triangle toward camera)
        |
      [face, n̂ pointing toward camera]   --> draw
      
      Camera
        |
        |
      [face, n̂ pointing AWAY from camera]   --> cull
```

For a closed solid (like a sphere or box), you never see the inside of faces. Backface culling eliminates approximately 50% of triangles from being rasterized — a free 2x speedup. Enabled in OpenGL with `glEnable(GL_CULL_FACE)`.

The calculation is done in clip/eye space. The entire mechanism is just a dot product with the surface normal.

---

### 5.4 Shadow Volumes

Shadow volumes (Crow 1977) work by extruding silhouette edges away from the light to form shadow geometry. The silhouette edges are where the shadow boundary is. Finding silhouette edges requires normals.

For each edge shared by two triangles (call them face A and face B):
- Compute `n̂_A · l̂` and `n̂_B · l̂` where `l̂` is the direction from the surface toward the light.
- Face A is front-facing (toward light) if `n̂_A · l̂ > 0`, back-facing if `< 0`.
- The edge is a silhouette edge if one face is front-facing and the other is back-facing.

```
Light
  \
   \     [face A: n̂_A · l̂ > 0, faces light]
    \  /
     \/--- silhouette edge (one face front, one back)
     /\
    /  \  [face B: n̂_B · l̂ < 0, faces away from light]
```

The normals define the light/shadow boundary. Without normals, you cannot find the silhouette.

---

### 5.5 Clipping Plane Tests

The view frustum is bounded by six clipping planes (near, far, left, right, top, bottom). Each clip plane is defined by a normal `n̂` (pointing inward, into the frustum) and a signed distance `d`.

To test whether a point `p` is on the inside (visible side) of a clip plane:

```
If (p - d) · n̂ > 0  →  inside, keep
If (p - d) · n̂ < 0  →  outside, clip
```

The Cohen-Sutherland algorithm (line clipping) assigns each vertex an outcode by testing it against all four (2D) or six (3D) clip planes. Each test is exactly this dot product against the clip plane normal. The entire clipping pipeline is built on normals — of the clip planes, not the triangles.

---

### 5.6 Collision Detection and Response — Sphere vs. Plane

A plane is defined by a normal `n̂` and a signed distance `d` from the origin. A sphere has center `c` and radius `r`.

**Detection**: the signed distance from the sphere center to the plane is:
```
dist = c · n̂ - d
```
If `dist < r`, the sphere has penetrated the plane. Penetration depth = `r - dist`.

**Response (bounce)**: if the sphere's velocity is `v`, the reflected velocity off the plane is:
```
v_reflected = v - 2(v · n̂) n̂
```

This formula decomposes `v` into a component along `n̂` (perpendicular to the surface) and a component parallel to the surface. It flips the perpendicular component and keeps the parallel component — which is exactly what a perfect elastic bounce does.

The surface normal `n̂` is the fundamental piece. It tells you what "perpendicular to the surface" means so you can compute both penetration depth and bounce direction.

---

### 5.7 Environment / Reflection Mapping

For a shiny surface that reflects the environment, you need to know which direction the surface is reflecting toward. Given a view ray direction `v̂` (from camera toward surface) and surface normal `n̂`:

```
r̂ = v̂ - 2(v̂ · n̂) n̂
```

This reflection formula is the same structure as the bounce formula — decompose, flip the normal component. The result `r̂` is the direction that a perfectly mirrored surface would reflect toward.

You then use `r̂` to look up a cubemap or sphere map:

```glsl
vec3 reflectDir = reflect(viewDir, N);
vec4 envColor = texture(environmentCubemap, reflectDir);
```

The `reflect()` GLSL built-in does exactly `v - 2*(dot(v,n))*n`. The surface normal determines which part of the environment the surface reflects toward the viewer. Rotate the object, the normal rotates, the reflection direction changes — you see a different part of the environment. All driven by the normal.

---

### 5.8 Bump Mapping and Normal Mapping

The geometric triangle normal is flat — the same across the entire triangle. But real surfaces have microscopic bumps and wrinkles. You can fake this with texture maps that encode normals per fragment.

**Normal mapping**: store pre-computed perturbed normals in an RGB texture. Each texel stores `(nx, ny, nz)` encoded in the [0,1] range (shift and scale from [-1,1]). In the fragment shader, you read the texture and decode:

```glsl
vec3 N = texture(normalMap, uv).rgb;
N = normalize(N * 2.0 - 1.0);   // decode from [0,1] to [-1,1]
// Use N in lighting calculation instead of the geometric normal
```

The geometric normal says "flat surface." The normal map says "this fragment on the flat surface has a bumpy normal that points in this specific perturbed direction." The lighting equation sees the bumpy normal and produces bumpy lighting — creating the illusion of surface detail without a single extra triangle.

This technique is in every modern real-time game. The normals in the texture represent the same physical quantity as the geometric normals, just evaluated at much higher resolution than the mesh allows.

---

### 5.9 Refraction (Snell's Law)

When light passes from one medium to another (air to glass, air to water), it bends. The direction of bending depends on the surface normal and the ratio of indices of refraction `n1/n2`.

GLSL has a built-in:

```glsl
vec3 refractDir = refract(viewRay, surfaceNormal, n1 / n2);
```

Internally this applies Snell's law in vector form. The surface normal is the reference axis — the normal tells you which direction is "perpendicular to the interface," which is the axis about which the ray bends.

Without the correct surface normal, you cannot compute the refracted ray direction. The normal defines the geometry of the interface.

---

### 5.10 Ambient Occlusion

Ambient occlusion asks: how much of the ambient (indirect) light reaches this surface point? A point in a crevice is blocked from light in most directions; an exposed point on a hilltop is blocked in few directions.

To compute this for a surface point P with normal `n̂`:
1. Generate many random ray directions in the *hemisphere centered on `n̂`* (the hemisphere on the side the surface faces).
2. For each ray, test whether it hits nearby geometry.
3. Count the fraction of rays that are blocked.
4. More blocked = more occluded = darken that point.

```
          hemisphere of rays
        . . | . .
      .     |     .
    .      n̂      .
           |
     [surface point P]
```

The hemisphere is defined by `n̂`. If `n̂` points up, you sample rays in the upper hemisphere. Every sample direction `s̃` must satisfy `s̃ · n̂ > 0` (it must be on the same side as the normal, not below the surface). Without `n̂`, you do not know which hemisphere to sample — half your rays would go into the surface, which is physically meaningless.

Ambient occlusion gives realistic darkening in corners, crevices, and under objects — the "contact shadows" effect that makes CG feel grounded. All of it hinges on the surface normal.

---

## 6. Why Normals Transform Differently from Positions

This is the most commonly misunderstood part of normals in graphics programming, and getting it wrong will cause subtle, hard-to-debug rendering artifacts.

### The Problem

Positions transform by the model matrix `M`:
```
p_world = M * p_model
```

Naive assumption: maybe normals transform the same way? Let us test this assumption.

**Setup**: a flat surface in the XZ plane. The surface normal is `n = (0, 1, 0)` (pointing straight up). The surface passes through the origin.

Apply a non-uniform scale: `S = scale(2, 1, 1)` — stretch the surface horizontally (X) by 2, leave Y and Z unchanged.

```
Before scaling:
  n = (0,1,0) pointing up
  
          n
          ^
          |
==========|==========   <-- flat XZ surface, unscaled
```

After the scaling, the surface stretches in X. Intuitively, the normal should still point straight up — the surface is still horizontal.

If we apply M to the normal: `M * (0,1,0) = (0,1,0)`. Fine here. The normal happens to be correct.

Now try a **tilted surface**. Let the surface normal be `n = normalize(1, 1, 0)` — pointing diagonally up and to the right (the surface tilts 45° around the Z axis).

Apply the same `S = scale(2, 1, 1)`:

Under `S`, the normal becomes: `S * (1,1,0) = (2, 1, 0)`.

Is `(2,1,0)` still perpendicular to the stretched surface? Let us check.

The surface had tangent `t = (1,-1,0)` (along the surface, perpendicular to the original normal). After scaling, the tangent becomes `S * (1,-1,0) = (2,-1,0)`.

Check: is the transformed "normal" `(2,1,0)` perpendicular to the transformed tangent `(2,-1,0)`?

```
(2,1,0) · (2,-1,0) = 4 - 1 + 0 = 3  ≠ 0
```

The dot product is not zero. The "normal" is no longer perpendicular to the surface. It is wrong.

```
Before:               After naive transform (WRONG):
 n=(1,1,0)              n'=(2,1,0)
  ^                       ^
  |   /surface             |    /surface (stretched in X)
  |  /                     |   /
  | /                      |  /
  |/                       | /
  +----                    +----
  
The surface stretched sideways. n' should point more "up" now
to remain perpendicular. But naive scaling pushed it more "right."
Result: n' is NOT perpendicular to the stretched surface.
```

---

### The Fix: Inverse Transpose

The correct transformation for normals is: **the inverse transpose of the upper 3×3 of the model matrix**.

```
n_world = (M^{-T}) * n_model
```

Where `M^{-T}` means: take the inverse of M, then transpose it (or equivalently, transpose M then invert — same result).

**Why?** Here is the derivation, step by step.

Let `T` be any tangent vector on the surface, and `N` be the normal. By definition:

```
N · T = 0    (N is perpendicular to T)
```

After transformation, the tangent becomes `M*T` (positions use M). We need to find some matrix X such that the transformed normal `X*N` is still perpendicular to `M*T`:

```
(X*N) · (M*T) = 0
```

Rewrite using the dot product as a matrix product:

```
(X*N)^T * (M*T) = 0
N^T * X^T * M * T = 0
```

We already know `N^T * T = 0` (original perpendicularity). So we need:

```
X^T * M = I    (identity)
```

This means `X^T = M^{-1}`, so `X = (M^{-1})^T = M^{-T}`. QED.

Normals must be transformed by the inverse transpose of M to remain perpendicular to the transformed surface.

In GLSL:

```glsl
// Modern core profile — compute yourself:
mat3 normalMatrix = transpose(inverse(mat3(model)));
vec3 N_world = normalize(normalMatrix * N_model);

// Legacy built-in (OpenGL computed this for you):
vec3 N_eye = normalize(gl_NormalMatrix * gl_Normal);
// gl_NormalMatrix was the inverse transpose of the upper 3x3 of ModelView
```

---

### When You Can Skip It

If your model matrix contains only **rotations and uniform scale** (same scale factor in all three axes), you can use the upper 3×3 of M directly. Here is why:

- Rotation matrices are orthonormal: `R^{-1} = R^T`, so `R^{-T} = (R^T)^T = R`. The inverse transpose of a rotation is the rotation itself. No need to invert.
- Translation does not affect normals — normals are directions (vectors with `w=0`), not positions. Translation has no effect on directions.
- Uniform scale `sI`: inverse is `(1/s)I`, transpose of that is `(1/s)I`. Applied to a normal, this just scales it — and you normalize anyway, so the magnitude does not matter.

If you are only doing rotation + translation + uniform scale, the upper 3×3 of M is fine. The moment you introduce **non-uniform scale** (different scale factors on X, Y, Z), or **shear**, you must use the inverse transpose.

A very common pipeline mistake: developer transforms normals with M directly for performance, everything looks fine in tests (because test scene only uses uniform scale), ships, and then a designer stretches an object non-uniformly and lighting breaks. The fix is always the inverse transpose.

---

## 7. Face Normals vs. Vertex Normals

The same mesh can look faceted or smooth depending on whether you use **face normals** or **vertex normals**.

### Face Normals (Flat Shading)

One normal per triangle, computed from the face's edges. Every fragment in that triangle uses the same normal. The lighting calculation returns the same value across the whole face.

Result: a faceted, low-poly look. Each face is uniformly lit. The edges between faces are clearly visible because the normal — and thus the lighting — jumps abruptly at the boundary.

```
Sphere with face normals (flat shading):

  ___________
 / \ | / \ /|
/   \|/   X |    <- each triangular facet has a single
|  flat \   |       uniform color. Looks like a gem.
|   face |  |
 \___|__/|_/
```

### Vertex Normals (Smooth Shading)

One normal per vertex, typically computed as a weighted average of the normals of all triangles that share that vertex. The vertex shader receives this averaged normal. The rasterizer interpolates the normals smoothly across each triangle. The fragment shader sees a continuously varying normal and computes lighting per fragment.

Result: the surface looks smooth even though the underlying geometry is the same low-poly mesh. The lighting transitions smoothly across face boundaries because the normals transition smoothly.

**Computing vertex normals**: for each vertex V, collect all triangles that share V:

```
n_vertex = normalize( sum of (face_normal_i * weight_i) for each adjacent face i )
```

Common weighting strategies:
- **Area weighting**: weight each face's normal by the face's area (larger faces contribute more). The cross product magnitude already encodes area, so summing unnormalized face normals before the final normalize achieves this naturally.
- **Angle weighting**: weight by the interior angle of the face at that vertex. More physically correct — a large triangle that only barely touches V should not dominate V's normal.

Worked concept: a 4-triangle mesh around a vertex. Each face has a normal that points in a slightly different direction. The vertex normal is the average. A sphere mesh has many vertices, each with an averaged normal that points radially outward — even though the geometry is a polygon mesh, the normals produce smooth spherical lighting.

```
Sphere with vertex normals (smooth shading):

     .---.
   .'     '.        <- looks round! Normals point radially outward at each vertex.
  /         \          The rasterizer interpolates them across each triangle.
 |     O     |         Lighting appears to curve with the surface.
  \         /
   '.     .'
     '---'
```

**The underlying geometry is identical in both cases.** The only difference is the normals. This illustrates the central role of normals in appearance: the normal is the interface between geometry and lighting. You can have the same geometry produce completely different visual results by changing the normals.

### Industry Context

In Blender, Maya, and 3ds Max: right-clicking a mesh and choosing "Shade Smooth" or "Shade Flat" is toggling between vertex normals and face normals. No geometry changes. The software computes vertex normals automatically when you switch to smooth shading.

The **normal map workflow** in games takes this further. Artists sculpt an extremely detailed high-poly mesh (millions of triangles). They then bake — compute, for every point on the low-poly mesh's surface, what the high-poly mesh's normal is at that point — and store those normals in a texture. The low-poly mesh (thousands of triangles) is shipped in the game. The fragment shader samples the normal map per fragment and uses those high-poly normals in the lighting calculation. The player sees the visual complexity of the high-poly sculpt on geometry with the performance cost of the low-poly mesh.

The normal map is literally a texture full of normals. The entire technique exists because normals are the primary way geometry influences lighting — if you can fake the normals, you can fake the geometry's appearance.

---

## Closing Thoughts

Work through the derivation in Section 6 until you can reproduce it from memory. That derivation — that normals must be transformed by the inverse transpose to remain perpendicular — is a question you will be asked in technical interviews, and more importantly, it is a bug you will hit if you do not understand it.

The conceptual shift to internalize: a normal vector is not a lighting accessory. It is a mathematical description of surface orientation — the result of a cross product of two edge vectors, perpendicular to the surface by construction, used in every system that needs to know *which way a surface faces*.

Lighting, culling, clipping, collision, reflection, refraction, occlusion — every one of these needs to know which way the surface faces. That is what the normal tells you. Everything else is just the dot product doing its job.

---

*Normal Vectors — Sept 2026 | CG Self-Study Workspace | Brendan Aucoin*
