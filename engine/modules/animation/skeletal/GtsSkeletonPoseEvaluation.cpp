#include "GtsSkeletonPoseEvaluation.h"

#include <cmath>
#include <gtc/matrix_transform.hpp>

#include "GtsAnimationSampling.h"
#include "model/domain/animation/GtsAnimationClipAsset.h"
#include "model/domain/animation/GtsAnimationClipValidation.h"
#include "model/domain/skeleton/GtsSkeletonAsset.h"
#include "model/domain/skeleton/GtsSkeletonValidation.h"

namespace
{
    GtsSkeletonPose defaultLocals(const GtsSkeletonAsset& skeleton)
    {
        GtsSkeletonPose pose;
        // Both entry points validate the skeleton before constructing its pose.
        pose.skeletonCompatibility = *makeGtsSkeletonCompatibility(skeleton).compatibility();
        pose.localTransforms.reserve(skeleton.nodes.size());
        for (const auto& node : skeleton.nodes)
            pose.localTransforms.push_back(node.defaultLocalTransform);
        return pose;
    }

    std::optional<GtsSkeletonPoseEvaluationError> composeHierarchy(const GtsSkeletonAsset& skeleton,
                                                                   GtsSkeletonPose&        pose)
    {
        pose.modelTransforms.reserve(skeleton.nodes.size());
        for (size_t i = 0; i < skeleton.nodes.size(); ++i)
        {
            glm::mat4 local;
            if (const auto* trs = std::get_if<GtsSkeletonTrs>(&pose.localTransforms[i]))
                local = glm::translate(glm::mat4(1), trs->translation) * glm::mat4_cast(trs->rotation) *
                        glm::scale(glm::mat4(1), trs->scale);
            else
                local = std::get<glm::mat4>(pose.localTransforms[i]);
            const auto parent = skeleton.nodes[i].parentIndex;
            const auto model  = parent ? pose.modelTransforms[*parent] * local : local;
            for (glm::length_t c = 0; c < 4; ++c)
                for (glm::length_t r = 0; r < 4; ++r)
                    if (!std::isfinite(local[c][r]) || !std::isfinite(model[c][r]))
                        return GtsSkeletonPoseEvaluationError{"POSE_TRANSFORM_NONFINITE",
                                                              "Transform composition exceeded finite pose storage.",
                                                              "nodes[" + std::to_string(i) + "]"};
            pose.modelTransforms.push_back(model);
        }
        return std::nullopt;
    }
} // namespace

GtsSkeletonPoseEvaluationResult evaluateGtsDefaultPose(const GtsSkeletonAsset& skeleton)
{
    std::vector<GtsSkeletonPoseEvaluationError> errors;
    for (const auto& error : validateGtsSkeletonAsset(skeleton).diagnostics)
        errors.push_back({error.code, error.message, "skeleton." + error.location});
    if (!errors.empty())
        return {std::nullopt, std::move(errors)};
    auto pose = defaultLocals(skeleton);
    if (const auto error = composeHierarchy(skeleton, pose))
        return {std::nullopt, {*error}};
    return {std::move(pose), {}};
}

GtsSkeletonPoseEvaluationResult
evaluateGtsAnimationPose(const GtsSkeletonAsset& skeleton, const GtsAnimationClipAsset& clip, float timeSeconds)
{
    std::vector<GtsSkeletonPoseEvaluationError> errors;
    for (const auto& error : validateGtsAnimationClip(clip, skeleton).diagnostics)
        errors.push_back({error.code, error.message, error.location});
    if (!std::isfinite(timeSeconds) || timeSeconds < 0.0f || timeSeconds > clip.durationSeconds)
        errors.push_back({"POSE_TIME_INVALID", "Time must be finite and within [0, clip duration].", "timeSeconds"});
    if (!errors.empty())
        return {std::nullopt, std::move(errors)};

    auto pose = defaultLocals(skeleton);
    for (size_t i = 0; i < clip.tracks.size(); ++i)
    {
        const auto& track   = clip.tracks[i];
        const auto  sampled = gts::animation::detail::sampleTrack(track, timeSeconds);
        if (!sampled)
            return {std::nullopt,
                    {{"POSE_SAMPLE_INVALID",
                      "Interpolation produced a nonfinite value or zero-length rotation.",
                      "tracks[" + std::to_string(i) + "]"}}};
        auto& local = std::get<GtsSkeletonTrs>(pose.localTransforms[track.skeletonNodeIndex]);
        switch (track.target)
        {
        case GtsAnimationTarget::Translation:
            local.translation = std::get<glm::vec3>(*sampled);
            break;
        case GtsAnimationTarget::Rotation:
            local.rotation = std::get<glm::quat>(*sampled);
            break;
        case GtsAnimationTarget::Scale:
            local.scale = std::get<glm::vec3>(*sampled);
            break;
        }
    }
    if (const auto error = composeHierarchy(skeleton, pose))
        return {std::nullopt, {*error}};
    return {std::move(pose), {}};
}
