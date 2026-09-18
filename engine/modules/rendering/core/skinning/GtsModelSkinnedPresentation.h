#pragma once

#include <map>
#include <stdexcept>
#include "SkinnedFrameData.h"
#include "rendering/core/material/model/GtsModelMaterialRealization.h"

// Temporary adapter to the existing by-value presentation. No geometry/material interpretation.
inline std::shared_ptr<const GtsSkinnedModelData>
makeGtsModelSkinnedPresentation(const GtsRealizedModel& model, const GtsRealizedModelMaterials& materials)
{
    auto                                              result = std::make_shared<GtsSkinnedModelData>();
    std::map<std::pair<uint32_t, uint32_t>, uint32_t> indices;
    for (const auto& occurrence : model.occurrences)
    {
        const auto& geometry = model.geometry[occurrence.geometryIndex];
        const auto* mesh     = geometry.skinnedMesh();
        if (!mesh || !occurrence.skinBindingIndex)
            throw std::runtime_error("Skinned presentation requires bound geometry at node " +
                                     std::to_string(occurrence.modelNodeIndex));
        auto& part           = result->parts.emplace_back(GtsSkinnedMeshPart{*mesh, *occurrence.skinBindingIndex});
        result->bindingCount = std::max(result->bindingCount, *occurrence.skinBindingIndex + 1);
        for (uint32_t p = 0; p < part.mesh.primitives.size(); ++p)
        {
            const auto handle = materials.materialFor(model, occurrence.geometryIndex, p);
            if (!handle.valid())
                throw std::runtime_error("Expired or foreign model material association");
            auto [it, inserted] = indices.emplace(std::make_pair(handle.id, handle.generation),
                                                  static_cast<uint32_t>(result->materials.size()));
            if (inserted)
                result->materials.push_back(materials.frameStateFor(model, occurrence.geometryIndex, p));
            part.mesh.primitives[p].materialIndex = it->second;
        }
    }
    return result;
}
