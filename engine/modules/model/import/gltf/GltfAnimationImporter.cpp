#include "GltfAnimationImporter.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <utility>

#include "GltfSkinImporter.h"
#include "GltfSourceReader.h"
#include "GtsAnimationClipValidation.h"
#include "GtsSkeletonAsset.h"

namespace gts::gltf
{
    namespace
    {
        struct AnimationSampler
        {
            uint32_t                  output;
            GtsAnimationInterpolation interpolation;
            std::vector<float>        times;
            std::string               location;
        };

        AnimationSampler
        readSampler(const SourceDocument& data, const GtsJsonValue& source, const std::string& location)
        {
            object(source, location);
            AnimationSampler result;
            result.location       = location;
            const auto inputIndex = uintField(source, "input", location);
            result.output         = uintField(source, "output", location);
            if (inputIndex >= data.accessors.size() || result.output >= data.accessors.size())
                fail("GLTF_ANIMATION_ACCESSOR", "Sampler references an invalid input/output accessor", location);
            const auto& input         = data.accessor(inputIndex);
            const auto  inputLocation = location + ".input.accessors[" + std::to_string(inputIndex) + "]";
            if (input.type != "SCALAR" || input.componentType != 5126 || input.normalized ||
                data.views[input.view].stride)
                fail("GLTF_ANIMATION_INPUT_TYPE",
                     "Animation times require tightly packed SCALAR FLOAT data",
                     inputLocation);
            const auto interpolation = stringField(source, "interpolation", location, "LINEAR");
            if (interpolation == "STEP")
                result.interpolation = GtsAnimationInterpolation::Step;
            else if (interpolation == "LINEAR")
                result.interpolation = GtsAnimationInterpolation::Linear;
            else if (interpolation == "CUBICSPLINE")
            {
                result.interpolation = GtsAnimationInterpolation::CubicSpline;
                if (input.count < 2)
                    fail("GLTF_ANIMATION_CUBIC_KEYS", "glTF CUBICSPLINE requires at least two keys", inputLocation);
            }
            else
                fail("GLTF_ANIMATION_INTERPOLATION", "Unsupported animation interpolation: " + interpolation, location);

            result.times.reserve(input.count);
            for (size_t k = 0; k < input.count; ++k)
            {
                const float time = data.floatComponent(input, k, 0);
                if (!std::isfinite(time) || time < 0 || (k && time <= result.times.back()))
                    fail("GLTF_ANIMATION_TIME",
                         "Key times must be finite, nonnegative and strictly increasing",
                         inputLocation + "[" + std::to_string(k) + "]");
                result.times.push_back(time);
            }
            const auto& inputJson = array(data.root.find("accessors"), "accessors")[inputIndex];
            const auto* minimum   = inputJson.find("min");
            const auto* maximum   = inputJson.find("max");
            if (!minimum || !maximum)
                fail("GLTF_ANIMATION_TIME_BOUNDS", "Animation input accessor requires min and max", inputLocation);
            if (vectorValue(*minimum, 1, inputLocation + ".min")[0] != result.times.front() ||
                vectorValue(*maximum, 1, inputLocation + ".max")[0] != result.times.back())
                fail("GLTF_ANIMATION_TIME_BOUNDS",
                     "Input min/max must match the decoded first/last key time",
                     inputLocation);
            return result;
        }

