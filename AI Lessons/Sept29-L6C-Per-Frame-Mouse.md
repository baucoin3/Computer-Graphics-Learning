# Lesson 6C — Per-Frame Mouse Input: Deltas, Sensitivity, and WASD Movement

**Date:** Sept 29, 2026
**Prerequisites:** L6B (Yaw and Pitch Math)
**Goal:** Understand how GLFW mouse callbacks deliver cursor position, how to compute per-frame deltas, and how to wire WASD movement to the camera with correct frame-rate independence.

---

## Why This Exists

You now have a formula that converts (yaw, pitch) into a forward vector. The missing piece is: how does moving the mouse change yaw and pitch? This is not a math problem — it is an API problem. GLFW delivers cursor positions as absolute screen-pixel coordinates. You need to compute the *change* per frame (delta), scale it by a sensitivity factor, and add it to yaw and pitch. Small details — which way Y increases, the firstMouse edge case, cursor locking — determine whether your camera feels correct or broken.

---

## How GLFW Delivers Mouse Position

GLFW reports cursor position via a callback: a function you register that GLFW calls every time the cursor moves. You register it with:

```cpp
glfwSetCursorPosCallback(window, mouseCallback);
```

Where `mouseCallback` has the signature:

```cpp
void mouseCallback(GLFWwindow* window, double xpos, double ypos);
```

GLFW calls this function automatically when the cursor moves. It is called inside `glfwPollEvents()` — every time you call PollEvents in your render loop, any accumulated cursor movement events fire their callbacks synchronously before PollEvents returns.

**xpos:** horizontal cursor position in screen pixels. Increases left to right. (0,0) is the top-left corner of the window.

**ypos:** vertical cursor position in screen pixels. **Increases top to bottom.** This is opposite to OpenGL's Y convention (which increases bottom to top). It matters when computing pitch delta.

---

## The firstMouse Problem

The first time your program runs, the callback fires immediately with the cursor's current screen position — which could be (400, 300) or (0, 0) or any value. If you compute delta on the first call using an uninitialized `lastX, lastY`, you get a massive delta, and the camera makes a violent jump.

Fix: track whether this is the first callback call.

```cpp
float lastX = 400.0f;  // initialize to window center as a reasonable default
float lastY = 300.0f;
bool firstMouse = true;

void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = (float)xpos;
        lastY = (float)ypos;
        firstMouse = false;
        return;   // skip this frame — no delta to compute yet
    }

    float deltaX = (float)xpos - lastX;
    float deltaY = lastY - (float)ypos;  // note: lastY - ypos, not ypos - lastY

    lastX = (float)xpos;
    lastY = (float)ypos;

    // Apply to yaw and pitch (covered below)
}
```

Why `lastY - ypos` for deltaY and not `ypos - lastY`?

GLFW Y increases downward on screen. Moving the mouse upward decreases ypos (you move toward a smaller Y value). If you compute `ypos - lastY`, moving up gives a negative delta. If you want "mouse up = look up = positive pitch", you need to negate: `lastY - ypos` produces a positive value when moving the mouse up. This matches the intuitive expectation.

Worked numeric example: lastX = 400, lastY = 300. Mouse moves 10 pixels to the right and 5 pixels up.
```
New position: xpos = 410, ypos = 295

deltaX = 410 - 400 = +10  (rightward, positive)
deltaY = 300 - 295 = +5   (upward, positive — because lastY(300) > ypos(295))
```

Mouse moves 10 pixels left and 8 pixels down:
```
New position: xpos = 390, ypos = 308

deltaX = 390 - 400 = -10  (leftward, negative)
deltaY = 300 - 308 = -8   (downward, negative)
```

---

## Sensitivity and Applying Deltas

Scale the pixel deltas by a sensitivity factor (in degrees per pixel) before adding to yaw and pitch:

```cpp
float sensitivity = 0.1f;  // 0.1 degrees per pixel of mouse movement

float yawDelta   = deltaX * sensitivity;
float pitchDelta = deltaY * sensitivity;

yaw   += yawDelta;
pitch += pitchDelta;

// Clamp pitch to prevent gimbal lock (from L6B)
if (pitch >  89.0f) pitch =  89.0f;
if (pitch < -89.0f) pitch = -89.0f;
```

Worked numeric example: mouse moved 25 pixels right in one frame, 10 pixels up.

```
deltaX = 25, deltaY = 10, sensitivity = 0.1

yawDelta   = 25 * 0.1 = 2.5 degrees
pitchDelta = 10 * 0.1 = 1.0 degree

New yaw   = old_yaw + 2.5
New pitch = old_pitch + 1.0
```

If the camera was at yaw = -90° (looking along -Z) and pitch = 0°, after this frame:
- yaw = -87.5° (slightly rotated toward -X direction)
- pitch = 1.0° (very slightly looking up)

