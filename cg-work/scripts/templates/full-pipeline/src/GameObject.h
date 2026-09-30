#pragma once
#include "Transform.h"
#include "Material.h"
#include "AABB.h"

class Mesh;

struct GameObject {
    Transform transform;
    Material  material;
    Mesh*     mesh       = nullptr;  // non-owning — caller manages Mesh lifetime
    bool      collidable = true;

    AABB getAABB() const { return AABB::fromTransform(transform); }
};
