# L7A — Player Movement & AABB Collision

**Date:** Sept 29 2026  
**Project reference:** `cg-work/projects/03-full-render-pipeline-game-structure/`

---

## Part 1 — What Problem Does Player Movement Solve?

You have a camera in world space. Left to itself it floats anywhere — through walls, underground, into the sky. "Player movement" means:

1. Translate input (keys, mouse) into a world-space displacement each frame.
2. Prevent that displacement from putting the player inside solid geometry.
3. Optionally apply physics forces (gravity, friction, acceleration).

Project 3 handles points 1 and 2 in the simplest possible way. Point 3 is absent — there is no gravity, no jump, no friction. That is intentional: it isolates the core concept.

---

## Part 2 — Delta Time: Why Every Velocity Multiplies by It

### The problem without delta time

Suppose you move the camera `0.05f` units per frame. On a machine running at 60 FPS you cross 1 meter in `1.0 / (0.05 * 60) ≈ 0.33` seconds. On a machine running at 144 FPS you cross the same meter in `0.14` seconds — you move 2.4× faster. The game is unplayable on fast hardware.

### The fix

Delta time (`dt`) is the number of seconds the last frame took to render. You measured it in L6:

```cpp
float now       = static_cast<float>(glfwGetTime());
float deltaTime = now - lastFrame;
lastFrame       = now;
```

Multiply every velocity by `dt`:

```
displacement = speed [units/second] × dt [seconds] = units
```

Now crossing 1 meter at `speed = 5.0f` takes exactly `1.0 / 5.0 = 0.2` seconds on every machine regardless of frame rate.

Project 3 does this correctly:

```cpp
// Camera.h — processKeyboard
float v = movementSpeed * deltaTime;   // movementSpeed = 5.0f units/sec
position += front * v;
```

**Industry note:** In Unity the equivalent is `Time.deltaTime`. In Unreal it is the `DeltaSeconds` float passed to every `Tick()` function. In Godot it is `delta` in `_process(delta)`. Every engine exposes it because fixed-frame velocity is a hard bug.

---

## Part 3 — FPS Movement: Horizontal Plane Lock

### What the camera's `front` vector points at

`front` is computed from yaw and pitch (see L6B). If you look up 45°, `front` points diagonally upward:

```
front.y = sin(45°) ≈ 0.707
```

Moving along `front` directly means you fly upward when you look up. That is correct for a flight simulator. It is wrong for a first-person shooter where the floor stays under you.

### Project 3 solves this with a hard Y-lock

```cpp
// main.cpp — game loop
camera.position.y = 1.0f;   // clamp after every movement
```

This discards whatever vertical displacement `processKeyboard` applied. It works, but it is the bluntest possible tool. Consequence: you cannot walk up stairs or ramps. Gravity cannot affect you.

### The FPS-correct alternative (not in project 3, but industry standard)

Project the `front` vector onto the horizontal plane before movement:

```cpp
glm::vec3 flatFront = glm::normalize(glm::vec3(front.x, 0.0f, front.z));
glm::vec3 flatRight = glm::normalize(glm::vec3(right.x, 0.0f, right.z));

if (W) position += flatFront * speed * dt;
if (S) position -= flatFront * speed * dt;
if (A) position -= flatRight * speed * dt;
if (D) position += flatRight * speed * dt;
```

Now looking up does not pull you forward-and-upward. Your horizontal velocity is always horizontal. Gravity (separate vertical velocity) handles Y independently.

---

## Part 4 — AABB Collision: First Principles

### What AABB means

**Axis-Aligned Bounding Box.** An AABB is a rectangular box whose faces are always parallel to the world X, Y, Z axes. It never rotates. You define it by two points:

```
min = (minX, minY, minZ)   ← closest corner to -∞
max = (maxX, maxY, maxZ)   ← farthest corner to +∞
```

Any point P is inside the box when:

```
min.x ≤ P.x ≤ max.x
min.y ≤ P.y ≤ max.y
min.z ≤ P.z ≤ max.z
```

### Why axis-aligned specifically?

A box aligned to the world axes only needs 6 numbers to describe it (minX, minY, minZ, maxX, maxY, maxZ). Two AABBs overlap when they overlap on every axis simultaneously — and checking axis overlap is three comparisons, one per axis. Rotated boxes (OBBs — Oriented Bounding Boxes) require projecting onto 15 separating axes via the Separating Axis Theorem. AABB is fast; OBB is expensive. For blocky geometry where objects don't rotate (walls, floors, crates), AABB is almost always sufficient.

### The overlap test

Two AABBs do **not** overlap when there is a gap on any one axis. In 1D:

```
A:  [aMin ──────── aMax]
                           B: [bMin ──── bMax]
         gap on X → no overlap
```

