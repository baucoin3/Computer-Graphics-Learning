# L1C — Homogeneous Coordinates: The Mathematical Foundation of Every Graphics Pipeline

**Course:** CPS 511 Bridge — Modern Computer Graphics  
**Prereqs assumed:** Linear algebra (matrices, vectors, dot product). You've seen it. You don't remember it. That's fine — we'll derive what we need.

---

## 1. The Problem That Motivated Everything

Before we define anything, let's be concrete about why a second coordinate system was invented at all.

You're writing a 3D graphics program. For every object in the scene, you need to apply three kinds of transforms:

- **Scale** — make the object bigger or smaller
- **Rotation** — spin it around some axis
- **Translation** — move it to a position in the world

Scale and rotation are linear transforms. "Linear" has a specific meaning: they can be expressed as matrix–vector multiplication. For a 3D point `(x, y, z)`, rotation by angle θ around the z-axis is:

```
[x']   [cosθ  -sinθ   0] [x]
[y'] = [sinθ   cosθ   0] [y]
[z']   [  0      0    1] [z]
```

Scale by `(sx, sy, sz)` is:

```
[x']   [sx   0   0] [x]
[y'] = [ 0  sy   0] [y]
[z']   [ 0   0  sz] [z]
```

Both are 3×3 matrix multiplications. Good. Now look at translation — moving the point by `(tx, ty, tz)`:

```
x' = x + tx
y' = y + ty
z' = z + tz
```

That is **vector addition**, not matrix multiplication. There is no 3×3 matrix `M` such that `M * [x, y, z]^T = [x+tx, y+ty, z+tz]^T` for all points. (You can verify: a 3×3 matrix acting on the zero vector always produces the zero vector. But translation of the origin should give `[tx, ty, tz]`, not `[0, 0, 0]`.)

This matters enormously in practice. In a real scene, every vertex passes through:

```
Model Transform → View Transform → Projection Transform
```

Each is a combination of translation, rotation, and scale. If some of those are matrix multiplications and some are vector additions, you cannot precompute a single combined "do everything" operation. You'd need branching logic per vertex, and you couldn't combine model + view + projection into one step.

In modern graphics, for a scene with one million vertices, the vertex shader runs once per vertex. The GPU pipeline is designed so that a single matrix multiply `gl_Position = MVP * vertex` handles the entire chain. If translation broke that, you'd need special-casing everywhere, and the whole elegant pipeline falls apart.

The fix is to add a fourth coordinate and lift everything into one dimension higher. That's homogeneous coordinates.

---

## 2. Historical Background — Where This Came From

The idea goes back to the early 19th century. August Ferdinand Möbius (1827, "Der barycentrische Calcül") introduced barycentric coordinates — a way of expressing points in terms of weighted combinations of triangle vertices. That work seeded the idea that a point can be represented by coordinates that are defined up to a common scale factor.

The broader mathematical framework — **projective geometry** — developed through the 1800s. The central observation was this:

Take two parallel railroad tracks. In Euclidean geometry, parallel lines never meet. But if you stand on the tracks and look down them, they appear to converge at a point on the horizon.

```
   \       /
    \     /
     \   /
      \ /
       V  ← "vanishing point" on the horizon
```

Euclidean geometry says this convergence point doesn't exist — the lines never actually meet. Projective geometry says: let's extend our geometry so that it does exist. Every family of parallel lines in a given direction meets at a **point at infinity** in that direction. This isn't a physical point; it's a mathematical entity added to the space to make the geometry complete.

This might sound like a curiosity. It isn't. The vanishing point in perspective drawing IS a point at infinity — it's the projective point in the direction the rails run. Perspective projection, which is what your GPU performs billions of times per frame, is built on exactly this mathematics.

Homogeneous coordinates are the algebraic tool that lets us work with projective geometry, including points at infinity, using ordinary arithmetic.

---

## 3. The Projective Plane — Points as Lines Through the Origin

Here is the core geometric insight. Pay attention — this is the "aha" that makes the rest click.

