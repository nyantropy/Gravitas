#pragma once

#include <vector>

#include "Entity.h"

// Optional component that links an entity into a parent-child transform tree.
// Entities without this component are treated as root-level transforms.
struct HierarchyComponent
{
    Entity              parent   = INVALID_ENTITY;
    std::vector<Entity> children;
};