After updating yaw and pitch, call `updateVectors()` (see L6D) to recompute `front`, `right`, and `up`.

**Sensitivity tuning:** 0.1 degrees/pixel means moving the mouse 180 pixels horizontally rotates the camera 18°. Most FPS games have sensitivities ranging from 0.05 to 0.3 degrees/pixel. Lower = more precise aiming, higher = faster rotation.

---

## Cursor Locking with glfwSetInputMode

For an FPS camera, you need the cursor invisible and locked so it never hits the screen edge (which would prevent further movement):

```cpp
glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
```

This does three things:
1. Hides the cursor visually
2. Centers the cursor in the window
3. Allows unlimited cursor movement — the cursor is "captured" and can move infinitely in any direction, reported in xpos/ypos as if the cursor were never constrained

With GLFW_CURSOR_DISABLED, `xpos` and `ypos` in the callback can go far outside the window bounds (e.g., xpos = 5000 after the player has rotated a lot). This is correct behavior. Only the delta between frames matters, not the absolute position.

`GLFW_CURSOR_NORMAL` — default, cursor visible and can leave the window.
`GLFW_CURSOR_HIDDEN` — cursor invisible but not captured; still constrained to screen bounds.
`GLFW_CURSOR_DISABLED` — captured, invisible, unlimited movement.

---

## WASD Keyboard Input

GLFW keyboard input is polled, not callback-based (for held keys). Check key state in the render loop:

```cpp
void processKeyboard(GLFWwindow* window, float deltaTime, Camera& camera) {
    float speed = camera.movementSpeed * deltaTime;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.position += camera.front * speed;

    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.position -= camera.front * speed;

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.position -= camera.right * speed;

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.position += camera.right * speed;
}
```

`glfwGetKey(window, GLFW_KEY_W)` returns `GLFW_PRESS` while the key is held and `GLFW_RELEASE` when it is not. This is checked every frame — as long as the key is held, the camera moves that frame.

W/S move along `camera.front` — the direction the camera is looking. A/D move along `camera.right` — perpendicular to the viewing direction.

**Ground-locked movement variant:** if you want W/S to move along the ground plane (not fly upward when looking up), project `front` onto the horizontal plane before using it:

```cpp
glm::vec3 horizontalFront = glm::normalize(glm::vec3(camera.front.x, 0.0f, camera.front.z));

if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    camera.position += horizontalFront * speed;
if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    camera.position -= horizontalFront * speed;
```

Setting `y=0` and renormalizing gives a vector in the XZ plane only. This is how a standard FPS character walks: pressing W on a slope does not launch you into the air, it moves you forward along the ground. Pure free-fly cameras (no gravity) use `camera.front` directly.

---

## Frame-Rate Independence with deltaTime

Movement speed must be multiplied by deltaTime so the camera moves the same total distance per second regardless of framerate.

**Why this matters — worked numeric example:**

movementSpeed = 5.0 units/second.

At 60fps: deltaTime = 1/60 = 0.01667 seconds.
Movement per frame = 5.0 * 0.01667 = 0.0833 units.
Distance in 1 second = 60 frames * 0.0833 = 5.0 units. ✓

At 144fps: deltaTime = 1/144 = 0.00694 seconds.
Movement per frame = 5.0 * 0.00694 = 0.0347 units.
Distance in 1 second = 144 frames * 0.0347 = 4.997 ≈ 5.0 units. ✓

**Without deltaTime at 60fps:** movement per frame = 5.0 units.
Distance in 1 second = 60 * 5.0 = 300 units. The camera flies across the scene in one second.

**Without deltaTime at 144fps:** 144 * 5.0 = 720 units per second. A 144fps player moves 2.4× faster than a 60fps player — completely broken for multiplayer, unfair for any timing-dependent gameplay.

---

## Computing deltaTime in the Render Loop

```cpp
float lastFrame = 0.0f;

// In the render loop:
float currentFrame = (float)glfwGetTime();
float deltaTime    = currentFrame - lastFrame;
lastFrame          = currentFrame;
```

`glfwGetTime()` returns seconds since GLFW initialization, as a double. Cast to float for your calculations (sufficient precision — you do not need nanosecond precision for frame timing).

`deltaTime` on frame 0: `lastFrame = 0.0f`, `currentFrame = time since init` (typically a few milliseconds). The first deltaTime is the time from init to the first rendered frame, not a full frame duration. This causes a small position jump on the first frame if the init time is significant (e.g., loading assets). Usually negligible but worth knowing.

---

## Putting It Together: Render Loop Structure

