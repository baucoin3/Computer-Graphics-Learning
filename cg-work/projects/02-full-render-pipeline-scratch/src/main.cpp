#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Config.h"
#include "Transform.h"
#include "Camera.h"
#include "Shader.h"

int main() {
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Full pipeline", nullptr, nullptr);

    if (!window){
        std::cerr << "Failed to create GLFW WINDOW";
        return -1;
    }


    glfwMakeContextCurrent(window);
    // v-sync turned on
    glfwSwapInterval(1);  // cap to monitor refresh rate (60fps)

    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)){
        std::cerr << "Failed to initialize GLAD";
        return -1;
    }

    // Enable back facing culling for optimzation
    glEnable(GL_CULL_FACE);
    // Discard triangles whose vertices wind clockwise from the cameras perspective
    glCullFace(GL_BACK);

    // You dont always want depth test for instance a hud you draw last in front of everything. or the sky you draw first and everything else in front of it
    // depth buffer stores the last depth of each fragment and does comparison each new frame without it it just draws each thing on top of each other
    glEnable(GL_DEPTH_TEST);

    glViewport(0,0,SCREEN_WIDTH,SCREEN_HEIGHT);

    // setup models
    Transform redCube;
    Transform blueCube;
    blueCube.position = glm::vec3(2.5f, 0.0f,0.0f);
    blueCube.scale = glm::vec3(1.0f, 2.0f, 1.0f);
    blueCube.rotation = glm::vec3(45.0f, 0.0f, 0.0f);

 
    // Setup Camera defaults are fine as is
    Camera camera;

    // setup cube vertices
    // Have to go CCW vertex for openGL
    float vertices[] = {                                                                                                                  
      // Front face  (normal  0, 0,+1)                                                                                                  
     -0.5f, -0.5f,  0.5f,   0.0f,  0.0f,  1.0f,                                                                                         
      0.5f, -0.5f,  0.5f,   0.0f,  0.0f,  1.0f,                                                                                         
      0.5f,  0.5f,  0.5f,   0.0f,  0.0f,  1.0f,                                                                                         
     -0.5f,  0.5f,  0.5f,   0.0f,  0.0f,  1.0f,                                                                                         
                                                                                                                                        
      // Back face   (normal  0, 0,-1)                                                                                                  
     -0.5f, -0.5f, -0.5f,   0.0f,  0.0f, -1.0f,                                                                                         
     -0.5f,  0.5f, -0.5f,   0.0f,  0.0f, -1.0f,                                                                                         
      0.5f,  0.5f, -0.5f,   0.0f,  0.0f, -1.0f,                                                                                         
      0.5f, -0.5f, -0.5f,   0.0f,  0.0f, -1.0f,                                                                                         
                                                                                                                                        
      // Left face   (normal -1, 0, 0)                                                                                                  
     -0.5f,  0.5f,  0.5f,  -1.0f,  0.0f,  0.0f,                                                                                         
     -0.5f,  0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,                                                                                         
     -0.5f, -0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,                                                                                         
     -0.5f, -0.5f,  0.5f,  -1.0f,  0.0f,  0.0f,                                                                                         
                                                                                                                                        
      // Right face  (normal +1, 0, 0)                                                                                                  
      0.5f,  0.5f,  0.5f,   1.0f,  0.0f,  0.0f,                                                                                         
      0.5f, -0.5f,  0.5f,   1.0f,  0.0f,  0.0f,                                                                                         
      0.5f, -0.5f, -0.5f,   1.0f,  0.0f,  0.0f,                                                                                         
      0.5f,  0.5f, -0.5f,   1.0f,  0.0f,  0.0f,                                                                                         
                                                                                                                                        
      // Bottom face (normal  0,-1, 0)                                                                                                  
     -0.5f, -0.5f, -0.5f,   0.0f, -1.0f,  0.0f,                                                                                         
      0.5f, -0.5f, -0.5f,   0.0f, -1.0f,  0.0f,                                                                                         
      0.5f, -0.5f,  0.5f,   0.0f, -1.0f,  0.0f,                                                                                         
     -0.5f, -0.5f,  0.5f,   0.0f, -1.0f,  0.0f,                                                                                         
                                                                                                                                        
      // Top face    (normal  0,+1, 0)                                                                                                  
     -0.5f,  0.5f, -0.5f,   0.0f,  1.0f,  0.0f,                                                                                         
     -0.5f,  0.5f,  0.5f,   0.0f,  1.0f,  0.0f,                                                                                         
      0.5f,  0.5f,  0.5f,   0.0f,  1.0f,  0.0f,                                                                                         
      0.5f,  0.5f, -0.5f,   0.0f,  1.0f,  0.0f,                                                                                         
  };          
  
