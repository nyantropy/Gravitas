#pragma once

#include <vector>
#include <algorithm>

#include "ECSWorld.hpp"
#include "HierarchyComponent.h"

// Attach child to parent, updating both HierarchyComponents.
// Creates a HierarchyComponent on either entity if one does not exist yet.
inline void setParent(ECSWorld& world, Entity child, Entity parent)
{
    // Remove child from its current parent's children list
    if (world.hasComponent<HierarchyComponent>(child))
    {
        Entity oldParent = world.getComponent<HierarchyComponent>(child).parent;
        if (oldParent != INVALID_ENTITY && world.hasComponent<HierarchyComponent>(oldParent))
        {
            auto& siblings = world.getComponent<HierarchyComponent>(oldParent).children;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), child), siblings.end());
        }
        world.getComponent<HierarchyComponent>(child).parent = parent;
    }
    else
    {
        world.addComponent(child, HierarchyComponent{ parent, {} });
    }

    // Register child under the new parent
    if (parent != INVALID_ENTITY)
    {
        if (!world.hasComponent<HierarchyComponent>(parent))
            world.addComponent(parent, HierarchyComponent{ INVALID_ENTITY, { child } });
        else
            world.getComponent<HierarchyComponent>(parent).children.push_back(child);
    }
}

// Detach child from its current parent.
// The child's HierarchyComponent is kept with parent reset to INVALID_ENTITY.
inline void removeParent(ECSWorld& world, Entity child)
{
    if (!world.hasComponent<HierarchyComponent>(child))
        return;

    Entity oldParent = world.getComponent<HierarchyComponent>(child).parent;
    if (oldParent != INVALID_ENTITY && world.hasComponent<HierarchyComponent>(oldParent))
    {
        auto& siblings = world.getComponent<HierarchyComponent>(oldParent).children;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), child), siblings.end());
    }

    world.getComponent<HierarchyComponent>(child).parent = INVALID_ENTITY;
}

namespace hierarchy_detail
{
    inline void collectDescendants(ECSWorld& world, Entity e, std::vector<Entity>& out)
    {
        if (!world.hasComponent<HierarchyComponent>(e))
            return;
        for (Entity child : world.getComponent<HierarchyComponent>(e).children)
        {
            out.push_back(child);
            collectDescendants(world, child, out);
        }
    }
}

// Recursively collect all descendants of root (depth-first).
inline std::vector<Entity> getDescendants(ECSWorld& world, Entity root)
{
    std::vector<Entity> result;
    hierarchy_detail::collectDescendants(world, root, result);
    return result;
}
