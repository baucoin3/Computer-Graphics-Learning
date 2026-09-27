#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Config.h"

struct Camera {
    glm::vec3 position = glm::vec3(0.0f, 1.0f, 5.0f);
    glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f);

    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

    float fovDegrees = 45.0f;
    float aspectRatio = ASPECT_RATIO;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;

    glm::mat4 getViewMatrix() const {
        return glm::lookAt(position, target, up);
    }

    glm::mat4 getProjectionMatrix() const {
        return glm::perspective(glm::radians(fovDegrees), aspectRatio, nearPlane, farPlane);
    }
};