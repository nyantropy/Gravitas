#pragma once

#include "GtsModelSkinnedPresentation.h"
#include "model/runtime/GtsModelInstance.h"

// Temporary immutable frame snapshot for the existing palette-indexed skinned presentation.
// CPU occurrence state in GtsModelInstance remains authoritative.
inline std::shared_ptr<const std::vector<GtsSkinPalette>>
snapshotGtsModelInstancePalettes(const GtsModelInstance& instance)
{
    if (!instance.worldMaterialsValid())
        throw std::runtime_error("Model instance material scope has expired; presentation must be rebound/recreated");
    size_t count = 0;
    for (const auto& occurrence : instance.skeletonOccurrences())
        count += occurrence.palettes().size();
    auto result = std::make_shared<std::vector<GtsSkinPalette>>(count);
    for (const auto& occurrence : instance.skeletonOccurrences())
        for (const auto& entry : occurrence.palettes())
            result->at(entry.skinBindingIndex) = entry.palette;
    return result;
}
