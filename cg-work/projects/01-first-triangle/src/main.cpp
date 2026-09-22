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
    // the context which is a record of gpu driver (running on cpu) of everything that was configured VAO, shader program thats active etc
    // ** this context is the cpu side gpu driver (lives on cpu) and contains the pointers to the gpu (i.e. where in vram is this)
    glfwMakeContextCurrent(window);

    // context must exist before checking if glad exists
    
    // ── LOAD OPENGL FUNCTIONS VIA GLAD ───────────────────────────────────────
    // On Windows/Mac, OpenGL functions are not available by default.
    // GLAD loads the function pointers at runtime from the GPU driver.
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return -1;
    }
    //  what happens when someone resizes the window
    // Could potentially have 2 view ports for split screen
    // If i change this for some reason maybe mac related it always stays in the centre
    
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
    
    // actually upload the data to the buffer
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