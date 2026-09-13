#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "assets/model/GtsModelDiagnostic.h"

struct GtsModelAsset;
struct GtsSkeletonAsset;
struct GtsAnimationClipAsset;
struct GtsModelImportBundle;
class GtsModelResource;
class GtsModelRegistry;

// non-owning, scoped to a resource retained by the registry or a model handle
class GtsModelClipReference
{
    public:
    uint32_t index() const
    {
        return clipIndex;
    }
    const GtsModelResource* model() const
    {
        return owner;
    }
    bool operator==(const GtsModelClipReference&) const = default;

    private:
    friend class GtsModelResource;
    GtsModelClipReference(const GtsModelResource* owner, uint32_t index) : owner(owner), clipIndex(index) {}
    const GtsModelResource* owner;
    uint32_t                clipIndex;
};

struct GtsModelClipLookupResult
{
    std::optional<GtsModelClipReference> reference;
    std::vector<GtsModelDiagnostic>      diagnostics;
    bool                                 succeeded() const
    {
        return reference.has_value();
    }
};

class GtsModelResource
{
    public:
    ~GtsModelResource();
    GtsModelResource(const GtsModelResource&)            = delete;
    GtsModelResource& operator=(const GtsModelResource&) = delete;

    const std::filesystem::path& sourcePath() const
    {
        return source;
    }
    const GtsModelAsset&                                     model() const;
    std::span<const std::shared_ptr<const GtsSkeletonAsset>> skeletons() const;
    std::span<const GtsAnimationClipAsset>                   clips() const;
    GtsModelClipLookupResult findClip(std::string_view name, std::optional<uint32_t> skeletonUseIndex = {}) const;
    // foreign-resource references are rejected, even if their numeric index exists here
    const GtsAnimationClipAsset* clip(GtsModelClipReference reference) const;

    private:
    friend class GtsModelRegistry;
    GtsModelResource(std::filesystem::path source, GtsModelImportBundle bundle);
    std::filesystem::path                       source;
    std::unique_ptr<const GtsModelImportBundle> definitions;
};
