#include "model/realization/GtsModelRealizationCache.h"
#include "../assets/importers/gltf/GltfFixtureBuilder.h"
#include "../assets/runtime/ScopedRuntimeAssetPolicy.h"
#include "model/world/GtsModelInstanceRuntime.h"
#include "model/extraction/GtsModelRenderExtraction.h"
#include "model/loading/GtsModelRegistry.h"
#include "rendering/backend/vulkan/rendering/skinning/VulkanSkinnedSceneRenderer.h"
#include "rendering/backend/vulkan/rendering/framegraph/VulkanModelStaticDraws.h"
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>

#include "VulkanContext.hpp"
#include "VulkanBackendContext.h"
#include "DescriptorSetManager.hpp"
#include "VulkanRenderPass.hpp"
#include "VulkanRenderPassConfig.h"
#include "VulkanPipeline.hpp"
#include "VulkanSkinnedPipelineConfig.h"
#include "VulkanSkinPaletteBuffer.h"
#include "VulkanSkinnedMeshResource.h"
#include "animation/skinning/GtsSkinPalette.h"

namespace
{
    void require(bool value, const char* message)
    {
        if (!value)
            throw std::runtime_error(message);
    }

    template <class F> void rejects(F action)
    {
        try
        {
            action();
        }
        catch (const std::invalid_argument&)
        {
            return;
        }
        throw std::runtime_error("Invalid skinning input accepted");
    }
} // namespace

