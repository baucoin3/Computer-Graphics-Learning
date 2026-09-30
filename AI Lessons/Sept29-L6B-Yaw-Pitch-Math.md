# Lesson 6B — Yaw and Pitch: Deriving the Forward Vector from Angles

**Date:** Sept 29, 2026
**Prerequisites:** L6A (Camera Math)
**Goal:** Derive the forward direction vector from yaw and pitch angles using spherical coordinates, understand gimbal lock, and understand the pitch clamping requirement.

---

## Why This Exists

L6A showed that the correct camera model stores yaw and pitch angles instead of a target point. But where do those angles come from geometrically, and how exactly do you convert them to a 3D forward vector? This lesson derives that conversion from first principles. The derivation is not complicated — it is basic spherical coordinate geometry — but every detail matters: which angle is yaw, which is pitch, what zero means for each, and which direction the axes go. Get any of these wrong and your camera rotates in the wrong direction, pitches when it should yaw, or locks up at certain angles.

---

## Yaw and Pitch Defined

**Yaw** is rotation around the world Y axis (vertical axis). Positive yaw rotates left or right. Looking left and right at the horizon is purely yaw. In aviation and nautical terminology, yaw is the same concept — turning left or right while staying level.

**Pitch** is rotation around the camera's local X axis (the right vector, which points sideways). Positive pitch tilts the nose up; negative pitch tilts it down. Looking up at the sky or down at your feet is purely pitch.

**Roll** is rotation around the camera's local Z axis — tilting the horizon left or right. A standard FPS camera has no roll (the horizon stays level). We ignore roll in this design.

---

## Spherical Coordinates

Any point on the unit sphere can be described by two angles:
- **Azimuthal angle φ** (phi): angle measured in the horizontal (XZ) plane from the +X axis. Sometimes called longitude.
- **Polar angle θ** (theta): angle measured from the +Y axis (the "top" of the sphere). Sometimes called colatitude. A polar angle of 0° is straight up, 90° is horizontal.

In Cartesian coordinates, a point on the unit sphere with azimuthal angle φ and polar angle θ is:
```
x = sin(θ) * cos(φ)
y = cos(θ)
z = sin(θ) * sin(φ)
```

We will adapt this for a camera convention where:
- Yaw 0° = looking down -Z (the OpenGL default forward direction)
- Pitch 0° = looking horizontally (level horizon)

This requires a slight reinterpretation. Instead of using the polar angle from +Y, use **elevation** (pitch) measured from the horizontal plane: positive elevation = looking up, negative = looking down.

---

## Deriving the Forward Vector

Let **yaw** = φ (azimuthal angle, rotation around Y) and **pitch** = elevation above the horizontal.

The unit sphere point in elevation form uses pitch directly:
- The horizontal component magnitude is `cos(pitch)` (maximum at pitch=0°, decreasing to 0 at pitch=±90°)
- The vertical component is `sin(pitch)` (0 at pitch=0°, 1 at pitch=90°)

Splitting the horizontal component across X and Z using yaw:

```
forward.x = cos(pitch) * sin(yaw)
forward.y = sin(pitch)
forward.z = -cos(pitch) * cos(yaw)
```

Why negative Z for `forward.z`? At yaw=0 and pitch=0, the camera should look down -Z. At yaw=0: `forward.x = cos(0)*sin(0) = 0`, `forward.y = sin(0) = 0`, `forward.z = -cos(0)*cos(0) = -1`. Result: (0, 0, -1). Correct — looking straight ahead in OpenGL's coordinate system.

Why `sin(yaw)` for X and `-cos(yaw)` for Z? At yaw = 90°: sin(90°) = 1, cos(90°) = 0. So forward = (1, 0, 0) — looking to the right. Rotating yaw counterclockwise (positive direction) from 0° to 90° rotates the camera from -Z toward +X, which is rotating leftward when viewed from above. This is the standard convention. You can negate yaw to reverse the direction if needed.

---

## Worked Numeric Examples

All examples assume yaw and pitch in degrees, converted to radians before the trig functions: `radians = degrees * π / 180`.

### Example 1: yaw = 0°, pitch = 0°

```
forward.x = cos(0°) * sin(0°) = 1.0 * 0.0 = 0.0
forward.y = sin(0°) = 0.0
forward.z = -cos(0°) * cos(0°) = -1.0 * 1.0 = -1.0
```

Result: (0, 0, -1). Camera looks straight along -Z, level horizon. ✓

### Example 2: yaw = 90°, pitch = 0°

```
forward.x = cos(0°) * sin(90°) = 1.0 * 1.0 = 1.0
forward.y = sin(0°) = 0.0
forward.z = -cos(0°) * cos(90°) = -1.0 * 0.0 = 0.0
```

