#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <vector>

#include "BoundsComponent.h"
#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "EngineToolSelectionHelpers.h"
#include "EngineToolSelectionHighlightComponent.h"
#include "EngineToolStateComponent.h"
#include "GeometryBindingLifecycle.h"
#include "GraphicsConstants.h"
#include "DynamicMeshComponent.h"
#include "MaterialComponent.h"
#include "ToolEntityLabelComponent.h"
#include "TransformComponent.h"
#include "TransformDirtyHelpers.h"
#include "TransformMatrixHelpers.h"
#include "Vertex.h"
#include "WorldTransformComponent.h"

namespace gts::tools
{
    class EngineToolSelectionHighlightSystem : public ECSControllerSystem
    {
    public:
        void update(const EcsControllerContext& ctx) override
        {
            EngineToolSelectionHighlightComponent& highlight = ensureHighlight(ctx.world);
            if (!ctx.world.hasAny<EngineToolStateComponent>())
            {
                destroyHighlight(ctx.world, highlight);
                return;
            }

            EngineToolStateComponent& state = ctx.world.getSingleton<EngineToolStateComponent>();
            if (!state.visible
                || !isValidToolEntity(state.selectedEntity)
                || !ctx.world.hasComponent<WorldTransformComponent>(state.selectedEntity)
                || !ctx.world.hasComponent<BoundsComponent>(state.selectedEntity)
                || isToolInternalEntity(ctx.world, state.selectedEntity))
            {
                destroyHighlight(ctx.world, highlight);
                return;
            }

            ensureHighlightEntity(ctx.world, highlight, state.selectedEntity);
            syncHighlight(ctx.world, highlight, state.selectedEntity);
        }

    private:
        static EngineToolSelectionHighlightComponent& ensureHighlight(ECSWorld& world)
        {
            if (!world.hasAny<EngineToolSelectionHighlightComponent>())
                return world.createSingleton<EngineToolSelectionHighlightComponent>();
            return world.getSingleton<EngineToolSelectionHighlightComponent>();
        }

        static void destroyHighlight(ECSWorld& world, EngineToolSelectionHighlightComponent& highlight)
        {
            if (isValidToolEntity(highlight.highlightEntity)
                && world.hasComponent<TransformComponent>(highlight.highlightEntity))
            {
                world.destroyEntity(highlight.highlightEntity);
            }
            highlight = {};
        }

        static void ensureHighlightEntity(ECSWorld& world,
                                          EngineToolSelectionHighlightComponent& highlight,
                                          Entity target)
        {
            if (isValidToolEntity(highlight.highlightEntity)
                && world.hasComponent<TransformComponent>(highlight.highlightEntity))
            {
                return;
            }

            Entity entity = world.createEntity();
            highlight.highlightEntity = entity;
            highlight.targetEntity = invalidToolEntity();

            MaterialComponent material;
            material.texturePath = GraphicsConstants::ENGINE_RESOURCES + "/textures/engine_debug_neutral.png";
            material.tint = {1.0f, 1.0f, 1.0f, 0.92f};
            material.doubleSided = true;
            material.vertexColorOnly = true;

            ToolEntityLabelComponent label;
            label.name = "Tool Selection Highlight";
            label.category = "Tools";
            label.selectable = false;

            world.addComponent(entity, TransformComponent{});
            world.addComponent(entity, material);
            world.addComponent(entity, DynamicMeshComponent{});
            world.addComponent(entity, BoundsComponent{});
            world.addComponent(entity, label);
            gts::rendering::queueDynamicMeshRefresh(world, entity);
            (void)target;
        }

