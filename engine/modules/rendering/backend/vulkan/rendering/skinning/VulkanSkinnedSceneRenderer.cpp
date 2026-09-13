#include "VulkanSkinnedSceneRenderer.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include "VulkanSkinnedPipelineConfig.h"
#include "VulkanSceneMaterialPushConstants.h"

VulkanSkinnedSceneRenderer::VulkanSkinnedSceneRenderer(VulkanBackendContext& context,
    DescriptorSetManager& descriptors, RenderResourceManager& resources, VkRenderPass renderPass)
    : context(context), descriptors(descriptors), resources(resources), renderPass(renderPass)
{
}

void VulkanSkinnedSceneRenderer::prepare(const SkinnedFrameData& data, uint32_t frame)
{
    camera = data.cameraViewID;
    drawCalls = triangles = 0;
    std::vector<std::shared_ptr<Instance>> next;
    for (const auto& draw : data.draws)
    {
        if (!draw.instance || !draw.instance->model || !draw.palettes ||
            draw.palettes->size() != draw.instance->model->bindingCount)
            throw std::runtime_error("Skinned draw has incomplete geometry or binding palettes");
        if (!camera) throw std::runtime_error("Skinned draw requires an active camera");
        auto instance = instances[draw.instance.get()];
        if (instance && instance->source.lock() != draw.instance) instance.reset();
        if (!instance)
        {
            auto geometry = geometries[draw.instance->model.get()].lock();
            if (!geometry)
            {
                geometry = std::make_shared<Geometry>();
                geometry->source = draw.instance->model;
                geometry->materials = geometry->source->materials;
                for (auto& material : geometry->materials)
                {
                    auto fallback = [&](texture_id_type& texture, MaterialTextureRole role)
                    {
                        if (!texture) texture = resources.requestMaterialFallbackTexture(role);
                    };
                    fallback(material.textures.baseColor, MaterialTextureRole::BaseColor);
                    fallback(material.textures.metallicRoughness, MaterialTextureRole::MetallicRoughness);
                    fallback(material.textures.normal, MaterialTextureRole::Normal);
                    fallback(material.textures.ambientOcclusion, MaterialTextureRole::AmbientOcclusion);
                    fallback(material.textures.emissive, MaterialTextureRole::Emissive);
                    if (!resources.getMaterialTextureDescriptorSets(material.textures))
                        throw std::runtime_error("Skinned material realization failed");
                }
                for (const auto& part : geometry->source->parts)
                {
                    if (part.bindingIndex >= geometry->source->bindingCount)
                        throw std::runtime_error("Skinned geometry references invalid binding");
                    geometry->parts.push_back(std::make_unique<VulkanSkinnedMeshResource>(
                        context.device(), context.physicalDevice(), part.mesh));
                }
                geometries[geometry->source.get()] = geometry;
                std::clog << "[Skinning] uploaded " << geometry->parts.size() << " immutable mesh parts\n";
            }
            instance = std::make_shared<Instance>();
            instance->source = draw.instance;
            instance->geometry = geometry;
            instance->resources = &resources;
            instance->objectSlot = resources.requestObjectSlot();
            instance->ownsSlot = true;
            instance->objectIndex = std::make_unique<VulkanSkinningBuffer>(context.device(),
                context.physicalDevice(), sizeof(uint32_t), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
            const uint32_t slot = instance->objectSlot;
            instance->objectIndex->write(&slot, sizeof(slot));
            for (uint32_t binding = 0; binding < geometry->source->bindingCount; ++binding)
                instance->palettes.push_back(std::make_unique<VulkanSkinPaletteBuffer>(context.device(),
                    context.physicalDevice(), GraphicsConstants::MAX_FRAMES_IN_FLIGHT));
            instances[draw.instance.get()] = instance;
        }
        for (size_t binding = 0; binding < draw.palettes->size(); ++binding)
            instance->palettes[binding]->update(frame, (*draw.palettes)[binding]);
        resources.writeObjectDataForFrameAndMarkStale(frame, instance->objectSlot,
            draw.worldFromReference, glm::vec4(1, 1, 0, 0));
        next.push_back(std::move(instance));
    }
    // Keep the old current-frame references alive until the next list is acquired.
    frames.at(frame).swap(next);
    std::erase_if(instances, [](const auto& entry) { return entry.second->source.expired(); });
    std::erase_if(geometries, [](const auto& entry) { return entry.second.expired(); });
}

VulkanPipeline& VulkanSkinnedSceneRenderer::pipeline(const MaterialFrameState& material, VkDescriptorSetLayout palette)
{
    // This first character uses opaque scalar materials. Other queues need joint
    // static/skinned sorting; reject them explicitly until that policy is added.
    if (material.renderState.alphaMode != MaterialAlphaMode::Opaque || !material.renderState.depthWrite)
        throw std::runtime_error("Skinned scene submission currently requires opaque depth-writing materials");
    auto key = std::make_pair(material.shaderFamily, material.renderState.doubleSided);
    auto& result = pipelines[key];
    if (!result)
    {
        VulkanPipelineConfig config;
        config.vkRenderPass = renderPass;
        config.fragmentShaderPath = material.shaderFamily == MaterialShaderFamily::StandardSurface
            ? GraphicsConstants::PBR_F_SHADER_PATH : GraphicsConstants::F_SHADER_PATH;
        config.pushConstantSize = sizeof(VulkanSceneMaterialPushConstants);
        config.cullMode = material.renderState.doubleSided ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;
        config = makeVulkanSkinnedPipelineConfig(config, descriptors.getDescriptorSetLayouts(), palette,
            (std::filesystem::path(GraphicsConstants::V_SHADER_PATH).parent_path() / "skinned_vert.spv").string());
        result = std::make_unique<VulkanPipeline>(context, descriptors, config);
    }
    return *result;
}

void VulkanSkinnedSceneRenderer::record(VkCommandBuffer command, uint32_t frame)
{
    if (frames.at(frame).empty()) return;
    auto* cameraResource = resources.getCameraView(camera);
    if (!cameraResource) throw std::runtime_error("Skinned camera resource is unavailable");
    auto* environment = resources.getEnvironmentTextureDescriptorSets(cameraResource->environment);
    if (!environment) throw std::runtime_error("Skinned environment descriptors unavailable");
    for (const auto& instance : frames.at(frame))
    {
        const auto& source = *instance->geometry->source;
        for (size_t part = 0; part < source.parts.size(); ++part)
        {
            const auto& mesh = *instance->geometry->parts[part];
            const auto& palette = *instance->palettes.at(source.parts[part].bindingIndex);
            for (uint32_t primitive = 0; primitive < mesh.primitives().size(); ++primitive)
            {
                const auto& range = mesh.primitives()[primitive];
                const auto& material = instance->geometry->materials.at(range.materialIndex.value_or(
                    static_cast<uint32_t>(instance->geometry->materials.size() - 1)));
                auto& selected = pipeline(material, palette.layout());
                vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, selected.getPipeline());
                auto* textureSets = resources.getMaterialTextureDescriptorSets(material.textures);
                if (!textureSets) throw std::runtime_error("Skinned material descriptors unavailable");
                const VkDescriptorSet sets[] = {cameraResource->descriptorSets[frame],
                    resources.getObjectSSBODescriptorSet(frame), textureSets->at(frame), environment->at(frame)};
                vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, selected.getPipelineLayout(),
                    0, 4, sets, 0, nullptr);
                VulkanSceneMaterialPushConstants push;
                push.materialFlags = {material.vertexColorOnly ? 1 : 0, static_cast<int32_t>(material.featureFlags), 0, 0};
                push.baseColor = material.parameters.baseColor;
                push.surfaceFactors = material.parameters.surfaceParameters;
                push.emissiveFactorStrength = material.parameters.emissiveFactorStrength;
                vkCmdPushConstants(command, selected.getPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
                mesh.drawPrimitive(command, selected.getPipelineLayout(), palette, frame, primitive, instance->objectIndex->handle());
                ++drawCalls;
                triangles += range.indexCount / 3;
            }
        }
    }
}
