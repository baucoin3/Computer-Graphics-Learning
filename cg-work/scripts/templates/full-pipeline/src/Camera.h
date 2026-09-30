#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Config.h"

enum class CameraMovement { FORWARD, BACKWARD, LEFT, RIGHT };

struct Camera {
    glm::vec3 position  = glm::vec3(0.0f, 1.0f,  5.0f);
    glm::vec3 front     = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 up        = glm::vec3(0.0f, 1.0f,  0.0f);
    glm::vec3 right     = glm::vec3(1.0f, 0.0f,  0.0f);
    glm::vec3 worldUp   = glm::vec3(0.0f, 1.0f,  0.0f);

    float yaw              = -90.0f;  // -90 so front starts pointing -Z
    float pitch            =   0.0f;
    float movementSpeed    =   5.0f;
    float mouseSensitivity =   0.1f;
    float fovDegrees       =  45.0f;
    float aspectRatio      = ASPECT_RATIO;
    float nearPlane        =   0.1f;
    float farPlane         = 100.0f;

    Camera() { updateVectors(); }

    // Move along the camera's axes. Caller is responsible for locking Y if needed.
    void processKeyboard(CameraMovement dir, float deltaTime) {
        float v = movementSpeed * deltaTime;
        if (dir == CameraMovement::FORWARD)  position += front * v;
        if (dir == CameraMovement::BACKWARD) position -= front * v;
        if (dir == CameraMovement::LEFT)     position -= right * v;
        if (dir == CameraMovement::RIGHT)    position += right * v;
    }

    // dx/dy are pixel deltas from the OS mouse callback (screen-Y is inverted).
    void processMouseMovement(float dx, float dy, bool constrainPitch = true) {
        yaw   += dx * mouseSensitivity;
        pitch += dy * mouseSensitivity;
        if (constrainPitch) pitch = glm::clamp(pitch, -89.0f, 89.0f);
        updateVectors();
    }

    glm::mat4 getViewMatrix()       const { return glm::lookAt(position, position + front, up); }
    glm::mat4 getProjectionMatrix() const { return glm::perspective(glm::radians(fovDegrees), aspectRatio, nearPlane, farPlane); }

private:
    // Recompute front/right/up from yaw and pitch (Euler angles in degrees).
    void updateVectors() {
        glm::vec3 f;
        f.x   = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        f.y   = sin(glm::radians(pitch));
        f.z   = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(f);
        right = glm::normalize(glm::cross(front, worldUp));
        up    = glm::normalize(glm::cross(right, front));
    }
};
