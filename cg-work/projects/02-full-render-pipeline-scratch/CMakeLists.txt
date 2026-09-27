add_gl_project(full-render-pipeline-scratch src/main.cpp)

target_compile_definitions(full-render-pipeline-scratch PRIVATE
    SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders/")
