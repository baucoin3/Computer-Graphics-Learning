# Computer Graphics Learning Workspace

Personal learning workspace for computer graphics — built while re-studying Ryerson/TMU course material (CPS 511, CPS 643) and targeting a career in the graphics industry.

## What Is In This Repo

```
Computer Graphics/
├── AI Lessons/          — deep lesson notes generated from course slides
│   ├── Sept14-L1A-Framebuffer-VSync.md
│   ├── Sept14-L1B-CoordSpaces-TRS.md
│   ├── Sept14-L1C-HomogeneousCoords.md
│   ├── Sept14-L1D-NormalVectors.md
│   ├── Sept14-L1F-Full-Render-Pipeline.md
│   ├── Sept14-L2-FirstTriangle.md
│   └── ...
├── cg-work/             — OpenGL projects and experiments (C++)
│   ├── CMakeLists.txt
│   ├── vendor/          — vendored GLAD (see below)
│   ├── projects/        — one folder per program
│   └── scripts/         — helper scripts (new-project.sh)
├── CLAUDE.md            — AI tutor instructions
├── README.md            — this file
└── BUILD.md             — build system explained + all commands
```

The course PDFs (CPS 511, CPS 643) are not included — they are proprietary Ryerson material.

---

## Mac Setup — Prerequisites

Complete these steps in order before building any project.

### 1. Xcode

Xcode installs Apple's C++ compiler (`clang++`) and the macOS SDK. Without it, `c++` does not exist on the machine.

```bash
# Install from the Mac App Store, then run:
sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer
```

Verify:
```bash
clang++ --version
# Expected: Apple clang version 14.x or later
```

### 2. Homebrew

Package manager for macOS. Used to install CMake and GLFW.

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

After install, follow the printed instructions to add Homebrew to your `PATH` (required on Apple Silicon).

Verify:
```bash
brew --version
```

### 3. CMake

Build system generator. Reads `CMakeLists.txt` and produces the actual compiler instructions.

```bash
brew install cmake
```

Verify:
```bash
cmake --version
# Expected: cmake version 3.20 or later
```

### 4. GLFW

Window creation and input library for OpenGL. Handles creating the OS window, the OpenGL context, keyboard/mouse events.

```bash
brew install glfw
```

Verify:
```bash
brew list glfw
```

### 5. GLAD (already vendored — no install needed)

GLAD is a **loader** — it asks the GPU driver at runtime for every OpenGL function pointer by name, then stores them so you can call `glDrawArrays`, `glBindVertexArray`, etc.

OpenGL ships inside the GPU driver, not as a linkable library. The function pointers only exist after an OpenGL context is created (GLFW creates this). GLAD bridges that gap.

GLAD is pre-generated for **OpenGL 3.3 Core** and lives in `cg-work/vendor/glad/`. It is compiled once into a static library (`libglad.a`) and linked into every project automatically. You do not install it — it is already in the repo.

---

## Building

See [BUILD.md](BUILD.md) for the full build system explanation and all commands.

Quick start:

```bash
cd cg-work
cmake -S . -B build
cmake --build build --target first-triangle
./build/projects/01-first-triangle/first-triangle
```

---

## Adding a New Project

`new-project.sh` creates a numbered project folder, writes boilerplate, and reconfigures CMake automatically. Two modes:

### Blank canvas

```bash
cd cg-work
./scripts/new-project.sh <project-name>
```

Creates a minimal window + clear-color render loop with nothing rendered. Use this when you want to build the structure yourself from scratch.

### Full pipeline boilerplate

```bash
cd cg-work
./scripts/new-project.sh --full-pipeline <project-name>
```

Copies the complete game-structure boilerplate from `scripts/templates/full-pipeline/` into the new project. Everything below is wired and compiles immediately — add objects to the `objects` vector in `main.cpp` and write your shader logic.

| File | What it provides |
|---|---|
| `Shader.h` | Compile/link/error-check + typed uniform setters (`setMat4`, `setVec3`, `setFloat`, etc.) with per-program location cache |
| `Camera.h` | FPS camera — yaw/pitch Euler angles, `processKeyboard(direction, deltaTime)`, `processMouseMovement(dx, dy)` |
| `Mesh.h` | RAII VAO/VBO/EBO wrapper for interleaved pos+normal geometry; single `draw()` call |
| `Transform.h` | Position/rotation/scale → model matrix (`T × Rz × Ry × Rx × S`) |
| `Material.h` | Per-object color, ambient, diffuse, specular, shininess — uploaded as uniforms |
| `Light.h` | World-space light position and color |
| `AABB.h` | Axis-aligned bounding box + `intersects()` / `containsPoint()` |
| `GameObject.h` | `Transform + Material + Mesh* + getAABB()` — put these in a `std::vector` and loop |
| `blinnPhong.vert/frag` | Blinn-Phong shaders — no hard-coded values; all lighting parameters come from uniforms |
| `main.cpp` | Mouse callback, FPS camera wired, delta time, empty `std::vector<GameObject>`, full render loop |

Controls: `WASD` move, mouse look, `Escape` quit. AABB collision prevents walking through collidable objects.

**Reference demo:** `projects/03-full-render-pipeline-game-structure` shows the full boilerplate in action — three lit cubes on a ground plane, walkable with collision.

### Build and run

```bash
cmake --build build --target <project-name>
./build/projects/NN-<project-name>/<project-name>
```

Replace `NN` with the auto-assigned project number printed by the script.

---

## Deleting a Project

```bash
cd cg-work
./scripts/delete-project.sh <exact-folder-name>
```

Pass the full folder name including the number prefix, e.g. `04-my-experiment`. The script will:

1. Confirm the folder exists under `projects/`
2. Confirm the matching `add_subdirectory` line exists in `CMakeLists.txt`
3. Print what it is about to delete and prompt you to **type the folder name again** to confirm
4. Remove the folder, strip the CMake line, and reconfigure

To see existing project names before running:

```bash
ls cg-work/projects/
```
