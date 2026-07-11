#pragma once

#include <algorithm>

#include "ECSWorld.hpp"
#include "HierarchyComponent.h"
#include "TransformDirtyHelpers.h"
#include "TransformMatrixHelpers.h"
#include "WorldTransformComponent.h"

namespace gts::transform
{
    enum class ReparentTransformPolicy
    {
        PreserveLocal,
        PreserveWorld
    };

    inline HierarchyComponent& ensureHierarchy(ECSWorld& world, Entity entity)
    {
        if (!world.hasComponent<HierarchyComponent>(entity))
            world.addComponent(entity, HierarchyComponent{});
        return world.getComponent<HierarchyComponent>(entity);
    }

    inline bool hasAncestor(ECSWorld& world, Entity entity, Entity ancestor)
    {
        if (entity == INVALID_ENTITY || ancestor == INVALID_ENTITY)
            return false;

        Entity current = entity;
        while (current != INVALID_ENTITY && world.hasComponent<HierarchyComponent>(current))
        {
            current = world.getComponent<HierarchyComponent>(current).parent;
            if (current.id == ancestor.id)
                return true;
        }

        return false;
    }

    inline glm::mat4 resolvedWorldMatrix(ECSWorld& world, Entity entity)
    {
        if (entity == INVALID_ENTITY)
            return glm::mat4(1.0f);

        glm::mat4 matrix = glm::mat4(1.0f);
        if (world.hasComponent<TransformComponent>(entity))
            matrix = world.getComponent<TransformComponent>(entity).getModelMatrix();
        else if (world.hasComponent<WorldTransformComponent>(entity))
            matrix = world.getComponent<WorldTransformComponent>(entity).matrix;

        if (!world.hasComponent<HierarchyComponent>(entity))
            return matrix;

        const Entity parent = world.getComponent<HierarchyComponent>(entity).parent;
        if (parent == INVALID_ENTITY || parent.id == entity.id || hasAncestor(world, parent, entity))
            return matrix;

        return resolvedWorldMatrix(world, parent) * matrix;
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

    inline bool attachToParent(ECSWorld& world,
                               Entity child,
                               Entity parent,
                               ReparentTransformPolicy policy)
    {
        if (child == INVALID_ENTITY || child.id == parent.id)
            return false;

        if (parent != INVALID_ENTITY && hasAncestor(world, parent, child))
            return false;

        glm::mat4 previousWorldMatrix(1.0f);
        if (policy == ReparentTransformPolicy::PreserveWorld)
            previousWorldMatrix = resolvedWorldMatrix(world, child);

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

        if (policy == ReparentTransformPolicy::PreserveWorld &&
            world.hasComponent<TransformComponent>(child))
        {
            const glm::mat4 parentWorldMatrix = resolvedWorldMatrix(world, parent);
            const glm::mat4 localMatrix = glm::inverse(parentWorldMatrix) * previousWorldMatrix;
            TransformComponent& transform = world.getComponent<TransformComponent>(child);
            if (decomposeMatrixToTransform(localMatrix, transform))
                markDirty(world, child);
        }

        queueTransformDirty(world, child);
        return true;
    }

    // Default reparenting preserves local transform values. If the parent has a
    // non-identity world transform, the child will move in world space.
    inline bool attachToParent(ECSWorld& world, Entity child, Entity parent)
    {
        return attachToParent(world, child, parent, ReparentTransformPolicy::PreserveLocal);
    }

    inline bool attachToParentPreserveWorld(ECSWorld& world, Entity child, Entity parent)
    {
        return attachToParent(world, child, parent, ReparentTransformPolicy::PreserveWorld);
    }

    // Removing a hierarchy component or destroying a parent promotes its
    // children to roots. Entity lifetime stays owned by application/gameplay
    // code; the transform module only repairs parent-child links.
    inline void detachHierarchyForRemoval(ECSWorld& world, Entity entity, HierarchyComponent& hierarchy)
    {
        if (hierarchy.parent != INVALID_ENTITY && world.hasComponent<HierarchyComponent>(hierarchy.parent))
        {
            auto& siblings = world.getComponent<HierarchyComponent>(hierarchy.parent).children;
            siblings.erase(std::remove_if(siblings.begin(),
                                          siblings.end(),
                                          [entity](Entity sibling)
                                          {
                                              return sibling.id == entity.id;
                                          }),
                           siblings.end());
        }

        for (Entity child : hierarchy.children)
        {
            if (!world.hasComponent<HierarchyComponent>(child))
                continue;

            HierarchyComponent& childHierarchy = world.getComponent<HierarchyComponent>(child);
            if (childHierarchy.parent.id == entity.id)
                childHierarchy.parent = INVALID_ENTITY;
            queueTransformDirty(world, child);
        }

        hierarchy.parent = INVALID_ENTITY;
        hierarchy.children.clear();
        queueTransformDirty(world, entity);
    }
}
