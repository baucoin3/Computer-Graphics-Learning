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

```bash
cd cg-work
./scripts/new-project.sh your-project-name
cmake --build build --target your-project-name
./build/projects/NN-your-project-name/your-project-name
```

The script creates the folder structure, boilerplate, and reconfigures CMake automatically.
