#include "../assets/importers/gltf/GltfFixtureBuilder.h"
#include "../assets/runtime/ScopedRuntimeAssetPolicy.h"
#include "rendering/core/model/GtsModelInstanceRuntime.h"
#include "assets/loading/model/GtsModelRegistry.h"
#include "assets/model/GtsModelAsset.h"
#include "assets/animation/GtsAnimationClipAsset.h"
#include "assets/serialization/AssetSerializers.h"
#include <limits>
#include <type_traits>

using namespace gts::rendering;
namespace
{
    struct World : ECSWorld
    {
        ~World()
        {
            resetMaterialRuntime(*this);
        }
    };
    GtsModelHandle load(GtsModelRegistry& registry, GltfFixtureBuilder fixture, const std::filesystem::path& path)
    {
        std::filesystem::create_directories(path);
        auto        result = registry.requestModel(fixture.write(path, GltfFixtureFormat::Glb));
        std::string errors;
        for (const auto& error : result.diagnostics())
            errors += error.message + "\n";
        require(result.succeeded(), errors);
        return result.handle();
    }
    std::unique_ptr<GtsModelInstance> create(GtsModelInstanceRuntime& runtime, GtsModelHandle handle)
    {
        auto        result = runtime.create(std::move(handle));
        std::string errors;
        for (const auto& error : result.diagnostics)
            errors += error.message + "\n";
        require(result.succeeded(), errors);
        return std::move(result.instance);
    }
    GtsModelClipReference clip(const GtsModelHandle& model, const char* name, uint32_t use = 0)
    {
        auto result = model->findClip(name, use);
        require(result.succeeded(), result.diagnostics.empty() ? "clip lookup" : result.diagnostics[0].message);
        return *result.reference;
    }
    void ok(GtsModelInstanceStatus result)
    {
        require(result.succeeded(), result.diagnostics.empty() ? "instance status" : result.diagnostics[0].message);
    }
    void animation(GltfFixtureBuilder&          f,
                   const char*                  name,
                   uint32_t                     node,
                   const char*                  property = "translation",
                   std::initializer_list<float> values   = {0, 0, 0, 0, 2, 0})
    {
        auto a       = f.addAnimation(name);
        auto input   = f.addAnimationTimes({0, 1});
        auto output  = f.addAccessor(floats(values), "VEC3", 5126, 2);
        auto sampler = f.addAnimationSampler(a, input, output);
        f.addAnimationChannel(a, sampler, node, property);
    }
    GltfFixtureBuilder rig(bool animated = true)
    {
        GltfFixtureBuilder f;
        f.addVertexStream("JOINTS_0", std::vector<uint8_t>(12, 0), "VEC4", 5121);
        f.addVertexStream("WEIGHTS_0", floats({1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}), "VEC4", 5126);
        field(f.root, "nodes")  = parse(R"([{"mesh":0,"skin":0,"translation":[30,0,0]}, {}, {"mesh":0,"skin":1}])");
        field(f.root, "skins")  = parse(R"([{"joints":[1]},{"joints":[1]}])");
        field(f.root, "scenes") = parse(R"([{"nodes":[0,1,2]}])");
        const auto ib = f.addAccessor(floats({1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 2, 0, 0, 1}), "MAT4", 5126, 1);
        field(at(field(f.root, "skins"), 1), "inverseBindMatrices") = ib;
        if (animated)
        {
            animation(f, "motion", 1);
            animation(f, "other", 1, "translation", {0, 0, 0, 0, 4, 0});
        }
        return f;
    }
    void staticAndLifetime(const std::filesystem::path& root)
    {
        World                                 world;
        std::unique_ptr<GtsModelInstance>     retained;
        std::weak_ptr<const GtsModelResource> resource;
        std::weak_ptr<const GtsRealizedModel> geometry;
        {
            GtsModelRegistry         registry;
            GtsModelRealizationCache cache;
            auto&                    runtime = modelInstances(world, cache, nullptr);
            require(!runtime.create({}).succeeded(), "invalid handle exposes no instance");
            auto model = load(registry, GltfFixtureBuilder{}, root / "static");
            retained   = create(runtime, model);
            resource   = model;
            geometry   = retained->geometry();
            auto other = create(runtime, model);
            require(other->model() == retained->model() && other->geometry() == retained->geometry() &&
                        other->materials() == retained->materials(),
                    "shared resource/geometry/world materials");
            require(retained->skeletonOccurrences().empty() && !retained->palette(0) &&
                        !retained->paletteForOccurrence(0),
                    "static model has no dummy animation");
            require(retained->materialFor(0, 0).valid(), "static occurrence material view");
            other.reset();
            require(retained->worldMaterialsValid(), "one occurrence destruction leaves another valid");
            // Cooked static data uses the same creation operation without fabricated canonical data.
            MeshAssetData mesh;
            mesh.vertices.resize(3);
            mesh.vertices[1].pos.x = 1;
            mesh.vertices[2].pos.y = 1;
            mesh.indices           = {0, 1, 2};
            mesh.submeshes         = {{0, 3, {}, {}}};
            std::string error;
            require(MeshAssetSerializer::writeFile(mesh, root / "static.gmesh", &error), error);
            auto cooked = registry.requestModel(root / "static.gmesh");
            require(cooked.succeeded(), "cooked static load");
            auto staticCooked = create(runtime, cooked.handle());
            require(staticCooked->skeletonOccurrences().empty() && !staticCooked->model()->canonicalModel(),
                    "cooked static instance without canonical backing");
        }
        require(!resource.expired() && !geometry.expired() && retained->model()->nodes().size() == 1,
                "instance retains definition/geometry after registry and cache destruction");
        resetMaterialRuntime(world);
        require(!retained->worldMaterialsValid() && !retained->materialFor(0, 0).valid(), "material reset detected");
        require(!resource.expired() && !geometry.expired(), "reset cannot destroy definitions");
        ok(retained->updateAnimations(0.1));
        ok(retained->rebindMaterials(modelMaterialRealization(world, nullptr).realize(retained->geometry()).materials));
        require(retained->worldMaterialsValid(), "rebind without reimport/repreparation");
    }
    void occurrences(const std::filesystem::path& root)
    {
        GtsModelRegistry         registry;
        GtsModelRealizationCache cache;
        World                    world;
        auto&                    runtime = modelInstances(world, cache, nullptr);
        auto                     rest    = create(runtime, load(registry, rig(false), root / "rest"));
        require(rest->skeletonOccurrences().size() == 1 && !rest->skeletonOccurrences()[0].activeClip() &&
                    rest->palette(0)->matrices[0] == glm::mat4(1) && rest->palette(1)->matrices[0][3].x == 2,
                "default pose and binding-specific palettes exist without clips; no mesh-node translation");
        auto  mixedFixture = rig(false);
        auto  staticMesh   = at(field(mixedFixture.root, "meshes"), 0);
        auto& attrs        = field(at(field(staticMesh, "primitives"), 0), "attributes");
        erase(attrs, "JOINTS_0");
        erase(attrs, "WEIGHTS_0");
        std::get<Array>(field(mixedFixture.root, "meshes").value).push_back(staticMesh);
        std::get<Array>(field(mixedFixture.root, "nodes").value).push_back(parse(R"({"mesh":1})"));
        field(mixedFixture.root, "scenes") = parse(R"([{"nodes":[0,1,2,3]}])");
        auto mixed                         = create(runtime, load(registry, mixedFixture, root / "mixed"));
        require(mixed->geometry()->occurrences.size() == 3 && mixed->skeletonOccurrences().size() == 1 &&
                    mixed->paletteForOccurrence(0) && !mixed->paletteForOccurrence(2) &&
                    mixed->materialFor(2, 0).valid(),
                "mixed static/skinned views preserve profile-specific state");
        auto model = load(registry, rig(), root / "rig");
        auto a = create(runtime, model), b = create(runtime, model);
        auto reference = a->skeletonOccurrence(0);
        auto before    = reference.get();
        require(before && &before->skeleton() == model->skeletonUses()[0].skeleton.get(), "no skeleton copy");
        ok(a->play(0, clip(model, "motion")));
        ok(a->updateAnimations(.25));
        require(a->skeletonOccurrence(0).get() == before && reference.get() == before, "stable occurrence reference");
        require(a->skeletonOccurrences()[0].pose().modelTransforms !=
                        b->skeletonOccurrences()[0].pose().modelTransforms &&
                    a->palette(0)->matrices != b->palette(0)->matrices &&
                    b->skeletonOccurrences()[0].playback().timeSeconds == 0,
                "independent pose, palettes, playback");
        const auto time = before->playback().timeSeconds;
        ok(a->play(0, clip(model, "motion")));
        require(before->playback().timeSeconds == time, "same clip idempotent");
        ok(a->updateAnimations(1));
        require(before->playback().timeSeconds == time, "loop uses supplied delta");
        ok(a->play(0, clip(model, "other")));
        require(before->playback().timeSeconds == 0, "switch resets clock");
        ok(a->setPlaybackPolicy(0, 2, false));
        ok(a->updateAnimations(.75));
        require(before->playback().timeSeconds == 1, "speed and nonlooping endpoint");
        ok(a->stop(0));
        require(!before->activeClip() && before->playback().timeSeconds == 0 &&
                    a->palette(0)->matrices[0] == glm::mat4(1),
                "stop publishes coherent default pose and palettes");
        auto zeroFixture = rig(false);
        auto anim        = zeroFixture.addAnimation("zero");
        auto input       = zeroFixture.addAnimationTimes({0});
        auto output      = zeroFixture.addAccessor(floats({0, 0, 0}), "VEC3", 5126, 1);
        auto sampler     = zeroFixture.addAnimationSampler(anim, input, output);
        zeroFixture.addAnimationChannel(anim, sampler, 1, "translation");
        auto zeroModel = load(registry, zeroFixture, root / "zero");
        auto zero      = create(runtime, zeroModel);
        ok(zero->play(0, clip(zeroModel, "zero")));
        ok(zero->updateAnimations(123));
        require(zero->skeletonOccurrences()[0].playback().timeSeconds == 0, "zero-duration playback safe");
        auto foreign = load(registry, rig(), root / "foreign");
        require(!a->play(0, clip(foreign, "motion")).succeeded(), "foreign clip rejected despite compatible structure");
        require(!a->play(99, clip(model, "motion")).succeeded() && !a->setPlaybackPolicy(0, -1, true).succeeded(),
                "invalid occurrence/policy rejected");
        require(!a->updateAnimations(std::numeric_limits<double>::quiet_NaN()).succeeded(), "nonfinite delta rejected");
        const auto preservedPalette = a->palette(0)->matrices;
        resetMaterialRuntime(world);
        require(!a->worldMaterialsValid() && !a->materialFor(0, 0).valid() &&
                    a->palette(0)->matrices == preservedPalette,
                "world reset leaves pose/palette definitions intact");
        ok(a->rebindMaterials(modelMaterialRealization(world, nullptr).realize(a->geometry()).materials));
        ok(b->rebindMaterials(modelMaterialRealization(world, nullptr).realize(b->geometry()).materials));
        require(a->materials() == b->materials() && a->palette(0)->matrices == preservedPalette,
                "rebind replaces only world association");
        World otherWorld;
        auto  other = create(modelInstances(otherWorld, cache, nullptr), model);
        require(other->model() == model && other->geometry() == a->geometry() && other->materials() != a->materials(),
                "same definitions with separate world materials");
        require(model->nodes()[0].localTransform[3].x == 30 && model->clips()[0].tracks[0].timesSeconds[1] == 1,
                "authored hierarchy and clips unchanged");
        // ECS structural copies preserve the one entity-owned identity, then destruction invalidates references.
        struct Owner
        {
            std::shared_ptr<GtsModelInstance> instance;
        };
        struct Marker
        {
            int value = 1;
        };
        auto entity = world.createEntity();
        world.addComponent(entity, Owner{std::move(a)});
        world.addComponent(entity, Marker{});
        require(reference.get() == before, "ECS relocation preserves occurrence identity");
        world.destroyEntity(entity);
        require(!reference.get() && b->worldMaterialsValid(),
                "entity destruction expires pose reference only for its instance");
    }
    void multipleUsesAndFailure(const std::filesystem::path& root)
    {
        GtsModelRegistry         registry;
        GtsModelRealizationCache cache;
        World                    world;
        auto&                    runtime = modelInstances(world, cache, nullptr);
        auto                     f       = rig(false);
        field(f.root, "nodes")  = parse(R"([{"mesh":0,"skin":0},{},{"mesh":0,"skin":1},{"translation":[7,0,0]}])");
        field(f.root, "skins")  = parse(R"([{"joints":[1]},{"joints":[3]}])");
        field(f.root, "scenes") = parse(R"([{"nodes":[0,1,2,3]}])");
        animation(f, "first", 1);
        animation(f, "second", 3);
        auto model    = load(registry, f, root / "multiple");
        auto instance = create(runtime, model);
        require(instance->skeletonOccurrences().size() == 2, "explicit skeleton uses stay separate");
        auto first = clip(model, "first", 0), second = clip(model, "second", 1);
        require(!instance->play(1, first).succeeded(), "same-model incompatible clip rejected");
        auto prior = instance->palette(1)->matrices;
        ok(instance->play(0, first));
        ok(instance->updateAnimations(.25));
        require(instance->palette(1)->matrices == prior && !instance->skeletonOccurrences()[1].activeClip(),
                "targeted play/update leaves inactive occurrence intact");
        ok(instance->play(1, second));
        ok(instance->setPlaybackPolicy(1, std::numeric_limits<float>::max(), true));
        auto pose  = instance->skeletonOccurrences()[0].pose().modelTransforms;
        auto clock = instance->skeletonOccurrences()[0].playback().timeSeconds;
        // Occurrence 0 can finish; occurrence 1 overflows time. Neither may be published.
        require(!instance->updateAnimations(1e300).succeeded(), "late occurrence time overflow fails");
        require(instance->skeletonOccurrences()[0].pose().modelTransforms == pose &&
                    instance->skeletonOccurrences()[0].playback().timeSeconds == clock,
                "whole-instance update rollback");

        auto       overflow = rig(false);
        const auto inverse =
            overflow.addAccessor(floats({1e20f, 0, 0, 0, 0, 1e20f, 0, 0, 0, 0, 1e20f, 0, 0, 0, 0, 1}), "MAT4", 5126, 1);
        field(at(field(overflow.root, "skins"), 1), "inverseBindMatrices") = inverse;
        animation(overflow, "scale", 1, "scale", {1, 1, 1, 1e20f, 1e20f, 1e20f});
        auto source  = load(registry, overflow, root / "overflow");
        auto failing = create(runtime, source);
        ok(failing->play(0, clip(source, "scale")));
        ok(failing->setPlaybackPolicy(0, 1, false));
        auto palette0 = failing->palette(0)->matrices, palette1 = failing->palette(1)->matrices;
        auto failure = failing->updateAnimations(1);
        require(!failure.succeeded() && failure.diagnostics[0].message.find("binding 1") != std::string::npos,
                "late binding overflow retains slot/binding diagnostics");
        require(failing->palette(0)->matrices == palette0 && failing->palette(1)->matrices == palette1 &&
                    failing->skeletonOccurrences()[0].playback().timeSeconds == 0,
                "no partial pose/time/palette publication");
        field(at(field(overflow.root, "nodes"), 1), "scale") = parse("[1e20,1e20,1e20]");
        auto invalidDefault = load(registry, overflow, root / "default-overflow");
        auto rejected = runtime.create(invalidDefault);
        require(!rejected.succeeded() && !rejected.instance && !rejected.diagnostics.empty(),
                "default palette overflow publishes no partial instance");
    }
} // namespace
int main()
{
    static_assert(!std::is_copy_constructible_v<GtsModelInstance>);
    ScopedRuntimeAssetPolicy policy("development");
    auto                     root = std::filesystem::temp_directory_path() / "gravitas-model-instance-test";
    std::filesystem::create_directories(root);
    staticAndLifetime(root);
    occurrences(root);
    multipleUsesAndFailure(root);
    std::filesystem::remove_all(root);
}
