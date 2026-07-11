#include <cmath>
#include <cstdio>
#include <string>

#include "ECSWorld.hpp"
#include "EcsControllerContext.hpp"
#include "GlmConfig.h"
#include "HierarchyComponent.h"
#include "TransformComponent.h"
#include "TransformDirtyHelpers.h"
#include "TransformHierarchyHelpers.h"
#include "TransformMatrixHelpers.h"
#include "TransformSceneFeature.h"
#include "WorldTransformComponent.h"

namespace
{
    bool require(bool condition, const char* message)
    {
        if (condition)
            return true;

        std::fprintf(stderr, "FAIL: %s\n", message);
        return false;
    }

    bool near(float lhs, float rhs, float epsilon = 0.0005f)
    {
        return std::abs(lhs - rhs) <= epsilon;
    }

    bool nearVec3(const glm::vec3& lhs, const glm::vec3& rhs, float epsilon = 0.0005f)
    {
        return near(lhs.x, rhs.x, epsilon)
            && near(lhs.y, rhs.y, epsilon)
            && near(lhs.z, rhs.z, epsilon);
    }

    glm::vec3 worldPosition(ECSWorld& world, Entity entity)
    {
        return gts::transform::worldPositionFromMatrix(
            world.getComponent<WorldTransformComponent>(entity).matrix);
    }

    Entity makeEntity(ECSWorld& world,
                      const glm::vec3& position,
                      const glm::vec3& scale = glm::vec3(1.0f))
    {
        Entity entity = world.createEntity();
        TransformComponent transform;
        transform.position = position;
        transform.scale = scale;
        world.addComponent(entity, transform);
        return entity;
    }

    void resolveTransforms(ECSWorld& world)
    {
        EcsControllerContext ctx{world};
        world.updateControllers(ctx);
    }

    bool rootEntitiesResolveWorldTransforms()
    {
        ECSWorld world;
        gts::transform::installTransformFeature(world);

        const Entity root = makeEntity(world, {2.0f, 3.0f, 4.0f});
        resolveTransforms(world);

        return require(world.hasComponent<WorldTransformComponent>(root), "root has world transform")
            && require(nearVec3(worldPosition(world, root), {2.0f, 3.0f, 4.0f}),
                       "root world position matches local position");
    }

    bool deepHierarchyResolvesParentBeforeChild()
    {
        ECSWorld world;
        gts::transform::installTransformFeature(world);

        const Entity root = makeEntity(world, {1.0f, 0.0f, 0.0f});
        const Entity child = makeEntity(world, {0.0f, 2.0f, 0.0f});
        const Entity grandchild = makeEntity(world, {0.0f, 0.0f, 3.0f});
        require(gts::transform::attachToParent(world, child, root), "attach child to root");
        require(gts::transform::attachToParent(world, grandchild, child), "attach grandchild to child");

        resolveTransforms(world);

        return require(nearVec3(worldPosition(world, grandchild), {1.0f, 2.0f, 3.0f}),
                       "grandchild world position includes all ancestors");
    }

    bool parentMovementPropagatesToChildren()
    {
        ECSWorld world;
        gts::transform::installTransformFeature(world);

        const Entity parent = makeEntity(world, {1.0f, 0.0f, 0.0f});
        const Entity child = makeEntity(world, {0.0f, 2.0f, 0.0f});
        require(gts::transform::attachToParent(world, child, parent), "attach child for parent movement");
        resolveTransforms(world);
        const uint32_t previousChildVersion = world.getComponent<WorldTransformComponent>(child).version;

        auto& transform = world.getComponent<TransformComponent>(parent);
        transform.position = {5.0f, 0.0f, 0.0f};
        gts::transform::markDirty(world, parent);
        resolveTransforms(world);

        const uint32_t childVersion = world.getComponent<WorldTransformComponent>(child).version;
        return require(nearVec3(worldPosition(world, child), {5.0f, 2.0f, 0.0f}),
                       "parent movement propagates to child world transform")
            && require(childVersion != previousChildVersion,
                       "child world transform version updates when parent moves");
    }

    bool childMovementUpdatesOnlyChildBranch()
    {
        ECSWorld world;
        gts::transform::installTransformFeature(world);

        const Entity parent = makeEntity(world, {1.0f, 0.0f, 0.0f});
        const Entity child = makeEntity(world, {0.0f, 2.0f, 0.0f});
        require(gts::transform::attachToParent(world, child, parent), "attach child for child movement");
        resolveTransforms(world);

        auto& transform = world.getComponent<TransformComponent>(child);
        transform.position = {0.0f, 4.0f, 0.0f};
        gts::transform::markDirty(world, child);
        resolveTransforms(world);

        return require(nearVec3(worldPosition(world, child), {1.0f, 4.0f, 0.0f}),
                       "child local movement updates child world transform");
    }

    bool reparentPreserveLocalIsExplicit()
    {
        ECSWorld world;
        gts::transform::installTransformFeature(world);

        const Entity parentA = makeEntity(world, {10.0f, 0.0f, 0.0f});
        const Entity parentB = makeEntity(world, {20.0f, 0.0f, 0.0f});
        const Entity child = makeEntity(world, {1.0f, 0.0f, 0.0f});
        require(gts::transform::attachToParent(world, child, parentA), "attach child to parent A");
        resolveTransforms(world);

        require(gts::transform::attachToParent(world, child, parentB), "reparent child preserving local");
        resolveTransforms(world);

        return require(nearVec3(worldPosition(world, child), {21.0f, 0.0f, 0.0f}),
                       "default reparent preserves local transform");
    }

