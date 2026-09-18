#include "../importers/gltf/GltfFixtureBuilder.h"
#include "assets/runtime/model/GtsModelRegistry.h"
#include "assets/runtime/model/GtsModelResource.h"
#include "assets/runtime/model/GtsPreparedModelDefinition.h"
#include "AssetSerializers.h"
#include "ScopedRuntimeAssetPolicy.h"

using namespace gts::rendering;

namespace
{
    MeshAssetData triangle()
    {
        MeshAssetData mesh;
        mesh.debugName  = "cooked triangle";
        mesh.attributes = StandardVertexAttributes;
        mesh.vertices.resize(3);
        mesh.vertices[1].pos.x = 1;
        mesh.vertices[2].pos.y = 1;
        mesh.indices           = {0, 1, 2};
        mesh.submeshes.push_back({0, 3, AssetReference::fromLogicalPath("surface.gmat"), "surface"});
        mesh.dependencies.push_back(mesh.submeshes[0].material);
        return mesh;
    }
    void requireFailure(const GtsModelRequestResult& result, const char* code)
    {
        require(!result.succeeded() && !result.handle(), "Failure exposes no partial resource");
        require(std::any_of(result.diagnostics().begin(),
                            result.diagnostics().end(),
                            [&](const auto& diagnostic)
                            {
                                return diagnostic.code == code;
                            }),
                code);
    }
} // namespace

