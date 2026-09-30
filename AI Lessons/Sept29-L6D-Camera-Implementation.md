# Lesson 6D — Free-Fly Camera: Full Implementation for Project 03

**Date:** Sept 29, 2026
**Prerequisites:** L6C (Per-Frame Mouse)
**Goal:** Implement the complete free-fly Camera.h for project 03 with yaw/pitch orientation, mouse movement processing, WASD keyboard movement, and full integration into the render loop.

---

## Why This Exists

You now have all the theory: yaw/pitch → forward vector (L6B), mouse delta → yaw/pitch update (L6C), keyboard → position update (L6C). This lesson assembles everything into a clean, self-contained Camera.h that replaces the static Camera.h from project 02 and integrates into project 03's structure.

---

## Complete Updated Camera.h

```cpp
#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Config.h"

class Camera {
public:
    // --- State ---
    glm::vec3 position;
    float     yaw;        // degrees, horizontal rotation around world Y
    float     pitch;      // degrees, vertical tilt, clamped to [-89, 89]

    // --- Derived (recomputed by updateVectors) ---
    glm::vec3 front;      // normalized direction camera is looking
    glm::vec3 right;      // normalized right direction
    glm::vec3 up;         // camera's up direction (not world up)

    // --- Projection parameters ---
    float fovDegrees;
    float nearPlane;
    float farPlane;

    // --- Movement settings ---
    float movementSpeed;
    float mouseSensitivity;

    Camera(
        glm::vec3 startPosition = glm::vec3(0.0f, 1.0f, 5.0f),
        float     startYaw      = -90.0f,
        float     startPitch    =   0.0f
    )
        : position(startPosition)
        , yaw(startYaw)
        , pitch(startPitch)
        , fovDegrees(45.0f)
        , nearPlane(0.1f)
        , farPlane(100.0f)
        , movementSpeed(5.0f)
        , mouseSensitivity(0.1f)
    {
        updateVectors();   // derive front, right, up from initial yaw/pitch
    }

    // --- Matrices ---

    glm::mat4 getViewMatrix() const {
        return glm::lookAt(position, position + front, up);
    }

    glm::mat4 getProjectionMatrix() const {
        return glm::perspective(
            glm::radians(fovDegrees),
            static_cast<float>(SCREEN_WIDTH) / SCREEN_HEIGHT,
            nearPlane,
            farPlane
        );
    }

    // --- Input processing ---

    void processMouseMovement(float xoffset, float yoffset, bool constrainPitch = true) {
        xoffset *= mouseSensitivity;
        yoffset *= mouseSensitivity;

        yaw   += xoffset;
        pitch += yoffset;

        if (constrainPitch) {
            if (pitch >  89.0f) pitch =  89.0f;
            if (pitch < -89.0f) pitch = -89.0f;
        }

        updateVectors();
    }

    void processKeyboard(GLFWwindow* window, float deltaTime) {
        float speed = movementSpeed * deltaTime;

        // Ground-locked movement: project front onto XZ plane so
        // pressing W/S doesn't make the camera fly when looking up/down.
        glm::vec3 horizontalFront = glm::normalize(glm::vec3(front.x, 0.0f, front.z));

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            position += horizontalFront * speed;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            position -= horizontalFront * speed;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            position -= right * speed;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            position += right * speed;

        // Optional: vertical movement on Space / Ctrl
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
            position += glm::vec3(0.0f, 1.0f, 0.0f) * speed;
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
            position -= glm::vec3(0.0f, 1.0f, 0.0f) * speed;
    }

private:
    void updateVectors() {
        glm::vec3 newFront;
        newFront.x = cos(glm::radians(pitch)) * sin(glm::radians(yaw));
        newFront.y = sin(glm::radians(pitch));
        newFront.z = -cos(glm::radians(pitch)) * cos(glm::radians(yaw));

        front = glm::normalize(newFront);
        right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
        up    = glm::normalize(glm::cross(right, front));
    }
};
```

### Line-by-Line Explanation

**`position, yaw, pitch`** — the three primary state variables. Everything else is derived from these.

**`front, right, up`** — the camera's three axis vectors in world space. Recomputed by `updateVectors()` every time yaw or pitch changes. They are cached to avoid recomputing trig functions in every frame's keyboard/view matrix calls.

**`yaw = -90.0f` default** — at yaw = -90°:
```
sin(-90°) = -1, cos(-90°) = 0
front.x = cos(0°) * sin(-90°) = 1 * (-1) = -1? No wait — let me recalculate.
```
Recalculate carefully with yaw = -90°, pitch = 0°:
```
front.x = cos(0°) * sin(-90°) = 1.0 * (-1.0) = -1.0   -- Hmm.
```

Wait — that points along -X, not -Z. Let me re-examine.

With the formula from L6B: `forward.z = -cos(pitch) * cos(yaw)`.
At yaw = 0°: `forward.z = -1.0 * 1.0 = -1.0`. Camera looks along -Z. ✓

