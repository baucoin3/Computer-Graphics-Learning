add_gl_project(full-render-pipeline-game-structure src/main.cpp)

target_compile_definitions(full-render-pipeline-game-structure PRIVATE
    SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders/")
