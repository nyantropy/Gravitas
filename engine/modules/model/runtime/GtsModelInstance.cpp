#include "GtsModelInstance.h"

#include <cmath>
#include <exception>
#include "assets/model/GtsModelSkin.h"
#include "assets/animation/GtsAnimationClipValidation.h"
#include "animation/skeletal/GtsSkeletonPoseEvaluation.h"
#include "animation/skinning/GtsSkinPaletteEvaluation.h"

GtsModelInstance::GtsModelInstance(GtsModelHandle                                   model,
                                   std::shared_ptr<const GtsRealizedModel>          geometry,
                                   std::shared_ptr<const GtsRealizedModelMaterials> materials)
    : resource(std::move(model)), realized(std::move(geometry)), materialSet(std::move(materials))
{
}

GtsModelInstanceStatus
GtsModelInstance::failure(const std::string& code, const std::string& message, std::optional<uint32_t> use) const
{
    auto context = resource ? resource->identityPath().string() : "model";
    if (use)
        context += ": skeletonUse[" + std::to_string(*use) + "]";
    return {{{GtsModelDiagnosticSeverity::Error, code, message, context}}};
}

GtsModelInstanceStatus GtsModelInstance::initialize()
{
    if (!resource || !realized || realized->model != resource || !materialSet || !materialSet->belongsTo(*realized))
        return failure("model.instance.identity", "Model, geometry and live material scope must match");
    const auto uses = resource->skeletonUses();
    occurrences.resize(uses.size());
    for (uint32_t use = 0; use < uses.size(); ++use)
    {
        auto& occurrence      = occurrences[use];
        occurrence.useIndex   = use;
        occurrence.definition = uses[use].skeleton.get();
        auto pose             = evaluateGtsDefaultPose(occurrence.skeleton());
        if (!pose.succeeded())
        {
            auto result = failure("model.instance.default_pose", "Default pose initialization failed", use);
            for (const auto& error : pose.diagnostics())
                result.diagnostics.push_back(
                    {GtsModelDiagnosticSeverity::Error, error.code, error.message, error.location});
            return result;
        }
        occurrence.state.pose = *pose.pose();
    }
    for (uint32_t binding = 0; binding < resource->skinBindings().size(); ++binding)
    {
        const auto use = resource->skinBindings()[binding].skeletonUseIndex;
        if (use >= occurrences.size())
            return failure("model.instance.binding", "Invalid skeleton use for binding " + std::to_string(binding));
        occurrences[use].bindingPalettes.push_back({binding, {}});
    }
    for (const auto& node : realized->occurrences)
    {
        if (node.geometryIndex >= realized->geometry.size())
            return failure("model.instance.geometry", "Invalid geometry association");
        if (realized->geometry[node.geometryIndex].profile() == GtsGeometryProfile::Skinned &&
            (!node.skinBindingIndex || !node.skeletonUseIndex ||
             *node.skinBindingIndex >= resource->skinBindings().size() ||
             resource->skinBindings()[*node.skinBindingIndex].skeletonUseIndex != *node.skeletonUseIndex))
            return failure("model.instance.binding", "Skinned occurrence has incompatible binding/use associations");
    }
    for (auto& occurrence : occurrences)
        if (auto result = evaluatePalettes(occurrence); !result.succeeded())
            return result;
    return {};
}

GtsModelInstanceStatus GtsModelInstance::evaluatePalettes(GtsSkeletonOccurrence& occurrence) const
{
    for (auto& entry : occurrence.bindingPalettes)
    {
        const auto& binding = resource->skinBindings()[entry.skinBindingIndex].binding;
        auto        palette = evaluateGtsSkinPalette(occurrence.pose(), binding, occurrence.skeleton());
        if (!palette.succeeded())
        {
            auto result = failure("model.instance.palette",
                                  "Palette evaluation failed for binding " + std::to_string(entry.skinBindingIndex),
                                  occurrence.useIndex);
            for (const auto& error : palette.diagnostics())
                result.diagnostics.push_back(
                    {GtsModelDiagnosticSeverity::Error, error.code, error.message, error.location});
            return result;
        }
        entry.palette = *palette.palette();
    }
    return {};
}

GtsSkeletonOccurrenceReference GtsModelInstance::skeletonOccurrence(uint32_t use) const
{
    GtsSkeletonOccurrenceReference result;
    if (use < occurrences.size())
    {
        result.lifetime   = lifetime;
        result.occurrence = &occurrences[use];
    }
    return result;
}

const GtsSkinPalette* GtsModelInstance::palette(uint32_t binding) const
{
    if (binding >= resource->skinBindings().size())
        return nullptr;
    const auto use = resource->skinBindings()[binding].skeletonUseIndex;
    for (const auto& entry : occurrences[use].bindingPalettes)
        if (entry.skinBindingIndex == binding)
            return &entry.palette;
    return nullptr;
}

