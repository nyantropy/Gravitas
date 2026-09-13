#include "GtsAnimationPlayback.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "animation/skeletal/GtsSkeletonPoseEvaluation.h"
#include "assets/animation/GtsAnimationClipAsset.h"

void GtsAnimationPlayback::selectClip(uint32_t index)
{
    if (activeClip != index)
    {
        activeClip = index;
        timeSeconds = 0;
        pose.reset();
    }
}

void advanceGtsAnimationPlayback(GtsAnimationPlayback& playback, const GtsSkeletonAsset& skeleton,
                                 const GtsAnimationClipAsset& clip, double deltaSeconds)
{
    if (!playback.activeClip || !std::isfinite(deltaSeconds) || deltaSeconds < 0 ||
        !std::isfinite(playback.speed) || playback.speed < 0 || !std::isfinite(playback.timeSeconds) ||
        playback.timeSeconds < 0 || !std::isfinite(clip.durationSeconds) || clip.durationSeconds < 0)
        throw std::invalid_argument("Invalid animation playback time, speed, duration or unselected clip");
    double next = playback.timeSeconds + deltaSeconds * playback.speed;
    if (!std::isfinite(next)) throw std::invalid_argument("Animation playback time overflow");
    if (clip.durationSeconds == 0) next = 0;
    else if (playback.looping) next = std::fmod(next, static_cast<double>(clip.durationSeconds));
    else next = std::min(next, static_cast<double>(clip.durationSeconds));
    auto result = evaluateGtsAnimationPose(skeleton, clip, static_cast<float>(next));
    if (!result.succeeded())
        throw std::runtime_error("Animation playback evaluation failed: " + result.diagnostics().front().message);
    playback.pose = *result.pose();
    playback.timeSeconds = next;
}
