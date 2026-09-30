# Lesson 6A — What glm::lookAt Computes and Why It Cannot Free-Fly

**Date:** Sept 29, 2026
**Prerequisites:** L1B (TRS Matrices and Coordinate Spaces), L1F (Full Render Pipeline)
**Goal:** Derive exactly what the view matrix is, what glm::lookAt computes step by step, and understand why the current target-based Camera.h cannot support free camera movement.

---

## Why This Exists

Your current Camera.h holds a `position` and a `target` point and calls `glm::lookAt(position, target, up)`. This works for a static scene — the camera stares at the origin forever. But the moment you want WASD movement or mouse look, you need to understand what lookAt actually constructs and why the target-based model breaks.

The view matrix is not just "another model matrix" — it encodes a coordinate frame change. Understanding how it is built from scratch lets you replace the target-based model with an orientation-based model (yaw and pitch) that is far more suitable for interactive cameras.

---

## What Eye Space Is

Recall from L1F: the vertex pipeline transforms vertex positions through four spaces — model space → world space → eye (camera) space → clip space.

Eye space is defined so that:
- The camera is at the **origin** (0, 0, 0)
- The camera looks along the **negative Z axis** (into the screen)
- **+Y** points up relative to the camera
- **+X** points right relative to the camera

This is a convention. OpenGL chose -Z as the viewing direction for historical reasons. In the resulting eye-space coordinate system, objects in front of the camera have negative Z values (since they are in the -Z direction from the origin).

