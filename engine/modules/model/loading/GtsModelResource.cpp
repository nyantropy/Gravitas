#include "model/loading/GtsModelResource.h"
#include "model/loading/GtsPreparedModelDefinition.h"

#include <string>
#include <utility>

#include "model/import/GtsModelImportBundle.h"
#include "model/domain/animation/GtsAnimationClipValidation.h"

GtsModelResource::GtsModelResource(std::filesystem::path identity, GtsModelImportBundle bundle)
    : identity(std::move(identity)), definitions(std::make_unique<const GtsModelImportBundle>(std::move(bundle)))
{
}

GtsModelResource::~GtsModelResource() = default;

GtsModelResource::GtsModelResource(std::filesystem::path identity, GtsPreparedModelDefinition model)
    : identity(std::move(identity)), prepared(std::make_unique<const GtsPreparedModelDefinition>(std::move(model)))
{
}

const GtsModelAsset* GtsModelResource::canonicalModel() const
{
    return definitions ? &*definitions->model : nullptr;
}

const GtsPreparedModelDefinition* GtsModelResource::preparedModel() const
{
    return prepared.get();
}

std::span<const GtsModelNode> GtsModelResource::nodes() const
{
    return definitions ? std::span<const GtsModelNode>(definitions->model->nodes) : prepared->nodes;
}

std::span<const uint32_t> GtsModelResource::rootNodes() const
{
    return definitions ? std::span<const uint32_t>(definitions->model->rootNodes) : prepared->rootNodes;
}

std::size_t GtsModelResource::meshCount() const
{
    return definitions ? definitions->model->meshes.size() : prepared->meshes.size();
}

std::span<const std::shared_ptr<const GtsSkeletonAsset>> GtsModelResource::skeletons() const
{
    return definitions ? std::span<const std::shared_ptr<const GtsSkeletonAsset>>(definitions->skeletons)
                       : std::span<const std::shared_ptr<const GtsSkeletonAsset>>{};
}

std::span<const GtsAnimationClipAsset> GtsModelResource::clips() const
{
    return definitions ? std::span<const GtsAnimationClipAsset>(definitions->animationClips)
                       : std::span<const GtsAnimationClipAsset>{};
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
                                          identity.string() + ": clips[" + std::string(name) + "]"}}};
    };
    if (skeletonUseIndex && (!definitions || *skeletonUseIndex >= definitions->model->skeletonUses.size()))
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
            validateGtsAnimationClip(clips()[*found], *definitions->model->skeletonUses[*skeletonUseIndex].skeleton);
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

GtsModelCapabilities GtsModelResource::capabilities() const
{
    bool hierarchy = false;
    for (const auto& node : nodes())
    {
        hierarchy = hierarchy || !node.children.empty();
    }
    return {meshCount() != 0,
            hierarchy,
            !skeletons().empty(),
            definitions && !definitions->model->skinBindings.empty(),
            !clips().empty()};
}

std::span<const GtsModelSkeletonUse> GtsModelResource::skeletonUses() const
{
    return definitions ? std::span<const GtsModelSkeletonUse>(definitions->model->skeletonUses)
                       : std::span<const GtsModelSkeletonUse>{};
}

std::span<const GtsModelSkinBinding> GtsModelResource::skinBindings() const
{
    return definitions ? std::span<const GtsModelSkinBinding>(definitions->model->skinBindings)
                       : std::span<const GtsModelSkinBinding>{};
}
