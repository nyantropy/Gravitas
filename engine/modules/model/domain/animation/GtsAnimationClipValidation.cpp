#include "GtsAnimationClipValidation.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <set>
#include <utility>

#include "GtsAnimationClipAsset.h"
#include "model/domain/skeleton/GtsSkeletonAsset.h"
#include "model/domain/skeleton/GtsSkeletonValidation.h"

namespace
{
    template <class T> bool isFinite(const T& value)
    {
        for (glm::length_t i = 0; i < value.length(); ++i)
            if (!std::isfinite(value[i]))
                return false;
        return true;
    }

    void validateValue(const glm::vec3& value, const std::string& location, GtsAnimationClipValidationResult& result)
    {
        if (!isFinite(value))
            result.diagnostics.push_back({"ANIMATION_VALUE_NONFINITE", "Key value must be finite.", location});
    }

    void validateValue(const glm::quat& value, const std::string& location, GtsAnimationClipValidationResult& result)
    {
        if (!isFinite(value))
        {
            result.diagnostics.push_back({"ANIMATION_VALUE_NONFINITE", "Rotation key must be finite.", location});
            return;
        }
        double squaredLength = 0.0;
        for (glm::length_t i = 0; i < value.length(); ++i)
        {
            const double component = value[i];
            squaredLength += component * component;
        }
        // same validity threshold as skeleton defaults; never normalize or change signs
        if (std::abs(squaredLength - 1.0) > 0.0001)
            result.diagnostics.push_back({"ANIMATION_ROTATION_NOT_UNIT",
                                          "Rotation key must have unit magnitude (squared-length tolerance 1e-4).",
                                          location});
    }

    template <class Key>
    void validateCubicKey(const Key& key, const std::string& location, GtsAnimationClipValidationResult& result)
    {
        if (!isFinite(key.inTangent))
            result.diagnostics.push_back(
                {"ANIMATION_TANGENT_NONFINITE", "Incoming derivative must be finite.", location + ".inTangent"});
        validateValue(key.value, location + ".value", result);
        if (!isFinite(key.outTangent))
            result.diagnostics.push_back(
                {"ANIMATION_TANGENT_NONFINITE", "Outgoing derivative must be finite.", location + ".outTangent"});
    }

    void validateValue(const GtsAnimationCubicVec3Key&   key,
                       const std::string&                location,
                       GtsAnimationClipValidationResult& result)
    {
        validateCubicKey(key, location, result);
    }

    void validateValue(const GtsAnimationCubicRotationKey& key,
                       const std::string&                  location,
                       GtsAnimationClipValidationResult&   result)
    {
        validateCubicKey(key, location, result);
    }
} // namespace

