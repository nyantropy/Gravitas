#include "../importers/gltf/GltfFixtureBuilder.h"
#include "assets/runtime/model/GtsModelRegistry.h"
#include "assets/runtime/model/GtsModelResource.h"
#include "assets/skeleton/GtsSkeletonAsset.h"
#include <type_traits>

namespace
{
    std::filesystem::path write(GltfFixtureBuilder fixture, const std::filesystem::path& root, const char* name)
    {
        const auto directory = root / name;
        std::filesystem::create_directories(directory);
        return fixture.write(directory, GltfFixtureFormat::Glb);
    }
    void addClip(GltfFixtureBuilder& fixture, const char* name, uint32_t node)
    {
        const auto animation = fixture.addAnimation(name);
        const auto input     = fixture.addAnimationTimes({0, 1});
        const auto output    = fixture.addAccessor(floats({0, 0, 0, 0, 1, 0}), "VEC3", 5126, 2);
        const auto sampler   = fixture.addAnimationSampler(animation, input, output, "LINEAR");
        fixture.addAnimationChannel(animation, sampler, node, "translation");
    }
    GltfFixtureBuilder rig()
    {
        GltfFixtureBuilder fixture;
        fixture.addVertexStream("JOINTS_0", std::vector<uint8_t>(12, 0), "VEC4", 5121);
        fixture.addVertexStream("WEIGHTS_0", floats({1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}), "VEC4", 5126);
        field(fixture.root, "nodes")  = parse(R"([{"mesh":0,"skin":0},{"name":"joint"},{"mesh":0,"skin":0}])");
        field(fixture.root, "skins")  = parse(R"([{"joints":[1]}])");
        field(fixture.root, "scenes") = parse(R"([{"nodes":[0,1,2]}])");
        addClip(fixture, "Motion", 1);
        return fixture;
    }
    void requireFailure(const GtsModelRequestResult& result)
    {
        require(!result.succeeded() && !result.handle() && !result.diagnostics().empty(),
                "Failure has diagnostics and no resource");
    }
} // namespace

