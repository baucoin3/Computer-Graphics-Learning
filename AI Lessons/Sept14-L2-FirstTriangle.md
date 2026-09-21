# Lesson 2 — Your First OpenGL Triangle

**Prerequisites:** Lesson 1 complete — Mac dev environment set up (GLFW, GLAD, CMake, GLM via Homebrew or manual install). Comfortable with C++ basics (pointers, arrays, functions). No GPU programming experience assumed.

**Goal:** Render a single color-interpolated triangle using a VAO, VBO, and custom GLSL vertex + fragment shaders. By the end you will understand not just what each API call does, but why it exists at the driver and hardware level.

**Pipeline context:** This lesson covers the full path from a `float[]` array on your CPU all the way to pixels on screen — upload, attribute configuration, vertex shader, rasterization, fragment shader, framebuffer swap. Every concept maps directly to a stage in the CPS 511 pipeline diagram.

---

## What Is an OpenGL Context?

Before you write a single OpenGL call, you need to understand what you are actually calling into.

**The OpenGL context is a state machine owned by the GPU driver.** It is not a C++ object you instantiate. It is a large block of state maintained by the driver process that tracks everything the GPU needs to know in order to execute draw calls. That state includes, non-exhaustively:

- Which shader program is currently active
- Which VAO is bound
- Which VBOs are bound to which binding points
- Which framebuffer is the current render target
- Which textures are bound to which texture units
- The current blend state, depth test state, stencil state
- The current viewport rectangle
- A stack of matrix state (legacy) or your own uniforms (modern)

When you call `glfwMakeContextCurrent(window)`, you are telling the OS and driver: "attach this context to the current thread." **OpenGL is fundamentally thread-bound.** Each thread can have at most one context active at a time. If you call any `gl*` function before making a context current on that thread, the call either silently does nothing or crashes — the driver has no state machine to act on.

This has a direct practical consequence: if you ever try to do OpenGL work on a background thread (for async mesh loading, for example), you need either a shared context or a mechanism like pixel buffer objects. It is a constant source of subtle bugs.

**Why was it designed this way?** OpenGL was standardized in 1992 when GPUs were literal hardware state machines — physical register files that tracked current color, current normal, current matrix mode. The software API was designed to mirror that hardware model. The "context" is the software abstraction of those registers.

**Modern relevance — Vulkan has no global context.** In Vulkan, every piece of state is explicit: you record commands into a `VkCommandBuffer`, you describe the full pipeline state in a `VkPipeline` object created ahead of time, and you submit command buffers to a `VkQueue`. There is no hidden global state. This is why Vulkan code is significantly more verbose — you specify everything — but it is also why Vulkan has no "why is my draw call using the wrong shader?" bugs. Everything is explicit.

```
OpenGL mental model:

  Thread A                       GPU Driver
  ─────────                      ─────────────────────────────────────────
  glfwMakeContextCurrent(win) ─► Attaches Context #1 to Thread A
  glUseProgram(prog)          ─► Context #1.currentProgram = prog
  glBindVertexArray(vao)      ─► Context #1.currentVAO = vao
  glDrawArrays(...)           ─► Execute draw using current state in Context #1
```

Every `gl*` call is an implicit operation on "the current context of the current thread." Keep this in mind whenever something draws wrong — ask: "what state is the context actually in at this point?"

---

## Why GLAD Exists

You added `#include <glad/glad.h>` and called `gladLoadGLLoader(...)` without necessarily knowing what problem GLAD is solving. Here is the full story.

**On Windows**, the system ships with `opengl32.dll`. This DLL exports exactly the OpenGL 1.1 API from 1997. That is all. Functions like `glGenBuffers`, `glCreateShader`, `glVertexAttribPointer` — everything introduced after 1997 — are not exported from that DLL. They are available as "extensions" that must be loaded at runtime by asking the driver for a function pointer.

Without a loader library, you would write this, manually, for every modern OpenGL function:

```c
// You would need to declare a function pointer type for each function:
typedef void (*PFNGLGENBUFFERSPROC)(GLsizei n, GLuint* buffers);

// Then load it from the driver at runtime:
PFNGLGENBUFFERSPROC glGenBuffers =
    (PFNGLGENBUFFERSPROC)wglGetProcAddress("glGenBuffers");

// And repeat this for: glBindBuffer, glBufferData, glCreateShader,
// glShaderSource, glCompileShader, glCreateProgram, glAttachShader,
// glLinkProgram, glGetUniformLocation, glUniform1f, glGenVertexArrays,
// glBindVertexArray, glVertexAttribPointer, glEnableVertexAttribArray...
// There are hundreds of these.
```

GLAD is a code generator that does this automatically. You tell it which OpenGL version and profile you want (3.3 Core, in our case), it generates a `.c` file containing all the function pointer declarations and a loader function that calls `wglGetProcAddress` (Windows) or `glXGetProcAddress` (Linux) for each one. You compile that `.c` file into your project and call the loader once.

**On Mac**, the system's OpenGL framework exports all functions directly — no extension loading needed. So `GLAD`'s loading code is effectively a no-op on Mac. But it still compiles and runs correctly, which means your code is portable across platforms without `#ifdef` blocks.

```
gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)
```

This passes GLFW's own function-pointer resolver to GLAD. GLAD iterates over every OpenGL function it knows about, calls `glfwGetProcAddress("glGenBuffers")` (for example), and stores the resulting address in a global function pointer named `glGenBuffers`. After this one call returns, every modern OpenGL function in your code resolves correctly.

**If you forget to call `gladLoadGLLoader` before any `gl*` call:** on Mac it often still works (because the symbols are exported directly), but on Windows the function pointers are null and your program crashes with a null pointer dereference. Always initialize GLAD immediately after `glfwMakeContextCurrent`.

---

## GPU Memory and the CPU-GPU Transfer Model

This is the most important conceptual leap in getting from "C++ arrays" to "GPU rendering."

### Two Separate Memory Spaces

On a discrete GPU (NVIDIA, AMD), the GPU has its own DRAM chips on the card — GDDR6, GDDR6X, HBM. A modern RTX 4090 has 24 GB of VRAM with over 1 TB/s of internal bandwidth. Your CPU has DDR5 system RAM with maybe 80 GB/s of bandwidth. These are connected by a PCIe bus with roughly 64 GB/s bidirectional bandwidth. **They are physically separate.**

```
  CPU (system RAM, ~80 GB/s)        GPU (VRAM, ~1 TB/s internal)
  ──────────────────────────        ─────────────────────────────
  float vertices[] = { ... };       [GPU Buffer — allocated by driver]
                 │                              │
                 └──── PCIe bus ───────────────►│
                       (~64 GB/s)    glBufferData copies here
```

When the GPU executes a draw call, its shader cores read vertex data from VRAM. They cannot reach into your CPU's RAM during execution. **This is why you upload geometry to the GPU first.**

**On your Mac with Apple Silicon (M-series):** The CPU and GPU share one physical pool of LPDDR5 — unified memory. There is no PCIe bus, and no explicit copy across a slow interconnect. The GPU driver still manages a separate heap within that pool for GPU allocations, so the programming model is identical. You still call `glBufferData`, the driver still copies from your CPU buffer to its GPU-managed allocation. The benefit is that this copy is fast (memory bus speeds, not PCIe speeds) and the total pool can be flexibly divided between CPU and GPU use. On a discrete GPU machine this is a genuine bottleneck; on Apple Silicon it matters less, but the concept is the same.

### What glBufferData Actually Does

```cpp
glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
```

Step by step:
1. The driver allocates a block of GPU memory of size `sizeof(vertices)`.
2. It copies the bytes from `vertices` (a pointer into your CPU stack/heap) into that GPU allocation.
3. It associates that allocation with the currently bound `GL_ARRAY_BUFFER` (which is your VBO).

