#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "BoundsComponent.h"
#include "DebugDrawPrimitives.h"
#include "DebugDrawQueueComponent.h"
#include "DebugDrawRenderableComponent.h"
#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "GeometryBindingLifecycle.h"
#include "GraphicsConstants.h"
#include "MaterialComponent.h"
#include "ProceduralMeshComponent.h"
#include "RenderDirtyComponent.h"
#include "RenderGpuComponent.h"
#include "TransformComponent.h"
#include "Vertex.h"

namespace gts::debugdraw
{
    class DebugDrawSystem : public ECSControllerSystem
    {
    public:
        void update(const EcsControllerContext& ctx) override
        {
            if (!ctx.world.hasAny<DebugDrawQueueComponent>())
            {
                destroyAll(ctx.world);
                return;
            }

            DebugDrawQueueComponent& queue = ctx.world.getSingleton<DebugDrawQueueComponent>();
            if (queue.lines.empty())
            {
                destroyAll(ctx.world);
                return;
            }

            std::array<std::vector<DebugDrawLine>, ColorCount> batches{};
            for (const DebugDrawLine& line : queue.lines)
                batches[colorIndex(line.color)].push_back(line);

            for (size_t i = 0; i < batches.size(); ++i)
            {
                if (batches[i].empty())
                {
                    destroyBatch(ctx.world, i);
                    continue;
                }

                const DebugDrawColor color = colorFromIndex(i);
                Entity entity = ensureBatchEntity(ctx.world, color, i);
                syncBatch(ctx.world, entity, color, batches[i]);
            }

            queue.lines.clear();
        }

    private:
        static constexpr entity_id_type InvalidEntityId = std::numeric_limits<entity_id_type>::max();
        static constexpr size_t ColorCount = 9;

        std::array<Entity, ColorCount> batchEntities{
            Entity{InvalidEntityId},
            Entity{InvalidEntityId},
            Entity{InvalidEntityId},
            Entity{InvalidEntityId},
            Entity{InvalidEntityId},
            Entity{InvalidEntityId},
            Entity{InvalidEntityId},
            Entity{InvalidEntityId},
            Entity{InvalidEntityId}
        };

        static size_t colorIndex(DebugDrawColor color)
        {
            return static_cast<size_t>(color);
        }

        static DebugDrawColor colorFromIndex(size_t index)
        {
            return static_cast<DebugDrawColor>(index);
        }

        static bool validBatchEntity(ECSWorld& world, Entity entity)
        {
            return entity.id != InvalidEntityId && world.hasComponent<DebugDrawRenderableComponent>(entity);
        }

        void destroyBatch(ECSWorld& world, size_t index)
        {
            Entity entity = batchEntities[index];
            if (validBatchEntity(world, entity))
                world.destroyEntity(entity);
            batchEntities[index] = Entity{InvalidEntityId};
        }

        void destroyAll(ECSWorld& world)
        {
            for (size_t i = 0; i < batchEntities.size(); ++i)
                destroyBatch(world, i);
        }

        Entity ensureBatchEntity(ECSWorld& world, DebugDrawColor color, size_t index)
        {
            if (validBatchEntity(world, batchEntities[index]))
                return batchEntities[index];

            Entity entity = world.createEntity();
            batchEntities[index] = entity;

            MaterialComponent material;
            material.texturePath = texturePath();
            material.alpha = alpha(color);
            material.doubleSided = true;
            material.vertexColorOnly = true;

            DebugDrawRenderableComponent renderable;
            renderable.color = color;

            ProceduralMeshComponent mesh;
            mesh.useCustomGeometry = true;

            BoundsComponent bounds;
            bounds.min = {-0.01f, -0.01f, -0.01f};
            bounds.max = { 0.01f,  0.01f,  0.01f};

            world.addComponent(entity, TransformComponent{});
            world.addComponent(entity, material);
            world.addComponent(entity, mesh);
            world.addComponent(entity, bounds);
            world.addComponent(entity, renderable);
            gts::rendering::queueProceduralRefresh(world, entity);
            return entity;
        }

        static std::string texturePath()
        {
            return GraphicsConstants::ENGINE_RESOURCES + "/textures/grey_texture.png";
        }

