# L1B — TRS Matrices and the Coordinate Space Walk

**CPS 511 Bridge Series | Lesson 1B**
**Prerequisite:** L1A (pipeline overview, framebuffer, GPU basics)

---

## What This Lesson Is Actually About

Every vertex you ever render travels through five distinct coordinate spaces before it becomes a pixel on screen. Every step of that journey is a matrix multiply. Before you can write a vertex shader, you need to understand what those matrices are doing and why they are shaped the way they are — not just "here is the formula," but where the formula comes from.


---

# Part A: TRS Matrix Derivations

## Why We Need Matrices at All

A transform is anything that moves, spins, or stretches a point. The elegant property of linear transforms is that they can be represented as matrix multiplies: `p' = M × p`. Once you have that, combining two transforms is just matrix multiplication: `M_combined = M2 × M1`. The CPU computes one combined matrix once per object per frame, then the GPU applies it to every vertex in parallel.

But here is the problem: not every transform we care about is linear. And that is where things get interesting.

---

## Translation: Why a 3×3 Matrix Cannot Do It

Before deriving the translation matrix, we need to prove why a 3×3 matrix is insufficient. This is not a historical curiosity — it explains why every modern graphics API uses 4×4 matrices.

**Claim:** There is no 3×3 matrix T such that `T × p = p + t` for all 3D points p.

**Proof by contradiction:**

Suppose such a T existed. Plug in `p = (0, 0, 0)`:

```
T × (0, 0, 0) = (0, 0, 0) + t = t
```

But for any matrix M and the zero vector **0**:

```
M × 0 = 0
```

This is true by definition of matrix-vector multiplication. Every dot product has a zero vector on the right side, so every result is zero. Therefore:

```
T × (0, 0, 0) = (0, 0, 0) ≠ t   (assuming t ≠ 0)
```

Contradiction. No such 3×3 matrix exists. Translation is an **affine** transform, not a linear one. Linear transforms always map the origin to itself. Translation moves the origin. Therefore it cannot be linear.

### Enter Homogeneous Coordinates

The solution computer graphics has used since the 1960s is to add a fourth dimension. We represent:

- A **3D point**     `(x, y, z)`   as `(x, y, z, 1)` — the fourth component w = 1
- A **3D direction** `(x, y, z)`   as `(x, y, z, 0)` — the fourth component w = 0

The distinction matters physically:
- **Points** can be translated. A position in space moves when the camera moves.
- **Directions** (normals, velocity vectors) cannot be translated. If you move a normal vector by (5, 0, 0), you have not rotated the surface; you have just computed nonsense.

The w = 0 vs w = 1 convention enforces this for free. You will see how in a moment.

### Deriving the 4×4 Translation Matrix

We want a 4×4 matrix T such that:

```
T × (x, y, z, 1) = (x + tx, y + ty, z + tz, 1)
```

Work backwards. The output has four rows. Let us call the rows of T: `[r0, r1, r2, r3]`. The input column vector is `(x, y, z, 1)`.

**Row 0** must produce `x + tx`. That means:
```
r0 · (x, y, z, 1) = 1·x + 0·y + 0·z + tx·1 = x + tx
```
So `r0 = (1, 0, 0, tx)`.

**Row 1** must produce `y + ty`:
```
r1 · (x, y, z, 1) = 0·x + 1·y + 0·z + ty·1 = y + ty
```
So `r1 = (0, 1, 0, ty)`.

**Row 2** must produce `z + tz`:
```
r2 · (x, y, z, 1) = 0·x + 0·y + 1·z + tz·1 = z + tz
```
So `r2 = (0, 0, 1, tz)`.

**Row 3** must produce `1` (we need to preserve w = 1):
```
r3 · (x, y, z, 1) = 0·x + 0·y + 0·z + 1·1 = 1
```
So `r3 = (0, 0, 0, 1)`.

Assembling these rows into a matrix:

```
        | 1  0  0  tx |
T(t) =  | 0  1  0  ty |
        | 0  0  1  tz |
        | 0  0  0   1 |
```

That is the translation matrix. Every entry was forced by the requirement that `T × p = p + t`.

### Symbolic Verification

Let us multiply it out in full to confirm:

