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

static Camera* gCamera     = nullptr;
static float   gLastX      = SCREEN_WIDTH  * 0.5f;
static float   gLastY      = SCREEN_HEIGHT * 0.5f;
static bool    gFirstMouse = true;

void mouseCallback(GLFWwindow*, double xpos, double ypos) {
    float x = static_cast<float>(xpos);
    float y = static_cast<float>(ypos);
    if (gFirstMouse) { gLastX = x; gLastY = y; gFirstMouse = false; }
    float dx =  x - gLastX;
    float dy =  gLastY - y;
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

    GLFWwindow* window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "PROJECT_NAME", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, mouseCallback);

    Camera camera;
    gCamera = &camera;

    // TODO: define vertex/index arrays and create Mesh objects.
    // Example for a pos+normal cube (6 floats per vertex):
    //   Mesh myMesh(vertices, sizeof(vertices), indices, 36);

    std::vector<GameObject> objects;
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

        glm::vec3 oldPos = camera.position;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.processKeyboard(CameraMovement::FORWARD,  deltaTime);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.processKeyboard(CameraMovement::BACKWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.processKeyboard(CameraMovement::LEFT,     deltaTime);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.processKeyboard(CameraMovement::RIGHT,    deltaTime);

        camera.position.y = 1.0f;  // lock player height

        glm::vec3 halfPlayer(0.3f, 0.9f, 0.3f);
        AABB playerBox{ camera.position - halfPlayer, camera.position + halfPlayer };
        for (const auto& obj : objects)
            if (obj.collidable && playerBox.intersects(obj.getAABB()))
                camera.position = oldPos;

        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 view = camera.getViewMatrix();
        glm::mat4 proj = camera.getProjectionMatrix();

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