    bool reparentPreserveWorldIsExplicit()
    {
        ECSWorld world;
        gts::transform::installTransformFeature(world);

        const Entity parentA = makeEntity(world, {10.0f, 0.0f, 0.0f});
        const Entity parentB = makeEntity(world, {20.0f, 0.0f, 0.0f});
        const Entity child = makeEntity(world, {1.0f, 0.0f, 0.0f});
        require(gts::transform::attachToParent(world, child, parentA), "attach child to parent A");
        resolveTransforms(world);

        auto& childTransform = world.getComponent<TransformComponent>(child);
        childTransform.position = {2.0f, 0.0f, 0.0f};
        gts::transform::markDirty(world, child);

        require(gts::transform::attachToParentPreserveWorld(world, child, parentB),
                "reparent child preserving world");
        resolveTransforms(world);

        const auto& childLocal = world.getComponent<TransformComponent>(child);
        return require(nearVec3(worldPosition(world, child), {12.0f, 0.0f, 0.0f}),
                       "preserve-world reparent keeps current dirty child world position")
            && require(nearVec3(childLocal.position, {-8.0f, 0.0f, 0.0f}),
                       "preserve-world reparent rewrites child local position");
    }

    bool parentDestructionPromotesChildrenToRoots()
    {
        ECSWorld world;
        gts::transform::installTransformFeature(world);

        const Entity parent = makeEntity(world, {5.0f, 0.0f, 0.0f});
        const Entity child = makeEntity(world, {0.0f, 2.0f, 0.0f});
        require(gts::transform::attachToParent(world, child, parent), "attach child before parent destruction");
        resolveTransforms(world);

        world.destroyEntity(parent);
        resolveTransforms(world);

        return require(world.hasComponent<TransformComponent>(child), "child survives parent destruction")
            && require(world.hasComponent<HierarchyComponent>(child), "child keeps hierarchy component")
            && require(world.getComponent<HierarchyComponent>(child).parent == INVALID_ENTITY,
                       "child parent is cleared after parent destruction")
            && require(nearVec3(worldPosition(world, child), {0.0f, 2.0f, 0.0f}),
                       "promoted child keeps local transform as new root world transform");
    }

    bool ancestorCyclesAreRejected()
    {
        ECSWorld world;
        gts::transform::installTransformFeature(world);

        const Entity root = makeEntity(world, {0.0f, 0.0f, 0.0f});
        const Entity child = makeEntity(world, {1.0f, 0.0f, 0.0f});
        const Entity grandchild = makeEntity(world, {2.0f, 0.0f, 0.0f});
        require(gts::transform::attachToParent(world, child, root), "attach child for cycle test");
        require(gts::transform::attachToParent(world, grandchild, child), "attach grandchild for cycle test");

        const bool accepted = gts::transform::attachToParent(world, root, grandchild);
        return require(!accepted, "ancestor cycle is rejected")
            && require(world.getComponent<HierarchyComponent>(root).parent == INVALID_ENTITY,
                       "cycle rejection preserves previous root parent");
    }

    bool negativeScalePropagates()
    {
        ECSWorld world;
        gts::transform::installTransformFeature(world);

        const Entity parent = makeEntity(world, {0.0f, 0.0f, 0.0f}, {-2.0f, 1.0f, 1.0f});
        const Entity child = makeEntity(world, {1.0f, 0.0f, 0.0f});
        require(gts::transform::attachToParent(world, child, parent), "attach child for negative scale");
        resolveTransforms(world);

        return require(nearVec3(worldPosition(world, child), {-2.0f, 0.0f, 0.0f}),
                       "negative parent scale affects child world transform");
    }

    bool nonUniformScalePropagates()
    {
        ECSWorld world;
        gts::transform::installTransformFeature(world);

        const Entity parent = makeEntity(world, {0.0f, 0.0f, 0.0f}, {2.0f, 3.0f, 4.0f});
        const Entity child = makeEntity(world, {1.0f, 1.0f, 1.0f});
        require(gts::transform::attachToParent(world, child, parent), "attach child for non-uniform scale");
        resolveTransforms(world);

        return require(nearVec3(worldPosition(world, child), {2.0f, 3.0f, 4.0f}),
                       "non-uniform parent scale affects child world transform");
    }
}

int main()
{
    bool ok = true;
    ok &= rootEntitiesResolveWorldTransforms();
    ok &= deepHierarchyResolvesParentBeforeChild();
    ok &= parentMovementPropagatesToChildren();
    ok &= childMovementUpdatesOnlyChildBranch();
    ok &= reparentPreserveLocalIsExplicit();
    ok &= reparentPreserveWorldIsExplicit();
    ok &= parentDestructionPromotesChildrenToRoots();
    ok &= ancestorCyclesAreRejected();
    ok &= negativeScalePropagates();
    ok &= nonUniformScalePropagates();

    if (!ok)
        return 1;

    std::printf("TransformHierarchyRuntimeTest passed\n");
    return 0;
}
