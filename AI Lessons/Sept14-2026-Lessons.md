# Computer Graphics Lesson Plan — Sept 14, 2026

**Student:** Brendan Aucoin — mid-level full-stack engineer, 4.0 in CPS 511, breaking into graphics  
**Source material:** CPS 511 slides (TMU/Ryerson), CPS 643 VR slides  
**Goal:** Build low-level graphics fundamentals in C++, understand every line in terms of the GPU pipeline, position for graphics engineering roles

---

## API Choice Decision — Read This First

You asked about Vulkan. Here is an honest analysis:

### Vulkan (Industry-Standard Low-Level API)
- Used in production by: Unreal Engine, Unity HDRP, most AAA PC/console titles, Android, id Software, DICE
- Explicitly controls: command buffers, render passes, synchronization, memory allocation, pipeline state objects
- Teaches you exactly how a GPU works — no hidden state, no magic
- **Problem for learning:** Drawing a triangle requires ~800–1200 lines of boilerplate just to reach the point where you clear a window. You spend 2–4 weeks on setup (swapchains, render passes, framebuffers, command pools, descriptor sets) before you can even understand what you're writing. This is brutal if you don't yet have a clear mental model of the pipeline. People who succeed with Vulkan-first are people who already know what a render pass or a command buffer conceptually IS before they fight Vulkan's API to set one up.

### Modern OpenGL 3.3 Core Profile (GLFW + GLAD + GLM)
- Deprecated on Mac by Apple (since macOS 14 Sonoma officially) but works fine — just add `#define GL_SILENCE_DEPRECATION`
- Cross-platform (Mac, Windows, Linux)
- Much less ceremony: drawing a triangle takes ~60 lines
- Vertex shaders, fragment shaders, VAOs, VBOs, uniforms — ALL the same concepts as Vulkan, just with less setup code
- `learnopengl.com` is one of the best graphics learning resources ever written, and it uses this exact stack
- The pipeline stages you learned in CPS 511 map DIRECTLY to modern OpenGL

### Recommendation: Modern OpenGL First, Then Vulkan

The fastest path to Vulkan competence is:
1. Learn the pipeline with modern OpenGL (short feedback loop = faster understanding)
2. Once the pipeline clicks — what a vertex shader does, what the rasterizer does, what a fragment shader does, what a uniform is — translate that understanding to Vulkan

Concepts that are IDENTICAL between OpenGL and Vulkan:
- Vertex shader / fragment shader logic (GLSL ↔ SPIR-V compiled from GLSL)
- VAO/VBO → Vertex Input State + Vertex Buffers
- Uniforms → Descriptor Sets / Push Constants
- Framebuffer → Framebuffer + Render Pass + Attachments
- Depth testing → Depth/Stencil State

If someone asks you in an interview "how does the pipeline work" — the answer is the same whether you learned it via OpenGL or Vulkan. What matters is understanding, not which API you used to learn it.

**Plan:** Lessons 1–3 use modern OpenGL. After lesson 3 we will optionally start a Vulkan side track. You will recognize everything.

---

## Mac Environment Setup (Do This Before Lesson 2)

