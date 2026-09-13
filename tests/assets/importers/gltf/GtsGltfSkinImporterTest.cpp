#include "GltfFixtureBuilder.h"

#include <limits>
#include <set>
#include <variant>

#include "assets/importer/GtsModelImportBundleValidation.h"
#include "assets/model/GtsModelValidation.h"
#include "assets/skeleton/GtsSkeletonAsset.h"
#include "assets/skin/GtsSkinBindingValidation.h"

namespace
{
    GltfFixtureBuilder makeSingleJointRig()
    {
        GltfFixtureBuilder f;
        f.addVertexStream("JOINTS_0", std::vector<uint8_t>(12, 0), "VEC4", 5121);
        f.addVertexStream("WEIGHTS_0", floats({1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}), "VEC4", 5126);
        field(f.root, "nodes")  = parse(R"([{"mesh":0,"skin":0},{"name":"joint"}])");
        field(f.root, "skins")  = parse(R"([{"joints":[1]}])");
        field(f.root, "scenes") = parse(R"([{"nodes":[0,1]}])");
        return f;
    }

    GltfFixtureBuilder makeHelperHierarchyRig()
    {
        auto f = makeSingleJointRig();
        // Source order deliberately differs from parent-first evaluation order.
        field(f.root, "nodes")  = parse(R"([
            {"mesh":0,"skin":0,"translation":[100,0,0]},
            {"name":"duplicate","translation":[0,3,0]},
            {"name":"armature","translation":[10,0,0],"children":[4]},
            {"name":"matrix helper","matrix":[1,0,0,0, 0.25,1,0,0, 0,0,1,0, 0,2,0,1],"children":[1]},
            {"name":"duplicate","rotation":[0,0,0,1],"scale":[1,2,1],"children":[3]},
            {"name":"unrelated","mesh":0}
        ])");
        field(f.root, "skins")  = parse(R"([{"name":"body","joints":[1,4],"skeleton":4}])");
        field(f.root, "scenes") = parse(R"([{"nodes":[0,2]},{"nodes":[5]}])");
        return f;
    }

    uint32_t addInverseBindAccessor(GltfFixtureBuilder& fixture, const std::vector<glm::mat4>& matrices)
    {
        std::vector<uint8_t> bytes;
        for (const auto& matrix : matrices)
            for (size_t column = 0; column < 4; ++column)
                for (size_t row = 0; row < 4; ++row)
                    append32(bytes, std::bit_cast<uint32_t>(matrix[column][row]));
        return fixture.addAccessor(bytes, "MAT4", 5126, static_cast<uint32_t>(matrices.size()));
    }

    const GtsModelImportBundle& requireRiggedImportSuccess(const GtsModelImportResult& result)
    {
        std::string errors;
        for (const auto& diagnostic : result.diagnostics())
            errors += diagnostic.code + ": " + diagnostic.message + " @ " + diagnostic.location + "\n";
        require(result.succeeded() && result.bundle() && result.asset(), errors);
        require(validateGtsModelImportBundle(*result.bundle()).isValid(), "Complete rigged bundle validates");
        return *result.bundle();
    }

    void simpleAndHierarchy(const std::filesystem::path& root)
    {
        for (const auto format : {GltfFixtureFormat::External, GltfFixtureFormat::DataUri, GltfFixtureFormat::Glb})
        {
            auto        f      = makeSingleJointRig();
            const auto  result = importFixture(f, root, format);
            const auto& bundle = requireRiggedImportSuccess(result);
            const auto& model  = *bundle.model;
            require(bundle.skeletons.size() == 1 && model.skeletonUses.size() == 1 && model.skinBindings.size() == 1,
                    "One-joint rig emits one definition/use/binding");
            require(model.nodes[0].skinBindingIndex == 0 && model.nodes[0].meshIndex == 0,
                    "node.skin is a node association");
            require(model.skeletonUses[0].skeleton == bundle.skeletons[0],
                    "Bundle and model share the definition object");
            require(model.skeletonUses[0].modelNodeIndices == std::vector<uint32_t>{1},
                    "Correspondence preserves joint/model identity");
            const auto& binding = model.skinBindings[0].binding;
            require(binding.joints[0].skeletonNodeIndex == 0 && binding.joints[0].inverseBindMatrix == glm::mat4(1),
                    "Missing inverse binds are identity, not inverse default transforms");
            require(validateGtsSkinBinding(binding, *bundle.skeletons[0]).isValid(),
                    "Binding targets exact emitted compatibility");
        }
        auto      f = makeHelperHierarchyRig();
        glm::mat4 a(1), b(1);
        a[3]                = {-5, 6, 7, 1};
        b[0][0]             = 0; // Singular affine inverse binds remain accepted.
        const auto accessor = addInverseBindAccessor(f, {a, b, glm::mat4(1)});
        field(at(field(f.root, "skins"), 0), "inverseBindMatrices") = accessor;
        const auto  result                                          = importFixture(f, root, GltfFixtureFormat::Glb);
        const auto& bundle                                          = requireRiggedImportSuccess(result);
        const auto& model                                           = *bundle.model;
        const auto& skeleton                                        = *bundle.skeletons[0];
        require(skeleton.nodes.size() == 4 && model.nodes.size() == 5,
                "Ancestors/helpers retained, unselected scene excluded");
        require(model.skeletonUses[0].modelNodeIndices == std::vector<uint32_t>({2, 4, 3, 1}),
                "Source-to-evaluation correspondence preserves nontrivial order");
        for (uint32_t i = 1; i < skeleton.nodes.size(); ++i)
            require(skeleton.nodes[i].parentIndex == i - 1,
                    "Parent-first chain retains helper between deforming joints");
        require(skeleton.nodes[1].name == skeleton.nodes[3].name && skeleton.nodes[1].id != skeleton.nodes[3].id,
                "Duplicate display names do not define identity");
        require(std::holds_alternative<GtsSkeletonTrs>(skeleton.nodes[1].defaultLocalTransform),
                "Authored TRS stays TRS");
        const auto& matrix = std::get<glm::mat4>(skeleton.nodes[2].defaultLocalTransform);
        require(matrix[1][0] == 0.25f && matrix[3][1] == 2, "Matrix helper retains exact shear and translation");
        const auto& binding = model.skinBindings[0].binding;
        require(binding.joints.size() == 2 && binding.joints[0].skeletonNodeIndex == 3 &&
                    binding.joints[1].skeletonNodeIndex == 1,
                "Skin slots retain source order despite reordered evaluation hierarchy");
        require(binding.joints[0].inverseBindMatrix == a && binding.joints[1].inverseBindMatrix == b,
                "IBMs decode in slot order; permitted extra accessor elements are omitted");
        const auto& primitive = model.meshes[0].primitives[0];
        require(
            std::get<std::vector<glm::uvec4>>(findVertexAttribute(primitive, GtsVertexSemantic::Joints)->values)[0].x ==
                0,
            "JOINTS remain local slots, not evaluation-node index three");
        require(model.nodes[0].localTransform[3][0] == 100 &&
                    std::get<GtsSkeletonTrs>(skeleton.nodes[0].defaultLocalTransform).translation.x == 10,
                "Mesh-node and ancestor transforms remain separate for future skinning ownership");
        require(std::get<std::vector<glm::vec3>>(findVertexAttribute(primitive, GtsVertexSemantic::Position)->values)[0]
                        .x == 0,
                "No mesh or skeleton transforms are baked into geometry");
        const auto  second   = importFixture(f, root);
        const auto& repeated = requireRiggedImportSuccess(second);
        require(areGtsSkeletonsCompatible(skeleton, *repeated.skeletons[0]),
                "Repeated GLB/JSON imports produce exact equivalent skeletons");
        require(repeated.model->skeletonUses[0].modelNodeIndices == model.skeletonUses[0].modelNodeIndices,
                "Repeated import correspondence is deterministic");
        field(at(field(f.root, "nodes"), 4), "name") = "renamed";
        const auto renamed                           = importFixture(f, root);
        require(areGtsSkeletonsCompatible(skeleton, *requireRiggedImportSuccess(renamed).skeletons[0]),
                "Display renaming does not change stable IDs");

        // Insert an unselected node before active nodes and remap source indices.
        f                       = makeSingleJointRig();
        field(f.root, "nodes")  = parse(R"([{}, {"mesh":0,"skin":0}, {"children":[3]}, {}])");
        field(f.root, "skins")  = parse(R"([{"joints":[3]}])");
        field(f.root, "scenes") = parse(R"([{"nodes":[1,2]},{"nodes":[0]}])");
        const auto  pruned      = importFixture(f, root);
        const auto& p           = requireRiggedImportSuccess(pruned);
        require(p.model->skeletonUses[0].modelNodeIndices == std::vector<uint32_t>({1, 2}) &&
                    p.model->nodes[0].skinBindingIndex == 0,
                "Correspondence remaps along with scene-pruned model nodes");
    }

    void groupsAndOccurrences(const std::filesystem::path& root)
    {
        auto f                                                      = makeHelperHierarchyRig();
        field(f.root, "skins")                                      = parse(R"([{"joints":[1,4]},{"joints":[4,1]}])");
        field(at(field(f.root, "nodes"), 5), "skin")                = 1u;
        field(f.root, "scenes")                                     = parse(R"([{"nodes":[0,2,5]}])");
        auto custom                                                 = glm::mat4(1);
        custom[3][0]                                                = -9;
        field(at(field(f.root, "skins"), 1), "inverseBindMatrices") = addInverseBindAccessor(f, {custom, custom});
        const auto  result                                          = importFixture(f, root);
        const auto& bundle                                          = requireRiggedImportSuccess(result);
        const auto& model                                           = *bundle.model;
        require(model.meshes.size() == 1 && model.nodes[0].meshIndex == model.nodes[5].meshIndex,
                "Two skins share one mesh without duplication");
        require(bundle.skeletons.size() == 1 && model.skeletonUses.size() == 1 && model.skinBindings.size() == 2,
                "Reordered skins share one source rig occurrence and one definition");
        require(model.nodes[0].skinBindingIndex != model.nodes[5].skinBindingIndex,
                "Node-specific skin choice remains distinct");
        require(model.skinBindings[0].binding.joints[0].skeletonNodeIndex == 3 &&
                    model.skinBindings[1].binding.joints[0].skeletonNodeIndex == 1,
                "Each binding preserves its own joint order");
        require(model.skinBindings[0].binding.joints[0].inverseBindMatrix !=
                    model.skinBindings[1].binding.joints[0].inverseBindMatrix,
                "Shared pose definition does not imply shared inverse binds");
        field(at(field(f.root, "nodes"), 5), "skin") = 0u;
        const auto reused                            = importFixture(f, root);
        require(requireRiggedImportSuccess(reused).model->skinBindings.size() == 1 &&
                    reused.asset()->skeletonUses.size() == 1,
                "Repeated mesh instances selecting same source joints reuse binding/use; no per-mesh pose copies");

        f                       = makeSingleJointRig();
        field(f.root, "nodes")  = parse(R"([{"mesh":0,"skin":0},{"children":[2,3]}, {}, {}, {"mesh":0,"skin":1}])");
        field(f.root, "skins")  = parse(R"([{"joints":[2],"skeleton":1},{"joints":[3],"skeleton":1}])");
        field(f.root, "scenes") = parse(R"([{"nodes":[0,1,4]}])");
        const auto unioned      = importFixture(f, root);
        require(requireRiggedImportSuccess(unioned).skeletons.size() == 1 &&
                    unioned.bundle()->skeletons[0]->nodes.size() == 3,
                "Disjoint subsets with explicit common rig root safely use union hierarchy");
        erase(at(field(f.root, "skins"), 0), "skeleton");
        erase(at(field(f.root, "skins"), 1), "skeleton");
        const auto separate = importFixture(f, root);
        require(requireRiggedImportSuccess(separate).skeletons.size() == 2 &&
                    hasDiagnostic(separate, "GLTF_SKIN_GROUP_SEPARATE"),
                "Ambiguous disjoint subsets are kept separate and diagnosed");
        field(f.root, "skins") = parse(R"([{"joints":[1,2]},{"joints":[2,3]}])");
        const auto overlap     = importFixture(f, root);
        require(requireRiggedImportSuccess(overlap).skeletons.size() == 1 &&
                    overlap.bundle()->skeletons[0]->nodes.size() == 3,
                "Overlapping joint subsets retain their union");

        f = makeSingleJointRig();
        field(f.root, "nodes") =
            parse(R"([{"mesh":0,"skin":0},{"translation":[1,0,0]}, {"mesh":0,"skin":1},{"translation":[2,0,0]}])");
        field(f.root, "skins")  = parse(R"([{"joints":[1]},{"joints":[3]}])");
        field(f.root, "scenes") = parse(R"([{"nodes":[0,1,2,3]}])");
        const auto  unrelated   = importFixture(f, root);
        const auto& u           = requireRiggedImportSuccess(unrelated);
        require(u.skeletons.size() == 2 && u.model->skeletonUses.size() == 2 && u.skeletons[0] != u.skeletons[1],
                "Separate placed source joint trees remain separate definitions and occurrences");
        field(f.root, "scenes") = parse(R"([{"nodes":[0,1]},{"nodes":[2,3]}])");
        const auto selected     = importFixture(f, root);
        require(requireRiggedImportSuccess(selected).skeletons.size() == 1 && selected.asset()->nodes.size() == 2,
                "Unused unrelated rig and its mesh occurrence are not emitted");
    }

    void malformedSources(const std::filesystem::path& root)
    {
        for (const auto& [skin, code] :
             std::vector<std::pair<std::string, std::string>>{{R"([{"joints":[]}])", "GLTF_SKIN_JOINTS_EMPTY"},
                                                              {R"([{"joints":[8]}])", "GLTF_SKIN_JOINT_REFERENCE"},
                                                              {R"([{"joints":[1,1]}])", "GLTF_SKIN_JOINT_DUPLICATE"},
                                                              {R"([{"joints":[0,1]}])", "GLTF_SKIN_ROOT"},
                                                              {R"([{"joints":[1],"skeleton":0}])", "GLTF_SKIN_ROOT"},
                                                              {R"([{"joints":[1],"skeleton":20}])", "GLTF_SKIN_ROOT"}})
        {
            auto f                 = makeSingleJointRig();
            field(f.root, "skins") = parse(skin);
            requireImportFailure(f, root, code);
        }
        auto f = makeSingleJointRig();
        erase(at(field(f.root, "nodes"), 0), "mesh");
        requireImportFailure(f, root, "GLTF_SKIN_MESH_REQUIRED");
        f                                            = makeSingleJointRig();
        field(at(field(f.root, "nodes"), 0), "skin") = 99u;
        requireImportFailure(f, root, "GLTF_SKIN_REFERENCE");
        f                       = makeSingleJointRig();
        field(f.root, "scenes") = parse(R"([{"nodes":[0]}])");
        requireImportFailure(f, root, "GLTF_SKIN_SCENE");
        f                                                = makeSingleJointRig();
        field(at(field(f.root, "nodes"), 1), "children") = parse("[1]");
        requireImportFailure(f, root, "GLTF_NODE_CYCLE");
        f                           = makeSingleJointRig();
        field(f.root, "animations") = parse("[{}]");
        const auto animated         = importFixture(f, root, GltfFixtureFormat::Glb);
        require(!animated.succeeded() && !animated.bundle() && hasDiagnostic(animated, "GLTF_ANIMATION_UNSUPPORTED"),
                "Rigged animated GLB still fails explicitly");
        f                               = makeSingleJointRig();
        field(f.primitive(), "targets") = parse("[{}]");
        requireImportFailure(f, root, "GLTF_MORPH_UNSUPPORTED");
        f                                   = makeSingleJointRig();
        field(f.root, "extensionsRequired") = parse(R"(["UNKNOWN_skin_feature"])");
        requireImportFailure(f, root, "GLTF_REQUIRED_EXTENSION");

        for (const auto kind : {"type", "component", "short", "stride", "nonfinite", "affine", "sparse", "implicit"})
        {
            f      = makeHelperHierarchyRig();
            auto m = glm::mat4(1);
            if (std::string(kind) == "nonfinite")
                m[2][2] = std::numeric_limits<float>::quiet_NaN();
            if (std::string(kind) == "affine")
                m[3][3] = 2;
            const auto a                                                = addInverseBindAccessor(f, {m, m});
            field(at(field(f.root, "skins"), 0), "inverseBindMatrices") = a;
            auto&       accessor                                        = at(field(f.root, "accessors"), a);
            std::string error                                           = "GLTF_SKIN_INVERSE_BIND_TYPE";
            if (std::string(kind) == "type")
                field(accessor, "type") = "VEC4";
            if (std::string(kind) == "component")
                field(accessor, "componentType") = 5123u;
            if (std::string(kind) == "short")
                field(accessor, "count") = 1u;
            if (std::string(kind) == "stride")
                field(at(field(f.root, "bufferViews"), a), "byteStride") = 64u;
            if (std::string(kind) == "nonfinite" || std::string(kind) == "affine")
                error = "GLTF_SKIN_INVERSE_BIND_VALUE";
            if (std::string(kind) == "sparse")
            {
                field(accessor, "sparse") = parse("{}");
                error                     = "GLTF_SPARSE_UNSUPPORTED";
            }
            if (std::string(kind) == "implicit")
            {
                erase(accessor, "bufferView");
                error = "GLTF_ACCESSOR_SOURCE_UNSUPPORTED";
            }
            requireImportFailure(f, root, error);
        }
    }

    void influences(const std::filesystem::path& root)
    {
        auto                 f = makeSingleJointRig();
        std::vector<uint8_t> joints(12, 0);
        joints[3] = 1;
        f.addVertexStream("JOINTS_0", joints, "VEC4", 5121);
        requireImportFailure(f, root, "MODEL_SKIN_SLOT_OUT_OF_RANGE"); // Slot invalid even though weight is zero.
        for (float weight : {0.0f, 0.9f, -0.1f, std::numeric_limits<float>::infinity()})
        {
            f = makeSingleJointRig();
            f.addVertexStream("WEIGHTS_0", floats({weight, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}), "VEC4", 5126);
            requireImportFailure(f,
                                 root,
                                 weight == 0      ? "MODEL_SKIN_WEIGHT_ZERO"
                                 : weight == 0.9f ? "MODEL_SKIN_WEIGHT_SUM"
                                                  : "GLTF_ATTRIBUTE_VALUE");
        }
        f = makeSingleJointRig();
        f.addVertexStream("WEIGHTS_0", floats({0.99995f, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}), "VEC4", 5126);
        const auto roundoff = importFixture(f, root);
        requireRiggedImportSuccess(roundoff);
        require(
            std::get<std::vector<glm::vec4>>(
                findVertexAttribute(roundoff.asset()->meshes[0].primitives[0], GtsVertexSemantic::Weights)->values)[0]
                    .x == 0.99995f,
            "Within-tolerance weights are not normalized");
        f                      = makeSingleJointRig();
        field(f.root, "nodes") = parse(R"([{"mesh":0,"skin":0},{"children":[2,3,4,5,6,7,8]}, {},{},{},{},{},{},{}])");
        field(f.root, "skins") = parse(R"([{"joints":[8,7,6,5,4,3,2,1]}])");
        f.addVertexStream("JOINTS_0", {0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3}, "VEC4", 5121);
        f.addVertexStream("JOINTS_1", {4, 5, 6, 7, 4, 5, 6, 7, 4, 5, 6, 7}, "VEC4", 5121);
        for (const auto name : {"WEIGHTS_0", "WEIGHTS_1"})
            f.addVertexStream(
                name,
                floats({.125f, .125f, .125f, .125f, .125f, .125f, .125f, .125f, .125f, .125f, .125f, .125f}),
                "VEC4",
                5126);
        const auto  eight  = importFixture(f, root, GltfFixtureFormat::Glb);
        const auto& bundle = requireRiggedImportSuccess(eight);
        require(bundle.model->skinBindings[0].binding.joints.size() == 8 &&
                    findVertexAttribute(bundle.model->meshes[0].primitives[0], GtsVertexSemantic::Weights, 1),
                "Eight distinct skin slots and eight positive influences survive across both sets");
        require(std::get<std::vector<glm::uvec4>>(
                    findVertexAttribute(bundle.model->meshes[0].primitives[0], GtsVertexSemantic::Joints, 1)->values)[0]
                        .w == 7,
                "Extra joint slots are not truncated or rewritten");
        erase(f.attributes(), "WEIGHTS_1");
        requireImportFailure(f, root, "GLTF_INFLUENCE_PAIR");
        f                                                  = makeSingleJointRig();
        field(at(field(f.root, "accessors"), 1), "sparse") = parse("{}");
        requireImportFailure(f, root, "GLTF_SPARSE_UNSUPPORTED");
    }
} // namespace

int main()
{
    try
    {
        const auto root = std::filesystem::temp_directory_path() / "gravitas_gltf_skin_test";
        std::filesystem::create_directories(root);
        simpleAndHierarchy(root);
        groupsAndOccurrences(root);
        malformedSources(root);
        influences(root);
        std::filesystem::remove_all(root);
        std::puts("GtsGltfSkinImporterTest passed");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
