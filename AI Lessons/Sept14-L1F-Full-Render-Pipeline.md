# L1F — The Full Rendering Pipeline: One Triangle From CPU to Screen

**CPS 511 Bridge Series | Lesson 1F**  
**Prereqs:** L1A (framebuffer, vsync), L1B (TRS matrices, coordinate spaces), L1C (homogeneous coordinates), L1D (normal vectors), L1E (projection, clipping, depth, rasterization)

---

## What This Lesson Is Actually About

You have now studied each stage of the pipeline in isolation. L1B taught you coordinate spaces. L1C taught you homogeneous coordinates. L1E taught you projection, clipping, and rasterization. But these lessons do not yet answer the question that matters most for writing real code:

**What exactly happens to a triangle, step by step, from the moment your CPU describes it to the moment its pixels appear on screen?**

This lesson answers that question completely. We will pick a specific concrete triangle — three vertices with known model-space coordinates — and carry them through every single pipeline stage. Every matrix multiply will be written out. Every coordinate will be computed. Every stage will be explained in terms of what problem it is solving, what the GPU is actually doing, and where this same stage appears in modern engines like Unreal Engine 5 and Vulkan.

At the end, you will have a single mental model of the full pipeline that you can reference every time you write a vertex shader, debug a depth artifact, or wonder why a triangle is not drawing. This is the kind of understanding that separates graphics programmers who can debug from those who can only copy tutorials.

This file has two parts. **Part A** explains every pipeline stage from scratch — why it exists, what it does, with diagrams. Read this first. **Part B** then takes one concrete triangle and carries it through every stage with all numbers computed at each step.

---

## Part A: The Full Render Pipeline — Every Stage Explained From Scratch

---

### Why Does the Pipeline Exist at All?

A 3D scene is a collection of triangles. Each triangle is three points in space. Your monitor is a grid of pixels. The pipeline answers one question for every pixel on the screen: **what color should this be?**

Two fundamental problems make this hard:

**Problem 1: Triangles live in 3D. Pixels live in 2D.**
You need to project 3D geometry onto a flat screen in a way that creates the illusion of depth — things farther away must appear smaller.

**Problem 2: Thousands of triangles can overlap. Multiple triangles can cover the same pixel.**
You need to figure out which triangle is in front at every pixel and show only that one.

Every stage in the pipeline exists to solve one of these problems, or to set up the conditions for a later stage to solve it. Nothing in the pipeline is arbitrary. If you removed any stage, something specific would break.

---

### Stage 0: The CPU — Describing the Scene to the GPU

**Why does this stage exist?**

The GPU starts each frame knowing nothing. It does not know what objects are in the scene, where the camera is, or how to shade anything. The CPU's job is to describe everything the GPU needs before telling it to start.

**Vertex buffers — getting geometry to the GPU**

A triangle is three points. You define them on the CPU as a flat array of floats:

```
float vertices[] = {
   0.0,  1.0,  0.0,   // top vertex
  -1.0, -1.0,  0.0,   // bottom-left vertex
   1.0, -1.0,  0.0,   // bottom-right vertex
};
```

This array lives in CPU RAM. The GPU has its own separate memory (VRAM) and cannot read CPU RAM directly. You must transfer the data. In OpenGL, you upload it to a **Vertex Buffer Object (VBO)** — a region of GPU VRAM. After that transfer, the vertex data lives on the GPU and is ready for the vertex shader.

```
CPU RAM                              GPU VRAM
┌──────────────────────┐             ┌──────────────────────┐
│ float vertices[]     │──(DMA)─────▶│  VBO                 │
│  [ 0, 1, 0,          │             │  [ 0, 1, 0,          │
│   -1,-1, 0,          │             │   -1,-1, 0,          │
│    1,-1, 0 ]         │             │    1,-1, 0 ]         │
└──────────────────────┘             └──────────────────────┘
```

You also tell the GPU how to interpret that flat float array — which floats are position, which are UV, which are normal. This layout description is stored in a **Vertex Array Object (VAO)**.

**Uniforms — camera and transform data**

Vertices describe shape. The GPU also needs to know where the object sits in the world, where the camera is, and what the field of view is. These are sent as **uniforms** — values set once per draw call that are the same for every vertex. The three key uniforms are the Model matrix, View matrix, and Projection matrix. These will be explained in Stage 1.

**The draw call — starting everything**

Once data is on the GPU and uniforms are set, you issue a draw call. In OpenGL: `glDrawArrays(GL_TRIANGLES, 0, 3)`. This is the trigger. The GPU reads this command and begins executing the vertex shader on every vertex simultaneously.

---

### Stage 1: The Vertex Shader — Placing Each Vertex on Screen

**Why does this stage exist?**

Your vertices are defined in the object's own local coordinate system. The top vertex of your triangle is at (0, 1, 0) relative to the triangle's center. The GPU needs to know where this vertex projects onto the screen — a 2D pixel coordinate. Getting there requires three transforms, applied in sequence. These are the M, V, and P of MVP.

The vertex shader runs **once per vertex, fully in parallel** across GPU shader cores. For a scene with 100,000 vertices, 100,000 instances of this shader run simultaneously.

---

#### 1a. The Model Matrix (M) — Placing the Object in the World

Your object's vertices are defined relative to the object's own center. This is **model space**. The model matrix moves the object from model space into the shared scene — **world space**.

The model matrix is a TRS matrix: Translation × Rotation × Scale. You already understand how these work from L1B. The key point here is what this transform accomplishes conceptually: after multiplying by the model matrix, the vertex coordinates are now relative to the world origin, not the object's center.

```
Model Space                           World Space
(triangle centered at origin)         (triangle placed in scene)

     V0 (0, 1, 0)                          V0 (0, 1, -5)
      *                                     *
     /|\              Model matrix         /|\
    / | \             translate(0,0,-5)   / | \
   /  |  \      ─────────────────────▶  /  |  \
V1*──────*V2                         V1*──────*V2
(-1,-1, 0)(1,-1, 0)              (-1,-1,-5) (1,-1,-5)
```

The triangle now lives at z = -5 in the world. The camera is at the origin (z = 0). Negative z is in front of the camera.

---

#### 1b. The View Matrix (V) — The Camera Trick

This is where the pipeline gets unintuitive. Let's answer directly: **what is the camera, and how does the GPU implement it?**

The GPU always renders from the same position: the origin (0, 0, 0), looking straight down the -Z axis. There is no "camera" object in the hardware. The GPU just renders whatever is in front of it.

So how do you put the camera somewhere else? You don't move the camera. You move **the entire world** in the opposite direction.

If you want the camera at position (5, 2, 0), the view matrix translates the entire world by (-5, -2, 0). The camera stays at the origin. Everything else shifts around it.

```
What you want:                What the GPU does:

   Camera at (5, 2, 0)           Camera always at (0,0,0)
         [C]                            [C]
           \                              \
            \  Triangle at               \  Triangle shifted to
             *  (5, 0, -10)               *  (0,-2,-10)
```

If the camera is rotated 30° to the right, the view matrix rotates the world 30° to the left. The camera stays looking down -Z. The world rotates around it.

After the view matrix, all vertices are in **eye space** (also called camera space or view space). Eye space has a fixed convention: camera at origin, looking down -Z, Y points up, X points right. This consistent starting point is what makes projection predictable.

`gluLookAt(eye, center, up)` in legacy OpenGL (or `glm::lookAt` in modern code) constructs this matrix from the camera's position, where it's pointing, and which direction is up. It computes the inverse of the camera's own transform — place the camera, then invert — and that inverse is the view matrix.

---

#### 1c. The Projection Matrix (P) — Making 3D Look 3D

After the view matrix, you have 3D points in eye space. You need to arrive at 2D screen positions that create the illusion of depth. The projection matrix handles this.

**The core insight: things farther away should appear smaller.**

A person 10 feet away looks taller than the same person 100 feet away. A road's lane lines converge toward the horizon. This is perspective: projected size is inversely proportional to depth.

Mathematically, if a point is at eye-space position (x, y, z), where z is negative (in front of the camera), its projected screen position should be:

```
projected_x = x / (-z)
projected_y = y / (-z)
```

Divide by depth. Larger -z (farther away) = smaller projected coordinates. That is the entire principle of perspective projection.

**Why can't the matrix just do the division?**

Matrix multiplication is a linear operation — you can multiply and add, but not divide by a variable. Dividing by z (which varies per vertex) is not linear. The projection matrix works around this by storing z inside the **w component** of the output vector, then deferring the division to a dedicated fixed-function hardware stage (Stage 4, the perspective divide). This lets the rest of the pipeline work on the pre-divided values while the frustum tests are still easy.

**The frustum — the camera's visible volume**

The projection also defines the limits of what the camera can see: a minimum depth (near plane), a maximum depth (far plane), and a field of view angle. These four boundaries — near, far, and the FOV cone — form a **frustum**: a truncated pyramid.

```
Top view:

            near    far
             |       |
  Camera  ╱──┤       ├──╲
    [C]──╱   │  FOV  │   ╲
          ╲  │       │   ╱
           ╲─┤       ├──╱
             |       |

Side view:

  Camera ────▶ looking down -Z

      ┌──┐────────────────┐
      │  │                │   ← top of frustum
  [C]─┤  │                │
      │  │                │   ← bottom of frustum
      └──┘────────────────┘
       near               far

  Anything inside = rendered.
  Anything outside = clipped (removed in Stage 3).
```

