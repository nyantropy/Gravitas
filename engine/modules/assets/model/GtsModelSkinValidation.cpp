#include "GtsModelSkinValidation.h"

#include <cmath>
#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "GtsModelAsset.h"
#include "GtsModelValidation.h"
#include "assets/skin/GtsSkinBindingValidation.h"
#include "assets/skeleton/GtsSkeletonValidation.h"

namespace
{
    void addError(GtsModelValidationResult& result, const char* code, std::string message, std::string location)
    {
        result.diagnostics.push_back(
            {GtsModelDiagnosticSeverity::Error, code, std::move(message), std::move(location)});
    }

    template <class Validation>
    void appendErrors(const Validation& validation, const std::string& location, GtsModelValidationResult& result)
    {
        for (const auto& error : validation.diagnostics)
            result.diagnostics.push_back(
                {GtsModelDiagnosticSeverity::Error, error.code, error.message, location + "." + error.location});
    }

    struct InfluenceSet
    {
        const GtsVertexAttribute* joints  = nullptr;
        const GtsVertexAttribute* weights = nullptr;
    };

    void validatePrimitiveSkin(const GtsModelPrimitive&  primitive,
                               const GtsSkinBinding&     binding,
                               const std::string&        location,
                               GtsModelValidationResult& result)
    {
        std::map<uint32_t, InfluenceSet> sets;
        size_t                           vertexCount = 0;
        bool                             readable    = true;
        for (const auto& attribute : primitive.attributes)
        {
            if (attribute.semantic == GtsVertexSemantic::Position && attribute.setIndex == 0)
                if (const auto* values = std::get_if<std::vector<glm::vec3>>(&attribute.values))
                    vertexCount = values->size();
            if (attribute.semantic != GtsVertexSemantic::Joints && attribute.semantic != GtsVertexSemantic::Weights)
                continue;
            auto& set   = sets[attribute.setIndex];
            auto& entry = attribute.semantic == GtsVertexSemantic::Joints ? set.joints : set.weights;
            if (entry)
                readable = false; // generic validation reports duplicate keys
            entry = &attribute;
        }
        if (sets.empty())
        {
            addError(result,
                     "MODEL_SKIN_INFLUENCES_REQUIRED",
                     "A bound primitive requires paired Joints/Weights streams.",
                     location);
            return;
        }
        for (const auto& [index, set] : sets)
        {
            if (!set.joints || !set.weights)
            {
                addError(result,
                         "MODEL_SKIN_SET_UNPAIRED",
                         "Joints and Weights must both exist for set " + std::to_string(index) + ".",
                         location + ".influences[" + std::to_string(index) + "]");
                readable = false;
                continue;
            }
            const auto* joints  = std::get_if<std::vector<glm::uvec4>>(&set.joints->values);
            const auto* weights = std::get_if<std::vector<glm::vec4>>(&set.weights->values);
            // types/counts belong to generic primitive validation, while we guard access
            // here without repeating those diagnostics for every node use
            if (!joints || !weights || joints->size() != vertexCount || weights->size() != vertexCount)
                readable = false;
        }
        if (!readable || vertexCount == 0)
            return;

        for (size_t vertex = 0; vertex < vertexCount; ++vertex)
        {
            double total         = 0;
            bool   usableWeights = true;
            for (const auto& [index, set] : sets)
            {
                const auto& joints  = std::get<std::vector<glm::uvec4>>(set.joints->values)[vertex];
                const auto& weights = std::get<std::vector<glm::vec4>>(set.weights->values)[vertex];
                for (glm::length_t component = 0; component < 4; ++component)
                {
                    const auto suffix = "[" + std::to_string(index) + "][" + std::to_string(vertex) + "][" +
                                        std::to_string(component) + "]";
                    if (joints[component] >= binding.joints.size())
                        addError(result,
                                 "MODEL_SKIN_SLOT_OUT_OF_RANGE",
                                 "Skin slot " + std::to_string(joints[component]) + " exceeds binding slot count " +
                                     std::to_string(binding.joints.size()) + ".",
                                 location + ".Joints" + suffix);
                    if (!std::isfinite(weights[component]))
                        usableWeights = false; // generic validation reports nonfinite data
                    else if (weights[component] < 0)
                    {
                        addError(result,
                                 "MODEL_SKIN_WEIGHT_NEGATIVE",
                                 "Skin weights must be nonnegative.",
                                 location + ".Weights" + suffix);
                        usableWeights = false;
                    }
                    total += static_cast<double>(weights[component]);
                }
            }
            if (!usableWeights)
                continue;
            const auto vertexLocation = location + ".vertices[" + std::to_string(vertex) + "].skinWeightTotal";
            if (total <= 0)
                addError(result, "MODEL_SKIN_WEIGHT_ZERO", "Total skin weight must be positive.", vertexLocation);
            else if (std::abs(total - 1.0) > GtsModelSkinWeightSumTolerance)
                addError(result,
                         "MODEL_SKIN_WEIGHT_SUM",
                         "Total skin weight across all sets must be within 1e-4 of one; got " + std::to_string(total) +
                             ".",
                         vertexLocation);
        }
    }
} // namespace

