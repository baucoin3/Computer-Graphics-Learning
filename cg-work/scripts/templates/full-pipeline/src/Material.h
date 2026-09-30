#pragma once
#include <glm/glm.hpp>

struct Material {
    glm::vec3 color     = glm::vec3(1.0f);
    float     ambient   = 0.15f;
    float     diffuse   = 1.0f;
    float     specular  = 0.4f;
    float     shininess = 32.0f;
};
