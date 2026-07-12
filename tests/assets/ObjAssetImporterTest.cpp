#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "AssetCooker.h"
#include "ObjAssetImporter.h"

namespace
{
    class TempAssetDirectory
    {
    public:
        TempAssetDirectory()
        {
            const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
            root = std::filesystem::temp_directory_path() /
                ("gravitas_obj_importer_test_" + std::to_string(stamp));
            std::filesystem::create_directories(root);
        }

        ~TempAssetDirectory()
        {
            std::error_code ignored;
            std::filesystem::remove_all(root, ignored);
        }

        std::filesystem::path write(const std::string& relativePath,
                                    const std::string& contents) const
        {
            const std::filesystem::path path = root / relativePath;
            std::filesystem::create_directories(path.parent_path());
            std::ofstream out(path);
            out << contents;
            return path;
        }

        std::filesystem::path path(const std::string& relativePath) const
        {
            return root / relativePath;
        }

    private:
        std::filesystem::path root;
    };

    bool require(bool condition, const char* message)
    {
        if (condition)
            return true;

        std::fprintf(stderr, "FAIL: %s\n", message);
        return false;
    }

    bool near(float lhs, float rhs, float epsilon = 0.0001f)
    {
        return std::abs(lhs - rhs) <= epsilon;
    }

    bool nearVec3(const glm::vec3& lhs, const glm::vec3& rhs, float epsilon = 0.0001f)
    {
        return near(lhs.x, rhs.x, epsilon)
            && near(lhs.y, rhs.y, epsilon)
            && near(lhs.z, rhs.z, epsilon);
    }

    bool nearVec4(const glm::vec4& lhs, const glm::vec4& rhs, float epsilon = 0.0001f)
    {
        return near(lhs.x, rhs.x, epsilon)
            && near(lhs.y, rhs.y, epsilon)
            && near(lhs.z, rhs.z, epsilon)
            && near(lhs.w, rhs.w, epsilon);
    }

    gts::rendering::AssetImportResult importObj(const std::filesystem::path& path)
    {
        gts::rendering::ObjAssetImporter importer;
        gts::rendering::AssetImportRequest request;
        request.sourcePath = path;
        return importer.importAsset(request);
    }

    bool hasDiagnostic(const gts::rendering::AssetImportResult& result,
                       const std::string& code)
    {
        for (const gts::rendering::AssetDiagnostic& diagnostic : result.diagnostics)
        {
            if (diagnostic.code == code)
                return true;
        }
        return false;
    }

    bool hasDependency(const gts::rendering::AssetImportResult& result,
                       const std::filesystem::path& path,
                       gts::rendering::AssetDependencyType type)
    {
        const std::filesystem::path normalized = path.lexically_normal();
        for (const gts::rendering::AssetDependency& dependency : result.dependencies)
        {
            if (dependency.type == type && dependency.path == normalized)
                return true;
        }
        return false;
    }

    bool positionsAndIndicesImport()
    {
        TempAssetDirectory temp;
        const std::filesystem::path path = temp.write(
            "positions.obj",
            "o positions\n"
            "v 0 0 0\n"
            "v 1 0 0\n"
            "v 0 1 0\n"
            "f 1 2 3\n");

        const gts::rendering::AssetImportResult result = importObj(path);
        if (!require(result.succeeded(), "position-only OBJ import succeeds") ||
            !require(result.meshes.size() == 1, "position-only OBJ produces one imported mesh"))
        {
            return false;
        }

        const gts::rendering::ImportedMesh& mesh = result.meshes.front();
        return require(mesh.vertices.size() == 3, "position-only OBJ has three vertices")
            && require(mesh.indices.size() == 3, "position-only OBJ has three indices")
            && require(mesh.primitives.size() == 1, "position-only OBJ records one primitive")
            && require(mesh.primitives.front().firstIndex == 0, "primitive first index is zero")
            && require(mesh.primitives.front().indexCount == 3, "primitive index count is three")
            && require(hasVertexAttribute(mesh.sourceAttributes, VertexAttributeFlags::Position), "source attributes include position")
            && require(hasVertexAttribute(mesh.sourceAttributes, VertexAttributeFlags::Color), "source attributes include color")
            && require(!hasVertexAttribute(mesh.sourceAttributes, VertexAttributeFlags::Normal), "source attributes do not claim missing normals")
            && require(!hasVertexAttribute(mesh.sourceAttributes, VertexAttributeFlags::UV0), "source attributes do not claim missing UVs")
            && require(hasDiagnostic(result, "OBJ_MISSING_NORMALS"), "missing normals diagnostic is emitted")
            && require(hasDiagnostic(result, "OBJ_MISSING_UV0"), "missing UV diagnostic is emitted");
    }

