#include "GltfFixtureBuilder.h"

#include <functional>
#include <limits>
#include <type_traits>

#include "GltfAnimationImporter.h"
#include "GltfSkinImporter.h"
#include "GltfSourceReader.h"
#include "GtsModelImportBundleValidation.h"
#include "GtsSkeletonAsset.h"

namespace
{
    GltfFixtureBuilder makeAnimationRig()
    {
        GltfFixtureBuilder f;
        f.addVertexStream("JOINTS_0", std::vector<uint8_t>(12, 0), "VEC4", 5121);
        f.addVertexStream("WEIGHTS_0", floats({1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}), "VEC4", 5126);
        // Source node 1 becomes evaluation node 2. Node 2 is an unweighted helper.
        field(f.root, "nodes")  = parse(R"([
            {"mesh":0,"skin":0}, {"name":"same"},
            {"name":"helper","children":[3]}, {"name":"same","children":[1]}, {}
        ])");
        field(f.root, "skins")  = parse(R"([{"joints":[1,3]}])");
        field(f.root, "scenes") = parse(R"([{"nodes":[0,2,4]}])");
        return f;
    }

    uint32_t addTrack(GltfFixtureBuilder& f,
                      const std::string&  path,
                      uint32_t            node          = 1,
                      const std::string&  interpolation = "LINEAR",
                      uint32_t            animation     = 0)
    {
        const auto input    = f.addAnimationTimes({0, 1});
        const bool rotation = path == "rotation";
        const bool cubic    = interpolation == "CUBICSPLINE";
        const auto bytes =
            rotation ? (cubic ? floats({0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, -2, -3, -4, -5, 1, 0, 0, 0, 0, 0, 0, 0})
                              : floats({0, 0, 0, 1, 0.5f, -0.5f, 0.5f, -0.5f}))
                     : (cubic ? floats({0, 0, 0, 1, 2, 3, 4, 5, 6, -4, -5, -6, 7, 8, 9, 0, 0, 0})
                              : floats({1, 2, 3, 4, 5, 6}));
        const auto output  = f.addAccessor(bytes, rotation ? "VEC4" : "VEC3", 5126, cubic ? 6 : 2);
        const auto sampler = f.addAnimationSampler(animation, input, output, interpolation);
        f.addAnimationChannel(animation, sampler, node, path);
        return sampler;
    }

    GltfFixtureBuilder makeAnimatedRig(const std::string& path          = "translation",
                                       const std::string& interpolation = "LINEAR")
    {
        auto f = makeAnimationRig();
        f.addAnimation("Walk");
        addTrack(f, path, 1, interpolation);
        return f;
    }

    Json& animationSampler(GltfFixtureBuilder& f, size_t index = 0)
    {
        return at(field(at(field(f.root, "animations"), 0), "samplers"), index);
    }
    Json& animationChannel(GltfFixtureBuilder& f, size_t index = 0)
    {
        return at(field(at(field(f.root, "animations"), 0), "channels"), index);
    }
    Json& samplerAccessor(GltfFixtureBuilder& f, const char* role)
    {
        return at(field(f.root, "accessors"), static_cast<size_t>(field(animationSampler(f), role).asNumber()));
    }
    void setAccessorFloat(GltfFixtureBuilder& f, const char* role, size_t component, float value)
    {
        auto&      accessor = samplerAccessor(f, role);
        const auto view     = static_cast<size_t>(field(accessor, "bufferView").asNumber());
        const auto offset = static_cast<size_t>(field(at(field(f.root, "bufferViews"), view), "byteOffset").asNumber());
        const auto bytes  = floats({value});
        std::copy(bytes.begin(), bytes.end(), f.bin.begin() + offset + component * 4);
    }

    const GtsModelImportBundle& requireAnimatedImport(const GtsModelImportResult& result, size_t clips = 1)
    {
        std::string errors;
        for (const auto& diagnostic : result.diagnostics())
            errors += diagnostic.code + ": " + diagnostic.message + " @ " + diagnostic.location + "\n";
        require(result.succeeded() && result.bundle() && result.asset(), errors);
        require(result.bundle()->animationClips.size() == clips && !result.bundle()->skeletons.empty(),
                "Clip and definition products are exposed");
        require(validateGtsModelImportBundle(*result.bundle()).isValid(), "Animated bundle validates completely");
        return *result.bundle();
    }

    void valuesAndFormats(const std::filesystem::path& root)
    {
        for (const auto format : {GltfFixtureFormat::External, GltfFixtureFormat::DataUri, GltfFixtureFormat::Glb})
            for (const auto path : {"translation", "rotation", "scale"})
                for (const auto interpolation : {"STEP", "LINEAR", "CUBICSPLINE"})
                {
                    auto        f      = makeAnimatedRig(path, interpolation);
                    const auto  result = importFixture(f, root, format);
                    const auto& bundle = requireAnimatedImport(result);
                    const auto& clip   = bundle.animationClips[0];
                    const auto& track  = clip.tracks[0];
                    require(clip.name == "Walk" && clip.durationSeconds == 1 &&
                                track.timesSeconds == std::vector<float>({0, 1}),
                            "Name, duration and times preserved");
                    require(track.skeletonNodeIndex == 2,
                            "Source node 1 maps to skeleton evaluation node 2, never raw source index");
                    require(isGtsSkeletonCompatible(*bundle.skeletons[0], clip.targetSkeletonCompatibility),
                            "Clip targets emitted structure");
                    const bool rotation = std::string(path) == "rotation";
                    require(track.target == (rotation                       ? GtsAnimationTarget::Rotation
                                             : std::string(path) == "scale" ? GtsAnimationTarget::Scale
                                                                            : GtsAnimationTarget::Translation),
                            "Typed property preserved");
                    const bool cubic = std::string(interpolation) == "CUBICSPLINE";
                    require(track.interpolation == (cubic ? GtsAnimationInterpolation::CubicSpline
                                                    : std::string(interpolation) == "STEP"
                                                        ? GtsAnimationInterpolation::Step
                                                        : GtsAnimationInterpolation::Linear),
                            "Interpolation preserved");
                    if (rotation && cubic)
                    {
                        const auto& keys = std::get<std::vector<GtsAnimationCubicRotationKey>>(track.values);
                        require(keys[0].value.w == 1 && keys[1].value.x == 1 && keys[1].value.w == 0,
                                "XYZW rotation values become correct GLM quaternions");
                        require(keys[0].outTangent.x == 2 && keys[0].outTangent.w == 5 && keys[1].inTangent.z == -4,
                                "Non-unit XYZW derivatives remain unnormalized and unscaled");
                    }
                    else if (rotation)
                    {
                        const auto& keys = std::get<std::vector<glm::quat>>(track.values);
                        require(keys[1].x == 0.5f && keys[1].y == -0.5f && keys[1].z == 0.5f && keys[1].w == -0.5f,
                                "All quaternion components and signs preserved");
                    }
                    else if (cubic)
                    {
                        const auto& keys = std::get<std::vector<GtsAnimationCubicVec3Key>>(track.values);
                        require(keys[0].value.y == 2 && keys[0].outTangent.z == 6 && keys[1].inTangent.x == -4,
                                "Vector cubic triples retained");
                    }
                    else
                        require(std::get<std::vector<glm::vec3>>(track.values)[1].z == 6,
                                "Source axes and vector values preserved");
                }
        auto f = makeAnimatedRig();
        erase(animationSampler(f), "interpolation");
        erase(at(field(f.root, "animations"), 0), "name");
        const auto unnamed = importFixture(f, root);
        require(requireAnimatedImport(unnamed).animationClips[0].name == "animation_0",
                "Unnamed source gets deterministic index name");
        require(unnamed.bundle()->animationClips[0].tracks[0].interpolation == GtsAnimationInterpolation::Linear,
                "Omitted interpolation defaults to LINEAR");

        f = makeAnimatedRig("rotation");
        for (const uint32_t component : {5120u, 5121u, 5122u, 5123u})
        {
            const bool           wide            = component >= 5122;
            const bool           signedComponent = component == 5120 || component == 5122;
            std::vector<uint8_t> bytes(8 * (wide ? 2 : 1), 0);
            for (size_t key = 0; key < 2; ++key)
            {
                const size_t index = (key * 4 + 3) * (wide ? 2 : 1);
                bytes[index]       = wide ? 255 : signedComponent ? 127 : 255;
                if (wide)
                    bytes[index + 1] = signedComponent ? 127 : 255;
            }
            const auto output                    = f.addAccessor(bytes, "VEC4", component, 2, true);
            field(animationSampler(f), "output") = output;
            const auto normalized                = importFixture(f, root);
            require(std::get<std::vector<glm::quat>>(
                        requireAnimatedImport(normalized).animationClips[0].tracks[0].values)[0]
                            .w == 1,
                    "Valid normalized signed/unsigned 8/16-bit rotation encoding is decoded");
        }
    }

    void timingAndReuse(const std::filesystem::path& root)
    {
        auto f = makeAnimatedRig();
        addTrack(f, "rotation");
        const auto times                       = f.addAnimationTimes({0.25f, 2.5f});
        field(animationSampler(f, 1), "input") = times;
        addTrack(f, "scale");
        const auto constantTimes                = f.addAnimationTimes({0.5f});
        const auto constant                     = f.addAccessor(floats({0, -1, 2}), "VEC3", 5126, 1);
        field(animationSampler(f, 2), "input")  = constantTimes;
        field(animationSampler(f, 2), "output") = constant;
        f.addAnimationChannel(0, 0, 2, "translation");
        f.addAnimationChannel(0, 1, 3, "rotation");
        const auto  result = importFixture(f, root);
        const auto& clip   = requireAnimatedImport(result).animationClips[0];
        require(clip.tracks.size() == 5 && clip.durationSeconds == 2.5f,
                "Duration is maximum last key across independent channels");
        require(clip.tracks[0].timesSeconds == std::vector<float>({0, 1}) &&
                    clip.tracks[1].timesSeconds == std::vector<float>({0.25f, 2.5f}) &&
                    clip.tracks[2].timesSeconds == std::vector<float>({0.5f}),
                "Independent TRS timing remains intact");
        require(clip.tracks[3].skeletonNodeIndex == 0 && clip.tracks[4].skeletonNodeIndex == 1,
                "Unweighted helper and another joint resolve by source membership");
        require(std::get<std::vector<glm::vec3>>(clip.tracks[2].values)[0].y == -1, "Scale is not clamped");
        const auto  repeated = importFixture(f, root, GltfFixtureFormat::Glb);
        const auto& again    = requireAnimatedImport(repeated).animationClips[0];
        require(
            areGtsSkeletonCompatibilitiesEqual(clip.targetSkeletonCompatibility, again.targetSkeletonCompatibility) &&
                clip.name == again.name && clip.durationSeconds == again.durationSeconds,
            "Repeated formats preserve exact target contract and clip metadata");
        for (size_t i = 0; i < clip.tracks.size(); ++i)
        {
            const auto& a = clip.tracks[i];
            const auto& b = again.tracks[i];
            require(a.skeletonNodeIndex == b.skeletonNodeIndex && a.target == b.target &&
                        a.interpolation == b.interpolation && a.timesSeconds == b.timesSeconds &&
                        a.values.index() == b.values.index(),
                    "Repeated track structure is deterministic");
            std::visit(
                [&](const auto& values)
                {
                    const auto& other = std::get<std::decay_t<decltype(values)>>(b.values);
                    require(values.size() == other.size(), "Repeated key counts match");
                    using Value = typename std::decay_t<decltype(values)>::value_type;
                    if constexpr (std::is_same_v<Value, glm::vec3> || std::is_same_v<Value, glm::quat>)
                        for (size_t k = 0; k < values.size(); ++k)
                            for (glm::length_t c = 0; c < values[k].length(); ++c)
                                require(values[k][c] == other[k][c], "Repeated numeric components match exactly");
                },
                a.values);
        }
        const auto second = f.addAnimation();
        addTrack(f, "translation", 2, "STEP", second);
        const auto several = importFixture(f, root);
        require(requireAnimatedImport(several, 2).animationClips[1].name == "animation_1",
                "Multiple source clips retain order");
        // A later unsupported channel invalidates earlier otherwise valid clips too.
        f.addAnimationChannel(second, 0, 4, "translation");
        requireImportFailure(f, root, "GLTF_ANIMATION_NOT_SKELETAL");
    }

    void malformedAnimation(const std::filesystem::path& root)
    {
        using Mutation = std::pair<std::string, std::function<void(GltfFixtureBuilder&)>>;
        const std::vector<Mutation> mutations{
            {"GLTF_ANIMATION_DUPLICATE",
             [](auto& f)
             {
                 f.addAnimationChannel(0, 0, 1, "translation");
             }},
            {"GLTF_ANIMATION_SAMPLER",
             [](auto& f)
             {
                 field(animationChannel(f), "sampler") = 99u;
             }},
            {"GLTF_ANIMATION_NODE",
             [](auto& f)
             {
                 field(field(animationChannel(f), "target"), "node") = 99u;
             }},
            {"GLTF_ANIMATION_ACCESSOR",
             [](auto& f)
             {
                 field(animationSampler(f), "input") = 99u;
             }},
            {"GLTF_ANIMATION_ACCESSOR",
             [](auto& f)
             {
                 field(animationSampler(f), "output") = 99u;
             }},
            {"GLTF_ANIMATION_INPUT_TYPE",
             [](auto& f)
             {
                 field(samplerAccessor(f, "input"), "componentType") = 5125u;
             }},
            {"GLTF_ANIMATION_INPUT_TYPE",
             [](auto& f)
             {
                 field(samplerAccessor(f, "input"), "type")  = "VEC2";
                 field(samplerAccessor(f, "input"), "count") = 1u;
             }},
            {"GLTF_ANIMATION_OUTPUT_TYPE",
             [](auto& f)
             {
                 field(samplerAccessor(f, "output"), "type") = "VEC2";
             }},
            {"GLTF_ANIMATION_OUTPUT_TYPE",
             [](auto& f)
             {
                 field(samplerAccessor(f, "output"), "componentType") = 5123u;
             }},
            {"GLTF_ANIMATION_OUTPUT_COUNT",
             [](auto& f)
             {
                 field(samplerAccessor(f, "output"), "count") = 1u;
             }},
            {"GLTF_ANIMATION_TIME",
             [](auto& f)
             {
                 setAccessorFloat(f, "input", 1, 0);
             }},
            {"GLTF_ANIMATION_TIME",
             [](auto& f)
             {
                 setAccessorFloat(f, "input", 0, 2);
             }},
            {"GLTF_ANIMATION_TIME",
             [](auto& f)
             {
                 setAccessorFloat(f, "input", 0, -1);
             }},
            {"GLTF_ANIMATION_TIME",
             [](auto& f)
             {
                 setAccessorFloat(f, "input", 0, std::numeric_limits<float>::infinity());
             }},
            {"GLTF_ANIMATION_TIME",
             [](auto& f)
             {
                 setAccessorFloat(f, "input", 1, std::numeric_limits<float>::quiet_NaN());
             }},
            {"GLTF_ANIMATION_TIME_BOUNDS",
             [](auto& f)
             {
                 erase(samplerAccessor(f, "input"), "min");
             }},
            {"GLTF_ANIMATION_TIME_BOUNDS",
             [](auto& f)
             {
                 field(samplerAccessor(f, "input"), "max") = parse("[2]");
             }},
            {"GLTF_ANIMATION_VALUE",
             [](auto& f)
             {
                 setAccessorFloat(f, "output", 0, std::numeric_limits<float>::infinity());
             }},
            {"GLTF_ANIMATION_INTERPOLATION",
             [](auto& f)
             {
                 field(animationSampler(f), "interpolation") = "BEZIER";
             }},
            {"GLTF_ANIMATION_MATRIX_NODE",
             [](auto& f)
             {
                 field(at(field(f.root, "nodes"), 1), "matrix") = parse("[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]");
             }},
            {"GLTF_ANIMATION_NOT_SKELETAL",
             [](auto& f)
             {
                 field(field(animationChannel(f), "target"), "node") = 4u;
             }},
            {"GLTF_ANIMATION_NOT_SKELETAL",
             [](auto& f)
             {
                 f.addAnimationChannel(0, 0, 4, "translation");
             }},
            {"GLTF_ANIMATION_TARGET_UNSUPPORTED",
             [](auto& f)
             {
                 field(field(animationChannel(f), "target"), "path") = "weights";
             }},
            {"GLTF_ANIMATION_TARGET_UNSUPPORTED",
             [](auto& f)
             {
                 erase(field(animationChannel(f), "target"), "node");
             }},
            {"GLTF_ANIMATION_EMPTY",
             [](auto& f)
             {
                 field(at(field(f.root, "animations"), 0), "channels") = Array{};
             }},
            {"GLTF_SPARSE_UNSUPPORTED",
             [](auto& f)
             {
                 field(samplerAccessor(f, "input"), "sparse") = parse("{}");
             }},
            {"GLTF_ACCESSOR_SOURCE_UNSUPPORTED",
             [](auto& f)
             {
                 erase(samplerAccessor(f, "output"), "bufferView");
             }},
            {"GLTF_ACCESSOR_RANGE",
             [](auto& f)
             {
                 field(samplerAccessor(f, "output"), "count") = 99u;
             }}};
        for (const auto& [code, mutate] : mutations)
        {
            auto f = makeAnimatedRig();
            mutate(f);
            requireImportFailure(f, root, code);
        }
        auto f = makeAnimatedRig("rotation");
        setAccessorFloat(f, "output", 3, 0);
        requireImportFailure(f, root, "GLTF_ANIMATION_INVALID");
        for (const auto path : {"translation", "rotation", "scale"})
        {
            f                                            = makeAnimatedRig(path, "CUBICSPLINE");
            field(samplerAccessor(f, "output"), "count") = 5u;
            requireImportFailure(f, root, "GLTF_ANIMATION_OUTPUT_COUNT");
            f                                           = makeAnimatedRig(path, "CUBICSPLINE");
            field(samplerAccessor(f, "input"), "count") = 1u;
            requireImportFailure(f, root, "GLTF_ANIMATION_CUBIC_KEYS");
            f = makeAnimatedRig(path, "CUBICSPLINE");
            setAccessorFloat(f, "output", 0, std::numeric_limits<float>::quiet_NaN());
            requireImportFailure(f, root, "GLTF_ANIMATION_VALUE");
        }
        f = makeAnimatedRig("rotation");
        setAccessorFloat(f, "output", 3, 2);
        const auto invalid = importFixture(f, root);
        require(!invalid.bundle() && hasDiagnostic(invalid, "GLTF_ANIMATION_INVALID"),
                "Invalid quaternion exposes no products");
        require(invalid.diagnostics().back().location.starts_with("animations[0].channels[0]"),
                "Canonical key errors retain source channel context");
    }

    void scenePruning(const std::filesystem::path& root)
    {
        auto f                  = makeAnimationRig();
        field(f.root, "nodes")  = parse(R"([{}, {"mesh":0,"skin":0}, {"children":[3]}, {}])");
        field(f.root, "skins")  = parse(R"([{"joints":[3]}])");
        field(f.root, "scenes") = parse(R"([{"nodes":[1,2]},{"nodes":[0]}])");
        f.addAnimation();
        addTrack(f, "translation", 3);
        const auto  imported = importFixture(f, root);
        const auto& bundle   = requireAnimatedImport(imported);
        require(bundle.model->nodes.size() == 3 &&
                    bundle.model->skeletonUses[0].modelNodeIndices == std::vector<uint32_t>({1, 2}) &&
                    bundle.animationClips[0].tracks[0].skeletonNodeIndex == 1,
                "Source node 3 maps to evaluation node 1 even after becoming model node 2");
        f.addAnimationChannel(0, 0, 0, "translation");
        requireImportFailure(f, root, "GLTF_ANIMATION_NOT_SKELETAL");
    }

    GltfFixtureBuilder makeTwoRigs()
    {
        auto f                  = makeAnimationRig();
        field(f.root, "nodes")  = parse(R"([{"mesh":0,"skin":0},{"name":"joint"},
            {"name":"shared helper","children":[1,4]},{"mesh":0,"skin":1},{"name":"joint"}])");
        field(f.root, "skins")  = parse(R"([{"joints":[1]},{"joints":[4]}])");
        field(f.root, "scenes") = parse(R"([{"nodes":[0,2,3]}])");
        f.addAnimation("Walk");
        return f;
    }

    void sourceMembership(const std::filesystem::path& root)
    {
        auto f = makeTwoRigs();
        addTrack(f, "translation", 2);
        requireImportFailure(f, root, "GLTF_ANIMATION_AMBIGUOUS");
        addTrack(f, "rotation", 1);
        const auto  resolved = importFixture(f, root);
        const auto& bundle   = requireAnimatedImport(resolved);
        require(bundle.skeletons.size() == 2 && bundle.animationClips[0].tracks[0].skeletonNodeIndex == 0,
                "Complete channel intersection resolves shared helper using joint context");
        require(isGtsSkeletonCompatible(*bundle.skeletons[0], bundle.animationClips[0].targetSkeletonCompatibility),
                "Resolved source rig establishes clip contract");
        addTrack(f, "rotation", 4);
        requireImportFailure(f, root, "GLTF_ANIMATION_MULTIPLE_RIGS");

        f                                                = makeTwoRigs();
        field(at(field(f.root, "nodes"), 2), "children") = parse("[1]");
        field(f.root, "scenes")                          = parse(R"([{"nodes":[0,2,3,4]}])");
        addTrack(f, "translation", 1);
        addTrack(f, "rotation", 4);
        requireImportFailure(f, root, "GLTF_ANIMATION_MULTIPLE_RIGS");

        f                                            = makeAnimatedRig();
        field(at(field(f.root, "nodes"), 4), "mesh") = 0u;
        field(at(field(f.root, "nodes"), 4), "skin") = 0u;
        const auto shared                            = importFixture(f, root);
        require(requireAnimatedImport(shared).model->nodes[4].skinBindingIndex ==
                    shared.asset()->nodes[0].skinBindingIndex,
                "Multiple skinned mesh nodes do not duplicate the clip");

        // Resolver seam: compatible definitions with independent source memberships.
        // The current extractor gives separate source trees distinct stable IDs;
        // this also protects future intentional definition unification/linking.
        f = makeTwoRigs();
        addTrack(f, "translation", 1);
        std::vector<GtsModelDiagnostic> diagnostics;
        const auto                      data = gts::gltf::readSource(f.write(root), diagnostics);
        GtsSkeletonAsset                skeleton;
        skeleton.nodes                    = {{{"helper"}, "same", std::nullopt, {}}, {{"joint"}, "same", 0, {}}};
        auto                            a = std::make_shared<const GtsSkeletonAsset>(skeleton);
        auto                            b = std::make_shared<const GtsSkeletonAsset>(skeleton);
        gts::gltf::GltfSkinImportResult mappings{{a, b}, {{2, 1}, {2, 4}}};
        auto                            clips = gts::gltf::importAnimations(data, mappings);
        require(clips.size() == 1 && clips[0].tracks[0].skeletonNodeIndex == 1,
                "Compatible non-member definition never creates a source candidate");
        GtsModelImportBundle products;
        products.skeletons      = {a, b};
        products.animationClips = clips;
        require(validateGtsModelImportBundle(products).isValid(),
                "Two separate compatible definitions accept one reusable clip");
        std::swap(mappings.skeletons[0], mappings.skeletons[1]);
        std::swap(mappings.sourceNodes[0], mappings.sourceNodes[1]);
        require(areGtsSkeletonCompatibilitiesEqual(
                    clips[0].targetSkeletonCompatibility,
                    gts::gltf::importAnimations(data, mappings)[0].targetSkeletonCompatibility),
                "Resolution does not depend on pointer or definition ordering");
        f = makeTwoRigs();
        addTrack(f, "translation", 2);
        const auto ambiguousData = gts::gltf::readSource(f.write(root), diagnostics);
        bool       rejected      = false;
        try
        {
            (void)gts::gltf::importAnimations(ambiguousData, mappings);
        }
        catch (const gts::gltf::DecodeError& error)
        {
            rejected = error.diagnostic.code == "GLTF_ANIMATION_AMBIGUOUS";
        }
        require(rejected, "Even compatible shared-helper candidates require unambiguous source ownership");
    }
} // namespace

int main()
{
    try
    {
        const auto root = std::filesystem::temp_directory_path() / "gravitas_gltf_animation_test";
        std::filesystem::create_directories(root);
        valuesAndFormats(root);
        timingAndReuse(root);
        malformedAnimation(root);
        scenePruning(root);
        sourceMembership(root);
        std::filesystem::remove_all(root);
        std::puts("GtsGltfAnimationImporterTest passed");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