```
| 1  0  0  tx |   | x |   | 1·x + 0·y + 0·z + tx·1 |   | x + tx |
| 0  1  0  ty | × | y | = | 0·x + 1·y + 0·z + ty·1 | = | y + ty |
| 0  0  1  tz |   | z |   | 0·x + 0·y + 1·z + tz·1 |   | z + tz |
| 0  0  0   1 |   | 1 |   | 0·x + 0·y + 0·z +  1·1 |   |   1    |
```

Now watch what happens with a direction vector (w = 0):

```
| 1  0  0  tx |   | x |   | x + tx·0 |   | x |
| 0  1  0  ty | × | y | = | y + ty·0 | = | y |
| 0  0  1  tz |   | z |   | z + tz·0 |   | z |
| 0  0  0   1 |   | 0 |   |     0    |   | 0 |
```

Directions are untranslated automatically. The w = 0 convention does the right thing without any special-case code.

### Worked Example: Translation

Vertex: `p = (2, 3, 0, 1)`. Translation vector: `t = (5, -1, 2)`.

Matrix:
```
| 1  0  0  5 |
| 0  1  0 -1 |
| 0  0  1  2 |
| 0  0  0  1 |
```

Multiply:

```
Row 0: 1·2 + 0·3 + 0·0 + 5·1  =  2 + 0 + 0 + 5  =  7
Row 1: 0·2 + 1·3 + 0·0 + (-1)·1 = 0 + 3 + 0 - 1 =  2
Row 2: 0·2 + 0·3 + 1·0 + 2·1  =  0 + 0 + 0 + 2  =  2
Row 3: 0·2 + 0·3 + 0·0 + 1·1  =  0 + 0 + 0 + 1  =  1
```

Result: `(7, 2, 2, 1)`. The vertex moved by `(5, -1, 2)`.