    bool normalsImport()
    {
        TempAssetDirectory temp;
        const std::filesystem::path path = temp.write(
            "normals.obj",
            "o normals\n"
            "v 0 0 0\n"
            "v 1 0 0\n"
            "v 0 1 0\n"
            "vn 0 0 1\n"
            "f 1//1 2//1 3//1\n");

        const gts::rendering::AssetImportResult result = importObj(path);
        if (!require(result.succeeded(), "normal OBJ import succeeds"))
            return false;

        const gts::rendering::ImportedMesh& mesh = result.meshes.front();
        bool ok = require(hasVertexAttribute(mesh.sourceAttributes, VertexAttributeFlags::Normal), "source attributes include normals")
            && require(!mesh.hadMissingNormals, "imported normals are complete");
        for (const Vertex& vertex : mesh.vertices)
            ok = require(nearVec3(vertex.normal, {0.0f, 0.0f, 1.0f}), "normal value is imported") && ok;
        return ok;
    }

    bool uv0ImportsWithCurrentVFlip()
    {
        TempAssetDirectory temp;
        const std::filesystem::path path = temp.write(
            "uv.obj",
            "o uv\n"
            "v 0 0 0\n"
            "v 1 0 0\n"
            "v 0 1 0\n"
            "vt 0 0\n"
            "vt 1 0.25\n"
            "vt 0.5 1\n"
            "f 1/1 2/2 3/3\n");

        const gts::rendering::AssetImportResult result = importObj(path);
        if (!require(result.succeeded(), "UV OBJ import succeeds"))
            return false;

        const gts::rendering::ImportedMesh& mesh = result.meshes.front();
        return require(hasVertexAttribute(mesh.sourceAttributes, VertexAttributeFlags::UV0), "source attributes include UV0")
            && require(near(mesh.vertices[0].texCoord.x, 0.0f), "first UV U is imported")
            && require(near(mesh.vertices[0].texCoord.y, 1.0f), "first UV V is flipped")
            && require(near(mesh.vertices[1].texCoord.y, 0.75f), "second UV V is flipped")
            && require(near(mesh.vertices[2].texCoord.y, 0.0f), "third UV V is flipped");
    }

    bool missingNormalAndUvStateIsRecorded()
    {
        TempAssetDirectory temp;
        const std::filesystem::path path = temp.write(
            "missing.obj",
            "o missing\n"
            "v 0 0 0\n"
            "v 1 0 0\n"
            "v 0 1 0\n"
            "f 1 2 3\n");

        const gts::rendering::AssetImportResult result = importObj(path);
        if (!require(result.succeeded(), "missing data OBJ import succeeds with warnings"))
            return false;

        const gts::rendering::ImportedMesh& mesh = result.meshes.front();
        return require(mesh.hadMissingNormals, "missing normal state is recorded")
            && require(mesh.hadMissingTexCoords, "missing UV state is recorded")
            && require(hasDiagnostic(result, "OBJ_MISSING_NORMALS"), "structured missing-normal diagnostic exists")
            && require(hasDiagnostic(result, "OBJ_MISSING_UV0"), "structured missing-UV diagnostic exists");
    }

