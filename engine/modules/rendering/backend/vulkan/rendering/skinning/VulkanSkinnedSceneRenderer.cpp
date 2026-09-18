#include "VulkanSkinnedSceneRenderer.h"
#include <filesystem>
#include <set>
#include <iostream>
#include <stdexcept>
#include "VulkanSkinnedPipelineConfig.h"
#include "VulkanSceneMaterialPushConstants.h"

VulkanSkinnedSceneRenderer::VulkanSkinnedSceneRenderer(VulkanBackendContext&  context,
                                                       DescriptorSetManager&  descriptors,
                                                       RenderResourceManager& resources,
                                                       VkRenderPass           renderPass)
    : context(context), descriptors(descriptors), resources(resources), renderPass(renderPass)
{
}

void VulkanSkinnedSceneRenderer::prepare(const GtsModelFrameData& data, uint32_t frame)
{
    cameras.at(frame) = data.cameraViewID;
    drawCalls = triangles = 0;
    std::vector<Draw> next;
    std::set<Key>     updatedPalettes;
    for (const auto& draw : data.skinnedDraws)
    {
        if (!draw.geometry || !draw.geometry->skinnedMesh() || !draw.palette || !draw.occurrenceOwner ||
            draw.primitiveIndex >= draw.geometry->primitives().size())
            throw std::runtime_error("Skinned render resource: incomplete extracted draw");
        if (!data.cameraViewID)
            throw std::runtime_error("Skinned draw requires an active camera");
        auto& geometry = geometries[draw.geometry.get()];
        if (!geometry)
        {
            auto created    = std::make_shared<Geometry>();
            created->source = draw.geometry;
            created->mesh   = std::make_unique<VulkanSkinnedMeshResource>(
                context.device(), context.physicalDevice(), *draw.geometry->skinnedMesh());
            geometry = std::move(created);
        }
        const Key placementKey{draw.occurrenceOwner.get(), draw.occurrenceIndex};
        auto&     placement = placements[placementKey];
        if (placement && placement->owner.lock() != draw.occurrenceOwner)
            placement.reset();
        if (!placement)
        {
            auto created         = std::make_shared<Placement>();
            created->owner       = draw.occurrenceOwner;
            created->resources   = &resources;
            created->objectSlot  = resources.requestObjectSlot();
            created->ownsSlot    = true;
            created->objectIndex = std::make_unique<VulkanSkinningBuffer>(
                context.device(), context.physicalDevice(), sizeof(uint32_t), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
            const uint32_t slot = created->objectSlot;
            created->objectIndex->write(&slot, sizeof(slot));
            placement = std::move(created);
        }
        const Key paletteKey{draw.occurrenceOwner.get(), draw.paletteSlot};
        auto&     palette = palettes[paletteKey];
        if (palette && palette->owner.lock() != draw.occurrenceOwner)
            palette.reset();
        if (!palette)
        {
            auto created    = std::make_shared<Palette>();
            created->owner  = draw.occurrenceOwner;
            created->buffer = std::make_unique<VulkanSkinPaletteBuffer>(
                context.device(), context.physicalDevice(), GraphicsConstants::MAX_FRAMES_IN_FLIGHT);
            palette = std::move(created);
        }
        if (updatedPalettes.insert(paletteKey).second)
            palette->buffer->update(frame, *draw.palette);
        resources.writeObjectDataForFrameAndMarkStale(
            frame, placement->objectSlot, draw.worldFromGeometry, glm::vec4(1, 1, 0, 0));
        auto material = draw.material;
        auto fallback = [&](texture_id_type& texture, MaterialTextureRole role)
        {
            if (!texture)
                texture = resources.requestMaterialFallbackTexture(role);
        };
        fallback(material.textures.baseColor, MaterialTextureRole::BaseColor);
        fallback(material.textures.metallicRoughness, MaterialTextureRole::MetallicRoughness);
        fallback(material.textures.normal, MaterialTextureRole::Normal);
        fallback(material.textures.ambientOcclusion, MaterialTextureRole::AmbientOcclusion);
        fallback(material.textures.emissive, MaterialTextureRole::Emissive);
        if (!resources.getMaterialTextureDescriptorSets(material.textures))
            throw std::runtime_error("Skinned render resource: material descriptors unavailable");
        next.push_back({geometry, placement, palette, material, draw.primitiveIndex});
    }
    frames.at(frame).swap(next);
    std::erase_if(placements,
                  [](const auto& entry)
                  {
                      return !entry.second || entry.second->owner.expired();
                  });
    std::erase_if(palettes,
                  [](const auto& entry)
                  {
                      return !entry.second || entry.second->owner.expired();
                  });
}

VulkanPipeline& VulkanSkinnedSceneRenderer::pipeline(const MaterialFrameState& material, VkDescriptorSetLayout palette)
{
    // This first character uses opaque scalar materials. Other queues need joint
    // static/skinned sorting; reject them explicitly until that policy is added.
    if (material.renderState.alphaMode != MaterialAlphaMode::Opaque || !material.renderState.depthWrite)
        throw std::runtime_error("Skinned scene submission currently requires opaque depth-writing materials");
    auto  key    = std::make_pair(material.shaderFamily, material.renderState.doubleSided);
    auto& result = pipelines[key];
    if (!result)
    {
        VulkanPipelineConfig config;
        config.vkRenderPass       = renderPass;
        config.fragmentShaderPath = material.shaderFamily == MaterialShaderFamily::StandardSurface
                                        ? GraphicsConstants::PBR_F_SHADER_PATH
                                        : GraphicsConstants::F_SHADER_PATH;
        config.pushConstantSize   = sizeof(VulkanSceneMaterialPushConstants);
        config.cullMode           = material.renderState.doubleSided ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;
        config                    = makeVulkanSkinnedPipelineConfig(
            config,
            descriptors.getDescriptorSetLayouts(),
            palette,
            (std::filesystem::path(GraphicsConstants::V_SHADER_PATH).parent_path() / "skinned_vert.spv").string());
        result = std::make_unique<VulkanPipeline>(context, descriptors, config);
    }
    return *result;
}

void VulkanSkinnedSceneRenderer::record(VkCommandBuffer command, uint32_t frame)
{
    if (frames.at(frame).empty())
        return;
    auto* cameraResource = resources.getCameraView(cameras.at(frame));
    if (!cameraResource)
        throw std::runtime_error("Skinned camera resource is unavailable");
    auto* environment = resources.getEnvironmentTextureDescriptorSets(cameraResource->environment);
    if (!environment)
        throw std::runtime_error("Skinned environment descriptors unavailable");
    for (const auto& draw : frames.at(frame))
    {
        const auto& mesh     = *draw.geometry->mesh;
        const auto& palette  = *draw.palette->buffer;
        const auto& material = draw.material;
        const auto& range    = mesh.primitives().at(draw.primitive);
        auto&       selected = pipeline(material, palette.layout());
        vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, selected.getPipeline());
        auto* textureSets = resources.getMaterialTextureDescriptorSets(material.textures);
        if (!textureSets)
            throw std::runtime_error("Skinned material descriptors unavailable");
        const VkDescriptorSet sets[] = {cameraResource->descriptorSets[frame],
                                        resources.getObjectSSBODescriptorSet(frame),
                                        textureSets->at(frame),
                                        environment->at(frame)};
        vkCmdBindDescriptorSets(
            command, VK_PIPELINE_BIND_POINT_GRAPHICS, selected.getPipelineLayout(), 0, 4, sets, 0, nullptr);
        VulkanSceneMaterialPushConstants push;
        push.materialFlags  = {material.vertexColorOnly ? 1 : 0, static_cast<int32_t>(material.featureFlags), 0, 0};
        push.baseColor      = material.parameters.baseColor;
        push.surfaceFactors = material.parameters.surfaceParameters;
        push.emissiveFactorStrength = material.parameters.emissiveFactorStrength;
        vkCmdPushConstants(command, selected.getPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
        mesh.drawPrimitive(command,
                           selected.getPipelineLayout(),
                           palette,
                           frame,
                           draw.primitive,
                           draw.placement->objectIndex->handle());
        ++drawCalls;
        triangles += range.indexCount / 3;
    }
}
