#pragma once

#include <vector>

#include "Entity.h"

// Optional component that links an entity into a parent-child tree.
// Entities without this component are treated as root-level and their
// behaviour is identical to before hierarchy support was added.
struct HierarchyComponent
{
    Entity              parent   = INVALID_ENTITY;
    std::vector<Entity> children;
};
