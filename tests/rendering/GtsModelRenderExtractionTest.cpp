#include "../assets/importers/gltf/GltfFixtureBuilder.h"
#include "../assets/runtime/ScopedRuntimeAssetPolicy.h"
#include "rendering/core/model/GtsModelInstanceRuntime.h"
#include "rendering/core/model/GtsModelRenderExtraction.h"
#include "rendering/ecssetup/extraction/ModelFrameExtraction.h"
#include "assets/loading/model/GtsModelRegistry.h"
#include "assets/model/GtsModelAsset.h"
#include "assets/serialization/AssetSerializers.h"
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
        auto primitive = at(field(at(field(f.root, "meshes"), 0), "primitives"), 0);
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
} // namespace
int main()
{
    ScopedRuntimeAssetPolicy policy("development");
    auto                     root = std::filesystem::temp_directory_path() / "gravitas-model-extraction-test";
    std::filesystem::create_directories(root);
    staticHierarchy(root);
    mixedAndPalettes(root);
    std::filesystem::remove_all(root);
}
