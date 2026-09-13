#include "GtsModelResource.h"

#include <string>
#include <utility>

#include "assets/importer/GtsModelImportBundle.h"
#include "assets/animation/GtsAnimationClipValidation.h"

GtsModelResource::GtsModelResource(std::filesystem::path source, GtsModelImportBundle bundle)
    : source(std::move(source)), definitions(std::make_unique<const GtsModelImportBundle>(std::move(bundle)))
{
}

GtsModelResource::~GtsModelResource() = default;

const GtsModelAsset& GtsModelResource::model() const
{
    return *definitions->model;
}

std::span<const std::shared_ptr<const GtsSkeletonAsset>> GtsModelResource::skeletons() const
{
    return definitions->skeletons;
}

std::span<const GtsAnimationClipAsset> GtsModelResource::clips() const
{
    return definitions->animationClips;
}

GtsModelClipLookupResult GtsModelResource::findClip(std::string_view        name,
                                                    std::optional<uint32_t> skeletonUseIndex) const
{
    auto failure = [&](const char* code, const std::string& message)
    {
        return GtsModelClipLookupResult{{},
                                        {{GtsModelDiagnosticSeverity::Error,
                                          code,
                                          message,
                                          source.string() + ": clips[" + std::string(name) + "]"}}};
    };
    if (skeletonUseIndex && *skeletonUseIndex >= model().skeletonUses.size())
    {
        return failure("model.clip.skeleton_use", "Invalid skeleton-use index " + std::to_string(*skeletonUseIndex));
    }
    std::optional<uint32_t> found;
    for (uint32_t index = 0; index < clips().size(); ++index)
    {
        if (clips()[index].name != name)
        {
            continue;
        }
        if (found)
        {
            return failure("model.clip.ambiguous", "Multiple clips have this name; name lookup is ambiguous");
        }
        found = index;
    }
    if (!found)
    {
        return failure("model.clip.missing", "No clip with the requested name");
    }
    if (skeletonUseIndex)
    {
        const auto validation =
            validateGtsAnimationClip(clips()[*found], *model().skeletonUses[*skeletonUseIndex].skeleton);
        if (!validation.isValid())
        {
            auto result = failure("model.clip.incompatible",
                                  "Clip is incompatible with skeleton use " + std::to_string(*skeletonUseIndex));
            for (const auto& diagnostic : validation.diagnostics)
            {
                result.diagnostics.push_back(
                    {GtsModelDiagnosticSeverity::Error, diagnostic.code, diagnostic.message, diagnostic.location});
            }
            return result;
        }
    }
    return {GtsModelClipReference(this, *found), {}};
}

const GtsAnimationClipAsset* GtsModelResource::clip(GtsModelClipReference reference) const
{
    return reference.model() == this && reference.index() < clips().size() ? &clips()[reference.index()] : nullptr;
}
