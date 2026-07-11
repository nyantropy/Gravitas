#pragma once

#include <unordered_set>
#include <vector>

#include "ECSControllerSystem.hpp"
#include "GeometryBindingLifecycle.h"
#include "MaterialBindingLifecycle.h"
#include "MaterialReferenceComponent.h"
#include "MaterialRuntime.h"

class MaterialBindingSystem : public ECSControllerSystem
{
public:
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

        syncReferencedMaterials(ctx);
    }

private:
    struct MaterialUser
    {
        Entity entity;
        MaterialInstanceHandle material;
    };

    static void syncReferencedMaterials(const EcsControllerContext& ctx)
    {
        gts::rendering::MaterialRuntime& materials = gts::rendering::materialRuntime(ctx.world);
        std::vector<MaterialUser> users;
        std::unordered_set<MaterialInstanceHandle> uniqueHandles;
        std::unordered_set<MaterialInstanceHandle> changedHandles;
        std::unordered_set<MaterialInstanceHandle> topologyChangedHandles;

        ctx.world.forEach<MaterialReferenceComponent>(
            [&](Entity entity, MaterialReferenceComponent& reference)
            {
                const MaterialInstanceHandle resolved = materials.resolveInstanceHandle(reference.material);
                if (resolved != reference.material)
                {
                    reference.material = resolved;
                    gts::rendering::markMaterialRepresentationDirty(ctx.world, entity);
                    gts::rendering::queueRenderObjectRefresh(ctx.world, entity);
                }

                users.push_back({entity, resolved});
                uniqueHandles.insert(resolved);
            });

        for (MaterialInstanceHandle handle : uniqueHandles)
        {
            const MaterialSyncResult result = materials.synchronizeGpuState(handle, ctx.resources);
            if (!result.changed)
                continue;

            changedHandles.insert(handle);
            if (result.topologyChanged)
                topologyChangedHandles.insert(handle);
        }

        for (const MaterialUser& user : users)
        {
            if (changedHandles.find(user.material) == changedHandles.end())
                continue;

            gts::rendering::markMaterialRepresentationDirty(ctx.world, user.entity);
            if (topologyChangedHandles.find(user.material) != topologyChangedHandles.end())
                gts::rendering::queueRenderObjectRefresh(ctx.world, user.entity);
        }
    }
};
