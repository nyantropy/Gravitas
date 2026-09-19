#include "animation/skinning/GtsSkinPaletteEvaluation.h"
#include "animation/skeletal/GtsSkeletonPoseEvaluation.h"
#include "animation/skeletal/GtsSkeletonPoseValidation.h"
#include "model/domain/skin/GtsSkinBinding.h"
#include "model/domain/skeleton/GtsSkeletonAsset.h"

#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <gtc/matrix_transform.hpp>

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }
    void matrixEquals(const glm::mat4& actual, const glm::mat4& expected)
    {
        for (glm::length_t c = 0; c < 4; ++c)
            for (glm::length_t r = 0; r < 4; ++r)
                require(actual[c][r] == expected[c][r], "Unexpected palette matrix component");
    }
    glm::mat4 translation(float x)
    {
        return glm::translate(glm::mat4(1), glm::vec3(x, 0, 0));
    }
    GtsSkeletonAsset rig()
    {
        GtsSkeletonAsset skeleton;
        GtsSkeletonNode  root;
        root.id.value = "root";
        skeleton.nodes.push_back(root);
        return skeleton;
    }
    GtsSkinBinding bind(const GtsSkeletonAsset& skeleton, std::initializer_list<uint32_t> nodes)
    {
        GtsSkinBinding binding;
        const auto     contract = makeGtsSkeletonCompatibility(skeleton);
        require(contract.succeeded(), "Valid fixture skeleton");
        binding.targetSkeletonCompatibility = *contract.compatibility();
        for (uint32_t node : nodes)
            binding.joints.push_back({node, glm::mat4(1)});
        return binding;
    }
    GtsSkeletonPose defaultPose(const GtsSkeletonAsset& skeleton)
    {
        const auto result = evaluateGtsDefaultPose(skeleton);
        require(result.succeeded(), "Default fixture pose evaluates");
        return *result.pose();
    }
    void failure(const GtsSkinPaletteEvaluationResult& result, const char* code)
    {
        require(!result.succeeded() && !result.palette() && !result.diagnostics().empty(), "No palette on failure");
        for (const auto& d : result.diagnostics())
            if (d.code == code && !d.location.empty())
                return;
        throw std::runtime_error(std::string("Missing diagnostic: ") + code);
    }
    void oneJointAndMultiplication()
    {
        auto skeleton = rig();
        auto binding  = bind(skeleton, {0});
        auto pose     = defaultPose(skeleton);
        auto result   = evaluateGtsSkinPalette(pose, binding, skeleton);
        require(result.succeeded() && result.palette()->matrices.size() == 1, "One skin slot");
        matrixEquals(result.palette()->matrices[0], glm::mat4(1));
        std::get<GtsSkeletonTrs>(skeleton.nodes[0].defaultLocalTransform).translation.x = 4;
        binding                                                                         = bind(skeleton, {0});
        pose                                                                            = defaultPose(skeleton);
        result = evaluateGtsSkinPalette(pose, binding, skeleton);
        require(result.succeeded(), "Translated pose");
        matrixEquals(result.palette()->matrices[0], translation(4));
        binding.joints[0].inverseBindMatrix = translation(-2);
        result                              = evaluateGtsSkinPalette(pose, binding, skeleton);
        require(result.succeeded(), "Nonidentity inverse bind");
        matrixEquals(result.palette()->matrices[0], translation(2));
        auto& trs                           = std::get<GtsSkeletonTrs>(skeleton.nodes[0].defaultLocalTransform);
        trs.translation                     = glm::vec3(0);
        trs.scale                           = {2, 3, 4};
        binding                             = bind(skeleton, {0});
        binding.joints[0].inverseBindMatrix = translation(5);
        pose                                = defaultPose(skeleton);
        result                              = evaluateGtsSkinPalette(pose, binding, skeleton);
        require(result.succeeded(), "Noncommuting pose and inverse bind");
        auto expected  = glm::scale(glm::mat4(1), glm::vec3(2, 3, 4));
        expected[3][0] = 10;
        matrixEquals(result.palette()->matrices[0], expected);
        require((binding.joints[0].inverseBindMatrix * pose.modelTransforms[0])[3][0] == 5,
                "Reversed multiplication would produce a different answer");
        trs.scale                           = glm::vec3(0);
        pose                                = defaultPose(skeleton);
        binding                             = bind(skeleton, {0});
        binding.joints[0].inverseBindMatrix = glm::scale(glm::mat4(1), glm::vec3(0, 1, 1));
        result                              = evaluateGtsSkinPalette(pose, binding, skeleton);
        require(result.succeeded(), "Singular affine pose, inverse bind and palette are allowed");
        matrixEquals(result.palette()->matrices[0], glm::scale(glm::mat4(1), glm::vec3(0)));
    }
    void mappingAndModularBindings()
    {
        auto skeleton                                                                   = rig();
        std::get<GtsSkeletonTrs>(skeleton.nodes[0].defaultLocalTransform).translation.x = 10;
        GtsSkeletonNode child;
        child.id.value                                                      = "child";
        child.parentIndex                                                   = 0;
        std::get<GtsSkeletonTrs>(child.defaultLocalTransform).translation.x = 2;
        skeleton.nodes.push_back(child);
        const auto pose  = defaultPose(skeleton); // one pose evaluation, independent body/garment consumers
        const auto body  = bind(skeleton, {0, 1});
        auto       dress = bind(skeleton, {1, 0});
        dress.joints[0].inverseBindMatrix = translation(-2);
        dress.joints[1].inverseBindMatrix = translation(-4);
        const auto originalPose           = pose;
        const auto originalDress          = dress;
        const auto originalSkeleton       = makeGtsSkeletonCompatibility(skeleton);
        const auto bodyPalette            = evaluateGtsSkinPalette(pose, body, skeleton);
        const auto dressPalette           = evaluateGtsSkinPalette(pose, dress, skeleton);
        require(bodyPalette.succeeded() && dressPalette.succeeded(), "Shared pose feeds multiple bindings");
        matrixEquals(bodyPalette.palette()->matrices[0], translation(10));
        matrixEquals(bodyPalette.palette()->matrices[1], translation(12));
        matrixEquals(dressPalette.palette()->matrices[0], translation(10));
        matrixEquals(dressPalette.palette()->matrices[1], translation(6));
        const auto reversed = evaluateGtsSkinPalette(pose, bind(skeleton, {1, 0}), skeleton);
        require(reversed.succeeded(), "Reordered slots");
        matrixEquals(reversed.palette()->matrices[0], translation(12));
        matrixEquals(reversed.palette()->matrices[1], translation(10));
        const auto subset = evaluateGtsSkinPalette(pose, bind(skeleton, {1}), skeleton);
        require(subset.succeeded() && subset.palette()->matrices.size() == 1, "Subset size follows binding");
        matrixEquals(subset.palette()->matrices[0], translation(12));
        auto duplicated                        = bind(skeleton, {1, 1});
        duplicated.joints[0].inverseBindMatrix = translation(-2);
        duplicated.joints[1].inverseBindMatrix = translation(-5);
        const auto duplicates                  = evaluateGtsSkinPalette(pose, duplicated, skeleton);
        require(duplicates.succeeded(), "Duplicate node mappings remain separate local slots");
        matrixEquals(duplicates.palette()->matrices[0], translation(10));
        matrixEquals(duplicates.palette()->matrices[1], translation(7));
        auto renamed              = skeleton;
        renamed.name              = "new display name";
        renamed.nodes[0].name     = "another node name";
        const auto renamedPalette = evaluateGtsSkinPalette(pose, body, renamed);
        require(renamedPalette.succeeded(), "Separate object with changed display names is compatible");
        for (size_t i = 0; i < pose.modelTransforms.size(); ++i)
        {
            matrixEquals(pose.modelTransforms[i], originalPose.modelTransforms[i]);
            const auto& a = std::get<GtsSkeletonTrs>(pose.localTransforms[i]);
            const auto& b = std::get<GtsSkeletonTrs>(originalPose.localTransforms[i]);
            require(a.translation == b.translation && a.rotation == b.rotation && a.scale == b.scale,
                    "Locals unchanged");
        }
        require(areGtsSkeletonCompatibilitiesEqual(pose.skeletonCompatibility, originalPose.skeletonCompatibility) &&
                    areGtsSkeletonCompatibilitiesEqual(dress.targetSkeletonCompatibility,
                                                       originalDress.targetSkeletonCompatibility) &&
                    isGtsSkeletonCompatible(skeleton, *originalSkeleton.compatibility()),
                "Contracts unchanged");
        for (size_t i = 0; i < dress.joints.size(); ++i)
        {
            require(dress.joints[i].skeletonNodeIndex == originalDress.joints[i].skeletonNodeIndex, "Remap unchanged");
            matrixEquals(dress.joints[i].inverseBindMatrix, originalDress.joints[i].inverseBindMatrix);
        }
        const auto repeated = evaluateGtsSkinPalette(pose, dress, skeleton);
        require(repeated.succeeded(), "Repeated evaluation succeeds");
        for (size_t i = 0; i < dress.joints.size(); ++i)
            matrixEquals(repeated.palette()->matrices[i], dressPalette.palette()->matrices[i]);
    }
    void invalidInputs()
    {
        const auto skeleton     = rig();
        const auto binding      = bind(skeleton, {0});
        const auto pose         = defaultPose(skeleton);
        auto       other        = skeleton;
        other.nodes[0].id.value = "different";
        failure(evaluateGtsSkinPalette(pose, binding, other), "SKIN_SKELETON_INCOMPATIBLE");
        failure(evaluateGtsSkinPalette(defaultPose(other), binding, skeleton), "POSE_SKELETON_INCOMPATIBLE");
        other                                                                        = skeleton;
        std::get<GtsSkeletonTrs>(other.nodes[0].defaultLocalTransform).translation.x = 3;
        failure(evaluateGtsSkinPalette(defaultPose(other), binding, skeleton), "POSE_SKELETON_INCOMPATIBLE");
        other                                                                     = skeleton;
        std::get<GtsSkeletonTrs>(other.nodes[0].defaultLocalTransform).rotation.w = -1;
        failure(evaluateGtsSkinPalette(defaultPose(other), binding, skeleton), "POSE_SKELETON_INCOMPATIBLE");
        other                                = skeleton;
        other.nodes[0].defaultLocalTransform = glm::mat4(1);
        failure(evaluateGtsSkinPalette(defaultPose(other), binding, skeleton), "POSE_SKELETON_INCOMPATIBLE");
        other                      = skeleton;
        other.nodes[0].parentIndex = 0;
        failure(evaluateGtsSkinPalette(pose, binding, other), "SKIN_SKELETON_INVALID");
        auto badBinding = binding;
        badBinding.joints.clear();
        failure(evaluateGtsSkinPalette(pose, badBinding, skeleton), "SKIN_JOINTS_EMPTY");
        badBinding                             = binding;
        badBinding.joints[0].skeletonNodeIndex = 2;
        failure(evaluateGtsSkinPalette(pose, badBinding, skeleton), "SKIN_NODE_OUT_OF_RANGE");
        badBinding                                   = binding;
        badBinding.joints[0].inverseBindMatrix[1][3] = 2;
        failure(evaluateGtsSkinPalette(pose, badBinding, skeleton), "SKIN_INVERSE_BIND_NOT_AFFINE");
        badBinding                                   = binding;
        badBinding.joints[0].inverseBindMatrix[1][0] = std::numeric_limits<float>::infinity();
        failure(evaluateGtsSkinPalette(pose, badBinding, skeleton), "SKIN_INVERSE_BIND_NONFINITE");
        auto badPose = pose;
        badPose.localTransforms.clear();
        failure(evaluateGtsSkinPalette(badPose, binding, skeleton), "POSE_LOCAL_COUNT");
        badPose = pose;
        badPose.modelTransforms.clear();
        failure(evaluateGtsSkinPalette(badPose, binding, skeleton), "POSE_MODEL_COUNT");
        badPose                       = pose;
        badPose.skeletonCompatibility = {};
        require(!evaluateGtsSkinPalette(badPose, binding, skeleton).succeeded(),
                "Untagged pose cannot prove compatibility");
        badPose                          = pose;
        badPose.modelTransforms[0][2][1] = std::numeric_limits<float>::quiet_NaN();
        failure(evaluateGtsSkinPalette(badPose, binding, skeleton), "POSE_MATRIX_NONFINITE");
        badPose                          = pose;
        badPose.modelTransforms[0][0][3] = 1;
        failure(evaluateGtsSkinPalette(badPose, binding, skeleton), "POSE_MATRIX_NOT_AFFINE");
        badPose                                                            = pose;
        std::get<GtsSkeletonTrs>(badPose.localTransforms[0]).translation.x = std::numeric_limits<float>::infinity();
        failure(evaluateGtsSkinPalette(badPose, binding, skeleton), "POSE_TRS_NONFINITE");
        badPose                                                       = pose;
        std::get<GtsSkeletonTrs>(badPose.localTransforms[0]).rotation = glm::quat(0, 0, 0, 0);
        failure(evaluateGtsSkinPalette(badPose, binding, skeleton), "POSE_ROTATION_NOT_UNIT");
        badPose                    = pose;
        badPose.localTransforms[0] = glm::mat4(1);
        failure(evaluateGtsSkinPalette(badPose, binding, skeleton), "POSE_TRANSFORM_FORM");
        const auto first  = evaluateGtsSkinPalette(badPose, binding, skeleton);
        const auto second = evaluateGtsSkinPalette(badPose, binding, skeleton);
        require(first.diagnostics().size() == second.diagnostics().size(), "Deterministic error count");
        for (size_t i = 0; i < first.diagnostics().size(); ++i)
            require(first.diagnostics()[i].code == second.diagnostics()[i].code &&
                        first.diagnostics()[i].location == second.diagnostics()[i].location &&
                        first.diagnostics()[i].message == second.diagnostics()[i].message,
                    "Deterministic diagnostics");
    }
    void matrixHelpersAndOverflow()
    {
        auto skeleton                           = rig();
        skeleton.nodes[0].defaultLocalTransform = translation(3);
        auto pose                               = defaultPose(skeleton);
        auto binding                            = bind(skeleton, {0});
        auto result                             = evaluateGtsSkinPalette(pose, binding, skeleton);
        require(result.succeeded(), "Matrix-form evaluation node feeds palette");
        matrixEquals(result.palette()->matrices[0], translation(3));
        std::get<glm::mat4>(pose.localTransforms[0])[0][0] = std::numeric_limits<float>::infinity();
        failure(evaluateGtsSkinPalette(pose, binding, skeleton), "POSE_MATRIX_NONFINITE");
        skeleton = rig();
        GtsSkeletonNode large;
        large.id.value                                                = "large";
        std::get<GtsSkeletonTrs>(large.defaultLocalTransform).scale.x = std::numeric_limits<float>::max();
        skeleton.nodes.push_back(large);
        pose                                      = defaultPose(skeleton);
        binding                                   = bind(skeleton, {0, 1});
        binding.joints[1].inverseBindMatrix[0][0] = 2;
        result                                    = evaluateGtsSkinPalette(pose, binding, skeleton);
        failure(result, "SKIN_PALETTE_NONFINITE");
        require(result.diagnostics()[0].location == "slots[1].skeletonNodes[1]",
                "Overflow reports failing slot and node");
        require(std::isfinite(pose.modelTransforms[1][0][0]), "Overflow failure does not modify input");
        auto unrelated                 = skeleton;
        unrelated.nodes[1].parentIndex = 0;
        failure(evaluateGtsSkinPalette(defaultPose(unrelated), bind(skeleton, {0}), skeleton),
                "POSE_SKELETON_INCOMPATIBLE");
    }
} // namespace
int main()
{
    try
    {
        oneJointAndMultiplication();
        mappingAndModularBindings();
        invalidInputs();
        matrixHelpersAndOverflow();
        std::puts("GtsSkinPaletteTest passed");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
