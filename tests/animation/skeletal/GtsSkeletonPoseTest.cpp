#include "GtsSkeletonPoseEvaluation.h"
#include "GtsAnimationClipAsset.h"
#include "GtsSkeletonAsset.h"

#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }
    void near(float actual, float expected, const char* message)
    {
        require(std::isfinite(actual) && std::abs(actual - expected) <= 2e-5f, message);
    }
    void vectorNear(const glm::vec3& actual, const glm::vec3& expected)
    {
        for (glm::length_t i = 0; i < 3; ++i)
            near(actual[i], expected[i], "Unexpected vector component");
    }
    void quaternionNear(const glm::quat& actual, const glm::quat& expected)
    {
        for (glm::length_t i = 0; i < 4; ++i)
            near(actual[i], expected[i], "Unexpected quaternion component");
    }
    void exactMatrix(const glm::mat4& a, const glm::mat4& b)
    {
        for (glm::length_t c = 0; c < 4; ++c)
            for (glm::length_t r = 0; r < 4; ++r)
                require(a[c][r] == b[c][r], "Matrix changed");
    }
    GtsSkeletonPose successful(const GtsSkeletonPoseEvaluationResult& result)
    {
        if (!result.succeeded())
        {
            std::string errors;
            for (const auto& error : result.diagnostics())
                errors += error.code + ": " + error.message + " @ " + error.location + "\n";
            throw std::runtime_error(errors);
        }
        require(result.pose() && result.diagnostics().empty(), "Successful pose has no errors");
        return *result.pose();
    }
    void failure(const GtsSkeletonPoseEvaluationResult& result, const std::string& code)
    {
        require(!result.succeeded() && !result.pose(), "Failure must expose no partial pose");
        for (const auto& error : result.diagnostics())
            if (error.code == code && !error.location.empty() && !error.message.empty())
                return;
        throw std::runtime_error("Expected " + code);
    }
    GtsSkeletonAsset skeleton()
    {
        GtsSkeletonAsset result;
        result.nodes = {{{"root"}, "Root", std::nullopt, GtsSkeletonTrs{{2, 3, 4}, {1, 0, 0, 0}, {2, 3, 4}}}};
        return result;
    }
    GtsAnimationTrack track(GtsAnimationTarget target = GtsAnimationTarget::Translation)
    {
        GtsAnimationTrack result;
        result.skeletonNodeIndex = 0;
        result.target            = target;
        result.timesSeconds      = {0, 2};
        if (target == GtsAnimationTarget::Rotation)
            result.values = std::vector<glm::quat>{{1, 0, 0, 0}, {0, 0, 0, 1}};
        else
            result.values = std::vector<glm::vec3>{{0, 2, 4}, {4, 6, 8}};
        return result;
    }
    GtsAnimationClipAsset clip(const GtsSkeletonAsset& rig, GtsAnimationTrack channel = track(), float duration = 2)
    {
        const auto contract = makeGtsSkeletonCompatibility(rig);
        require(contract.succeeded(), "Fixture skeleton valid");
        GtsAnimationClipAsset result;
        result.targetSkeletonCompatibility = *contract.compatibility();
        result.durationSeconds             = duration;
        result.tracks                      = {std::move(channel)};
        return result;
    }
    const GtsSkeletonTrs& trs(const GtsSkeletonPose& pose, size_t node = 0)
    {
        return std::get<GtsSkeletonTrs>(pose.localTransforms[node]);
    }

    void defaultHierarchy()
    {
        auto rig  = skeleton();
        auto pose = successful(evaluateGtsDefaultPose(rig));
        require(pose.localTransforms.size() == 1 && pose.modelTransforms.size() == 1, "One-node pose index contract");
        vectorNear(trs(pose).translation, {2, 3, 4});
        vectorNear(trs(pose).scale, {2, 3, 4});
        quaternionNear(trs(pose).rotation, {1, 0, 0, 0});
        vectorNear(glm::vec3(pose.modelTransforms[0] * glm::vec4(1, 1, 1, 1)), {4, 6, 8});

        const float h                      = std::sqrt(0.5f);
        rig.nodes[0].defaultLocalTransform = GtsSkeletonTrs{{10, 0, 0}, {h, 0, 0, h}, {2, 3, 1}};
        rig.nodes.push_back({{"child"}, "Child", 0, GtsSkeletonTrs{{1, 0, 0}, {1, 0, 0, 0}, {1, 1, 1}}});
        glm::mat4 helper(1);
        helper[1][0] = 0.5f;
        helper[3][1] = 2;
        rig.nodes.push_back({{"helper"}, "Helper", 1, helper});
        rig.nodes.push_back({{"tip"}, "Tip", 2, GtsSkeletonTrs{{0, 2, 0}, {1, 0, 0, 0}, {1, 1, 1}}});
        rig.nodes.push_back(
            {{"second"}, "Other root", std::nullopt, GtsSkeletonTrs{{-5, 0, 0}, {1, 0, 0, 0}, {1, 1, 1}}});
        pose = successful(evaluateGtsDefaultPose(rig));
        vectorNear(glm::vec3(pose.modelTransforms[1][3]), {10, 2, 0});
        vectorNear(glm::vec3(pose.modelTransforms[2][3]), {4, 2, 0});
        vectorNear(glm::vec3(pose.modelTransforms[3][3]), {-2, 4, 0});
        vectorNear(glm::vec3(pose.modelTransforms[4][3]), {-5, 0, 0});
        exactMatrix(std::get<glm::mat4>(pose.localTransforms[2]), helper);
        const auto motion   = clip(rig);
        const auto animated = successful(evaluateGtsAnimationPose(rig, motion, 1));
        exactMatrix(std::get<glm::mat4>(animated.localTransforms[2]), helper);
        exactMatrix(animated.modelTransforms[4], pose.modelTransforms[4]);
    }

    void stepAndEndpoints()
    {
        const auto rig        = skeleton();
        auto       channel    = track();
        channel.interpolation = GtsAnimationInterpolation::Step;
        channel.timesSeconds  = {1, 2, 3};
        channel.values        = std::vector<glm::vec3>{{10, 0, 0}, {20, 0, 0}, {30, 0, 0}};
        auto motion           = clip(rig, channel, 4);
        for (const auto [time, expected] :
             std::vector<std::pair<float, float>>{{0, 10}, {1, 10}, {1.5f, 10}, {2, 20}, {2.99f, 20}, {3, 30}, {4, 30}})
            near(trs(successful(evaluateGtsAnimationPose(rig, motion, time))).translation.x,
                 expected,
                 "STEP boundary semantics");
        for (const auto interpolation : {GtsAnimationInterpolation::Step,
                                         GtsAnimationInterpolation::Linear,
                                         GtsAnimationInterpolation::CubicSpline})
        {
            channel.interpolation = interpolation;
            channel.timesSeconds  = {1};
            if (interpolation == GtsAnimationInterpolation::CubicSpline)
                channel.values = std::vector<GtsAnimationCubicVec3Key>{{{99, 0, 0}, {7, 8, 9}, {-99, 0, 0}}};
            else
                channel.values = std::vector<glm::vec3>{{7, 8, 9}};
            motion = clip(rig, channel, 2);
            for (float time : {0.0f, 1.0f, 2.0f})
                vectorNear(trs(successful(evaluateGtsAnimationPose(rig, motion, time))).translation, {7, 8, 9});
        }
        auto rotation         = track(GtsAnimationTarget::Rotation);
        rotation.timesSeconds = {0.5f, 1.5f};
        rotation.values       = std::vector<glm::quat>{{1.00002f, 0, 0, 0}, {-1, 0, 0, 0}};
        motion                = clip(rig, rotation);
        for (float time : {0.0f, 0.5f})
            require(trs(successful(evaluateGtsAnimationPose(rig, motion, time))).rotation.w == 1.00002f,
                    "Endpoint returns authored near-unit key exactly");
        for (float time : {1.5f, 2.0f})
            require(trs(successful(evaluateGtsAnimationPose(rig, motion, time))).rotation.w == -1,
                    "Exact final key retains authored sign");
    }

    void linearAndProperties()
    {
        const auto rig = skeleton();
        for (auto property : {GtsAnimationTarget::Translation, GtsAnimationTarget::Scale})
        {
            const auto motion = clip(rig, track(property));
            const auto pose   = successful(evaluateGtsAnimationPose(rig, motion, 1));
            if (property == GtsAnimationTarget::Translation)
            {
                vectorNear(trs(pose).translation, {2, 4, 6});
                vectorNear(trs(pose).scale, {2, 3, 4});
            }
            else
            {
                vectorNear(trs(pose).scale, {2, 4, 6});
                vectorNear(trs(pose).translation, {2, 3, 4});
            }
            quaternionNear(trs(pose).rotation, {1, 0, 0, 0});
        }
        auto        motion = clip(rig, track(GtsAnimationTarget::Rotation));
        auto        pose   = successful(evaluateGtsAnimationPose(rig, motion, 1));
        const float h      = std::sqrt(0.5f);
        quaternionNear(trs(pose).rotation, {h, 0, 0, h});
        vectorNear(trs(pose).translation, {2, 3, 4});
        vectorNear(trs(pose).scale, {2, 3, 4});
        vectorNear(glm::vec3(pose.modelTransforms[0] * glm::vec4(1, 0, 0, 1)), {2, 5, 4});
        std::get<std::vector<glm::quat>>(motion.tracks[0].values)[1] = {-h, 0, 0, -h};
        pose = successful(evaluateGtsAnimationPose(rig, motion, 1));
        quaternionNear(trs(pose).rotation,
                       {std::cos(3.14159265358979323846f / 8), 0, 0, std::sin(3.14159265358979323846f / 8)});
        require(std::get<std::vector<glm::quat>>(motion.tracks[0].values)[1].w == -h,
                "Shortest arc does not change stored sign");
        std::get<std::vector<glm::quat>>(motion.tracks[0].values)[1] = {-1, 0, 0, 0};
        quaternionNear(trs(successful(evaluateGtsAnimationPose(rig, motion, 1))).rotation, {1, 0, 0, 0});

        motion             = clip(rig);
        auto scale         = track(GtsAnimationTarget::Scale);
        scale.timesSeconds = {1, 2};
        motion.tracks.push_back(scale);
        auto rotation         = track(GtsAnimationTarget::Rotation);
        rotation.timesSeconds = {0.5f};
        rotation.values       = std::vector<glm::quat>{{h, 0, 0, h}};
        motion.tracks.push_back(rotation);
        pose = successful(evaluateGtsAnimationPose(rig, motion, 0.5f));
        vectorNear(trs(pose).translation, {1, 3, 5});
        vectorNear(trs(pose).scale, {0, 2, 4});
        quaternionNear(trs(pose).rotation, {h, 0, 0, h});
    }

    void cubicCurves()
    {
        const auto rig = skeleton();
        for (const auto target : {GtsAnimationTarget::Translation, GtsAnimationTarget::Scale})
        {
            auto channel          = track(target);
            channel.interpolation = GtsAnimationInterpolation::CubicSpline;
            channel.timesSeconds  = {1, 3};
            channel.values        = std::vector<GtsAnimationCubicVec3Key>{{{9, 9, 9}, {0, 0, 0}, {4, 0, 0}},
                                                                          {{0, 0, 0}, {0, 0, 0}, {9, 9, 9}}};
            auto       motion     = clip(rig, channel, 4);
            const auto pose       = successful(evaluateGtsAnimationPose(rig, motion, 2));
            vectorNear(target == GtsAnimationTarget::Translation ? trs(pose).translation : trs(pose).scale, {1, 0, 0});
            // h10(1/4) * dt * derivative = 9/64 * 2 * 4 = 1.125
            const auto quarter = successful(evaluateGtsAnimationPose(rig, motion, 1.5f));
            near((target == GtsAnimationTarget::Translation ? trs(quarter).translation : trs(quarter).scale).x,
                 1.125f,
                 "Cubic basis and dt scaling");
            for (float time : {0.0f, 1.0f, 3.0f, 4.0f})
            {
                const auto endpoint = successful(evaluateGtsAnimationPose(rig, motion, time));
                vectorNear(target == GtsAnimationTarget::Translation ? trs(endpoint).translation : trs(endpoint).scale,
                           {0, 0, 0});
            }
        }
        auto rotation          = track(GtsAnimationTarget::Rotation);
        rotation.interpolation = GtsAnimationInterpolation::CubicSpline;
        rotation.values        = std::vector<GtsAnimationCubicRotationKey>{{{0, 0, 0, 0}, {1, 0, 0, 0}, {4, 8, 12, 16}},
                                                                           {{0, 0, 0, 0}, {1, 0, 0, 0}, {0, 0, 0, 0}}};
        auto motion            = clip(rig, rotation);
        auto pose              = successful(evaluateGtsAnimationPose(rig, motion, 1));
        // XYZW raw midpoint = (1,2,3,5), norm sqrt(39); tangent W is not scalar constructor first
        const float length = std::sqrt(39.0f);
        quaternionNear(trs(pose).rotation, {5 / length, 1 / length, 2 / length, 3 / length});
        near(glm::dot(trs(pose).rotation, trs(pose).rotation), 1, "Cubic rotation output normalized");
        require(std::get<std::vector<GtsAnimationCubicRotationKey>>(motion.tracks[0].values)[0].outTangent.w == 16,
                "Cubic derivatives are not normalized in place");
        for (float time : {0.0f, 2.0f})
            quaternionNear(trs(successful(evaluateGtsAnimationPose(rig, motion, time))).rotation, {1, 0, 0, 0});
        auto& keys         = std::get<std::vector<GtsAnimationCubicRotationKey>>(motion.tracks[0].values);
        keys[0].outTangent = {4, 0, 0, 0};
        keys[1].value      = {-1, 0, 0, 0};
        quaternionNear(trs(successful(evaluateGtsAnimationPose(rig, motion, 1))).rotation, {0, 1, 0, 0});
        keys[0].outTangent = {0, 0, 0, 0};
        failure(evaluateGtsAnimationPose(rig, motion, 1), "POSE_SAMPLE_INVALID");
        quaternionNear(trs(successful(evaluateGtsAnimationPose(rig, motion, 2))).rotation, {-1, 0, 0, 0});
    }

    void hierarchyOverrides()
    {
        auto rig                           = skeleton();
        rig.nodes[0].defaultLocalTransform = GtsSkeletonTrs{{10, 0, 0}, {1, 0, 0, 0}, {1, 1, 1}};
        rig.nodes.push_back({{"helper"}, "Helper", 0, GtsSkeletonTrs{{0, 2, 0}, {1, 0, 0, 0}, {1, 1, 1}}});
        rig.nodes.push_back({{"joint"}, "Joint", 1, GtsSkeletonTrs{{0, 0, 3}, {1, 0, 0, 0}, {2, 2, 2}}});
        rig.nodes.push_back({{"other"}, "Other", std::nullopt, GtsSkeletonTrs{{-5, 0, 0}, {1, 0, 0, 0}, {1, 1, 1}}});
        auto channel              = track();
        channel.skeletonNodeIndex = 1;
        auto motion               = clip(rig, channel);
        auto second               = track(GtsAnimationTarget::Scale);
        second.skeletonNodeIndex  = 2;
        motion.tracks.insert(motion.tracks.begin(), second);
        const auto pose = successful(evaluateGtsAnimationPose(rig, motion, 1));
        vectorNear(trs(pose, 0).translation, {10, 0, 0});
        vectorNear(glm::vec3(pose.modelTransforms[1][3]), {12, 4, 6});
        vectorNear(glm::vec3(pose.modelTransforms[2][3]), {12, 4, 9});
        vectorNear(trs(pose, 2).translation, {0, 0, 3});
        vectorNear(trs(pose, 2).scale, {2, 4, 6});
        vectorNear(glm::vec3(pose.modelTransforms[3][3]), {-5, 0, 0});
    }

    void errorsAndImmutability()
    {
        auto rig    = skeleton();
        auto motion = clip(rig);
        for (float time : {-1.0f,
                           std::nextafter(2.0f, 3.0f),
                           std::numeric_limits<float>::infinity(),
                           std::numeric_limits<float>::quiet_NaN()})
            failure(evaluateGtsAnimationPose(rig, motion, time), "POSE_TIME_INVALID");
        auto zero                   = motion;
        zero.durationSeconds        = 0;
        zero.tracks[0].timesSeconds = {0};
        zero.tracks[0].values       = std::vector<glm::vec3>{{1, 2, 3}};
        vectorNear(trs(successful(evaluateGtsAnimationPose(rig, zero, 0))).translation, {1, 2, 3});
        failure(evaluateGtsAnimationPose(rig, zero, std::numeric_limits<float>::denorm_min()), "POSE_TIME_INVALID");
        auto badRig              = rig;
        badRig.nodes[0].id.value = "other";
        failure(evaluateGtsAnimationPose(badRig, motion, 0), "ANIMATION_SKELETON_INCOMPATIBLE");
        badRig.nodes[0].parentIndex = 0;
        failure(evaluateGtsDefaultPose(badRig), "SKELETON_PARENT_ORDER");
        failure(evaluateGtsAnimationPose(badRig, motion, 0), "ANIMATION_SKELETON_INVALID");
        auto badClip             = motion;
        badClip.tracks[0].values = std::vector<glm::vec3>{};
        failure(evaluateGtsAnimationPose(rig, badClip, 0), "ANIMATION_KEY_COUNT");
        badRig                                = rig;
        badRig.nodes[0].defaultLocalTransform = glm::mat4(1);
        failure(evaluateGtsAnimationPose(badRig, clip(badRig), 0), "ANIMATION_NODE_NOT_TRS");

        const auto before        = successful(evaluateGtsDefaultPose(rig));
        const auto times         = motion.tracks[0].timesSeconds;
        const auto values        = std::get<std::vector<glm::vec3>>(motion.tracks[0].values);
        const auto compatibility = motion.targetSkeletonCompatibility;
        const auto a             = successful(evaluateGtsAnimationPose(rig, motion, 0.75f));
        (void)successful(evaluateGtsAnimationPose(rig, motion, 1.5f));
        const auto b = successful(evaluateGtsAnimationPose(rig, motion, 0.75f));
        exactMatrix(a.modelTransforms[0], b.modelTransforms[0]);
        for (glm::length_t c = 0; c < 3; ++c)
        {
            require(trs(a).translation[c] == trs(b).translation[c],
                    "Deterministic local output independent of history");
            for (size_t i = 0; i < values.size(); ++i)
                require(std::get<std::vector<glm::vec3>>(motion.tracks[0].values)[i][c] == values[i][c],
                        "Clip values unchanged");
        }
        exactMatrix(before.modelTransforms[0], successful(evaluateGtsDefaultPose(rig)).modelTransforms[0]);
        require(times == motion.tracks[0].timesSeconds &&
                    areGtsSkeletonCompatibilitiesEqual(compatibility, motion.targetSkeletonCompatibility),
                "Clip timing and compatibility unchanged");

        auto huge = skeleton();
        std::get<GtsSkeletonTrs>(huge.nodes[0].defaultLocalTransform).scale =
            glm::vec3(std::numeric_limits<float>::max());
        huge.nodes.push_back({{"child"}, "", 0, GtsSkeletonTrs{{0, 0, 0}, {1, 0, 0, 0}, {2, 2, 2}}});
        failure(evaluateGtsDefaultPose(huge), "POSE_TRANSFORM_NONFINITE");
        auto extreme          = track();
        extreme.interpolation = GtsAnimationInterpolation::CubicSpline;
        extreme.values        = std::vector<GtsAnimationCubicVec3Key>{
            {{0, 0, 0}, {0, 0, 0}, {std::numeric_limits<float>::max(), 0, 0}}, {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}}};
        extreme.timesSeconds = {0, 32};
        failure(evaluateGtsAnimationPose(rig, clip(rig, extreme, 32), 16), "POSE_SAMPLE_INVALID");
    }
} // namespace

int main()
{
    try
    {
        defaultHierarchy();
        stepAndEndpoints();
        linearAndProperties();
        cubicCurves();
        hierarchyOverrides();
        errorsAndImmutability();
        std::puts("GtsSkeletonPoseTest passed");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
