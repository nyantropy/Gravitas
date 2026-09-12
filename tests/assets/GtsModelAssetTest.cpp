#include "assets/model/GtsModelAsset.h"
#include "assets/model/GtsModelImportResult.h"
#include "assets/model/GtsModelValidation.h"
#include "assets/model/IGtsModelImporter.h"

#include <cstdint>
#include <cstdio>
#include <exception>
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
        {
            throw std::runtime_error(message);
        }
    }

    void requireError(const GtsModelValidationResult& result, const std::string& code)
    {
        require(!result.isValid(), "Malformed data must fail validation");
        for (const auto& diagnostic : result.diagnostics)
        {
            if (diagnostic.code == code)
            {
                require(diagnostic.severity == GtsModelDiagnosticSeverity::Error, "Validation reports errors");
                require(!diagnostic.message.empty() && !diagnostic.location.empty(), "Error has context");
                return;
            }
        }
        throw std::runtime_error("Expected diagnostic: " + code);
    }

    GtsModelPrimitive triangle()
    {
        GtsModelPrimitive primitive;
        primitive.attributes.push_back({GtsVertexSemantic::Position, 0,
            std::vector<glm::vec3>{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}}});
        primitive.indices = {0, 1, 2};
        return primitive;
    }

    GtsModelAsset model()
    {
        GtsModelAsset asset;
        asset.meshes = {{"first", {triangle(), triangle()}}, {"second", {triangle()}}};
        asset.materials = {{"red"}, {"blue"}};
        asset.meshes[0].primitives[0].materialIndex = 0;
        asset.meshes[0].primitives[1].materialIndex = 1;
        asset.nodes.resize(3);
        asset.nodes[0].name = "parent";
        asset.nodes[0].children = {1};
        asset.nodes[0].localTransform[3] = glm::vec4(2, 3, 4, 1);
        asset.nodes[1].meshIndex = 0;
        asset.nodes[2].meshIndex = 1;
        asset.rootNodes = {0, 2};
        return asset;
    }

    void primitiveValidation()
    {
        auto primitive = triangle();
        require(validateGtsModelPrimitive(primitive).isValid(), "Positions and indices alone are valid");
        require(!primitive.materialIndex, "No material is represented safely");
        require(primitive.attributes.size() == 1, "Validation does not generate attributes");
        primitive.attributes.push_back({GtsVertexSemantic::Normal, 0, std::vector<glm::vec3>(3, glm::vec3(0, 0, 1))});
        primitive.attributes.push_back({GtsVertexSemantic::Tangent, 0, std::vector<glm::vec4>(3, glm::vec4(1, 0, 0, 1))});
        primitive.attributes.push_back({GtsVertexSemantic::TexCoord, 0, std::vector<glm::vec2>(3, glm::vec2(0))});
        primitive.attributes.push_back({GtsVertexSemantic::TexCoord, 1, std::vector<glm::vec2>(3, glm::vec2(1))});
        primitive.attributes.push_back({GtsVertexSemantic::Color, 2, std::vector<glm::vec4>(3, glm::vec4(1))});
        primitive.attributes.push_back({GtsVertexSemantic::Joints, 0, std::vector<glm::uvec4>(3, glm::uvec4(0))});
        primitive.attributes.push_back({GtsVertexSemantic::Weights, 0, std::vector<glm::vec4>(3, glm::vec4(1, 0, 0, 0))});
        require(validateGtsModelPrimitive(primitive).isValid(), "All canonical types and numbered semantic sets are valid");
        require(std::get<std::vector<glm::vec2>>(primitive.attributes[3].values)[0] == glm::vec2(0)
            && std::get<std::vector<glm::vec2>>(primitive.attributes[4].values)[0] == glm::vec2(1),
            "Independent UV sets retain their values");

        auto invalid = primitive;
        std::get<std::vector<glm::vec3>>(invalid.attributes[1].values).pop_back();
        requireError(validateGtsModelPrimitive(invalid), "MODEL_ATTRIBUTE_COUNT");
        invalid = primitive;
        std::get<std::vector<glm::vec3>>(invalid.attributes[1].values).clear();
        requireError(validateGtsModelPrimitive(invalid), "MODEL_ATTRIBUTE_COUNT");
        invalid = primitive;
        invalid.attributes.push_back(primitive.attributes[3]);
        requireError(validateGtsModelPrimitive(invalid), "MODEL_ATTRIBUTE_DUPLICATE");
        invalid = primitive;
        invalid.indices[2] = 3;
        requireError(validateGtsModelPrimitive(invalid), "MODEL_INDEX_OUT_OF_RANGE");
        invalid.indices[2] = std::numeric_limits<uint32_t>::max();
        requireError(validateGtsModelPrimitive(invalid), "MODEL_INDEX_OUT_OF_RANGE");
        invalid = primitive;
        invalid.attributes.erase(invalid.attributes.begin());
        requireError(validateGtsModelPrimitive(invalid), "MODEL_POSITION_REQUIRED");
        invalid = triangle();
        invalid.attributes[0].setIndex = 1;
        requireError(validateGtsModelPrimitive(invalid), "MODEL_POSITION_REQUIRED");
        invalid = triangle();
        invalid.attributes[0].values = std::vector<glm::vec3>{};
        requireError(validateGtsModelPrimitive(invalid), "MODEL_POSITION_REQUIRED");
        invalid = primitive;
        invalid.attributes[6].values = std::vector<glm::vec4>(3, glm::vec4(0));
        requireError(validateGtsModelPrimitive(invalid), "MODEL_ATTRIBUTE_TYPE");
        invalid = primitive;
        invalid.attributes[3].values = std::vector<glm::vec3>(3, glm::vec3(0));
        requireError(validateGtsModelPrimitive(invalid), "MODEL_ATTRIBUTE_TYPE");
        invalid = triangle();
        invalid.attributes[0].semantic = static_cast<GtsVertexSemantic>(99);
        requireError(validateGtsModelPrimitive(invalid), "MODEL_ATTRIBUTE_TYPE");
        for (float value : {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        {
            invalid = triangle();
            std::get<std::vector<glm::vec3>>(invalid.attributes[0].values)[0].x = value;
            requireError(validateGtsModelPrimitive(invalid), "MODEL_ATTRIBUTE_NONFINITE");
        }

        primitive = triangle();
        primitive.indices.clear();
        require(validateGtsModelPrimitive(primitive).isValid(), "Nonindexed triangles use sequential vertices");
        primitive.indices = {0, 1};
        requireError(validateGtsModelPrimitive(primitive), "MODEL_TOPOLOGY_COUNT");
        primitive.topology = GtsModelPrimitiveTopology::Lines;
        require(validateGtsModelPrimitive(primitive).isValid(), "Line lists are valid");
        primitive.indices = {0};
        requireError(validateGtsModelPrimitive(primitive), "MODEL_TOPOLOGY_COUNT");
        primitive.topology = GtsModelPrimitiveTopology::Points;
        require(validateGtsModelPrimitive(primitive).isValid(), "Point lists are valid");
        primitive.topology = static_cast<GtsModelPrimitiveTopology>(99);
        requireError(validateGtsModelPrimitive(primitive), "MODEL_TOPOLOGY_INVALID");
    }

    void modelValidation()
    {
        const auto asset = model();
        require(validateGtsModelAsset(asset).isValid(), "Multiple roots, meshes, primitives, and materials are valid");
        require(asset.meshes.size() == 2 && asset.meshes[0].primitives.size() == 2, "Mesh and primitive boundaries survive");
        require(asset.meshes[0].primitives[0].materialIndex == 0
            && asset.meshes[0].primitives[1].materialIndex == 1, "Materials belong to individual primitives");
        require(asset.nodes[0].localTransform[3] == glm::vec4(2, 3, 4, 1), "Local transform retained");
        require(asset.nodes[1].localTransform == glm::mat4(1), "Default local transform is identity");
        require(validateGtsModelAsset({}).isValid(), "An empty asset is a valid container");

        auto invalid = asset;
        invalid.meshes[1].primitives[0].materialIndex = 2;
        requireError(validateGtsModelAsset(invalid), "MODEL_MATERIAL_OUT_OF_RANGE");
        invalid = asset;
        invalid.nodes[1].meshIndex = 2;
        requireError(validateGtsModelAsset(invalid), "MODEL_MESH_OUT_OF_RANGE");
        invalid = asset;
        invalid.nodes[0].children = {3};
        requireError(validateGtsModelAsset(invalid), "MODEL_CHILD_OUT_OF_RANGE");
        invalid = asset;
        invalid.rootNodes = {3};
        requireError(validateGtsModelAsset(invalid), "MODEL_ROOT_OUT_OF_RANGE");
        invalid = asset;
        invalid.rootNodes.push_back(0);
        requireError(validateGtsModelAsset(invalid), "MODEL_ROOT_DUPLICATE");
        invalid = asset;
        invalid.rootNodes.push_back(1);
        requireError(validateGtsModelAsset(invalid), "MODEL_ROOT_HAS_PARENT");
        invalid = asset;
        invalid.nodes[0].children.push_back(1);
        requireError(validateGtsModelAsset(invalid), "MODEL_NODE_MULTIPLE_PARENTS");
        invalid = asset;
        invalid.nodes[2].children.push_back(1);
        requireError(validateGtsModelAsset(invalid), "MODEL_NODE_MULTIPLE_PARENTS");
        invalid = asset;
        invalid.rootNodes = {0};
        requireError(validateGtsModelAsset(invalid), "MODEL_NODE_UNREACHABLE");
        invalid = asset;
        invalid.nodes[1].children = {0};
        requireError(validateGtsModelAsset(invalid), "MODEL_ROOT_HAS_PARENT");
        invalid = asset;
        invalid.nodes[2].children = {2};
        invalid.rootNodes = {0};
        requireError(validateGtsModelAsset(invalid), "MODEL_NODE_UNREACHABLE");
        invalid = asset;
        invalid.nodes[1].children = {0};
        invalid.rootNodes = {2};
        requireError(validateGtsModelAsset(invalid), "MODEL_NODE_UNREACHABLE");
        invalid = asset;
        invalid.nodes[0].localTransform[1][2] = std::numeric_limits<float>::infinity();
        requireError(validateGtsModelAsset(invalid), "MODEL_TRANSFORM_NONFINITE");
        invalid = asset;
        invalid.meshes[1].primitives[0].indices[0] = 3;
        const auto result = validateGtsModelAsset(invalid);
        requireError(result, "MODEL_INDEX_OUT_OF_RANGE");
        require(result.diagnostics[0].location == "meshes[1].primitives[0]", "Nested errors identify the primitive");

        GtsModelAsset deep;
        deep.nodes.resize(10000);
        deep.rootNodes = {0};
        for (uint32_t i = 1; i < deep.nodes.size(); ++i)
        {
            deep.nodes[i - 1].children.push_back(i);
        }
        require(validateGtsModelAsset(deep).isValid(), "Deep hierarchies do not need recursive traversal");
        deep.nodes.back().children = {1};
        requireError(validateGtsModelAsset(deep), "MODEL_NODE_MULTIPLE_PARENTS");
    }

    class FixtureImporter : public IGtsModelImporter
    {
    public:
        GtsModelImportResult importAsset(const GtsModelImportRequest& request) const override
        {
            if (request.sourcePath.empty())
            {
                return GtsModelImportResult::failure({{GtsModelDiagnosticSeverity::Error,
                    "SOURCE_REQUIRED", "A source path is required.", {}}});
            }
            return GtsModelImportResult::success(model());
        }
    };

    void importResults()
    {
        const FixtureImporter fixture;
        const IGtsModelImporter& importer = fixture;
        const auto imported = importer.importAsset({"fixture.model"});
        require(imported.succeeded() && imported.asset(), "Strategy returns a model on success");
        require(imported.diagnostics().empty() && !imported.hasWarnings(), "Clean success is explicit");
        require(imported.asset()->meshes.size() == 2, "Result retains all meshes");
        require(!importer.importAsset({}).succeeded(), "Strategy can report failure");

        const GtsModelDiagnostic warning{GtsModelDiagnosticSeverity::Warning,
            "MISSING_NORMALS", "Source contains no normals.", "fixture.model"};
        const auto warned = GtsModelImportResult::success(model(), {warning});
        require(warned.succeeded() && warned.hasWarnings() && warned.asset(), "Warnings retain a valid asset");
        require(warned.diagnostics().size() == 1, "Warnings are retained");
        const auto failed = GtsModelImportResult::failure({warning});
        require(!failed.succeeded() && !failed.asset() && failed.hasWarnings(), "Failure never exposes an asset");
        require(failed.diagnostics().size() == 2
            && failed.diagnostics()[1].severity == GtsModelDiagnosticSeverity::Error, "Failure always has an error");
        require(!GtsModelImportResult::failure({}).diagnostics().empty(), "Unexplained failure gets a diagnostic");

        auto malformed = model();
        malformed.meshes[0].primitives[0].indices[0] = 99;
        const auto rejected = GtsModelImportResult::success(malformed, {warning});
        require(!rejected.succeeded() && !rejected.asset() && rejected.hasWarnings(), "Invalid canonical data cannot become success");
        const auto error = GtsModelImportResult::success(model(), {{GtsModelDiagnosticSeverity::Error,
            "UNSUPPORTED_REQUIRED_FEATURE", "Unsupported required source feature.", "fixture.model"}});
        require(!error.succeeded() && !error.asset(), "Importer errors discard even structurally valid assets");
    }
}

int main()
{
    try
    {
        primitiveValidation();
        modelValidation();
        importResults();
        std::puts("GtsModelAssetTest passed");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
