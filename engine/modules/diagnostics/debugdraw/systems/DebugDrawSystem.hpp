#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "DebugDrawQueueComponent.h"
#include "ECSControllerSystem.hpp"
#include "Entity.h"

struct GtsStaticVertex;

namespace gts::debugdraw
{
    class DebugDrawSystem : public ECSControllerSystem
    {
        public:
        void update(const EcsControllerContext& ctx) override;

        private:
        static constexpr entity_id_type InvalidEntityId = std::numeric_limits<entity_id_type>::max();
        static constexpr size_t         ColorCount      = DebugDrawColorCount;

        std::array<Entity, ColorCount> batchEntities{Entity{InvalidEntityId},
                                                     Entity{InvalidEntityId},
                                                     Entity{InvalidEntityId},
                                                     Entity{InvalidEntityId},
                                                     Entity{InvalidEntityId},
                                                     Entity{InvalidEntityId},
                                                     Entity{InvalidEntityId},
                                                     Entity{InvalidEntityId},
                                                     Entity{InvalidEntityId}};
        std::array<bool, ColorCount>   batchCacheValid{};
        std::array<size_t, ColorCount> batchHashes{};
        std::array<size_t, ColorCount> batchLineCounts{};

        static size_t colorIndex(DebugDrawColor color);

        static DebugDrawColor colorFromIndex(size_t index);

        static bool validBatchEntity(ECSWorld& world, Entity entity);

        void destroyBatch(ECSWorld& world, size_t index);

        void destroyAll(ECSWorld& world);

        Entity ensureBatchEntity(ECSWorld& world, DebugDrawColor color, size_t index);

        static std::string texturePath();

        static glm::vec3 colorValue(DebugDrawColor color);

        static float opacity(DebugDrawColor color);

        struct BoundsAccumulator;

        static void
        syncBatch(ECSWorld& world, Entity entity, DebugDrawColor color, const std::vector<DebugDrawLine>& lines);

        static void markExtractionDirty(ECSWorld& world, Entity entity);

        static void appendLineBox(std::vector<GtsStaticVertex>& vertices,
                                  std::vector<uint32_t>&        indices,
                                  BoundsAccumulator&            bounds,
                                  const DebugDrawLine&          line);
    };
} // namespace gts::debugdraw