The view matrix is the matrix that transforms world-space vertex positions into this eye-space coordinate frame. It is a rigid-body transform — a rotation followed by a translation (or equivalently, a translation of the world so the camera is at the origin, then a rotation to align the world axes with the camera's axes).

---

## Deriving lookAt From Scratch

Given three inputs:
- **P** = camera position in world space (where the camera is)
- **T** = target point in world space (what the camera looks at)
- **W** = world up vector, usually (0, 1, 0)

### Step 1: Forward vector

The camera looks from P toward T. The forward direction (from P to T) is:

```
f = normalize(T - P)
```

This is the direction the camera is looking. Length 1.

For P = (0, 1, 5) and T = (0, 0, 0):
```
T - P = (0-0, 0-1, 0-5) = (0, -1, -5)
|T - P| = sqrt(0 + 1 + 25) = sqrt(26) = 5.099
f = (0/5.099, -1/5.099, -5/5.099) = (0.0, -0.196, -0.981)
```

### Step 2: Right vector

The right vector is perpendicular to both the forward direction and the world up. The cross product gives exactly this:

```
r = normalize(cross(f, W))
```

For f = (0.0, -0.196, -0.981) and W = (0, 1, 0):
```
cross(f, W) = (f.y * W.z - f.z * W.y,
               f.z * W.x - f.x * W.z,
               f.x * W.y - f.y * W.x)
            = ((-0.196)(0) - (-0.981)(1),
               (-0.981)(0) - (0.0)(0),
               (0.0)(1) - (-0.196)(0))
            = (0.0 + 0.981, 0.0 - 0.0, 0.0 - 0.0)
            = (0.981, 0.0, 0.0)
r = normalize(0.981, 0.0, 0.0) = (1.0, 0.0, 0.0)
```

The right vector points purely along +X — the camera has no roll, so right is exactly the world X axis. This makes geometric sense: the camera is directly above the scene looking slightly downward with no horizontal rotation.

Why cross(f, W) and not cross(W, f)? Cross product is anti-commutative: cross(a,b) = -cross(b,a). With a right-handed coordinate system where +X is right, +Y is up, and -Z is forward, `cross(forward, worldUp)` points to the right. If you reverse the order, you get the left vector.

### Step 3: Camera up vector

The world up W might not be exactly perpendicular to f (it usually is not when the camera tilts). Recompute a true up vector that is perpendicular to both r and f:

```
u = cross(r, f)
```

Note: no normalization needed because r and f are already unit vectors and are perpendicular to each other (by construction from Step 2). The cross product of two perpendicular unit vectors is a unit vector.

For r = (1.0, 0.0, 0.0) and f = (0.0, -0.196, -0.981):
```
cross(r, f) = (r.y*f.z - r.z*f.y,
               r.z*f.x - r.x*f.z,
               r.x*f.y - r.y*f.x)
            = ((0)(-0.981) - (0)(-0.196),
               (0)(0.0) - (1.0)(-0.981),
               (1.0)(-0.196) - (0)(0.0))
            = (0, 0.981, -0.196)
u = (0.0, 0.981, -0.196)
```

This is the camera's up vector, tilted slightly backward (negative Z component) because the camera is looking down slightly.

### Step 4: Assembling the View Matrix

The view matrix combines two operations:
1. Rotate the world so the camera's basis vectors (r, u, -f) align with the world axes (X, Y, Z).
2. Translate the world so the camera position P moves to the origin.

The rotation part: the rows of the rotation matrix are the camera basis vectors. (This is the inverse of building a matrix from column basis vectors, because we want to transform world coordinates into camera coordinates, not the other way.)

```
          | r.x   r.y   r.z   0 |
R =       | u.x   u.y   u.z   0 |
          |-f.x  -f.y  -f.z   0 |
          |  0     0     0    1 |
```

Note the negation of f in the third row. The camera looks along -Z in eye space, but f points toward the target (positive direction). To make forward align with -Z, use -f.

The translation part (move P to origin): multiply each position by T_(-P).

Combined view matrix (the rotation absorbed into the translation):

```
         | r.x   r.y   r.z   -dot(r, P) |
V =      | u.x   u.y   u.z   -dot(u, P) |
         |-f.x  -f.y  -f.z    dot(f, P) |
         |  0     0     0         1      |
```

The right column entries are the negated dot products of each basis vector with the camera position. This is what glm::lookAt computes internally.

### Full Numeric Result

P = (0, 1, 5), T = (0, 0, 0), W = (0, 1, 0).
r = (1.0, 0.0, 0.0), u = (0.0, 0.981, -0.196), f = (0.0, -0.196, -0.981).

```
-dot(r, P) = -(1.0*0 + 0.0*1 + 0.0*5) = 0
-dot(u, P) = -(0.0*0 + 0.981*1 + (-0.196)*5) = -(0.981 - 0.980) = -0.001 ≈ 0
 dot(f, P) =  (0.0*0 + (-0.196)*1 + (-0.981)*5) = -0.196 - 4.905 = -5.101
```

View matrix (approximately):
```
| 1.000   0.000   0.000   0.000 |
| 0.000   0.981  -0.196   0.000 |
| 0.000   0.196   0.981  -5.101 |
| 0.000   0.000   0.000   1.000 |
```

Multiply this by the world-space position of the origin (0, 0, 0, 1):
```
eye_x = 1*0 + 0*0 + 0*0 + 0    = 0
eye_y = 0*0 + 0.981*0 - 0.196*0 + 0 = 0
eye_z = 0*0 + 0.196*0 + 0.981*0 - 5.101 = -5.101
```

The origin in eye space is at (0, 0, -5.101). It is in front of the camera (negative Z) at distance ~5.1 units. Correct — the origin is 5 units in front of the camera position (0,1,5).

---

## Why the Target-Based Camera Cannot Free-Fly

The current Camera.h stores `position` and `target` separately. When you press W to move forward, the natural implementation is:

```cpp
glm::vec3 forward = glm::normalize(target - position);
position += forward * speed * deltaTime;
```

But what happens to `target`? If you only update `position` and not `target`, the camera rotates to continue facing the old target point. Moving forward does not move the camera forward — it moves the camera toward the target, which is wrong for a free-fly camera.

You could update target too: `target += forward * speed * deltaTime`. But now what is `target`? It is always exactly one forward unit in front of the position. It contains no information that `position + normalize(target - position)` does not already contain. The `target` variable is doing nothing useful.

For mouse look: when the mouse moves right, you want to rotate the camera's viewing direction right. With a target-based model, you would rotate `target` around `position`. But to rotate `target` correctly, you need to know the camera's current orientation. The target-based model expresses orientation *implicitly* (as a direction vector to target), which makes rotation awkward.

The solution: throw away `target`. Replace it with explicit orientation state — two angles, **yaw** and **pitch**, from which you compute the `forward` vector directly. Yaw and pitch are the natural parameters for a free-fly camera and make all operations direct.

---

## What the New Design Looks Like

Instead of:
```cpp
glm::vec3 position = ...;
glm::vec3 target = ...;
```

Use:
```cpp
glm::vec3 position = ...;
float yaw   = -90.0f;  // start looking down -Z
float pitch =   0.0f;  // level horizon
// Derived (recomputed from yaw/pitch):
glm::vec3 front;   // forward direction
glm::vec3 right;   // right direction
glm::vec3 up;      // camera up
```

The view matrix becomes:
```cpp
return glm::lookAt(position, position + front, up);
```

`position + front` is always a valid target — it is always one unit in front of the camera. But now it is derived from the explicit orientation state (yaw, pitch), not stored as a separate variable.

This design has a single source of truth for orientation (yaw and pitch). Updating orientation = updating two floats. Everything else is derived.

---

## Pitfalls

**Passing worldUp directly as the up parameter to lookAt.** If the camera is tilted (pitch ≠ 0), the worldUp vector (0,1,0) is not perpendicular to the forward direction. glm::lookAt handles this internally by recomputing the true up vector. But if you bypass lookAt and construct the view matrix manually, you must use the recomputed u vector (Step 3 above), not worldUp directly.

**Degenerate lookAt when forward is parallel to worldUp.** If the camera points straight up (T = P + (0,1,0)) or straight down (T = P + (0,-1,0)), then f is parallel to W. `cross(f, W)` = (0, 0, 0) — undefined right vector. lookAt breaks. This is the gimbal lock problem in target-based cameras. The pitch-clamped yaw/pitch model avoids it by preventing pitch from reaching ±90°.

**Not normalizing f.** Forgetting `normalize()` in `f = normalize(T - P)` means f has a length equal to the distance from camera to target. The cross product and dot products in the view matrix then have wrong magnitudes. The view matrix is incorrect. Symptom: objects appear at wrong scales or positions relative to the camera.

---

## Industry Context

**Unreal Engine:** `UCameraComponent` does not store a target point. It stores a `FRotator` (pitch, yaw, roll) and constructs the view matrix from it each frame. The equivalent of `position + front` is `GetForwardVector()` which Unreal derives from the rotator.

**Unity:** `Camera` inherits from `Transform`. `transform.forward` is the -Z axis of the camera's local transform, derived from `transform.rotation` (a quaternion). No target point is stored.

**Film (virtual cameras in Houdini/Maya):** camera aim constraints are target-based — the camera's orientation is driven to always look at a specific null object. This is exactly the target-based model, appropriate for cinematic cameras that follow subjects. Interactive game cameras use orientation angles instead.

**Vulkan/OpenGL:** no built-in camera. You construct the view matrix yourself using exactly the derivation above. glm::lookAt is a helper function, not a GL API call. In a raw Vulkan renderer, you would compute the 4×4 matrix manually or use glm.

Next: L6B derives the yaw/pitch → forward vector conversion from spherical coordinates with full worked examples.
