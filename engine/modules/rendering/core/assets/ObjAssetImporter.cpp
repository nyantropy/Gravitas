#include "ObjAssetImporter.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <unordered_map>

#include <tiny_obj_loader.h>

#include "MeshGeometryProcessor.h"

namespace gts::rendering
{
namespace
{
    struct ObjVertexKey
    {
        int position = -1;
        int normal = -1;
        int texCoord = -1;
        glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};

        bool operator==(const ObjVertexKey& other) const
        {
            return position == other.position
                && normal == other.normal
                && texCoord == other.texCoord
                && color == other.color;
        }
    };

    struct ObjVertexKeyHash
    {
        size_t operator()(const ObjVertexKey& key) const
        {
            size_t value = std::hash<int>()(key.position);
            value ^= std::hash<int>()(key.normal + 0x9e3779b9) + (value << 6u) + (value >> 2u);
            value ^= std::hash<int>()(key.texCoord + 0x7f4a7c15) + (value << 6u) + (value >> 2u);
            value ^= std::hash<float>()(key.color.x) + (value << 6u) + (value >> 2u);
            value ^= std::hash<float>()(key.color.y) + (value << 6u) + (value >> 2u);
            value ^= std::hash<float>()(key.color.z) + (value << 6u) + (value >> 2u);
            value ^= std::hash<float>()(key.color.w) + (value << 6u) + (value >> 2u);
            return value;
        }
    };

    bool validVecIndex(int index, size_t elementCount)
    {
        return index >= 0 && static_cast<size_t>(index) < elementCount;
    }

    void addDiagnostic(AssetImportResult& result,
                       AssetDiagnosticSeverity severity,
                       std::string code,
                       std::string message,
                       const std::filesystem::path& sourcePath,
                       uint32_t line = 0)
    {
        result.diagnostics.push_back({
            severity,
            std::move(code),
            std::move(message),
            sourcePath,
            line
        });
    }

    bool hasDependency(const AssetImportResult& result,
                       const std::filesystem::path& path,
                       AssetDependencyType type)
    {
        for (const AssetDependency& dependency : result.dependencies)
        {
            if (dependency.type == type && dependency.path == path)
                return true;
        }
        return false;
    }

    void addDependency(AssetImportResult& result,
                       std::filesystem::path path,
                       AssetDependencyType type,
                       bool required = true)
    {
        path = path.lexically_normal();
        if (hasDependency(result, path, type))
            return;

        result.dependencies.push_back({std::move(path), type, required});
    }

    std::filesystem::path resolveRelativeTo(const std::filesystem::path& baseDirectory,
                                            const std::filesystem::path& path)
    {
        if (path.empty() || path.is_absolute())
            return path.lexically_normal();
        return (baseDirectory / path).lexically_normal();
    }