//   forming the triangles for each face (each face is 2 triangles)
    unsigned int indices[] = {
        // Front face
            0, 1, 2, 0, 2, 3,
        // Back
            4,5,6, 4,6,7,
        // Left
            8,9,10, 8,10,11,
        // Right
            12,13,14, 12,14,15,
        // Bottom
            16,17,18, 16,18, 19,
        // top
            20,21,22, 20, 22, 23
    };

    unsigned int VAO, VBO, EBO;

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1,&EBO);

    // Start recording info into this VAO
    glBindVertexArray(VAO);

    // Upload vertex data to gpu
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // upload index data to gpu auto attaches ebo to this current VAO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // tell gpu how to read the positions and normals in vao
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);


    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6* sizeof(float), (void*)(3* sizeof(float)));
    glEnableVertexAttribArray(1);

    // Unbind VAO 
    glBindVertexArray(0);

    // SHADER_DIR defined in the build to the shaders folder
    Shader shader(SHADER_DIR "cubePhong.vert", SHADER_DIR "cubePhong.frag");

    // Render Loop
    while (!glfwWindowShouldClose(window)){ 
        // Needs to be called for IO 
        glfwPollEvents();

        // Clear the back buffer with the color we just wrote.
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Render setup and draw call
        glm::mat4 view = camera.getViewMatrix();
        glm::mat4 projection = camera.getProjectionMatrix();

        // Red Cube 
        glm::mat4 redCubeModel = redCube.getModelMatrix();
        glm::mat3 redCubeNormals = glm::transpose(glm::inverse(glm::mat3(redCubeModel)));

        // Set up shader
        shader.use();

        // upload uniforms to shader
        // Get unifpr,lcoation is slow you can do this outside the render loop or cache it to make it faster
        glUniformMatrix4fv(glGetUniformLocation(shader.id, "uModel"), 1, GL_FALSE, glm::value_ptr(redCubeModel));
        glUniformMatrix4fv(glGetUniformLocation(shader.id, "uView"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shader.id, "uProj"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix3fv(glGetUniformLocation(shader.id, "uNormalMatrix"), 1, GL_FALSE, glm::value_ptr(redCubeNormals));
        glUniform3f(glGetUniformLocation(shader.id, "uColor"), 0.8f, 0.2f, 0.2f); //hard code red here could also add that to a cube class for color eventually 

        glUniform3f(glGetUniformLocation(shader.id, "uLightPos"), 3.0f, 5.0f, 4.0f);
        glUniform3f(glGetUniformLocation(shader.id, "uViewPos"), camera.position.x, camera.position.y, camera.position.z);

        // Bind vertices/vao
        glBindVertexArray(VAO);


        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

        // Blue Cube
        glm::mat4 blueCubeModel = blueCube.getModelMatrix();

        // transpose the inverse matrix 3 (remove the last column to ignore the translation normals cant have length)
        glm::mat3 blueCubeNormals = glm::transpose(glm::inverse(glm::mat3(blueCubeModel)));
        
        glUniformMatrix4fv(glGetUniformLocation(shader.id, "uModel"), 1, GL_FALSE, glm::value_ptr(blueCubeModel));
        glUniformMatrix3fv(glGetUniformLocation(shader.id, "uNormalMatrix"), 1, GL_FALSE, glm::value_ptr(blueCubeNormals));
        glUniform3f(glGetUniformLocation(shader.id, "uColor"), 0.2f, 0.4f, 0.9f); // hard code blue

        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

        // Want to render this new frame so swap the back and front buffers
        glfwSwapBuffers(window);

    }


    // Cleanup
    glfwTerminate();

    return 0;
}
