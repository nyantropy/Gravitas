#include "GtsModelResource.h"
#include "GtsModelRealizationCache.h"
#include "GtsModelImportResult.h"
#include <limits>
#include "GtsModelCooker.h"
#include "GtsGltfModelImporter.h"
#include "GtsObjModelImporter.h"
#include "GtsModelRegistry.h"
#include "GtsPreparedModelDefinition.h"
#include "GtsStaticMeshPreparation.h"
#include "AssetSerializers.h"
#include "GtsModelInstanceRuntime.h"
#include "GtsModelRenderExtraction.h"
#include "../rendering/material/TestMaterialResources.h"
#include "runtime/ScopedRuntimeAssetPolicy.h"
#include <cstring>
#include <fstream>
#include <iostream>

using namespace gts::rendering;
namespace
{
    void check(bool value, const std::string& message)
    {
        if (!value)
            throw std::runtime_error(message);
    }
    std::vector<char> bytes(const std::filesystem::path& p)
    {
        std::ifstream f(p, std::ios::binary);
        check(bool(f), "read " + p.string());
        return {std::istreambuf_iterator<char>(f), {}};
    }
    void cookedOK(const AssetCookResult& result)
    {
        for (const auto& d : result.diagnostics)
            if (d.severity == AssetDiagnosticSeverity::Error)
                throw std::runtime_error(d.code + ": " + d.message);
        check(!result.outputs.empty(), "package produced");
    }
    struct World : ECSWorld
    {
        ~World()
        {
            resetMaterialRuntime(*this);
        }
    };
    std::shared_ptr<GtsModelInstance> instance(GtsModelInstanceRuntime& service, GtsModelHandle handle)
    {
        auto r = service.create(handle);
        check(r.succeeded(), r.diagnostics.empty() ? "instance creation" : r.diagnostics.front().message);
        return std::move(r.instance);
    }
    void roundtrip(const std::filesystem::path& source, const std::filesystem::path& output, bool golden = false)
    {
        ScopedRuntimeAssetPolicy policy("development");
        GtsModelRegistry         registry;
        auto                     loadedSource = registry.requestModel(source);
        check(loadedSource.succeeded(), "canonical source loading: " + source.string());
        const auto&        canonical = *loadedSource.handle()->canonicalModel();
        GtsModelCookerOptions options;
        options.outputDirectory = output;
        const auto result       = GtsModelCooker::cookSourceAsset(source, options);
        cookedOK(result);
        options.outputDirectory = output / "repeat";
        const auto repeat       = GtsModelCooker::cookSourceAsset(source, options);
        cookedOK(repeat);
        check(result.outputs.size() == repeat.outputs.size(), "deterministic file count");
        std::filesystem::path entry;
        for (const auto& file : result.outputs)
        {
            check(bytes(file.path) == bytes(options.outputDirectory / file.path.filename()),
                  "deterministic bytes/names");
            if (golden)
                check(bytes(file.path) == bytes(source.parent_path() / "legacy-golden" / file.path.filename()),
                      "legacy byte parity " + file.path.string());
            if (file.type == CookedAssetOutputType::Model ||
                (entry.empty() && file.type == CookedAssetOutputType::Mesh))
                entry = file.path;
        }
        ScopedRuntimeAssetPolicy::set("strict");
        auto loaded = registry.requestModel(entry);
        check(loaded.succeeded(), "strict load newly cooked package");
        check(loaded.handle()->canonicalModel() == nullptr, "no fake canonical reconstruction");
        const auto& prepared = *loaded.handle()->preparedModel();
        check(prepared.nodes.size() == canonical.nodes.size(), "node count through storage");
        for (size_t i = 0; i < canonical.nodes.size(); ++i)
        {
            check(prepared.nodes[i].localTransform == canonical.nodes[i].localTransform, "local transform preserved");
            check(prepared.nodes[i].children == canonical.nodes[i].children, "hierarchy preserved");
            check(prepared.nodes[i].meshIndex == canonical.nodes[i].meshIndex, "shared mesh indices preserved");
        }
        check(prepared.rootNodes == canonical.rootNodes, "root forest preserved");
        check(prepared.meshes.size() == canonical.meshes.size(), "one mesh per definition");
        for (size_t i = 0; i < canonical.meshes.size(); ++i)
        {
            auto p = prepareGtsStaticMesh(canonical.meshes[i]);
            check(p.succeeded(), "canonical preparation");
            const auto& g = *p.mesh();
            const auto& c = prepared.meshes[i];
            check(g.vertices.size() == c.vertices.size() && g.indices == c.indices, "geometry counts/indices");
            check(std::memcmp(g.vertices.data(), c.vertices.data(), g.vertices.size() * sizeof(GtsStaticVertex)) == 0,
                  "prepared vertex bytes");
            check(g.metadata.attributes == c.attributes && g.metadata.generatedNormals == c.generatedNormals &&
                      g.metadata.generatedTangents == c.generatedTangents,
                  "metadata");
            check(c.bounds.min == result.meshes[i].bounds.min && c.bounds.max == result.meshes[i].bounds.max,
                  "mesh-local bounds");
            check(g.primitives.size() == c.submeshes.size(), "primitive count");
            for (size_t j = 0; j < g.primitives.size(); ++j)
                check(g.primitives[j].firstIndex == c.submeshes[j].firstIndex &&
                          g.primitives[j].indexCount == c.submeshes[j].indexCount,
                      "primitive ranges");
        }
        Resources                resources;
        GtsModelRealizationCache cache;
        World                    world;
        auto&                    service     = modelInstances(world, cache, &resources);
        auto                     a           = instance(service, loadedSource.handle());
        auto                     b           = instance(service, loaded.handle());
        auto                     sourceFrame = extractModelRenderState(a, glm::mat4(1), materialRuntime(world));
        auto                     cookedFrame = extractModelRenderState(b, glm::mat4(1), materialRuntime(world));
        check(sourceFrame.succeeded() && cookedFrame.succeeded(), "complete instance/extraction roundtrip");
        check(sourceFrame.frame.staticDraws.size() == cookedFrame.frame.staticDraws.size() &&
                  cookedFrame.frame.skinnedDraws.empty(),
              "occurrences converge");
        for (size_t i = 0; i < sourceFrame.frame.staticDraws.size(); ++i)
        {
            const auto& x = sourceFrame.frame.staticDraws[i];
            const auto& y = cookedFrame.frame.staticDraws[i];
            check(x.worldFromGeometry == y.worldFromGeometry && x.primitiveIndex == y.primitiveIndex,
                  "extracted hierarchy/ranges converge");
            check(x.material.parameters.baseColor == y.material.parameters.baseColor &&
                      x.material.parameters.metallic() == y.material.parameters.metallic() &&
                      x.material.parameters.roughness() == y.material.parameters.roughness(),
                  "runtime PBR factors converge");
            check(x.material.parameters.emissiveFactorStrength == y.material.parameters.emissiveFactorStrength &&
                      x.material.parameters.normalScale() == y.material.parameters.normalScale() &&
                      x.material.parameters.ambientOcclusionStrength() ==
                          y.material.parameters.ambientOcclusionStrength(),
                  "runtime emissive/normal/AO semantics");
            check(x.material.renderState.alphaMode == y.material.renderState.alphaMode &&
                      x.material.renderState.alphaCutoff == y.material.renderState.alphaCutoff &&
                      x.material.renderState.doubleSided == y.material.renderState.doubleSided &&
                      x.material.renderState.depthWrite == y.material.renderState.depthWrite,
                  "runtime alpha/sidedness semantics");
            check(y.geometry->staticVertices().data() ==
                      prepared.meshes[b->geometry()->occurrences[y.occurrenceIndex].geometryIndex].vertices.data(),
                  "cooked geometry reused without regeneration");
        }
        if (canonical.nodes.size() == 5)
        {
            check(cookedFrame.frame.staticDraws.size() == 4, "helper node creates no draw");
            check(cookedFrame.frame.staticDraws[0].geometry == cookedFrame.frame.staticDraws[2].geometry &&
                      cookedFrame.frame.staticDraws[0].geometry == cookedFrame.frame.staticDraws[3].geometry,
                  "shared mesh through complete runtime");
        }
        for (const auto& material : result.materials)
        {
            if (material.metallicRoughnessTexture.empty())
                continue;
            TextureAssetData packed;
            std::string      error;
            check(TextureAssetSerializer::readFile(
                      output / material.metallicRoughnessTexture.logicalPath, packed, &error),
                  error);
            check(packed.colorSpace == TextureColorSpace::Linear && packed.mips[0].bytes[1] == 96 &&
                      packed.mips[0].bytes[2] == 160,
                  "canonical scalar channels packed G/B");
            check(
                TextureAssetSerializer::readFile(output / material.ambientOcclusionTexture.logicalPath, packed, &error),
                error);
            check(packed.mips[0].bytes[0] == 32, "AO channel R");
            check(TextureAssetSerializer::readFile(output / material.baseColorTexture.logicalPath, packed, &error),
                  error);
            check(packed.colorSpace == TextureColorSpace::SRgb, "base color sRGB");
        }
        std::cout << "roundtrip " << source << '\n';
    }
    void rejected(const AssetCookResult& r, const std::filesystem::path& output)
    {
        check(r.hasErrors() && r.outputs.empty() && !std::filesystem::exists(output),
              "capability failure publishes no package");
    }
    void materialSlotsAndPublication(const std::filesystem::path& fixtures, const std::filesystem::path& root)
    {
        auto imported = GtsGltfModelImporter{}.importAsset({fixtures / "external.gltf"});
        check(imported.succeeded(), "canonical material fixture");
        auto model = *imported.asset();
        model.materials.push_back(model.materials[0]);
        model.meshes[0].primitives.push_back(model.meshes[0].primitives[0]);
        model.meshes[0].primitives[1].materialIndex = 1;
        model.materials[1].alphaMode                = GtsModelAlphaMode::Blend;
        GtsModelCookerOptions options;
        options.outputDirectory = root / "slots";
        const auto result       = GtsModelCooker::cookModelAsset(model, "slots.glb", options);
        cookedOK(result);
        check(result.materials.size() == 2 && result.materials[0].id != result.materials[1].id,
              "equal named slots remain distinct");
        check(result.materials[0].baseColorTexture.id == result.materials[1].baseColorTexture.id &&
                  result.materials[0].metallicRoughnessTexture.id == result.materials[1].metallicRoughnessTexture.id &&
                  result.textures.size() == 5,
              "role-sensitive image/packed combination reuse");
        check(result.materials[1].renderState.alphaMode == MaterialAlphaMode::Blend &&
                  !result.materials[1].renderState.depthWrite,
              "blend persistence");
        check(result.meshes[0].submeshes.size() == 2 && result.meshes[0].submeshes[1].firstIndex == 3 &&
                  result.meshes[0].submeshes[1].material.id == result.materials[1].id,
              "multiple primitive/material boundaries");
        // Independent channel semantics: roughness-only samples alpha; absent metallic is white.
        model.materials[0].metallicImage.reset();
        model.materials[0].roughnessImage->channel = GtsModelTextureChannel::Alpha;
        options.outputDirectory                    = root / "roughness";
        auto roughness                             = GtsModelCooker::cookModelAsset(model, "slots.glb", options);
        cookedOK(roughness);
        TextureAssetData packed;
        std::string      error;
        check(
            TextureAssetSerializer::readFile(
                options.outputDirectory / roughness.materials[0].metallicRoughnessTexture.logicalPath, packed, &error),
            error);
        check(packed.mips[0].bytes[1] == 255 && packed.mips[0].bytes[2] == 255,
              "roughness alpha selection and absent metallic default");
        // A later output collision must restore earlier existing files, with no partial model entry.
        options.outputDirectory = root / "rollback";
        std::filesystem::create_directories(options.outputDirectory / "slots_triangle.gmesh");
        const auto oldMaterial = options.outputDirectory / "slots_surface.gmat";
        {
            std::ofstream f(oldMaterial);
            f << "previous material";
        }
        const auto before = bytes(oldMaterial);
        const auto failed = GtsModelCooker::cookModelAsset(model, "slots.glb", options);
        check(failed.hasErrors() && failed.outputs.empty() && bytes(oldMaterial) == before &&
                  !std::filesystem::exists(options.outputDirectory / "slots.gmodel"),
              "publication rollback preserves preexisting package components");
        for (const auto& file : std::filesystem::directory_iterator(options.outputDirectory))
            check(file.path().filename().string().find(".assetc-") != 0, "staging cleaned after failure");
        // Missing encoded input fails before any output publication.
        model.images[0].source  = fixtures / "absent.png";
        options.outputDirectory = root / "missing-image";
        rejected(GtsModelCooker::cookModelAsset(model, "missing", options), options.outputDirectory);
    }
    void failures(const std::filesystem::path& fixtures, const std::filesystem::path& root)
    {
        GtsModelCookerOptions options;
        options.outputDirectory = root / "rejected";
        auto imported           = GtsGltfModelImporter{}.importAsset({fixtures / "simple.gltf"});
        check(imported.succeeded(), "import capability fixture");
        auto bundle = *imported.bundle();
        bundle.animationClips.emplace_back();
        rejected(GtsModelCooker::cookModelBundle(bundle, "clips", options), options.outputDirectory);
        bundle = {};
        rejected(GtsModelCooker::cookModelBundle(bundle, "modelless", options), options.outputDirectory);
        auto model = *imported.asset();
        model.skeletonUses.emplace_back();
        rejected(GtsModelCooker::cookModelAsset(model, "skeleton", options), options.outputDirectory);
        model = *imported.asset();
        model.skinBindings.emplace_back();
        rejected(GtsModelCooker::cookModelAsset(model, "binding", options), options.outputDirectory);
        model = *imported.asset();
        model.meshes[0].primitives[0].attributes.push_back(
            {GtsVertexSemantic::Weights, 0, std::vector<glm::vec4>(3, glm::vec4(1, 0, 0, 0))});
        rejected(GtsModelCooker::cookModelAsset(model, "weights", options), options.outputDirectory);
        model                             = *imported.asset();
        model.materials[0].baseColorImage = GtsModelImageBinding{0, 1};
        model.images.push_back({"bad", fixtures / "image.png"});
        rejected(GtsModelCooker::cookModelAsset(model, "uv1", options), options.outputDirectory);
        model                               = *imported.asset();
        model.nodes[0].localTransform[0][0] = std::numeric_limits<float>::infinity();
        rejected(GtsModelCooker::cookModelAsset(model, "invalid", options), options.outputDirectory);
    }
} // namespace
int main(int argc, char** argv)
{
    try
    {
        const auto fixtures = std::filesystem::path(__FILE__).parent_path() / "fixtures/canonical-cooking";
        const auto root     = std::filesystem::temp_directory_path() / "gravitas_canonical_cooking_test";
        std::filesystem::remove_all(root);
        for (const char* f :
             {"simple.gltf", "hierarchy.gltf", "embedded.glb", "external.gltf", "parity.gltf", "triangle.obj"})
            roundtrip(fixtures / f, root / f, std::string(f) == "parity.gltf");
        failures(fixtures, root);
        materialSlotsAndPublication(fixtures, root);
        if (argc > 1)
            roundtrip(argv[1], root / "real");
        if (argc > 2)
        {
            const auto imported = GtsGltfModelImporter{}.importAsset({argv[2]});
            check(imported.succeeded() && !imported.bundle()->skeletons.empty() &&
                      !imported.bundle()->animationClips.empty() && !imported.asset()->skinBindings.empty(),
                  "Yune canonical animation intact");
            GtsModelCookerOptions options;
            options.outputDirectory = root / "animated";
            rejected(GtsModelCooker::cookSourceAsset(argv[2], options), options.outputDirectory);
            ScopedRuntimeAssetPolicy policy("strict");
            GtsModelRegistry         registry;
            check(!registry
                       .requestModel(GtsModelRequest{
                           argv[2], {.geometry = true, .skeletons = true, .skinBindings = true, .animations = true}})
                       .succeeded(),
                  "strict animated request cannot accept static cooked data");
        }
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
