#pragma once

#include <vector>

#include "ECSControllerSystem.hpp"
#include "GeometryBindingLifecycle.h"
#include "MaterialBindingLifecycle.h"
#include "MaterialReferenceComponent.h"
#include "MaterialRuntime.h"

class MaterialBindingSystem : public ECSControllerSystem
{
public:
    using Metrics = gts::rendering::MaterialBindingMetrics;

    void update(const EcsControllerContext& ctx) override
    {
        auto pendingMaterials = gts::rendering::takeMaterialRefreshes(ctx.world);

        auto& commands = ctx.world.commands();
        for (entity_id_type entityId : pendingMaterials)
        {
            Entity entity{entityId};
            gts::rendering::syncLegacyMaterialBinding(ctx.world, entity, ctx.resources, commands);
            gts::rendering::syncWorldTextMaterialBinding(ctx.world, entity, ctx.resources, commands);
        }

        drainQueuedMaterials(ctx);
    }

    static Metrics getLastMetrics()
    {
        return lastMetrics;
    }

private:
    inline static Metrics lastMetrics{};

    static void drainQueuedMaterials(const EcsControllerContext& ctx)
    {
        gts::rendering::MaterialRuntime& materials = gts::rendering::materialRuntime(ctx.world);
        auto& lifecycleState = gts::rendering::geometryBindingLifecycleState(ctx.world);
        Metrics& metrics = lifecycleState.materialMetrics;
        std::vector<MaterialInstanceHandle> queuedMaterials = materials.takeQueuedMaterialSyncs();
        metrics.queuedMaterials += static_cast<uint32_t>(queuedMaterials.size());

        for (MaterialInstanceHandle handle : queuedMaterials)
        {
            if (!handle.valid())
                continue;

            if (!materials.isInstanceAlive(handle))
            {
                processDestroyedMaterial(ctx, handle, metrics);
                continue;
            }

            if (!materials.materialNeedsGpuSync(handle))
                continue;

            const MaterialSyncResult result = materials.synchronizeGpuState(handle, ctx.resources);
            metrics.synchronizedMaterials += 1;
            if (!result.changed || !result.topologyChanged)
                continue;

            invalidateMaterialUsers(ctx, handle, metrics);
        }

        lastMetrics = metrics;
        lifecycleState.materialMetrics = {};
    }

    static void processDestroyedMaterial(const EcsControllerContext& ctx,
                                         MaterialInstanceHandle destroyed,
                                         Metrics& metrics)
    {
        gts::rendering::MaterialRuntime& materials = gts::rendering::materialRuntime(ctx.world);
        const std::vector<Entity> users = gts::rendering::materialUsers(ctx.world, destroyed);
        if (users.empty())
            return;

        const MaterialInstanceHandle fallback = materials.defaultMaterial();
        if (materials.materialNeedsGpuSync(fallback))
        {
            materials.synchronizeGpuState(fallback, ctx.resources);
            metrics.synchronizedMaterials += 1;
        }

        for (Entity entity : users)
        {
            if (!ctx.world.hasComponent<MaterialReferenceComponent>(entity))
            {
                gts::rendering::unregisterMaterialUser(ctx.world, entity);
                continue;
            }

            MaterialReferenceComponent& reference =
                ctx.world.getComponent<MaterialReferenceComponent>(entity);
            if (reference.material != destroyed)
            {
                gts::rendering::registerMaterialUser(ctx.world, entity, reference.material, false);
                continue;
            }

            reference.material = fallback;
            gts::rendering::registerMaterialUser(ctx.world, entity, fallback, false);
            gts::rendering::markMaterialRepresentationDirty(ctx.world, entity);
            gts::rendering::queueRenderObjectRefresh(ctx.world, entity);
            metrics.fallbackSubstitutions += 1;
            metrics.userInvalidations += 1;
        }
    }

    static void invalidateMaterialUsers(const EcsControllerContext& ctx,
                                        MaterialInstanceHandle material,
                                        Metrics& metrics)
    {
        const std::vector<Entity> users = gts::rendering::materialUsers(ctx.world, material);
        for (Entity entity : users)
        {
            if (!ctx.world.hasComponent<MaterialReferenceComponent>(entity))
                continue;

            const MaterialReferenceComponent& reference =
                ctx.world.getComponent<MaterialReferenceComponent>(entity);
            if (reference.material != material)
                continue;

            gts::rendering::markMaterialRepresentationDirty(ctx.world, entity);
            gts::rendering::queueRenderObjectRefresh(ctx.world, entity);
            metrics.userInvalidations += 1;
        }
    }
};