    std::string trim(const std::string& value)
    {
        const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch)
        {
            return std::isspace(ch) != 0;
        });
        const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch)
        {
            return std::isspace(ch) != 0;
        }).base();

        if (first >= last)
            return {};
        return std::string(first, last);
    }

    std::vector<std::filesystem::path> readObjMaterialLibraries(
        const std::filesystem::path& sourcePath)
    {
        std::ifstream input(sourcePath);
        std::vector<std::filesystem::path> libraries;
        std::string line;
        while (std::getline(input, line))
        {
            const size_t comment = line.find('#');
            if (comment != std::string::npos)
                line.erase(comment);
            line = trim(line);
            if (!line.starts_with("mtllib"))
                continue;

            std::istringstream stream(line);
            std::string keyword;
            stream >> keyword;
            std::string materialLibrary;
            while (stream >> materialLibrary)
                libraries.emplace_back(materialLibrary);
        }
        return libraries;
    }

    glm::vec3 readPosition(const tinyobj::attrib_t& attrib,
                           int vertexIndex,
                           const std::filesystem::path& sourcePath,
                           AssetImportResult& result)
    {
        const size_t count = attrib.vertices.size() / 3u;
        if (!validVecIndex(vertexIndex, count))
        {
            addDiagnostic(
                result,
                AssetDiagnosticSeverity::Error,
                "OBJ_POSITION_INDEX_OUT_OF_RANGE",
                "OBJ position index out of range: " + std::to_string(vertexIndex),
                sourcePath);
            return {0.0f, 0.0f, 0.0f};
        }

        const size_t offset = static_cast<size_t>(vertexIndex) * 3u;
        return {
            attrib.vertices[offset + 0u],
            attrib.vertices[offset + 1u],
            attrib.vertices[offset + 2u]
        };
    }

    glm::vec3 readNormal(const tinyobj::attrib_t& attrib,
                         int normalIndex,
                         const std::filesystem::path& sourcePath,
                         AssetImportResult& result)
    {
        if (normalIndex < 0)
            return {0.0f, 0.0f, 1.0f};

        const size_t count = attrib.normals.size() / 3u;
        if (!validVecIndex(normalIndex, count))
        {
            addDiagnostic(
                result,
                AssetDiagnosticSeverity::Error,
                "OBJ_NORMAL_INDEX_OUT_OF_RANGE",
                "OBJ normal index out of range: " + std::to_string(normalIndex),
                sourcePath);
            return {0.0f, 0.0f, 1.0f};
        }

        const size_t offset = static_cast<size_t>(normalIndex) * 3u;
        return safeNormalize(
            {attrib.normals[offset + 0u], attrib.normals[offset + 1u], attrib.normals[offset + 2u]},
            {0.0f, 0.0f, 1.0f});
    }

    glm::vec2 readTexCoord(const tinyobj::attrib_t& attrib,
                           int texCoordIndex,
                           bool flipTexCoordV,
                           const std::filesystem::path& sourcePath,
                           AssetImportResult& result)
    {
        if (texCoordIndex < 0)
            return {0.0f, 0.0f};

        const size_t count = attrib.texcoords.size() / 2u;
        if (!validVecIndex(texCoordIndex, count))
        {
            addDiagnostic(
                result,
                AssetDiagnosticSeverity::Error,
                "OBJ_TEXCOORD_INDEX_OUT_OF_RANGE",
                "OBJ texture coordinate index out of range: " + std::to_string(texCoordIndex),
                sourcePath);
            return {0.0f, 0.0f};
        }

        const size_t offset = static_cast<size_t>(texCoordIndex) * 2u;
        const float u = attrib.texcoords[offset + 0u];
        const float v = attrib.texcoords[offset + 1u];
        return {
            u,
            flipTexCoordV ? 1.0f - v : v
        };
    }

    glm::vec4 readColor(const tinyobj::attrib_t& attrib, int vertexIndex)
    {
        const size_t count = attrib.colors.size() / 3u;
        if (!validVecIndex(vertexIndex, count))
            return {1.0f, 1.0f, 1.0f, 1.0f};

        const size_t offset = static_cast<size_t>(vertexIndex) * 3u;
        return {
            attrib.colors[offset + 0u],
            attrib.colors[offset + 1u],
            attrib.colors[offset + 2u],
            1.0f
        };
    }

    std::string materialNameForIndex(const std::vector<tinyobj::material_t>& materials,
                                     int materialIndex)
    {
        if (!validVecIndex(materialIndex, materials.size()))
            return {};
        return materials[static_cast<size_t>(materialIndex)].name;
    }

    ImportedMaterial importMaterial(const tinyobj::material_t& material,
                                    const std::filesystem::path& baseDirectory)
    {
        ImportedMaterial imported;
        imported.name = material.name;
        imported.baseColor = glm::vec4{
            material.diffuse[0],
            material.diffuse[1],
            material.diffuse[2],
            material.dissolve
        };
        imported.metallic = material.metallic;
        imported.roughness = material.roughness;
        imported.emissiveFactor = glm::vec3{
            material.emission[0],
            material.emission[1],
            material.emission[2]
        };
        imported.emissiveStrength = 1.0f;
        imported.baseColorTexture = resolveRelativeTo(baseDirectory, material.diffuse_texname);
        imported.metallicTexture = resolveRelativeTo(baseDirectory, material.metallic_texname);
        imported.roughnessTexture = resolveRelativeTo(baseDirectory, material.roughness_texname);
        imported.normalTexture = resolveRelativeTo(
            baseDirectory,
            !material.normal_texname.empty() ? material.normal_texname : material.bump_texname);
        imported.ambientOcclusionTexture = resolveRelativeTo(baseDirectory, material.ambient_texname);
        imported.emissiveTexture = resolveRelativeTo(baseDirectory, material.emissive_texname);
        if (material.dissolve < 1.0f)
        {
            imported.renderState.alphaMode = MaterialAlphaMode::Blend;
            imported.renderState.depthWrite = false;
        }
        return imported;
    }

    void addTextureDependency(AssetImportResult& result,
                              const std::filesystem::path& texturePath,
                              const std::filesystem::path& sourcePath,
                              TextureColorSpace colorSpace,
                              MaterialTextureRole role)
    {
        if (texturePath.empty())
            return;

        addDependency(result, texturePath, AssetDependencyType::Texture);
        ImportedTexture texture;
        texture.debugName = texturePath.filename().string();
        texture.sourcePath = texturePath;
        texture.logicalPath = texturePath.lexically_normal().generic_string();
        texture.colorSpace = colorSpace;
        texture.intendedRole = role;
        result.textures.push_back(std::move(texture));

        if (!std::filesystem::exists(texturePath))
        {
            addDiagnostic(
                result,
                AssetDiagnosticSeverity::Warning,
                "ASSET_DEPENDENCY_MISSING",
                "Referenced texture dependency is missing: " + texturePath.string(),
                sourcePath);
        }
    }

    void addMaterialTextureDependencies(AssetImportResult& result,
                                        const ImportedMaterial& material,
                                        const std::filesystem::path& sourcePath)
    {
        addTextureDependency(
            result,
            material.baseColorTexture,
            sourcePath,
            TextureColorSpace::SRgb,
            MaterialTextureRole::BaseColor);
        addTextureDependency(
            result,
            material.metallicTexture,
            sourcePath,
            TextureColorSpace::Linear,
            MaterialTextureRole::MetallicRoughness);
        addTextureDependency(
            result,
            material.roughnessTexture,
            sourcePath,
            TextureColorSpace::Linear,
            MaterialTextureRole::MetallicRoughness);
        addTextureDependency(
            result,
            material.normalTexture,
            sourcePath,
            TextureColorSpace::Linear,
            MaterialTextureRole::Normal);
        addTextureDependency(
            result,
            material.ambientOcclusionTexture,
            sourcePath,
            TextureColorSpace::Linear,
            MaterialTextureRole::AmbientOcclusion);
        addTextureDependency(
            result,
            material.emissiveTexture,
            sourcePath,
            TextureColorSpace::SRgb,
            MaterialTextureRole::Emissive);
    }

    void closePrimitive(ImportedMesh& mesh,
                        uint32_t firstIndex,
                        int materialIndex,
                        const std::string& materialName,
                        const std::string& shapeName)
    {
        const uint32_t indexCount = static_cast<uint32_t>(mesh.indices.size()) - firstIndex;
        if (indexCount == 0)
            return;

        ImportedMeshPrimitive primitive;
        primitive.name = shapeName;
        primitive.firstIndex = firstIndex;
        primitive.indexCount = indexCount;
        primitive.materialIndex = materialIndex;
        primitive.materialName = materialName;
        mesh.primitives.push_back(std::move(primitive));
    }
}