        static glm::vec3 colorValue(DebugDrawColor color)
        {
            switch (color)
            {
                case DebugDrawColor::White:  return {1.0f, 1.0f, 1.0f};
                case DebugDrawColor::Red:    return {1.0f, 0.02f, 0.02f};
                case DebugDrawColor::Green:  return {0.0f, 0.82f, 0.18f};
                case DebugDrawColor::Blue:   return {0.02f, 0.28f, 1.0f};
                case DebugDrawColor::Cyan:   return {0.0f, 0.88f, 1.0f};
                case DebugDrawColor::Yellow: return {1.0f, 0.78f, 0.08f};
                case DebugDrawColor::Orange: return {1.0f, 0.38f, 0.02f};
                case DebugDrawColor::Purple: return {0.68f, 0.24f, 1.0f};
                case DebugDrawColor::Grey:   return {0.55f, 0.58f, 0.62f};
            }
            return {1.0f, 1.0f, 1.0f};
        }

        static float alpha(DebugDrawColor color)
        {
            return color == DebugDrawColor::Grey ? 0.55f : 0.92f;
        }

        struct BoundsAccumulator
        {
            glm::vec3 min{std::numeric_limits<float>::max()};
            glm::vec3 max{std::numeric_limits<float>::lowest()};
            bool valid = false;

            void add(const glm::vec3& point)
            {
                min = glm::min(min, point);
                max = glm::max(max, point);
                valid = true;
            }
        };

        static void syncBatch(ECSWorld& world,
                              Entity entity,
                              DebugDrawColor color,
                              const std::vector<DebugDrawLine>& lines)
        {
            ProceduralMeshComponent& mesh = world.getComponent<ProceduralMeshComponent>(entity);
            BoundsComponent& bounds = world.getComponent<BoundsComponent>(entity);
            DebugDrawRenderableComponent& renderable = world.getComponent<DebugDrawRenderableComponent>(entity);
            MaterialComponent& material = world.getComponent<MaterialComponent>(entity);

            mesh.useCustomGeometry = true;
            mesh.vertices.clear();
            mesh.indices.clear();
            mesh.vertices.reserve(lines.size() * 8);
            mesh.indices.reserve(lines.size() * 36);

            BoundsAccumulator accumulator;
            for (const DebugDrawLine& line : lines)
                appendLineBox(mesh.vertices, mesh.indices, accumulator, line);

            if (accumulator.valid)
            {
                const glm::vec3 pad(0.05f);
                bounds.min = accumulator.min - pad;
                bounds.max = accumulator.max + pad;
            }
            else
            {
                bounds.min = {-0.01f, -0.01f, -0.01f};
                bounds.max = { 0.01f,  0.01f,  0.01f};
            }

            ++renderable.geometryVersion;
            mesh.geometryVersion = renderable.geometryVersion;
            material.texturePath = texturePath();
            material.alpha = alpha(color);
            material.doubleSided = true;
            material.vertexColorOnly = true;

            markExtractionDirty(world, entity);
            gts::rendering::queueProceduralRefresh(world, entity);
        }

        static void markExtractionDirty(ECSWorld& world, Entity entity)
        {
            if (world.hasComponent<RenderDirtyComponent>(entity))
                world.getComponent<RenderDirtyComponent>(entity).transformDirty = true;

            if (world.hasComponent<RenderGpuComponent>(entity))
                world.getComponent<RenderGpuComponent>(entity).commandDirty = true;
        }

        static void appendLineBox(std::vector<Vertex>& vertices,
                                  std::vector<uint32_t>& indices,
                                  BoundsAccumulator& bounds,
                                  const DebugDrawLine& line)
        {
            const glm::vec3 delta = line.end - line.start;
            const float length = glm::length(delta);
            if (length <= 0.0001f)
                return;

            const glm::vec3 axis = delta / length;
            const glm::vec3 fallback = std::abs(glm::dot(axis, glm::vec3(0.0f, 1.0f, 0.0f))) > 0.92f
                ? glm::vec3(1.0f, 0.0f, 0.0f)
                : glm::vec3(0.0f, 1.0f, 0.0f);
            const glm::vec3 side = glm::normalize(glm::cross(axis, fallback));
            const glm::vec3 up = glm::normalize(glm::cross(side, axis));
            const float half = std::max(0.001f, line.thickness * 0.5f);

            std::array<glm::vec3, 8> corners{
                line.start - side * half - up * half,
                line.start + side * half - up * half,
                line.start + side * half + up * half,
                line.start - side * half + up * half,
                line.end - side * half - up * half,
                line.end + side * half - up * half,
                line.end + side * half + up * half,
                line.end - side * half + up * half
            };

            const uint32_t start = static_cast<uint32_t>(vertices.size());
            const glm::vec3 color = colorValue(line.color);
            for (const glm::vec3& corner : corners)
            {
                vertices.push_back({corner, color, {0.0f, 0.0f}});
                bounds.add(corner);
            }

            constexpr std::array<uint32_t, 36> boxIndices{
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