After this call returns, `vertices` can be freed or go out of scope. The GPU has its own independent copy. The CPU array is no longer involved.

**What you cannot do:** modify the GPU buffer by writing to `vertices` afterward. The GPU copy is separate. To update GPU data you call `glBufferSubData` or re-call `glBufferData`.

### GL_STATIC_DRAW vs GL_DYNAMIC_DRAW vs GL_STREAM_DRAW

These are **hints to the driver** about how you intend to use the buffer. They do not enforce anything — you can call `glBufferSubData` on a `GL_STATIC_DRAW` buffer and it will not crash. But the driver uses these hints to decide **where in GPU memory to place the allocation** and **how to optimize internal caching.**

| Hint | Meaning | Driver behavior |
|---|---|---|
| `GL_STATIC_DRAW` | Written once, read many times per frame | Place in fastest GPU-local VRAM; good for static meshes |
| `GL_DYNAMIC_DRAW` | Written repeatedly, read many times per frame | May place in CPU-visible memory for fast CPU writes; used for skinned meshes, particles with simulation on CPU |
| `GL_STREAM_DRAW` | Written once, read once (or a few times) then discarded | Optimized for high-churn streaming; driver may use ring buffers |

**The wrong choice costs you performance, not correctness.** Using `GL_STATIC_DRAW` for geometry you update every frame means the driver placed it in deep VRAM — writing to it now requires a slow cross-bus synchronization. Using `GL_DYNAMIC_DRAW` for completely static geometry is harmless (maybe a few percent slower on reads) but wastes the driver's placement optimization.

For static geometry like this triangle: always `GL_STATIC_DRAW`.

### VBO Lifecycle and GPU Memory Leaks

GPU memory is not garbage-collected. When you allocate a VBO, the driver allocates VRAM. When you forget to free it, that VRAM is gone until the OpenGL context is destroyed (typically when the window closes).

```
glGenBuffers(1, &VBO);    // allocate a buffer NAME (just an integer handle)
glBindBuffer(..., VBO);   // make it the active buffer
glBufferData(...);         // ALLOCATE GPU MEMORY and upload data

// ... use it ...

glDeleteBuffers(1, &VBO); // FREE the GPU memory — this is mandatory
```

For this lesson's triangle, skipping `glDeleteBuffers` is harmless because the window closes immediately after. But in a game engine that streams mesh data in and out — loading a level, unloading old geometry — failing to delete VBOs is a genuine VRAM leak. The mesh data accumulates until you run out of VRAM and the driver starts failing allocations or swapping to system RAM (a catastrophic performance cliff on discrete GPUs).

The cleanup pattern at the end of main is not optional boilerplate — it is the correct RAII pattern for GPU resources. In modern C++ and Vulkan-style code you'd wrap these in RAII classes. In this lesson we do it manually so you see every step.

---

## What a VAO Actually Stores (and What It Does Not)

**Common misconception: the VAO stores vertex data.** This is wrong, and it leads to bugs where you delete a VBO while a VAO still references it, then get garbage geometry.

**What a VAO stores:** a table of attribute configurations. For each attribute slot (0 through `GL_MAX_VERTEX_ATTRIBS - 1`, typically 16), the VAO records:

- Which VBO is currently bound to `GL_ARRAY_BUFFER` when `glVertexAttribPointer` was called (stored as a VBO handle, not a copy of the data)
- The byte offset into that VBO where this attribute starts
- The stride (bytes between the start of one vertex's data and the next)
- The data type (`GL_FLOAT`, `GL_INT`, etc.) and component count (1, 2, 3, or 4)
- Whether the attribute is enabled (`glEnableVertexAttribArray`)

```
  VAO #1 — internal state table:
  ┌──────┬────────────┬────────────────┬────────────────┬───────┬───────┬─────────┐
  │ Slot │ VBO handle │ Offset (bytes) │ Stride (bytes) │ Type  │ Count │ Enabled │
  ├──────┼────────────┼────────────────┼────────────────┼───────┼───────┼─────────┤
  │  0   │  VBO #1    │      0         │      24        │ FLOAT │   3   │   YES   │
  │  1   │  VBO #1    │     12         │      24        │ FLOAT │   3   │   YES   │
  │  2   │    —       │      —         │       —        │   —   │   —   │   NO    │
  │ ...  │    —       │      —         │       —        │   —   │   —   │   NO    │
  └──────┴────────────┴────────────────┴────────────────┴───────┴───────┴─────────┘
```

The VBO (with the actual float data) lives completely independently. The VAO just holds a **reference** to it by handle. If you delete the VBO while the VAO references it, the VAO's table entry is now pointing at a freed allocation — undefined behavior when you draw.

### The Bind-Record-Unbind Pattern

```cpp
glBindVertexArray(VAO);          // START recording into this VAO's table
glBindBuffer(GL_ARRAY_BUFFER, VBO);
glBufferData(...);
glVertexAttribPointer(0, ...);   // writes slot 0 in VAO's table
glEnableVertexAttribArray(0);    // sets Enabled=YES for slot 0
glVertexAttribPointer(1, ...);   // writes slot 1 in VAO's table
glEnableVertexAttribArray(1);
glBindVertexArray(0);            // STOP recording; unbind for safety
```

Later, in the render loop:

```cpp
glBindVertexArray(VAO);          // RESTORE the entire attribute config in one call
glDrawArrays(GL_TRIANGLES, 0, 3);
```

This one `glBindVertexArray` call restores everything in the table — both attribute configurations, both VBO references, both enabled states. This is the performance benefit of VAOs: instead of re-issuing 6+ `glVertexAttribPointer` calls every frame, you issue one `glBindVertexArray`.

In Vulkan, this concept maps to `VkVertexInputAttributeDescription` and `VkVertexInputBindingDescription` structs baked into the pipeline object at creation time — even more explicit, no runtime binding.

---

## Stride and Offset — Worked Calculation with Memory Layout

This is the part most tutorials skip, and it causes real bugs when you have more complex vertex formats.

Your vertex array stores **interleaved** data — position and color packed together per vertex:

```
Interleaved vertex buffer in CPU memory (and mirrored in GPU VRAM after glBufferData):

Byte offset:   0    4    8   12   16   20  | 24   28   32   36   40   44  | 48...
               ↓    ↓    ↓    ↓    ↓    ↓  |  ↓    ↓    ↓    ↓    ↓    ↓  |
Data (floats): [x0] [y0] [z0] [r0] [g0] [b0] [x1] [y1] [z1] [r1] [g1] [b1] [x2]...
               ←─────── vertex 0 ────────→ ←─────── vertex 1 ────────→

               ←── position (bytes 0–11) ──→
                                        ←── color (bytes 12–23) ─────→
               ←──────────── 24 bytes total per vertex ─────────────→
```

From this layout, the two parameters that matter:

**Stride** = total bytes per vertex = 6 floats × 4 bytes/float = **24 bytes**. This is how far apart the start of vertex N is from the start of vertex N+1. The GPU uses this to step through the buffer during vertex fetch.

**Position offset** = 0. Position data starts at the very beginning of each vertex block.

**Color offset** = 12. Color data starts after the 3 position floats = 3 × 4 = **12 bytes** into each vertex block.

These feed directly into `glVertexAttribPointer`:

```cpp
// Attribute 0 — position
glVertexAttribPointer(
    0,                 // slot
    3,                 // 3 components (x, y, z)
    GL_FLOAT,
    GL_FALSE,
    24,                // stride: 24 bytes between vertices
    (void*)0           // offset: starts at byte 0
);

// Attribute 1 — color
glVertexAttribPointer(
    1,                 // slot
    3,                 // 3 components (r, g, b)
    GL_FLOAT,
    GL_FALSE,
    24,                // same stride
    (void*)12          // offset: starts at byte 12
);
```

### Why `(void*)12` — The Historical Reason

The last parameter of `glVertexAttribPointer` has type `const void*`. This looks like a pointer, and in ancient OpenGL (before VBOs), it was a pointer — a literal CPU-side pointer to the start of the vertex data in your application's memory. You would call `glDrawArrays` and the driver would read directly from that RAM address each frame, across the bus, every draw call. This was the "client-side vertex arrays" model and it was slow.

When VBOs were introduced, the API needed a way to specify a byte offset into the VBO instead of a CPU pointer. Rather than add a new function (and break backward compatibility), the committee repurposed this parameter: when a VBO is bound, the driver interprets the `void*` value not as a pointer but as an integer byte offset cast to a pointer type.

So `(void*)12` means "offset 12 bytes into the currently bound VBO." The cast is required because the parameter type is `const void*`. You will often see it written as `(void*)(3 * sizeof(float))` to make the calculation explicit rather than a magic number. Both are identical.

**Vulkan fix:** `VkVertexInputAttributeDescription.offset` is a `uint32_t`. No cast, no historical baggage.

---

## Shader Compilation — What Actually Happens Under the Hood

You call `glCompileShader` and it either succeeds or fails. What actually happens inside that call?

### glShaderSource

```cpp
glShaderSource(shader, 1, &source, nullptr);
```

This uploads the GLSL source text as a string into the driver. Nothing compiles yet. The driver just stores the string. You can call `glShaderSource` multiple times to replace the source before compiling.

### glCompileShader

This is where the GPU driver invokes its built-in compiler. The compilation target is not your GPU's actual machine code — it is a vendor-specific intermediate representation:

- **NVIDIA drivers:** GLSL is compiled to **PTX** (Parallel Thread eXecution), NVIDIA's virtual ISA. At draw time, the driver may JIT-compile PTX to the actual SM (Streaming Multiprocessor) ISA for the specific GPU generation (Ampere, Ada Lovelace, etc.). This two-stage approach lets NVIDIA update the final code generation without re-shipping drivers.
- **AMD drivers:** GLSL is compiled to an LLVM-based IR, then to RDNA ISA instructions.
- **Apple's OpenGL compatibility layer (on top of Metal):** Apple translates GLSL to **MSL** (Metal Shading Language) and compiles it to **AIR** (Apple Intermediate Representation), which then gets compiled to the GPU's native ISA.

This compilation can take anywhere from a few milliseconds to 200+ milliseconds for complex shaders. **This is a real problem in game development.** If a game compiles 10,000 shader permutations at runtime on first launch, you get the infamous "shader compilation stutter" — frame hitches when new shader combinations are first encountered. Solutions: pre-warm shader pipelines, use async compilation, or move to Vulkan/DX12 where GLSL is compiled to **SPIR-V offline** during the build.

### glLinkProgram

Linking combines the compiled vertex and fragment shaders into a complete "program" — a full pipeline configuration. The linker checks:

- **Interface matching:** every `out` variable in the vertex shader must match a corresponding `in` variable in the fragment shader by name and type. If the vertex shader declares `out vec3 vertexColor` but the fragment shader declares `in vec4 vertexColor`, the link fails.
- **Uniform resolution:** all `uniform` variables must be resolvable. Undeclared uniforms fail the link.
- **Attribute locations:** every `in` variable in the vertex shader must have a declared location (via `layout(location = N)` or `glBindAttribLocation`). Without locations, the driver assigns them arbitrarily and your VAO attribute slots won't match.

**Critical pitfall:** a failed `glCompileShader` or `glLinkProgram` does not crash your program. OpenGL silently proceeds. If you use a program that failed to link, your geometry renders as black or invisible — no error is thrown at the draw call. **Always check compile and link status explicitly:**

```cpp
int success;
glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
if (!success) {
    char log[512];
    glGetShaderInfoLog(shader, 512, nullptr, log);
    std::cerr << "Compile error: " << log << "\n";
}

glGetProgramiv(program, GL_LINK_STATUS, &success);
if (!success) {
    char log[512];
    glGetProgramInfoLog(program, 512, nullptr, log);
    std::cerr << "Link error: " << log << "\n";
}
```

This is the first thing to add when you see a black screen.

### Vulkan / SPIR-V Comparison

In Vulkan, you do not pass GLSL source text to the driver at runtime. Instead, you compile GLSL to **SPIR-V** (Standard Portable Intermediate Representation V) offline — during your project's build step, using `glslc` (from the Vulkan SDK) or `glslangValidator`. The SPIR-V binary is a portable IR that any Vulkan driver can consume. At runtime:

```cpp
// Vulkan: load pre-compiled SPIR-V binary, no GLSL parsing at runtime
vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
```

No runtime GLSL parsing. No vendor-specific compiler variation. Fast startup. This is one of the architectural advantages of Vulkan — the compilation chain is separated from the runtime.

---

## 2.1 Project Setup

After completing the Mac setup steps at the top:

```bash
mkdir ~/cg-projects && cd ~/cg-projects
mkdir lesson02-triangle && cd lesson02-triangle
mkdir include src build
```

Download GLAD from https://glad.dav1d.de/ with:
- Language: C/C++
- Specification: OpenGL
- API gl: Version 3.3
- Profile: Core
- Generate a loader: checked

Copy the downloaded `include/` into your `include/` and `src/glad.c` into your `src/`.

**CMakeLists.txt:**
```cmake
cmake_minimum_required(VERSION 3.10)
project(triangle)

set(CMAKE_CXX_STANDARD 17)

# Tell compiler not to warn about deprecated OpenGL on Mac
add_definitions(-DGL_SILENCE_DEPRECATION)

find_package(OpenGL REQUIRED)

# GLFW via Homebrew
find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3)

include_directories(
    ${GLFW_INCLUDE_DIRS}
    include/
)

add_executable(triangle
    src/main.cpp
    src/glad.c
)

target_link_libraries(triangle
    ${OPENGL_LIBRARIES}
    ${GLFW_LIBRARIES}
)
```

**Build and run:**
```bash
cd build
cmake ..
make
./triangle
```

---

## 2.2 The Code — main.cpp (Fully Annotated)

```cpp
// GL_SILENCE_DEPRECATION: Apple deprecated OpenGL 4.1+ on macOS.
// This silences warnings. OpenGL 3.3 still works fine.
#define GL_SILENCE_DEPRECATION

#include <glad/glad.h>      // Load OpenGL function pointers (must come first)
#include <GLFW/glfw3.h>     // Window creation + input
#include <iostream>

// ─── VERTEX SHADER (runs on GPU, once per vertex) ────────────────────────────
// Pipeline stage: Vertex Processing
// Input:  aPos — the raw (x,y,z) position from the VBO (model coordinates)
// Output: gl_Position — the 4D clip coordinate after MVP transform
//
// For now, MVP = identity (no transform). The triangle IS in NDC space already.
// Lesson 3 will add the actual MVP matrices here.
const char* vertexShaderSource = R"glsl(
    #version 330 core
    layout(location = 0) in vec3 aPos;   // attribute 0 = position
    layout(location = 1) in vec3 aColor; // attribute 1 = color

    out vec3 vertexColor; // passed to fragment shader (interpolated by rasterizer)

    void main() {
        gl_Position = vec4(aPos, 1.0); // (x, y, z, w=1) → clip coords
        // No MVP here yet — vertices are already in NDC [-1,1]
        // w=1 so perspective division just gives (x,y,z)/1 = (x,y,z)
        vertexColor = aColor;
    }
)glsl";

// ─── FRAGMENT SHADER (runs on GPU, once per fragment) ────────────────────────
// Pipeline stage: Fragment Processing
// Input:  vertexColor — interpolated from vertex shader outputs by the rasterizer
// Output: FragColor — RGBA color written to the framebuffer color buffer
const char* fragmentShaderSource = R"glsl(
    #version 330 core
    in vec3 vertexColor;    // interpolated across triangle by rasterizer
    out vec4 FragColor;     // output to color buffer

    void main() {
        FragColor = vec4(vertexColor, 1.0); // RGB + alpha=1 (fully opaque)
    }
)glsl";

// ─── SHADER COMPILATION HELPER ───────────────────────────────────────────────
unsigned int compileShader(GLenum type, const char* source) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, nullptr, log);
        std::cerr << "Shader compile error:\n" << log << "\n";
    }
    return shader;
}

int main() {
    // ── WINDOW SETUP ─────────────────────────────────────────────────────────
    glfwInit();
    // Tell GLFW: use OpenGL 3.3 core profile (no legacy functions)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    // Required on Mac:
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Lesson 2 - Triangle", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // ── LOAD OPENGL FUNCTIONS VIA GLAD ───────────────────────────────────────
    // On Windows/Mac, OpenGL functions are not available by default.
    // GLAD loads the function pointers at runtime from the GPU driver.
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return -1;
    }
    glViewport(0, 0, 800, 600); // Pipeline: Viewport Transform
                                 // Tell GPU: NDC [-1,1] maps to pixels [0..800] x [0..600]

    // ── COMPILE AND LINK SHADERS ──────────────────────────────────────────────
    unsigned int vs = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

    // Link both shaders into a "program" — a complete pipeline config
    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vs);
    glAttachShader(shaderProgram, fs);
    glLinkProgram(shaderProgram);
    glDeleteShader(vs); // once linked, raw shader objects no longer needed
    glDeleteShader(fs);

    // ── DEFINE TRIANGLE GEOMETRY ──────────────────────────────────────────────
    // These are the 3 vertices of the triangle.
    // Each vertex has: position (x,y,z) and color (r,g,b)
    //
    // Coordinates are in NDC space (since our vertex shader has no MVP yet).
    // NDC: x in [-1,1], y in [-1,1], center of screen is (0,0)
    // z=0 puts triangle on the near plane (visible)
    float vertices[] = {
    //   x      y     z      r     g     b
        -0.5f, -0.5f, 0.0f,  1.0f, 0.0f, 0.0f,  // bottom-left  (red)
         0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f,  // bottom-right (green)
         0.0f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f,  // top-center   (blue)
    };

    // ── VAO AND VBO ───────────────────────────────────────────────────────────
    //
    // VBO (Vertex Buffer Object): GPU-side memory buffer storing vertex data.
    // This is the slide concept: "store vertex data in GPU memory for fast access".
    // Without a VBO, you'd re-send vertices from CPU to GPU every frame — slow.
    //
    // VAO (Vertex Array Object): records HOW to interpret the VBO.
    // "Attribute 0 = position: starts at byte 0, stride 24 bytes, 3 floats"
    // "Attribute 1 = color:    starts at byte 12, stride 24 bytes, 3 floats"
    // When you draw, GPU reads VAO to know how to feed data to the vertex shader.

    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);         // start recording into this VAO

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    // GL_STATIC_DRAW: data won't change (hint to GPU for memory placement)

    // Tell GPU: attribute 0 = position
    // layout(location=0) in vertex shader corresponds to this
    glVertexAttribPointer(
        0,                  // attribute location (matches "layout(location=0)")
        3,                  // 3 floats per vertex for position (x,y,z)
        GL_FLOAT,
        GL_FALSE,           // don't normalize
        6 * sizeof(float),  // stride: each vertex is 6 floats wide (pos + color)
        (void*)0            // offset: position data starts at byte 0
    );
    glEnableVertexAttribArray(0);

    // Tell GPU: attribute 1 = color
    glVertexAttribPointer(
        1,                          // attribute location
        3,                          // 3 floats per vertex for color (r,g,b)
        GL_FLOAT,
        GL_FALSE,
        6 * sizeof(float),          // same stride
        (void*)(3 * sizeof(float))  // offset: color starts after 3 floats (12 bytes)
    );
    glEnableVertexAttribArray(1);

    glBindVertexArray(0); // done recording into VAO

    // ── MAIN RENDER LOOP ──────────────────────────────────────────────────────
    while (!glfwWindowShouldClose(window)) {
        // Input
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // Clear color buffer and depth buffer for this frame
        // This paints the background dark grey before drawing anything
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Activate shader program (selects which vertex + fragment shader runs)
        glUseProgram(shaderProgram);

        // Bind VAO → GPU knows the vertex layout
        glBindVertexArray(VAO);

        // DRAW CALL: GPU pipeline fires here
        // - Vertex shader runs 3 times (once per vertex)
        // - Rasterizer fills in fragments inside the triangle
        // - Fragment shader runs once per covered pixel
        // - Output goes to framebuffer
        glDrawArrays(GL_TRIANGLES, 0, 3); // primitive type, start index, vertex count

        // Swap back buffer to front (show the rendered frame)
        glfwSwapBuffers(window);
        glfwPollEvents(); // process keyboard/mouse events
    }

    // Cleanup
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);
    glfwTerminate();
    return 0;
}
```

---

## 2.3 What You Should See

A window with a dark background, and a smooth-gradient triangle: red bottom-left, green bottom-right, blue top. The color gradient across the triangle is the rasterizer interpolating the per-vertex colors.

If the triangle doesn't appear: check that `z=0.0` is within the near/far range (it is, by default), and that your GLFW OpenGL version hints match what your Mac supports (3.3 core is supported on all Macs since 2012).

---

## 2.4 Pipeline Trace — Where Does Each Line of Code Fit?

| Code | Pipeline Stage |
|---|---|
| `float vertices[]` | Model coordinates — raw vertex data on CPU |
| `glBufferData(...)` | Upload to GPU memory (VBO) — happens before pipeline |
| `glVertexAttribPointer(...)` | Tells GPU how to read VBO → feeds vertex shader inputs |
| `glViewport(0, 0, 800, 600)` | Viewport Transform — NDC to pixels |
| `vertexShaderSource` | Vertex Processing stage — runs per vertex on GPU |
| `gl_Position = vec4(aPos, 1.0)` | Outputs clip coordinates (here: NDC since no MVP) |
| `out vec3 vertexColor` | Data passed from vertex shader → rasterizer → fragment shader |
| Rasterizer (between shaders) | GPU hardware: determines which pixels the triangle covers, interpolates color |
| `fragmentShaderSource` | Fragment Processing — runs per covered pixel |
| `FragColor = vec4(vertexColor, 1.0)` | Writes color to color buffer |
| `glClear(GL_DEPTH_BUFFER_BIT)` | Clears z-buffer at frame start |
| `glfwSwapBuffers(window)` | Swaps front/back buffers (double buffering) |

---

## 2.5 Exercises for Lesson 2

1. Change the background color to dark blue. Which function controls this?
2. Change the triangle so all 3 vertices are yellow (`1.0, 1.0, 0.0`). What happens to the gradient?
3. Move all 3 vertices upward by 0.3 in y (just edit the float array). Does the triangle move on screen as expected?
4. Add a second triangle to the vertices array (6 vertices total) and change `glDrawArrays` count from 3 to 6. Where does it appear?
5. **Challenge:** Look at the vertex shader. Currently `gl_Position = vec4(aPos, 1.0)`. If you change it to `gl_Position = vec4(aPos * 0.5, 1.0)`, what happens to the triangle? Why? (Think: what space are the coordinates in, and what does multiplying by 0.5 do in that space?)