    bool vertexColorsImport()
    {
        TempAssetDirectory temp;
        const std::filesystem::path path = temp.write(
            "colors.obj",
            "o colors\n"
            "v 0 0 0 1 0 0\n"
            "v 1 0 0 0 1 0\n"
            "v 0 1 0 0 0 1\n"
            "f 1 2 3\n");

        const gts::rendering::AssetImportResult result = importObj(path);
        if (!require(result.succeeded(), "vertex-color OBJ import succeeds"))
            return false;

        const gts::rendering::ImportedMesh& mesh = result.meshes.front();
        return require(nearVec4(mesh.vertices[0].color, {1.0f, 0.0f, 0.0f, 1.0f}), "first vertex color is imported")
            && require(nearVec4(mesh.vertices[1].color, {0.0f, 1.0f, 0.0f, 1.0f}), "second vertex color is imported")
            && require(nearVec4(mesh.vertices[2].color, {0.0f, 0.0f, 1.0f, 1.0f}), "third vertex color is imported");
    }

    bool multipleShapesMaterialsAndDependencies()
    {
        TempAssetDirectory temp;
        const std::filesystem::path mtlPath = temp.write(
            "mats.mtl",
            "newmtl red\n"
            "Kd 1 0 0\n"
            "map_Kd missing_red.png\n"
            "newmtl blue\n"
            "Kd 0 0 1\n");
        const std::filesystem::path objPath = temp.write(
            "multi.obj",
            "mtllib mats.mtl\n"
            "o first\n"
            "v 0 0 0\n"
            "v 1 0 0\n"
            "v 0 1 0\n"
            "usemtl red\n"
            "f 1 2 3\n"
            "o second\n"
            "v 2 0 0\n"
            "v 3 0 0\n"
            "v 2 1 0\n"
            "usemtl blue\n"
            "f 4 5 6\n");

        const gts::rendering::AssetImportResult result = importObj(objPath);
        if (!require(result.succeeded(), "multi-shape OBJ import succeeds"))
            return false;

        const gts::rendering::ImportedMesh& mesh = result.meshes.front();
        return require(result.materials.size() == 2, "MTL materials are imported")
            && require(mesh.primitives.size() == 2, "shape boundaries are preserved as primitives")
            && require(mesh.primitives[0].name == "first", "first shape name is preserved")
            && require(mesh.primitives[0].materialName == "red", "first material assignment is preserved")
            && require(mesh.primitives[1].name == "second", "second shape name is preserved")
            && require(mesh.primitives[1].materialName == "blue", "second material assignment is preserved")
            && require(hasDependency(result, mtlPath, gts::rendering::AssetDependencyType::MaterialLibrary), "MTL dependency is recorded")
            && require(hasDependency(result, temp.path("missing_red.png"), gts::rendering::AssetDependencyType::Texture), "texture dependency is recorded")
            && require(hasDiagnostic(result, "ASSET_DEPENDENCY_MISSING"), "missing texture dependency diagnostic is emitted");
    }

    bool deduplicationPreservesTupleIdentity()
    {
        TempAssetDirectory temp;
        const std::filesystem::path path = temp.write(
            "dedup.obj",
            "o dedup\n"
            "v 0 0 0\n"
            "v 1 0 0\n"
            "v 0 1 0\n"
            "vt 0 0\n"
            "vt 1 0\n"
            "vt 0 1\n"
            "vn 0 0 1\n"
            "vn 0 0 -1\n"
            "f 1/1/1 2/2/1 3/3/1\n"
            "f 1/1/1 3/3/1 2/2/1\n"
            "f 1/1/2 2/2/2 3/3/2\n");

        const gts::rendering::AssetImportResult result = importObj(path);
        if (!require(result.succeeded(), "dedup OBJ import succeeds"))
            return false;

        const gts::rendering::ImportedMesh& mesh = result.meshes.front();
        return require(mesh.indices.size() == 9, "all indices are emitted")
            && require(mesh.vertices.size() == 6, "same position/UV with different normal creates distinct vertices")
            && require(nearVec3(mesh.vertices[0].normal, {0.0f, 0.0f, 1.0f}), "first normal tuple is +Z")
            && require(nearVec3(mesh.vertices[3].normal, {0.0f, 0.0f, -1.0f}), "second normal tuple is -Z");
    }

