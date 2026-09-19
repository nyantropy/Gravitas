#pragma once

#include "GtsSkeletonOccurrence.h"
#include "assets/realization/model/GtsRealizedModel.h"
#include "GtsRealizedModelMaterials.h"

struct GtsModelInstanceStatus
{
    std::vector<GtsModelDiagnostic> diagnostics;
    bool                            succeeded() const
    {
        return diagnostics.empty();
    }
};

class GtsModelInstanceRuntime;

// One mutable occurrence. No world transform: the entity's TransformComponent remains authoritative.
class GtsModelInstance
{
    public:
    GtsModelInstance(const GtsModelInstance&)            = delete;
    GtsModelInstance& operator=(const GtsModelInstance&) = delete;

    const GtsModelHandle& model() const
    {
        return resource;
    }
    const std::shared_ptr<const GtsRealizedModel>& geometry() const
    {
        return realized;
    }
    const std::shared_ptr<const GtsRealizedModelMaterials>& materials() const
    {
        return materialSet;
    }
    bool worldMaterialsValid() const
    {
        return materialSet && materialSet->valid() &&
               (!materialOverride || materialSet->isMaterialAlive(*materialOverride));
    }
    std::span<const GtsSkeletonOccurrence> skeletonOccurrences() const
    {
        return occurrences;
    }
    GtsSkeletonOccurrenceReference skeletonOccurrence(uint32_t use) const;
    // Explicit resource binding indices; no assumed palette-array ordering.
    const GtsSkinPalette*  palette(uint32_t skinBindingIndex) const;
    const GtsSkinPalette*  paletteForOccurrence(uint32_t occurrenceIndex) const;
    MaterialInstanceHandle materialFor(uint32_t occurrenceIndex, uint32_t primitiveIndex) const;

    [[nodiscard]] GtsModelInstanceStatus
    setMaterialOverride(MaterialInstanceHandle material, std::weak_ptr<const int> runtimeScope);
    void clearMaterialOverride() { materialOverride.reset(); }

    [[nodiscard]] GtsModelInstanceStatus play(uint32_t use, GtsModelClipReference clip);
    [[nodiscard]] GtsModelInstanceStatus stop(uint32_t use);
    [[nodiscard]] GtsModelInstanceStatus setPlaybackPolicy(uint32_t use, float speed, bool looping);
    [[nodiscard]] GtsModelInstanceStatus updateAnimations(double deltaSeconds);
    [[nodiscard]] GtsModelInstanceStatus rebindMaterials(std::shared_ptr<const GtsRealizedModelMaterials> materials);

    private:
    friend class GtsModelInstanceRuntime;
    GtsModelInstance(GtsModelHandle                                   model,
                     std::shared_ptr<const GtsRealizedModel>          geometry,
                     std::shared_ptr<const GtsRealizedModelMaterials> materials);
    GtsModelInstanceStatus initialize();
    GtsModelInstanceStatus evaluatePalettes(GtsSkeletonOccurrence& occurrence) const;
    GtsModelInstanceStatus
    failure(const std::string& code, const std::string& message, std::optional<uint32_t> use = {}) const;

    GtsModelHandle                                   resource;
    std::shared_ptr<const GtsRealizedModel>          realized;
    std::shared_ptr<const GtsRealizedModelMaterials> materialSet;
    std::optional<MaterialInstanceHandle>            materialOverride;
    std::vector<GtsSkeletonOccurrence>               occurrences; // Sized once; updates preserve occurrence addresses.
    std::shared_ptr<const int>                       lifetime = std::make_shared<const int>(0);
};