int main()
{
    const auto root = std::filesystem::temp_directory_path() / "gravitas-model-registry-test";
    std::filesystem::create_directories(root);
    GtsModelRegistry registry;
    auto             plain              = GltfFixtureBuilder{};
    field(plain.root, "extensionsUsed") = parse(R"(["TEST_model_registry_optional"])");
    const auto path                     = write(plain, root, "static");
    auto       loaded                   = registry.requestModel(path);
    require(loaded.succeeded(), "Static model loads");
    auto handle = loaded.handle();
    require(registry.lookup(handle) == handle.get() && registry.size() == 1, "Handle identifies registry entry");
    require(handle->clips().empty() && handle->skeletons().empty() && handle->model().skinBindings.empty(),
            "Static capabilities need no placeholders");
    require(!loaded.diagnostics().empty() && loaded.diagnostics()[0].severity == GtsModelDiagnosticSeverity::Warning,
            "Importer warning preserved");
    auto repeated = registry.requestModel(path.parent_path() / "." / path.filename());
    require(repeated.handle() == handle && registry.size() == 1, "Normalized path reuses resource");
    require(repeated.diagnostics()[0].code == loaded.diagnostics()[0].code, "Cache hit preserves warning");
    require(registry.requestModel(std::filesystem::relative(path)).handle() == handle,
            "Relative and absolute paths alias");
    const auto      alias = root / "alias.glb";
    std::error_code ignored;
    std::filesystem::remove(alias, ignored);
    std::filesystem::create_symlink(path, alias, ignored);
    if (!ignored)
    {
        require(registry.requestModel(alias).handle() == handle, "Symlink resolves to source identity");
    }
    auto distinct = registry.requestModel(write(plain, root, "separate"));
    require(distinct.succeeded() && distinct.handle() != handle, "Separate identical files are distinct resources");
    GtsModelRegistry other;
    require(other.lookup(handle) == nullptr && registry.lookup({}) == nullptr,
            "Foreign or empty handles do not resolve here");
    require(other.requestModel(path).handle() != handle, "Registry entries have their own identity");
    require(other.lookup(handle) == nullptr, "Same-path entries in different registries do not accept foreign handles");
    const auto initialSize = registry.size();
    requireFailure(registry.requestModel({}));
    requireFailure(registry.requestModel(root / "missing.glb"));
    requireFailure(registry.requestModel(root / "unsupported.gmesh"));
    auto malformed                                         = rig();
    field(at(field(malformed.root, "skins"), 0), "joints") = parse("[99]");
    const auto retryPath                                   = write(malformed, root, "retry");
    requireFailure(registry.requestModel(retryPath));
    require(registry.size() == initialSize, "Failed loads publish nothing");
    write(rig(), root, "retry");
    auto animated = registry.requestModel(retryPath);
    require(animated.succeeded(), "Failed request can be repaired and retried");
    const auto& model = animated.handle()->model();
    require(animated.handle()->skeletons().size() == 1 && !model.skinBindings.empty(),
            "Rig definitions and bindings retained");
    require(model.skeletonUses[0].skeleton == animated.handle()->skeletons()[0], "Bundle definition sharing preserved");
    require(model.nodes[0].skinBindingIndex == model.nodes[2].skinBindingIndex,
            "Shared source rig associations survive");
    require(animated.handle()->clips().size() == 1, "Animation enumeration exposed");
    const auto found = animated.handle()->findClip("Motion", 0);
    require(found.succeeded() && found.reference->model() == animated.handle().get(), "Clip is scoped to resource");
    require(animated.handle()->clip(*found.reference)->name == "Motion", "Scoped reference resolves");
    require(handle->clip(*found.reference) == nullptr, "Cross-model clip reference rejected");
    auto matchingDefinition = registry.requestModel(write(rig(), root, "matching-rig"));
    require(matchingDefinition.handle()->findClip("Motion", 0).succeeded(),
            "Equivalent definition can select its own clip");
    require(matchingDefinition.handle()->clip(*found.reference) == nullptr,
            "Identical compatible data does not erase clip resource scope");
    require(!animated.handle()->findClip("missing").succeeded(), "Missing name fails");
    require(!animated.handle()->findClip("Motion", 99).succeeded(), "Invalid skeleton use fails");
    auto duplicate = rig();
    addClip(duplicate, "Motion", 1);
    auto ambiguous = registry.requestModel(write(duplicate, root, "ambiguous"));
    require(ambiguous.succeeded() && ambiguous.handle()->clips().size() == 2,
            "Duplicate display names remain valid canonical data");
    require(ambiguous.handle()->findClip("Motion").diagnostics[0].code == "model.clip.ambiguous",
            "Name selection rejects ambiguity");
    auto twoRigs = rig();
    field(twoRigs.root, "nodes") =
        parse(R"([{"mesh":0,"skin":0},{"name":"same"},{"mesh":0,"skin":1},{"name":"same","translation":[1,0,0]}])");
    field(twoRigs.root, "skins")  = parse(R"([{"joints":[1]},{"joints":[3]}])");
    field(twoRigs.root, "scenes") = parse(R"([{"nodes":[0,1,2,3]}])");
    auto multi                    = registry.requestModel(write(twoRigs, root, "two-rigs"));
    require(multi.succeeded() && multi.handle()->skeletons().size() == 2, "Distinct skeleton definitions retained");
    const auto& uses = multi.handle()->model().skeletonUses;
    require(uses.size() == 2, "Distinct occurrences retained");
    require(multi.handle()->findClip("Motion", 0).succeeded(), "Exact compatible use accepted");
    require(multi.handle()->findClip("Motion", 1).diagnostics[0].code == "model.clip.incompatible",
            "Other rig rejected despite equal display names");
    std::weak_ptr<const GtsModelResource> weak;
    {
        auto temporary = registry.requestModel(write(rig(), root, "retained"));
        weak           = temporary.handle();
    }
    require(!weak.expired(), "Registry owns resource without request results or game instances");
    GtsModelHandle survivor;
    {
        GtsModelRegistry temporary;
        survivor = temporary.requestModel(path).handle();
    }
    require(survivor && !survivor->model().meshes.empty(),
            "Strong model reference can safely outlive registry shutdown");
    static_assert(std::is_const_v<GtsModelHandle::element_type>);
    static_assert(std::is_same_v<decltype(handle->model()), const GtsModelAsset&>);
    static_assert(!std::is_copy_constructible_v<GtsModelResource>);
    std::filesystem::remove_all(root);
}