const GtsSkinPalette* GtsModelInstance::paletteForOccurrence(uint32_t occurrenceIndex) const
{
    if (occurrenceIndex >= realized->occurrences.size())
        return nullptr;
    const auto& occurrence = realized->occurrences[occurrenceIndex];
    return occurrence.skinBindingIndex ? palette(*occurrence.skinBindingIndex) : nullptr;
}

MaterialInstanceHandle GtsModelInstance::materialFor(uint32_t occurrenceIndex, uint32_t primitiveIndex) const
{
    if (!worldMaterialsValid() || occurrenceIndex >= realized->occurrences.size())
        return {};
    const auto base =
        materialSet->materialFor(*realized, realized->occurrences[occurrenceIndex].geometryIndex, primitiveIndex);
    return base.valid() && materialOverride ? *materialOverride : base;
}

GtsModelInstanceStatus
GtsModelInstance::setMaterialOverride(MaterialInstanceHandle material, std::weak_ptr<const int> runtimeScope)
{
    if (!materialSet || !materialSet->valid() || runtimeScope.expired() ||
        runtimeScope.lock() != materialSet->scopeToken().lock() || !materialSet->isMaterialAlive(material))
        return failure("model.instance.material_override", "Override requires a live material in this instance's world");
    materialOverride = material;
    return {};
}

GtsModelInstanceStatus GtsModelInstance::play(uint32_t use, GtsModelClipReference reference)
{
    if (use >= occurrences.size())
        return failure("model.instance.skeleton_use", "Invalid skeleton use", use);
    const auto* clip = resource->clip(reference);
    if (!clip)
        return failure("model.instance.clip", "Clip reference belongs to another model", use);
    if (occurrences[use].clip == reference)
        return {}; // Idempotent gameplay selection.
    const auto validation = validateGtsAnimationClip(*clip, occurrences[use].skeleton());
    if (!validation.isValid())
    {
        auto result = failure("model.instance.clip", "Clip is incompatible with skeleton occurrence", use);
        for (const auto& error : validation.diagnostics)
            result.diagnostics.push_back(
                {GtsModelDiagnosticSeverity::Error, error.code, error.message, error.location});
        return result;
    }
    auto next = occurrences[use];
    next.clip = reference;
    next.state.selectClip(reference.index());
    try
    {
        advanceGtsAnimationPlayback(next.state, next.skeleton(), *clip, 0);
    }
    catch (const std::exception& error)
    {
        return failure("model.instance.playback", error.what(), use);
    }
    if (auto result = evaluatePalettes(next); !result.succeeded())
        return result;
    occurrences[use] = std::move(next);
    return {};
}

GtsModelInstanceStatus GtsModelInstance::stop(uint32_t use)
{
    if (use >= occurrences.size())
        return failure("model.instance.skeleton_use", "Invalid skeleton use", use);
    auto next = occurrences[use];
    auto pose = evaluateGtsDefaultPose(next.skeleton());
    if (!pose.succeeded())
        return failure("model.instance.default_pose", pose.diagnostics().front().message, use);
    next.clip.reset();
    next.state.activeClip.reset();
    next.state.timeSeconds = 0;
    next.state.pose        = *pose.pose();
    if (auto result = evaluatePalettes(next); !result.succeeded())
        return result;
    occurrences[use] = std::move(next);
    return {};
}

GtsModelInstanceStatus GtsModelInstance::setPlaybackPolicy(uint32_t use, float speed, bool looping)
{
    if (use >= occurrences.size() || !std::isfinite(speed) || speed < 0)
        return failure("model.instance.playback_policy", "Expected valid occurrence and finite nonnegative speed", use);
    occurrences[use].state.speed   = speed;
    occurrences[use].state.looping = looping;
    return {};
}

GtsModelInstanceStatus GtsModelInstance::updateAnimations(double deltaSeconds)
{
    if (!std::isfinite(deltaSeconds) || deltaSeconds < 0)
        return failure("model.instance.delta", "Animation delta must be finite and nonnegative");
    // Stage all occurrences; no new time/pose/palette becomes visible if any evaluation fails.
    auto next = occurrences;
    for (auto& occurrence : next)
    {
        if (!occurrence.clip)
            continue; // Default pose and palettes are already initialized.
        try
        {
            advanceGtsAnimationPlayback(
                occurrence.state, occurrence.skeleton(), *resource->clip(*occurrence.clip), deltaSeconds);
        }
        catch (const std::exception& error)
        {
            return failure("model.instance.playback", error.what(), occurrence.useIndex);
        }
        if (auto result = evaluatePalettes(occurrence); !result.succeeded())
            return result;
    }
    // Keep the occurrence array and its reference addresses stable.
    for (size_t index = 0; index < occurrences.size(); ++index)
        occurrences[index] = std::move(next[index]);
    return {};
}

GtsModelInstanceStatus GtsModelInstance::rebindMaterials(std::shared_ptr<const GtsRealizedModelMaterials> materials)
{
    if (!materials || !materials->belongsTo(*realized))
        return failure("model.instance.material_scope", "Foreign material set");
    if (materialSet->scopeToken().lock() != materials->scopeToken().lock())
        clearMaterialOverride();
    materialSet = std::move(materials);
    return {};
}