std::string_view ObjAssetImporter::name() const
{
    return "obj";
}

uint32_t ObjAssetImporter::version() const
{
    return 1;
}

AssetImportCapability ObjAssetImporter::capabilities() const
{
    return AssetImportCapability::Meshes
        | AssetImportCapability::Materials
        | AssetImportCapability::Textures;
}

bool ObjAssetImporter::supports(const std::filesystem::path& sourcePath) const
{
    return sourcePath.extension() == ".obj" || sourcePath.extension() == ".OBJ";
}

AssetImportResult ObjAssetImporter::importAsset(const AssetImportRequest& request) const
{
    AssetImportResult result;
    const std::filesystem::path sourcePath = request.sourcePath;
    addDependency(result, sourcePath, AssetDependencyType::SourceFile);

    if (sourcePath.empty() || !std::filesystem::exists(sourcePath))
    {
        addDiagnostic(
            result,
            AssetDiagnosticSeverity::Error,
            "ASSET_SOURCE_MISSING",
            "OBJ source file does not exist: " + sourcePath.string(),
            sourcePath);
        return result;
    }

    const std::filesystem::path baseDirectory = sourcePath.parent_path();
    for (const std::filesystem::path& materialLibrary : readObjMaterialLibraries(sourcePath))
    {
        const std::filesystem::path resolved = resolveRelativeTo(baseDirectory, materialLibrary);
        addDependency(result, resolved, AssetDependencyType::MaterialLibrary);
        if (!std::filesystem::exists(resolved))
        {
            addDiagnostic(
                result,
                AssetDiagnosticSeverity::Warning,
                "ASSET_DEPENDENCY_MISSING",
                "Referenced material library is missing: " + resolved.string(),
                sourcePath);
        }
    }

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn;
    std::string err;

    const std::string sourceString = sourcePath.string();
    const std::string baseDirectoryString = baseDirectory.string();
    const bool loaded = tinyobj::LoadObj(
        &attrib,
        &shapes,
        &materials,
        &warn,
        &err,
        sourceString.c_str(),
        baseDirectoryString.empty() ? nullptr : baseDirectoryString.c_str(),
        true,
        true);

    if (!warn.empty())
    {
        addDiagnostic(
            result,
            AssetDiagnosticSeverity::Warning,
            "OBJ_IMPORT_WARNING",
            warn,
            sourcePath);
    }

    if (!loaded)
    {
        addDiagnostic(
            result,
            AssetDiagnosticSeverity::Error,
            "OBJ_IMPORT_FAILED",
            err.empty() ? "OBJ import failed" : err,
            sourcePath);
        return result;
    }

    if (!err.empty())
    {
        addDiagnostic(
            result,
            AssetDiagnosticSeverity::Warning,
            "OBJ_IMPORT_MESSAGE",
            err,
            sourcePath);
    }

    if (request.options.includeSourceMaterials &&
        hasImportCapability(request.requestedCapabilities, AssetImportCapability::Materials))
    {
        result.materials.reserve(materials.size());
        for (const tinyobj::material_t& material : materials)
        {
            ImportedMaterial imported = importMaterial(material, baseDirectory);
            addMaterialTextureDependencies(result, imported, sourcePath);
            result.materials.push_back(std::move(imported));
        }
    }

    if (!hasImportCapability(request.requestedCapabilities, AssetImportCapability::Meshes))
    {
        return result;
    }

    ImportedMesh mesh;
    mesh.debugName = sourcePath.stem().string();
    mesh.sourcePath = sourcePath;

    std::unordered_map<ObjVertexKey, uint32_t, ObjVertexKeyHash> uniqueVertices;
    bool allNormalsPresent = true;
    bool allTexCoordsPresent = true;

    for (const tinyobj::shape_t& shape : shapes)
    {
        size_t indexOffset = 0;
        uint32_t primitiveFirstIndex = static_cast<uint32_t>(mesh.indices.size());
        int primitiveMaterial = -2;
        std::string primitiveMaterialName;
        const std::string shapeName = shape.name.empty() ? mesh.debugName : shape.name;

        for (size_t faceIndex = 0; faceIndex < shape.mesh.num_face_vertices.size(); ++faceIndex)
        {
            const int faceVertexCount = shape.mesh.num_face_vertices[faceIndex];
            const int faceMaterial =
                faceIndex < shape.mesh.material_ids.size()
                    ? shape.mesh.material_ids[faceIndex]
                    : -1;
            const std::string faceMaterialName = materialNameForIndex(materials, faceMaterial);

            if (primitiveMaterial == -2)
            {
                primitiveFirstIndex = static_cast<uint32_t>(mesh.indices.size());
                primitiveMaterial = faceMaterial;
                primitiveMaterialName = faceMaterialName;
            }
            else if (faceMaterial != primitiveMaterial)
            {
                closePrimitive(
                    mesh,
                    primitiveFirstIndex,
                    primitiveMaterial,
                    primitiveMaterialName,
                    shapeName);
                primitiveFirstIndex = static_cast<uint32_t>(mesh.indices.size());
                primitiveMaterial = faceMaterial;
                primitiveMaterialName = faceMaterialName;
            }

            if (faceVertexCount != 3)
            {
                addDiagnostic(
                    result,
                    AssetDiagnosticSeverity::Warning,
                    "OBJ_NON_TRIANGULAR_FACE",
                    "OBJ face was not triangulated to three vertices",
                    sourcePath);
            }

            for (int vertexInFace = 0; vertexInFace < faceVertexCount; ++vertexInFace)
            {
                if (indexOffset >= shape.mesh.indices.size())
                {
                    addDiagnostic(
                        result,
                        AssetDiagnosticSeverity::Error,
                        "OBJ_INDEX_STREAM_TRUNCATED",
                        "OBJ shape index stream ended before all face vertices were read",
                        sourcePath);
                    return result;
                }

                const tinyobj::index_t& index = shape.mesh.indices[indexOffset++];
                if (index.vertex_index < 0)
                {
                    addDiagnostic(
                        result,
                        AssetDiagnosticSeverity::Error,
                        "OBJ_MISSING_POSITION",
                        "OBJ face is missing a position index",
                        sourcePath);
                    return result;
                }

                if (index.normal_index < 0)
                    allNormalsPresent = false;
                if (index.texcoord_index < 0)
                    allTexCoordsPresent = false;

                const glm::vec4 color = readColor(attrib, index.vertex_index);
                ObjVertexKey key{
                    index.vertex_index,
                    index.normal_index,
                    index.texcoord_index,
                    color
                };

                auto found = uniqueVertices.find(key);
                if (found == uniqueVertices.end())
                {
                    Vertex vertex;
                    vertex.pos = readPosition(attrib, index.vertex_index, sourcePath, result);
                    vertex.normal = readNormal(attrib, index.normal_index, sourcePath, result);
                    vertex.tangent = {1.0f, 0.0f, 0.0f, 1.0f};
                    vertex.color = color;
                    vertex.texCoord = readTexCoord(
                        attrib,
                        index.texcoord_index,
                        request.options.flipTexCoordV,
                        sourcePath,
                        result);

                    if (result.hasErrors())
                        return result;

                    const uint32_t vertexId = static_cast<uint32_t>(mesh.vertices.size());
                    uniqueVertices.emplace(key, vertexId);
                    mesh.vertices.push_back(vertex);
                    mesh.indices.push_back(vertexId);
                }
                else
                {
                    mesh.indices.push_back(found->second);
                }
            }
        }

        closePrimitive(
            mesh,
            primitiveFirstIndex,
            primitiveMaterial,
            primitiveMaterialName,
            shapeName);
    }

    mesh.hadMissingNormals = !allNormalsPresent;
    mesh.hadMissingTexCoords = !allTexCoordsPresent;
    mesh.sourceAttributes = VertexAttributeFlags::Position | VertexAttributeFlags::Color;
    if (allNormalsPresent && !mesh.indices.empty())
        mesh.sourceAttributes |= VertexAttributeFlags::Normal;
    if (allTexCoordsPresent && !mesh.indices.empty())
        mesh.sourceAttributes |= VertexAttributeFlags::UV0;

    if (mesh.hadMissingNormals && !mesh.indices.empty())
    {
        addDiagnostic(
            result,
            AssetDiagnosticSeverity::Warning,
            "OBJ_MISSING_NORMALS",
            "One or more OBJ vertices are missing normals; normals will be generated by mesh preparation",
            sourcePath);
    }
    if (mesh.hadMissingTexCoords && !mesh.indices.empty())
    {
        addDiagnostic(
            result,
            AssetDiagnosticSeverity::Warning,
            "OBJ_MISSING_UV0",
            "One or more OBJ vertices are missing UV0 coordinates; missing UVs default to zero",
            sourcePath);
    }

    if (mesh.indices.empty())
    {
        addDiagnostic(
            result,
            AssetDiagnosticSeverity::Warning,
            "OBJ_EMPTY_MESH",
            "OBJ import produced no mesh indices",
            sourcePath);
    }

    result.meshes.push_back(std::move(mesh));
    return result;
}
}