        void readValues(const SourceDocument&   data,
                        const AnimationSampler& sampler,
                        GtsAnimationTrack&      track,
                        const std::string&      channelLocation)
        {
            const auto& output             = data.accessor(sampler.output);
            const auto  location           = channelLocation + " -> " + sampler.location + ".output.accessors[" +
                                             std::to_string(sampler.output) + "]";
            const bool  rotation           = track.target == GtsAnimationTarget::Rotation;
            const bool  cubic              = sampler.interpolation == GtsAnimationInterpolation::CubicSpline;
            const bool  normalizedRotation = rotation && output.normalized &&
                                             (output.componentType == 5120 || output.componentType == 5121 ||
                                              output.componentType == 5122 || output.componentType == 5123);
            if (output.type != (rotation ? "VEC4" : "VEC3") ||
                !((output.componentType == 5126 && !output.normalized) || normalizedRotation) ||
                data.views[output.view].stride)
                fail("GLTF_ANIMATION_OUTPUT_TYPE",
                     "Animation output requires tightly packed VEC3 FLOAT, or VEC4 FLOAT/normalized 8/16-bit rotation "
                     "data",
                     location);
            const size_t factor = cubic ? 3 : 1;
            if (sampler.times.size() > std::numeric_limits<size_t>::max() / factor ||
                output.count != sampler.times.size() * factor)
                fail("GLTF_ANIMATION_OUTPUT_COUNT",
                     "Output count must be key count times " + std::to_string(factor) +
                         (cubic ? " (incoming/value/outgoing CUBICSPLINE triples)" : ""),
                     location);
            auto components = [&](size_t element)
            {
                glm::vec4 value(0);
                for (size_t c = 0; c < output.components; ++c)
                {
                    value[c] = data.floatComponent(output, element, c);
                    if (!std::isfinite(value[c]))
                        fail("GLTF_ANIMATION_VALUE",
                             "Animation values and derivatives must be finite",
                             location + "[" + std::to_string(element) + "]");
                }
                return value;
            };
            auto quaternion = [](const glm::vec4& xyzw)
            {
                return glm::quat(xyzw.w, xyzw.x, xyzw.y, xyzw.z);
            };
            if (cubic && rotation)
            {
                std::vector<GtsAnimationCubicRotationKey> keys;
                for (size_t k = 0; k < sampler.times.size(); ++k)
                    keys.push_back({components(k * 3), quaternion(components(k * 3 + 1)), components(k * 3 + 2)});
                track.values = std::move(keys);
            }
            else if (cubic)
            {
                std::vector<GtsAnimationCubicVec3Key> keys;
                for (size_t k = 0; k < sampler.times.size(); ++k)
                    keys.push_back({glm::vec3(components(k * 3)),
                                    glm::vec3(components(k * 3 + 1)),
                                    glm::vec3(components(k * 3 + 2))});
                track.values = std::move(keys);
            }
            else if (rotation)
            {
                std::vector<glm::quat> keys;
                for (size_t k = 0; k < sampler.times.size(); ++k)
                    keys.push_back(quaternion(components(k)));
                track.values = std::move(keys);
            }
            else
            {
                std::vector<glm::vec3> keys;
                for (size_t k = 0; k < sampler.times.size(); ++k)
                    keys.push_back(glm::vec3(components(k)));
                track.values = std::move(keys);
            }
        }

        struct AnimationChannel
        {
            uint32_t           node;
            uint32_t           sampler;
            GtsAnimationTarget target;
            std::string        location;
        };
    } // namespace

