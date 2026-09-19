#include "model/loading/GtsModelResource.h"
#include "GtsModelInstance.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include "model/domain/model/GtsModelSkin.h"
#include "model/domain/animation/GtsAnimationClipValidation.h"
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

namespace
{
    bool sameLogicalMaterial(const GtsRealizedMaterial& a, const GtsRealizedMaterial& b)
    {
        if (a.index() != b.index())
            return false;
        if (const auto* slot = std::get_if<uint32_t>(&a))
            return *slot == std::get<uint32_t>(b);
        if (const auto* external = std::get_if<GtsExternalMaterialReference>(&a))
        {
            const auto& other = std::get<GtsExternalMaterialReference>(b);
            return external->reference.id == other.reference.id &&
                   external->reference.logicalPath == other.reference.logicalPath &&
                   external->referenceDirectory == other.referenceDirectory;
        }
        return false; // Unassigned/default is not a logical slot.
    }
} // namespace

bool GtsModelInstance::validOverride(const MaterialOverride& replacement) const
{
    const auto scope = replacement.runtimeLifetime.lock();
    return materialSet && scope && scope == materialSet->scopeToken().lock() &&
           materialSet->isMaterialAlive(replacement.material);
}

bool GtsModelInstance::worldMaterialsValid() const
{
    return materialSet && materialSet->valid() && (!materialOverride || validOverride(*materialOverride)) &&
           std::all_of(slotOverrides.begin(),
                       slotOverrides.end(),
                       [this](const SlotOverride& entry)
                       {
                           return validOverride(entry.replacement);
                       });
}

std::optional<GtsModelMaterialSlot> GtsModelInstance::materialSlot(const GtsRealizedMaterial& material) const
{
    if (std::holds_alternative<std::monostate>(material))
        return {};
    // Bare integers or copied associations cannot establish model ownership.
    for (const auto& geometry : realized->geometry)
        for (const auto& primitive : geometry.primitives())
            if (&primitive.material == &material)
            {
                GtsModelMaterialSlot slot;
                slot.owner       = realized;
                slot.association = &material;
                return slot;
            }
    return {};
}

bool GtsModelInstance::ownsMaterialSlot(const GtsModelMaterialSlot& slot) const
{
    return slot.association && slot.owner.lock() == realized;
}

MaterialInstanceHandle GtsModelInstance::materialFor(uint32_t occurrenceIndex, uint32_t primitiveIndex) const
{
    if (!worldMaterialsValid() || occurrenceIndex >= realized->occurrences.size())
        return {};
    const auto geometryIndex = realized->occurrences[occurrenceIndex].geometryIndex;
    if (geometryIndex >= realized->geometry.size())
        return {};
    const auto primitives = realized->geometry[geometryIndex].primitives();
    if (primitiveIndex >= primitives.size())
        return {};
    const auto& association = primitives[primitiveIndex].material;
    for (const auto& entry : slotOverrides)
        if (sameLogicalMaterial(*entry.slot.association, association))
            return entry.replacement.material;
    if (materialOverride)
        return materialOverride->material;
    return materialSet->materialFor(*realized, geometryIndex, primitiveIndex);
}

GtsModelInstanceStatus GtsModelInstance::setMaterialOverride(MaterialInstanceHandle   material,
                                                             std::weak_ptr<const int> runtimeScope)
{
    MaterialOverride replacement{material, std::move(runtimeScope)};
    if (!materialSet || !materialSet->valid() || !validOverride(replacement))
        return failure("model.instance.material_override",
                       "Override requires a live material in this instance's world");
    materialOverride = std::move(replacement);
    return {};
}

GtsModelInstanceStatus GtsModelInstance::setMaterialOverride(GtsModelMaterialSlot     slot,
                                                             MaterialInstanceHandle   material,
                                                             std::weak_ptr<const int> runtimeScope)
{
    if (!ownsMaterialSlot(slot))
        return failure("model.instance.material_slot", "Override requires a logical material slot from this model");
    MaterialOverride replacement{material, std::move(runtimeScope)};
    if (!materialSet || !materialSet->valid() || !validOverride(replacement))
        return failure("model.instance.material_override",
                       "Override requires a live material in this instance's world");
    for (auto& entry : slotOverrides)
        if (sameLogicalMaterial(*entry.slot.association, *slot.association))
        {
            entry.replacement = std::move(replacement);
            return {};
        }
    slotOverrides.push_back({std::move(slot), std::move(replacement)});
    return {};
}

void GtsModelInstance::clearMaterialOverride(GtsModelMaterialSlot slot)
{
    if (!ownsMaterialSlot(slot))
        return;
    std::erase_if(slotOverrides,
                  [&](const SlotOverride& entry)
                  {
                      return sameLogicalMaterial(*entry.slot.association, *slot.association);
                  });
}

void GtsModelInstance::clearMaterialOverrides()
{
    materialOverride.reset();
    slotOverrides.clear();
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
        clearMaterialOverrides();
    materialSet = std::move(materials);
    return {};
}