After the projection matrix, each vertex is in **clip space** — a 4D vector (x, y, z, w). The w component now encodes depth. The actual perspective division (x/w, y/w, z/w) has not happened yet.

---

#### 1d. Putting It Together: The Full MVP Chain

The vertex shader applies all three matrices in sequence. In code, one line does it:

```glsl
gl_Position = uProj * uView * uModel * vec4(aPosition, 1.0);
```

Matrices multiply right-to-left. The rightmost matrix (Model) is applied first. This is correct: first place the object in the world, then reframe the world around the camera, then project.

```
aPosition
(model space, object at its own origin)
     │
     │  × Model matrix
     ▼
world-space position
(object placed in the scene)
     │
     │  × View matrix
     ▼
eye-space position
(camera at origin, looking down -Z)
     │
     │  × Projection matrix
     ▼
clip-space position  ← this is gl_Position
(frustum encoded in w, division deferred)
```

The vertex shader outputs `gl_Position` for every vertex. The GPU stores these and passes them to the next stage.

---

### Stage 2: Primitive Assembly — Grouping Vertices Into Triangles

**Why does this stage exist?**

The vertex shader runs independently on each vertex. A vertex shader core processes vertex 0 and has no idea whether that vertex belongs to triangle 5 or triangle 500. It just transforms one point.

Primitive assembly is the step where the GPU groups the processed vertex stream into geometric primitives — triangles, lines, or points — according to what you specified in the draw call.

`glDrawArrays(GL_TRIANGLES, 0, 9)` tells the GPU: treat every three consecutive vertices as one triangle. Vertices 0,1,2 form triangle 1. Vertices 3,4,5 form triangle 2. And so on.

```
Processed vertex stream:

  V0  V1  V2  V3  V4  V5  V6  V7  V8
  ├─────────┤  ├─────────┤  ├─────────┤
  Triangle 1   Triangle 2   Triangle 3
```

This grouping is necessary before clipping, because clipping works on complete triangles — you cannot clip a triangle edge without knowing all three vertices.

**Winding order and face direction**

The order you specify vertices determines which side of the triangle faces the camera. Counter-clockwise winding (as seen from the front) = front face. Clockwise winding = back face.

```
Front face (CCW):    Back face (CW):
        V0                  V0
       ╱ ▲                 ▼ ╲
      ╱   ╲               ╱   ╲
    V1────▶V2           V2◀────V1

CCW = front face.    CW = back face.
Rendered.            Culled (discarded).
```

The GPU uses winding to perform **back-face culling**: triangles facing away from the camera (back faces of a solid mesh) are discarded before they reach rasterization. For a closed mesh like a sphere, roughly half the triangles face away from the camera at any given time. Culling them halves the shading work.

---

### Stage 3: Clipping — Removing Geometry Outside the Frustum

**Why does this stage exist?**

Two reasons, and the second is critical.

**Reason 1: Invisible geometry wastes work.**
A triangle entirely outside the frustum contributes no pixels. Processing it through rasterization and the fragment shader wastes GPU time.

**Reason 2: Geometry behind the camera causes mathematical disaster.**

Consider a vertex at eye-space z = +2. It is behind the camera (camera looks down -Z, so positive z is behind). The projection matrix sets w_clip = -z_eye = -(+2) = -2. Negative w.

When the perspective divide happens in Stage 4: x_ndc = x_clip / w_clip = x_clip / (-2). A negative denominator flips the sign of every coordinate. Geometry that was to the right appears on the left. Geometry that was above appears below. Everything is wrong.

Clipping removes behind-camera geometry **before** the perspective divide ever runs.

**How clipping works in clip space**

In clip space, a vertex is inside the frustum when these six inequalities all hold:

```
-w ≤ x ≤ w
-w ≤ y ≤ w
-w ≤ z ≤ w
```

These are simple comparisons — no division needed. This is why the pipeline works in clip space before doing the divide. Testing the frustum is easier here than in any other space.

**What happens to a partially-clipped triangle?**

If one or two vertices are outside the frustum, the triangle is cut at the frustum boundary. New vertices are generated exactly where the triangle's edges cross the boundary, with all their attributes (UV, normal, color) linearly interpolated at the intersection point. The result is a smaller polygon that fits inside the frustum.

```
Before clipping:              After clipping:

  V0 (inside)                       V0
    *                                *
   /|                               / \
  / |                              /   \
 /  |           ──────▶           /     \
*   |                            N0     N1   ← new vertices at boundary
V1  |                             \     /
    * V2 (outside)                 *───*
                                   V1
```

For most geometry entirely inside the frustum, clipping does nothing. For geometry that straddles the frustum edge — like a character's arm reaching out of the camera frame — clipping generates the exact visible portion.

---

### Stage 4: Perspective Divide — Actually Doing the Division

**Why does this stage exist?**

The projection matrix encoded the perspective into the w component and deferred the division. Clipping is now done (no dangerous negative-w vertices remain). The hardware performs the division:

```
x_ndc = x_clip / w_clip
y_ndc = y_clip / w_clip
z_ndc = z_clip / w_clip
```

The result is **NDC — Normalized Device Coordinates**. After this:

```
NDC space:

         y = +1
           │
  x = -1 ──┼────────── x = +1      z goes into screen:
           │                        -1 = near plane
           │                        +1 = far plane
         y = -1

All visible geometry is now in [-1, 1] on every axis.
```

- x = -1 is the left edge of the screen. x = +1 is the right edge.
- y = -1 is the bottom. y = +1 is the top.
- z = -1 is the near plane. z = +1 is the far plane.

This is a **fixed-function hardware step**. No shader code runs here. It happens automatically between the geometry stages and the rasterizer.

**Why NDC?**

NDC is resolution-independent. A point at NDC (0.5, 0.5) is in the upper-right quadrant of the screen regardless of whether the window is 800×600 or 3840×2160. The next stage (viewport transform) then maps to actual pixels for whatever specific window size you have.

---

### Stage 5: Viewport Transform — NDC to Pixel Coordinates

**Why does this stage exist?**

NDC is a unit square from -1 to 1. Your window has specific pixel dimensions. The viewport transform maps NDC coordinates to actual pixel coordinates.

```
NDC space:          Screen space (800×800 example):

y=+1 ┌──────┐       pixel y=0   ┌──────────────┐
     │      │                   │              │
     │      │    ──────▶        │              │
     │      │                   │              │
y=-1 └──────┘       pixel y=799 └──────────────┘
  x=-1    x=+1               pixel x=0   pixel x=799
```

Notice: the Y axis flips. NDC Y points up (standard math convention). Screen Y points down (pixel convention — origin is top-left). A point at NDC y=+1 (top of NDC) maps to screen y=0 (top of the window).

The mapping formulas:
```
screen_x = (x_ndc + 1) / 2 × window_width
screen_y = (1 - y_ndc) / 2 × window_height    ← Y flipped
depth    = (z_ndc + 1) / 2                    ← mapped to [0, 1]
```

The depth value (mapped to [0, 1]) is what gets stored in the depth buffer. More on that in Stage 8.

This is also fixed-function hardware. You control only the window dimensions via `glViewport(x, y, width, height)`.

---

### Stage 6: Rasterization — Converting Triangles to Fragments

**Why does this stage exist?**

Everything up to this point operated on vertices — three points defining a triangle. A monitor does not display triangles. It displays pixels. Rasterization bridges these two worlds.

The rasterizer takes a triangle defined by three 2D screen-space points and produces a list of **fragments** — one per pixel whose center falls inside the triangle.

A pixel is a tiny square on screen. It has one exact center point. The rasterizer asks: does that center point fall inside the triangle? If yes, generate a fragment. If no, skip it.

**Step 1: Bounding box**

The rasterizer first finds the smallest axis-aligned rectangle enclosing the triangle — minimum and maximum x and y across all three vertices. This limits the search space. Without it you'd test every pixel on screen against every triangle, which is impossibly slow. The bounding box contains all the triangle's pixels because the triangle cannot extend past its own corner vertices. Pixels in the box corners that miss the triangle fail the next step quickly.

**Step 2: Inside test — edge functions**

For each pixel in the bounding box, the rasterizer runs an **edge function** against all three triangle edges. The edge function is a 2D cross product that returns a signed number — positive if the point is on the inside of that edge, negative if outside. A pixel is inside the triangle only when all three edge functions agree in sign.

This test is three multiplications and two subtractions per edge — very fast, and identical arithmetic applied independently to every pixel, which means the GPU runs it in parallel across many pixels simultaneously using SIMD units.

**Step 3: Attribute interpolation — barycentric coordinates**

Every fragment that passes the inside test needs its own version of the vertex shader outputs: UV coordinates, normals, world-space position, colors — all of it. Most fragments are not sitting on a vertex; they are somewhere in between. The rasterizer blends vertex values in proportion to how close the fragment is to each vertex.

This uses **barycentric coordinates** — three weights (λ0, λ1, λ2) that sum to 1 and express where inside the triangle the fragment sits. A fragment close to V0 gets λ0 near 1 and the other two near 0. A fragment at the center gets λ0 = λ1 = λ2 = 1/3.

Any per-vertex attribute A interpolates as:

```
A_fragment = λ0 * A_V0 + λ1 * A_V1 + λ2 * A_V2
```

You do not write this code. The GPU does it automatically for every `out` variable in your vertex shader matched as `in` in your fragment shader.