They **do** overlap on axis X when:

```
aMin.x <= bMax.x   AND   aMax.x >= bMin.x
```

That must hold on all three axes for 3D overlap. Project 3's `AABB::intersects`:

```cpp
bool intersects(const AABB& other) const {
    return (min.x <= other.max.x && max.x >= other.min.x) &&
           (min.y <= other.max.y && max.y >= other.min.y) &&
           (min.z <= other.max.z && max.z >= other.min.z);
}
```

Six comparisons total. Extremely cheap.

### Building AABB from a Transform

```cpp
static AABB fromTransform(const Transform& t) {
    glm::vec3 half = t.scale * 0.5f;
    return { t.position - half, t.position + half };
}
```

This treats `scale` as the full side length of the box and `position` as the box center. Rotation is intentionally ignored — the box stays world-axis-aligned regardless of how the GameObject rotates. This is the definition of AABB: rotation does not affect the box.

**Numeric example.** A green tall cube: `position = (-2.5, 1.0, -2.0)`, `scale = (1.0, 2.0, 1.0)`.

```
half  = (0.5, 1.0, 0.5)
min   = (-3.0, 0.0, -2.5)
max   = (-2.0, 2.0, -1.5)
```

The box bottom face sits at Y=0 (ground level). Top face at Y=2.

---

## Part 5 — Project 3's Collision Response: Rollback

### How it works

```cpp
glm::vec3 oldPos = camera.position;   // save before movement

// ... process WASD input ...

camera.position.y = 1.0f;            // Y lock

glm::vec3 halfPlayer(0.3f, 0.9f, 0.3f);
AABB playerBox{ camera.position - halfPlayer, camera.position + halfPlayer };

for (const auto& obj : objects) {
    if (obj.collidable && playerBox.intersects(obj.getAABB()))
        camera.position = oldPos;     // discard this frame's movement
}
```

Player box dimensions: 0.6 wide, 1.8 tall, 0.6 deep — roughly a human capsule flattened to a box. If that box overlaps any collidable object after moving, undo the whole move.

### What this gets right

Simple. No false negatives on slow movement. Easy to reason about.

### What this gets wrong (and breaks)

**1. No sliding.**  
Walk toward a wall at a 45° angle. The X and Z components of your velocity both violate the box. Rollback cancels both. You stop dead instead of sliding along the wall. Every real FPS resolves X and Z independently and only cancels the penetrating axis.

**2. Tunneling.**  
At high speed (large `dt` spike, e.g., after a frame hitch), the player can jump through thin geometry in one frame because the check only tests the final position, not the path traveled.

**3. Corner sticking.**  
Walk along a wall and clip a corner. The rollback fires even though you were trying to move parallel to the wall. You stop.

**4. No push resolution.**  
Rollback does not tell you by how much you overlapped. You cannot push the player to the wall surface — only away from the new position entirely.

These limitations are acceptable for a learning project. Every commercial game uses a more sophisticated response.

---

## Part 6 — Better Response: Minimum Translation Vector (MTV)

**Industry standard for static collision resolution.**

Instead of "did we overlap?" ask "how far, and in which direction, is the minimum displacement to separate us?"

For two AABBs, compute the overlap depth on each axis:

```
overlapX = min(aMax.x, bMax.x) - max(aMin.x, bMin.x)
overlapY = min(aMax.y, bMax.y) - max(aMin.y, bMin.y)
overlapZ = min(aMax.z, bMax.z) - max(aMin.z, bMin.z)
```

All three must be positive for an overlap to exist (same as `intersects()`). The **smallest** of the three is the MTV axis — push the player along that axis by that amount.

```cpp
// Pseudocode — not in project 3
glm::vec3 resolveAABB(AABB player, AABB wall) {
    float ox = std::min(player.max.x, wall.max.x) - std::max(player.min.x, wall.min.x);
    float oy = std::min(player.max.y, wall.max.y) - std::max(player.min.y, wall.min.y);
    float oz = std::min(player.max.z, wall.max.z) - std::max(player.min.z, wall.min.z);

    // Push along smallest-overlap axis
    if (ox < oy && ox < oz)
        return { (player.min.x < wall.min.x ? -ox : ox), 0, 0 };
    else if (oy < ox && oy < oz)
        return { 0, (player.min.y < wall.min.y ? -oy : oy), 0 };
    else
        return { 0, 0, (player.min.z < wall.min.z ? -oz : oz) };
}
```

Apply the returned vector to `camera.position`. This gives you **wall sliding** for free: you push perpendicular to the wall surface, so tangential movement survives.

**This is what Unity's `CharacterController` does internally.** In Unreal, `UCharacterMovementComponent` does the same via swept capsule queries against the collision geometry.

