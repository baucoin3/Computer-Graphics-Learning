#!/usr/bin/env bash
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

# --- Flag parsing ---
FULL_PIPELINE=false
while [[ "$1" == --* ]]; do
    case "$1" in
        --full-pipeline) FULL_PIPELINE=true; shift ;;
        *) echo "Unknown flag: $1"; exit 1 ;;
    esac
done

if [ -z "$1" ]; then
    echo "Usage: ./scripts/new-project.sh [--full-pipeline] <project-name>"
    exit 1
fi

BASE_NAME="$1"

# Handle duplicate base names: shaders → shaders-2 → shaders-3
FINAL_NAME="$BASE_NAME"
COUNT=1
while find "$ROOT/projects" -maxdepth 1 -type d -name "*-${FINAL_NAME}" 2>/dev/null | grep -q .; do
    COUNT=$(( COUNT + 1 ))
    FINAL_NAME="${BASE_NAME}-${COUNT}"
done

# Auto-number by counting existing project dirs
NUM=$(printf "%02d" $(( $(find "$ROOT/projects" -maxdepth 1 -mindepth 1 -type d 2>/dev/null | wc -l | tr -d ' ') + 1 )))

FOLDER="$ROOT/projects/${NUM}-${FINAL_NAME}"

# Create directory structure
mkdir -p "${FOLDER}/src"
mkdir -p "${FOLDER}/shaders"

# Boilerplate main.cpp: includes + window creation only, nothing else
cat > "${FOLDER}/src/main.cpp" << EOF
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "${FINAL_NAME}", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return -1;
    }

    while (!glfwWindowShouldClose(window)) {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
EOF

# --- Full pipeline: overwrite main.cpp and copy headers + shaders from templates ---
if [ "$FULL_PIPELINE" = true ]; then
    TMPL="$ROOT/scripts/templates/full-pipeline"
    if [ ! -d "$TMPL" ]; then
        echo "Error: templates not found at $TMPL"
        exit 1
    fi
    cp "$TMPL"/src/*.h       "${FOLDER}/src/"
    cp "$TMPL"/src/main.cpp  "${FOLDER}/src/main.cpp"
    cp "$TMPL"/shaders/*     "${FOLDER}/shaders/"
    # Substitute the placeholder window title with the actual project name
    sed -i '' "s/PROJECT_NAME/${FINAL_NAME}/g" "${FOLDER}/src/main.cpp"
fi

# Project-level CMakeLists.txt
cat > "${FOLDER}/CMakeLists.txt" << EOF
add_gl_project(${FINAL_NAME} src/main.cpp)

target_compile_definitions(${FINAL_NAME} PRIVATE
    SHADER_DIR="\${CMAKE_CURRENT_SOURCE_DIR}/shaders/")
EOF

# Append add_subdirectory to root CMakeLists.txt
echo "add_subdirectory(projects/${NUM}-${FINAL_NAME})" >> "$ROOT/CMakeLists.txt"

# Reconfigure so compile_commands.json covers the new project
cmake -S "$ROOT" -B "$ROOT/build"

echo ""
echo "Created:  projects/${NUM}-${FINAL_NAME}/"
echo "Build:    cmake --build build --target ${FINAL_NAME}"
echo "Run:      ./build/projects/${NUM}-${FINAL_NAME}/${FINAL_NAME}"
