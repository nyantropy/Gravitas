#include "GtsModelRenderExtraction.h"
#include "model/runtime/GtsModelHierarchy.h"
#include <map>
#include <cmath>
#include <tuple>
#include <stdexcept>

GtsModelExtractionResult extractModelRenderState(std::shared_ptr<const GtsModelInstance> instance,
                                                 const glm::mat4&                        worldTransform,
                                                 gts::rendering::MaterialRuntime&        materials,
                                                 IResourceProvider*                      resources)
{
    GtsModelExtractionResult result;
    std::string              context = "model instance";
    try
    {
        if (!instance || !instance->model() || !instance->geometry())
            throw std::runtime_error("Invalid model instance");
        context = instance->model()->identityPath().string();
        if (!instance->worldMaterialsValid())
            throw std::runtime_error("Expired world materials; rebind the instance before extraction");
        if (instance->materials()->scopeToken().lock() != materials.lifetimeToken().lock())
            throw std::runtime_error("Material runtime belongs to another world");
        const auto& model      = instance->geometry();
        const auto  transforms = gtsModelNodeTransforms(*instance->model());
        std::map<uint32_t, std::shared_ptr<const GtsSkinPalette>> palettes;
        std::map<MaterialInstanceHandle, MaterialFrameState, decltype([](auto a, auto b)
            { return std::tie(a.id, a.generation) < std::tie(b.id, b.generation); })> materialStates;
        for (uint32_t index = 0; index < model->occurrences.size(); ++index)
        {
            const auto& occurrence = model->occurrences[index];
            context = instance->model()->identityPath().string() + " occurrence " + std::to_string(index) + " node " +
                      std::to_string(occurrence.modelNodeIndex) + " geometry " +
                      std::to_string(occurrence.geometryIndex);
            if (occurrence.geometryIndex >= model->geometry.size() || occurrence.modelNodeIndex >= transforms.size())
                throw std::runtime_error("Invalid occurrence geometry/node reference");
            auto geometry =
                std::shared_ptr<const GtsRealizedGeometry>(model, &model->geometry[occurrence.geometryIndex]);
            const bool                            skinned = geometry->profile() == GtsGeometryProfile::Skinned;
            std::shared_ptr<const GtsSkinPalette> palette;
            if (skinned)
            {
                if (!occurrence.skinBindingIndex || !occurrence.skeletonUseIndex || !geometry->skinnedMesh())
                    throw std::runtime_error("Skinned profile is missing its binding/skeleton association");
                const auto* current = instance->paletteForOccurrence(index);
                if (!current)
                    throw std::runtime_error("Missing CPU palette for binding " +
                                             std::to_string(*occurrence.skinBindingIndex));
                auto& snapshot = palettes[*occurrence.skinBindingIndex];
                if (!snapshot)
                    snapshot = std::make_shared<const GtsSkinPalette>(*current);
                palette = snapshot;
            }
            else if (occurrence.skinBindingIndex || occurrence.skeletonUseIndex)
                throw std::runtime_error("Static profile has an unexpected skin association");
            for (uint32_t primitive = 0; primitive < geometry->primitives().size(); ++primitive)
            {
                const auto handle = instance->materialFor(index, primitive);
                if (!handle.valid())
                    throw std::runtime_error("Invalid material for primitive " + std::to_string(primitive));
                auto found = materialStates.find(handle);
                if (found == materialStates.end())
                {
                    // Use the ordinary runtime synchronization, including role-specific texture fallbacks.
                    materials.synchronizeGpuState(handle, resources);
                    if (resources && !materials.getGpuState(handle))
                        throw std::runtime_error("Material synchronization produced no runtime state");
                    auto state = resources ? makeMaterialFrameState(*materials.getGpuState(handle))
                                           : materials.frameState(handle);
                    found      = materialStates.emplace(handle, state).first;
                }
                GtsModelDraw draw{instance,
                                  index,
                                  geometry,
                                  primitive,
                                  found->second,
                                  skinned ? worldTransform : worldTransform * transforms[occurrence.modelNodeIndex]};
                for (int column = 0; column < 4; ++column)
                    for (int row = 0; row < 4; ++row)
                        if (!std::isfinite(draw.worldFromGeometry[column][row]))
                            throw std::runtime_error("Non-finite object placement");
                if (skinned)
                    result.frame.skinnedDraws.push_back({std::move(draw), *occurrence.skinBindingIndex, palette});
                else
                    result.frame.staticDraws.push_back(std::move(draw));
            }
        }
    }
    catch (const std::exception& error)
    {
        result.frame = {};
        result.diagnostics.push_back(
            {GtsModelDiagnosticSeverity::Error, "model.render.extraction", error.what(), context});
    }
    return result;
}