One important correction happens here: naive barycentric interpolation in screen space is wrong for perspective projection. UV coordinates that vary linearly in 3D do not vary linearly in 2D after the perspective divide. The GPU corrects this using the 1/w values carried through the pipeline — **perspective-correct interpolation**. Without it, textures skew and swim on surfaces not face-on to the camera. The correction is automatic; you never write it, but you need to know it exists if you ever write a software rasterizer.

After interpolation, always call `normalize(vNormal)` in the fragment shader. Linear interpolation of unit vectors does not produce unit vectors — the result is slightly shorter when the two vertex normals point in different directions. An un-normalized normal produces incorrect lighting.

**Fragment vs Pixel — the critical distinction**

A **fragment** is a candidate pixel, not a committed pixel. It carries:
- Its screen-space (x, y) position
- Its interpolated depth value
- All interpolated vertex shader outputs (UV, normal, world-space position, etc.)

A fragment becomes a final written pixel only if it passes the depth test downstream. Multiple triangles can cover the same screen pixel — each generates its own fragment, runs its own fragment shader, and the depth test decides which one wins.

**This stage is the pipeline's pivot point.**

Before rasterization: the pipeline works on vertices and triangles. Parallelism is per-vertex.

After rasterization: the pipeline works on fragments. Parallelism is per-fragment.

Both halves are massively parallel, but over different things. The rasterizer is the amplifier — a small number of vertices becomes a much larger number of fragments. The quality of the interpolation here determines the quality of everything the fragment shader produces.

---

### Stage 7: The Fragment Shader — Computing a Color for Each Fragment

**Why does this stage exist?**

You have a fragment at a known screen position with interpolated attributes. Now you need to decide: what color is this pixel?

All interesting visual effects live here: lighting, texturing, normal mapping, shadows, reflections, subsurface scattering, PBR materials. The fragment shader is where you implement the Phong model, or any other shading algorithm.

The fragment shader runs **once per fragment, fully in parallel** across GPU cores.

**What it receives:**

The fragment shader automatically gets the interpolated outputs from the vertex shader:

```
Vertex shader declared:    Fragment shader receives:
──────────────────────     ──────────────────────────
out vec3 vNormal;    ──▶   in vec3 vNormal;    (interpolated)
out vec2 vUV;        ──▶   in vec2 vUV;        (interpolated)
out vec3 vFragPos;   ──▶   in vec3 vFragPos;   (interpolated)
```

It also automatically receives from the rasterizer:
- `gl_FragCoord.xy` — screen-space pixel position
- `gl_FragCoord.z` — interpolated depth (the value that goes into the depth buffer)
- `gl_FrontFacing` — whether this fragment came from a front-facing polygon

**What it outputs:**

```glsl
out vec4 fragColor;   // RGBA color for this fragment
```

A minimal fragment shader that outputs solid orange:
```glsl
void main() {
    fragColor = vec4(1.0, 0.5, 0.2, 1.0);
}
```

A diffuse-only Phong shader that uses the interpolated normal and light position:
```glsl
void main() {
    vec3 norm     = normalize(vNormal);       // interpolated normal, renormalized
    vec3 lightDir = normalize(lightPos - vFragPos);
    float diff    = max(dot(norm, lightDir), 0.0);  // Lambert's law
    fragColor     = vec4(objectColor * diff, 1.0);
}
```

The reason Phong shading (computing lighting in the fragment shader) produces smooth results is exactly this: every fragment gets its own interpolated normal and runs the full lighting equation. Gouraud shading computed lighting at vertices and interpolated the resulting colors — a cheaper approach that misses specular highlights between vertices.

The fragment shader can also call `discard` to throw the fragment away entirely — no color written, no depth update. This is how alpha testing (transparency cutout) works.

---

### Stage 8: Depth Test — Resolving Overlapping Geometry

**Why does this stage exist?**

