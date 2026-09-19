#include "GtsAnimationClipAsset.h"
#include "GtsAnimationClipValidation.h"
#include "GtsSkeletonAsset.h"

#include <cmath>
#include <cstdio>
#include <exception>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    void
    requireError(const GtsAnimationClipValidationResult& result, const std::string& code, const std::string& location)
    {
        require(!result.isValid(), "Malformed clip/context must fail");
        for (const auto& error : result.diagnostics)
            if (error.code == code && error.location == location && !error.message.empty())
                return;
        throw std::runtime_error("Expected " + code + " at " + location);
    }

    GtsSkeletonAsset skeleton()
    {
        GtsSkeletonAsset result;
        result.nodes = {{{"root"}, "Root", std::nullopt, {}},
                        {{"helper"}, "Helper", 0, glm::mat4(1)},
                        {{"hip"}, "Hip", 1, {}},
                        {{"spine"}, "Spine", 2, {}}};
        return result;
    }

    GtsSkeletonCompatibility compatibility(const GtsSkeletonAsset& rig)
    {
        const auto derived = makeGtsSkeletonCompatibility(rig);
        require(derived.succeeded(), "Test rig must be valid");
        return *derived.compatibility();
    }

    GtsAnimationTrack track(GtsAnimationTarget target = GtsAnimationTarget::Translation)
    {
        GtsAnimationTrack result;
        result.skeletonNodeIndex = 2;
        result.target            = target;
        result.timesSeconds      = {0, 1};
        if (target == GtsAnimationTarget::Rotation)
            result.values = std::vector<glm::quat>{{1, 0, 0, 0}, {-1, 0, 0, 0}};
        else
            result.values = std::vector<glm::vec3>{{1, 2, 3}, {4, 5, 6}};
        return result;
    }

    GtsAnimationClipAsset clip(GtsAnimationTarget target = GtsAnimationTarget::Translation)
    {
        GtsAnimationClipAsset result;
        result.name                        = "motion";
        result.targetSkeletonCompatibility = compatibility(skeleton());
        result.durationSeconds             = 1;
        result.tracks                      = {track(target)};
        return result;
    }

    GtsAnimationClipAsset cubicClip(GtsAnimationTarget target)
    {
        auto result                    = clip(target);
        result.tracks[0].interpolation = GtsAnimationInterpolation::CubicSpline;
        if (target == GtsAnimationTarget::Rotation)
            result.tracks[0].values = std::vector<GtsAnimationCubicRotationKey>{
                {{0, 0, 0, 0}, {1, 0, 0, 0}, {2, 3, 4, 5}}, {{-2, -3, -4, -5}, {0, 1, 0, 0}, {0, 0, 0, 0}}};
        else
            result.tracks[0].values = std::vector<GtsAnimationCubicVec3Key>{{{0, 0, 0}, {1, 2, 3}, {4, 5, 6}},
                                                                            {{-4, -5, -6}, {7, 8, 9}, {0, 0, 0}}};
        return result;
    }

    void propertiesAndConstants()
    {
        for (const auto target :
             {GtsAnimationTarget::Translation, GtsAnimationTarget::Rotation, GtsAnimationTarget::Scale})
        {
            auto value = clip(target);
            for (const auto interpolation : {GtsAnimationInterpolation::Step, GtsAnimationInterpolation::Linear})
            {
                value.tracks[0].interpolation = interpolation;
                require(validateGtsAnimationClip(value).isValid(), "Each typed property supports step and linear");
                require(validateGtsAnimationClip(value, skeleton()).isValid(), "Separate compatible skeleton accepted");
            }
            value.durationSeconds        = 0;
            value.tracks[0].timesSeconds = {0};
            std::visit(
                [](auto& values)
                {
                    values.resize(1);
                },
                value.tracks[0].values);
            require(validateGtsAnimationClip(value).isValid(), "Zero-duration one-key constants are valid");
        }

        auto value = clip();
        value.tracks.push_back(track(GtsAnimationTarget::Rotation));
        value.tracks.back().timesSeconds = {0.25f, 0.75f};
        value.tracks.push_back(track(GtsAnimationTarget::Scale));
        value.tracks.back().timesSeconds = {0.5f};
        value.tracks.back().values       = std::vector<glm::vec3>{{0, -1, 2}};
        auto spine                       = track(GtsAnimationTarget::Rotation);
        spine.skeletonNodeIndex          = 3;
        value.tracks.insert(value.tracks.begin(), spine);
        require(validateGtsAnimationClip(value).isValid(),
                "Independent times, track ordering, nodes, and missing properties are valid");
        require(value.tracks.front().skeletonNodeIndex == 3 && value.tracks[2].timesSeconds[0] == 0.25f,
                "Validation preserves producer order and authored timing");
        require(std::get<std::vector<glm::quat>>(value.tracks[2].values)[1].w == -1, "Quaternion sign is preserved");
        value.tracks.push_back(value.tracks.front());
        requireError(validateGtsAnimationClip(value), "ANIMATION_TRACK_DUPLICATE", "tracks[4].target");
    }

    void cubicData()
    {
        for (const auto target :
             {GtsAnimationTarget::Translation, GtsAnimationTarget::Rotation, GtsAnimationTarget::Scale})
        {
            auto value = cubicClip(target);
            require(validateGtsAnimationClip(value, skeleton()).isValid(),
                    "Cubic triples preserve typed values and derivatives");
            if (target == GtsAnimationTarget::Rotation)
            {
                const auto& keys = std::get<std::vector<GtsAnimationCubicRotationKey>>(value.tracks[0].values);
                require(keys[0].outTangent.x == 2 && keys[0].outTangent.w == 5 && keys[1].inTangent.y == -3,
                        "Non-unit quaternion derivatives retain XYZW components without normalization");
            }
            value.durationSeconds        = 0;
            value.tracks[0].timesSeconds = {0};
            std::visit(
                [](auto& values)
                {
                    values.resize(1);
                },
                value.tracks[0].values);
            require(validateGtsAnimationClip(value).isValid(),
                    "One-key canonical cubic constant retains a complete triple");
        }
        auto value = cubicClip(GtsAnimationTarget::Translation);
        std::get<std::vector<GtsAnimationCubicVec3Key>>(value.tracks[0].values)[0].inTangent.x =
            std::numeric_limits<float>::infinity();
        requireError(validateGtsAnimationClip(value), "ANIMATION_TANGENT_NONFINITE", "tracks[0].values[0].inTangent");
        value = cubicClip(GtsAnimationTarget::Scale);
        std::get<std::vector<GtsAnimationCubicVec3Key>>(value.tracks[0].values)[1].value.z =
            std::numeric_limits<float>::quiet_NaN();
        requireError(validateGtsAnimationClip(value), "ANIMATION_VALUE_NONFINITE", "tracks[0].values[1].value");
        value                = cubicClip(GtsAnimationTarget::Rotation);
        auto& keys           = std::get<std::vector<GtsAnimationCubicRotationKey>>(value.tracks[0].values);
        keys[0].outTangent.w = std::numeric_limits<float>::quiet_NaN();
        keys[1].value        = glm::quat(0, 0, 0, 0);
        requireError(validateGtsAnimationClip(value), "ANIMATION_TANGENT_NONFINITE", "tracks[0].values[0].outTangent");
        requireError(validateGtsAnimationClip(value), "ANIMATION_ROTATION_NOT_UNIT", "tracks[0].values[1].value");
        keys.pop_back();
        requireError(validateGtsAnimationClip(value), "ANIMATION_KEY_COUNT", "tracks[0].values");
    }

    void timingAndCounts()
    {
        for (const float invalid :
             {-1.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        {
            auto value            = clip();
            value.durationSeconds = invalid;
            requireError(validateGtsAnimationClip(value), "ANIMATION_DURATION_INVALID", "durationSeconds");
            value                           = clip();
            value.tracks[0].timesSeconds[0] = invalid;
            requireError(validateGtsAnimationClip(value), "ANIMATION_TIME_INVALID", "tracks[0].timesSeconds[0]");
        }
        for (const auto times : {std::vector<float>{1, 0}, std::vector<float>{0, 0}})
        {
            auto value                    = clip();
            value.tracks[0].interpolation = GtsAnimationInterpolation::Step;
            value.tracks[0].timesSeconds  = times;
            requireError(validateGtsAnimationClip(value), "ANIMATION_TIME_ORDER", "tracks[0].timesSeconds[1]");
            require(value.tracks[0].timesSeconds == times, "Invalid timestamps are not sorted or repaired");
        }
        auto value                      = clip();
        value.tracks[0].timesSeconds[1] = std::nextafter(1.0f, 2.0f);
        requireError(validateGtsAnimationClip(value), "ANIMATION_TIME_INVALID", "tracks[0].timesSeconds[1]");
        value = clip();
        std::get<std::vector<glm::vec3>>(value.tracks[0].values).pop_back();
        requireError(validateGtsAnimationClip(value), "ANIMATION_KEY_COUNT", "tracks[0].values");
        value.tracks[0].timesSeconds.clear();
        requireError(validateGtsAnimationClip(value), "ANIMATION_KEYS_EMPTY", "tracks[0].timesSeconds");
        value.tracks.clear();
        value.durationSeconds = 0;
        requireError(validateGtsAnimationClip(value), "ANIMATION_TRACKS_EMPTY", "tracks");
    }

    void malformedValues()
    {
        auto value                                                    = clip();
        std::get<std::vector<glm::vec3>>(value.tracks[0].values)[0].y = std::numeric_limits<float>::infinity();
        requireError(validateGtsAnimationClip(value), "ANIMATION_VALUE_NONFINITE", "tracks[0].values[0]");
        for (const glm::quat rotation :
             {glm::quat(0, 0, 0, 0), glm::quat(2, 0, 0, 0), glm::quat(std::numeric_limits<float>::max(), 0, 0, 0)})
        {
            value                                                       = clip(GtsAnimationTarget::Rotation);
            std::get<std::vector<glm::quat>>(value.tracks[0].values)[0] = rotation;
            requireError(validateGtsAnimationClip(value), "ANIMATION_ROTATION_NOT_UNIT", "tracks[0].values[0]");
            require(std::get<std::vector<glm::quat>>(value.tracks[0].values)[0].w == rotation.w,
                    "Invalid rotations are not normalized");
        }
        value           = clip(GtsAnimationTarget::Rotation);
        auto& rotations = std::get<std::vector<glm::quat>>(value.tracks[0].values);
        rotations[0].w  = std::numeric_limits<float>::quiet_NaN();
        requireError(validateGtsAnimationClip(value), "ANIMATION_VALUE_NONFINITE", "tracks[0].values[0]");
        rotations[0] = glm::quat(1.00002f, 0, 0, 0);
        require(validateGtsAnimationClip(value).isValid(),
                "Squared-length roundoff tolerance matches skeleton defaults");
        require(rotations[0].w == 1.00002f, "Accepted near-unit quaternion is not normalized");
        rotations[0].w = 1.0001f;
        requireError(validateGtsAnimationClip(value), "ANIMATION_ROTATION_NOT_UNIT", "tracks[0].values[0]");

        for (const auto target :
             {GtsAnimationTarget::Translation, GtsAnimationTarget::Rotation, GtsAnimationTarget::Scale})
        {
            value = clip(target);
            if (target == GtsAnimationTarget::Rotation)
                value.tracks[0].values = std::vector<glm::vec3>(2);
            else
                value.tracks[0].values = std::vector<glm::quat>(2, glm::quat(1, 0, 0, 0));
            requireError(validateGtsAnimationClip(value), "ANIMATION_VALUES_TYPE", "tracks[0].values");
            value                         = clip(target);
            value.tracks[0].interpolation = GtsAnimationInterpolation::CubicSpline;
            requireError(validateGtsAnimationClip(value), "ANIMATION_VALUES_TYPE", "tracks[0].values");
            value                         = cubicClip(target);
            value.tracks[0].interpolation = GtsAnimationInterpolation::Linear;
            requireError(validateGtsAnimationClip(value), "ANIMATION_VALUES_TYPE", "tracks[0].values");
        }
        value                  = clip();
        value.tracks[0].target = static_cast<GtsAnimationTarget>(99);
        requireError(validateGtsAnimationClip(value), "ANIMATION_PROPERTY_INVALID", "tracks[0].target");
        value                         = clip();
        value.tracks[0].interpolation = static_cast<GtsAnimationInterpolation>(99);
        requireError(validateGtsAnimationClip(value), "ANIMATION_INTERPOLATION_INVALID", "tracks[0].interpolation");
    }

    void compatibilityAndTargets()
    {
        auto value = clip();
        auto rig   = skeleton();
        rig.name   = "renamed";
        for (auto& node : rig.nodes)
            node.name = "duplicate";
        require(validateGtsAnimationClip(value, rig).isValid(), "Display names do not affect compatibility");
        const auto expectMismatch = [&](const GtsSkeletonAsset& changed)
        {
            requireError(validateGtsAnimationClip(value, changed), "ANIMATION_SKELETON_INCOMPATIBLE", "skeleton");
        };
        rig.nodes[2].id.value = "other";
        expectMismatch(rig);
        rig                      = skeleton();
        rig.nodes[3].parentIndex = 0;
        expectMismatch(rig);
        rig                                                                        = skeleton();
        std::get<GtsSkeletonTrs>(rig.nodes[2].defaultLocalTransform).translation.x = 1;
        expectMismatch(rig);
        rig                                                                     = skeleton();
        std::get<GtsSkeletonTrs>(rig.nodes[2].defaultLocalTransform).rotation.w = -1;
        expectMismatch(rig);
        rig                                                                     = skeleton();
        std::get<GtsSkeletonTrs>(rig.nodes[2].defaultLocalTransform).rotation.x = 1e-7f;
        expectMismatch(rig);
        rig                                = skeleton();
        rig.nodes[2].defaultLocalTransform = glm::mat4(1);
        expectMismatch(rig);
        requireError(
            validateGtsAnimationClip(value, rig), "ANIMATION_CONTEXT_NODE_NOT_TRS", "tracks[0].skeletonNodeIndex");

        value.tracks[0].skeletonNodeIndex = 1;
        requireError(validateGtsAnimationClip(value), "ANIMATION_NODE_NOT_TRS", "tracks[0].skeletonNodeIndex");
        value.tracks[0].skeletonNodeIndex = 99;
        requireError(validateGtsAnimationClip(value), "ANIMATION_NODE_OUT_OF_RANGE", "tracks[0].skeletonNodeIndex");
        requireError(validateGtsAnimationClip(value, skeleton()),
                     "ANIMATION_CONTEXT_NODE_OUT_OF_RANGE",
                     "tracks[0].skeletonNodeIndex");
        value.tracks[0].skeletonNodeIndex = GtsAnimationTrack::InvalidSkeletonNodeIndex;
        requireError(validateGtsAnimationClip(value), "ANIMATION_NODE_UNASSIGNED", "tracks[0].skeletonNodeIndex");
        value                             = clip();
        value.tracks[0].skeletonNodeIndex = 0;
        require(validateGtsAnimationClip(value).isValid(), "Explicit node zero is a valid target");
        rig = skeleton();
        rig.nodes.clear();
        requireError(validateGtsAnimationClip(value, rig), "ANIMATION_SKELETON_INVALID", "skeleton.nodes");
        value.targetSkeletonCompatibility = {};
        requireError(validateGtsAnimationClip(value), "ANIMATION_TARGET_INVALID", "targetSkeletonCompatibility.nodes");
        auto records                      = compatibility(skeleton()).nodes();
        records[2].id                     = records[0].id;
        value.targetSkeletonCompatibility = GtsSkeletonCompatibility(std::move(records));
        requireError(
            validateGtsAnimationClip(value), "ANIMATION_TARGET_INVALID", "targetSkeletonCompatibility.nodes[2].id");
    }

    void lifetimeAndDeterminism()
    {
        auto                                  value = cubicClip(GtsAnimationTarget::Rotation);
        std::weak_ptr<const GtsSkeletonAsset> lifetime;
        {
            auto source                       = std::make_shared<const GtsSkeletonAsset>(skeleton());
            lifetime                          = source;
            value.targetSkeletonCompatibility = compatibility(*source);
        }
        require(lifetime.expired(), "Clip compatibility does not retain skeleton lifetime");
        const auto& immutableClip = value;
        const auto  rig           = skeleton();
        const auto  before        = compatibility(rig);
        require(validateGtsAnimationClip(immutableClip, rig).isValid(), "Clip survives source destruction");
        require(areGtsSkeletonCompatibilitiesEqual(before, compatibility(rig)), "Validation leaves skeleton unchanged");
        require(immutableClip.name == "motion" && immutableClip.durationSeconds == 1 &&
                    immutableClip.tracks[0].skeletonNodeIndex == 2,
                "Validation leaves clip metadata unchanged");
        const auto& keys = std::get<std::vector<GtsAnimationCubicRotationKey>>(immutableClip.tracks[0].values);
        require(keys[0].outTangent.w == 5 && keys[1].inTangent.x == -2 && keys[1].value.x == 1,
                "Validation preserves cubic components");

        value.tracks[0].timesSeconds = {0.75f, 0.5f};
        value.tracks.push_back(value.tracks[0]);
        const auto first  = validateGtsAnimationClip(value, rig);
        const auto second = validateGtsAnimationClip(value, rig);
        require(!first.isValid() && first.diagnostics.size() == second.diagnostics.size(),
                "Deterministic failure count");
        for (size_t i = 0; i < first.diagnostics.size(); ++i)
            require(first.diagnostics[i].code == second.diagnostics[i].code &&
                        first.diagnostics[i].message == second.diagnostics[i].message &&
                        first.diagnostics[i].location == second.diagnostics[i].location,
                    "Deterministic error order and content");
        require(value.tracks.size() == 2 && value.tracks[0].timesSeconds[0] == 0.75f,
                "Failed validation does not remove tracks or fix key times");
    }
} // namespace

int main()
{
    try
    {
        propertiesAndConstants();
        cubicData();
        timingAndCounts();
        malformedValues();
        compatibilityAndTargets();
        lifetimeAndDeterminism();
        std::puts("GtsAnimationClipAsset tests passed");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