```cpp
// Before render loop:
glfwSetCursorPosCallback(window, mouseCallback);
glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
float lastFrame = 0.0f;

// Render loop:
while (!glfwWindowShouldClose(window)) {
    // --- Timing ---
    float currentFrame = (float)glfwGetTime();
    float deltaTime    = currentFrame - lastFrame;
    lastFrame          = currentFrame;

    // --- Input (keyboard polled here) ---
    processKeyboard(window, deltaTime, camera);

    // --- Clear + render ---
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    shader.use();
    glUniformMatrix4fv(locView, 1, GL_FALSE, glm::value_ptr(camera.getViewMatrix()));
    glUniformMatrix4fv(locProj, 1, GL_FALSE, glm::value_ptr(camera.getProjectionMatrix()));

    // draw objects...

    glfwSwapBuffers(window);
    glfwPollEvents();   // mouse callback fires here
}
```

Note: `glfwPollEvents()` is where the mouse callback fires. Placing it at the end of the loop means mouse input is processed at the start of the *next* frame. You could put it at the beginning of the loop instead to process input before the current frame's render — one frame less of input latency. Both orderings work; most tutorials put PollEvents at the end.

---

## The Static Callback Problem

GLFW callbacks are C function pointers. A C function pointer cannot be a non-static member function (because member functions have an implicit `this` parameter that C function pointers do not support).

**Option A: Global camera pointer**

```cpp
Camera* gCamera = nullptr;   // global

void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
    if (!gCamera) return;
    // compute delta...
    gCamera->processMouseMovement(deltaX, deltaY);
}

// In main:
Camera camera;
gCamera = &camera;
glfwSetCursorPosCallback(window, mouseCallback);
```

Simple, works. The global pointer is mildly inelegant but fine for a single camera program.

**Option B: Window user pointer**

```cpp
glfwSetWindowUserPointer(window, &camera);

void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
    Camera* cam = static_cast<Camera*>(glfwGetWindowUserPointer(window));
    if (!cam) return;
    // compute delta...
    cam->processMouseMovement(deltaX, deltaY);
}
```

`glfwSetWindowUserPointer` stores a void* on the window. `glfwGetWindowUserPointer` retrieves it. No global variable. Cleaner for class-based designs with multiple windows. `static_cast<Camera*>` is safe here because we know what we stored.

---

## Pitfalls

**Computing `ypos - lastY` instead of `lastY - ypos`.** Mouse up = ypos decreases (screen Y increases downward). `ypos - lastY` with upward movement gives a negative delta. If you add this to pitch, moving the mouse up decreases pitch (camera looks down). The camera is inverted. Fix: `lastY - ypos` for correct upward = positive pitch. Alternatively, negate pitch addition: `pitch -= deltaY`.

**Not setting `firstMouse = false` before returning.** If you return inside the firstMouse block without setting `firstMouse = false`, the camera resets every frame. No movement is ever computed. Symptom: camera completely frozen.

**Using `GLFW_CURSOR_HIDDEN` instead of `GLFW_CURSOR_DISABLED`.** HIDDEN makes the cursor invisible but still constrains it to the window. When the cursor hits the window edge, it stops moving in that direction. Rotating the camera against the window edge stops after a few degrees. DISABLED is required for unlimited rotation.

**Forgetting to update `lastX, lastY` in the firstMouse branch.** If lastX and lastY keep their initial values (e.g., 400, 300) and the cursor is actually at (50, 250), the first computed delta will be (-350, 50) — a huge jump. Setting lastX/lastY in the firstMouse branch and returning prevents this.

**deltaTime = 0 on first frame.** If currentFrame and lastFrame are both 0.0 on frame 0, deltaTime = 0. Speed * 0 = 0 movement. Not a bug per se — just zero movement on frame 0. Harmless.

---

## Industry Context

**Unreal Engine:** mouse input goes through `APlayerController::AddYawInput()` and `AddPitchInput()`. The engine accumulates input per frame and applies it to the controller rotation. Sensitivity is a per-player setting. Mouse polling happens in the engine's input processing subsystem, separated from rendering. Mouse input is processed before the physics tick, before the render.

**Unity:** `Input.GetAxis("Mouse X")` returns mouse delta in Unity units (already sensitivity-adjusted). The raw underlying call is polling the OS-level raw mouse input, similar to GLFW's GLFW_CURSOR_DISABLED. Unity's standard FPS controller uses exactly the yaw/pitch decomposition described in L6B.

**Raw mouse input (GLFW_RAW_MOUSE_MOTION):** by default, GLFW applies OS-level mouse acceleration curves (pointer ballistics). `glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE)` disables OS acceleration and reports physical hardware counts directly. Recommended for any game that wants consistent feel across different mouse hardware and OS settings. Only available when cursor is disabled.

Next: L6D implements the complete updated Camera.h and wires everything into project 03's main.cpp.