GtsAnimationClipValidationResult validateGtsAnimationClip(const GtsAnimationClipAsset& clip)
{
    GtsAnimationClipValidationResult result;
    for (const auto& error : validateGtsSkeletonCompatibility(clip.targetSkeletonCompatibility).diagnostics)
        result.diagnostics.push_back({"ANIMATION_TARGET_INVALID",
                                      error.code + ": " + error.message,
                                      "targetSkeletonCompatibility." + error.location});

    if (!std::isfinite(clip.durationSeconds) || clip.durationSeconds < 0.0f)
        result.diagnostics.push_back(
            {"ANIMATION_DURATION_INVALID", "Duration must be finite and nonnegative.", "durationSeconds"});
    if (clip.tracks.empty())
        result.diagnostics.push_back({"ANIMATION_TRACKS_EMPTY", "A clip requires at least one track.", "tracks"});

    const auto&                                       nodes = clip.targetSkeletonCompatibility.nodes();
    std::set<std::pair<uint32_t, GtsAnimationTarget>> targets;
    for (size_t i = 0; i < clip.tracks.size(); ++i)
    {
        const auto& track    = clip.tracks[i];
        const auto  location = "tracks[" + std::to_string(i) + "]";
        const auto  error    = [&](const char* code, const char* message, const char* field)
        {
            result.diagnostics.push_back({code, message, location + "." + field});
        };
        if (track.skeletonNodeIndex == GtsAnimationTrack::InvalidSkeletonNodeIndex)
            error(
                "ANIMATION_NODE_UNASSIGNED", "A track requires an explicit skeleton node target.", "skeletonNodeIndex");
        else if (track.skeletonNodeIndex >= nodes.size())
            error("ANIMATION_NODE_OUT_OF_RANGE",
                  "Track target is outside the compatibility contract.",
                  "skeletonNodeIndex");
        else if (!std::holds_alternative<GtsSkeletonTrs>(nodes[track.skeletonNodeIndex].defaultLocalTransform))
            error("ANIMATION_NODE_NOT_TRS",
                  "TRS tracks require a TRS default transform; matrices are not decomposed.",
                  "skeletonNodeIndex");

        bool validTarget = true;
        switch (track.target)
        {
        case GtsAnimationTarget::Translation:
        case GtsAnimationTarget::Rotation:
        case GtsAnimationTarget::Scale:
            if (!targets.emplace(track.skeletonNodeIndex, track.target).second)
                error("ANIMATION_TRACK_DUPLICATE", "A node/property pair may have only one track.", "target");
            break;
        default:
            validTarget = false;
            error("ANIMATION_PROPERTY_INVALID", "Unknown animated property.", "target");
            break;
        }
        bool validInterpolation = true;
        switch (track.interpolation)
        {
        case GtsAnimationInterpolation::Step:
        case GtsAnimationInterpolation::Linear:
        case GtsAnimationInterpolation::CubicSpline:
            break;
        default:
            validInterpolation = false;
            error("ANIMATION_INTERPOLATION_INVALID", "Unknown interpolation mode.", "interpolation");
            break;
        }
        if (track.timesSeconds.empty())
            error("ANIMATION_KEYS_EMPTY", "A track requires at least one key time.", "timesSeconds");
        for (size_t k = 0; k < track.timesSeconds.size(); ++k)
        {
            const float time         = track.timesSeconds[k];
            const auto  timeLocation = location + ".timesSeconds[" + std::to_string(k) + "]";
            if (!std::isfinite(time) || time < 0.0f || time > clip.durationSeconds)
                result.diagnostics.push_back(
                    {"ANIMATION_TIME_INVALID", "Key time must be finite and within [0, duration].", timeLocation});
            if (k > 0 && time <= track.timesSeconds[k - 1])
                result.diagnostics.push_back(
                    {"ANIMATION_TIME_ORDER", "Key times must be strictly increasing.", timeLocation});
        }

        if (track.values.valueless_by_exception())
        {
            error("ANIMATION_VALUES_INVALID", "Track must contain typed key data.", "values");
            continue;
        }
        if (validTarget && validInterpolation)
        {
            const bool rotation = track.target == GtsAnimationTarget::Rotation;
            const bool cubic    = track.interpolation == GtsAnimationInterpolation::CubicSpline;
            const bool matches =
                cubic ? (rotation ? std::holds_alternative<std::vector<GtsAnimationCubicRotationKey>>(track.values)
                                  : std::holds_alternative<std::vector<GtsAnimationCubicVec3Key>>(track.values))
                      : (rotation ? std::holds_alternative<std::vector<glm::quat>>(track.values)
                                  : std::holds_alternative<std::vector<glm::vec3>>(track.values));
            if (!matches)
                error("ANIMATION_VALUES_TYPE",
                      "Key data must match the animated property and interpolation mode.",
                      "values");
        }
        std::visit(
            [&](const auto& values)
            {
                if (values.size() != track.timesSeconds.size())
                    error("ANIMATION_KEY_COUNT", "Each key time requires exactly one value or cubic triple.", "values");
                for (size_t k = 0; k < values.size(); ++k)
                    validateValue(values[k], location + ".values[" + std::to_string(k) + "]", result);
            },
            track.values);
    }
    return result;
}

GtsAnimationClipValidationResult validateGtsAnimationClip(const GtsAnimationClipAsset& clip,
                                                          const GtsSkeletonAsset&      skeleton)
{
    auto       result  = validateGtsAnimationClip(clip);
    const auto derived = makeGtsSkeletonCompatibility(skeleton);
    for (const auto& error : derived.diagnostics())
        result.diagnostics.push_back(
            {"ANIMATION_SKELETON_INVALID", error.code + ": " + error.message, "skeleton." + error.location});
    if (derived.succeeded() && validateGtsSkeletonCompatibility(clip.targetSkeletonCompatibility).isValid() &&
        !areGtsSkeletonCompatibilitiesEqual(clip.targetSkeletonCompatibility, *derived.compatibility()))
        result.diagnostics.push_back({"ANIMATION_SKELETON_INCOMPATIBLE",
                                      "Supplied skeleton does not match the exact target contract.",
                                      "skeleton"});

    for (size_t i = 0; i < clip.tracks.size(); ++i)
    {
        const auto index = clip.tracks[i].skeletonNodeIndex;
        if (index == GtsAnimationTrack::InvalidSkeletonNodeIndex)
            continue;
        const auto location = "tracks[" + std::to_string(i) + "].skeletonNodeIndex";
        if (index >= skeleton.nodes.size())
            result.diagnostics.push_back(
                {"ANIMATION_CONTEXT_NODE_OUT_OF_RANGE", "Track does not target a supplied skeleton node.", location});
        else if (!std::holds_alternative<GtsSkeletonTrs>(skeleton.nodes[index].defaultLocalTransform))
            result.diagnostics.push_back({"ANIMATION_CONTEXT_NODE_NOT_TRS",
                                          "Supplied target node must have a TRS default transform.",
                                          location});
    }
    return result;
}