So at yaw = -90°:
```
front.x = cos(0°) * sin(-90°) = 1.0 * (-1.0) = -1.0
front.y = sin(0°) = 0.0
front.z = -cos(0°) * cos(-90°) = -1.0 * 0.0 = 0.0
```

That gives front = (-1, 0, 0), pointing along -X. That is NOT along -Z.

**Correction:** In the derivation in L6B, yaw = 0 was defined as looking along -Z. That means at yaw = 0, the camera looks down -Z. Starting with yaw = -90° would make the camera look along -X initially, not along -Z.

The correct starting yaw to look down -Z is **yaw = 0°** using the formula from L6B, or **yaw = -90°** if using an alternative convention where yaw = 90° is +Z-forward.

The convention choice affects only the starting value. LearnOpenGL (the canonical tutorial) uses the convention where at yaw = -90° the camera looks along -Z, which implies: `front.z = cos(pitch) * cos(yaw)` (no negation, different convention). Since this lesson follows the L6B formula where `front.z = -cos(pitch) * cos(yaw)`, the starting yaw should be **0°** for -Z forward, not -90°.

**Update Camera.h startYaw to 0.0f in the constructor.** The L6B derivation: at yaw=0 → looks along -Z. Use startYaw=0.0f. Verify:
```
front.x = cos(0°) * sin(0°) = 0
front.y = sin(0°) = 0
front.z = -cos(0°) * cos(0°) = -1
```
front = (0, 0, -1). Camera looks along -Z. ✓

Change the constructor default: `float startYaw = 0.0f`.

**`updateVectors()` called in constructor** — if you forget this, `front`, `right`, and `up` are uninitialized zero vectors. The first `getViewMatrix()` call passes a zero front to lookAt: `glm::lookAt(position, position + (0,0,0), up)` = `glm::lookAt(position, position, up)` — the eye and center are identical. lookAt produces undefined/NaN output. Nothing renders. Always call `updateVectors()` at the end of the constructor.

**`position + front` in getViewMatrix** — `position + front` is the point one unit in front of the camera. This is always a valid, distinct target point as long as front is nonzero (which it always is after updateVectors). lookAt needs eye ≠ center — this is guaranteed.

**`processMouseMovement(xoffset, yoffset)`** — `xoffset` and `yoffset` are raw pixel deltas, computed in the mouse callback (L6C). Multiply by sensitivity, add to angles, clamp pitch, call updateVectors.

**Horizontal front for W/S** — `glm::normalize(glm::vec3(front.x, 0.0f, front.z))` extracts only the XZ components of the front vector and renormalizes. If `front = (0.3, 0.5, -0.8)` (looking 30° up), `horizontalFront = normalize(0.3, 0.0, -0.8) = (0.351, 0, -0.936)`. Moving forward on W uses this ground-plane direction, not the tilted front. This prevents the camera from flying when looking up while pressing W.

When `front.x = 0` and `front.z = 0` (looking straight up or down), `horizontalFront` would be normalize(0,0,0) — undefined. Pitch clamping to ±89° ensures `cos(pitch) >= cos(89°) = 0.0175`, keeping front.x and front.z nonzero (tiny but nonzero).

---

## Wiring Into main.cpp (Project 03)

Two global variables are needed to bridge the GLFW callback and the Camera object:

```cpp
// Global state for mouse callback
Camera* gCamera  = nullptr;
float   gLastX   = 400.0f;
float   gLastY   = 300.0f;
bool    gFirstMouse = true;

void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
    if (gFirstMouse) {
        gLastX = (float)xpos;
        gLastY = (float)ypos;
        gFirstMouse = false;
        return;
    }

    float deltaX =  (float)xpos - gLastX;
    float deltaY =  gLastY - (float)ypos;   // flipped: up = positive

    gLastX = (float)xpos;
    gLastY = (float)ypos;

    if (gCamera)
        gCamera->processMouseMovement(deltaX, deltaY);
}
```

In main(), before the render loop:

```cpp
Camera camera;
gCamera = &camera;

glfwSetCursorPosCallback(window, mouseCallback);
glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
// Optional: raw input for consistent feel across hardware:
if (glfwRawMouseMotionSupported())
    glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

float lastFrame = 0.0f;
```

In the render loop:

```cpp
while (!glfwWindowShouldClose(window)) {
    // Timing
    float currentFrame = (float)glfwGetTime();
    float deltaTime    = currentFrame - lastFrame;
    lastFrame          = currentFrame;

    // Keyboard input
    camera.processKeyboard(window, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Clear
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Upload view and projection — same uniforms as project 02
    shader.use();
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 proj = camera.getProjectionMatrix();
    glUniformMatrix4fv(glGetUniformLocation(shader.id, "uView"), 1, GL_FALSE,
                       glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shader.id, "uProj"), 1, GL_FALSE,
                       glm::value_ptr(proj));
    glUniform3fv(glGetUniformLocation(shader.id, "uViewPos"), 1,
                 glm::value_ptr(camera.position));

    // Draw objects (same as before)
    // ...

    glfwSwapBuffers(window);
    glfwPollEvents();   // fires mouseCallback with accumulated deltas
}
```

