#pragma once
#include <glm/glm.hpp>
#include "Transform.h"

struct AABB {
    glm::vec3 min, max;

    // Derived from position and scale only. Rotation is intentionally ignored —
    // boxes stay world-axis-aligned regardless of Transform rotation.
    static AABB fromTransform(const Transform& t) {
        glm::vec3 half = t.scale * 0.5f;
        return { t.position - half, t.position + half };
    }

    bool intersects(const AABB& other) const {
        return (min.x <= other.max.x && max.x >= other.min.x) &&
               (min.y <= other.max.y && max.y >= other.min.y) &&
               (min.z <= other.max.z && max.z >= other.min.z);
    }

    bool containsPoint(const glm::vec3& p) const {
        return p.x >= min.x && p.x <= max.x &&
               p.y >= min.y && p.y <= max.y &&
               p.z >= min.z && p.z <= max.z;
    }
};
