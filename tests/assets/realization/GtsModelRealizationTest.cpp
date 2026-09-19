#include "model/serialization/ModelAssetSerializer.h"
#include "../importers/gltf/GltfFixtureBuilder.h"
#include "../runtime/ScopedRuntimeAssetPolicy.h"
#include "model/loading/GtsModelRegistry.h"
#include "model/loading/GtsModelResource.h"
#include "model/loading/GtsPreparedModelDefinition.h"
#include "model/realization/GtsModelRealizationCache.h"
#include "assets/serialization/AssetSerializers.h"
#include <cstring>

using namespace gts::rendering;

namespace
{
    GtsModelHandle load(GtsModelRegistry& registry, const std::filesystem::path& path)
    {
        auto        result = registry.requestModel(path);
        std::string errors;
        for (const auto& d : result.diagnostics())
            errors += d.code + ": " + d.message + " at " + d.location + "\n";
        require(result.succeeded(), errors);
        return result.handle();
    }

    GtsModelHandle
    loadFixture(GtsModelRegistry& registry, GltfFixtureBuilder fixture, const std::filesystem::path& directory)
    {
        std::filesystem::create_directories(directory);
        return load(registry, fixture.write(directory, GltfFixtureFormat::Glb));
    }

    void staticModels(const std::filesystem::path& directory)
    {
        GtsModelRegistry registry;
        const auto       obj = directory / "source.obj";
        std::ofstream(obj) << "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
        auto source = load(registry, obj);
        auto result = realizeGtsModel(source);
        require(result.succeeded(), "OBJ through complete model realization");
        const auto& geometry = result.model()->geometry[0];
        require(geometry.profile() == GtsGeometryProfile::Static && geometry.staticVertices().size() == 3 &&
                    geometry.indices().size() == 3 && geometry.skinnedVertices().empty(),
                "Typed static profile");
        require(geometry.metadata().generatedNormals && geometry.bounds().valid, "Preparation and bounds");
        require(result.model()->occurrences[0].modelNodeIndex == 0 && !result.model()->occurrences[0].skinBindingIndex,
                "Static occurrence");

        GltfFixtureBuilder fixture;
        fixture.addTexCoordStream();
        fixture.addTexCoordStream("TEXCOORD_1"); // Existing profile warning must survive cache hits.
        field(fixture.root, "materials")       = parse(R"([{"name":"first"},{"name":"second"}])");
        field(fixture.primitive(), "material") = 1u;
        auto second                            = fixture.primitive();
        field(second, "material")              = 0u;
        std::get<Array>(field(at(field(fixture.root, "meshes"), 0), "primitives").value).push_back(second);
        field(fixture.root, "nodes")    = parse(R"([{"name":"parent","children":[1],"translation":[4,0,0]},
            {"name":"child","mesh":0,"translation":[0,2,0]},{"name":"other root","mesh":0}])");
        field(fixture.root, "scenes")   = parse(R"([{"nodes":[0,2]}])");
        source                          = loadFixture(registry, fixture, directory / "static");
        const auto               before = source->canonicalModel()->meshes[0].primitives[0].attributes;
        GtsModelRealizationCache cache;
        result = cache.realize(source);
        require(result.succeeded() && result.model()->geometry.size() == 1 && result.model()->occurrences.size() == 2,
                "Shared mesh prepares once");
        const auto& model = *result.model();
        require(model.occurrences[0].modelNodeIndex == 1 && model.occurrences[1].modelNodeIndex == 2 &&
                    model.occurrences[0].geometryIndex == model.occurrences[1].geometryIndex,
                "Node order and sharing");
        require(model.model == source && model.model->rootNodes().size() == 2 &&
                    model.model->nodes()[0].localTransform[3].x == 4 &&
                    model.model->nodes()[1].localTransform[3].y == 2,
                "Exact original hierarchy retained by handle");
        require(model.geometry[0].staticVertices()[0].pos == glm::vec3(0), "Node transforms never baked");
        require(model.geometry[0].metadata().generatedTangents, "Existing tangent generation reused");
        const auto ranges = model.geometry[0].primitives();
        require(ranges.size() == 2 && ranges[0].firstIndex == 0 && ranges[1].firstIndex == 3 &&
                    std::get<uint32_t>(ranges[0].material) == 1 && std::get<uint32_t>(ranges[1].material) == 0,
                "Primitive order and canonical material slots unchanged");
        require(!result.diagnostics().empty(), "Preparation warnings retained");
        auto again = cache.realize(source);
        require(again.model() == result.model() && cache.size() == 1 &&
                    again.diagnostics()[0].location == result.diagnostics()[0].location,
                "Identity cache retains warnings");
        auto uncached = realizeGtsModel(source);
        require(uncached.model()->geometry[0].staticMesh()->vertices == model.geometry[0].staticMesh()->vertices &&
                    uncached.model()->occurrences[1].geometryIndex == 0,
                "Deterministic independent realization");
        require(source->canonicalModel()->meshes[0].primitives[0].attributes.size() == before.size() &&
                    !source->canonicalModel()->meshes[0].primitives[0].attributes.empty(),
                "Canonical streams retained");
        for (size_t i = 0; i < before.size(); ++i)
        {
            const auto& after = source->canonicalModel()->meshes[0].primitives[0].attributes[i];
            require(after.semantic == before[i].semantic && after.setIndex == before[i].setIndex &&
                        after.values == before[i].values,
                    "Canonical attribute values remain unchanged");
        }
        auto separate       = loadFixture(registry, fixture, directory / "static-separate-identity");
        auto separateResult = cache.realize(separate);
        require(separateResult.succeeded() && separateResult.model() != result.model() && cache.size() == 2,
                "Structural equality is not resource identity");
        require(!cache.realize({}).succeeded() && cache.size() == 2, "Invalid handles never cached");
    }

    GltfFixtureBuilder rigged()
    {
        GltfFixtureBuilder fixture;
        fixture.addVertexStream("JOINTS_0", std::vector<uint8_t>{0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3}, "VEC4", 5121);
        fixture.addVertexStream(
            "WEIGHTS_0", floats({.1f, .15f, .2f, .25f, .1f, .15f, .2f, .25f, .1f, .15f, .2f, .25f}), "VEC4", 5126);
        fixture.addVertexStream("JOINTS_1", std::vector<uint8_t>{4, 0, 0, 0, 4, 0, 0, 0, 4, 0, 0, 0}, "VEC4", 5121);
        fixture.addVertexStream("WEIGHTS_1", floats({.3f, 0, 0, 0, .3f, 0, 0, 0, .3f, 0, 0, 0}), "VEC4", 5126);
        // An unweighted attachment exercises mixed profiles without weakening static preparation.
        auto  staticMesh = at(field(fixture.root, "meshes"), 0);
        auto& attrs      = field(at(field(staticMesh, "primitives"), 0), "attributes");
        erase(attrs, "JOINTS_0");
        erase(attrs, "WEIGHTS_0");
        erase(attrs, "JOINTS_1");
        erase(attrs, "WEIGHTS_1");
        std::get<Array>(field(fixture.root, "meshes").value).push_back(staticMesh);
        field(fixture.root, "nodes")  = parse(R"([{"mesh":0,"skin":0,"translation":[8,0,0]},
            {"children":[2,3,4,5,6]},{},{},{},{},{},
            {"mesh":0,"skin":0},{"mesh":0,"skin":1},{"mesh":1}])");
        field(fixture.root, "skins")  = parse(R"([{"joints":[2,3,4,5,6]},{"joints":[6,5,4,3,2]}])");
        field(fixture.root, "scenes") = parse(R"([{"nodes":[0,1,7,8,9]}])");
        auto animation                = fixture.addAnimation("motion");
        auto times                    = fixture.addAnimationTimes({0, 1});
        auto output                   = fixture.addAccessor(floats({0, 0, 0, 0, 1, 0}), "VEC3", 5126, 2);
        auto sampler                  = fixture.addAnimationSampler(animation, times, output);
        fixture.addAnimationChannel(animation, sampler, 2, "translation");
        return fixture;
    }

    void skinnedModels(const std::filesystem::path& directory)
    {
        GtsModelRegistry registry;
        auto             fixture = rigged();
        auto             source  = loadFixture(registry, fixture, directory / "rigged");
        auto             result  = realizeGtsModel(source);
        require(result.succeeded(), "Animated mixed GLB realizes without pose evaluation");
        const auto& model = *result.model();
        require(model.geometry.size() == 3 && model.occurrences.size() == 4, "Two bindings and static attachment");
        require(model.occurrences[0].geometryIndex == 0 && model.occurrences[1].geometryIndex == 0 &&
                    model.occurrences[2].geometryIndex == 1 && model.occurrences[3].geometryIndex == 2,
                "Same binding shares; distinct binding and profile do not");
        require(model.geometry[2].profile() == GtsGeometryProfile::Static &&
                    model.geometry[0].profile() == GtsGeometryProfile::Skinned,
                "Explicit mixed profiles");
        for (size_t i = 0; i < 3; ++i)
        {
            const auto& occurrence = model.occurrences[i];
            const auto& node       = source->nodes()[occurrence.modelNodeIndex];
            require(occurrence.skinBindingIndex == node.skinBindingIndex &&
                        occurrence.skeletonUseIndex ==
                            source->canonicalModel()->skinBindings[*node.skinBindingIndex].skeletonUseIndex,
                    "Binding/use association");
        }
        const auto& mesh = *model.geometry[0].skinnedMesh();
        require(mesh.influences.reducedInfluenceVertices == 3 && mesh.influences.maxSourceInfluenceCount == 5,
                "Existing reduction metadata preserved");
        require(mesh.vertices[0].joints == glm::uvec4(4, 3, 2, 1), "Strongest skin-local slots, not skeleton indices");
        require(std::abs(glm::dot(mesh.vertices[0].weights, glm::vec4(1)) - 1) < 1e-6, "Reduced weights normalized");
        require(mesh.vertices[0].pos == glm::vec3(0) && source->nodes()[0].localTransform[3].x == 8,
                "Skinned mesh node transform not baked");
        require(!result.diagnostics().empty() &&
                    result.diagnostics()[0].location.find("skinBindings[0]") != std::string::npos,
                "Warnings carry binding context");
        require(model.model->skeletons()[0].get() == source->skeletons()[0].get(), "Skeleton definition not copied");

        // Canonical data permits unbound weighted occurrences; static profile deliberately rejects them.
        field(at(field(fixture.root, "nodes"), 9), "mesh") = 0u;
        auto                     unsupported               = loadFixture(registry, fixture, directory / "unsupported");
        GtsModelRealizationCache cache;
        auto                     failed          = cache.realize(unsupported);
        auto                     repeatedFailure = cache.realize(unsupported);
        require(cache.size() == 0 && !repeatedFailure.succeeded() &&
                    repeatedFailure.diagnostics().back().message == failed.diagnostics().back().message &&
                    repeatedFailure.diagnostics().back().location == failed.diagnostics().back().location,
                "Failures expose deterministic diagnostics and never poison cache");
        require(!failed.succeeded() && !failed.model(), "No partial model after earlier successful preparations");
        require(std::any_of(failed.diagnostics().begin(),
                            failed.diagnostics().end(),
                            [](const auto& d)
                            {
                                return d.code == "STATIC_SKINNING_UNSUPPORTED" &&
                                       d.location.find("nodes[9]") != std::string::npos &&
                                       d.location.find("meshes[0]") != std::string::npos &&
                                       d.location.find("primitives[0]") != std::string::npos;
                            }),
                "Failure preserves detailed profile/node/mesh/primitive context");
        require(unsupported->canonicalModel()->meshes[0].primitives[0].attributes.size() == 5,
                "Failure cannot mutate original attributes");
    }

    void cookedModels(const std::filesystem::path& directory)
    {
        MeshAssetData mesh;
        mesh.vertices.resize(3);
        mesh.vertices[1].pos.x   = 1;
        mesh.vertices[2].pos.y   = 1;
        mesh.vertices[0].normal  = {3, 4, 5};
        mesh.vertices[0].tangent = {6, 7, 8, -1};
        mesh.indices             = {2, 1, 0, 0, 1, 2};
        mesh.attributes          = VertexAttributeFlags::Position;
        mesh.generatedNormals    = true;
        mesh.bounds              = {{-5, -6, -7}, {5, 6, 7}, true}; // Deliberately differs from tight geometry bounds.
        mesh.submeshes    = {{0, 3, AssetReference::fromLogicalPath("first.gmat"), "first"}, {3, 3, {}, "unassigned"}};
        mesh.dependencies = {mesh.submeshes[0].material};
        std::string error;
        require(MeshAssetSerializer::writeFile(mesh, directory / "prepared.gmesh", &error), error);
        std::shared_ptr<const GtsRealizedModel> retained;
        {
            GtsModelRegistry registry;
            auto             source = load(registry, directory / "prepared.gmesh");
            auto             result = realizeGtsModel(source);
            require(result.succeeded(), "Prepared gmesh realizes");
            retained             = result.model();
            const auto& geometry = retained->geometry[0];
            const auto& original = source->preparedModel()->meshes[0];
            require(!source->canonicalModel() && geometry.staticVertices().data() == original.vertices.data() &&
                        geometry.indices().data() == original.indices.data(),
                    "Zero-copy cooked buffers, no fabricated streams");
            require(std::memcmp(geometry.staticVertices().data(),
                                original.vertices.data(),
                                original.vertices.size() * sizeof(GtsStaticVertex)) == 0,
                    "Prepared bytes identical");
            require(geometry.staticVertices()[0].normal == glm::vec3(3, 4, 5) &&
                        geometry.staticVertices()[0].tangent == glm::vec4(6, 7, 8, -1),
                    "No normal/tangent regeneration");
            require(geometry.bounds().min == mesh.bounds.min && geometry.bounds().max == mesh.bounds.max &&
                        geometry.metadata().generatedNormals && !geometry.metadata().generatedTangents &&
                        geometry.metadata().attributes == mesh.attributes,
                    "Cooked bounds and flags unchanged");
            require(geometry.primitives().size() == 2 && geometry.primitives()[1].firstIndex == 3 &&
                        std::holds_alternative<std::monostate>(geometry.primitives()[1].material),
                    "Ranges/default preserved");
            const auto& material = std::get<GtsExternalMaterialReference>(geometry.primitives()[0].material);
            require(material.reference.logicalPath == "first.gmat" && material.referenceDirectory == directory,
                    "External material and base directory preserved without lookup");

            ModelAssetData package;
            package.meshes       = {AssetReference::fromLogicalPath("prepared.gmesh")};
            package.materials    = {mesh.submeshes[0].material};
            package.dependencies = package.meshes;
            package.nodes.resize(3);
            package.nodes[0].parentIndex         = 2;
            package.nodes[0].mesh                = package.meshes[0];
            package.nodes[0].localTransform[3].x = 12;
            package.nodes[1].mesh                = package.meshes[0];
            require(ModelAssetSerializer::writeFile(package, directory / "package.gmodel", &error), error);
            auto model    = load(registry, directory / "package.gmodel");
            auto realized = realizeGtsModel(model);
            require(realized.succeeded() && realized.model()->geometry.size() == 1 &&
                        realized.model()->occurrences.size() == 2,
                    "gmodel sharing preserved");
            require(realized.model()->model->nodes()[2].children == std::vector<uint32_t>{0} &&
                        model->rootNodes().size() == 2 && model->nodes()[0].localTransform[3].x == 12,
                    "Forward parent and transforms preserved");
            require(realized.model()->geometry[0].staticVertices()[0].pos.x == 0 &&
                        model->preparedModel()->dependencies[0].logicalPath == "prepared.gmesh",
                    "Unbaked geometry and dependencies");
        }
        require(retained->geometry[0].staticVertices()[1].pos.x == 1,
                "Realization owns source beyond registry lifetime");
        auto geometry = retained->geometry[0];
        retained.reset();
        require(geometry.indices()[0] == 2, "Copied geometry views retain their own backing lifetime");
        mesh.submeshes.clear();
        require(MeshAssetSerializer::writeFile(mesh, directory / "implicit.gmesh", &error), error);
        GtsModelRegistry registry;
        auto             implicit = realizeGtsModel(load(registry, directory / "implicit.gmesh"));
        require(implicit.model()->geometry[0].primitives().size() == 1 &&
                    implicit.model()->geometry[0].primitives()[0].indexCount == 6,
                "Implicit cooked whole-mesh range");
        ModelAssetData empty;
        empty.nodes.resize(1);
        require(ModelAssetSerializer::writeFile(empty, directory / "empty.gmodel", &error), error);
        auto noGeometry = realizeGtsModel(load(registry, directory / "empty.gmodel"));
        require(!noGeometry.succeeded() && !noGeometry.model() &&
                    noGeometry.diagnostics()[0].code == "model.realization.empty",
                "Valid definition without geometry produces a focused realization failure");
    }
} // namespace

int main()
{
    ScopedRuntimeAssetPolicy policy("development");
    const auto               directory = std::filesystem::temp_directory_path() / "gravitas-model-realization-test";
    std::filesystem::create_directories(directory);
    staticModels(directory);
    skinnedModels(directory);
    cookedModels(directory);
    std::filesystem::remove_all(directory);
}
