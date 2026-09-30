#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include "Config.h"
#include "Camera.h"
#include "Mesh.h"
#include "Shader.h"
#include "Transform.h"
#include "Material.h"
#include "Light.h"
#include "AABB.h"
#include "GameObject.h"

// GLFW mouse callback needs a raw function pointer — store camera pointer in a global.
static Camera* gCamera     = nullptr;
static float   gLastX      = SCREEN_WIDTH  * 0.5f;
static float   gLastY      = SCREEN_HEIGHT * 0.5f;
static bool    gFirstMouse = true;

void mouseCallback(GLFWwindow*, double xpos, double ypos) {
    float x = static_cast<float>(xpos);
    float y = static_cast<float>(ypos);

    if (gFirstMouse) {
        gLastX = x;
        gLastY = y;
        gFirstMouse = false; 
    }

    float dx =  x - gLastX;
    float dy =  gLastY - y;  // screen Y increases downward; invert so looking up = positive pitch
    gLastX = x;
    gLastY = y;
    gCamera->processMouseMovement(dx, dy);
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Scene Explorer", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    // V sync
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    // Hide cursor and send all mouse movement to the callback.
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, mouseCallback);

    Camera camera;
    gCamera = &camera;

    // Cube geometry shared by all objects in the scene.
    float vertices[] = {
        // pos                normal
        // Front
        -0.5f, -0.5f,  0.5f,   0.0f,  0.0f,  1.0f,
         0.5f, -0.5f,  0.5f,   0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,   0.0f,  0.0f,  1.0f,
        -0.5f,  0.5f,  0.5f,   0.0f,  0.0f,  1.0f,
        // Back
        -0.5f, -0.5f, -0.5f,   0.0f,  0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,   0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,   0.0f,  0.0f, -1.0f,
         0.5f, -0.5f, -0.5f,   0.0f,  0.0f, -1.0f,
        // Left
        -0.5f,  0.5f,  0.5f,  -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,  -1.0f,  0.0f,  0.0f,
        // Right
         0.5f,  0.5f,  0.5f,   1.0f,  0.0f,  0.0f,
         0.5f, -0.5f,  0.5f,   1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,   1.0f,  0.0f,  0.0f,
         0.5f,  0.5f, -0.5f,   1.0f,  0.0f,  0.0f,
        // Bottom
        -0.5f, -0.5f, -0.5f,   0.0f, -1.0f,  0.0f,
         0.5f, -0.5f, -0.5f,   0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,   0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,   0.0f, -1.0f,  0.0f,
        // Top
        -0.5f,  0.5f, -0.5f,   0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f,  0.5f,   0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,   0.0f,  1.0f,  0.0f,
         0.5f,  0.5f, -0.5f,   0.0f,  1.0f,  0.0f,
    };
    unsigned int indices[] = {
         0, 1, 2,  0, 2, 3,   // Front
         4, 5, 6,  4, 6, 7,   // Back
         8, 9,10,  8,10,11,   // Left
        12,13,14, 12,14,15,   // Right
        16,17,18, 16,18,19,   // Bottom
        20,21,22, 20,22,23,   // Top
    };
    Mesh cubeMesh(vertices, sizeof(vertices), indices, 36);

    // --- Scene ---
    // Cubes sit with their bottom face at y=0. Ground top surface is at y=0.
    // Player eye level is locked at y=1.0 (roughly center of a 2-unit-tall capsule).
    std::vector<GameObject> objects;

    auto addCube = [&](glm::vec3 pos, glm::vec3 scale, glm::vec3 color,
                       float shininess = 32.0f, bool collidable = true) {
        GameObject obj;
        obj.transform.position = pos;
        obj.transform.scale    = scale;
        obj.material.color     = color;
        obj.material.shininess = shininess;
        obj.mesh               = &cubeMesh;
        obj.collidable         = collidable;
        objects.push_back(obj);
    };

    // y = 0.5 so cube center is at 0.5, bottom face sits on y=0
    addCube({  0.0f, 0.5f,  0.0f }, { 1.0f, 1.0f, 1.0f }, { 0.8f, 0.2f, 0.2f });         // red
    addCube({  2.5f, 0.5f,  0.0f }, { 1.0f, 1.0f, 1.0f }, { 0.2f, 0.4f, 0.9f });         // blue
    addCube({ -5.5f, 1.0f, -9.0f }, { 1.0f, 2.0f, 1.0f }, { 0.2f, 0.7f, 0.3f });         // green tall
    addCube({  0.0f, 0.0f,  0.0f }, { 20.0f, 0.1f, 20.0f }, { 0.1f, 0.75f, 0.35f }, 8.0f, false); // ground

    Light light;
    Shader shader(SHADER_DIR "blinnPhong.vert", SHADER_DIR "blinnPhong.frag");

    float lastFrame = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        float now       = static_cast<float>(glfwGetTime());
        float deltaTime = now - lastFrame;
        lastFrame       = now;

        glfwPollEvents();

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // Store position before movement so collision can roll it back.
        glm::vec3 oldPos = camera.position;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.processKeyboard(CameraMovement::FORWARD,  deltaTime);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.processKeyboard(CameraMovement::BACKWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.processKeyboard(CameraMovement::LEFT,     deltaTime);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.processKeyboard(CameraMovement::RIGHT,    deltaTime);

        // Lock player height — no gravity, no jumping.
        camera.position.y = 1.0f; //makes it so the player is always on the ground and cant move the camera position into the sky

        // AABB collision: player is a small box around the camera position.
        // If it overlaps any collidable object, discard this frame's movement.
        glm::vec3 halfPlayer(0.3f, 0.9f, 0.3f);
        AABB playerBox{ camera.position - halfPlayer, camera.position + halfPlayer };
        
        for (const auto& obj : objects) {
            if (obj.collidable && playerBox.intersects(obj.getAABB()))
                camera.position = oldPos;
        }

        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 view = camera.getViewMatrix();
        glm::mat4 proj = camera.getProjectionMatrix();

        // Uniforms that are constant for the whole frame — set once before the object loop.
        shader.use();
        shader.setMat4("uView",       view);
        shader.setMat4("uProj",       proj);
        shader.setVec3("uLightPos",   light.position);
        shader.setVec3("uLightColor", light.color);
        shader.setVec3("uViewPos",    camera.position);

        for (const auto& obj : objects) {
            glm::mat4 model  = obj.transform.getModelMatrix();
            glm::mat3 normal = glm::transpose(glm::inverse(glm::mat3(model)));

            shader.setMat4 ("uModel",        model);
            shader.setMat3 ("uNormalMatrix", normal);
            shader.setVec3 ("uColor",        obj.material.color);
            shader.setFloat("uAmbient",      obj.material.ambient);
            shader.setFloat("uSpecular",     obj.material.specular);
            shader.setFloat("uShininess",    obj.material.shininess);

            obj.mesh->draw();
        }

        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}
