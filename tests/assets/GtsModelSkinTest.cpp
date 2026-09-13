#include "assets/model/GtsModelAsset.h"
#include "assets/model/GtsModelValidation.h"
#include "assets/skeleton/GtsSkeletonAsset.h"

#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    void requireError(const GtsModelAsset& model, const std::string& code, const std::string& location = {})
    {
        const auto result = validateGtsModelAsset(model);
        for (const auto& error : result.diagnostics)
            if (error.code == code && error.location.find(location) != std::string::npos && !error.message.empty())
                return;
        throw std::runtime_error("Expected " + code + " at " + location);
    }

    GtsSkeletonAsset skeleton()
    {
        GtsSkeletonAsset result;
        result.nodes = {
            {{"root"}, "Root", std::nullopt, {}}, {{"helper"}, "Helper", 0, {}}, {{"joint"}, "Joint", 1, {}}};
        return result;
    }

    GtsModelAsset staticModel()
    {
        GtsModelAsset     result;
        GtsModelPrimitive primitive;
        primitive.attributes = {
            {GtsVertexSemantic::Position, 0, std::vector<glm::vec3>{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}}}};
        result.meshes = {{"triangle", {primitive}}};
        result.nodes.resize(1);
        result.nodes[0].meshIndex = 0;
        result.rootNodes          = {0};
        return result;
    }

    void addSet(GtsModelPrimitive& primitive, uint32_t index, glm::uvec4 joints, glm::vec4 weights)
    {
        primitive.attributes.push_back({GtsVertexSemantic::Joints, index, std::vector<glm::uvec4>(3, joints)});
        primitive.attributes.push_back({GtsVertexSemantic::Weights, index, std::vector<glm::vec4>(3, weights)});
    }

    GtsModelAsset boundModel()
    {
        auto result = staticModel();
        result.skeletonUses.push_back({std::make_shared<const GtsSkeletonAsset>(skeleton())});
        GtsModelSkinBinding association;
        const auto          derived = makeGtsSkeletonCompatibility(*result.skeletonUses[0].skeleton);
        require(derived.succeeded(), "Fixture skeleton is valid");
        association.binding.targetSkeletonCompatibility = *derived.compatibility();
        // Skin-local slot zero deliberately maps to the last skeleton node.
        association.binding.joints   = {{2, glm::mat4(1)}, {0, glm::mat4(1)}};
        association.skeletonUseIndex = 0;
        result.skinBindings.push_back(association);
        result.nodes[0].skinBindingIndex = 0;
        addSet(result.meshes[0].primitives[0], 0, {0, 1, 0, 0}, {0.75f, 0.25f, 0, 0});
        return result;
    }

    std::vector<glm::vec4>& weights(GtsModelAsset& model)
    {
        return std::get<std::vector<glm::vec4>>(model.meshes[0].primitives[0].attributes[2].values);
    }

    void associations()
    {
        auto model = staticModel();
        require(validateGtsModelAsset(model).isValid(), "Static models require no skeleton/binding placeholders");
        model.nodes[0].meshIndex.reset();
        require(validateGtsModelAsset(model).isValid(), "Transform-only nodes remain valid");
        model.skeletonUses.push_back({std::make_shared<const GtsSkeletonAsset>(skeleton())});
        require(validateGtsModelAsset(model).isValid(), "Valid unused skeleton use is allowed");
        model.skeletonUses.push_back(model.skeletonUses[0]);
        require(validateGtsModelAsset(model).isValid() && model.skeletonUses.size() == 2 &&
                    model.skeletonUses[0].skeleton == model.skeletonUses[1].skeleton,
                "Shared definition occurrences remain distinct");
        model.skeletonUses[1].skeleton.reset();
        requireError(model, "MODEL_SKELETON_REQUIRED", "skeletonUses[1].skeleton");
        model.skeletonUses[1].skeleton = std::make_shared<const GtsSkeletonAsset>();
        require(!validateGtsModelAsset(model).isValid(), "Invalid unused skeleton is rejected");

        model = boundModel();
        require(validateGtsModelAsset(model).isValid(), "Bound triangle and reordered skin-local mapping are valid");
        model.nodes[0].skinBindingIndex.reset();
        require(validateGtsModelAsset(model).isValid(), "Unused valid binding remains allowed");
        model.skinBindings[0].skeletonUseIndex = GtsModelSkinBinding::InvalidSkeletonUseIndex;
        requireError(model, "MODEL_SKELETON_USE_OUT_OF_RANGE");
        model.skinBindings[0].skeletonUseIndex = 1;
        requireError(model, "MODEL_SKELETON_USE_OUT_OF_RANGE");
        model                           = boundModel();
        model.nodes[0].skinBindingIndex = 1;
        requireError(model, "MODEL_SKIN_BINDING_OUT_OF_RANGE", "nodes[0]");
        model.nodes[0].meshIndex.reset();
        requireError(model, "MODEL_SKIN_MESH_REQUIRED");
        model.nodes[0].skinBindingIndex = 0;
        requireError(model, "MODEL_SKIN_MESH_REQUIRED");
        model.nodes[0].meshIndex = 100;
        requireError(model, "MODEL_MESH_OUT_OF_RANGE");

        model                          = boundModel();
        auto changed                   = skeleton();
        changed.nodes[2].id.value      = "other";
        model.skeletonUses[0].skeleton = std::make_shared<const GtsSkeletonAsset>(changed);
        requireError(model, "SKIN_SKELETON_INCOMPATIBLE", "skinBindings[0].skeletonUses[0]");
        model                                                     = boundModel();
        model.skinBindings[0].binding.targetSkeletonCompatibility = {};
        requireError(model, "SKIN_TARGET_INVALID");
        model                                                     = boundModel();
        model.skinBindings[0].binding.joints[0].skeletonNodeIndex = 3;
        requireError(model, "SKIN_NODE_OUT_OF_RANGE");
        model                                                           = boundModel();
        model.skinBindings[0].binding.joints[0].inverseBindMatrix[0][3] = 1;
        requireError(model, "SKIN_INVERSE_BIND_NOT_AFFINE");
        model = boundModel();
        model.skinBindings[0].binding.joints.clear();
        requireError(model, "SKIN_JOINTS_EMPTY");

        model                                                           = boundModel();
        model.skinBindings[0].binding.joints[1].skeletonNodeIndex       = 2;
        model.skinBindings[0].binding.joints[1].inverseBindMatrix[3][0] = 7;
        model.skinBindings.push_back(model.skinBindings[0]);
        model.skinBindings[1].binding.joints[0].inverseBindMatrix[3][1] = 4;
        model.skeletonUses.push_back(model.skeletonUses[0]);
        model.skinBindings.push_back(model.skinBindings[1]);
        model.skinBindings[2].skeletonUseIndex = 1;
        for (uint32_t binding : {0u, 1u, 2u})
        {
            model.nodes.push_back(model.nodes[0]);
            model.nodes.back().skinBindingIndex = binding;
            model.rootNodes.push_back(static_cast<uint32_t>(model.nodes.size() - 1));
        }
        require(validateGtsModelAsset(model).isValid(),
                "Nodes reuse bindings, bindings reuse occurrences, and duplicate remaps retain distinct inverse binds");
        // Same mesh is valid with binding zero, invalid with binding one: check every use.
        model.skinBindings[1].binding.joints.resize(1);
        requireError(model, "MODEL_SKIN_SLOT_OUT_OF_RANGE", "nodes[2]");
    }

    void streams()
    {
        auto model = boundModel();
        model.meshes[0].primitives[0].attributes.pop_back();
        requireError(model, "MODEL_SKIN_SET_UNPAIRED", "influences[0]");
        model = boundModel();
        model.meshes[0].primitives[0].attributes.erase(model.meshes[0].primitives[0].attributes.begin() + 1);
        requireError(model, "MODEL_SKIN_SET_UNPAIRED");
        model = boundModel();
        model.meshes[0].primitives[0].attributes.resize(1);
        requireError(model, "MODEL_SKIN_INFLUENCES_REQUIRED");
        model                                                                                       = boundModel();
        std::get<std::vector<glm::uvec4>>(model.meshes[0].primitives[0].attributes[1].values)[1][3] = 2;
        requireError(model, "MODEL_SKIN_SLOT_OUT_OF_RANGE", "Joints[0][1][3]");
        require(weights(model)[1][3] == 0, "Zero weight does not excuse invalid slot");
        model = boundModel();
        weights(model).pop_back();
        requireError(model, "MODEL_ATTRIBUTE_COUNT");
        model                                              = boundModel();
        model.meshes[0].primitives[0].attributes[1].values = std::vector<glm::vec3>(3);
        requireError(model, "MODEL_ATTRIBUTE_TYPE");
        model = boundModel();
        model.meshes[0].primitives[0].attributes.push_back(model.meshes[0].primitives[0].attributes[2]);
        requireError(model, "MODEL_ATTRIBUTE_DUPLICATE");

        model = boundModel();
        model.nodes[0].skinBindingIndex.reset();
        model.skinBindings.clear();
        model.skeletonUses.clear();
        weights(model)[0] = glm::vec4(0);
        model.meshes[0].primitives[0].attributes.erase(model.meshes[0].primitives[0].attributes.begin() + 1);
        require(validateGtsModelAsset(model).isValid(),
                "Unbound streams retain generic validation only, even unpaired/zero weights");
    }

    void weightRules()
    {
        auto model        = boundModel();
        weights(model)[1] = glm::vec4(0);
        requireError(model, "MODEL_SKIN_WEIGHT_ZERO", "vertices[1]");
        model             = boundModel();
        weights(model)[0] = {-0.25f, 1.25f, 0, 0};
        requireError(model, "MODEL_SKIN_WEIGHT_NEGATIVE", "Weights[0][0][0]");
        for (float invalid : {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        {
            model                = boundModel();
            weights(model)[0][0] = invalid;
            requireError(model, "MODEL_ATTRIBUTE_NONFINITE");
        }
        for (float offset : {-0.00005f, 0.00005f})
        {
            model = boundModel();
            weights(model)[0][0] += offset;
            require(validateGtsModelAsset(model).isValid(), "Unit sum accepts roundoff within 1e-4");
        }
        for (float offset : {-0.0002f, 0.0002f})
        {
            model = boundModel();
            weights(model)[0][0] += offset;
            requireError(model, "MODEL_SKIN_WEIGHT_SUM");
        }
        model = boundModel();
        for (auto& value : weights(model))
            value = glm::vec4(0.125f);
        addSet(model.meshes[0].primitives[0], 7, {1, 0, 1, 0}, glm::vec4(0.125f));
        require(validateGtsModelAsset(model).isValid(),
                "Eight positive influences across nonconsecutive sets are valid");
        model.meshes[0].primitives.push_back(model.meshes[0].primitives[0]);
        require(validateGtsModelAsset(model).isValid(), "Every primitive accepts independent complete influence sets");
        std::get<std::vector<glm::vec4>>(model.meshes[0].primitives[1].attributes[4].values)[2][0] = 0;
        requireError(model, "MODEL_SKIN_WEIGHT_SUM", "primitives[1]");
    }

    void immutableAndDeterministic()
    {
        auto model = boundModel();
        weights(model)[0][0] += 0.00005f;
        auto       before         = model;
        const auto beforeSkeleton = *model.skeletonUses[0].skeleton;
        const auto first          = validateGtsModelAsset(model);
        require(first.isValid(), "Valid slightly nonunit input passes without normalization");
        require(weights(model)[0][0] == weights(before)[0][0], "Weights are not normalized");
        require(areGtsSkeletonsCompatible(beforeSkeleton, *model.skeletonUses[0].skeleton), "Skeleton is unchanged");
        require(model.skeletonUses[0].skeleton == before.skeletonUses[0].skeleton &&
                    model.skinBindings[0].skeletonUseIndex == before.skinBindings[0].skeletonUseIndex &&
                    model.nodes[0].skinBindingIndex == before.nodes[0].skinBindingIndex,
                "Association identities are unchanged");
        const auto& skin = model.skinBindings[0].binding;
        require(areGtsSkeletonCompatibilitiesEqual(skin.targetSkeletonCompatibility,
                                                   before.skinBindings[0].binding.targetSkeletonCompatibility),
                "Binding contract remains unchanged");
        for (size_t i = 0; i < skin.joints.size(); ++i)
            require(skin.joints[i].skeletonNodeIndex == before.skinBindings[0].binding.joints[i].skeletonNodeIndex &&
                        skin.joints[i].inverseBindMatrix == before.skinBindings[0].binding.joints[i].inverseBindMatrix,
                    "Remaps and inverse binds remain unchanged");
        weights(model)[0]    = glm::vec4(0);
        weights(model)[1][0] = -1;
        const auto a         = validateGtsModelAsset(model);
        const auto b         = validateGtsModelAsset(model);
        require(!a.isValid() && a.diagnostics.size() == b.diagnostics.size(), "Repeated failure is deterministic");
        for (size_t i = 0; i < a.diagnostics.size(); ++i)
            require(a.diagnostics[i].code == b.diagnostics[i].code &&
                        a.diagnostics[i].message == b.diagnostics[i].message &&
                        a.diagnostics[i].location == b.diagnostics[i].location,
                    "Diagnostic order and content are stable");
        require(weights(model)[0][0] == 0 && weights(model)[1][0] == -1, "Malformed input is not repaired");
    }
} // namespace

int main()
{
    try
    {
        associations();
        streams();
        weightRules();
        immutableAndDeterministic();
        std::puts("GtsModelSkinTest passed");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