Result: (1, 0, 0). Camera looks along +X — rotated 90° to the right. ✓

### Example 3: yaw = 0°, pitch = 45°

```
forward.x = cos(45°) * sin(0°) = 0.707 * 0.0 = 0.0
forward.y = sin(45°) = 0.707
forward.z = -cos(45°) * cos(0°) = -0.707 * 1.0 = -0.707
```

Result: (0, 0.707, -0.707). Camera looks 45° above the horizon, straight ahead. ✓ Length check: sqrt(0² + 0.707² + 0.707²) = sqrt(0.5 + 0.5) = 1.0 ✓

### Example 4: yaw = 45°, pitch = 30°

Step 1 — convert to radians: yaw = 45° × π/180 = 0.7854 rad, pitch = 30° × π/180 = 0.5236 rad.

Step 2 — compute trig values:
```
cos(45°) = 0.7071
sin(45°) = 0.7071
cos(30°) = 0.8660
sin(30°) = 0.5000
```

Step 3 — apply formula:
```
forward.x = cos(30°) * sin(45°) = 0.8660 * 0.7071 = 0.6124
forward.y = sin(30°) = 0.5000
forward.z = -cos(30°) * cos(45°) = -0.8660 * 0.7071 = -0.6124
```

Result: (0.6124, 0.5000, -0.6124). Length: sqrt(0.6124² + 0.5² + 0.6124²) = sqrt(0.375 + 0.25 + 0.375) = sqrt(1.0) = 1.0 ✓

The forward vector points northeast and upward at 30°. If you placed a camera at the origin with these angles, it would look diagonally forward-right at 30° elevation.

### Example 5: yaw = 180°, pitch = 0°

```
forward.x = cos(0°) * sin(180°) = 1.0 * 0.0 = 0.0
forward.y = sin(0°) = 0.0
forward.z = -cos(0°) * cos(180°) = -1.0 * (-1.0) = 1.0
```

Result: (0, 0, 1). Camera looks along +Z — exactly backward from the default. ✓ (180° yaw = turned completely around.)

---

## Pitch Clamping to ±89°

What happens as pitch approaches ±90°?

At pitch = 90°:
```
forward.x = cos(90°) * sin(yaw) = 0.0 * sin(yaw) = 0.0
forward.y = sin(90°) = 1.0
forward.z = -cos(90°) * cos(yaw) = 0.0 * (-cos(yaw)) = 0.0
```

forward = (0, 1, 0) — pointing straight up, regardless of yaw.

Now compute the right vector: `right = normalize(cross(forward, worldUp))`. With forward = (0,1,0) and worldUp = (0,1,0):

```
cross((0,1,0), (0,1,0)):
x = 1*0 - 0*1 = 0
y = 0*0 - 0*0 = 0
z = 0*1 - 1*0 = 0
```

Result: (0, 0, 0) — the zero vector. You cannot normalize the zero vector (division by zero). lookAt (and any code that computes a camera basis) breaks completely.

The fix: clamp pitch before calling the formula.

```cpp
if (pitch > 89.0f)  pitch = 89.0f;
if (pitch < -89.0f) pitch = -89.0f;
```

At pitch = 89°:
```
forward.y = sin(89°) = 0.9998
cos(89°) = 0.0175
```

The horizontal component (cos) is still 0.0175 — small but nonzero. The right vector is computable:
```
right = normalize(cross((tiny, ~1, tiny), (0,1,0)))
```

This produces a valid right vector (mostly along X, tiny Z component). The camera can look almost straight up without breaking.

---

## Gimbal Lock

Gimbal lock is a fundamental problem with Euler angle representations of rotation. It occurs when two rotation axes align, causing a loss of one degree of freedom.

In the yaw/pitch/roll Euler decomposition, gimbal lock occurs when pitch = ±90°. At that point, yaw and roll become equivalent — rotating yaw has the same effect as rotating roll. You have effectively lost one axis of rotation. The camera cannot distinguish between "turn left from looking straight up" and "roll while looking straight up."

For an FPS camera, pitch clamping to ±89° is sufficient to avoid this — you simply prevent reaching the degenerate configuration.

For cameras that need to look straight up and down (free-flying spaceship camera, VR head tracking, drone simulation), pitch clamping is not acceptable. The correct solution is **quaternion** orientation.

### Quaternions as a Gimbal Lock Solution

A quaternion represents a rotation as:
```
q = (w, x, y, z)  where  w = cos(θ/2),  (x,y,z) = sin(θ/2) * axis
```

Quaternion rotation never "runs out" of degrees of freedom — the four-dimensional representation always maintains three fully independent rotation axes. Quaternion composition is just multiplication (q1 * q2 applies q2 then q1).

