#pragma once
#include "ECSWorld.hpp"
#include "ModelInstanceComponent.h"
#include "WorldTransformComponent.h"
#include "rendering/core/model/GtsModelRenderExtraction.h"
#include <stdexcept>

inline GtsModelFrameData extractModelFrame(ECSWorld&          world,
                                           view_id_type       camera,
                                           IResourceProvider* resources,
                                           const glm::mat4&   view = glm::mat4(1))
{
    GtsModelFrameData frame;
    frame.cameraViewID = camera;
    world.forEach<ModelInstanceComponent, WorldTransformComponent>(
        [&](Entity entity, const ModelInstanceComponent& model, const WorldTransformComponent& transform)
        {
            auto result = extractModelRenderState(
                model.instance, transform.matrix, gts::rendering::materialRuntime(world), resources);
            if (!result.succeeded())
            {
                const auto& diagnostic = result.diagnostics.front();
                throw std::runtime_error("Entity " + std::to_string(entity.id) + " " + diagnostic.location + ": " +
                                         diagnostic.message);
            }
            for (auto& draw : result.frame.staticDraws)
            {
                const auto& bounds = draw.geometry->bounds();
                draw.cameraDepth = -(view * draw.worldFromGeometry * glm::vec4((bounds.min + bounds.max) * 0.5f, 1)).z;
                frame.staticDraws.push_back(std::move(draw));
            }
            for (auto& draw : result.frame.skinnedDraws)
                frame.skinnedDraws.push_back(std::move(draw));
        });
    return frame;
}