### Prerequisites
- Homebrew (already installed)
- VS Code (already using)
- Xcode Command Line Tools (run `xcode-select --install` if you haven't)

### Install Step by Step

**1. Install GLFW** (window creation + OpenGL context + keyboard/mouse input)
```bash
brew install glfw
```

**2. Install GLM** (C++ math library — vectors, matrices — mirrors GLSL types exactly)
```bash
brew install glm
```

**3. GLAD** (OpenGL function loader — loads the actual OpenGL function pointers at runtime)
- GLAD is not in Homebrew. Download it from: https://glad.dav1d.de/
- Settings: Language = C/C++, Specification = OpenGL, API gl = Version 3.3, Profile = Core
- Click Generate, download the zip, extract it
- You get: `include/glad/glad.h`, `include/KHR/khrplatform.h`, `src/glad.c`
- Copy `include/` and `src/glad.c` into your project directory

**4. Project structure (we will create this in Lesson 2)**
```
my-cg-project/
├── include/
│   ├── glad/glad.h
│   └── KHR/khrplatform.h
├── src/
│   ├── main.cpp
│   └── glad.c
└── CMakeLists.txt
```

**5. Install CMake** (build system)
```bash
brew install cmake
```

**6. VS Code Extensions to install**
- C/C++ (Microsoft)
- CMake Tools (Microsoft)
- GLSL Lint (optional, for shader syntax highlighting)

**7. Quick test — does OpenGL work on your Mac?**
```bash
# After installing glfw:
brew info glfw
# Should show version 3.x installed
```

---

---

# LESSON 1 — The Full Render Pipeline (Theory)

Lesson 1 is split across multiple files. Read them in order before moving to the code in Lesson 2.

## Reading Order

| File | What it covers |
|---|---|
| [Sept14-L1A-Framebuffer-VSync.md](Sept14-L1A-Framebuffer-VSync.md) | GPU memory, framebuffer, color/depth/stencil buffers, double buffering, vsync, triple buffering, Vulkan present modes |
| [Sept14-L1B-CoordSpaces-TRS.md](Sept14-L1B-CoordSpaces-TRS.md) | TRS matrix derivations from first principles + full coordinate space walk: model → world → eye → clip → NDC → screen with concrete numbers |
| [Sept14-L1C-HomogeneousCoords.md](Sept14-L1C-HomogeneousCoords.md) | Homogeneous coordinates: history (Möbius 1827), projective geometry, w=0 vs w=1, perspective division, pitfalls |
| [Sept14-L1D-NormalVectors.md](Sept14-L1D-NormalVectors.md) | Normal vectors from linear algebra through all 10 graphics uses (lighting, backface culling, collision, reflection, etc.) + inverse transpose derivation |
| [Sept14-L1E-Pipeline-Later-Stages.md](Sept14-L1E-Pipeline-Later-Stages.md) | Projection matrix derivation, clipping (Cohen-Sutherland, Sutherland-Hodgman), depth precision + reversed-Z, rasterization (barycentric, perspective-correct interpolation, early-Z, MSAA), fragment processing (z-buffer, alpha blending, stencil), pipeline history, MVP deep dive |
| [Sept14-L1F-Full-Render-Pipeline.md](Sept14-L1F-Full-Render-Pipeline.md) | **Integrating lesson:** one concrete triangle tracked end-to-end through every stage — CPU upload → vertex shader → primitive assembly → clipping → perspective divide → viewport → rasterization → fragment shader → depth test → blending → framebuffer. Full numeric worked example. Legacy vs modern vs Vulkan mapping. Pitfalls. |

## Lesson 1 Check — Questions to Answer Before Lesson 2

Before you move to coding, make sure you can answer these from memory (rough answers, no formulas needed):

1. Why are homogeneous coordinates 4D for 3D graphics?
2. What does the `w` component become after the perspective projection matrix, and what happens to it during perspective division?
3. What is the difference between a vertex and a fragment?
4. Why doesn't the GPU need objects to be drawn in depth order when using a z-buffer?
5. In the MVP matrix chain `P × V × M × p`, which transform is applied FIRST to the vertex?
6. What is the difference between perspective and orthographic projection?
7. What does the rasterizer interpolate, and why is perspective-correct interpolation needed?
8. After the fragment shader runs, what test determines if the fragment's color gets written to the color buffer?
9. Why does a normal vector transform by the inverse transpose of the model matrix, not the model matrix itself?
10. What is V-Sync and what problem does it solve? What is the cost of enabling it?

---

# LESSON 2 — Environment Setup + Your First Triangle

See: [Sept14-L2-FirstTriangle.md](Sept14-L2-FirstTriangle.md)

Covers: OpenGL context model, GLAD internals, GPU memory and CPU-GPU transfer, VBO lifecycle, VAO internals (what it actually stores), stride/offset memory layout, shader compilation under the hood, full annotated triangle code.

---

# LESSON 3 — The MVP Matrix in Code

See: [Sept14-L3-MVP.md](Sept14-L3-MVP.md)

Covers: why `uniform` variables exist, GLM column-major storage, transform order in GLM (right-multiply convention), lookAt matrix derivation, perspective matrix derivation from similar triangles, full MVP code with animation.