The only changes from project 02's render loop:
- Add `deltaTime` computation at the top
- Add `camera.processKeyboard(window, deltaTime)` call
- Remove the static `camera.position` and `camera.target` — now driven by mouse callback
- `uViewPos` is now `camera.position` directly (same as before, just the camera's position)

Everything else — uniform uploads for uModel, uNormalMatrix, uColor, uLightPos — is unchanged.

---

## The GLFW Window User Pointer Alternative

If you prefer no global variables:

```cpp
struct AppState {
    Camera camera;
    float  lastX = 400.0f;
    float  lastY = 300.0f;
    bool   firstMouse = true;
};

AppState app;
glfwSetWindowUserPointer(window, &app);

glfwSetCursorPosCallback(window, [](GLFWwindow* w, double xpos, double ypos) {
    AppState* app = static_cast<AppState*>(glfwGetWindowUserPointer(w));
    if (app->firstMouse) {
        app->lastX = (float)xpos;
        app->lastY = (float)ypos;
        app->firstMouse = false;
        return;
    }
    float dx = (float)xpos - app->lastX;
    float dy = app->lastY - (float)ypos;
    app->lastX = (float)xpos;
    app->lastY = (float)ypos;
    app->camera.processMouseMovement(dx, dy);
});
```

A C++ lambda that captures nothing (empty `[]`) decays to a plain function pointer and is compatible with GLFW's C callback signature. The app state is retrieved via `glfwGetWindowUserPointer`.

---

## Pitfalls

**`updateVectors()` not called in the constructor.** Front, right, and up are uninitialized. The first lookAt call produces NaN matrices. Nothing renders — you see a blank screen with no error messages (NaN matrices just produce black output since transformed positions are NaN). Add `updateVectors()` as the last line of the constructor.

**`startYaw = -90.0f` vs `startYaw = 0.0f` convention confusion.** Whether the default yaw is 0° or -90° depends on the exact formula used. With the L6B formula (`front.z = -cos(pitch)*cos(yaw)`), yaw=0 → looking along -Z. LearnOpenGL uses a different sign convention where -90° is the -Z default. Mixing the two conventions produces a camera pointing the wrong direction by default. Use the formula consistently — do not copy one sign convention from one source and a different default from another.

**Not guarding against front.x=0 and front.z=0 in processKeyboard.** Pitch clamping to ±89° prevents this in practice. But if constrainPitch is disabled or pitch is set programmatically to ±90°, `glm::normalize(glm::vec3(front.x, 0, front.z))` divides by zero. Either keep constrainPitch on or add an explicit guard: check if `glm::length(glm::vec3(front.x, 0, front.z)) > 0.001f` before normalizing.

**Including `<GLFW/glfw3.h>` in Camera.h.** Camera.h calls `glfwGetKey`. This means any file that includes Camera.h must have GLFW linked. For project 03 this is acceptable. In a larger engine, decouple input processing from the Camera class — have main.cpp compute the input vector and pass it to the camera. The camera then has no GLFW dependency.

---

## Industry Context

**Unreal Engine FPS camera:** `ACharacter` with a `UCameraComponent` and `APlayerController`. Horizontal input (yaw) rotates the controller, vertical input (pitch) rotates the camera component attached to it. The input goes through the engine's input binding system (BindAxis "Turn", "LookUp"). Under the hood: the same yaw/pitch decomposition, just wrapped in many layers of abstraction.

**Unity FPS camera:** Standard Assets FPSController (deprecated but widely used) stores pitch in a `float rotationX` variable and applies it to the camera's `localEulerAngles.x`. Yaw is applied to the character body's `eulerAngles.y`. The split between local pitch and world yaw is the same pattern as here.

**DOOM Eternal / Call of Duty:** Raw mouse input (DirectInput or Raw Input API on Windows), no OS pointer acceleration. Sensitivity settings map to a multiplier on the raw mouse delta. The implementation is exactly processMouseMovement: delta * sensitivity → yaw/pitch update → forward vector recompute.

**VR (OpenXR):** you do NOT implement camera orientation for VR. The HMD provides an `XrPosef` (position + quaternion orientation) for each eye each frame from the tracking system. You apply it directly to the view matrix. Any attempt to "drive" the view with mouse look breaks head tracking. The camera position (room-scale offset) may be WASD-driven, but orientation comes from tracking hardware.

Next: the Sept29 camera lessons are complete. Sept30 covers player movement, velocity integration, and collision detection with axis-aligned bounding boxes.