**Industry note:** In Unreal Engine, `FVector::operator+` computes this addition directly when you write `Location + FVector(5, -1, 2)` in Blueprint or C++. Under the hood, the engine's scene graph stores each actor's transform as a `FTransform`, which decomposes into translation, rotation, and scale — but when the time comes to send vertices to the GPU, it assembles them into exactly this matrix (or its equivalent in UE5's `FMatrix`). The hardware sees a matrix multiply, always.

---

## Rotation: Deriving from Geometry, Not Memorization

### 2D Rotation First

Before 3D, understand 2D. Consider a point P at position `(x, y)`. It is sitting on a circle of radius r around the origin. Its current angle from the positive X axis is φ. So:

```
x = r·cos(φ)
y = r·sin(φ)
```

```
            y
            |      . P' (after rotation)
            |    /
            |   /   . P (before rotation)
            |  /  /
            | / /
            |/ /   φ+θ
            +------------- x
              φ
```

We want to rotate P by angle θ to get `P' = (x', y')`. After rotation, P' is at angle `φ + θ` on the same circle:

```
x' = r·cos(φ + θ)
y' = r·sin(φ + θ)
```

Apply the trig addition identities:

```
cos(φ + θ) = cos(φ)·cos(θ) - sin(φ)·sin(θ)
sin(φ + θ) = sin(φ)·cos(θ) + cos(φ)·sin(θ)
```

Substitute `cos(φ) = x/r` and `sin(φ) = y/r`:

```
x' = r · (x/r · cos(θ) - y/r · sin(θ)) = x·cos(θ) - y·sin(θ)
y' = r · (y/r · cos(θ) + x/r · sin(θ)) = x·sin(θ) + y·cos(θ)
```

This is the 2D rotation. Written as a matrix:

```
         | cos(θ)  -sin(θ) |   | x |   | x·cos(θ) - y·sin(θ) |
R(θ) =   | sin(θ)   cos(θ) | × | y | = | x·sin(θ) + y·cos(θ) |
```

The matrix arises directly from the trig addition identities. You do not memorize the matrix; you remember "rotation by angle θ, expand `cos(φ+θ)` and `sin(φ+θ)`, collect the x and y terms."

### Extending to 3D: Rotation About the Z Axis

Rotation about the Z axis is rotation in the XY plane. Z stays the same. In homogeneous 4D:

```
         | cos(θ)  -sin(θ)   0   0 |
Rz(θ) =  | sin(θ)   cos(θ)   0   0 |
         |   0        0       1   0 |
         |   0        0       0   1 |
```

The top-left 2×2 is exactly the 2D rotation from above. The Z row is `(0, 0, 1, 0)` — Z passes through unchanged. The W row is `(0, 0, 0, 1)` — W passes through unchanged.

### Rotation About X: The YZ Plane

Rotation about X leaves X unchanged. The Y and Z coordinates rotate the same way X and Y did in 2D — Y plays the role of "x" and Z plays the role of "y":

```
         | 1     0        0    0 |
Rx(θ) =  | 0   cos(θ)  -sin(θ) 0 |
         | 0   sin(θ)   cos(θ) 0 |
         | 0     0        0    1 |
```

X row: `(1, 0, 0, 0)` — X unchanged. The YZ block is the 2D rotation.

### Rotation About Y: Why the Signs Flip

This is the one that trips people up. Rotation about Y leaves Y unchanged. X and Z rotate. But the matrix is:

```
         |  cos(θ)   0   sin(θ)   0 |
Ry(θ) =  |    0      1     0      0 |
         | -sin(θ)   0   cos(θ)   0 |
         |    0      0     0      1 |
```

Notice the negative sign is on `-sin(θ)` in the bottom-left, not the top-right. For Rz and Rx, the negative is on the top-right. Why?

**The right-hand rule and cyclic permutation.** In a right-handed coordinate system, the axes cycle as:

```
X → Y → Z → X → Y → Z → ...
```

Positive rotation about any axis follows the right-hand rule: curl your right-hand fingers from the first axis toward the second.

- Rz rotates X toward Y (standard 2D: x→y for positive θ). Negative is at (row 0, col 1): `-sin(θ)`.
- Rx rotates Y toward Z (cyclic: y→z for positive θ). Negative is at (row 1, col 2): `-sin(θ)`.
- Ry rotates Z toward X (cyclic: z→x for positive θ). But in matrix layout, X is row 0 and Z is row 2. Rotating Z toward X means the positive sin is at (row 0, col 2) = `sin(θ)` and the negative is at (row 2, col 0) = `-sin(θ)`.

The pattern holds consistently — it is the cyclic order `x→y→z→x` that determines the sign placement. For the middle axis (Y), the layout in the matrix appears flipped relative to the other two because Z appears after Y in the index order, but comes before X in the cycle.

### Worked Example: Rz(90°)

Vertex: `p = (1, 0, 0, 1)`. Rotate 90° around Z.

```
cos(90°) = 0,  sin(90°) = 1
```

Matrix:
```
| 0  -1   0   0 |
| 1   0   0   0 |
| 0   0   1   0 |
| 0   0   0   1 |
```

Multiply:
```
Row 0: 0·1 + (-1)·0 + 0·0 + 0·1  =  0 - 0 + 0 + 0  =  0
Row 1: 1·1 +   0·0 + 0·0 + 0·1   =  1 + 0 + 0 + 0  =  1
Row 2: 0·1 +   0·0 + 1·0 + 0·1   =  0 + 0 + 0 + 0  =  0
Row 3: 0·1 +   0·0 + 0·0 + 1·1   =  0 + 0 + 0 + 1  =  1
```

Result: `(0, 1, 0, 1)`.

Geometric check: the positive X axis `(1, 0, 0)` rotated 90° counterclockwise in the XY plane becomes the positive Y axis `(0, 1, 0)`. Correct.

### Why R⁻¹ = Rᵀ (and Why This Matters)

Rotation matrices are **orthonormal**: each row is a unit vector, and all rows are mutually perpendicular. A consequence of this is that the inverse equals the transpose.

**Proof for the 2D case:**

Let R be the 2D rotation matrix. Compute `Rᵀ × R`:

```
Rᵀ = | cos(θ)   sin(θ) |
     | -sin(θ)  cos(θ) |

R  = | cos(θ)  -sin(θ) |
     | sin(θ)   cos(θ) |
```

```
(Rᵀ × R)[0][0] = cos(θ)·cos(θ) + sin(θ)·sin(θ) = cos²(θ) + sin²(θ) = 1
(Rᵀ × R)[0][1] = cos(θ)·(-sin(θ)) + sin(θ)·cos(θ) = -cos·sin + sin·cos = 0
(Rᵀ × R)[1][0] = (-sin(θ))·cos(θ) + cos(θ)·sin(θ) = -sin·cos + cos·sin = 0
(Rᵀ × R)[1][1] = (-sin(θ))·(-sin(θ)) + cos(θ)·cos(θ) = sin²(θ) + cos²(θ) = 1
```

`Rᵀ × R = I`. Therefore `Rᵀ = R⁻¹`.

The identity `cos²(θ) + sin²(θ) = 1` is the Pythagorean theorem hiding inside the dot product of two perpendicular unit vectors. The same proof extends to 3D and 4D rotation matrices.

**Why this matters in practice:** To invert a general matrix you need Gaussian elimination — O(n³) operations and significant numerical precision problems. To invert a rotation matrix you just transpose it — O(n²) with no numerical issues. This is why rotation matrices (and quaternions) are preferred over general matrices for storing orientations. In your vertex shader, you will see `gl_NormalMatrix` defined as the inverse transpose of the ModelView's upper 3×3. For the rotation portion, the inverse IS the transpose — two operations that cancel each other, leaving just the rotation block.

**Industry note:** Quaternions are the practical storage format for rotations in every modern engine (Unity, Unreal, Godot). They avoid gimbal lock, are compact (4 floats vs 16), and interpolate smoothly via SLERP (covered in the VR orientation module). But when a quaternion is used to rotate a vertex, it is internally converted to a 3×3 or 4×4 rotation matrix — the same one derived here. The math is identical.

---

## Scale: The Diagonal Matrix

Scale is the simplest of the three. We want:

```
S × (x, y, z, 1) = (sx·x, sy·y, sz·z, 1)
```

Work through the row requirements the same way we did for translation:

- **Row 0** must produce `sx·x`: `(sx, 0, 0, 0) · (x, y, z, 1) = sx·x`. So `r0 = (sx, 0, 0, 0)`.
- **Row 1** must produce `sy·y`: `r1 = (0, sy, 0, 0)`.
- **Row 2** must produce `sz·z`: `r2 = (0, 0, sz, 0)`.
- **Row 3** must produce `1`: `r3 = (0, 0, 0, 1)`.

```
        | sx   0    0   0 |
S(s) =  |  0   sy   0   0 |
        |  0    0   sz  0 |
        |  0    0    0  1 |
```

A diagonal matrix. Every axis scaled independently.

### Worked Example: Scale

Vertex: `p = (1, 1, 0, 1)`. Scale by `(2, 3, 1)`.

Matrix:
```
| 2  0  0  0 |
| 0  3  0  0 |
| 0  0  1  0 |
| 0  0  0  1 |
```

Multiply:
```
Row 0: 2·1 + 0·1 + 0·0 + 0·1  =  2
Row 1: 0·1 + 3·1 + 0·0 + 0·1  =  3
Row 2: 0·1 + 0·1 + 1·0 + 0·1  =  0
Row 3: 0·1 + 0·1 + 0·0 + 1·1  =  1
```

Result: `(2, 3, 0, 1)`. The XY point `(1, 1)` became `(2, 3)`.

### Uniform vs Non-Uniform Scale

**Uniform scale:** `sx = sy = sz = s`. The object grows or shrinks equally in all directions. Normals are scaled but remain perpendicular to surfaces — a scaled normal still points in the correct direction, it just has a different magnitude. You re-normalize and everything is fine.

**Non-uniform scale:** `sx ≠ sy` (or any pair differs). The object is stretched in one direction and not others. Normals are *not* preserved under non-uniform scale. A normal that was perpendicular to a surface before may no longer be perpendicular after the scale is applied.

This is why normals must be transformed by the **inverse transpose** of the model matrix, not the model matrix itself. This is one of the most common bugs when porting legacy OpenGL code to shaders. You will see it in full detail in L1D (Normals and Lighting).

**Industry pitfall — "my normals look wrong after import":** You import a .fbx asset, it appears in the viewport with completely wrong lighting, shading goes dark on the wrong side. Nine times out of ten, the asset was exported with a non-uniform scale applied and the normals were not baked to account for it. The fix is either to apply the scale in the DCC tool (Blender: Ctrl+A → Apply Scale) before export, or to pass the correct normal matrix to your shader. Both issues trace back to this math.

---

## Combining TRS: Order Matters

Matrix multiplication is not commutative: `A × B ≠ B × A` in general. This has direct consequences for how you compose transforms.

### Concrete Proof That Order Matters

Take vertex `p = (1, 0, 0, 1)`.

**Case 1: Rotate first (Rz 90°), then Translate (+2 along X)**

```
Step 1 — Rz(90°) × p:
| 0  -1  0  0 |   | 1 |   | 0·1 + (-1)·0 + 0·0 + 0·1 |   |  0 |
| 1   0  0  0 | × | 0 | = | 1·1 +   0·0 + 0·0 + 0·1 |   =  |  1 |
| 0   0  1  0 |   | 0 |   | 0·1 +   0·0 + 1·0 + 0·1 |       |  0 |
| 0   0  0  1 |   | 1 |   | 0·1 +   0·0 + 0·0 + 1·1 |       |  1 |

After Rz: (0, 1, 0, 1)

Step 2 — T(2,0,0) × (0,1,0,1):
| 1  0  0  2 |   | 0 |   | 1·0 + 0·1 + 0·0 + 2·1 |   | 2 |
| 0  1  0  0 | × | 1 | = | 0·0 + 1·1 + 0·0 + 0·1 | = | 1 |
| 0  0  1  0 |   | 0 |   | 0·0 + 0·1 + 1·0 + 0·1 |   | 0 |
| 0  0  0  1 |   | 1 |   | 0·0 + 0·1 + 0·0 + 1·1 |   | 1 |
```

**Result (Rotate then Translate): `(2, 1, 0, 1)`**

The object spun in place, then was moved +2 along X. The final position is 2 units to the right of where it landed after spinning.

**Case 2: Translate first (+2 along X), then Rotate (Rz 90°)**

```
Step 1 — T(2,0,0) × p:
| 1  0  0  2 |   | 1 |   | 1·1 + 0·0 + 0·0 + 2·1 |   | 3 |
| 0  1  0  0 | × | 0 | = | 0·1 + 1·0 + 0·0 + 0·1 | = | 0 |
| 0  0  1  0 |   | 0 |   | 0·1 + 0·0 + 1·0 + 0·1 |   | 0 |
| 0  0  0  1 |   | 1 |   | 0·1 + 0·0 + 0·0 + 1·1 |   | 1 |

After T: (3, 0, 0, 1)

Step 2 — Rz(90°) × (3,0,0,1):
| 0  -1  0  0 |   | 3 |   | 0·3 + (-1)·0 + 0·0 + 0·1 |   |  0 |
| 1   0  0  0 | × | 0 | = | 1·3 +   0·0  + 0·0 + 0·1 | = |  3 |
| 0   0  1  0 |   | 0 |   | 0·3 +   0·0  + 1·0 + 0·1 |   |  0 |
| 0   0  0  1 |   | 1 |   | 0·3 +   0·0  + 0·0 + 1·1 |   |  1 |
```

**Result (Translate then Rotate): `(0, 3, 0, 1)`**

The vertex moved 3 units along X to `(3,0,0)`, then the rotation swept it around the world origin, placing it at `(0, 3, 0)`. This is an **orbit** around the origin, not a spin in place.

**The two cases give completely different results: `(2, 1, 0)` vs `(0, 3, 0)`.** 

### The Canonical Order: Scale → Rotate → Translate

The standard model matrix is `M = T × R × S`. Because matrix multiplication applies right to left, this means:

```
p_world = T × R × S × p_model

           (S is applied first)
           (R is applied second)
           (T is applied last)
```

**Why this order?**

1. **Scale first:** you want to resize the mesh around its own origin (the object's center). If you translated first, scaling would also push the object farther from world origin.

2. **Rotate second:** you want to orient the object around its own center. If you translated first and then rotated, the rotation would sweep the object around the world origin — it would orbit instead of spin.

3. **Translate last:** once the object has the right size and orientation, place it in the world.

```
     Scale          Rotate         Translate
(resize in place) → (orient in place) → (move to world position)
```

This is what every engine's transform component does internally. In Unity it is `transform.localScale`, `transform.localRotation`, `transform.localPosition`. In Unreal it is `FTransform`. The decomposed representation is stored separately for artist editability, but the assembled matrix is always TRS in that multiplication order.

---