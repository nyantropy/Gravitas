#include "ECSWorld.hpp"
#include "MaterialRuntimeRegistry.h"
#include "GtsModelResource.h"
#include "GtsModelMaterialRealization.h"
#include "GtsModelRealizationCache.h"
#include "../assets/importers/gltf/GltfFixtureBuilder.h"
#include "../assets/runtime/ScopedRuntimeAssetPolicy.h"
#include "GtsModelInstanceRuntime.h"
#include "GtsModelRenderExtraction.h"
#include "ModelFrameExtraction.h"
#include "GtsModelRegistry.h"
#include "GtsModelAsset.h"
#include "AssetSerializers.h"
#include "ModelAssetSerializer.h"
#include <limits>

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
        auto result = registry.requestModel(fixture.write(path, GltfFixtureFormat::Glb));
        require(result.succeeded(), result.diagnostics().empty() ? "load" : result.diagnostics().front().message);
        return result.handle();
    }
    std::shared_ptr<GtsModelInstance> create(GtsModelInstanceRuntime& service, GtsModelHandle model)
    {
        auto result = service.create(model);
        require(result.succeeded(), result.diagnostics.empty() ? "create" : result.diagnostics.front().message);
        return std::move(result.instance);
    }
    GtsModelFrameData
    extract(std::shared_ptr<GtsModelInstance> instance, World& world, glm::mat4 transform = glm::mat4(1))
    {
        auto result = extractModelRenderState(instance, transform, materialRuntime(world));
        require(result.succeeded(), result.diagnostics.empty() ? "extract" : result.diagnostics.front().message);
        return std::move(result.frame);
    }
    void ok(GtsModelInstanceStatus status)
    {
        require(status.succeeded(), "animation/rebind status");
    }
    void materialOverrides(const std::filesystem::path& root)
    {
        GtsModelRegistry         registry;
        GtsModelRealizationCache cache;
        World                    world;
        auto&                    service = modelInstances(world, cache, nullptr);
        auto                     model   = load(registry, GltfFixtureBuilder{}, root / "overrides");
        auto                     a = create(service, model), b = create(service, model);
        auto&                    materials = materialRuntime(world);
        const auto               base      = a->materialFor(0, 0);
        const auto               shared    = a->materials();
        require(base == b->materialFor(0, 0), "instances initially use shared base materials");
        MaterialInstance red;
        red.baseColor = {1, 0, 0, 1};
        MaterialInstance blue;
        blue.baseColor        = {0, 0, 1, 1};
        const auto redHandle  = materials.createInstance(red);
        const auto blueHandle = materials.createInstance(blue);
        ok(a->setMaterialOverride(redHandle, materials.lifetimeToken()));
        require(b->materialFor(0, 0) == base && shared->materialFor(*a->geometry(), 0, 0) == base,
                "override does not mutate another instance or shared materials");
        ok(b->setMaterialOverride(blueHandle, materials.lifetimeToken()));
        auto redFrame  = extract(a, world);
        auto blueFrame = extract(b, world);
        require(redFrame.staticDraws[0].material.instance == redHandle &&
                    blueFrame.staticDraws[0].material.instance == blueHandle &&
                    redFrame.staticDraws[0].material.parameters.baseColor == red.baseColor &&
                    blueFrame.staticDraws[0].material.parameters.baseColor == blue.baseColor &&
                    a->geometry() == b->geometry() && a->materials() == b->materials(),
                "extraction reads independent instance overrides with shared definitions");
        require(!a->materialFor(999, 0).valid() && !a->materialFor(0, 999).valid(),
                "override cannot hide an invalid occurrence or primitive");
        World      foreign;
        const auto foreignHandle = materialRuntime(foreign).createInstance(red);
        require(!a->setMaterialOverride(foreignHandle, materialRuntime(foreign).lifetimeToken()).succeeded() &&
                    !a->setMaterialOverride({}, materials.lifetimeToken()).succeeded() &&
                    !a->setMaterialOverride(redHandle, {}).succeeded() && a->materialFor(0, 0) == redHandle,
                "foreign, empty and expired override requests fail without changing the instance");
        a->clearMaterialOverride();
        require(extract(a, world).staticDraws[0].material.instance == base &&
                    redFrame.staticDraws[0].material.instance == redHandle,
                "clearing restores base materials; previous frames remain captured");
        materials.destroyInstance(blueHandle);
        require(!b->worldMaterialsValid() && !extractModelRenderState(b, glm::mat4(1), materials).succeeded(),
                "destroyed override handle fails live validation");
        b->clearMaterialOverride();
        require(b->worldMaterialsValid() && b->materialFor(0, 0) == base, "clear recovers a destroyed override");
        ok(a->setMaterialOverride(redHandle, materials.lifetimeToken()));
        resetMaterialRuntime(world);
        require(!a->worldMaterialsValid(), "runtime reset invalidates overrides as well as base materials");
        ok(a->rebindMaterials(modelMaterialRealization(world, nullptr).realize(a->geometry()).materials));
        require(a->materialFor(0, 0) == a->materials()->materialFor(*a->geometry(), 0, 0),
                "rebinding to a new scope clears old overrides even if handle numbers are reused");
        extract(a, world);
    }

    // Both canonical slots and cooked references exercise the same public override contract.
    void verifySlotOverrides(GtsModelHandle model, GtsModelHandle foreignModel)
    {
        GtsModelRealizationCache cache;
        World                    world;
        auto&                    service = modelInstances(world, cache, nullptr);
        auto                     a = create(service, model), b = create(service, model);
        auto                     foreign    = create(service, foreignModel);
        const auto               primitives = a->geometry()->geometry[0].primitives();
        auto                     slot0      = a->materialSlot(primitives[0].material);
        auto                     slot1      = a->materialSlot(primitives[1].material);
        auto                     repeated   = a->materialSlot(primitives[2].material);
        require(
            slot0 && slot1 && repeated &&
                a->materialSlot(primitives[3].material).has_value() == (model->canonicalModel() != nullptr),
            "assigned slots are addressable; cooked unassigned is not a slot (glTF imports an explicit default slot)");
        const auto copied = primitives[0].material;
        require(!a->materialSlot(copied) && !a->materialSlot(GtsRealizedMaterial{uint32_t(999)}) &&
                    !a->materialSlot(foreign->geometry()->geometry[0].primitives()[0].material),
                "copied, fabricated and foreign associations cannot establish model slot ownership");
        auto&      runtime = materialRuntime(world);
        const auto A = a->materialFor(0, 0), B = a->materialFor(0, 1), fallback = a->materialFor(0, 3);
        require(A != B && B == a->materialFor(0, 2) &&
                    (model->canonicalModel() || fallback == runtime.defaultMaterial()),
                "distinct base slots, repeated slot and default material");
        const auto C     = runtime.createInstance(MaterialInstance{});
        const auto D     = runtime.createInstance(MaterialInstance{});
        auto       check = [&](MaterialInstanceHandle x, MaterialInstanceHandle y, MaterialInstanceHandle z)
        {
            require(a->materialFor(0, 0) == x && a->materialFor(0, 1) == y && a->materialFor(0, 2) == y &&
                        a->materialFor(0, 3) == z,
                    "per-slot then model-wide then base precedence");
            const auto frame = extract(a, world);
            require(frame.staticDraws.size() == 4 && frame.staticDraws[0].material.instance == x &&
                        frame.staticDraws[1].material.instance == y && frame.staticDraws[2].material.instance == y &&
                        frame.staticDraws[3].material.instance == z,
                    "extraction immediately observes logical override changes without rebuilding");
        };
        check(A, B, fallback);
        ok(a->setMaterialOverride(*slot1, C, runtime.lifetimeToken()));
        check(A, C, fallback);
        ok(a->setMaterialOverride(D, runtime.lifetimeToken()));
        check(D, C, D);
        a->clearMaterialOverride(*repeated);
        check(D, D, D);
        a->clearMaterialOverride();
        check(A, B, fallback);
        ok(a->setMaterialOverride(*slot0, D, runtime.lifetimeToken()));
        ok(a->setMaterialOverride(*slot1, C, runtime.lifetimeToken()));
        ok(a->setMaterialOverride(C, runtime.lifetimeToken()));
        check(D, C, C);
        a->clearMaterialOverride();
        check(D, C, fallback);
        ok(a->setMaterialOverride(*repeated, D, runtime.lifetimeToken()));
        check(D, D, fallback);
        a->clearMaterialOverride(*slot1);
        check(D, B, fallback);
        a->clearMaterialOverride(*slot1); // Idempotent.
        a->clearMaterialOverride(GtsModelMaterialSlot{});
        ok(b->setMaterialOverride(*slot0, C, runtime.lifetimeToken()));
        require(b->materialFor(0, 0) == C && b->materialFor(0, 1) == B && a->materials() == b->materials() &&
                    a->geometry() == b->geometry() && a->materials()->materialFor(*a->geometry(), 0, 0) == A &&
                    a->materials()->materialFor(*a->geometry(), 0, 1) == B,
                "instances have independent overrides with immutable shared base materials");
        auto       foreignSlot = foreign->materialSlot(foreign->geometry()->geometry[0].primitives()[0].material);
        World      foreignWorld;
        const auto foreignHandle = materialRuntime(foreignWorld).createInstance(MaterialInstance{});
        require(foreignSlot && !a->setMaterialOverride(*foreignSlot, C, runtime.lifetimeToken()).succeeded() &&
                    !a->setMaterialOverride(GtsModelMaterialSlot{}, C, runtime.lifetimeToken()).succeeded() &&
                    !a->setMaterialOverride(*slot0, {}, runtime.lifetimeToken()).succeeded() &&
                    !a->setMaterialOverride(*slot0, C, {}).succeeded() &&
                    !a->setMaterialOverride(*slot0, foreignHandle, materialRuntime(foreignWorld).lifetimeToken())
                         .succeeded(),
                "assignment rejects invalid slots, handles, expired tokens and foreign runtime scopes");
        a->clearMaterialOverride(*foreignSlot);
        check(D, B, fallback); // Rejected operations leave existing state unchanged.
        ok(a->setMaterialOverride(C, runtime.lifetimeToken()));
        a->clearMaterialOverrides();
        a->clearMaterialOverrides();
        check(A, B, fallback);
        ok(a->setMaterialOverride(*slot1, C, runtime.lifetimeToken()));
        runtime.destroyInstance(C);
        require(!a->worldMaterialsValid() && !a->materialFor(0, 1).valid() &&
                    !extractModelRenderState(a, glm::mat4(1), runtime).succeeded(),
                "destroyed slot replacement is detected before extraction");
        a->clearMaterialOverride(*slot1);
        check(A, B, fallback);
        ok(a->setMaterialOverride(*slot1, D, runtime.lifetimeToken()));
        const auto oldLifetime = runtime.lifetimeToken();
        resetMaterialRuntime(world);
        require(oldLifetime.expired() && !a->worldMaterialsValid() && !a->materialFor(0, 1).valid(),
                "runtime reset expires slot override ownership without touching model geometry");
        ok(a->rebindMaterials(modelMaterialRealization(world, nullptr).realize(a->geometry()).materials));
        require(!a->setMaterialOverride(*slot1, D, oldLifetime).succeeded() &&
                    a->materialFor(0, 1) == a->materials()->materialFor(*a->geometry(), 0, 1),
                "new world scope clears overrides; expired tokens cannot revive reused handle values");
        extract(a, world);
    }

    void logicalMaterialOverrides(const std::filesystem::path& root)
    {
        GtsModelRegistry   registry;
        GltfFixtureBuilder fixture;
        field(fixture.root, "materials") = parse(R"([{"name":"A"},{"name":"B"}])");
        auto  primitive                  = fixture.primitive();
        auto& primitives = std::get<Array>(field(at(field(fixture.root, "meshes"), 0), "primitives").value);
        primitives.clear();
        for (uint32_t slot : {0u, 1u, 1u})
        {
            auto assigned               = primitive;
            field(assigned, "material") = slot;
            primitives.push_back(assigned);
        }
        primitives.push_back(primitive);
        verifySlotOverrides(load(registry, fixture, root / "slots"), load(registry, fixture, root / "foreign-slots"));

        MeshAssetData mesh;
        mesh.vertices.resize(3);
        mesh.vertices[1].pos.x = 1;
        mesh.vertices[2].pos.y = 1;
        mesh.indices           = {0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2};
        mesh.submeshes         = {{0, 3, AssetReference::fromLogicalPath("a.gmat"), {}},
                                  {3, 3, AssetReference::fromLogicalPath("b.gmat"), {}},
                                  {6, 3, AssetReference::fromLogicalPath("b.gmat"), {}},
                                  {9, 3, {}, {}}};
        std::string       error;
        MaterialAssetData material;
        material.shaderFamily = MaterialShaderFamily::StandardSurface;
        require(MaterialAssetSerializer::writeFile(material, root / "a.gmat", &error), error);
        require(MaterialAssetSerializer::writeFile(material, root / "b.gmat", &error), error);
        require(MeshAssetSerializer::writeFile(mesh, root / "slots.gmesh", &error), error);
        require(MeshAssetSerializer::writeFile(mesh, root / "foreign-slots.gmesh", &error), error);
        auto cooked  = registry.requestModel(root / "slots.gmesh");
        auto foreign = registry.requestModel(root / "foreign-slots.gmesh");
        require(cooked.succeeded() && foreign.succeeded(), "load cooked override fixtures");
        verifySlotOverrides(cooked.handle(), foreign.handle());
    }

    void staticHierarchy(const std::filesystem::path& root)
    {
        GtsModelRegistry         registry;
        GtsModelRealizationCache cache;
        World                    world;
        auto&                    service = modelInstances(world, cache, nullptr);
        GltfFixtureBuilder       fixture;
        field(fixture.root, "nodes")  = parse(R"([{"mesh":0,"translation":[1,0,0],"children":[1]},
            {"mesh":0,"translation":[0,2,0]}, {"mesh":0,"translation":[0,0,3]}])");
        field(fixture.root, "scenes") = parse(R"([{"nodes":[0,2]}])");
        auto model                    = load(registry, fixture, root / "static");
        auto a = create(service, model), b = create(service, model);
        auto placement = glm::translate(glm::mat4(1), glm::vec3(10, 20, 30));
        auto frame     = extract(a, world, placement);
        require(frame.staticDraws.size() == 3 && frame.skinnedDraws.empty() && a->skeletonOccurrences().empty(),
                "static profile creates no skeletal/frame palette state");
        require(frame.staticDraws[0].worldFromGeometry[3] == glm::vec4(11, 20, 30, 1) &&
                    frame.staticDraws[1].worldFromGeometry[3] == glm::vec4(11, 22, 30, 1) &&
                    frame.staticDraws[2].worldFromGeometry[3] == glm::vec4(10, 20, 33, 1),
                "hierarchy/world composition, multiple roots");
        auto shared = frame.staticDraws[0].geometry;
        for (const auto& draw : frame.staticDraws)
        {
            require(draw.geometry == shared && draw.geometry.get() == &a->geometry()->geometry[0],
                    "one definition under three nodes");
            require(draw.geometry->staticVertices().data() == a->geometry()->geometry[0].staticVertices().data() &&
                        draw.geometry->indices().data() == a->geometry()->geometry[0].indices().data(),
                    "no array copies");
            require(draw.material.instance == a->materialFor(draw.occurrenceIndex, draw.primitiveIndex),
                    "live material association");
        }
        require(extract(b, world).staticDraws[0].geometry == shared, "resource identity independent of entity");
        require(model->nodes()[1].localTransform[3] == glm::vec4(0, 2, 0, 1), "hierarchy remains immutable");
        const auto entity = world.createEntity();
        world.addComponent(entity, ModelInstanceComponent{a});
        WorldTransformComponent transform;
        transform.matrix = placement;
        world.addComponent(entity, transform);
        auto ecsFrame                                                   = extractModelFrame(world, 7, nullptr);
        world.getComponent<WorldTransformComponent>(entity).matrix[3].x = 50;
        auto moved                                                      = extractModelFrame(world, 7, nullptr);
        require(ecsFrame.cameraViewID == 7 && ecsFrame.staticDraws.size() == 3 &&
                    moved.staticDraws[0].worldFromGeometry[3].x == 51 &&
                    ecsFrame.staticDraws[0].worldFromGeometry[3].x == 11,
                "generic ECS discovery and transform frame isolation without presentation refresh");
        auto before = a->geometry();
        resetMaterialRuntime(world);
        auto expired = extractModelRenderState(a, placement, materialRuntime(world));
        require(!expired.succeeded() && expired.frame.staticDraws.empty() && !expired.diagnostics[0].location.empty(),
                "expired scope fails atomically");
        ok(a->rebindMaterials(modelMaterialRealization(world, nullptr).realize(a->geometry()).materials));
        auto newMaterial  = a->materialFor(0, 0);
        auto updated      = *materialRuntime(world).getInstance(newMaterial);
        updated.baseColor = {0.2f, 0.3f, 0.4f, 1};
        materialRuntime(world).setInstance(newMaterial, updated);
        auto rebound = extract(a, world);
        require(rebound.staticDraws[0].material.parameters.baseColor == updated.baseColor && a->geometry() == before,
                "material reset/rebind uses live state without rebuilding geometry/presentation");
        require(frame.staticDraws[0].material.parameters.baseColor != updated.baseColor, "frame material isolation");
        World otherWorld;
        require(!extractModelRenderState(a, placement, materialRuntime(otherWorld)).succeeded(),
                "foreign material scope fails");
        require(!extractModelRenderState({}, placement, materialRuntime(world)).succeeded(), "invalid instance fails");
        auto invalidTransform  = placement;
        invalidTransform[0][0] = std::numeric_limits<float>::infinity();
        auto rejected          = extractModelRenderState(a, invalidTransform, materialRuntime(world));
        require(!rejected.succeeded() && rejected.frame.staticDraws.empty() &&
                    rejected.diagnostics.front().location.find("node") != std::string::npos,
                "invalid placement rejects the complete extraction with occurrence context");
        // Already-prepared cooked input goes through the identical extraction contract.
        MeshAssetData mesh;
        mesh.vertices.resize(3);
        mesh.vertices[1].pos.x = 1;
        mesh.vertices[2].pos.y = 1;
        mesh.indices           = {0, 1, 2};
        mesh.submeshes         = {{0, 3, {}, {}}};
        std::string error;
        require(MeshAssetSerializer::writeFile(mesh, root / "mesh.gmesh", &error), error);
        auto cooked = registry.requestModel(root / "mesh.gmesh");
        require(cooked.succeeded(), "cooked request");
        auto ci = create(service, cooked.handle());
        auto cf = extract(ci, world);
        require(cf.staticDraws.size() == 1 && !ci->model()->canonicalModel() &&
                    cf.staticDraws[0].geometry->staticVertices().data() ==
                        ci->geometry()->geometry[0].staticVertices().data(),
                "prepared cooked extraction retains original arrays");
    }
    void mixedAndPalettes(const std::filesystem::path& root)
    {
        GtsModelRegistry         registry;
        GtsModelRealizationCache cache;
        World                    world;
        auto&                    service = modelInstances(world, cache, nullptr);
        GltfFixtureBuilder       f;
        f.addVertexStream("JOINTS_0", std::vector<uint8_t>(12, 0), "VEC4", 5121);
        f.addVertexStream("WEIGHTS_0", floats({1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}), "VEC4", 5126);
        field(f.root, "materials")       = parse(R"([{"name":"A"},{"name":"B"}])");
        field(f.primitive(), "material") = 0u;
        auto primitive                   = at(field(at(field(f.root, "meshes"), 0), "primitives"), 0);
        field(primitive, "material")     = 1u;
        std::get<Array>(field(at(field(f.root, "meshes"), 0), "primitives").value).push_back(primitive);
        auto staticMesh = at(field(f.root, "meshes"), 0);
        for (auto& p : std::get<Array>(field(staticMesh, "primitives").value))
        {
            erase(field(p, "attributes"), "JOINTS_0");
            erase(field(p, "attributes"), "WEIGHTS_0");
        }
        std::get<Array>(field(f.root, "meshes").value).push_back(staticMesh);
        field(f.root, "nodes")  = parse(R"([{"mesh":0,"skin":0,"translation":[30,0,0]}, {},
            {"mesh":0,"skin":0}, {"mesh":1,"translation":[0,3,0]}, {"mesh":0,"skin":1}])");
        field(f.root, "skins")  = parse(R"([{"joints":[1]},{"joints":[1]}])");
        field(f.root, "scenes") = parse(R"([{"nodes":[0,1,2,3,4]}])");
        auto ib = f.addAccessor(floats({1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 2, 0, 0, 1}), "MAT4", 5126, 1);
        field(at(field(f.root, "skins"), 1), "inverseBindMatrices") = ib;
        auto anim                                                   = f.addAnimation("motion");
        auto times                                                  = f.addAnimationTimes({0, 1});
        auto values = f.addAccessor(floats({0, 0, 0, 0, 2, 0}), "VEC3", 5126, 2);
        f.addAnimationChannel(anim, f.addAnimationSampler(anim, times, values), 1, "translation");
        auto model = load(registry, f, root / "mixed");
        auto a = create(service, model), b = create(service, model);
        auto frame = extract(a, world);
        require(frame.staticDraws.size() == 2 && frame.skinnedDraws.size() == 6,
                "one traversal dispatches mixed profiles and primitives");
        const auto& prepared = std::get<std::vector<glm::mat4>>(a->geometry()->authoredNodeTransforms);
        require(prepared.size() == model->nodes().size() && prepared[0][3].x == 30,
                "preparation includes authored skinned nodes although their transforms are not applied to draws");
        const auto& d = frame.skinnedDraws;
        require(d[0].geometry == d[2].geometry && d[0].geometry != d[4].geometry,
                "same binding shares geometry, different binding context distinct");
        require(d[0].palette == d[1].palette && d[0].palette == d[2].palette && d[0].palette != d[4].palette,
                "one frame snapshot per binding across primitives and nodes");
        require(d[0].palette->matrices != d[4].palette->matrices, "binding palettes remain distinct");
        require(d[0].worldFromGeometry == glm::mat4(1), "authored skinned mesh transform is not applied twice");
        require(d[0].geometry->skinnedVertices().data() == a->geometry()->geometry[0].skinnedVertices().data(),
                "skinned bytes never copied");
        require(d[0].geometry->primitives()[d[0].primitiveIndex].firstIndex == 0 &&
                    d[1].geometry->primitives()[d[1].primitiveIndex].firstIndex == 3,
                "primitive ranges preserved");
        auto       slot        = a->materialSlot(d[1].geometry->primitives()[1].material);
        auto&      runtime     = materialRuntime(world);
        const auto replacement = runtime.createInstance(MaterialInstance{});
        require(slot.has_value(), "skinned material logical slot");
        ok(a->setMaterialOverride(*slot, replacement, runtime.lifetimeToken()));
        const auto overridden = extract(a, world);
        require(overridden.staticDraws[1].material.instance == replacement &&
                    overridden.staticDraws[0].material.instance == frame.staticDraws[0].material.instance,
                "slot selection is independent of static/skinned profile");
        for (const auto& draw : overridden.skinnedDraws)
            require(draw.material.instance == (draw.primitiveIndex == 1 ? replacement : d[0].material.instance),
                    "one logical slot override reaches all skinned bindings and nodes");
        a->clearMaterialOverrides();
        const auto matrices = a->palette(0)->matrices;
        auto       moved    = extract(a, world, glm::translate(glm::mat4(1), glm::vec3(10, 20, 30)));
        require(a->palette(0)->matrices == matrices &&
                    moved.skinnedDraws[0].worldFromGeometry[3] == glm::vec4(10, 20, 30, 1),
                "world placement separate from skin deformation");
        ok(a->play(0, *model->findClip("motion", 0).reference));
        ok(a->updateAnimations(.5));
        auto animated = extract(a, world);
        require(animated.skinnedDraws[0].palette->matrices != d[0].palette->matrices &&
                    d[0].palette->matrices == matrices,
                "next frame sees current pose; old frame retains immutable matrix bytes");
        auto independent = extract(b, world);
        require(independent.skinnedDraws[0].geometry == d[0].geometry &&
                    independent.skinnedDraws[0].palette != d[0].palette &&
                    independent.skinnedDraws[0].palette->matrices == matrices,
                "independent instance deformation, shared geometry");
        require(a->skeletonOccurrences()[0].playback().timeSeconds == .5, "extraction never advances playback");
        auto retained = animated.skinnedDraws[0].geometry;
        a.reset();
        require(extract(b, world).skinnedDraws[0].geometry == retained,
                "destroying one instance preserves shared geometry");
    }
    void preparedHierarchy(const std::filesystem::path& root)
    {
        GtsModelRegistry         registry;
        GtsModelRealizationCache cache;
        GltfFixtureBuilder       fixture;
        field(fixture.root, "nodes")  = parse(R"([
            {"translation":[4,0,0],"scale":[2,3,4],"children":[1]},
            {"mesh":0,"translation":[1,2,3],"rotation":[0,0,1,0],"children":[3]},
            {"mesh":0}, {"translation":[0,5,0]}])");
        field(fixture.root, "scenes") = parse(R"([{"nodes":[0,2]}])");
        auto      canonical           = load(registry, fixture, root / "prepared-hierarchy");
        glm::mat4 parent(1), child(1), helper(1);
        parent[0][0] = 2;
        parent[1][1] = 3;
        parent[2][2] = 4;
        parent[3].x  = 4;
        child[0][0]  = -1;
        child[1][1]  = -1;
        child[3]     = {1, 2, 3, 1};
        helper[3].y  = 5;
        const std::vector<glm::mat4> expected{parent, parent * child, glm::mat4(1), (parent * child) * helper};

        MeshAssetData mesh;
        mesh.vertices.resize(3);
        mesh.vertices[1].pos.x = 1;
        mesh.vertices[2].pos.y = 1;
        mesh.indices           = {0, 1, 2};
        mesh.submeshes         = {{0, 3, {}, {}}};
        std::string error;
        require(MeshAssetSerializer::writeFile(mesh, root / "prepared-hierarchy.gmesh", &error), error);
        ModelAssetData package;
        package.meshes = {AssetReference::fromLogicalPath("prepared-hierarchy.gmesh")};
        package.nodes.resize(4);
        const int parents[] = {-1, 0, -1, 1};
        for (size_t i = 0; i < 4; ++i)
        {
            package.nodes[i].parentIndex    = parents[i];
            package.nodes[i].localTransform = canonical->nodes()[i].localTransform;
            if (i == 1 || i == 2)
                package.nodes[i].mesh = package.meshes[0];
        }
        require(ModelAssetSerializer::writeFile(package, root / "prepared-hierarchy.gmodel", &error), error);
        auto cooked = registry.requestModel(root / "prepared-hierarchy.gmodel");
        require(cooked.succeeded(), "cooked hierarchy loads");
        for (const auto& model : {canonical, cooked.handle()})
        {
            World       first, second;
            auto        a        = create(modelInstances(first, cache, nullptr), model);
            auto        b        = create(modelInstances(first, cache, nullptr), model);
            auto        c        = create(modelInstances(second, cache, nullptr), model);
            const auto& matrices = std::get<std::vector<glm::mat4>>(a->geometry()->authoredNodeTransforms);
            require(matrices == expected, "exact prepared matrices for roots, helpers and noncommuting locals");
            require(a->geometry() == b->geometry() && a->geometry() == c->geometry() &&
                        matrices.data() ==
                            std::get<std::vector<glm::mat4>>(c->geometry()->authoredNodeTransforms).data(),
                    "instances in simultaneous worlds share one immutable prepared array");
            auto placement = glm::translate(glm::mat4(1), glm::vec3(7, 8, 9));
            auto frame     = extract(a, first, placement);
            require(frame.staticDraws.size() == 2 && frame.staticDraws[0].occurrenceIndex == 0 &&
                        frame.staticDraws[1].occurrenceIndex == 1 && frame.staticDraws[0].primitiveIndex == 0 &&
                        frame.staticDraws[0].geometry == frame.staticDraws[1].geometry &&
                        frame.staticDraws[0].worldFromGeometry == placement * expected[1] &&
                        frame.staticDraws[1].worldFromGeometry == placement * expected[2],
                    "canonical/cooked final matrices, order and repeated geometry unchanged");
            require(frame.staticDraws[0].occurrenceOwner == a &&
                        frame.staticDraws[0].material.instance == a->materialFor(0, 0),
                    "draw owners and materials unchanged");
            first.clear();
            require(!a->worldMaterialsValid() && c->worldMaterialsValid() &&
                        std::get<std::vector<glm::mat4>>(a->geometry()->authoredNodeTransforms) == expected,
                    "clear expires only world materials, not shared prepared state");
            extract(c, second);
            ok(a->rebindMaterials(modelMaterialRealization(first, nullptr).realize(a->geometry()).materials));
            require(extract(a, first, placement).staticDraws[0].worldFromGeometry ==
                        frame.staticDraws[0].worldFromGeometry,
                    "rebind retains prepared matrices and old frame values");
        }

        GtsModelFrameData                     retainedFrame;
        std::weak_ptr<const GtsRealizedModel> retained;
        const glm::mat4*                      preparedAddress = nullptr;
        {
            GtsModelRegistry         temporaryRegistry;
            GtsModelRealizationCache temporaryCache;
            World                    temporaryWorld;
            auto                     model = load(temporaryRegistry, GltfFixtureBuilder{}, root / "retained-prepared");
            auto                     instance = create(modelInstances(temporaryWorld, temporaryCache, nullptr), model);
            retained                          = instance->geometry();
            preparedAddress = std::get<std::vector<glm::mat4>>(instance->geometry()->authoredNodeTransforms).data();
            retainedFrame   = extract(instance, temporaryWorld);
        }
        require(!retained.expired() &&
                    std::get<std::vector<glm::mat4>>(retained.lock()->authoredNodeTransforms).data() ==
                        preparedAddress &&
                    *preparedAddress == glm::mat4(1) && retainedFrame.staticDraws[0].geometry->indices().size() == 3,
                "frame owners retain prepared and geometry storage beyond registry/cache/world destruction");
        retainedFrame = {};
        require(retained.expired(), "prepared data has no ownership cycle");

        GtsModelRegistry replacementRegistry;
        field(fixture.root, "nodes")  = parse(R"([{"mesh":0,"translation":[99,0,0]}])");
        field(fixture.root, "scenes") = parse(R"([{"nodes":[0]}])");
        auto replacement              = load(replacementRegistry, fixture, root / "prepared-hierarchy");
        auto oldGeometry              = cache.realize(canonical).model();
        auto newGeometry              = cache.realize(replacement).model();
        require(replacement->identityPath() == canonical->identityPath() && replacement != canonical &&
                    oldGeometry != newGeometry &&
                    std::get<std::vector<glm::mat4>>(oldGeometry->authoredNodeTransforms) == expected &&
                    std::get<std::vector<glm::mat4>>(newGeometry->authoredNodeTransforms)[0][3].x == 99,
                "same-path replacement uses independent resource identity and preparation");
    }

    void preparedFailureAndLifetime(const std::filesystem::path& root)
    {
        GtsModelRegistry         registry;
        GtsModelRealizationCache cache;
        GltfFixtureBuilder       fixture;
        field(fixture.root, "nodes")  = parse(R"([{"mesh":0},
            {"scale":[1e30,1,1],"children":[2]}, {"scale":[1e30,1,1]}])");
        field(fixture.root, "scenes") = parse(R"([{"nodes":[0,1]}])");
        auto model                    = load(registry, fixture, root / "overflow-prepared");
        auto realized                 = cache.realize(model);
        require(realized.succeeded(), "composed overflow must not become realization failure");
        require(std::get<std::string>(realized.model()->authoredNodeTransforms) ==
                    "Non-finite model transform at node 2",
                "non-rendering helper overflow retained as immutable failure");
        World world, other;
        auto  instance = create(modelInstances(world, cache, nullptr), model);
        auto  check    = [&](gts::rendering::MaterialRuntime& runtime, const std::string& message)
        {
            auto failed = extractModelRenderState(instance, glm::mat4(1), runtime);
            require(!failed.succeeded() && failed.frame.staticDraws.empty() && failed.frame.skinnedDraws.empty() &&
                        failed.diagnostics.size() == 1 && failed.diagnostics[0].code == "model.render.extraction" &&
                        failed.diagnostics[0].message == message &&
                        failed.diagnostics[0].location == model->identityPath().string(),
                    "deferred failure keeps extraction stage, atomic output, code, message and model context");
        };
        check(materialRuntime(world), "Non-finite model transform at node 2");
        check(materialRuntime(other), "Material runtime belongs to another world");
        auto& runtime     = materialRuntime(world);
        auto  replacement = runtime.createInstance(MaterialInstance{});
        ok(instance->setMaterialOverride(replacement, runtime.lifetimeToken()));
        runtime.destroyInstance(replacement);
        const std::string invalid =
            "Invalid or expired instance materials; clear a destroyed override or rebind after runtime reset";
        check(runtime, invalid);
        instance->clearMaterialOverride();
        check(runtime, "Non-finite model transform at node 2");
        auto base = instance->materialFor(0, 0);
        require(runtime.destroyInstance(base), "canonical base material can be destroyed");
        check(runtime, invalid);
        resetMaterialRuntime(world);
        check(materialRuntime(world), invalid);
        ok(instance->rebindMaterials(modelMaterialRealization(world, nullptr).realize(instance->geometry()).materials));
        check(materialRuntime(world), "Non-finite model transform at node 2");

        auto invalidPlacement  = glm::mat4(1);
        invalidPlacement[0][0] = std::numeric_limits<float>::infinity();
        auto firstError        = extractModelRenderState(instance, invalidPlacement, materialRuntime(world));
        require(firstError.diagnostics[0].message == "Non-finite model transform at node 2" &&
                    firstError.diagnostics[0].location == model->identityPath().string(),
                "hierarchy failure still precedes final placement validation");
        ModelAssetData package;
        package.meshes = {AssetReference::fromLogicalPath("prepared-hierarchy.gmesh")};
        package.nodes.resize(3);
        package.nodes[0].mesh                 = package.meshes[0];
        package.nodes[1].localTransform[0][0] = 1e30f;
        package.nodes[2].localTransform[0][0] = 1e30f;
        package.nodes[2].parentIndex          = 1;
        std::string error;
        require(ModelAssetSerializer::writeFile(package, root / "overflow.gmodel", &error), error);
        auto cooked = registry.requestModel(root / "overflow.gmodel");
        require(cooked.succeeded(), "finite cooked locals load before composed overflow");
        auto cookedInstance = create(modelInstances(world), cooked.handle());
        auto cookedFailure  = extractModelRenderState(cookedInstance, glm::mat4(1), materialRuntime(world));
        require(!cookedFailure.succeeded() && cookedFailure.frame.staticDraws.empty() &&
                    cookedFailure.diagnostics[0].code == "model.render.extraction" &&
                    cookedFailure.diagnostics[0].message == "Non-finite model transform at node 2" &&
                    cookedFailure.diagnostics[0].location == cooked.handle()->identityPath().string(),
                "cooked non-rendering overflow also remains an atomic extraction error");

        // Reconstruct the world at the same address while frames and instances retain the model.
        auto                              validModel = load(registry, GltfFixtureBuilder{}, root / "recreated-world");
        alignas(ECSWorld) std::byte       storage[sizeof(ECSWorld)];
        std::shared_ptr<GtsModelInstance> previous;
        for (int cycle = 0; cycle < 3; ++cycle)
        {
            auto* current = std::construct_at(reinterpret_cast<ECSWorld*>(storage));
            auto  next    = create(modelInstances(*current, cache, nullptr), validModel);
            if (previous)
            {
                require(previous->geometry() == next->geometry() && !previous->worldMaterialsValid() &&
                            !extractModelRenderState(previous, glm::mat4(1), materialRuntime(*current)).succeeded(),
                        "address reuse shares immutable model but never stale world materials");
            }
            auto frame = extractModelRenderState(next, glm::mat4(1), materialRuntime(*current));
            require(frame.succeeded(), "replacement world extracts successfully");
            std::destroy_at(current);
            require(!next->worldMaterialsValid() && frame.frame.staticDraws[0].worldFromGeometry == glm::mat4(1),
                    "destroyed world expires scope while retained frame remains valid");
            previous = std::move(next);
        }
    }
} // namespace
int main()
{
    ScopedRuntimeAssetPolicy policy("development");
    auto                     root = std::filesystem::temp_directory_path() / "gravitas-model-extraction-test";
    std::filesystem::create_directories(root);
    preparedHierarchy(root);
    preparedFailureAndLifetime(root);
    materialOverrides(root);
    logicalMaterialOverrides(root);
    staticHierarchy(root);
    mixedAndPalettes(root);
    std::filesystem::remove_all(root);
}