int main(int argc, char** argv)
try
{
    if (argc != 2)
        throw std::runtime_error("Expected shader directory");
    VulkanContextConfig config{};
    config.headless             = true;
    config.enableSurfaceSupport = false;
    config.applicationName      = "Skinned GPU contract smoke";
    VulkanContext           context(config);
    VulkanBackendContext    backend(context);
    DescriptorSetManager    sceneDescriptors(backend, 2);
    VulkanSkinPaletteBuffer palette(backend.device(), backend.physicalDevice(), 2);
    GtsSkinPalette          source{{glm::mat4(1), glm::translate(glm::mat4(1), glm::vec3(2, 3, 4))}};
    palette.update(0, source);
    const auto descriptor = palette.descriptorSet(0);
    require(palette.matrixCount(0) == 2 && palette.matrixCount(1) == 0, "Independent frame slots");
    palette.update(1, source);
    source.matrices[1][3].x = 5;
    palette.update(0, source);
    require(palette.descriptorSet(0) == descriptor, "Descriptor reused across updates");
    source.matrices.resize(256, glm::mat4(1));
    palette.update(0, source);
    require(palette.matrixCount(0) == 256 && palette.matrixCount(1) == 2, "Variable palette growth stays frame-local");
    source.matrices.resize(1);
    palette.update(0, source);
    require(palette.matrixCount(0) == 1, "Descriptor range follows reduced palette length");
    rejects(
        [&]
        {
            palette.update(0, {});
        });
    source.matrices[0][0][0] = std::numeric_limits<float>::infinity();
    rejects(
        [&]
        {
            palette.update(0, source);
        });
    require(palette.matrixCount(0) == 1 && palette.descriptorSet(0) == descriptor,
            "Rejected upload retains prior palette");

    GtsPreparedSkinnedMesh mesh;
    mesh.vertices = {{.pos = {-0.5f, -0.5f, 0}, .joints = {1, 0, 0, 0}, .weights = {1, 0, 0, 0}},
                     {.pos = {0.5f, -0.5f, 0}, .joints = {1, 0, 0, 0}, .weights = {1, 0, 0, 0}},
                     {.pos = {0.0f, 0.5f, 0}, .joints = {1, 0, 0, 0}, .weights = {1, 0, 0, 0}}};
    mesh.indices  = {0, 1, 2};
    mesh.primitives.push_back({0, 3, 9});
    VulkanSkinnedMeshResource geometry(backend.device(), backend.physicalDevice(), mesh);
    require(geometry.requiredPaletteSize() == 2 && geometry.primitives()[0].materialIndex == 9,
            "Geometry retains skin slots and material ranges");
    rejects(
        [&]
        {
            geometry.drawPrimitive(VK_NULL_HANDLE, VK_NULL_HANDLE, palette, 0, 0, VK_NULL_HANDLE);
        });

    VulkanRenderPassConfig passConfig;
    passConfig.colorFormat      = VK_FORMAT_R8G8B8A8_UNORM;
    passConfig.depthFormat      = VK_FORMAT_D32_SFLOAT;
    passConfig.colorFinalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VulkanRenderPass  pass(backend, passConfig);
    const std::string shaders = argv[1];
    for (const auto* fragment : {"frag.spv", "frag_pbr.spv"})
    {
        VulkanPipelineConfig scene;
        scene.vkRenderPass       = pass.getRenderPass();
        scene.fragmentShaderPath = shaders + "/" + fragment;
        scene.pushConstantSize   = 64;
        auto skinned             = makeVulkanSkinnedPipelineConfig(
            scene, sceneDescriptors.getDescriptorSetLayouts(), palette.layout(), shaders + "/skinned_vert.spv");
        require(skinned.vertexAttributes.size() == 8 && skinned.vertexAttributes.back().location == 7 &&
                    skinned.descriptorSetLayouts.size() == 5 && skinned.pushConstantSize == 64 &&
                    scene.vertexAttributes.size() == 5 && scene.descriptorSetLayouts.empty(),
                "Skinned configuration is separate and retains scene material contract");
        VulkanPipeline pipeline(backend, sceneDescriptors, skinned);
        require(pipeline.getPipeline() != VK_NULL_HANDLE, "Production material pipeline creation");
    }
    // Exercise actual extracted mixed-model inputs through the production caches.
    {
        ScopedRuntimeAssetPolicy policy("development");
        auto root = std::filesystem::temp_directory_path() / "gravitas-model-gpu-smoke";
        std::filesystem::create_directories(root);
        GltfFixtureBuilder fixture;
        auto staticMesh = at(field(fixture.root, "meshes"), 0);
        fixture.addVertexStream("JOINTS_0", std::vector<uint8_t>(12, 0), "VEC4", 5121);
        fixture.addVertexStream("WEIGHTS_0", floats({1,0,0,0,1,0,0,0,1,0,0,0}), "VEC4", 5126);
        std::get<Array>(field(fixture.root, "meshes").value).push_back(staticMesh);
        field(fixture.root, "nodes") = parse(R"([{"mesh":0,"skin":0},{},{"mesh":1}])");
        field(fixture.root, "skins") = parse(R"([{"joints":[1]}])");
        field(fixture.root, "scenes") = parse(R"([{"nodes":[0,1,2]}])");
        GtsModelRegistry registry;
        GtsModelRealizationCache cache;
        RenderResourceManager resources(backend);
        ECSWorld world;
        auto requested = registry.requestModel(fixture.write(root, GltfFixtureFormat::Glb));
        require(requested.succeeded(), "Model smoke load");
        auto& service = modelInstances(world, cache, &resources);
        auto createdA = service.create(requested.handle()), createdB = service.create(requested.handle());
        require(createdA.succeeded() && createdB.succeeded(), "Model smoke instances");
        std::shared_ptr<GtsModelInstance> a = std::move(createdA.instance), b = std::move(createdB.instance);
        auto af = extractModelRenderState(a, glm::mat4(1), gts::rendering::materialRuntime(world), &resources);
        auto bf = extractModelRenderState(b, glm::mat4(1), gts::rendering::materialRuntime(world), &resources);
        require(af.succeeded() && bf.succeeded(), "Model smoke extraction");
        const auto id = resources.realizeStaticGeometry(af.frame.staticDraws[0].geometry);
        const auto buffer = resources.getMesh(id)->vertexBuffer;
        require(resources.realizeStaticGeometry(bf.frame.staticDraws[0].geometry) == id &&
                resources.getMesh(id)->vertices.empty() && resources.getMesh(id)->indices.empty(),
                "Static GPU reuse with no persistent CPU geometry mirror");
        af.frame.cameraViewID = bf.frame.cameraViewID = resources.requestCameraBuffer();
        VulkanSkinnedSceneRenderer skinned(backend, resources.getDescriptorSetManager(), resources, pass.getRenderPass());
        auto combined = af.frame;
        combined.skinnedDraws.push_back(bf.frame.skinnedDraws[0]);
        skinned.prepare(combined, 0);
        require(skinned.residentGeometryCount() == 1 && skinned.residentPaletteCount() == 2,
                "Shared skinned GPU geometry with separate occurrence palettes");
        VulkanModelStaticDraws staticDraws(resources);
        std::vector<RenderCommand> commands;
        MaterialFrameData materials;
        staticDraws.append(af.frame, 0, commands, materials);
        require(commands.size() == 1 && commands[0].meshID == id && commands[0].indexCount == 3,
                "Extracted static model reaches ordinary static commands");
        // No submission is in flight in this resource smoke test.
        a.reset(); af = {}; combined = {};
        skinned.prepare(bf.frame, 0);
        require(skinned.residentGeometryCount() == 1 && skinned.residentPaletteCount() == 1 &&
                resources.realizeStaticGeometry(bf.frame.staticDraws[0].geometry) == id && resources.getMesh(id)->vertexBuffer == buffer,
                "Destroying one occurrence never recreates shared GPU geometry");
        gts::rendering::resetMaterialRuntime(world);
        std::filesystem::remove_all(root);
    }
    vkDeviceWaitIdle(backend.device());
    return 0;
}
catch (const std::exception& error)
{
    std::fprintf(stderr, "%s\n", error.what());
    if (std::string(error.what()).find("failed to find GPUs with Vulkan support") != std::string::npos)
        return 77;
    return 1;
}