Multiple triangles can cover the same pixel. The one closest to the camera should win. The GPU does not guarantee any particular draw order, and enforcing draw order manually (painter's algorithm) is expensive and breaks for intersecting triangles.

The **depth buffer** (z-buffer) solves this. It is a floating-point image the same resolution as your framebuffer. Each pixel in the depth buffer stores the depth of the closest fragment seen so far at that pixel. At the start of each frame, the depth buffer is cleared to 1.0 (maximum = farthest possible).

When a fragment arrives at the depth test:

```
1. Read stored depth at this pixel:  z_stored
2. Compare:  is z_fragment < z_stored?  (is this fragment closer?)
3a. YES → PASS. Write fragment color to framebuffer. Update z_stored = z_fragment.
3b. NO  → DISCARD. The fragment is thrown away. Nothing changes.
```

Example: two triangles covering the same pixel at the center of the screen.

```
Fragment A: depth = 0.3  (closer to camera)
Fragment B: depth = 0.8  (farther)

Depth buffer starts at 1.0.

Fragment A arrives: 0.3 < 1.0 → PASS. Write orange. Depth → 0.3.
Fragment B arrives: 0.8 < 0.3? NO → DISCARD. Orange stays.

Result: the closer triangle wins. Correct.
```

This works regardless of which order the GPU processes the triangles. Fragment A could arrive after Fragment B and still win because the depth test is based on values, not arrival order.

**Depth precision and z-fighting**

Depth buffer values are not linearly distributed across eye-space depth. The projection non-linearly compresses depth — most of the depth buffer's precision is used near the camera, very little near the far plane. If your near plane is 0.01 and your far plane is 10,000, the far/near ratio is 1,000,000. Two triangles sitting at the same location in the distance may map to identical depth buffer values, causing them to flicker randomly between frames. This is **z-fighting** — a visual shimmering artifact.

Fix: keep near as large as practical. A near of 1.0 with a far of 1000 (ratio 1000) is fine. Near of 0.001 with far of 10,000 (ratio 10,000,000) will fight.

---

### Stage 9: Stencil Test — Masking Rendering to a Region

**Why does this stage exist?**

The stencil buffer is an 8-bit integer image alongside the depth buffer. It lets you mask rendering to specific regions of the screen.

The typical pattern:
1. Render a shape (a mirror, a portal frame, a mask). Write a value (e.g., 1) to the stencil buffer at those pixels.
2. Render the next thing (a reflection, portal content) with the stencil test set to "only pass where stencil = 1."
3. Pixels outside the mask fail the stencil test and are discarded.

Mirror reflection: render the room, stencil the mirror surface, render the reflected scene only where the mirror was drawn.

For basic rendering with no special masking effects, the stencil test is disabled and every fragment passes automatically.

---

### Stage 10: Blending — Transparency

**Why does this stage exist?**

The depth test handles opaque geometry: closest wins, done. Transparent objects are different — a 50% transparent piece of glass should show both the glass color and whatever is behind it, mixed together. You cannot resolve transparency with a depth test alone, because you need the color behind the glass, not just the depth.

Blending mixes the incoming fragment's color with the color already in the framebuffer behind it:

```
output = fragment_color × fragment_alpha + framebuffer_color × (1 - fragment_alpha)
```

For a 50% transparent blue fragment over an orange framebuffer pixel:
```
output = blue × 0.5 + orange × 0.5  =  blue-orange mix
```

For an opaque fragment (alpha = 1.0):
```
output = orange × 1.0 + whatever × 0.0  =  orange
```

Opaque fragments ignore what was in the framebuffer. Correct.

**The order-dependence problem**

Blending reads the current framebuffer color. If the scene behind the glass has not been rendered yet, you blend against black — wrong.

This forces a two-pass rendering rule:
1. Render all opaque geometry first (depth test on, depth writes on, blending off).
2. Render all transparent geometry back-to-front, from farthest to closest (depth test on, depth writes OFF, blending on).

Depth writes are disabled for transparent objects in pass 2 because you want transparent objects to stack correctly on top of each other — if a near-transparent object writes its depth, it blocks farther transparent objects from blending. This sorting and pass separation is something you manage as the programmer. The GPU does not automatically sort transparent geometry.

---

### Stage 11: Framebuffer and Present — Getting the Frame to the Screen

**Why does this stage exist?**

After blending, the fragment's color is written into the **back buffer** — a region of GPU VRAM that is the in-progress frame. You cannot write directly to the display while the monitor is scanning it — that produces tearing (the display shows part of one frame and part of another mid-scan).

The solution is **double buffering**:

```
While GPU renders frame N+1:       When frame N+1 is done:

Front buffer     Back buffer        Front buffer     Back buffer
┌──────────┐    ┌──────────┐        ┌──────────┐    ┌──────────┐
│  frame N │    │ frame N+1│        │ frame N+1│    │  frame N │
│(display) │    │(writing) │  swap  │(display) │    │(writing) │
└──────────┘    └──────────┘ ─────▶ └──────────┘    └──────────┘
```

The swap waits until the monitor's vertical blank interval (the gap between scan frames). This prevents the monitor from reading a partially-written buffer. With V-Sync enabled, this caps your frame rate to the monitor's refresh rate and eliminates tearing. With V-Sync disabled, the GPU swaps immediately — maximum frame rate, possible tearing.

In OpenGL: `glfwSwapBuffers(window)`. In Vulkan: `vkQueuePresentKHR`.

---

### The Complete Chain — How Every Stage Connects

```
CPU Application
  ├─ Upload vertex data to GPU (VBO)
  ├─ Set uniforms: Model, View, Projection matrices
  └─ Issue draw call: glDrawArrays(GL_TRIANGLES, 0, N)
          │
          │ [runs once per vertex, fully parallel]
          ▼
  Vertex Shader
  ├─ Apply Model matrix → world space     (place object in scene)
  ├─ Apply View matrix  → eye space       (reframe world around camera at origin)
  ├─ Apply Projection   → clip space      (encode perspective in w, define frustum)
  └─ Output: gl_Position (clip-space 4D vector)
          │
          ▼
  Primitive Assembly
  ├─ Group vertices into triangles per draw call topology
  └─ Determine front/back face from winding order
          │
          ▼
  Clipping
  ├─ Test: -w ≤ x,y,z ≤ w for each vertex
  ├─ Discard triangles fully outside frustum
  └─ Cut triangles partially outside, generate new boundary vertices
          │
          ▼
  Perspective Divide  [fixed hardware]
  └─ x,y,z ÷ w → NDC (all visible geometry now in [-1, 1])
          │
          ▼
  Viewport Transform  [fixed hardware]
  └─ NDC → pixel coordinates for this window; Y flipped; z → [0,1] depth
          │
          ▼
  Rasterization  [fixed hardware]
  ├─ Triangle (3 screen points) → set of covered pixel positions
  ├─ Interpolate all vertex shader outputs across the triangle surface
  └─ Output: fragments (candidate pixels with interpolated attributes)
  ── GEOMETRY HALF ENDS / PIXEL HALF BEGINS ──
          │
          │ [runs once per fragment, fully parallel]
          ▼
  Fragment Shader
  ├─ Receives interpolated UV, normal, position, etc.
  ├─ Computes color: lighting, texture samples, effects
  └─ Outputs: vec4 color (RGBA); or discards
          │
          ▼
  Depth Test  [fixed hardware, configurable]
  ├─ Compare fragment depth to stored depth buffer value
  ├─ Closer → PASS: write color, update depth
  └─ Farther → DISCARD: nothing written
          │
          ▼
  Stencil Test  [optional, off by default]
  └─ Mask-based accept/reject against stencil buffer
          │
          ▼
  Blending  [fixed hardware, configurable]
  ├─ Opaque (alpha=1): overwrite framebuffer
  └─ Transparent: mix fragment color with existing framebuffer color
          │
          ▼
  Back Buffer
  └─ Fragment color written to this frame's in-progress buffer
          │
          │ [swap at vertical blank]
          ▼
  Front Buffer → Monitor → Your Eyes
```

Every stage has exactly one job. The output of each stage is exactly what the next stage needs as input. No stage exists for any other reason.

---

## Part B: The Concrete Numeric Trace — One Triangle Through Every Stage

## The Test Case

Throughout this lesson, we track one triangle.

**Model-space vertices** (the triangle as defined in the asset file, centered at origin):

```
V0 = ( 0,  1,  0)   ← top center
V1 = (-1, -1,  0)   ← bottom left
V2 = ( 1, -1,  0)   ← bottom right
```

Visually in model space:

```
     +y
      |
   V0 *              ← (0, 1, 0)
     /|\
    / | \
   /  |  \
V1*---+---*V2        ← (-1,-1,0) and (1,-1,0)
      |
      +x
```

**Scene setup:**
- Model transform: translate the triangle to position (0, 0, -5) in world space. No rotation, no scaling.
- Camera (view transform): camera sits at the world origin, looking straight down the -Z axis. No rotation.
- Projection: fovy = 90 degrees, aspect ratio = 1.0 (square window), near = 1, far = 100.
- Viewport: 800 × 800 pixel window.

This is deliberately minimal so the math is readable. The concepts scale exactly to any scene.

---

## The Full Pipeline at a Glance

Before we walk through each stage, here is the complete flow. Every named box is a stage we will explain in depth.

```
[CPU Application]
       |
       |  (upload vertex buffer to GPU, issue draw call)
       v
[Vertex Shader]          — runs once per vertex on GPU
       |
       |  (gl_Position in clip space; outputs to next stage)
       v
[Primitive Assembly]     — group vertices into triangles
       |
       v
[Clipping]               — discard/clip geometry outside view frustum
       |
       v
[Perspective Divide]     — divide x,y,z by w → NDC
       |
       v
[Viewport Transform]     — NDC [-1,1]² → pixel coordinates
       |
       v
[Rasterization]          — triangle → list of fragment candidates
       |
       |  (interpolated attributes: depth, UV, normals, etc.)
       v
[Fragment Shader]        — runs once per fragment on GPU
       |
       |  (outputs color, may discard)
       v
[Per-Fragment Tests]     — depth test, stencil test
       |
       v
[Blending]               — alpha blending into framebuffer
       |
       v
[Framebuffer]            — back buffer; swapped at vsync
       |
       v
[Monitor]
```

The stages split naturally into two halves at a boundary nobody names explicitly but everyone feels:

- **Geometry stages** (CPU through rasterization): work on vertices and geometric primitives. The result is a set of fragments (candidates for pixels).
- **Pixel stages** (fragment shader through framebuffer): work on individual fragments. The result is final pixel colors.

This boundary is where the "parallel-per-vertex" GPU model transitions to the "parallel-per-fragment" GPU model. Vertex shaders and fragment shaders are both parallel — but they are parallel over different things.

---

## Stage 0: The CPU Application — Defining Geometry and Issuing a Draw Call

### What problem does this stage solve?

The GPU has no idea what a "triangle" is until you tell it. The CPU's job in the pipeline is to describe the scene to the GPU: what geometry exists, what shaders to run, what textures to use, what matrices to apply. Then it issues a command — the draw call — that tells the GPU to start working.

### How geometry gets to the GPU

A triangle is defined by vertices. Each vertex is a bundle of attributes: position, normal, texture coordinate (UV), tangent, color, etc. For our test triangle, we only care about position.

On the CPU, you build a **vertex buffer** — a flat array of floats:

```
float vertices[] = {
    // x     y     z
     0.0f,  1.0f,  0.0f,    // V0
    -1.0f, -1.0f,  0.0f,    // V1
     1.0f, -1.0f,  0.0f,    // V2
};
```

This is sitting in CPU RAM. The GPU cannot see CPU RAM directly (recall L1A: GPU and CPU have separate memory). You must upload it.

In modern OpenGL:
```c
GLuint vbo;
glGenBuffers(1, &vbo);
glBindBuffer(GL_ARRAY_BUFFER, vbo);
glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
```

`glBufferData` initiates a DMA transfer: the driver queues a copy from your CPU-side array into a region of GPU VRAM. After this call, the vertex data lives in GPU memory and is ready for the vertex shader.

In Vulkan, this is more explicit: you create a `VkBuffer`, allocate `VkDeviceMemory`, call `vkMapMemory` / `memcpy` / `vkUnmapMemory`, and optionally do a staging buffer copy for DEVICE_LOCAL memory. The concept is identical — move vertex data from CPU RAM to GPU VRAM — but Vulkan makes the mechanism visible.

You also need to tell the GPU how to read that flat float array as structured vertex data. In modern OpenGL, a **Vertex Array Object (VAO)** stores this layout:

```c
GLuint vao;
glGenVertexArrays(1, &vao);
glBindVertexArray(vao);

glEnableVertexAttribArray(0);          // attribute location 0 = position
glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
```

This says: "attribute 0 is 3 floats, stride is 12 bytes, starting at offset 0." The GPU uses this layout table to unpack the float array into per-vertex attribute values when the vertex shader reads `layout(location = 0) in vec3 position`.

### Uniforms

The model matrix, view matrix, and projection matrix are passed separately as **uniforms** — values set once per draw call that are the same for every vertex. In the application:

```c
// Build matrices (using GLM)
glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, -5));
glm::mat4 view  = glm::mat4(1.0f);   // identity — camera at origin
glm::mat4 proj  = glm::perspective(glm::radians(90.0f), 1.0f, 1.0f, 100.0f);

glUniformMatrix4fv(glGetUniformLocation(shader, "uModel"), 1, GL_FALSE, glm::value_ptr(model));
glUniformMatrix4fv(glGetUniformLocation(shader, "uView"),  1, GL_FALSE, glm::value_ptr(view));
glUniformMatrix4fv(glGetUniformLocation(shader, "uProj"),  1, GL_FALSE, glm::value_ptr(proj));
```

### The draw call

```c
glUseProgram(shaderProgram);
glBindVertexArray(vao);
glDrawArrays(GL_TRIANGLES, 0, 3);
```

`glDrawArrays(GL_TRIANGLES, 0, 3)` says: "start reading vertices at index 0, read 3 of them, treat every group of 3 as a triangle." The driver translates this into a GPU command buffer entry. The GPU begins executing the pipeline at the vertex shader for vertex 0, 1, and 2.

### Where in Unreal / Vulkan this lives

In Unreal Engine 5, this stage is the rendering thread building a `FMeshDrawCommand` and submitting it via `FRHICommandList`. The draw call maps to `RHICmdList.DrawIndexedPrimitive(...)` which eventually becomes a `vkCmdDrawIndexed` call in the Vulkan backend. The VAO concept maps to Vulkan's `VkVertexInputBindingDescription` and `VkVertexInputAttributeDescription` in the pipeline state object.

---

## Stage 1: The Vertex Shader — Transforming Geometry into Clip Space

### What problem does this stage solve?

The GPU has your vertices in model space — coordinates defined relative to the model's own origin. But you need to know where those vertices project onto the screen. The vertex shader's only mandatory job is to compute `gl_Position` — the clip-space position of each vertex.

That transformation is the MVP chain you studied in L1B and L1E:

```
Model space → (Model matrix) → World space → (View matrix) → Eye space → (Projection matrix) → Clip space
```

The vertex shader runs **once per vertex, fully in parallel**. The GPU fires up thousands of shader cores and each one handles one vertex simultaneously.

### The vertex shader code

```glsl
#version 330 core

layout(location = 0) in vec3 aPosition;   // input: model-space position

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

void main() {
    gl_Position = uProj * uView * uModel * vec4(aPosition, 1.0);
}
```

That one line does everything. But let us actually compute it for all three vertices.

### The matrices for our test scene

**Model matrix** — translate by (0, 0, -5):

```
M = [ 1  0  0   0 ]
    [ 0  1  0   0 ]
    [ 0  0  1  -5 ]
    [ 0  0  0   1 ]
```

**View matrix** — identity (camera at origin, no rotation):

```
V = [ 1  0  0  0 ]
    [ 0  1  0  0 ]
    [ 0  0  1  0 ]
    [ 0  0  0  1 ]
```

**Projection matrix** — fovy=90°, aspect=1, near=1, far=100:

```
f = 1 / tan(fovy/2) = 1 / tan(45°) = 1 / 1.0 = 1.0

P = [ f/aspect   0      0             0          ]
    [ 0          f      0             0          ]
    [ 0          0   -(far+near)/(far-near)   -2*far*near/(far-near) ]
    [ 0          0     -1             0          ]

Substituting f=1, aspect=1, near=1, far=100:

(far+near)/(far-near) = 101/99 ≈ 1.0202
2*far*near/(far-near) = 200/99 ≈ 2.0202

P = [ 1   0      0         0     ]
    [ 0   1      0         0     ]
    [ 0   0   -1.0202   -2.0202  ]
    [ 0   0     -1         0     ]
```

The combined MVP = P * V * M = P * M (since V = identity):

```
MVP = P * M

     [ 1  0  0   0  ]   [ 1  0  0   0 ]
   = [ 0  1  0   0  ] * [ 0  1  0   0 ]
     [ 0  0 -1.0202 -2.0202 ]   [ 0  0  1  -5 ]
     [ 0  0 -1   0  ]   [ 0  0  0   1 ]
```

Matrix-multiply this out. P[row i] * M[col j] for each entry:

```
MVP[0][0] = 1*1 + 0*0 + 0*0 + 0*0 = 1
MVP[0][1] = 0
MVP[0][2] = 0
MVP[0][3] = 0         (P[0] = [1,0,0,0] · M col3 = [0,0,-5,1] = 0)

MVP[1] = [0, 1, 0, 0]   (same structure)

MVP[2][0] = 0*1 + 0*0 + (-1.0202)*0 + (-2.0202)*0 = 0
MVP[2][1] = 0
MVP[2][2] = 0*0 + 0*0 + (-1.0202)*1 + (-2.0202)*0 = -1.0202
MVP[2][3] = 0*0 + 0*0 + (-1.0202)*(-5) + (-2.0202)*1
          = 5.1010 - 2.0202 = 3.0808

MVP[3][0] = 0
MVP[3][1] = 0
MVP[3][2] = -1*1 = -1
MVP[3][3] = -1*(-5) + 0*1 = 5

MVP = [ 1   0      0        0      ]
      [ 0   1      0        0      ]
      [ 0   0   -1.0202    3.0808  ]
      [ 0   0    -1        5       ]
```

### Computing clip-space positions for all three vertices

**V0 = (0, 1, 0, 1):**

```
gl_Position.x = 1*0 + 0*1 + 0*0 + 0*1      = 0
gl_Position.y = 0*0 + 1*1 + 0*0 + 0*1      = 1
gl_Position.z = 0*0 + 0*1 + (-1.0202)*0 + 3.0808*1 = 3.0808
gl_Position.w = 0*0 + 0*1 + (-1)*0 + 5*1   = 5

V0_clip = (0, 1, 3.0808, 5)
```

**V1 = (-1, -1, 0, 1):**

```
gl_Position.x = 1*(-1)    = -1
gl_Position.y = 1*(-1)    = -1
gl_Position.z = (-1.0202)*0 + 3.0808*1 = 3.0808
gl_Position.w = (-1)*0 + 5*1 = 5

V1_clip = (-1, -1, 3.0808, 5)
```

**V2 = (1, -1, 0, 1):**

```
gl_Position.x = 1*(1)     = 1
gl_Position.y = 1*(-1)    = -1
gl_Position.z = 3.0808
gl_Position.w = 5

V2_clip = (1, -1, 3.0808, 5)
```

These are the outputs the vertex shader writes to `gl_Position`. Clip space. The GPU stores them and passes them to the next stage.

Notice: w = 5 for all three vertices. That is correct — they were all at z = -5 in eye space, and w_clip = -z_eye = -(-5) = 5. This w will be used to undo the perspective scaling after clipping.

---

## Stage 2: Primitive Assembly — Grouping Vertices into Triangles

### What problem does this stage solve?

The vertex shader runs independently on each vertex. It does not know whether a vertex belongs to a triangle, a line, or a point. Primitive assembly is the fixed-function step where the GPU takes the stream of processed vertices and groups them into the geometric primitive you requested.

You issued `glDrawArrays(GL_TRIANGLES, 0, 3)`. Primitive assembly takes vertices 0, 1, 2 and groups them into one triangle:

```
Triangle: { V0_clip, V1_clip, V2_clip }
         = { (0,1,3.08,5), (-1,-1,3.08,5), (1,-1,3.08,5) }
```

For a mesh with thousands of triangles, this would be a stream of triangle records passing to the next stage. For our case: one triangle.

Primitive assembly also determines **winding order**. OpenGL's default is counter-clockwise winding = front face. Looking at our triangle from the front:

```
V0 at top center, V1 at bottom-left, V2 at bottom-right.
Traversal V0 → V1 → V2 goes: up → down-left → down-right → back.
That is counter-clockwise. This triangle's front face points toward -Z (toward the camera).
```

The winding order is used in the culling step — triangles with clockwise winding (back faces) are discarded by default (`GL_BACK` face culling). This cuts fragment shading work roughly in half for closed meshes. The rasterizer enforces this after clipping.

### Modern / Vulkan

In Vulkan, you specify `VkPrimitiveTopology` (e.g., `VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST`) in the pipeline state object. `VkPipelineInputAssemblyStateCreateInfo` encodes this. In Unreal, this is encoded in `FMeshDrawCommand::PrimitiveType`.

---

## Stage 3: Clipping — Cutting Geometry at the Frustum Boundary

### What problem does this stage solve?

Not all geometry fits inside the view frustum. A triangle might partially stick outside the frustum boundaries (the sides, near plane, or far plane). If you try to project geometry that is behind the camera (z > 0 in eye space, which means w_clip < 0), the perspective divide inverts coordinates, producing garbage. Clipping must happen before the perspective divide.

The clipping stage tests each triangle against the six frustum planes and clips geometry to produce fragments only from the visible region.

In homogeneous clip space, the six planes are:

```
-w ≤ x ≤ w
-w ≤ y ≤ w
-w ≤ z ≤ w    (OpenGL; z range is [-w, w] before the divide → NDC [-1, 1])
```

This is why homogeneous coordinates matter (L1C): the frustum planes are simple linear inequalities in (x, y, z, w) space, which lets the GPU clip efficiently without doing division first.

**For our test triangle**, check all three vertices:

```
V0_clip = (0, 1, 3.08, 5):
  -5 ≤ 0 ≤ 5   ✓   (x within [-w, w])
  -5 ≤ 1 ≤ 5   ✓   (y within [-w, w])
  -5 ≤ 3.08 ≤ 5 ✓  (z within [-w, w])

V1_clip = (-1, -1, 3.08, 5):
  -5 ≤ -1 ≤ 5  ✓
  -5 ≤ -1 ≤ 5  ✓
  -5 ≤ 3.08 ≤ 5 ✓

V2_clip = (1, -1, 3.08, 5):
  -5 ≤ 1 ≤ 5   ✓
  -5 ≤ -1 ≤ 5  ✓
  -5 ≤ 3.08 ≤ 5 ✓
```

All three vertices are inside the frustum. No clipping occurs. The triangle passes through intact.

### What clipping looks like when it triggers

Suppose V1_clip was (-7, -1, 3.08, 5). Then x = -7, but -w = -5. The vertex is outside the left plane (-w ≤ x violated). The clipping algorithm (Sutherland-Hodgman) intersects the triangle's edges with the left plane and generates new vertices at the intersection points. The original triangle becomes a quad (two triangles) with the offending corner replaced by the clipped edge. All new vertices get interpolated attribute values (UV, normal, color) at the intersection points.

For line clipping (used for debug wireframes), Cohen-Sutherland tests both endpoints and clips to the frustum in one pass. Your slides cover both algorithms. The key insight from both: clipping generates new vertices with linearly interpolated attributes, exactly as if they had been in the original mesh.

### Near-plane clipping: the special case that matters most

The near plane clip (-w ≤ z, i.e., z ≥ -w) deserves special attention. If any vertex has z_eye > 0 (it is behind the camera), w_clip = -z_eye becomes negative. The perspective divide then flips the sign of x and y, placing the geometry on the wrong side of the screen. The near plane clip catches this before it happens. This is why every perspective matrix requires near > 0 — you must clip before dividing.

---

## Stage 4: Perspective Divide — Converting Clip Space to NDC

### What problem does this stage solve?

Perspective projection requires dividing by depth (z) to create foreshortening. The projection matrix encoded the perspective into the w component (w_clip = -z_eye), deferring the actual division. Now, after clipping in homogeneous space, the hardware performs the division:

```
x_ndc = x_clip / w_clip
y_ndc = y_clip / w_clip
z_ndc = z_clip / w_clip
w_ndc = w_clip / w_clip = 1    (discarded)
```

This is a fixed-function hardware step. No shader code runs here. It happens automatically between the geometry stages and the rasterizer.

**For our three vertices:**

**V0:** w_clip = 5
```
x_ndc = 0    / 5 = 0.000
y_ndc = 1    / 5 = 0.200
z_ndc = 3.08 / 5 = 0.616
```

**V1:** w_clip = 5
```
x_ndc = -1   / 5 = -0.200
y_ndc = -1   / 5 = -0.200
z_ndc = 3.08 / 5 =  0.616
```

**V2:** w_clip = 5
```
x_ndc =  1   / 5 = 0.200
y_ndc = -1   / 5 = -0.200
z_ndc =  0.616
```

All three vertices are now in NDC — Normalized Device Coordinates. Every coordinate is in [-1, 1] (confirmed: all values between -0.2 and 0.2 for x/y, and 0.616 for z). The coordinate space is now API-convention-independent: x points right, y points up, z points into the screen (OpenGL convention: z = -1 at near plane, z = +1 at far plane).

Visualizing the NDC positions of the triangle:

```
y = +1.0 ─────────────────────
            *  V0 (0, 0.2)
           /|\
          / | \
         /  |  \
y =  0.0──────────────────────
       V1* ─────* V2          ← both at y = -0.2
     (-0.2,-0.2) (0.2,-0.2)

y = -1.0 ─────────────────────
    x = -1.0       x = +1.0
```

The triangle is small, centered near the top-center of the NDC square. Makes sense: we put a triangle at z=-5 with a 90-degree FOV. At z=-5, the right edge of the frustum is x = 5 (because at 90-degree FOV, tan(45°)=1, so at depth 5, the half-width is 5). Our triangle spans x from -1 to +1 in world space, which maps to -0.2 to +0.2 in NDC — exactly 1/5 of the screen width.

---

## Stage 5: Viewport Transform — NDC to Pixel Coordinates

### What problem does this stage solve?

NDC is a unit cube: [-1, 1] in x and y. Your actual window has a specific pixel size — in our case, 800×800. The viewport transform maps NDC to actual window pixel coordinates.

The formula, as derived in L1E:

```
screen_x = (x_ndc + 1) / 2 * viewport_width  + viewport_x_origin
screen_y = (1 - y_ndc) / 2 * viewport_height + viewport_y_origin
```

The Y axis flip exists because NDC Y points up (standard math convention) but screen Y points down (top-left origin for pixel coordinates on screen). Z gets mapped to the depth buffer range [0, 1] by default:

```
depth = (z_ndc + 1) / 2    →    range [0, 1]
```

For our window (800×800, origin at (0,0)):

**V0_ndc = (0, 0.2, 0.616):**
```
screen_x = (0.000 + 1) / 2 * 800 = 0.5 * 800       = 400.0
screen_y = (1 - 0.200) / 2 * 800 = 0.400 * 800      = 320.0
depth    = (0.616 + 1) / 2       = 1.616 / 2        = 0.808
```

V0 maps to pixel **(400, 320)**, depth 0.808.

**V1_ndc = (-0.2, -0.2, 0.616):**
```
screen_x = (-0.2 + 1) / 2 * 800 = 0.4 * 800 = 320.0
screen_y = (1 - (-0.2)) / 2 * 800 = 0.6 * 800 = 480.0
depth    = 0.808
```

V1 maps to pixel **(320, 480)**, depth 0.808.

**V2_ndc = (0.2, -0.2, 0.616):**
```
screen_x = (0.2 + 1) / 2 * 800 = 0.6 * 800 = 480.0
screen_y = (1 - (-0.2)) / 2 * 800 = 0.6 * 800 = 480.0
depth    = 0.808
```

V2 maps to pixel **(480, 480)**, depth 0.808.

The triangle in screen space:

```
pixel y=320:           V0 at (400, 320)
                          *
                         /|\
                        / | \
                       /  |  \
pixel y=480:   V1 *──────────────* V2
             (320,480)        (480,480)
```

A 160×160 pixel isoceles triangle, centered horizontally on the screen, upper portion of the screen (y=320 in a 800px tall window means roughly 40% from the top). This matches intuition: a small triangle 5 units away with a 90-degree FOV.

---

## Stage 6: Rasterization — Converting the Triangle Into Fragments

### What problem does this stage solve?

You now have three pixel coordinates for the triangle's vertices. But you need to know which of the ~12,000 pixels inside the triangle's bounding box are actually inside the triangle, so you can shade them. Rasterization turns a geometric primitive (a triangle defined by three 2D points) into a set of **fragments** — candidate pixels with interpolated attributes.

### Bounding box and coverage test

The rasterizer first computes the triangle's axis-aligned bounding box in screen space:

```
x: min(400, 320, 480) = 320  to  max(400, 320, 480) = 480
y: min(320, 480, 480) = 320  to  max(320, 480, 480) = 480

Bounding box: x in [320, 480], y in [320, 480] → 161×161 = ~25,921 candidate pixels
```

For each pixel in the bounding box, the rasterizer tests whether the pixel's center point is inside the triangle. The standard test uses **edge functions** (also called the cross-product test).

For a triangle edge from point A to point B, and a test point P:

```
edge(A, B, P) = (B.x - A.x) * (P.y - A.y) - (B.y - A.y) * (P.x - A.x)
```

A point is inside the triangle when all three edge tests have the same sign. For counter-clockwise winding (front face), a point inside gives a positive edge function for each edge.

For a pixel at (400, 400) — the rough center of the triangle:

```
Edge V0→V1: A=(400,320), B=(320,480)
  edge = (320-400)*(400-320) - (480-320)*(400-400)
       = (-80)*(80) - (160)*(0)
       = -6400 - 0 = -6400

Hmm, negative. Let us reconsider winding in screen space.
```

Careful: in screen space, Y is flipped (Y increases downward). Counter-clockwise in NDC becomes clockwise on screen. The GPU's front-face winding test accounts for this. For our triangle, the rasterizer uses the screen-space positions and handles the convention correctly via the sign of the signed area. What matters is that pixels inside get shaded and pixels outside are skipped.

The key insight: the edge function tests are all done in parallel. Modern GPU rasterizers test many pixels simultaneously using SIMD units, discarding those outside the triangle quickly. Only pixels that pass all three edge tests become fragments and proceed to the fragment shader.

### Barycentric coordinates and attribute interpolation

Every fragment inside the triangle gets interpolated values for all the vertex shader outputs. This is done via **barycentric interpolation**.

For a fragment at screen position P inside triangle V0, V1, V2, the barycentric weights (λ0, λ1, λ2) are computed from areas:

```
λ0 = area(P, V1, V2) / area(V0, V1, V2)
λ1 = area(V0, P, V2) / area(V0, V1, V2)
λ2 = area(V0, V1, P) / area(V0, V1, V2)

where λ0 + λ1 + λ2 = 1
```

Any per-vertex attribute A is then interpolated:

```
A_fragment = λ0 * A_V0 + λ1 * A_V1 + λ2 * A_V2
```

**What gets interpolated?** Everything you declare as an `out` in the vertex shader and `in` in the fragment shader:
- Texture coordinates (UV)
- Normal vectors
- Tangent vectors
- Vertex colors
- World-space or eye-space position (for lighting)
- Any custom attribute you output from the vertex shader

**Perspective-correct interpolation**: Naive screen-space barycentric interpolation of depth or UV coordinates is wrong for perspective projection. The denominator w varies across the triangle. The GPU automatically corrects for this using the 1/w values (also passed through the pipeline), which is why you do not need to do anything special — but you should know it is happening. If you ever implement a software rasterizer, perspective-correct interpolation is one of the first things you must add to make textures not skew on rotated polygons.

**Depth interpolation** at each fragment:

The depth buffer value for each fragment is interpolated from the three vertex depths (0.808 for all three in this case, since all vertices are at the same world-space depth). For a more interesting triangle with vertices at different depths, each fragment would get a linearly interpolated depth value.

### Fragments vs Pixels

A **fragment** is not yet a pixel. A fragment is a candidate. It carries:
- Screen-space position (x, y)
- Interpolated depth
- Interpolated vertex shader outputs (UV, normal, etc.)

Whether this fragment actually becomes a pixel depends on the depth test, stencil test, and alpha test downstream. One pixel position can generate multiple fragments per frame if geometry overlaps — the depth test resolves which one wins.

---

## Stage 7: The Fragment Shader — Computing a Color for Each Fragment

### What problem does this stage solve?

You now have a fragment at a known screen position with interpolated attributes (UVs, normals, etc.). The fragment shader decides what color that fragment should be. This is where all the interesting visual effects live: lighting, texturing, normal mapping, shadows, subsurface scattering, PBR materials.

The fragment shader runs **once per fragment, fully in parallel**.

### A minimal fragment shader for our test scene

```glsl
#version 330 core

out vec4 fragColor;

void main() {
    fragColor = vec4(1.0, 0.5, 0.2, 1.0);   // solid orange, fully opaque
}
```

This outputs an orange color for every fragment. Every pixel covered by the triangle will be orange (subject to the depth test).

### A Phong lighting fragment shader

In a real scene, you would compute lighting. Here is a diffuse-only Phong shader using the attributes the vertex shader interpolated:

```glsl
#version 330 core

in vec3 vNormal;        // interpolated normal (world space)
in vec3 vFragPos;       // interpolated fragment position (world space)

out vec4 fragColor;

uniform vec3 uLightPos;
uniform vec3 uLightColor;
uniform vec3 uObjectColor;

void main() {
    vec3 norm     = normalize(vNormal);
    vec3 lightDir = normalize(uLightPos - vFragPos);
    float diff    = max(dot(norm, lightDir), 0.0);

    vec3 ambient  = 0.1 * uLightColor;
    vec3 diffuse  = diff * uLightColor;

    fragColor = vec4((ambient + diffuse) * uObjectColor, 1.0);
}
```

The vertex shader must output `vNormal` and `vFragPos` for this to work:

```glsl
// vertex shader additions
out vec3 vNormal;
out vec3 vFragPos;

void main() {
    gl_Position = uProj * uView * uModel * vec4(aPosition, 1.0);
    vNormal   = mat3(transpose(inverse(uModel))) * aNormal;   // normal matrix
    vFragPos  = vec3(uModel * vec4(aPosition, 1.0));          // world-space pos
}
```

The **normal matrix** (`transpose(inverse(model))`) is required because normals do not transform the same way as positions under non-uniform scale (see L1D for the full derivation). If you just multiply a normal by the model matrix, scaling distorts it.

### GLSL built-in inputs to the fragment shader

The fragment shader automatically receives from the rasterizer (without you declaring them):

- `gl_FragCoord` — the screen-space x, y coordinates of the fragment, plus the interpolated depth in z. (The depth value here is the same one that goes into the depth buffer, 0.808 for our test case.)
- `gl_FrontFacing` — bool: true if this fragment came from a front-face polygon.

### Industry relevance

In Unreal Engine 5, the fragment shader is the **material shader**. When you author a material in the Material Editor, Unreal compiles your node graph into HLSL (`float4 PS_Main(...)`) that runs as a pixel shader (DirectX terminology for fragment shader). The G-buffer pass, the lighting pass, the translucency pass — each runs different pixel shaders on different sets of fragments. In Vulkan, your GLSL fragment shader is compiled to SPIR-V binary and handed to `vkCreateGraphicsPipeline` as part of `VkPipelineShaderStageCreateInfo`.

---

## Stage 8: Per-Fragment Tests — Depth Test, Stencil Test

### What problem does these stages solve?

Multiple triangles can cover the same pixel. If triangle A is closer to the camera than triangle B, triangle A should win. You cannot guarantee the GPU processes triangles in front-to-back order (it often does not, for efficiency). The depth test resolves overlaps correctly regardless of draw order.

### The Depth Test

Every pixel in the framebuffer has a corresponding entry in the **depth buffer** (z-buffer), a floating-point texture covering the same resolution. Initialized to 1.0 (farthest possible depth) at the start of each frame.

When a fragment arrives at depth test:

1. Read the current depth buffer value at (x, y): `z_current`
2. Compare fragment's interpolated depth `z_frag` against `z_current`
3. If `z_frag < z_current` (fragment is closer): **pass** — write the fragment's color to the color buffer, update depth buffer to `z_frag`
4. If `z_frag >= z_current` (fragment is farther or equal): **fail** — discard the fragment. Color buffer unchanged.

For our test triangle, all fragments have depth ≈ 0.808. If nothing else has been drawn, `z_current` = 1.0 everywhere, so all fragments pass. The depth buffer is updated to 0.808 for every pixel the triangle covers.

Now if you draw a second triangle at depth 0.5 (closer to camera), its fragments have `z_frag` = 0.5 < 0.808 = `z_current`, so they pass the depth test and overwrite the first triangle's colors. Correct hidden-surface removal, no painter's algorithm needed.

The depth test comparison function is configurable: `glDepthFunc(GL_LESS)` is the default. `GL_LEQUAL` allows equal-depth fragments to overwrite (used for stencil shadow passes). `GL_ALWAYS` disables depth testing (used for UI elements you always want on top).

### Depth precision and Z-fighting

Depth buffer values are not linear in eye-space depth. The perspective matrix maps eye-space z non-linearly into clip-space z, which maps non-linearly into the [0,1] depth buffer range. The result: most of the depth buffer's precision is consumed near the camera, and very little precision remains at the far plane.

Practical consequence: if your near plane is at 0.1 and your far plane is at 10000, the depth buffer has almost no precision past about 100 units. Two triangles at depth 5000 might hash to the same depth buffer value and alternate randomly between frames — this is **z-fighting**, a visual shimmering artifact.

Fix: keep the near/far ratio (far/near) as small as practical. A near=1, far=1000 ratio of 1000 is fine. A near=0.01, far=10000 ratio of 1,000,000 will produce visible z-fighting. In Unreal, the near plane is typically set to 10 cm (0.1 UU = ~1cm, so near ≈ 10 UU). This keeps far/near manageable.

### The Stencil Test

The stencil buffer is an 8-bit integer image alongside the depth buffer. It is used for masking — "only shade pixels inside this region" or "only shade pixels outside this portal." The test works like the depth test: compare the fragment's stencil reference value against the current stencil buffer value using a configurable function, and either pass or discard.

Common uses:
- Mirror reflections: render the scene, stencil-mark the mirror surface, render the reflected scene only where stencil is marked.
- Portals in games (e.g., Portal's portals): render what is visible through the portal only where the portal surface has been drawn.
- Shadow volumes: count enter/exit rays to determine if a fragment is in shadow.
- Decals: apply only inside a specific region.

For our test triangle with no stencil setup, the stencil test is disabled and all fragments pass.

---

## Stage 9: Blending — Alpha Compositing

### What problem does this stage solve?

Transparent objects (glass, smoke, particles, UI overlays) need to be composited against the scene behind them rather than opaquely overwriting it. Blending mixes the incoming fragment's color with the color already in the framebuffer.

The blend equation (standard alpha blending):

```
output = src_color * src_alpha + dst_color * (1 - src_alpha)
```

Where:
- `src_color` = fragment shader output color
- `src_alpha` = fragment's alpha channel
- `dst_color` = current color in the framebuffer at this pixel

For our opaque orange triangle, alpha = 1.0:

```
output = orange * 1.0 + dst_color * (1 - 1.0)
       = orange * 1.0 + dst_color * 0
       = orange
```

For a 50%-transparent blue triangle (alpha = 0.5) on top of the orange triangle already in the framebuffer:

```
output = blue * 0.5 + orange * 0.5
       = half-blue-half-orange mix
```

### The order-dependence problem

Alpha blending is order-dependent. Front-to-back compositing and back-to-front compositing produce different results. The rule: **transparent geometry must be drawn back-to-front (far objects first)**, so that each new fragment composites on top of the correct base color.

Opaque geometry has no order-dependence (depth test handles it). This is why game engines split rendering into two passes: first all opaque geometry (depth test, no blending), then all transparent geometry (no depth writes, sorted back-to-front, with blending enabled).

In Unreal Engine 5, this is the separation between the **Base Pass** (opaque geometry, writes to G-buffer) and the **Translucency Pass** (transparent objects drawn back-to-front over the final lit buffer).

---

## Stage 10: Framebuffer Output — Back Buffer, V-Sync, and Present

### What problem does this stage solve?

You have rendered a frame. All fragments have passed through the shader, depth test, and blending, and the color buffer is complete. But you cannot write directly to the display — the monitor is currently scanning through the previous frame. Writing to the display buffer mid-scan produces a torn image (L1A covered this).

The solution, as covered in L1A, is double buffering:
- **Front buffer**: what the monitor is currently displaying.
- **Back buffer**: where the GPU is writing the new frame.

When the frame is complete, you call `glfwSwapBuffers(window)` (or `vkQueuePresentKHR` in Vulkan). This triggers the buffer swap — after the monitor's vertical blank interval (the period between frames), the front and back buffers are swapped. The monitor starts scanning the newly completed frame. The GPU starts writing the next frame into what was the front buffer.

With V-Sync enabled, the swap blocks until the vertical blank, capping frame rate to the monitor's refresh rate (60 Hz, 120 Hz, etc.) and eliminating tearing. With V-Sync disabled, the GPU swaps immediately, producing the highest possible frame rate at the cost of potential tearing.

Vulkan's `VkPresentModeKHR` exposes this directly:
- `VK_PRESENT_MODE_FIFO_KHR`: swap at vblank, frame queue. V-sync on. Guaranteed to be supported.
- `VK_PRESENT_MODE_MAILBOX_KHR`: swap at vblank using only the latest frame ("triple buffering"). Reduces latency vs FIFO.
- `VK_PRESENT_MODE_IMMEDIATE_KHR`: swap immediately. No tearing prevention. Maximum frame rate.

---

## End-to-End Summary: One Triangle's Journey

Here is every transformation our test vertex V0 = (0, 1, 0) underwent, collected in one place:

```
Stage           Coordinate Value                    Space
─────────────────────────────────────────────────────────────────
CPU (input)     (0, 1, 0)                           Model space
                ↓ Model matrix: translate (0,0,-5)
Vertex Shader   (0, 1, -5, 1)                       World / Eye space
                ↓ Projection matrix
Vertex output   (0, 1, 3.08, 5)                     Clip space
                ↓ Clipping (passes through)
Clip            (0, 1, 3.08, 5)                     Clip space
                ↓ Perspective divide: x,y,z / w
Perspective ÷   (0, 0.2, 0.616, 1)                  NDC
                ↓ Viewport: 800×800 window
Viewport        (400, 320) depth=0.808              Screen pixels
                ↓ Rasterization (edge tests)
Fragments       ~1000 (fragments inside triangle)   Screen coords + depth
                ↓ Fragment Shader (each fragment)
Fragment out    (1.0, 0.5, 0.2, 1.0) = orange       RGBA [0,1]
                ↓ Depth test (depth=0.808 < 1.0)
Depth test      PASS                                 —
                ↓ Blending (alpha=1, no mix)
Blend           orange unchanged                     RGBA [0,1]
                ↓ Write to back buffer
Framebuffer     pixel (x,y) = orange                 Back buffer
                ↓ vSwapBuffers at vblank
Monitor         orange pixel on screen               Physical display
```

---

## Legacy OpenGL vs Modern OpenGL vs Vulkan

The slides you are studying use legacy OpenGL (pre-3.1). Understanding how each pipeline stage maps across the three eras helps you read old slide content and write modern code simultaneously.

```
Concept              Legacy OpenGL          Modern OpenGL 3.3+      Vulkan 1.x
────────────────────────────────────────────────────────────────────────────────
Vertex data          glBegin/glEnd          VAO + VBO               VkBuffer + VkVertexInputAttrib
Matrix upload        glMatrixMode + mult    glUniformMatrix4fv      Push constants / UBO
Vertex transform     Fixed pipeline         Vertex shader (GLSL)    Vertex shader (SPIR-V)
Primitive type       glBegin(GL_TRIANGLES)  glDrawArrays(GL_TRIANGLES) VkPipelineInputAssembly
Clipping             Fixed, automatic       Fixed, automatic         Fixed, automatic
Perspective divide   Fixed, automatic       Fixed, automatic         Fixed, automatic
Viewport             glViewport             glViewport               VkViewport in pipeline
Rasterization        Fixed (with interp)    Fixed (with interp)      Fixed (VkRasterizationState)
Fragment/pixel work  Fixed lighting pipeline Fragment shader (GLSL)  Fragment shader (SPIR-V)
Depth test           glDepthFunc            glDepthFunc              VkPipelineDepthStencilState
Blending             glBlendFunc            glBlendFunc              VkPipelineColorBlendState
Present              glutSwapBuffers        glfwSwapBuffers          vkQueuePresentKHR

```

The geometric stages (clipping, perspective divide, viewport) have never been programmable in any of these APIs. They are always fixed-function hardware. What changed across eras was the programmability of the vertex and fragment stages.

---

## Pipeline Stage Industry Mapping

Where does each stage live in the tools you will use professionally?

**CPU Application Stage:**
Unreal Engine 5: `FMeshPassProcessor` builds `FMeshDrawCommand` objects; the render thread calls `RHICmdList.DrawIndexedPrimitive`. In Vulkan directly: `vkCmdDrawIndexed` inside a `vkCommandBuffer`.

**Vertex Shader:**
HLSL in DirectX / Unreal. GLSL in OpenGL. SPIR-V binary consumed by Vulkan (typically compiled from GLSL via `glslangValidator` or `shaderc`). In Unreal's Material system, the Vertex shader is generated from the Material Graph and compiled to DXBC/DXIL/SPIR-V depending on the platform.

**Primitive Assembly + Clipping:**
Fully handled by `VkPipelineInputAssemblyStateCreateInfo` topology and the fixed hardware rasterizer. No programmer control over the actual clipping math — but you control the near/far planes and frustum parameters that determine what gets clipped.

**Rasterization:**
`VkPipelineRasterizationStateCreateInfo` in Vulkan: `polygonMode` (fill/wireframe/points), `cullMode` (front/back/none), `frontFace` (CW vs CCW), `depthBiasEnable` (for shadow map slope-scale bias). In OpenGL: `glPolygonMode`, `glCullFace`, `glFrontFace`.

**Fragment Shader:**
This is the bulk of your creative work as a graphics programmer. Every material in Unreal, every visual effect in Unity HDRP, every screen-space effect (ambient occlusion, reflections, bloom) — all fragment shaders. RenderDoc lets you inspect per-fragment values, step through shader execution, and check what the GPU actually computed at any pixel.

**Depth / Stencil:**
Shadow maps run a dedicated depth-only pass (no fragment shader, just depth writes) from the light's perspective, then read that depth texture in the main pass to test shadow visibility. This is a second full pipeline execution for every shadow-casting light. `glDepthMask(GL_FALSE)` disables depth writes (used for transparent geometry and decals).

**Framebuffer + Present:**
DXGI's `IDXGISwapChain::Present` in DirectX. `vkQueuePresentKHR` in Vulkan. `[MTLCommandBuffer presentDrawable:]` in Metal. Unreal's `RHIEndDrawingViewport` maps to these. Frame pacing, latency, and vsync settings in Unreal's project settings ultimately configure the present mode of the underlying graphics API.

---

## Pitfalls

**Forgetting to clear the depth buffer each frame.**
If you do not call `glClear(GL_DEPTH_BUFFER_BIT)` at the start of each frame, the depth buffer retains values from the previous frame. Geometry from the previous frame's depth data blocks current-frame geometry that should be visible. Symptom: flickering or objects that disappear when they should not. Fix: always clear color and depth at the start of each frame.

**Near plane too close.**
`near = 0.001` with `far = 1000` gives a ratio of 1,000,000. The depth buffer loses precision exponentially near the far plane. Symptom: z-fighting (shimmering stripes) on distant geometry. Fix: set near to the smallest distance you actually need (usually 0.1 to 1.0 in scene units). Reverse-Z (Vulkan, DirectX) maps near to depth 1.0 and far to 0.0, inverting the non-linearity and dramatically improving precision.

**Wrong winding order.**
If your triangle vertices are specified clockwise when the API expects counter-clockwise (or vice versa), back-face culling will cull the front face. Symptom: geometry that should be visible is invisible. Fix: check `glCullFace`/`glFrontFace` settings; or temporarily disable culling (`glDisable(GL_CULL_FACE)`) to confirm the geometry is present but culled.

**Transparent geometry drawn before opaque.**
Drawing transparent objects with depth writes enabled before drawing opaque geometry means the transparent object's depth values block the opaque geometry behind it. Symptom: opaque objects behind glass are invisible. Fix: render all opaque objects first with depth writes on; then render transparent objects with `glDepthMask(GL_FALSE)` (depth test still reads, but does not write) sorted back-to-front.

**Not normalizing interpolated normals in the fragment shader.**
Rasterization linearly interpolates vertex normals across the triangle. Linear interpolation of unit vectors does not produce unit vectors. A normal of (0.7, 0.7, 0) interpolated midway with (0.7, -0.7, 0) gives (0.7, 0, 0), which has magnitude 0.7, not 1.0. Using un-normalized normals in the lighting equation produces darkened or incorrect diffuse/specular values. Fix: always `normalize(vNormal)` in the fragment shader before any lighting calculation.

**Drawing without a valid VAO bound (modern OpenGL).**
Core profile OpenGL 3.2+ requires a VAO to be bound for any draw call. Forgetting to create or bind a VAO results in `GL_INVALID_OPERATION` on the draw call. Symptom: nothing draws, no error message unless you check `glGetError()` or use a debug context. Fix: create and bind a VAO; enable the debug context (`glEnable(GL_DEBUG_OUTPUT)` with a callback) so errors surface immediately.

**Transforming normals with the model matrix instead of the normal matrix.**
If the model matrix contains non-uniform scaling (scale x by 2, y by 1), multiplying the normal by the model matrix distorts it — the normal no longer points perpendicular to the surface. Symptom: lighting looks incorrect on scaled objects, especially noticeable with specular highlights. Fix: compute and upload the normal matrix: `mat3(transpose(inverse(model)))`. Pass it as a uniform to the vertex shader.

**Assuming NDC depth is linear.**
The depth buffer values are not linearly proportional to eye-space distance. They are compressed — most of the [0,1] range covers the region near the camera. If you try to do linear fog or depth-based effects by reading the depth buffer directly, you get incorrect results near the far plane. Fix: linearize depth when needed: `linear_depth = (2.0 * near * far) / (far + near - depth * (far - near))`.

---

## What to Build After This Lesson

The best way to verify you understand this pipeline is to implement it yourself, end to end:

1. **Software rasterizer (no GPU):** write a C or C++ program that takes three 2D triangle vertices, clips to a bounding box, runs the edge-function test per pixel, and colors the result to a PPM image. This forces you to implement every fragment-level step in code. Add perspective-correct interpolation once the basic version works.

2. **Modern OpenGL triangle with uniforms:** build on the code from L2 and L3. Add a second triangle that overlaps the first one. Put one triangle closer to the camera and verify the depth test resolves the overlap correctly. Toggle `glEnable/glDisable(GL_DEPTH_TEST)` to see what changes.

3. **Explicit MVP pass:** output the clip-space coordinates from the vertex shader in a separate debug pass (write them to a texture or log them via transform feedback). Verify they match the hand-calculated values above.

4. **Transparent object over opaque:** draw an orange opaque triangle, then a blue 50%-transparent triangle in front of it. Toggle the draw order and observe the blending artifacts when the transparent triangle is drawn first with depth writes enabled.

Each of these forces one specific piece of the pipeline to become concrete and debuggable, rather than a black box you trust.