    bool invalidAndMissingObjDiagnostics()
    {
        TempAssetDirectory temp;
        const gts::rendering::AssetImportResult missing = importObj(temp.path("missing.obj"));
        bool ok = require(!missing.succeeded(), "missing OBJ fails")
            && require(hasDiagnostic(missing, "ASSET_SOURCE_MISSING"), "missing OBJ diagnostic is structured");

        const std::filesystem::path invalidPath = temp.write(
            "invalid.obj",
            "o invalid\n"
            "v 0 0 0\n"
            "f 1 2 3\n");
        const gts::rendering::AssetImportResult invalid = importObj(invalidPath);
        ok = require(!invalid.succeeded(), "out-of-range OBJ fails") && ok;
        ok = require(hasDiagnostic(invalid, "OBJ_POSITION_INDEX_OUT_OF_RANGE"), "out-of-range index diagnostic is structured") && ok;
        return ok;
    }

    bool registrySelection()
    {
        gts::rendering::AssetImporterRegistry registry;
        registry.registerImporter(std::make_unique<gts::rendering::ObjAssetImporter>());

        const bool extensionSelected =
            registry.findImporter("mesh.obj") != nullptr;
        const bool explicitSelected =
            registry.findImporter(
                "mesh.asset",
                "obj",
                gts::rendering::AssetImportCapability::Meshes) != nullptr;
        const bool unsupportedCapabilityRejected =
            registry.findImporter(
                "mesh.obj",
                {},
                gts::rendering::AssetImportCapability::Nodes) == nullptr;

        return require(extensionSelected, "registry selects OBJ importer by extension")
            && require(explicitSelected, "registry supports explicit importer override")
            && require(unsupportedCapabilityRejected, "registry filters unsupported capabilities");
    }

    bool importedMeshCooksWithoutGpu()
    {
        TempAssetDirectory temp;
        const std::filesystem::path path = temp.write(
            "cook.obj",
            "o cook\n"
            "v 0 0 0\n"
            "v 1 0 0\n"
            "v 0 1 0\n"
            "vt 0 0\n"
            "vt 1 0\n"
            "vt 0 1\n"
            "f 1/1 2/2 3/3\n");

        const gts::rendering::AssetImportResult result = importObj(path);
        if (!require(result.succeeded(), "cook fixture imports"))
            return false;

        gts::rendering::MeshAssetData mesh =
            gts::rendering::cookImportedMesh(result.meshes.front());
        const MeshGeometryMetadata metadata =
            gts::rendering::meshMetadataFromAssetData(mesh);

        return require(mesh.vertices.size() == 3, "cooked mesh keeps vertices")
            && require(mesh.indices.size() == 3, "cooked mesh keeps indices")
            && require(mesh.submeshes.size() == 1, "cooked mesh keeps primitive range")
            && require(mesh.bounds.valid, "cooked mesh computes bounds")
            && require(metadata.generatedNormals, "cooked mesh preserves existing missing-normal generation behavior")
            && require(metadata.generatedTangents, "cooked mesh generates tangents when UV0 exists");
    }
}

int main()
{
    bool ok = true;
    ok = positionsAndIndicesImport() && ok;
    ok = normalsImport() && ok;
    ok = uv0ImportsWithCurrentVFlip() && ok;
    ok = missingNormalAndUvStateIsRecorded() && ok;
    ok = vertexColorsImport() && ok;
    ok = multipleShapesMaterialsAndDependencies() && ok;
    ok = deduplicationPreservesTupleIdentity() && ok;
    ok = invalidAndMissingObjDiagnostics() && ok;
    ok = registrySelection() && ok;
    ok = importedMeshCooksWithoutGpu() && ok;

    if (!ok)
        return 1;

    std::printf("ObjAssetImporterTest passed\n");
    return 0;
}
