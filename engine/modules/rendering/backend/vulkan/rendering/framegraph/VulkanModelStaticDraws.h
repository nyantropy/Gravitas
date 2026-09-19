#pragma once
#include <array>
#include <map>
#include "model/extraction/GtsModelFrameData.h"
#include "RenderCommand.h"
#include "RenderResourceManager.hpp"

// Adapts already-extracted static draws to the existing static command/batch path.
// Called only after the current frame fence. Geometry belongs to MeshManager.
class VulkanModelStaticDraws
{
    public:
    explicit VulkanModelStaticDraws(RenderResourceManager& resources) : resources(resources) {}
    void append(const GtsModelFrameData&    frame,
                uint32_t                    frameIndex,
                std::vector<RenderCommand>& commands,
                MaterialFrameData&          materials)
    {
        std::vector<std::shared_ptr<Placement>> next;
        for (const auto& draw : frame.staticDraws)
        {
            if (!draw.geometry || draw.geometry->profile() != GtsGeometryProfile::Static || !draw.occurrenceOwner ||
                draw.primitiveIndex >= draw.geometry->primitives().size())
                throw std::runtime_error("Static render resource: invalid extracted geometry/placement");
            auto& placement = placements[{draw.occurrenceOwner.get(), draw.occurrenceIndex}];
            if (placement && placement->owner.lock() != draw.occurrenceOwner)
                placement.reset();
            if (!placement)
            {
                auto created       = std::make_shared<Placement>();
                created->resources = &resources;
                created->owner     = draw.occurrenceOwner;
                created->slot      = resources.requestObjectSlot();
                created->allocated = true;
                placement          = std::move(created);
            }
            resources.writeObjectDataForFrameAndMarkStale(
                frameIndex, placement->slot, draw.worldFromGeometry, glm::vec4(1, 1, 0, 0));
            const auto&   range = draw.geometry->primitives()[draw.primitiveIndex];
            RenderCommand command;
            command.meshID         = resources.realizeStaticGeometry(draw.geometry);
            command.firstIndex     = range.firstIndex;
            command.indexCount     = range.indexCount;
            command.objectSSBOSlot = placement->slot;
            command.cameraViewID   = frame.cameraViewID;
            command.material       = draw.material.instance;
            command.materialGpu    = draw.material.gpuHandle;
            command.variantKey     = draw.material.variantKey;
            command.renderQueue    = draw.material.renderQueue;
            command.cameraDepth    = draw.cameraDepth;
            commands.push_back(command);
            materials.upsert(draw.material);
            next.push_back(placement);
        }
        frames.at(frameIndex).swap(next);
        std::erase_if(placements,
                      [](const auto& entry)
                      {
                          return !entry.second || entry.second->owner.expired();
                      });
        if (!frame.staticDraws.empty())
            std::stable_sort(commands.begin(),
                             commands.end(),
                             [](const auto& a, const auto& b)
                             {
                                 const bool at = a.renderQueue == RenderQueue::Transparent;
                                 const bool bt = b.renderQueue == RenderQueue::Transparent;
                                 if (at != bt)
                                     return !at;
                                 if (at && a.cameraDepth != b.cameraDepth)
                                     return a.cameraDepth > b.cameraDepth;
                                 return a.sortKey < b.sortKey;
                             });
    }

    private:
    struct Placement
    {
        std::weak_ptr<const void> owner;
        RenderResourceManager*    resources = nullptr;
        ssbo_id_type              slot      = 0;
        bool                      allocated = false;
        ~Placement()
        {
            if (allocated)
                resources->releaseObjectSlot(slot);
        }
    };
    RenderResourceManager&                                                                       resources;
    std::map<std::pair<const void*, uint32_t>, std::shared_ptr<Placement>>                       placements;
    std::array<std::vector<std::shared_ptr<Placement>>, GraphicsConstants::MAX_FRAMES_IN_FLIGHT> frames;
};