int main()
{
    ScopedRuntimeAssetPolicy policy("development");
    const auto               root = std::filesystem::temp_directory_path() / "gravitas-model-loading-test";
    std::filesystem::create_directories(root);
    std::string error;
    const auto  meshPath = root / "triangle.gmesh";
    const auto  original = triangle();
    require(MeshAssetSerializer::writeFile(original, meshPath, &error), "Write existing cooked v1 fixture");
    GtsModelRegistry registry;
    const auto       loaded = registry.requestModel(meshPath);
    require(loaded.succeeded(), "Explicit gmesh loads as model");
    const auto& resource = *loaded.handle();
    require(!resource.canonicalModel() && resource.preparedModel(),
            "Prepared data does not fabricate canonical streams");
    require(resource.meshCount() == 1 && resource.nodes().size() == 1 && resource.nodes()[0].meshIndex == 0,
            "Minimal static occurrence");
    require(resource.capabilities().geometry && !resource.capabilities().skeletons &&
                !resource.capabilities().animations,
            "Honest static capabilities");
    require(resource.preparedModel()->meshes[0].vertices == original.vertices &&
                resource.preparedModel()->meshes[0].indices == original.indices,
            "Cooked prepared bytes/values preserved without preparation");
    require(resource.preparedModel()->meshes[0].submeshes[0].material.logicalPath == "surface.gmat",
            "Primitive material reference retained");
    requireFailure(registry.requestModel(GtsModelRequest{meshPath, {.animations = true}}),
                   "model.request.capabilities");
    ModelAssetData package;
    package.meshes.push_back(AssetReference::fromLogicalPath("triangle.gmesh"));
    package.materials.push_back(original.submeshes[0].material);
    package.dependencies = {package.meshes[0], package.materials[0]};
    package.nodes.resize(3);
    package.nodes[0].name                = "root";
    package.nodes[1].parentIndex         = 0;
    package.nodes[1].mesh                = package.meshes[0];
    package.nodes[1].localTransform[3].x = 4;
    package.nodes[2].parentIndex         = 1;
    package.nodes[2].mesh                = package.meshes[0];
    const auto packagePath               = root / "hierarchy.gmodel";
    require(ModelAssetSerializer::writeFile(package, packagePath, &error), "Write cooked model package");
    const auto hierarchy = registry.requestModel(packagePath);
    require(hierarchy.succeeded(), "gmodel hierarchy loads");
    require(hierarchy.handle()->meshCount() == 1 && hierarchy.handle()->nodes().size() == 3 &&
                hierarchy.handle()->nodes()[0].children == std::vector<uint32_t>{1} &&
                hierarchy.handle()->nodes()[1].children == std::vector<uint32_t>{2} &&
                hierarchy.handle()->nodes()[1].localTransform[3].x == 4,
            "Hierarchy, transforms and shared mesh associations retained");
    require(hierarchy.handle()->capabilities().hierarchy &&
                hierarchy.handle()->preparedModel()->dependencies.size() == 2,
            "Package capabilities and dependencies retained");
    require(ModelAssetSerializer::writeFile(package, root / "triangle.gmodel", &error),
            "Write richer adjacent cooked package");
    package.nodes[0].parentIndex = 2;
    const auto cyclePath         = root / "cycle.gmodel";
    require(ModelAssetSerializer::writeFile(package, cyclePath, &error),
            "Codec can encode cycle; model boundary must reject");
    requireFailure(registry.requestModel(cyclePath), "model.cooked.invalid");

    // Source dispatch and rich-definition fidelity, independent of source extension.
    GtsModelRegistry   sources;
    GltfFixtureBuilder fixture;
    field(fixture.root, "extensionsUsed") = parse(R"(["TEST_optional_extension"])");
    std::filesystem::create_directories(root / "gltf");
    const auto gltf         = fixture.write(root / "gltf", GltfFixtureFormat::External);
    const auto staticSource = sources.requestModel(gltf);
    require(staticSource.succeeded() && staticSource.handle()->canonicalModel(), "gltf source uses canonical importer");
    require(!staticSource.diagnostics().empty(), "Successful source warnings preserved");
    require(sources.requestModel(gltf).diagnostics()[0].code == staticSource.diagnostics()[0].code,
            "Cached warnings retained");
    requireFailure(sources.requestModel(GtsModelRequest{gltf, {.animations = true}}), "model.request.capabilities");
    requireFailure(sources.requestModel(root / "file.xyz"), "model.request.unsupported");
    const auto obj = root / "triangle.obj";
    std::ofstream(obj) << "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
    const auto staticCooked = sources.requestModel(GtsModelRequest{obj, {.hierarchy = true}});
    require(staticCooked.succeeded() && staticCooked.handle()->preparedModel(),
            "Compatible cooked wins for static OBJ");
    require(staticCooked.handle()->nodes().size() == 3,
            "gmodel is preferred over flat gmesh without flattening hierarchy");
    require(staticCooked.handle()->identityPath() == std::filesystem::canonical(obj),
            "Identity remains request, not selected representation");
    ScopedRuntimeAssetPolicy::set("strict");
    require(sources.requestModel(obj).handle() == staticCooked.handle(), "Cached cooked usable under strict policy");
    requireFailure(sources.requestModel(gltf), "model.request.cooked_required");
    requireFailure(sources.requestModel(root / "missing.obj"), "model.request.cooked_required");
    ScopedRuntimeAssetPolicy::set("development");
    require(sources.requestModel(gltf).handle() == staticSource.handle(), "Strict failure preserves development cache");
    requireFailure(sources.requestModel(root / "missing.gmesh"), "model.cooked.missing");

    const auto fallback = root / "fallback.obj";
    std::filesystem::copy_file(obj, fallback, std::filesystem::copy_options::overwrite_existing);
    auto fallbackResult = sources.requestModel(fallback);
    require(fallbackResult.succeeded() && fallbackResult.handle()->canonicalModel(),
            "Missing cooked allows canonical OBJ fallback");
    requireFailure(sources.requestModel(GtsModelRequest{fallback, {.skeletons = true}}), "model.request.capabilities");
    require(MeshAssetSerializer::writeFile(original, root / "fallback.gmesh", &error),
            "Create cooked after source was cached");
    auto upgraded = sources.requestModel(fallback);
    require(upgraded.succeeded() && upgraded.handle()->preparedModel() && upgraded.handle() != fallbackResult.handle(),
            "Resolution reconsiders newly available cooked content before source cache reuse");
    require(sources.lookup(fallbackResult.handle()) && sources.lookup(upgraded.handle()),
            "Both immutable representations remain retained");
    std::ofstream(root / "broken.obj") << "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
    std::ofstream(root / "broken.gmesh") << "corrupt";
    const auto beforeFailure = sources.size();
    requireFailure(sources.requestModel(root / "broken.obj"), "model.cooked.invalid");
    require(sources.size() == beforeFailure, "Corrupt selected cooked never falls back or publishes partial source");
    require(MeshAssetSerializer::writeFile(original, root / "broken.gmesh", &error), "Repair cooked");
    require(sources.requestModel(root / "broken.obj").succeeded(), "Failed loads retry after repair");

    fixture.addVertexStream("JOINTS_0", std::vector<uint8_t>(12, 0), "VEC4", 5121);
    fixture.addVertexStream("WEIGHTS_0", floats({1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}), "VEC4", 5126);
    field(fixture.root, "nodes")  = parse(R"([{"mesh":0,"skin":0},{}])");
    field(fixture.root, "scenes") = parse(R"([{"nodes":[0,1]}])");
    field(fixture.root, "skins")  = parse(R"([{"joints":[1]}])");
    const auto animation          = fixture.addAnimation("motion");
    const auto input              = fixture.addAnimationTimes({0, 1});
    const auto output             = fixture.addAccessor(floats({0, 0, 0, 0, 1, 0}), "VEC3", 5126, 2);
    const auto sampler            = fixture.addAnimationSampler(animation, input, output, "LINEAR");
    fixture.addAnimationChannel(animation, sampler, 1, "translation");
    std::filesystem::create_directories(root / "animated");
    const auto animatedPath  = fixture.write(root / "animated", GltfFixtureFormat::Glb);
    auto       staticSibling = animatedPath;
    staticSibling.replace_extension(".gmesh");
    require(MeshAssetSerializer::writeFile(original, staticSibling, &error),
            "Adjacent static artifact cannot replace animated source");
    const GtsModelRequest animatedRequest{
        animatedPath, {.geometry = true, .skeletons = true, .skinBindings = true, .animations = true}};
    const auto animated = sources.requestModel(animatedRequest);
    require(animated.succeeded() && animated.handle()->capabilities().satisfies(animatedRequest.requiredCapabilities),
            "Animated capability request succeeds through same API");
    require(sources.requestModel(animatedPath).handle() == animated.handle(),
            "Default GLB request preserves richest definition too");
    ScopedRuntimeAssetPolicy::set("strict");
    auto strict = sources.requestModel(animatedRequest);
    requireFailure(strict, "model.request.cooked_required");
    auto strictAgain = sources.requestModel(animatedRequest);
    require(strict.diagnostics().size() == strictAgain.diagnostics().size() &&
                strict.diagnostics().back().message == strictAgain.diagnostics().back().message,
            "Deterministic policy diagnostics");
    requireFailure(sources.requestModel(GtsModelRequest{staticSibling, {.animations = true}}),
                   "model.request.capabilities");
    ScopedRuntimeAssetPolicy::set("development");
    require(sources.requestModel(animatedRequest).handle() == animated.handle(),
            "Strict failure did not poison animated source cache");
    const auto disguisedSource = root / "disguised.obj";
    std::filesystem::copy_file(obj, disguisedSource, std::filesystem::copy_options::overwrite_existing);
    require(sources.requestModel(disguisedSource).succeeded(), "Source snapshot available for provenance test");
    std::error_code symlinkError;
    std::filesystem::create_symlink(disguisedSource, root / "disguised.gmesh", symlinkError);
    if (!symlinkError)
    {
        ScopedRuntimeAssetPolicy::set("strict");
        requireFailure(sources.requestModel(disguisedSource), "model.cooked.invalid");
        ScopedRuntimeAssetPolicy::set("development");
    }
    // Explicitly incompatible artifacts are skipped without trying to interpret their bytes.
    std::ofstream(staticSibling) << "corrupt static data";
    require(sources.requestModel(animatedRequest).succeeded(),
            "Incompatible static candidate is never selected for animated source");
    auto invalid              = triangle();
    invalid.vertices[0].pos.x = std::numeric_limits<float>::infinity();
    require(MeshAssetSerializer::writeFile(invalid, root / "nonfinite.gmesh", &error),
            "Encode invalid geometry for boundary test");
    requireFailure(sources.requestModel(root / "nonfinite.gmesh"), "model.cooked.invalid");
    package.nodes[0].parentIndex = -1;
    package.meshes[0]            = AssetReference::fromLogicalPath("missing.gmesh");
    require(ModelAssetSerializer::writeFile(package, root / "missing-child.gmodel", &error), "Encode missing child");
    requireFailure(sources.requestModel(root / "missing-child.gmodel"), "model.cooked.invalid");
    std::filesystem::remove_all(root);
}
