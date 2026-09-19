#include "animation/skeletal/runtime/GtsAnimationPlayback.h"
#include "model/domain/animation/GtsAnimationClipAsset.h"
#include "model/domain/skeleton/GtsSkeletonAsset.h"
#include <cmath>
#include <limits>
#include <stdexcept>

void require(bool condition)
{
    if (!condition) throw std::runtime_error("Playback invariant failed");
}
int main()
{
    GtsSkeletonAsset skeleton;
    skeleton.nodes = {{{"root"}, "Root", std::nullopt, GtsSkeletonTrs{}}};
    GtsAnimationClipAsset clip;
    clip.targetSkeletonCompatibility = *makeGtsSkeletonCompatibility(skeleton).compatibility();
    clip.durationSeconds = 2;
    GtsAnimationTrack track;
    track.skeletonNodeIndex = 0;
    track.target = GtsAnimationTarget::Translation;
    track.timesSeconds = {0, 2};
    track.values = std::vector<glm::vec3>{{0, 0, 0}, {2, 0, 0}};
    clip.tracks = {track};
    GtsAnimationPlayback a, b;
    a.selectClip(0);
    b.selectClip(0);
    advanceGtsAnimationPlayback(a, skeleton, clip, 0.5);
    advanceGtsAnimationPlayback(b, skeleton, clip, 1.5);
    require(a.timeSeconds == 0.5 && b.timeSeconds == 1.5);
    require(a.pose->modelTransforms[0][3].x == 0.5f && b.pose->modelTransforms[0][3].x == 1.5f);
    a.selectClip(0);
    require(a.timeSeconds == 0.5);
    advanceGtsAnimationPlayback(a, skeleton, clip, 4);
    require(a.timeSeconds == 0.5);
    a.selectClip(1);
    require(a.timeSeconds == 0 && !a.pose);
    a.selectClip(0);
    require(a.timeSeconds == 0);
    a.speed = 2;
    advanceGtsAnimationPlayback(a, skeleton, clip, 0.25);
    require(a.timeSeconds == 0.5);
    a.looping = false;
    advanceGtsAnimationPlayback(a, skeleton, clip, 3);
    require(a.timeSeconds == 2);
    const auto before = a.timeSeconds;
    for (double invalid : {-1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
    {
        bool failed = false;
        try { advanceGtsAnimationPlayback(a, skeleton, clip, invalid); }
        catch (const std::invalid_argument&) { failed = true; }
        require(failed && a.timeSeconds == before);
    }
    clip.durationSeconds = 0;
    clip.tracks[0].timesSeconds = {0};
    clip.tracks[0].values = std::vector<glm::vec3>{{3, 0, 0}};
    a.looping = true;
    advanceGtsAnimationPlayback(a, skeleton, clip, 100);
    require(a.timeSeconds == 0 && a.pose->modelTransforms[0][3].x == 3);
    require(skeleton.nodes[0].name == "Root" && std::get<std::vector<glm::vec3>>(clip.tracks[0].values)[0].x == 3);
}
