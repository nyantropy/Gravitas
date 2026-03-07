#pragma once

#include <vector>
#include <limits>

#include "Entity.h"
#include "Types.h"

// Sentinel value meaning "no parent / root entity"
inline constexpr Entity INVALID_ENTITY = Entity{ std::numeric_limits<entity_id_type>::max() };

// Optional component that links an entity into a parent-child tree.
// Entities without this component are treated as root-level and their
// behaviour is identical to before hierarchy support was added.
struct HierarchyComponent
{
    Entity              parent   = INVALID_ENTITY;
    std::vector<Entity> children;
};