Critically, `slerp(q1, q2, t)` (spherical linear interpolation between two quaternions) produces smooth, constant-angular-velocity rotation along the shortest arc — impossible with Euler angles.

For an FPS-style camera, yaw + pitch + clamp is fine. For everything else (VR, flight simulation, cinematic cameras with arbitrary orientation), use quaternions.

---

## Deriving Right and Up From Forward

After computing `forward` from the formula above:

```cpp
glm::vec3 forward = glm::vec3(
    cos(glm::radians(pitch)) * sin(glm::radians(yaw)),
    sin(glm::radians(pitch)),
   -cos(glm::radians(pitch)) * cos(glm::radians(yaw))
);
forward = glm::normalize(forward);

glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);

glm::vec3 right = glm::normalize(glm::cross(forward, worldUp));
glm::vec3 up    = glm::normalize(glm::cross(right, forward));
```

Why `cross(forward, worldUp)` and not `cross(worldUp, forward)`? In OpenGL's right-handed coordinate system with +Y up, +X right, and -Z into the screen: `cross(forward, worldUp)` produces a vector in the +X direction (right) when forward is -Z. Verify: `cross((0,0,-1), (0,1,0))` = (-1*1 - (-1)*0, (-1)*0 - 0*1, 0*1 - 0*0) = wait, let me compute carefully:

```
cross((0,0,-1), (0,1,0)):
x = (0)(0) - (-1)(1) = 0 + 1 = 1
y = (-1)(0) - (0)(0) = 0
z = (0)(1) - (0)(0) = 0
```

Result: (1, 0, 0) — the +X direction. Correct, the right vector when looking down -Z is +X.

Why `cross(right, forward)` for up and not `cross(forward, right)`?

```
cross(right, forward) where right=(1,0,0), forward=(0,0,-1):
x = (0)(-1) - (0)(0) = 0
y = (0)(0) - (1)(-1) = 1
z = (1)(0) - (0)(0) = 0
```

Result: (0, 1, 0) — the +Y direction (up). Correct.

These three vectors — `forward`, `right`, `up` — form an orthonormal basis (all unit length, all mutually perpendicular). They are the three columns of the camera's local rotation matrix and the three rows of the view matrix rotation block (as derived in L6A).

---

## Pitfalls

**Using degrees vs radians.** `sin(yaw)` where `yaw` is in degrees produces completely wrong results. GLSL and C++ `<cmath>` trig functions take radians. Always convert: `sin(glm::radians(yawDegrees))`. The result of passing degrees to sin/cos is not obviously wrong (values are in [-1,1]) but the angles are off by a factor of π/180.

**Wrong sign on forward.z.** A common mistake is writing `forward.z = cos(pitch) * cos(yaw)` without the negation. At yaw=0, pitch=0, this gives forward = (0, 0, 1) — the camera looks along +Z, away from the scene. Everything appears behind the camera. Nothing renders. Add the negation.

**Not re-normalizing after computing forward.** Due to floating-point rounding in the cos/sin calculations, forward may not be exactly unit length. `glm::normalize()` corrects this. More importantly, `glm::cross(forward, worldUp)` scales with the magnitude of its inputs — if forward is not unit length, the right vector is not unit length either.

**worldUp = (0, 1, 0) when camera is pitched vertically.** At pitch = 89°, forward ≈ (0, 1, 0). `cross(forward, worldUp)` ≈ `cross((0,1,0), (0,1,0))` ≈ (0,0,0). This is the gimbal lock issue — pitch clamping to ±89° prevents reaching this degenerate case by maintaining a small horizontal forward component.

---

## Industry Context

**Unreal Engine:** `FRotator` stores pitch, yaw, roll in degrees. `FRotator::Vector()` computes the forward vector using the same cos/sin formula derived here (with a different axis convention — Unreal uses X-forward, Z-up instead of OpenGL's -Z-forward, Y-up). The yaw/pitch decomposition is identical in concept.

**Unity:** `Quaternion.Euler(pitch, yaw, 0)` builds a quaternion from pitch and yaw. `transform.rotation = Quaternion.Euler(...)` sets the camera orientation. Under the hood, the same matrix derivation happens. Unity's FPS controller separates vertical rotation (pitch, applied to camera only) from horizontal rotation (yaw, applied to character body).

**OpenXR (VR):** head tracking provides `XrQuaternionf orientation` directly from the HMD's IMU fusion. No yaw/pitch decomposition — quaternions all the way. Extracting yaw/pitch from an XrQuaternionf is possible but generally avoided for VR cameras since the quaternion can represent any orientation without gimbal lock.

Next: L6C covers the GLFW mouse callback system, per-frame delta computation, and how to convert mouse movement to yaw/pitch changes with worked numeric examples.
