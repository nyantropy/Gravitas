#pragma once
#include "model/public/GtsModelClipReference.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "model/domain/model/GtsModelDiagnostic.h"
#include "model/public/GtsModelRequest.h"

struct GtsModelAsset;
struct GtsModelNode;
struct GtsModelSkeletonUse;
struct GtsModelSkinBinding;
struct GtsPreparedModelDefinition;
struct GtsSkeletonAsset;
struct GtsAnimationClipAsset;
struct GtsModelImportBundle;
class GtsModelResource;
class GtsModelRegistry;

class GtsModelResource
{
    public:
    ~GtsModelResource();
    GtsModelResource(const GtsModelResource&)            = delete;
    GtsModelResource& operator=(const GtsModelResource&) = delete;

    const std::filesystem::path& identityPath() const
    {
        return identity;
    }
    GtsModelCapabilities          capabilities() const;
    std::span<const GtsModelNode> nodes() const;
    std::span<const uint32_t>     rootNodes() const;
    std::size_t                   meshCount() const;
    std::span<const GtsModelSkeletonUse> skeletonUses() const;
    std::span<const GtsModelSkinBinding> skinBindings() const;
    // Representation views for downstream CPU preparation/realization.
    const GtsModelAsset*                                     canonicalModel() const;
    const GtsPreparedModelDefinition*                        preparedModel() const;
    std::span<const std::shared_ptr<const GtsSkeletonAsset>> skeletons() const;
    std::span<const GtsAnimationClipAsset>                   clips() const;
    GtsModelClipLookupResult findClip(std::string_view name, std::optional<uint32_t> skeletonUseIndex = {}) const;
    // foreign-resource references are rejected, even if their numeric index exists here
    const GtsAnimationClipAsset* clip(GtsModelClipReference reference) const;

    private:
    friend class GtsModelRegistry;
    GtsModelResource(std::filesystem::path identity, GtsModelImportBundle bundle);
    GtsModelResource(std::filesystem::path identity, GtsPreparedModelDefinition model);
    std::filesystem::path                             identity;
    std::unique_ptr<const GtsModelImportBundle>       definitions;
    std::unique_ptr<const GtsPreparedModelDefinition> prepared;
};