void gtsModelValidationDetail::validateSkinAssociations(const GtsModelAsset& asset, GtsModelValidationResult& result)
{
    std::vector<bool> validUses(asset.skeletonUses.size(), false);
    for (size_t i = 0; i < asset.skeletonUses.size(); ++i)
    {
        const auto& use      = asset.skeletonUses[i];
        const auto  location = "skeletonUses[" + std::to_string(i) + "].skeleton";
        if (!use.skeleton)
        {
            addError(result, "MODEL_SKELETON_REQUIRED", "Skeleton use requires a definition.", location);
            continue;
        }
        const auto validation = validateGtsSkeletonAsset(*use.skeleton);
        validUses[i]          = validation.isValid();
        appendErrors(validation, location, result);
    }
    for (size_t i = 0; i < asset.skinBindings.size(); ++i)
    {
        const auto& association = asset.skinBindings[i];
        const auto  location    = "skinBindings[" + std::to_string(i) + "]";
        const auto  useIndex    = association.skeletonUseIndex;
        if (useIndex == GtsModelSkinBinding::InvalidSkeletonUseIndex || useIndex >= asset.skeletonUses.size())
            addError(result,
                     "MODEL_SKELETON_USE_OUT_OF_RANGE",
                     "Binding must select a model skeleton use.",
                     location + ".skeletonUseIndex");

        if (useIndex < validUses.size() && validUses[useIndex])
        {
            const auto validation = validateGtsSkinBinding(association.binding, *asset.skeletonUses[useIndex].skeleton);
            appendErrors(validation, location + ".skeletonUses[" + std::to_string(useIndex) + "].binding", result);
        }
        else
            appendErrors(validateGtsSkinBinding(association.binding), location + ".binding", result);
    }
    for (size_t i = 0; i < asset.nodes.size(); ++i)
    {
        const auto& node = asset.nodes[i];
        if (!node.skinBindingIndex)
            continue;
        const auto location = "nodes[" + std::to_string(i) + "]";
        if (!node.meshIndex)
            addError(result,
                     "MODEL_SKIN_MESH_REQUIRED",
                     "A node selecting a skin binding must also select a mesh.",
                     location);
        if (*node.skinBindingIndex >= asset.skinBindings.size())
        {
            addError(result,
                     "MODEL_SKIN_BINDING_OUT_OF_RANGE",
                     "Node must select a model skin binding.",
                     location + ".skinBindingIndex");
            continue;
        }
        if (!node.meshIndex || *node.meshIndex >= asset.meshes.size())
            continue; // generic model validation reports invalid mesh references
        const auto& mesh        = asset.meshes[*node.meshIndex];
        const auto& association = asset.skinBindings[*node.skinBindingIndex];
        for (size_t primitive = 0; primitive < mesh.primitives.size(); ++primitive)
            validatePrimitiveSkin(mesh.primitives[primitive],
                                  association.binding,
                                  location + ".meshes[" + std::to_string(*node.meshIndex) + "].primitives[" +
                                      std::to_string(primitive) + "].skinBindings[" +
                                      std::to_string(*node.skinBindingIndex) + "]",
                                  result);
    }
}
