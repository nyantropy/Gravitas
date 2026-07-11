#pragma once

#include "ECSControllerSystem.hpp"
#include "GeometryBindingLifecycle.h"
#include "MaterialBindingLifecycle.h"

class MaterialBindingSystem : public ECSControllerSystem
{
public:
    void update(const EcsControllerContext& ctx) override
    {
        auto pendingMaterials = gts::rendering::takeMaterialRefreshes(ctx.world);
        if (pendingMaterials.empty())
            return;

        auto& commands = ctx.world.commands();
        for (entity_id_type entityId : pendingMaterials)
        {
            Entity entity{entityId};
            gts::rendering::syncLegacyMaterialBinding(ctx.world, entity, ctx.resources, commands);
            gts::rendering::syncWorldTextMaterialBinding(ctx.world, entity, ctx.resources, commands);
        }
    }
};