---

## Part 7 — Adding Gravity and Velocity

Project 3 has no physics state. To add gravity, give the player a persistent velocity vector:

```cpp
// Player state (add to Camera or a separate PlayerController struct)
glm::vec3 velocity   = glm::vec3(0.0f);
bool      grounded   = false;
const float gravity  = -20.0f;   // units/sec², tuned for feel
const float jumpForce = 8.0f;
```

Each frame:

```cpp
// 1. Apply gravity (vertical axis only)
if (!grounded)
    velocity.y += gravity * dt;   // Euler integration

// 2. WASD sets horizontal velocity directly (no acceleration yet)
glm::vec3 flatFront = glm::normalize(glm::vec3(front.x, 0.0f, front.z));
velocity.x = 0.0f;
velocity.z = 0.0f;
if (W) velocity.x += flatFront.x * speed, velocity.z += flatFront.z * speed;
// ... etc

// 3. Jump
if (SPACE && grounded)
    velocity.y = jumpForce;

// 4. Integrate position
position += velocity * dt;

// 5. Collision resolve (now needs Y-axis resolution to set grounded)
```

### Euler integration

`position += velocity * dt` is **semi-implicit Euler integration** — the simplest time integrator. It is first-order accurate: small `dt` = good approximation, large `dt` = visible error (objects fall too fast or too slow for that frame). Every game engine uses this or a slightly better variant (Verlet, RK4 for projectiles).

### Determining `grounded`

After MTV resolution, if the push vector was in the +Y direction, the player is standing on something. Set `grounded = true`. If no downward contact, `grounded = false`. This is the ground-detection mechanism that enables jumping without double-jumping.

---

## Part 8 — Capsule Colliders: Why Games Don't Use Box Players

AABB works on axis-aligned boxes. Player characters have a problem: **corners**. A box-shaped player catches on every small ledge, stair riser, or floor seam. The box corners dig into geometry that the player visually walks over.

The industry solution: **capsule collider** — a cylinder with hemispherical caps. Properties:

- No corners. Slides over small bumps automatically.
- One radius, one height. Cheaply defined.
- Capsule-vs-AABB intersection is more expensive to compute than AABB-vs-AABB, but the behavior is vastly better.

In Unity: `CapsuleCollider` on the player. In Unreal: `UCapsuleComponent` is the default collision shape on `ACharacter`. Every shipped FPS uses a capsule.

For project 3's purposes, box is fine. In a shipping game, replace the player box with capsule queries.

---

## Part 9 — Swept AABB: Fixing Tunneling

Current project 3: test final position only. If the player moves 5 units in one frame and a wall is 1 unit thick, they pass through it.

**Swept AABB** (also called **continuous collision detection**, CCD) tests the entire path from `oldPos` to `newPos`.

Conceptually: expand the player's AABB to include its motion as a "swept volume." Check whether any wall intersects that swept volume. If so, find the earliest time-of-impact `t ∈ [0, 1]` and move only to `oldPos + t * (newPos - oldPos)`.

For a moving AABB against a static AABB the time-of-impact formula per axis is:

```
entry_time_x = (staticMin.x - movingMax.x) / velocity.x   (if velocity.x > 0)
entry_time_x = (staticMax.x - movingMin.x) / velocity.x   (if velocity.x < 0)
```

`t_entry = max(entry_time_x, entry_time_y, entry_time_z)`  
`t_exit  = min(exit_time_x,  exit_time_y,  exit_time_z)`

Collision occurs when `t_entry < t_exit` and `t_entry < 1.0`.

This is what `btConvexSweepTest` in Bullet Physics does, and what Unreal's `SweepSingleByChannel` does. For project 3's low speeds it is overkill, but you will need it in any game where characters or projectiles move fast.

---

## Part 10 — Advanced Movement Patterns in Other Game Genres

### 10.1 — Platform Games: Coyote Time and Jump Buffering

**Coyote time:** Player walks off a ledge. For ~0.1–0.15 seconds after leaving the ledge, allow a jump as if still grounded. Feels fair; the player "just barely" missed the edge.

```cpp
float coyoteTimer = 0.0f;
const float coyoteWindow = 0.12f;

if (grounded)
    coyoteTimer = coyoteWindow;
else
    coyoteTimer -= dt;

bool canJump = coyoteTimer > 0.0f;
if (SPACE && canJump) { velocity.y = jumpForce; coyoteTimer = 0.0f; }
```

**Jump buffering:** Player presses jump 0.1 seconds before landing. Without buffering, the input is ignored because `grounded = false`. With buffering, queue the jump and fire it the frame grounded becomes true.

