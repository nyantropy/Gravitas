#include "GtsStaticMeshPreparation.h"
#include "assets/model/GtsModelAsset.h"

#include <cmath>
#include <cstdio>
#include <limits>
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

    GtsModelPrimitive triangle(float x = 0)
    {
        GtsModelPrimitive primitive;
        primitive.attributes.push_back(
            {GtsVertexSemantic::Position, 0, std::vector<glm::vec3>{{x, 0, 0}, {x + 1, 0, 0}, {x, 1, 0}}});
        primitive.indices = {0, 1, 2};
        return primitive;
    }

    void addUV(GtsModelPrimitive& primitive)
    {
        primitive.attributes.push_back(
            {GtsVertexSemantic::TexCoord, 0, std::vector<glm::vec2>{{0, 0}, {1, 0}, {0, 1}}});
    }

    GtsModelPrimitive authoredTriangle()
    {
        auto primitive = triangle();
        primitive.attributes.push_back({GtsVertexSemantic::Normal, 0, std::vector<glm::vec3>(3, {0, 0, 2})});
        primitive.attributes.push_back({GtsVertexSemantic::Tangent, 0, std::vector<glm::vec4>(3, {2, 0, 0, -1})});
        primitive.attributes.push_back(
            {GtsVertexSemantic::Color, 0, std::vector<glm::vec4>(3, {0.2f, 0.3f, 0.4f, 0.5f})});
        addUV(primitive);
        return primitive;
    }

    const GtsPreparedStaticMesh& succeeded(const GtsStaticMeshPreparationResult& result)
    {
        require(result.succeeded() && result.mesh(), "Preparation should succeed");
        return *result.mesh();
    }

    void unchanged(const GtsModelMesh& original, const GtsModelMesh& input)
    {
        require(original.name == input.name && original.primitives.size() == input.primitives.size(),
                "Mesh input is immutable");
        for (size_t p = 0; p < original.primitives.size(); ++p)
        {
            const auto& a = original.primitives[p];
            const auto& b = input.primitives[p];
            require(a.topology == b.topology && a.indices == b.indices && a.materialIndex == b.materialIndex &&
                        a.attributes.size() == b.attributes.size(),
                    "Primitive input is immutable");
            for (size_t i = 0; i < a.attributes.size(); ++i)
            {
                require(a.attributes[i].semantic == b.attributes[i].semantic &&
                            a.attributes[i].setIndex == b.attributes[i].setIndex &&
                            a.attributes[i].values == b.attributes[i].values,
                        "Canonical attribute values are immutable");
            }
        }
    }

    void diagnostic(const GtsStaticMeshPreparationResult& result,
                    const std::string&                    code,
                    GtsModelDiagnosticSeverity            severity)
    {
        for (const auto& entry : result.diagnostics())
        {
            if (entry.code == code && entry.severity == severity)
            {
                require(!entry.message.empty() && entry.location.starts_with("primitives["),
                        "Diagnostic identifies canonical geometry");
                return;
            }
        }
        throw std::runtime_error("Missing diagnostic " + code);
    }

    void copiedAuthoredStreams()
    {
        GtsModelMesh input{"authored", {authoredTriangle()}};
        input.primitives[0].materialIndex = 42;
        const auto  original              = input;
        const auto  result                = prepareGtsStaticMesh(input);
        const auto& mesh                  = succeeded(result);
        require(mesh.name == "authored" && mesh.vertices.size() == 3 && mesh.indices == std::vector<uint32_t>{0, 1, 2},
                "Name, positions and indexed geometry are copied");
        require(mesh.vertices[1].pos == glm::vec3(1, 0, 0), "Position is exact");
        require(mesh.vertices[2].texCoord == glm::vec2(0, 1), "UV is copied without source-format transforms");
        for (const auto& vertex : mesh.vertices)
        {
            require(vertex.normal == glm::vec3(0, 0, 2), "Authored normals remain unchanged");
            require(vertex.tangent == glm::vec4(2, 0, 0, -1), "Authored tangents remain unchanged");
            require(vertex.color == glm::vec4(0.2f, 0.3f, 0.4f, 0.5f), "Authored color including alpha survives");
        }
        require(mesh.primitives[0].materialIndex == 42,
                "Material index retains containing-model meaning without resolution");
        require(mesh.metadata.attributes == StandardVertexAttributes && !mesh.metadata.generatedNormals &&
                    !mesh.metadata.generatedTangents && !result.hasWarnings(),
                "Authored metadata reports no generation");
        unchanged(original, input);
    }

    void generationAndDefaults()
    {
        GtsModelMesh input{"generated", {triangle()}};
        addUV(input.primitives[0]);
        const auto  original = input;
        const auto  result   = prepareGtsStaticMesh(input);
        const auto& mesh     = succeeded(result);
        require(mesh.metadata.generatedNormals && mesh.metadata.generatedTangents,
                "Normals and UV tangents are generated");
        require(!hasVertexAttribute(mesh.metadata.attributes, VertexAttributeFlags::Color),
                "White defaults do not claim authored colors");
        for (const auto& vertex : mesh.vertices)
        {
            require(vertex.normal == glm::vec3(0, 0, 1), "Generated normal follows triangle winding");
            require(vertex.tangent == glm::vec4(1, 0, 0, 1), "UV derivatives produce expected tangent and handedness");
            require(vertex.color == glm::vec4(1), "Absent colors default to prepared white");
        }
        unchanged(original, input);

        input.primitives[0].attributes.push_back({GtsVertexSemantic::Normal, 0, std::vector<glm::vec3>(3, {0, 0, 3})});
        const auto withNormals = input;
        const auto tangentOnly = prepareGtsStaticMesh(input);
        require(!succeeded(tangentOnly).metadata.generatedNormals && tangentOnly.mesh()->metadata.generatedTangents,
                "Existing normals are used during tangent generation");
        for (const auto& vertex : tangentOnly.mesh()->vertices)
        {
            require(vertex.normal == glm::vec3(0, 0, 3), "Tangent generation does not overwrite authored normals");
        }
        unchanged(withNormals, input);

        GtsModelMesh noUV{"no UV", {triangle()}};
        noUV.primitives[0].indices.clear();
        const auto  noUVOriginal = noUV;
        const auto  fallback     = prepareGtsStaticMesh(noUV);
        const auto& fallbackMesh = succeeded(fallback);
        require(fallbackMesh.indices == std::vector<uint32_t>{0, 1, 2},
                "Nonindexed canonical vertices gain sequential prepared indices");
        require(fallbackMesh.metadata.generatedNormals && !fallbackMesh.metadata.generatedTangents &&
                    !hasVertexAttribute(fallbackMesh.metadata.attributes, VertexAttributeFlags::Tangent) &&
                    !hasVertexAttribute(fallbackMesh.metadata.attributes, VertexAttributeFlags::UV0),
                "Missing UVs prevent tangent generation and retain absent capability flags");
        for (const auto& vertex : fallbackMesh.vertices)
        {
            require(vertex.texCoord == glm::vec2(0) && vertex.tangent == glm::vec4(1, 0, 0, 1) &&
                        vertex.color == glm::vec4(1),
                    "Missing UV, tangent and color use safe prepared defaults");
        }
        unchanged(noUVOriginal, noUV);
    }

    void independentPrimitives()
    {
        GtsModelMesh input{"runs", {authoredTriangle(), triangle(10), triangle(-10)}};
        addUV(input.primitives[1]);
        input.primitives[0].materialIndex = 0;
        input.primitives[1].materialIndex = 1;
        input.primitives[2].materialIndex = 0;
        const auto  original              = input;
        const auto  result                = prepareGtsStaticMesh(input);
        const auto& mesh                  = succeeded(result);
        require(mesh.primitives.size() == 3 && mesh.vertices.size() == 9, "Three primitive boundaries remain distinct");
        require(mesh.indices == std::vector<uint32_t>{0, 1, 2, 3, 4, 5, 6, 7, 8},
                "Local indices are rebased into shared buffers");
        require(mesh.vertices[3].pos == glm::vec3(10, 0, 0) && mesh.vertices[6].pos == glm::vec3(-10, 0, 0),
                "Positions from every primitive survive");
        for (size_t p = 0; p < 3; ++p)
        {
            const auto& range = mesh.primitives[p];
            require(range.firstIndex == p * 3 && range.indexCount == 3 && range.metadata.vertexCount == 3 &&
                        range.metadata.indexCount == 3 && range.materialIndex == input.primitives[p].materialIndex,
                    "Each range retains counts and A/B/A material association");
        }
        require(!mesh.primitives[0].metadata.generatedNormals && !mesh.primitives[0].metadata.generatedTangents &&
                    mesh.primitives[1].metadata.generatedNormals && mesh.primitives[1].metadata.generatedTangents &&
                    mesh.primitives[2].metadata.generatedNormals && !mesh.primitives[2].metadata.generatedTangents,
                "Generation is decided independently per primitive");
        require(mesh.vertices[0].normal == glm::vec3(0, 0, 2) && mesh.vertices[0].tangent == glm::vec4(2, 0, 0, -1),
                "Missing attributes in other primitives do not discard authored streams");
        require(mesh.metadata.attributes == (VertexAttributeFlags::Position | VertexAttributeFlags::Normal) &&
                    mesh.metadata.generatedNormals && mesh.metadata.generatedTangents &&
                    mesh.metadata.vertexCount == 9 && mesh.metadata.indexCount == 9,
                "Mesh metadata intersects capabilities, sums counts and ORs generation flags");
        unchanged(original, input);
    }

    void rebasingUsesVertexCounts()
    {
        auto quad                 = triangle();
        quad.attributes[0].values = std::vector<glm::vec3>{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0}};
        quad.indices              = {0, 1, 2, 2, 1, 3};
        auto sequential           = triangle(10);
        sequential.indices.clear();
        GtsModelMesh input{"indexed and sequential", {quad, sequential}};
        const auto   original = input;
        const auto   result   = prepareGtsStaticMesh(input);
        const auto&  mesh     = succeeded(result);
        require(mesh.vertices.size() == 7 && mesh.indices == std::vector<uint32_t>{0, 1, 2, 2, 1, 3, 4, 5, 6},
                "Index rebasing uses accumulated vertex count, not accumulated index count");
        require(mesh.primitives[0].firstIndex == 0 && mesh.primitives[0].indexCount == 6 &&
                    mesh.primitives[1].firstIndex == 6 && mesh.primitives[1].indexCount == 3 &&
                    mesh.primitives[0].metadata.vertexCount == 4 && mesh.primitives[1].metadata.vertexCount == 3 &&
                    !mesh.primitives[1].materialIndex,
                "Ranges and absent materials survive indexed/nonindexed combination");
        unchanged(original, input);
    }

    void degenerateFallbacks()
    {
        auto primitive                 = triangle();
        primitive.attributes[0].values = std::vector<glm::vec3>{{0, 0, 0}, {0, 1, 0}, {0, 0, 1}};
        primitive.attributes.push_back({GtsVertexSemantic::TexCoord, 0, std::vector<glm::vec2>(3, glm::vec2(0))});
        const auto  result = prepareGtsStaticMesh({"degenerate UV", {primitive}});
        const auto& mesh   = succeeded(result);
        require(mesh.metadata.generatedTangents, "Existing metadata counts generated tangent fallbacks as generated");
        for (const auto& vertex : mesh.vertices)
        {
            require(vertex.normal == glm::vec3(1, 0, 0) && vertex.tangent == glm::vec4(0, 1, 0, 1),
                    "Degenerate UVs reuse the existing normal-relative finite tangent fallback");
        }
        primitive.attributes[0].values = std::vector<glm::vec3>(3, glm::vec3(0));
        const auto degenerate          = prepareGtsStaticMesh({"degenerate positions", {primitive}});
        for (const auto& vertex : succeeded(degenerate).vertices)
        {
            require(vertex.normal == glm::vec3(0, 0, 1) && vertex.tangent == glm::vec4(1, 0, 0, 1),
                    "Degenerate positions receive existing safe finite fallbacks");
        }
    }

    void profileAndValidation()
    {
        GtsModelMesh input{"extra sets", {triangle()}};
        input.primitives[0].attributes.push_back(
            {GtsVertexSemantic::TexCoord, 1, std::vector<glm::vec2>(3, glm::vec2(1))});
        const auto original = input;
        const auto extra    = prepareGtsStaticMesh(input);
        require(succeeded(extra).vertices[0].texCoord == glm::vec2(0) && extra.hasWarnings(),
                "UV1 is not silently substituted for UV0");
        diagnostic(extra, "STATIC_ATTRIBUTE_IGNORED", GtsModelDiagnosticSeverity::Warning);
        unchanged(original, input);
        for (auto semantic : {GtsVertexSemantic::Joints, GtsVertexSemantic::Weights})
        {
            auto               invalid = input;
            GtsVertexAttribute stream;
            stream.semantic = semantic;
            stream.setIndex = 2;
            if (semantic == GtsVertexSemantic::Joints)
                stream.values = std::vector<glm::uvec4>(3, glm::uvec4(0));
            else
                stream.values = std::vector<glm::vec4>(3, glm::vec4(0));
            invalid.primitives[0].attributes.push_back(stream);
            const auto result = prepareGtsStaticMesh(invalid);
            require(!result.succeeded() && !result.mesh() && result.hasWarnings(),
                    "Skinning failure never exposes a partial static mesh");
            diagnostic(result, "STATIC_SKINNING_UNSUPPORTED", GtsModelDiagnosticSeverity::Error);
        }
        for (auto topology : {GtsModelPrimitiveTopology::Points, GtsModelPrimitiveTopology::Lines})
        {
            auto invalid     = triangle();
            invalid.topology = topology;
            if (topology == GtsModelPrimitiveTopology::Lines)
                invalid.indices = {0, 1};
            const auto result = prepareGtsStaticMesh({"nontriangle", {invalid}});
            require(!result.succeeded() && !result.mesh(), "Other topologies require another profile");
            diagnostic(result, "STATIC_TOPOLOGY_UNSUPPORTED", GtsModelDiagnosticSeverity::Error);
        }
        auto invalid = triangle();
        invalid.attributes.clear();
        const auto missing = prepareGtsStaticMesh({"invalid", {invalid}});
        require(!missing.mesh(), "Missing positions cannot become success");
        diagnostic(missing, "MODEL_POSITION_REQUIRED", GtsModelDiagnosticSeverity::Error);
        invalid            = triangle();
        invalid.indices[2] = 9;
        const auto indices = prepareGtsStaticMesh({"invalid", {triangle(), invalid}});
        require(!indices.mesh(), "A bad later primitive cannot expose partially prepared geometry");
        diagnostic(indices, "MODEL_INDEX_OUT_OF_RANGE", GtsModelDiagnosticSeverity::Error);
        invalid                                                             = authoredTriangle();
        std::get<std::vector<glm::vec3>>(invalid.attributes[1].values)[0].x = std::numeric_limits<float>::infinity();
        const auto nonfinite = prepareGtsStaticMesh({"invalid", {invalid}});
        require(!nonfinite.mesh(), "Nonfinite canonical data is rejected, not repaired by prepared defaults");
        diagnostic(nonfinite, "MODEL_ATTRIBUTE_NONFINITE", GtsModelDiagnosticSeverity::Error);
        invalid                      = triangle();
        invalid.attributes[0].values = std::vector<glm::vec2>(3, glm::vec2(0));
        const auto wrongType         = prepareGtsStaticMesh({"invalid", {invalid}});
        require(!wrongType.mesh(), "Invalid attribute types fail before typed conversion");
        diagnostic(wrongType, "MODEL_ATTRIBUTE_TYPE", GtsModelDiagnosticSeverity::Error);
        const auto empty = prepareGtsStaticMesh({"empty", {}});
        require(succeeded(empty).primitives.empty() &&
                    empty.mesh()->metadata.attributes == VertexAttributeFlags::None &&
                    empty.mesh()->metadata.vertexCount == 0 && empty.mesh()->metadata.indexCount == 0,
                "A valid empty canonical mesh produces a valid empty prepared mesh");
    }
} // namespace

int main()
{
    try
    {
        copiedAuthoredStreams();
        generationAndDefaults();
        independentPrimitives();
        rebasingUsesVertexCounts();
        degenerateFallbacks();
        profileAndValidation();
        std::puts("GtsStaticMeshPreparationTest passed");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