    std::vector<GtsAnimationClipAsset> importAnimations(const SourceDocument& data, const GltfSkinImportResult& skins)
    {
        const auto& sources = array(data.root.find("animations"), "animations");
        if (sources.empty())
            return {};
        // membership comes only from skin extraction, never names or compatibility
        const auto                              nodeCount = array(data.root.find("nodes"), "nodes").size();
        std::vector<std::map<size_t, uint32_t>> memberships(nodeCount);
        for (size_t definition = 0; definition < skins.skeletons.size(); ++definition)
            for (uint32_t node = 0; node < skins.sourceNodes[definition].size(); ++node)
                memberships[skins.sourceNodes[definition][node]].emplace(definition, node);

        std::vector<GtsAnimationClipAsset> clips;
        for (size_t i = 0; i < sources.size(); ++i)
        {
            const auto  location = "animations[" + std::to_string(i) + "]";
            const auto& source   = sources[i];
            object(source, location);
            GtsAnimationClipAsset clip;
            clip.name            = stringField(source, "name", location, "animation_" + std::to_string(i));
            const auto& channels = array(source.find("channels"), location + ".channels");
            if (channels.empty())
                fail("GLTF_ANIMATION_EMPTY", "Animation contains no supported skeletal tracks", location);
            std::vector<AnimationChannel>                     decodedChannels;
            std::set<size_t>                                  candidates;
            std::set<std::pair<uint32_t, GtsAnimationTarget>> targets;
            for (size_t c = 0; c < channels.size(); ++c)
            {
                const auto  channelLocation = location + ".channels[" + std::to_string(c) + "]";
                const auto& channel         = channels[c];
                object(channel, channelLocation);
                const auto* target = channel.find("target");
                if (!target)
                    fail("GLTF_ANIMATION_TARGET", "Channel requires a target", channelLocation);
                object(*target, channelLocation + ".target");
                const auto         path = stringField(*target, "path", channelLocation);
                GtsAnimationTarget property;
                if (path == "translation")
                    property = GtsAnimationTarget::Translation;
                else if (path == "rotation")
                    property = GtsAnimationTarget::Rotation;
                else if (path == "scale")
                    property = GtsAnimationTarget::Scale;
                else
                    fail("GLTF_ANIMATION_TARGET_UNSUPPORTED",
                         "Unsupported animation path: " + path + "; only skeletal TRS is represented",
                         channelLocation);
                if (!target->find("node"))
                    fail("GLTF_ANIMATION_TARGET_UNSUPPORTED",
                         "Channel has no source-node target; external/extension animation targeting is unsupported",
                         channelLocation);
                const auto node = uintField(*target, "node", channelLocation);
                if (node >= nodeCount)
                    fail("GLTF_ANIMATION_NODE",
                         "Invalid animation target node " + std::to_string(node),
                         channelLocation);
                if (memberships[node].empty())
                    fail("GLTF_ANIMATION_NOT_SKELETAL",
                         "Target node " + std::to_string(node) +
                             " belongs to no extracted skeleton; ordinary-node animation is unsupported",
                         channelLocation);
                if (!targets.emplace(node, property).second)
                    fail("GLTF_ANIMATION_DUPLICATE",
                         "Duplicate node " + std::to_string(node) + " / " + path + " channel",
                         channelLocation);
                if (c == 0)
                    for (const auto& [definition, skeletonNode] : memberships[node])
                        candidates.insert(definition);
                else
                    std::erase_if(candidates,
                                  [&](size_t definition)
                                  {
                                      return !memberships[node].contains(definition);
                                  });
                decodedChannels.push_back(
                    {node, uintField(channel, "sampler", channelLocation), property, channelLocation});
            }
            if (candidates.empty())
                fail("GLTF_ANIMATION_MULTIPLE_RIGS",
                     "Animation '" + clip.name +
                         "' channels span distinct source skeleton definitions; no common rig owns every target "
                         "(automatic splitting is unsupported)",
                     location);
            if (candidates.size() != 1)
                fail("GLTF_ANIMATION_AMBIGUOUS",
                     "Animation '" + clip.name +
                         "' targets shared nodes in multiple source rig definitions without enough channel context to "
                         "select one",
                     location);
            const auto  definition = *candidates.begin();
            const auto& skeleton   = *skins.skeletons[definition];
            const auto  contract   = makeGtsSkeletonCompatibility(skeleton);
            if (!contract.succeeded())
                fail("GLTF_ANIMATION_SKELETON", "Extracted animation target skeleton is invalid", location);
            clip.targetSkeletonCompatibility = *contract.compatibility();

            std::vector<AnimationSampler> samplers;
            const auto&                   sourceSamplers = array(source.find("samplers"), location + ".samplers");
            for (size_t s = 0; s < sourceSamplers.size(); ++s)
                samplers.push_back(
                    readSampler(data, sourceSamplers[s], location + ".samplers[" + std::to_string(s) + "]"));
            for (const auto& channel : decodedChannels)
            {
                if (channel.sampler >= samplers.size())
                    fail("GLTF_ANIMATION_SAMPLER",
                         "Invalid animation sampler " + std::to_string(channel.sampler),
                         channel.location);
                const auto&       sampler = samplers[channel.sampler];
                GtsAnimationTrack track;
                track.skeletonNodeIndex = memberships[channel.node].at(definition);
                if (!std::holds_alternative<GtsSkeletonTrs>(
                        skeleton.nodes[track.skeletonNodeIndex].defaultLocalTransform))
                    fail("GLTF_ANIMATION_MATRIX_NODE",
                         "Source node " + std::to_string(channel.node) +
                             " has an exact matrix default; TRS animation cannot decompose it",
                         channel.location);
                track.target        = channel.target;
                track.interpolation = sampler.interpolation;
                track.timesSeconds  = sampler.times;
                readValues(data, sampler, track, channel.location);
                clip.durationSeconds = std::max(clip.durationSeconds, track.timesSeconds.back());
                clip.tracks.push_back(std::move(track));
            }
            const auto validation = validateGtsAnimationClip(clip);
            if (!validation.isValid())
            {
                const auto& error         = validation.diagnostics.front();
                auto        errorLocation = location + "." + error.location;
                for (size_t c = 0; c < decodedChannels.size(); ++c)
                {
                    const auto prefix = "tracks[" + std::to_string(c) + "]";
                    if (error.location.starts_with(prefix))
                        errorLocation = decodedChannels[c].location + error.location.substr(prefix.size());
                }
                fail("GLTF_ANIMATION_INVALID", error.code + ": " + error.message, errorLocation);
            }
            clips.push_back(std::move(clip));
        }
        return clips;
    }
} // namespace gts::gltf
