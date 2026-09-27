cmake_minimum_required(VERSION 3.20)
project(cg-work)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Writes build/compile_commands.json — VSCode reads this to resolve #includes.
# Without this flag, the file is never generated and VSCode shows false errors.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find GLFW and OpenGL once at root. Every project inherits these via target_link_libraries.
find_package(OpenGL REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(glm REQUIRED)

# GLAD: compiled once as a static library, linked into every project.
# PUBLIC on target_include_directories means any target that links glad
# automatically gets vendor/glad/include added to its include path.
add_library(glad STATIC vendor/glad/glad.c)
target_include_directories(glad PUBLIC vendor/glad/include)

# Helper function: each project's CMakeLists.txt calls add_gl_project() instead
# of repeating find_package / target_link_libraries boilerplate every time.
# Usage: add_gl_project(my-project src/main.cpp src/other.cpp ...)
function(add_gl_project name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE OpenGL::GL glfw glad glm::glm)
    # Silence Apple's OpenGL deprecation warnings (they deprecated OpenGL in favour
    # of Metal, but OpenGL 3.3 still works fine on macOS).
    target_compile_definitions(${name} PRIVATE GL_SILENCE_DEPRECATION)
endfunction()

# ── ADD PROJECTS HERE ──────────────────────────────────────────────────────────
# Each line adds one project. CMake reads that project's CMakeLists.txt and
# registers its executable. All projects share this one configure step.
add_subdirectory(projects/01-first-triangle)
add_subdirectory(projects/02-full-render-pipeline-scratch)