        static void syncHighlight(ECSWorld& world,
                                  EngineToolSelectionHighlightComponent& highlight,
                                  Entity target)
        {
            if (!isValidToolEntity(highlight.highlightEntity)
                || !world.hasComponent<TransformComponent>(highlight.highlightEntity))
            {
                return;
            }

            const WorldTransformComponent& targetTransform = world.getComponent<WorldTransformComponent>(target);
            const BoundsComponent& targetBounds = world.getComponent<BoundsComponent>(target);

            TransformComponent& highlightTransform =
                world.getComponent<TransformComponent>(highlight.highlightEntity);
            gts::transform::decomposeMatrixToTransform(targetTransform.matrix, highlightTransform);
            gts::transform::markDirty(world, highlight.highlightEntity);

            if (highlight.targetEntity.id == target.id)
                return;

            DynamicMeshComponent& mesh =
                world.getComponent<DynamicMeshComponent>(highlight.highlightEntity);
            BoundsComponent& bounds = world.getComponent<BoundsComponent>(highlight.highlightEntity);
            rebuildWireBox(mesh, bounds, targetBounds, ++highlight.geometryVersion);
            highlight.targetEntity = target;
            gts::rendering::queueDynamicMeshRefresh(world, highlight.highlightEntity);
        }

        static void rebuildWireBox(DynamicMeshComponent& mesh,
                                   BoundsComponent& outBounds,
                                   const BoundsComponent& targetBounds,
                                   uint64_t geometryVersion)
        {
            mesh.geometryVersion = geometryVersion;
            mesh.vertices.clear();
            mesh.indices.clear();

            const glm::vec3 size = targetBounds.max - targetBounds.min;
            const float maxDim = std::max({std::abs(size.x), std::abs(size.y), std::abs(size.z), 1.0f});
            const float thickness = std::clamp(maxDim * 0.018f, 0.018f, 0.080f);
            const glm::vec3 pad(thickness);

            const glm::vec3 min = targetBounds.min - pad;
            const glm::vec3 max = targetBounds.max + pad;

            addEdgeBoxes(mesh.vertices, mesh.indices, min, max, thickness);
            outBounds.min = min - pad;
            outBounds.max = max + pad;
        }

        static void addEdgeBoxes(std::vector<Vertex>& vertices,
                                 std::vector<uint32_t>& indices,
                                 const glm::vec3& min,
                                 const glm::vec3& max,
                                 float thickness)
        {
            const std::array<float, 2> ys{min.y, max.y};
            const std::array<float, 2> zs{min.z, max.z};
            for (float y : ys)
                for (float z : zs)
                    addBox(vertices, indices,
                           {min.x, y - thickness, z - thickness},
                           {max.x, y + thickness, z + thickness});

            const std::array<float, 2> xs{min.x, max.x};
            for (float x : xs)
                for (float z : zs)
                    addBox(vertices, indices,
                           {x - thickness, min.y, z - thickness},
                           {x + thickness, max.y, z + thickness});

            for (float x : xs)
                for (float y : ys)
                    addBox(vertices, indices,
                           {x - thickness, y - thickness, min.z},
                           {x + thickness, y + thickness, max.z});
        }

        static void addBox(std::vector<Vertex>& vertices,
                           std::vector<uint32_t>& indices,
                           const glm::vec3& min,
                           const glm::vec3& max)
        {
            const uint32_t start = static_cast<uint32_t>(vertices.size());
            const glm::vec3 color{1.0f, 0.78f, 0.08f};
            vertices.push_back({{min.x, min.y, min.z}, color, {0.0f, 0.0f}});
            vertices.push_back({{max.x, min.y, min.z}, color, {1.0f, 0.0f}});
            vertices.push_back({{max.x, max.y, min.z}, color, {1.0f, 1.0f}});
            vertices.push_back({{min.x, max.y, min.z}, color, {0.0f, 1.0f}});
            vertices.push_back({{min.x, min.y, max.z}, color, {0.0f, 0.0f}});
            vertices.push_back({{max.x, min.y, max.z}, color, {1.0f, 0.0f}});
            vertices.push_back({{max.x, max.y, max.z}, color, {1.0f, 1.0f}});
            vertices.push_back({{min.x, max.y, max.z}, color, {0.0f, 1.0f}});

            const std::array<uint32_t, 36> boxIndices{
                0, 1, 2, 0, 2, 3,
                4, 6, 5, 4, 7, 6,
                0, 4, 5, 0, 5, 1,
                1, 5, 6, 1, 6, 2,
                2, 6, 7, 2, 7, 3,
                3, 7, 4, 3, 4, 0
            };

            for (uint32_t index : boxIndices)
                indices.push_back(start + index);
        }
    };
}