```cpp
float jumpBufferTimer = 0.0f;
const float jumpBufferWindow = 0.1f;

if (SPACE) jumpBufferTimer = jumpBufferWindow;
else       jumpBufferTimer -= dt;

if (grounded && jumpBufferTimer > 0.0f) {
    velocity.y = jumpForce;
    jumpBufferTimer = 0.0f;
}
```

Both tricks are in every shipped 2D and 3D platformer. They are not physics — they are feel tuning.

### 10.2 — Strafejumping and Air Strafing (Quake / Source)

In Quake and Half-Life, moving in the air does not simply apply velocity. It applies an **additive acceleration** in the wish direction, capped so the speed can only increase if the wish direction is orthogonal to current velocity. This allows a skilled player to accelerate past `movementSpeed` by "surfing" the acceleration cap.

The formula (Quake `PM_AirAccelerate`):

```cpp
// wishdir = normalized input direction
// currentSpeed = dot(velocity, wishdir) — projection
float addSpeed = std::min(sv_airaccel * wishspeed * dt,
                          wishspeed - currentSpeed);
if (addSpeed > 0)
    velocity += wishdir * addSpeed;
```

If `currentSpeed` is already above `wishspeed`, no acceleration is added in the wish direction. But if you strafe perpendicular, `currentSpeed` in that direction is near zero, so you still gain speed. Counter-Strike, TF2, and all Source games inherit this system.

### 10.3 — Sliding / Crouching

Sliding: player presses crouch while moving above a speed threshold. Set a `sliding = true` flag. While sliding, reduce friction (or zero it), lower the capsule height, and optionally add a forward impulse. After a timer or speed drop, return to walk.

Crouch: shrink the capsule height. Shrinking is easy. Un-crouching requires a check — if the full-height capsule would intersect a ceiling, block the uncrouch. Always sweep upward before restoring height.

### 10.4 — Wall Running (Titanfall, Mirror's Edge)

On contact with a near-vertical surface while airborne and above a minimum speed:

1. Detect wall normal (the collision normal of the surface).
2. Project velocity onto the wall plane: `v_along_wall = velocity - dot(velocity, wallNormal) * wallNormal`.
3. Kill gravity temporarily (or apply reduced gravity).
4. Allow a wall-jump by pressing jump: impulse = `wallNormal * wallJumpForce + up * wallJumpUp`.

The key insight: wall running is just "temporary grounded state on a tilted surface." The same grounded/ungrounded bool, just derived from a different contact normal.

### 10.5 — Swimming / Volume Triggers

Swimming zones are volume triggers — AABBs (or other shapes) that, when entered, change the movement mode. Inside the water volume: gravity is replaced with buoyancy (upward force toward surface), `movementSpeed` is halved, vertical input controls Y directly. The collision system is unchanged; only the force model changes.

This is how Source (`MOVETYPE_SWIM`) and Unreal (`UCharacterMovementComponent::Swimming`) handle it — same collision, different physics mode.

### 10.6 — Networked Movement (Multiplayer)

In multiplayer, player movement runs on both client and server. The client predicts its own position using local input and does not wait for the server. The server runs the same movement code and authoritatively corrects the client when the prediction drifts.

This is **client-side prediction with server reconciliation** (Valve GoldSrc/Source `cl_predict 1`). The movement code must be deterministic: same inputs, same `dt`, same result. Non-deterministic code (random floats, floating-point order differences across platforms) causes permanent desync.

---

## Part 11 — The Project 3 Movement: What to Add Next

The current code has four known limitations. Fix them in this order to build toward a real character controller:

| Priority | Limitation | Fix |
|----------|-----------|-----|
| 1 | No wall sliding | Replace rollback with per-axis MTV push |
| 2 | No gravity / jump | Add `velocity` vec3 + Euler integration |
| 3 | Corner sticking | Switch player shape to capsule queries |
| 4 | Tunneling at high speed | Add swept AABB time-of-impact check |

Fixing #1 and #2 takes ~30 lines. Those are the right next steps for project 3.

---

## Summary

| Concept | Project 3 approach | Industry approach |
|---------|-------------------|-------------------|
| Movement | Camera `front`/`right` * speed * dt | Same, but project to horizontal plane |
| Y control | Hard clamp to y=1.0 | Gravity + velocity integration |
| Collision shape | AABB box (0.6 × 1.8 × 0.6) | Capsule collider |
| Collision response | Rollback to `oldPos` | MTV push (per axis, sliding) |
| Tunneling | Not handled | Swept AABB / CCD |
| Grounded detection | Implicit (y clamp) | Contact normal from MTV resolve |

The project builds the correct scaffolding — `GameObject` + `AABB` + per-frame loop with `dt`. Every advanced feature in Part 10 plugs into that same loop. The architecture is not wrong, just simplified.
