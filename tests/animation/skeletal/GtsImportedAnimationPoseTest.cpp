#include "../../assets/importers/gltf/GltfFixtureBuilder.h"

#include "animation/skeletal/GtsSkeletonPoseEvaluation.h"
#include "assets/skeleton/GtsSkeletonAsset.h"

namespace
{
    void near(float value, float expected)
    {
        require(std::isfinite(value) && std::abs(value - expected) < 2e-5f, "Unexpected imported pose component");
    }
    void animatedGlb(const std::filesystem::path& directory)
    {
        GltfFixtureBuilder f;
        f.addVertexStream("JOINTS_0", std::vector<uint8_t>(12, 0), "VEC4", 5121);
        f.addVertexStream("WEIGHTS_0", floats({1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}), "VEC4", 5126);
        field(f.root, "nodes")     = parse(R"([
            {"mesh":0,"skin":0,"translation":[1000,0,0]},
            {"name":"joint","translation":[1,0,0]},
            {"name":"helper","translation":[10,0,0],"children":[1]}])");
        field(f.root, "skins")     = parse(R"([{"joints":[1]}])");
        field(f.root, "scenes")    = parse(R"([{"nodes":[0,2]}])");
        const auto walk            = f.addAnimation("Walk");
        const auto times           = f.addAnimationTimes({0, 2});
        const auto rotations       = f.addAccessor(floats({0, 0, 0, 1, 0, 0, 1, 0}), "VEC4", 5126, 2);
        const auto rotationSampler = f.addAnimationSampler(walk, times, rotations);
        f.addAnimationChannel(walk, rotationSampler, 2, "rotation");
        const auto translations =
            f.addAccessor(floats({0, 0, 0, 10, 0, 0, 0, 4, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0}), "VEC3", 5126, 6);
        const auto translationSampler = f.addAnimationSampler(walk, times, translations, "CUBICSPLINE");
        f.addAnimationChannel(walk, translationSampler, 2, "translation");
        const auto twist          = f.addAnimation("Twist");
        const auto cubicRotations = f.addAccessor(
            floats({0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0}), "VEC4", 5126, 6);
        const auto cubicSampler = f.addAnimationSampler(twist, times, cubicRotations, "CUBICSPLINE");
        f.addAnimationChannel(twist, cubicSampler, 2, "rotation");

        const auto  imported = importFixture(f, directory, GltfFixtureFormat::Glb);
        std::string diagnostics;
        for (const auto& d : imported.diagnostics())
            diagnostics += d.code + ": " + d.message + "\n";
        require(imported.succeeded(), diagnostics);
        const auto& bundle = *imported.bundle();
        require(bundle.skeletons.size() == 1 && bundle.animationClips.size() == 2,
                "Animated GLB exposes expected assets");
        const auto& skeleton = *bundle.skeletons[0];
        const auto& clip     = bundle.animationClips[0];
        require(clip.name == "Walk" && skeleton.nodes.size() == 2 && clip.tracks[0].skeletonNodeIndex == 0,
                "Source helper node 2 targets evaluation node 0");
        const std::vector<float>     timesToEvaluate{0, 0.5f, 1, 1.5f, 2};
        const float                  h = std::sqrt(0.5f);
        const std::vector<glm::vec3> expected{
            {11, 0, 0}, {10 + h, 1.125f + h, 0}, {10, 2, 0}, {10 - h, 0.375f + h, 0}, {9, 0, 0}};
        for (size_t i = 0; i < timesToEvaluate.size(); ++i)
        {
            const auto pose = evaluateGtsAnimationPose(skeleton, clip, timesToEvaluate[i]);
            require(pose.succeeded() && pose.pose(), "Imported animation evaluates without rendering");
            for (glm::length_t c = 0; c < 3; ++c)
                near(pose.pose()->modelTransforms[1][3][c], expected[i][c]);
            const auto& joint = std::get<GtsSkeletonTrs>(pose.pose()->localTransforms[1]);
            near(joint.translation.x, 1);
            near(joint.scale.x, 1);
        }
        const auto cubic = evaluateGtsAnimationPose(skeleton, bundle.animationClips[1], 1);
        require(cubic.succeeded(), "Imported quaternion cubic evaluates");
        const auto& helper = std::get<GtsSkeletonTrs>(cubic.pose()->localTransforms[0]);
        near(helper.rotation.z, h);
        near(helper.rotation.w, h);
        near(cubic.pose()->modelTransforms[1][3].x, 10);
        near(cubic.pose()->modelTransforms[1][3].y, 1);
        require(bundle.model->nodes[0].localTransform[3][0] == 1000, "Model placement remains outside the evaluator");
        const auto again = evaluateGtsAnimationPose(skeleton, bundle.animationClips[1], 1);
        for (size_t node = 0; node < skeleton.nodes.size(); ++node)
            for (glm::length_t c = 0; c < 4; ++c)
                for (glm::length_t r = 0; r < 4; ++r)
                    require(again.pose()->modelTransforms[node][c][r] == cubic.pose()->modelTransforms[node][c][r],
                            "Imported pose evaluation is deterministic");
    }
} // namespace

int main()
{
    try
    {
        const auto directory = std::filesystem::temp_directory_path() / "gravitas_imported_pose_test";
        std::filesystem::create_directories(directory);
        animatedGlb(directory);
        std::filesystem::remove_all(directory);
        std::puts("GtsImportedAnimationPoseTest passed");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
