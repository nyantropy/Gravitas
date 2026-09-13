#include "GtsSkinnedMeshPreparation.h"
#include "GtsStaticMeshPreparation.h"
#include "assets/model/GtsModelAsset.h"
#include "assets/skeleton/GtsSkeletonAsset.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <variant>

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }
    void near(float value, float expected)
    {
        require(std::isfinite(value) && std::abs(value - expected) < 1e-6f, "Unexpected weight/attribute value");
    }
    GtsSkinBinding makeBinding(size_t slots = 8)
    {
        GtsSkeletonAsset skeleton;
        for (size_t i = 0; i < slots; ++i)
        {
            GtsSkeletonNode node;
            node.id.value = "node_" + std::to_string(i);
            skeleton.nodes.push_back(node);
        }
        GtsSkinBinding binding;
        binding.targetSkeletonCompatibility = *makeGtsSkeletonCompatibility(skeleton).compatibility();
        for (size_t i = 0; i < slots; ++i)
            binding.joints.push_back({static_cast<uint32_t>(slots - i - 1), glm::mat4(1)});
        return binding;
    }
    GtsModelPrimitive triangle()
    {
        GtsModelPrimitive primitive;
        primitive.attributes.push_back(
            {GtsVertexSemantic::Position, 0, std::vector<glm::vec3>{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}}});
        primitive.indices = {0, 1, 2};
        return primitive;
    }
    void addInfluences(GtsModelPrimitive& primitive, glm::uvec4 joints, glm::vec4 weights, uint32_t set = 0)
    {
        const auto count = std::get<std::vector<glm::vec3>>(primitive.attributes[0].values).size();
        primitive.attributes.push_back({GtsVertexSemantic::Joints, set, std::vector<glm::uvec4>(count, joints)});
        primitive.attributes.push_back({GtsVertexSemantic::Weights, set, std::vector<glm::vec4>(count, weights)});
    }
    GtsModelPrimitive weightedTriangle()
    {
        auto primitive = triangle();
        addInfluences(primitive, {0, 0, 0, 0}, {1, 0, 0, 0});
        return primitive;
    }
    GtsVertexAttribute& stream(GtsModelPrimitive& primitive, GtsVertexSemantic semantic, uint32_t set = 0)
    {
        for (auto& attribute : primitive.attributes)
            if (attribute.semantic == semantic && attribute.setIndex == set)
                return attribute;
        throw std::runtime_error("Missing test stream");
    }
    void diagnostic(const GtsSkinnedMeshPreparationResult& result, const char* code, bool success = false)
    {
        require(result.succeeded() == success && (result.mesh() != nullptr) == success, "Unexpected result state");
        require(std::any_of(result.diagnostics().begin(),
                            result.diagnostics().end(),
                            [&](const auto& d)
                            {
                                return d.code == code && !d.location.empty();
                            }),
                code);
    }
    void unchanged(const GtsModelMesh& before, const GtsModelMesh& after)
    {
        require(before.name == after.name && before.primitives.size() == after.primitives.size(), "Mesh unchanged");
        for (size_t p = 0; p < before.primitives.size(); ++p)
        {
            const auto& a = before.primitives[p];
            const auto& b = after.primitives[p];
            require(a.topology == b.topology && a.indices == b.indices && a.materialIndex == b.materialIndex &&
                        a.attributes.size() == b.attributes.size(),
                    "Primitive unchanged");
            for (size_t i = 0; i < a.attributes.size(); ++i)
                require(a.attributes[i].semantic == b.attributes[i].semantic &&
                            a.attributes[i].setIndex == b.attributes[i].setIndex &&
                            a.attributes[i].values == b.attributes[i].values,
                        "Canonical stream unchanged");
        }
    }
    void basicInfluences()
    {
        static_assert(!std::is_same_v<GtsStaticVertex, GtsSkinnedVertex>);
        static_assert(!std::is_invocable_v<decltype(&prepareGtsSkinnedMesh), const GtsModelMesh&>);
        static_assert(!std::is_invocable_v<decltype(&prepareGtsSkinnedMesh), const GtsModelMesh&, std::nullptr_t>);
        const auto one = prepareGtsSkinnedMesh({"one", {weightedTriangle()}}, makeBinding(1));
        require(one.succeeded() && one.mesh()->vertices.size() == 3, "Minimal one-joint triangle");
        const auto binding = makeBinding();
        for (glm::length_t count = 1; count <= 4; ++count)
        {
            auto       p = triangle();
            glm::uvec4 joints(0);
            glm::vec4  weights(0);
            for (glm::length_t i = 0; i < count; ++i)
            {
                joints[i]  = static_cast<uint32_t>(i + 2);
                weights[i] = 1.0f / count;
            }
            addInfluences(p, joints, weights);
            GtsModelMesh input{"influences", {p}};
            const auto   before = input;
            const auto   result = prepareGtsSkinnedMesh(input, binding);
            require(result.succeeded() && !result.hasWarnings(), "One to four effective influences succeed");
            require(result.mesh()->influences.maxSourceInfluenceCount == static_cast<size_t>(count) &&
                        result.mesh()->influences.reducedInfluenceVertices == 0,
                    "No reduction metadata");
            for (const auto& v : result.mesh()->vertices)
            {
                require(v.joints == joints, "Skin-local slots preserved, safe zero fillers");
                for (glm::length_t i = 0; i < 4; ++i)
                    near(v.weights[i], weights[i]);
            }
            unchanged(before, input);
        }
        auto p = triangle();
        addInfluences(p, {3, 0, 6, 0}, {0.25f, 0, 0.75f, 0}, 7);
        const auto sparse = prepareGtsSkinnedMesh({"noncontiguous set", {p}}, binding);
        require(sparse.succeeded() && sparse.mesh()->vertices[0].joints == glm::uvec4(3, 6, 0, 0),
                "Noncontiguous set indices and zero-weight compaction work");
        auto secondBinding                              = binding;
        secondBinding.joints[3].skeletonNodeIndex       = 0;
        secondBinding.joints[6].skeletonNodeIndex       = 0; // duplicate remaps remain legal
        secondBinding.joints[3].inverseBindMatrix[3][0] = 123;
        const auto secondBefore                         = secondBinding;
        const auto second                               = prepareGtsSkinnedMesh({"same geometry", {p}}, secondBinding);
        require(second.succeeded() && second.mesh()->vertices[0].joints == sparse.mesh()->vertices[0].joints &&
                    second.mesh()->vertices[0].pos == sparse.mesh()->vertices[0].pos,
                "Different remaps/inverse binds do not alter prepared stored geometry");
        require(secondBinding.joints[3].inverseBindMatrix[3][0] == secondBefore.joints[3].inverseBindMatrix[3][0] &&
                    areGtsSkeletonCompatibilitiesEqual(secondBinding.targetSkeletonCompatibility,
                                                       secondBefore.targetSkeletonCompatibility),
                "Binding unchanged");
    }
    void reduction()
    {
        auto p = triangle();
        addInfluences(p, {0, 1, 2, 3}, {0.05f, 0.1f, 0.2f, 0.25f});
        addInfluences(p, {7, 0, 0, 0}, {0.4f, 0, 0, 0}, 1);
        GtsModelMesh input{"five", {p}};
        const auto   before = input;
        const auto   result = prepareGtsSkinnedMesh(input, makeBinding());
        diagnostic(result, "SKINNED_INFLUENCES_REDUCED", true);
        require(result.hasWarnings() && result.mesh()->vertices[0].joints == glm::uvec4(7, 3, 2, 1),
                "Strong later-set influence wins");
        for (const auto& v : result.mesh()->vertices)
        {
            near(v.weights[0], 0.4f / 0.95f);
            near(v.weights[1], 0.25f / 0.95f);
            near(v.weights[2], 0.2f / 0.95f);
            near(v.weights[3], 0.1f / 0.95f);
            near(v.weights.x + v.weights.y + v.weights.z + v.weights.w, 1);
        }
        require(result.mesh()->influences.reducedInfluenceVertices == 3 &&
                    result.mesh()->influences.maxSourceInfluenceCount == 5 &&
                    result.mesh()->primitives[0].influences.reducedInfluenceVertices == 3,
                "Reduction is counted");
        unchanged(before, input);
        p = triangle();
        addInfluences(p, {4, 5, 6, 7}, glm::vec4(0.125f), 9);
        addInfluences(p, {3, 2, 1, 0}, glm::vec4(0.125f), 2);
        const auto tied = prepareGtsSkinnedMesh({"eight", {p}}, makeBinding());
        require(tied.succeeded() && tied.mesh()->vertices[0].joints == glm::uvec4(3, 2, 1, 0) &&
                    tied.mesh()->vertices[0].weights == glm::vec4(0.25f) &&
                    tied.mesh()->influences.maxSourceInfluenceCount == 8,
                "Ties use numeric set then XYZW order");
        std::reverse(p.attributes.begin(), p.attributes.end());
        const auto reordered = prepareGtsSkinnedMesh({"eight", {p}}, makeBinding());
        require(reordered.succeeded() && reordered.mesh()->vertices[0].joints == tied.mesh()->vertices[0].joints &&
                    reordered.mesh()->vertices[0].weights == tied.mesh()->vertices[0].weights,
                "Attribute insertion order cannot affect reduction");
        p = triangle();
        addInfluences(p, {0, 1, 2, 3}, {0.01f, 0.02f, 0.03f, 0.04f});
        addInfluences(p, {4, 5, 6, 7}, {0.1f, 0.2f, 0.25f, 0.35f}, 1);
        const auto eight = prepareGtsSkinnedMesh({"eight unequal", {p}}, makeBinding());
        require(eight.succeeded() && eight.mesh()->vertices[0].joints == glm::uvec4(7, 6, 5, 4),
                "All four strongest influences may come from set 1");
        near(eight.mesh()->vertices[0].weights[0], 0.35f / 0.9f);
        p                                                                                   = weightedTriangle();
        std::get<std::vector<glm::vec4>>(stream(p, GtsVertexSemantic::Weights).values)[0].x = 1.00005f;
        const auto approximate = prepareGtsSkinnedMesh({"tolerance", {p}}, makeBinding());
        require(approximate.succeeded() && approximate.mesh()->vertices[0].weights.x == 1 && !approximate.hasWarnings(),
                "Valid approximate canonical totals normalize even without reduction");
    }
    void geometryAndRanges()
    {
        auto a = weightedTriangle();
        a.attributes.push_back({GtsVertexSemantic::Normal, 0, std::vector<glm::vec3>(3, {0, 0, 2})});
        a.attributes.push_back({GtsVertexSemantic::Tangent, 0, std::vector<glm::vec4>(3, {2, 0, 0, -1})});
        a.attributes.push_back({GtsVertexSemantic::Color, 0, std::vector<glm::vec4>(3, {0.2f, 0.3f, 0.4f, 0.5f})});
        a.attributes.push_back({GtsVertexSemantic::TexCoord, 0, std::vector<glm::vec2>{{0, 0}, {1, 0}, {0, 1}}});
        a.materialIndex = 4;
        auto b          = weightedTriangle();
        b.attributes.push_back(a.attributes.back());
        b.materialIndex = 2;
        auto c          = weightedTriangle();
        c.attributes.push_back({GtsVertexSemantic::TexCoord, 1, std::vector<glm::vec2>(3, glm::vec2(9))});
        c.materialIndex = 4;
        GtsModelMesh input{"ranges", {a, b, c}};
        const auto   before = input;
        const auto   result = prepareGtsSkinnedMesh(input, makeBinding());
        diagnostic(result, "SKINNED_ATTRIBUTE_IGNORED", true);
        const auto& mesh = *result.mesh();
        require(mesh.name == input.name && mesh.vertices.size() == 9 &&
                    mesh.indices == std::vector<uint32_t>{0, 1, 2, 3, 4, 5, 6, 7, 8},
                "Geometry concatenates/rebases");
        for (size_t i = 0; i < 3; ++i)
        {
            const auto& range = mesh.primitives[i];
            require(range.firstIndex == i * 3 && range.indexCount == 3 && range.metadata.vertexCount == 3 &&
                        range.materialIndex == input.primitives[i].materialIndex,
                    "Material runs remain separate");
        }
        require(mesh.vertices[0].normal == glm::vec3(0, 0, 2) && mesh.vertices[0].tangent == glm::vec4(2, 0, 0, -1) &&
                    mesh.vertices[0].color == glm::vec4(0.2f, 0.3f, 0.4f, 0.5f) &&
                    mesh.vertices[1].texCoord == glm::vec2(1, 0),
                "Authored streams preserved");
        require(!mesh.primitives[0].metadata.generatedNormals && !mesh.primitives[0].metadata.generatedTangents &&
                    mesh.primitives[1].metadata.generatedNormals && mesh.primitives[1].metadata.generatedTangents &&
                    mesh.primitives[2].metadata.generatedNormals && !mesh.primitives[2].metadata.generatedTangents,
                "Per-primitive generation follows static profile");
        require(mesh.vertices[3].normal == glm::vec3(0, 0, 1) && mesh.vertices[3].tangent == glm::vec4(1, 0, 0, 1) &&
                    mesh.vertices[6].texCoord == glm::vec2(0) && mesh.vertices[6].color == glm::vec4(1) &&
                    mesh.vertices[6].tangent == glm::vec4(1, 0, 0, 1),
                "Generated geometry and missing defaults");
        require(mesh.metadata.vertexCount == 9 && mesh.metadata.indexCount == 9 && mesh.metadata.generatedNormals &&
                    mesh.metadata.generatedTangents &&
                    mesh.metadata.attributes == (VertexAttributeFlags::Position | VertexAttributeFlags::Normal),
                "Metadata uses intersection and any-of generation");
        unchanged(before, input);
        // Compare the shared conversion against the existing static profile for these same non-skin streams.
        for (auto& p : input.primitives)
            std::erase_if(p.attributes,
                          [](const auto& attribute)
                          {
                              return attribute.semantic == GtsVertexSemantic::Joints ||
                                     attribute.semantic == GtsVertexSemantic::Weights;
                          });
        const auto staticResult = prepareGtsStaticMesh(input);
        require(staticResult.succeeded(), "Static reference succeeds");
        for (size_t i = 0; i < mesh.vertices.size(); ++i)
        {
            const auto& v = mesh.vertices[i];
            const auto& s = staticResult.mesh()->vertices[i];
            require(v.pos == s.pos && v.normal == s.normal && v.tangent == s.tangent && v.color == s.color &&
                        v.texCoord == s.texCoord,
                    "Both profiles use identical base geometry preparation");
        }
        std::erase_if(a.attributes,
                      [](const auto& attr)
                      {
                          return attr.semantic == GtsVertexSemantic::Tangent;
                      });
        const auto normals = prepareGtsSkinnedMesh({"normal restoration", {a}}, makeBinding());
        require(normals.succeeded() && normals.mesh()->metadata.generatedTangents &&
                    normals.mesh()->vertices[0].normal == glm::vec3(0, 0, 2),
                "Tangent generation preserves authored normals");
        auto quad                 = triangle();
        quad.attributes[0].values = std::vector<glm::vec3>{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0}};
        quad.indices              = {0, 1, 2, 2, 1, 3};
        addInfluences(quad, {0, 0, 0, 0}, {1, 0, 0, 0});
        auto sequential = weightedTriangle();
        sequential.indices.clear();
        const auto rebased = prepareGtsSkinnedMesh({"quad", {quad, sequential}}, makeBinding());
        require(rebased.succeeded() && rebased.mesh()->indices == std::vector<uint32_t>{0, 1, 2, 2, 1, 3, 4, 5, 6} &&
                    rebased.mesh()->primitives[1].firstIndex == 6,
                "Rebase uses vertex count, range uses index count");
    }
    void invalidInputs()
    {
        const auto binding = makeBinding();
        diagnostic(prepareGtsSkinnedMesh({"missing", {triangle()}}, binding), "MODEL_SKIN_INFLUENCES_REQUIRED");
        for (const auto semantic : {GtsVertexSemantic::Joints, GtsVertexSemantic::Weights})
        {
            auto p = weightedTriangle();
            std::erase_if(p.attributes,
                          [&](const auto& a)
                          {
                              return a.semantic == semantic;
                          });
            diagnostic(prepareGtsSkinnedMesh({"unpaired", {p}}, binding), "MODEL_SKIN_SET_UNPAIRED");
        }
        for (const glm::length_t component : {0, 3})
        {
            auto p = weightedTriangle();
            std::get<std::vector<glm::uvec4>>(stream(p, GtsVertexSemantic::Joints).values)[0][component] = 8;
            diagnostic(prepareGtsSkinnedMesh({"range", {p}}, binding), "MODEL_SKIN_SLOT_OUT_OF_RANGE");
        }
        for (const auto& [value, code] : std::vector<std::pair<float, const char*>>{
                 {0, "MODEL_SKIN_WEIGHT_ZERO"},
                 {-1, "MODEL_SKIN_WEIGHT_NEGATIVE"},
                 {0.5f, "MODEL_SKIN_WEIGHT_SUM"},
                 {std::numeric_limits<float>::infinity(), "MODEL_ATTRIBUTE_NONFINITE"},
                 {std::numeric_limits<float>::quiet_NaN(), "MODEL_ATTRIBUTE_NONFINITE"}})
        {
            auto p                                                                              = weightedTriangle();
            std::get<std::vector<glm::vec4>>(stream(p, GtsVertexSemantic::Weights).values)[0].x = value;
            diagnostic(prepareGtsSkinnedMesh({"bad weights", {p}}, binding), code);
        }
        auto invalidBinding                        = binding;
        invalidBinding.joints[0].skeletonNodeIndex = 100;
        diagnostic(prepareGtsSkinnedMesh({"bad binding", {weightedTriangle()}}, invalidBinding),
                   "SKINNED_BINDING_NODE_OUT_OF_RANGE");
        invalidBinding                             = binding;
        invalidBinding.targetSkeletonCompatibility = {};
        require(!prepareGtsSkinnedMesh({"no contract", {weightedTriangle()}}, invalidBinding).succeeded(),
                "Missing compatibility rejected");
        invalidBinding = binding;
        invalidBinding.joints.clear();
        require(!prepareGtsSkinnedMesh({"empty binding", {weightedTriangle()}}, invalidBinding).succeeded(),
                "Empty binding rejected");
        invalidBinding                                   = binding;
        invalidBinding.joints[0].inverseBindMatrix[0][3] = 1;
        require(!prepareGtsSkinnedMesh({"nonaffine", {weightedTriangle()}}, invalidBinding).succeeded(),
                "Invalid inverse bind rejected");
        auto p       = weightedTriangle();
        p.indices[2] = 20;
        diagnostic(prepareGtsSkinnedMesh({"bad index", {weightedTriangle(), p}}, binding), "MODEL_INDEX_OUT_OF_RANGE");
        p                                            = weightedTriangle();
        stream(p, GtsVertexSemantic::Weights).values = std::vector<glm::vec4>(2, glm::vec4(1, 0, 0, 0));
        require(!prepareGtsSkinnedMesh({"bad counts", {p}}, binding).succeeded(), "Mismatched counts rejected");
        p                                           = weightedTriangle();
        stream(p, GtsVertexSemantic::Joints).values = std::vector<glm::vec4>(3, glm::vec4(0));
        diagnostic(prepareGtsSkinnedMesh({"bad type", {p}}, binding), "MODEL_ATTRIBUTE_TYPE");
        p          = weightedTriangle();
        p.topology = GtsModelPrimitiveTopology::Points;
        diagnostic(prepareGtsSkinnedMesh({"bad topology", {p}}, binding), "SKINNED_TOPOLOGY_UNSUPPORTED");
        p                                                             = weightedTriangle();
        std::get<std::vector<glm::vec3>>(p.attributes[0].values)[0].x = std::numeric_limits<float>::infinity();
        diagnostic(prepareGtsSkinnedMesh({"bad position", {p}}, binding), "MODEL_ATTRIBUTE_NONFINITE");
        const auto empty = prepareGtsSkinnedMesh({"empty", {}}, binding);
        require(empty.succeeded() && empty.mesh()->vertices.empty() && empty.mesh()->primitives.empty(),
                "Empty mesh follows static profile but still requires valid binding");
    }
} // namespace
int main()
{
    try
    {
        basicInfluences();
        reduction();
        geometryAndRanges();
        invalidInputs();
        std::puts("GtsSkinnedMeshPreparationTest passed");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
