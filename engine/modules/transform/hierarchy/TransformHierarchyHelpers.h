#pragma once

#include <algorithm>

#include "ECSWorld.hpp"
#include "HierarchyComponent.h"
#include "TransformDirtyHelpers.h"

namespace gts::transform
{
    inline HierarchyComponent& ensureHierarchy(ECSWorld& world, Entity entity)
    {
        if (!world.hasComponent<HierarchyComponent>(entity))
            world.addComponent(entity, HierarchyComponent{});
        return world.getComponent<HierarchyComponent>(entity);
    }

    inline void detachFromParent(ECSWorld& world, Entity child)
    {
        if (!world.hasComponent<HierarchyComponent>(child))
            return;

        HierarchyComponent& childHierarchy = world.getComponent<HierarchyComponent>(child);
        const Entity parent = childHierarchy.parent;
        if (parent != INVALID_ENTITY && world.hasComponent<HierarchyComponent>(parent))
        {
            auto& siblings = world.getComponent<HierarchyComponent>(parent).children;
            siblings.erase(std::remove_if(siblings.begin(),
                                          siblings.end(),
                                          [child](Entity entity)
                                          {
                                              return entity.id == child.id;
                                          }),
                           siblings.end());
        }

        childHierarchy.parent = INVALID_ENTITY;
        queueTransformDirty(world, child);
    }

    inline void attachToParent(ECSWorld& world, Entity child, Entity parent)
    {
        if (child == INVALID_ENTITY || child.id == parent.id)
            return;

        detachFromParent(world, child);
        HierarchyComponent& childHierarchy = ensureHierarchy(world, child);
        childHierarchy.parent = parent;

        if (parent != INVALID_ENTITY)
        {
            auto& children = ensureHierarchy(world, parent).children;
            const auto it = std::find_if(children.begin(),
                                         children.end(),
                                         [child](Entity entity)
                                         {
                                             return entity.id == child.id;
                                         });
            if (it == children.end())
                children.push_back(child);
        }

        queueTransformDirty(world, child);
    }
}
