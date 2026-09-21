# Build System

Everything in `cg-work/` is built with CMake. This document explains what CMake is, why the build is structured the way it is, and every command you need.

---

## What CMake Is

CMake is a **build system generator** — it does not compile code itself. It reads `CMakeLists.txt` files, figures out where all dependencies live on your machine, resolves include paths, and writes the actual compiler instructions (a `Makefile` on macOS/Linux, a `.sln` on Windows).

The mental model is two separate steps:

```
CMakeLists.txt  →  cmake -S . -B build  →  build/Makefile + compile_commands.json
build/Makefile  →  cmake --build build  →  compiled binary
```

**Step 1 — Configure.** CMake reads all `CMakeLists.txt` files in the project, runs `find_package` to locate GLFW and OpenGL on your machine, resolves all include paths, and writes two things into `build/`:
- `Makefile` — compiler instructions for every target
- `compile_commands.json` — VSCode reads this to resolve `#include` paths

**Step 2 — Build.** CMake drives the generated `Makefile`. Only recompiles files that changed since the last build.

Re-run Step 1 only when a `CMakeLists.txt` file changes (adding a source file, adding a project). For normal code edits, Step 2 alone is enough.

---

## What `compile_commands.json` Is

`build/compile_commands.json` is generated during configure. It records, for every `.cpp` file in the project, the exact compiler command used to build it — including every `-I/path/to/headers` flag.

VSCode's C/C++ extension reads this file to resolve `#include` paths. Without it, the extension cannot find `GLFW/glfw3.h` or `glad/glad.h` and shows false red errors everywhere. The file is pointed at via `cg-work/.vscode/settings.json`:

```json
{ "C_Cpp.default.compileCommands": "${workspaceFolder}/build/compile_commands.json" }
```

A `.cpp` file not listed in any `CMakeLists.txt` will not appear in `compile_commands.json` and will show include errors in VSCode even if the code is otherwise correct.

---

## What GLAD Is and Why It Is Vendored

OpenGL is a specification, not a library. The actual implementation lives inside your GPU driver. The function pointers (`glDrawArrays`, `glBindVertexArray`, etc.) only exist after a GL context is created at runtime.

GLAD is a **loader**: after GLFW creates the OpenGL context, you call `gladLoadGLLoader(glfwGetProcAddress)` and GLAD asks the driver for each function pointer by name and stores them in global variables. After that call, every `gl*` function is usable.

GLAD is generated code — you pick your OpenGL version and profile at [glad.dav1d.de](https://glad.dav1d.de) and it produces `glad.c` + `glad/glad.h` for exactly that version. This project uses **OpenGL 3.3 Core**.

GLAD lives in `cg-work/vendor/glad/` because it is not a system library and not a Homebrew package — it is generated per OpenGL version and you own it. It is compiled once into a static library (`libglad.a`) and every project links it automatically via the `add_gl_project()` CMake helper.

---

## Project Structure

```
cg-work/
├── CMakeLists.txt                  — root: finds deps, builds GLAD, defines add_gl_project(), lists all projects
├── vendor/
│   └── glad/
│       ├── glad.c                  — GLAD source, compiled once as libglad.a
│       └── include/
│           ├── glad/glad.h
│           └── KHR/khrplatform.h
├── projects/
│   ├── 01-first-triangle/
│   │   ├── CMakeLists.txt          — one line: add_gl_project(first-triangle src/main.cpp)
│   │   └── src/
│   │       └── main.cpp
│   └── NN-your-project/            — one folder per program, same pattern
│       ├── CMakeLists.txt
│       └── src/
│           └── main.cpp
├── scripts/
│   └── new-project.sh              — scaffolds a new project automatically
├── build/                          — gitignored, never edit manually
└── .vscode/
    └── settings.json               — points VSCode at compile_commands.json
```

### Why Two-Level CMake

The root `CMakeLists.txt` handles shared dependencies (OpenGL, GLFW, GLAD) once. Each project's `CMakeLists.txt` contains a single line. When you run `cmake -S . -B build` from root, CMake walks all subdirectories and writes one `compile_commands.json` covering every `.cpp` file in every project.

### Why `target_*` Functions

The root CMakeLists uses `target_include_directories`, `target_link_libraries`, and `target_compile_definitions` — never the bare `include_directories` or `link_libraries`. The bare versions leak settings into every target in scope. The `target_*` versions are scoped to one named target. `PRIVATE` means only that target. `PUBLIC` means that target and anything linking it — GLAD uses `PUBLIC` on its include path so every linked target automatically gets `vendor/glad/include` without repeating it.

---

## Build Commands

All commands run from `cg-work/`.

### Configure (first time, or after any CMakeLists.txt change)

```bash
cmake -S . -B build
```

### Build a specific project

```bash
cmake --build build --target first-triangle
```

Replace `first-triangle` with the name passed to `add_gl_project()` in that project's `CMakeLists.txt`.

### Build all projects

```bash
cmake --build build
```

### Run a project

```bash
./build/projects/01-first-triangle/first-triangle
```

Binaries land at `build/projects/<folder-name>/<target-name>`.

### Full clean rebuild

```bash
rm -rf build && cmake -S . -B build && cmake --build build
```

---

## Adding a New Project

### Automatic (recommended)

```bash
./scripts/new-project.sh your-project-name
```

The script:
1. Auto-numbers the folder (`03-your-project-name`)
2. Creates `projects/03-your-project-name/src/main.cpp` with a working GLFW+GLAD window boilerplate
3. Creates `projects/03-your-project-name/CMakeLists.txt`
4. Appends `add_subdirectory(projects/03-your-project-name)` to root `CMakeLists.txt`
5. Runs `cmake -S . -B build` so VSCode includes resolve immediately

Then build and run:
```bash
cmake --build build --target your-project-name
./build/projects/03-your-project-name/your-project-name
```

### Manual

1. Create `projects/NN-name/src/main.cpp`
2. Create `projects/NN-name/CMakeLists.txt`:
   ```cmake
   add_gl_project(name src/main.cpp)
   ```
   Multiple source files: `add_gl_project(name src/main.cpp src/other.cpp)`
3. Add to root `CMakeLists.txt`:
   ```cmake
   add_subdirectory(projects/NN-name)
   ```
4. Re-configure:
   ```bash
   cmake -S . -B build
   ```

### Why Manual Registration Is Required

CMake is an explicit build system — it only builds what is declared. Auto-globbing directories (`file(GLOB ...)`) is possible but actively discouraged: globs are evaluated at configure time, so adding a file without re-running configure silently produces stale builds. Explicit `add_subdirectory` means the build is always exactly what is declared.

---

## Command Reference

| Action | Command |
|--------|---------|
| Configure | `cmake -S . -B build` |
| Build one project | `cmake --build build --target <name>` |
| Build all | `cmake --build build` |
| Run | `./build/projects/<NN-name>/<name>` |
| New project (auto) | `./scripts/new-project.sh <name>` |
| Full clean rebuild | `rm -rf build && cmake -S . -B build && cmake --build build` |
