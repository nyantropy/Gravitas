#include "../../assets/importers/gltf/GltfFixtureBuilder.h"
#include "animation/skeletal/GtsSkeletonPoseEvaluation.h"
#include "animation/skinning/GtsSkinPaletteEvaluation.h"
#include "GtsSkinnedMeshPreparation.h"

namespace
{
    void animatedGlb(const std::filesystem::path& directory)
    {
        GltfFixtureBuilder gltf;
        gltf.addVertexStream("JOINTS_0", std::vector<uint8_t>{0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0}, "VEC4", 5121);
        gltf.addVertexStream(
            "WEIGHTS_0", floats({0.25f, 0.75f, 0, 0, 0.25f, 0.75f, 0, 0, 0.25f, 0.75f, 0, 0}), "VEC4", 5126);
        field(gltf.root, "nodes")  = parse(R"([
            {"mesh":0,"skin":0,"translation":[1000,0,0]},
            {"name":"joint","translation":[1,0,0]},
            {"name":"helper","translation":[10,0,0],"children":[1]}])");
        field(gltf.root, "skins")  = parse(R"([{"joints":[1,2]}])");
        field(gltf.root, "scenes") = parse(R"([{"nodes":[0,2]}])");
        const auto inverseBinds    = gltf.addAccessor(floats({1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -11, 0, 0, 1,
                                                              1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -10, 0, 0, 1}),
                                                      "MAT4",
                                                      5126,
                                                      2);
        field(at(field(gltf.root, "skins"), 0), "inverseBindMatrices") = Json{double(inverseBinds)};
        const auto animation                                           = gltf.addAnimation("Walk");
        const auto times                                               = gltf.addAnimationTimes({0, 2});
        const auto helperValues = gltf.addAccessor(floats({10, 0, 0, 14, 0, 0}), "VEC3", 5126, 2);
        const auto jointValues  = gltf.addAccessor(floats({1, 0, 0, 3, 0, 0}), "VEC3", 5126, 2);
        gltf.addAnimationChannel(animation, gltf.addAnimationSampler(animation, times, helperValues), 2, "translation");
        gltf.addAnimationChannel(animation, gltf.addAnimationSampler(animation, times, jointValues), 1, "translation");
        const auto  imported = importFixture(gltf, directory, GltfFixtureFormat::Glb);
        std::string diagnostics;
        for (const auto& d : imported.diagnostics())
            diagnostics += d.code + ": " + d.message + "\n";
        require(imported.succeeded(), diagnostics);
        const auto& bundle = *imported.bundle();
        require(bundle.skeletons.size() == 1 && bundle.animationClips.size() == 1, "Animated rigged bundle");
        const auto& model = *bundle.model;
        const auto& node  = model.nodes[0];
        require(node.skinBindingIndex && node.meshIndex, "Skinned model node");
        const auto& association = model.skinBindings[*node.skinBindingIndex];
        const auto& binding     = association.binding;
        const auto& skeleton    = *model.skeletonUses[association.skeletonUseIndex].skeleton;
        require(binding.joints[0].skeletonNodeIndex == 1 && binding.joints[1].skeletonNodeIndex == 0,
                "Binding order differs from evaluation hierarchy");
        const auto prepared = prepareGtsSkinnedMesh(model.meshes[*node.meshIndex], binding);
        require(prepared.succeeded(), "Geometry branch prepares imported mesh");
        std::vector<GtsSkinPalette> palettes;
        for (float time : {0.0f, 1.0f, 2.0f})
        {
            const auto pose = evaluateGtsAnimationPose(skeleton, bundle.animationClips[0], time);
            require(pose.succeeded(), "Imported animation pose evaluates");
            const auto palette = evaluateGtsSkinPalette(*pose.pose(), binding, skeleton);
            require(palette.succeeded() && palette.palette()->matrices.size() == binding.joints.size(),
                    "Palette count follows skin slots");
            for (size_t slot = 0; slot < palette.palette()->matrices.size(); ++slot)
            {
                const auto& matrix = palette.palette()->matrices[slot];
                for (glm::length_t c = 0; c < 4; ++c)
                    for (glm::length_t r = 0; r < 4; ++r)
                    {
                        float expected = c == r ? 1.0f : 0.0f;
                        if (c == 3 && r == 0)
                            expected = time * (slot == 0 ? 3.0f : 2.0f);
                        require(std::isfinite(matrix[c][r]) && std::abs(matrix[c][r] - expected) < 1e-6f,
                                "Pose and authored inverse binds produce expected slot matrix without world placement");
                    }
            }
            for (const auto& vertex : prepared.mesh()->vertices)
                for (glm::length_t component = 0; component < 4; ++component)
                    require(vertex.joints[component] < palette.palette()->matrices.size(),
                            "Prepared joints directly index palette, including zero-weight padding");
            palettes.push_back(*palette.palette());
        }
        require(palettes[0].matrices[0][3][0] != palettes[1].matrices[0][3][0] &&
                    palettes[1].matrices[1][3][0] != palettes[2].matrices[1][3][0],
                "Palette follows animated joint movement");
        require(prepared.mesh()->vertices[0].pos == glm::vec3(0), "Palette evaluation does not deform geometry");
    }
} // namespace
int main()
{
    try
    {
        const auto directory = std::filesystem::temp_directory_path() / "gravitas_imported_skin_palette_test";
        std::filesystem::create_directories(directory);
        animatedGlb(directory);
        std::filesystem::remove_all(directory);
        std::puts("GtsImportedSkinPaletteTest passed");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
