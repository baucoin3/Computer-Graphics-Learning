#pragma once
#include <glm/glm.hpp>

struct Light {
    glm::vec3 position = glm::vec3(3.0f, 5.0f, 4.0f);
    glm::vec3 color    = glm::vec3(1.0f);
};
