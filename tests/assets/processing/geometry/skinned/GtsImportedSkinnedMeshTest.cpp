#include "../../../importers/gltf/GltfFixtureBuilder.h"
#include "GtsSkinnedMeshPreparation.h"

namespace
{
    void animatedGlb(const std::filesystem::path& directory)
    {
        GltfFixtureBuilder gltf;
        gltf.addVertexStream("JOINTS_0", std::vector<uint8_t>{0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0}, "VEC4", 5121);
        gltf.addVertexStream(
            "WEIGHTS_0", floats({0.25f, 0.75f, 0, 0, 0.25f, 0.75f, 0, 0, 0.25f, 0.75f, 0, 0}), "VEC4", 5126);
        gltf.addTexCoordStream();
        field(gltf.root, "nodes")  = parse(R"([
            {"mesh":0,"skin":0,"translation":[1000,0,0]},
            {"name":"joint","translation":[1,0,0]},
            {"name":"helper","translation":[10,0,0],"children":[1]}])");
        field(gltf.root, "skins")  = parse(R"([{"joints":[1,2]}])");
        field(gltf.root, "scenes") = parse(R"([{"nodes":[0,2]}])");
        const auto animation       = gltf.addAnimation("Walk");
        const auto times           = gltf.addAnimationTimes({0, 2});
        const auto translations    = gltf.addAccessor(floats({10, 0, 0, 20, 0, 0}), "VEC3", 5126, 2);
        const auto sampler         = gltf.addAnimationSampler(animation, times, translations);
        gltf.addAnimationChannel(animation, sampler, 2, "translation");
        const auto  imported = importFixture(gltf, directory, GltfFixtureFormat::Glb);
        std::string messages;
        for (const auto& d : imported.diagnostics())
            messages += d.code + ": " + d.message + "\n";
        require(imported.succeeded(), messages);
        const auto& bundle = *imported.bundle();
        require(bundle.skeletons.size() == 1 && bundle.animationClips.size() == 1, "Rigged animated bundle");
        const auto& model = *bundle.model;
        const auto& node  = model.nodes[0];
        require(node.meshIndex && node.skinBindingIndex, "Node selects mesh and binding");
        const auto& association = model.skinBindings[*node.skinBindingIndex];
        const auto& binding     = association.binding;
        require(binding.joints[0].skeletonNodeIndex == 1 && binding.joints[1].skeletonNodeIndex == 0,
                "Source skin order differs from skeleton hierarchy");
        const auto prepared = prepareGtsSkinnedMesh(model.meshes[*node.meshIndex], binding);
        require(prepared.succeeded(), "Imported weighted mesh prepares without pose evaluation");
        const auto& mesh = *prepared.mesh();
        require(mesh.vertices.size() == 3 && mesh.indices == std::vector<uint32_t>{0, 1, 2} &&
                    mesh.primitives.size() == 1,
                "Prepared triangle counts and range");
        for (const auto& vertex : mesh.vertices)
        {
            double total = 0;
            for (glm::length_t c = 0; c < 4; ++c)
            {
                require(vertex.joints[c] < binding.joints.size(), "Every slot, including padding, fits binding");
                total += vertex.weights[c];
            }
            require(std::abs(total - 1) < 1e-6, "Prepared weights sum to one");
            require(vertex.joints == glm::uvec4(0, 1, 0, 0) && vertex.weights == glm::vec4(0.25f, 0.75f, 0, 0),
                    "Prepared joint slots are not remapped into skeleton indices");
        }
        require(mesh.vertices[0].pos == glm::vec3(0) && mesh.vertices[1].pos == glm::vec3(1, 0, 0),
                "Neither model placement, skeleton defaults nor animation deforms stored geometry");
        require(mesh.metadata.generatedNormals && mesh.metadata.generatedTangents, "Shared geometry generation");
    }
} // namespace
int main()
{
    try
    {
        const auto directory = std::filesystem::temp_directory_path() / "gravitas_imported_skinned_mesh_test";
        std::filesystem::create_directories(directory);
        animatedGlb(directory);
        std::filesystem::remove_all(directory);
        std::puts("GtsImportedSkinnedMeshTest passed");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