Consider a 3D space with axes labeled `X`, `Y`, `W` (we'll use `W` instead of `Z` to reserve `Z` for actual depth later). Now take the plane at `W = 1`. Every ordinary 2D Euclidean point `(x, y)` corresponds to the point `(x, y, 1)` on this plane.

```
         W
         |
         |    W=1 plane
         |   ___________
         |  |           |
         | |    (2,3,1) |
         |_|_____________
        /  |
       /   |
      X    Y

    Lines through the origin pierce the W=1 plane.
    Each line = one projective point.
```

Now draw any line from the origin through the W=1 plane. Every point on that line — say `(2, 3, 1)`, `(4, 6, 2)`, `(1, 1.5, 0.5)` — maps to the same Euclidean point. Why? Because dividing by `W`:

```
(2,  3,  1)  → (2/1,  3/1)  = (2, 3)
(4,  6,  2)  → (4/2,  6/2)  = (2, 3)
(1, 1.5, 0.5) → (1/0.5, 1.5/0.5) = (2, 3)
```

All three are different representations of the same Euclidean point `(2, 3)`. In projective geometry, the **line through the origin** is the point. The individual coordinates `(X, Y, W)` are the homogeneous coordinates of that point. They're called "homogeneous" because scaling all three by any nonzero constant `k` — `(kX, kY, kW)` — represents the same point. The coordinates are defined up to a common scale.

The Euclidean point they represent is recovered by dividing by `W`:

```
Projective point (X, Y, W)  →  Euclidean point (X/W, Y/W)    [when W ≠ 0]
```

Now, what happens when `W = 0`? The line through the origin is parallel to the `W = 1` plane. It never hits it. There's no finite Euclidean point. But the line still has a direction — `(X, Y, 0)` points in the `(X, Y)` direction. As `W → 0` from above:

```
(X, Y, W) → (X/W, Y/W) → (∞, ∞) in the direction (X, Y)
```

So `(X, Y, 0)` is the **point at infinity** in the direction `(X, Y)`. In graphics, we don't usually work with actual points at infinity, but we do work with **direction vectors** — things like surface normals and light directions. These have no position, only direction. Encoding them with `W = 0` is not just a convention; it's the geometrically correct statement that they live at infinity in their respective directions.

---

## 4. Lifting to 3D: The w Component Distinguishes Points from Directions

For 3D graphics, extend by one more dimension. A 3D point `(x, y, z)` becomes the 4D homogeneous point `(x, y, z, 1)`. A 3D direction vector `(dx, dy, dz)` becomes `(dx, dy, dz, 0)`.

This distinction does real work. Consider the 4×4 translation matrix:

```
T(tx, ty, tz) =

[1   0   0   tx]
[0   1   0   ty]
[0   0   1   tz]
[0   0   0    1]
```

Apply it to a point with `w = 1`:

```
[1   0   0   tx]   [x ]   [x  + tx]
[0   1   0   ty] × [y ] = [y  + ty]
[0   0   1   tz]   [z ]   [z  + tz]
[0   0   0    1]   [1 ]   [1      ]
```

The point moves. Correct.

Now apply the same matrix to a direction with `w = 0`:

```
[1   0   0   tx]   [dx]   [dx]
[0   1   0   ty] × [dy] = [dy]
[0   0   1   tz]   [dz]   [dz]
[0   0   0    1]   [ 0]   [ 0]
```

The direction is unchanged. Also correct. "Moving the direction north by 5 meters" is geometrically meaningless — north doesn't have a position. The `w = 0` encoding enforces this automatically. You don't need to write special-case code; the math handles it.

Similarly, verify scale and rotation don't affect `w`:

- Scale matrix: last row is `[0, 0, 0, 1]`. A point with `w=1` stays `w=1`. A direction with `w=0` stays `w=0`.
- Rotation matrix: last row is also `[0, 0, 0, 1]`. Same argument.

The translation matrix is the special one — its last row ensures `w` passes through unchanged for both points and directions, while the `tx, ty, tz` entries in the last column only affect points (since `tx * 1 = tx` but `tx * 0 = 0`).

So the complete `4×4` transform matrices for the three fundamental operations are:

**Translation:**
```
[1  0  0  tx]
[0  1  0  ty]
[0  0  1  tz]
[0  0  0   1]
```

**Scale:**
```
[sx  0   0   0]
[ 0  sy  0   0]
[ 0   0  sz  0]
[ 0   0   0  1]
```

**Rotation about Z (angle θ):**
```
[cosθ  -sinθ  0  0]
[sinθ   cosθ  0  0]
[  0      0   1  0]
[  0      0   0  1]
```

All are `4×4`. All matrix-multiply with each other. You can chain them:

```
M_combined = T * R * S
```

And apply the combined matrix to a vertex exactly once:

```
v_out = M_combined * v_in
```

This is the payoff. The single multiply `gl_Position = MVP * vec4(position, 1.0)` in your vertex shader is doing the model transform, view transform, and projection transform all at once — because all three are `4×4` matrices that can be pre-multiplied on the CPU before the shader runs.

---

## 5. Operations on Homogeneous Coordinates — What Happens to w

Let's work through what happens to `w` for various operations. This builds intuition for why the algebra is consistent.

**Point + Point** (w = 1 + 1 = 2):

```
(x1, y1, z1, 1) + (x2, y2, z2, 1) = (x1+x2, y1+y2, z1+z2, 2)
```

This has `w = 2`, so it's not a normalized point. To recover the Euclidean point, divide by `w`:

```
((x1+x2)/2, (y1+y2)/2, (z1+z2)/2)
```

That's the **midpoint** of the two points. This is exactly what the rasterizer does: when interpolating vertex positions across a triangle, it's doing homogeneous addition. At `t = 0.5`, the interpolated position is the midpoint — and the math works out automatically.

**Point - Point** (w = 1 - 1 = 0):

```
(x1, y1, z1, 1) - (x2, y2, z2, 1) = (x1-x2, y1-y2, z1-z2, 0)
```

Result has `w = 0`. It's a **direction vector** — the vector from point 2 to point 1. This is not a coincidence. In linear algebra you've heard "point minus point equals vector." Homogeneous coordinates make this identity mechanical: the `w` arithmetic does it automatically.

**Point + Vector** (w = 1 + 0 = 1):

```
(x, y, z, 1) + (dx, dy, dz, 0) = (x+dx, y+dy, z+dz, 1)
```

Still a point. This is "move a point along a direction" — the fundamental operation of displacement.

**Vector + Vector** (w = 0 + 0 = 0):

```
(dx1, dy1, dz1, 0) + (dx2, dy2, dz2, 0) = (dx1+dx2, dy1+dy2, dz1+dz2, 0)
```

Still a direction vector. Adding two directions gives a combined direction. Consistent.

**Scalar × Point** (w = k × 1 = k):

```
k × (x, y, z, 1) = (kx, ky, kz, k)
```

Divide by `w = k` to recover the Euclidean point: `(x, y, z)`. Scaling a homogeneous point by a scalar doesn't move it — projective points are scale-invariant. This is the defining property of homogeneous coordinates.

Worked numeric example:

```
3 × (2, 1, 0, 1) = (6, 3, 0, 3)

Divide by w=3: (6/3, 3/3, 0/3) = (2, 1, 0)

Same point as before. Scalar multiplication does nothing to the Euclidean position.
```

---

## 6. What w Becomes After Perspective Projection — The Deep Part

This is where homogeneous coordinates stop being "just a trick for translation" and reveal their deeper purpose.

The perspective projection matrix (derived from `gluPerspective` or `glm::perspective`) has a specific structure. In OpenGL conventions (camera looking down -Z), its bottom two rows look like:

```
Projection matrix (simplified, near=n, far=f):

[  2n/(r-l)      0       (r+l)/(r-l)        0     ]
[     0       2n/(t-b)   (t+b)/(t-b)        0     ]
[     0          0      -(f+n)/(f-n)  -2fn/(f-n)  ]
[     0          0           -1              0     ]
```

Focus on the last row: `[0, 0, -1, 0]`. When you multiply an eye-space point `(x_e, y_e, z_e, 1)` by this matrix, the resulting `w` component is:

```
w_clip = 0*x_e + 0*y_e + (-1)*z_e + 0*1 = -z_e
```

The perspective matrix deliberately writes `-z_eye` into the `w` component of the clip-space result. Now the GPU performs **perspective division** — dividing all four components by `w_clip`:

```
(x_clip, y_clip, z_clip, w_clip)  →  (x_clip/w_clip, y_clip/w_clip, z_clip/w_clip, 1)
```

Since `w_clip = -z_eye`, this means we're dividing the x and y components by `-z_eye`.

In OpenGL, a point in front of the camera has negative z (the camera looks down -Z). So `z_eye` is negative, and `-z_eye` is positive. The division is by a positive number that grows larger as the point moves farther away.

**This is perspective foreshortening.** Objects farther from the camera divide by a larger number, making them appear smaller on screen. It emerges directly from the w division.

Let's see this with numbers.

**Example A — Point 5 units in front of the camera:**

Eye-space position: `(3, 2, -5, 1)`  
After projection (simplified — assume the projection scales by 1 for this example):
- `x_clip = 3`, `y_clip = 2`, `w_clip = -(-5) = 5`

Perspective division:
```
x_ndc = 3 / 5 = 0.60
y_ndc = 2 / 5 = 0.40
```

**Example B — Same point, but 50 units away:**

Eye-space position: `(3, 2, -50, 1)`  
After projection: `x_clip = 3`, `y_clip = 2`, `w_clip = 50`

Perspective division:
```
x_ndc = 3 / 50 = 0.06
y_ndc = 2 / 50 = 0.04
```

The second point is 10× farther away. Its screen position is 10× smaller. Perspective works, and it cost us nothing except the existing division by `w` that the hardware was going to do anyway.

```
Camera                 Point A          Point B
   |                  (5 units)        (50 units)
   ●  ─────────────────── ●  ──────────── ●
   |                  
   |   NDC x = 0.60       NDC x = 0.06
   |   (appears large)    (appears small)
   
   As z_eye doubles → w_clip doubles → NDC coords halve → object appears half-size
```

This is not a hack. The perspective matrix was specifically designed so that this division would produce the correct projective mapping from 3D to 2D. The `w` component carries the depth information needed to perform that division. Without a 4th coordinate to hold `w`, you'd have nowhere to put it.

---

## 7. Pitfalls — Bugs That Come From Misunderstanding w

These are real bugs that appear in real shader code. Know them now so you recognize them when they happen.

**Pitfall 1: Forgetting w=1 for points**

In GLSL, the canonical vertex shader line is:

```glsl
gl_Position = MVP * vec4(position, 1.0);
```

That `1.0` is the `w` component. If you accidentally pass `0.0`:

```glsl
gl_Position = MVP * vec4(position, 0.0);  // WRONG
```

Your vertex is now a direction vector. Translation has no effect on it, and after perspective division you get garbage — or the point appears at the far horizon in some direction. This is a common mistake when first writing vertex shaders. Always ask: is this a point or a direction?

**Pitfall 2: Normalizing too early (dividing by w yourself)**

The GPU automatically performs perspective division after the vertex shader returns `gl_Position`. If you manually divide by `w` inside the vertex shader, you've divided twice. The GPU sees `w = 1` (since you already divided) and divides again by 1 — which doesn't crash, but the z-buffer values are wrong because `z_clip / w_clip` no longer encodes depth correctly.

Do not touch `w` in the vertex shader output unless you specifically know what you're doing. Let the hardware handle perspective division.

**Pitfall 3: Normal vectors with w=1**

Surface normals are direction vectors — they should have `w = 0`. A common mistake:

```glsl
vec4 n = ModelMatrix * vec4(normal, 1.0);  // WRONG — w=1 means "point"
```

The translation component of `ModelMatrix` will be applied to your normal, moving it in space. A normal pointing up `(0, 1, 0)` would become `(tx, 1+ty, tz)` after translation — completely wrong. Normals have no position; translation must not affect them.

Correct:

```glsl
vec3 n = mat3(ModelMatrix) * normal;  // uses upper 3x3 only — no translation
```

Or equivalently:

```glsl
vec4 n = ModelMatrix * vec4(normal, 0.0);  // w=0 blocks translation
```

**Pitfall 4: w=0 normals and non-uniform scale (the normal matrix)**

Even with `w = 0`, if you apply a non-uniform scale to a normal, the direction is wrong. Imagine scaling an object by `(2, 1, 1)` — squishing it in Y and Z relative to X. A normal that was perpendicular to the surface before the scale will no longer be perpendicular after it, because the surface tilted but the normal didn't compensate correctly.

The fix: transform normals by the **inverse transpose** of the model matrix's upper 3×3, not the model matrix itself. This is `gl_NormalMatrix` in legacy OpenGL. L1D (the normals file in this series) covers this in full. For now, just know: normals need special handling; you cannot use the same matrix you use for vertex positions.

**Pitfall 5: Z-fighting from non-linear depth**

After perspective division, the value stored in the z-buffer is NOT linearly proportional to distance from the camera. The depth function is:

```
z_ndc = (f+n)/(f-n) + (2fn)/((f-n) * z_eye)
```

where `n` is the near plane distance and `f` is the far plane distance. This function is hyperbolic — it compresses most of the numeric precision into the region near the near plane and leaves almost none for the far plane.

If you set `near = 0.001` and `far = 100000`, roughly 99% of your z-buffer precision is consumed within the first few meters. Two surfaces at, say, 500 meters from the camera will have nearly identical z values. The depth test can't distinguish them. They'll flicker — one appearing in front of the other based on floating-point rounding. This is **z-fighting**.

Fix: make the near plane as large as you can tolerate, and the far plane as small as you can tolerate. The ratio `far/near` is what kills you — a ratio of 10,000,000 is catastrophic. A ratio of 1,000 is workable. This constraint is a direct consequence of division by `w` and how depth gets encoded.

**Pitfall 6: Points behind the camera — w going negative**

If a vertex is behind the camera, `z_eye > 0` (it's behind the -Z camera direction). Then `w_clip = -z_eye < 0`. After perspective division, the signs of `x_ndc` and `y_ndc` flip — the point appears mirrored on the opposite side of the screen.

The hardware clipper handles this by clipping against the homogeneous frustum planes **before** perspective division. The clip volume in homogeneous space is:

```
-w ≤ x ≤ w
-w ≤ y ≤ w
-w ≤ z ≤ w   (OpenGL convention)
```

For points with `w < 0`, these inequalities flip. The clipper correctly handles this in clip space, producing clipped triangles that, after division, appear correctly on screen.

This is another reason you must not perform perspective division manually in the vertex shader. You must return clip-space coordinates with the true `w_clip`, so the hardware clipper can operate correctly on `w < 0` cases before performing the division.

---

## 8. Why Homogeneous Coordinates Are Brilliant — The Summary

Step back and look at what one decision — add a 4th coordinate `w` — bought us:

1. **Translation as matrix multiplication.** The translation entries in the last column of a 4×4 matrix only affect vertices with `w = 1`. They don't affect direction vectors (`w = 0`). Translation is now matrix multiplication. The entire transform chain is matrix multiplication.

2. **Unified pipeline — MVP in one multiply.** Model transform, view transform, and projection transform are all 4×4 matrices. They compose by matrix multiplication. The GPU multiplies them together once on the CPU, then applies the combined matrix to every vertex. This is why `gl_Position = MVP * vec4(pos, 1.0)` handles everything.

3. **Point vs direction distinction — for free.** `w = 1` means point (position matters, translation affects it). `w = 0` means direction (position irrelevant, translation doesn't affect it). The hardware enforces this with no extra logic.

4. **Perspective foreshortening — for free.** The projection matrix writes `-z_eye` into `w`. The GPU divides by `w`. Objects farther away divide by larger numbers. They appear smaller. Perspective projection falls out of the arithmetic.

5. **Points at infinity.** `(X, Y, Z, 0)` represents the point at infinity in direction `(X, Y, Z)`. This is the mathematical home of direction vectors and the projective foundation of perspective vanishing points.

6. **Algebraically consistent operations.** Point minus point gives `w = 0` — a vector. Point plus vector gives `w = 1` — a point. The algebra enforces geometric sense automatically.

Every graphics pipeline in existence uses this. OpenGL, Vulkan, DirectX, Metal, WebGPU — all of them pass 4D homogeneous coordinates through the vertex shader. The GLSL type `vec4` for position is not OpenGL bureaucracy; it's the mathematical requirement of projective space. When you write `gl_Position = MVP * vec4(pos, 1.0)`, you are writing Möbius's projective coordinate system, deployed at GPU scale.

---

## Quick Reference — Homogeneous Coordinate Cheat Sheet

```
Representation:
  3D Point  (x, y, z)   →  4D Homogeneous (x, y, z, 1)
  3D Vector (dx,dy,dz)  →  4D Homogeneous (dx,dy,dz, 0)

Recovery (perspective division):
  (X, Y, Z, W) → (X/W, Y/W, Z/W)    [when W ≠ 0]
  (X, Y, Z, 0) → direction in (X,Y,Z)  [point at infinity]

Effect of w on operations:
  Point  + Point  → w = 2  → divide by 2 → midpoint
  Point  - Point  → w = 0  → direction vector
  Point  + Vector → w = 1  → point (displaced)
  Vector + Vector → w = 0  → direction vector
  k * Point       → w = k  → divide by k → original point (scale-invariant)

After perspective projection:
  w_clip = -z_eye
  x_ndc  = x_clip / w_clip  (objects farther → larger w → smaller NDC → appears smaller)
  
In GLSL — always:
  gl_Position = MVP * vec4(position, 1.0);   ← w=1 for points
  vec3 n = mat3(model) * normal;              ← no w at all, or vec4(normal, 0.0)
```

---

*Next: L1D — Normal Vectors and the Normal Matrix. Why you cannot use the model matrix to transform normals, and how the inverse transpose saves you.*
