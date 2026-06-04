#pragma once

#include <cstddef>
#include <limits>
#include <string>
#include <vector>

#include "BoundsComponent.h"
#include "CameraDescriptionComponent.h"
#include "DebugDrawRenderableComponent.h"
#include "DynamicMeshComponent.h"
#include "ECSWorld.hpp"
#include "Entity.h"
#include "MaterialComponent.h"
#include "ParticleEmitterComponent.h"
#include "PhysicsBodyComponent.h"
#include "QuadMeshComponent.h"
#include "SphereColliderComponent.h"
#include "StaticMeshComponent.h"
#include "ToolEntityLabelComponent.h"
#include "TransformComponent.h"

namespace gts::tools
{
    inline constexpr entity_id_type InvalidToolEntityId = std::numeric_limits<entity_id_type>::max();

    inline Entity invalidToolEntity()
    {
        return Entity{InvalidToolEntityId};
    }

    inline bool isValidToolEntity(Entity entity)
    {
        return entity.id != InvalidToolEntityId;
    }

    inline std::string entityDisplayName(ECSWorld& world, Entity entity)
    {
        if (!isValidToolEntity(entity))
            return "NO ENTITY";

        if (world.hasComponent<ToolEntityLabelComponent>(entity))
        {
            const auto& label = world.getComponent<ToolEntityLabelComponent>(entity);
            if (!label.name.empty())
                return label.name;
        }

        return "Entity " + std::to_string(entity.id);
    }

    inline std::string entityCategory(ECSWorld& world, Entity entity)
    {
        if (!isValidToolEntity(entity) || !world.hasComponent<ToolEntityLabelComponent>(entity))
            return {};

        return world.getComponent<ToolEntityLabelComponent>(entity).category;
    }

    inline bool entitySelectable(ECSWorld& world, Entity entity)
    {
        if (!isValidToolEntity(entity))
            return false;

        if (!world.hasComponent<ToolEntityLabelComponent>(entity))
            return true;

        return world.getComponent<ToolEntityLabelComponent>(entity).selectable;
    }

    inline bool isToolInternalEntity(ECSWorld& world, Entity entity)
    {
        if (!isValidToolEntity(entity))
            return false;

        if (world.hasComponent<gts::debugdraw::DebugDrawRenderableComponent>(entity))
            return true;

        return world.hasComponent<ToolEntityLabelComponent>(entity)
            && world.getComponent<ToolEntityLabelComponent>(entity).category == "Tools";
    }

    inline std::string entityComponentBadges(ECSWorld& world, Entity entity)
    {
        if (!isValidToolEntity(entity))
            return "";

        std::vector<std::string> badges;
        if (world.hasComponent<TransformComponent>(entity)) badges.push_back("TR");
        if (world.hasComponent<BoundsComponent>(entity)) badges.push_back("BND");
        if (world.hasComponent<StaticMeshComponent>(entity)) badges.push_back("MESH");
        if (world.hasComponent<QuadMeshComponent>(entity)) badges.push_back("QUAD");
        if (world.hasComponent<DynamicMeshComponent>(entity)) badges.push_back("DYN");
        if (world.hasComponent<MaterialComponent>(entity)) badges.push_back("MAT");
        if (world.hasComponent<ParticleEmitterComponent>(entity)) badges.push_back("PART");
        if (world.hasComponent<CameraDescriptionComponent>(entity)) badges.push_back("CAM");
        if (world.hasComponent<PhysicsBodyComponent>(entity)) badges.push_back("BODY");
        if (world.hasComponent<SphereColliderComponent>(entity)) badges.push_back("COL");

        std::string result;
        for (size_t i = 0; i < badges.size(); ++i)
        {
            if (i > 0)
                result += " ";
            result += badges[i];
        }
        return result;
    }
}
