#include "GtsObjModelImporter.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "ObjSourceReader.h"
#include "assets/model/GtsModelImportResult.h"

namespace
{
    void warn(std::vector<GtsModelDiagnostic>& diagnostics,
              const std::string&               code,
              const std::string&               message,
              const std::string&               location)
    {
        diagnostics.push_back({GtsModelDiagnosticSeverity::Warning, code, message, location});
    }

    bool fail(std::vector<GtsModelDiagnostic>& diagnostics,
              const std::string&               code,
              const std::string&               message,
              const std::string&               location)
    {
        diagnostics.push_back({GtsModelDiagnosticSeverity::Error, code, message, location});
        return false;
    }

    std::optional<GtsModelImageBinding> bindImage(const std::string&                         name,
                                                  const std::filesystem::path&               directory,
                                                  GtsModelAsset&                             asset,
                                                  std::map<std::filesystem::path, uint32_t>& imageIndices,
                                                  std::vector<GtsModelDiagnostic>&           diagnostics)
    {
        if (name.empty())
            return std::nullopt;
        const std::filesystem::path path = (directory / name).lexically_normal();
        if (const auto existing = imageIndices.find(path); existing != imageIndices.end())
        {
            return GtsModelImageBinding{existing->second, 0};
        }
        if (asset.images.size() >= GtsModelImageBinding::InvalidImageIndex)
        {
            fail(diagnostics, "OBJ_TOO_MANY_IMAGES", "Model exceeds image index capacity.", path.string());
            return std::nullopt;
        }
        const auto index = static_cast<uint32_t>(asset.images.size());
        imageIndices.emplace(path, index);
        asset.images.push_back({path.filename().string(), path});
        std::error_code error;
        if (!std::filesystem::is_regular_file(path, error))
        {
            warn(diagnostics,
                 "OBJ_IMAGE_MISSING",
                 "Referenced image is not a readable file; its reference is retained.",
                 path.string());
        }
        return GtsModelImageBinding{index, 0};
    }

    std::optional<GtsModelScalarImageBinding> bindScalarImage(const std::string&                         name,
                                                              const tinyobj::texture_option_t&           options,
                                                              const std::string&                         property,
                                                              const gts::obj::MaterialSource&            source,
                                                              GtsModelAsset&                             asset,
                                                              std::map<std::filesystem::path, uint32_t>& imageIndices,
                                                              std::vector<GtsModelDiagnostic>&           diagnostics)
    {
        if (name.empty())
            return std::nullopt;
        GtsModelTextureChannel channel         = GtsModelTextureChannel::Red;
        bool                   explicitChannel = false;
        if (const auto entry = source.properties.find(property); entry != source.properties.end())
        {
            std::istringstream words(entry->second);
            std::string        word;
            while (words >> word)
                explicitChannel = explicitChannel || word == "-imfchan";
        }
        if (explicitChannel)
        {
            switch (options.imfchan)
            {
            case 'r':
                channel = GtsModelTextureChannel::Red;
                break;
            case 'g':
                channel = GtsModelTextureChannel::Green;
                break;
            case 'b':
                channel = GtsModelTextureChannel::Blue;
                break;
            case 'm':
                channel = GtsModelTextureChannel::Alpha;
                break;
            default:
                warn(diagnostics,
                     "OBJ_IMAGE_CHANNEL_UNSUPPORTED",
                     property + " requests a non-RGBA channel; binding omitted.",
                     name);
                return std::nullopt;
            }
        }
        const auto binding = bindImage(name, source.directory, asset, imageIndices, diagnostics);
        if (!binding)
            return std::nullopt;
        return GtsModelScalarImageBinding{*binding, channel};
    }

    void importMaterials(const gts::obj::SourceData&      source,
                         GtsModelAsset&                   asset,
                         std::vector<GtsModelDiagnostic>& diagnostics)
    {
        std::map<std::filesystem::path, uint32_t> imageIndices;
        for (size_t i = 0; i < source.materials.size(); ++i)
        {
            const auto&      input      = source.materials[i];
            const auto&      metadata   = source.materialSources[i];
            const auto&      properties = metadata.properties;
            GtsModelMaterial material;
            material.name = input.name;
            // TinyOBJ supplies zero for some unauthored factors - we resolve absence
            // here instead of overriding canonical defaults with parser defaults
            if (properties.contains("Kd"))
            {
                material.baseColor = {input.diffuse[0], input.diffuse[1], input.diffuse[2], 1};
            }
            material.baseColor.a = input.dissolve;
            if (properties.contains("Pm"))
                material.metallic = input.metallic;
            if (properties.contains("Pr"))
                material.roughness = input.roughness;
            if (properties.contains("Ke"))
            {
                material.emissiveFactor = {input.emission[0], input.emission[1], input.emission[2]};
            }
            if (input.dissolve < 1)
                material.alphaMode = GtsModelAlphaMode::Blend;
            material.baseColorImage =
                bindImage(input.diffuse_texname, metadata.directory, asset, imageIndices, diagnostics);
            material.metallicImage = bindScalarImage(
                input.metallic_texname, input.metallic_texopt, "map_Pm", metadata, asset, imageIndices, diagnostics);
            material.roughnessImage = bindScalarImage(
                input.roughness_texname, input.roughness_texopt, "map_Pr", metadata, asset, imageIndices, diagnostics);
            material.normalImage =
                bindImage(input.normal_texname, metadata.directory, asset, imageIndices, diagnostics);
            if (material.normalImage)
                material.normalScale = input.normal_texopt.bump_multiplier;
            material.ambientOcclusionImage = bindScalarImage(
                input.ambient_texname, input.ambient_texopt, "map_Ka", metadata, asset, imageIndices, diagnostics);
            material.emissiveImage =
                bindImage(input.emissive_texname, metadata.directory, asset, imageIndices, diagnostics);
            if (!input.bump_texname.empty())
            {
                warn(diagnostics,
                     "OBJ_BUMP_MAP_UNSUPPORTED",
                     "Height/bump map omitted: canonical normal images require tangent-space normals.",
                     material.name);
            }
            if (!input.alpha_texname.empty())
            {
                warn(diagnostics,
                     "OBJ_OPACITY_MAP_UNSUPPORTED",
                     "Separate opacity map omitted: canonical opacity uses base-color alpha.",
                     material.name);
            }
            asset.materials.push_back(std::move(material));
        }
    }

    bool validIndex(int index, size_t count)
    {
        return index >= 0 && static_cast<size_t>(index) < count;
    }

    bool buildPrimitive(std::span<const tinyobj::index_t> corners,
                        int                               materialIndex,
                        const gts::obj::SourceData&       source,
                        GtsModelPrimitive&                primitive,
                        const std::string&                location,
                        std::vector<GtsModelDiagnostic>&  diagnostics)
    {
        const auto& attributes = source.attributes;
        if (materialIndex < -1 || (materialIndex >= 0 && !validIndex(materialIndex, source.materials.size())))
        {
            return fail(diagnostics, "OBJ_MATERIAL_INDEX_INVALID", "Invalid source material reference.", location);
        }
        if (materialIndex >= 0)
            primitive.materialIndex = static_cast<uint32_t>(materialIndex);

        size_t normalsPresent = 0;
        size_t uvsPresent     = 0;
        size_t colorsPresent  = 0;
        for (const auto& corner : corners)
        {
            if (!validIndex(corner.vertex_index, attributes.vertices.size() / 3))
            {
                return fail(
                    diagnostics, "OBJ_POSITION_INDEX_OUT_OF_RANGE", "Invalid source position reference.", location);
            }
            if (corner.normal_index != -1 && !validIndex(corner.normal_index, attributes.normals.size() / 3))
            {
                return fail(diagnostics, "OBJ_NORMAL_INDEX_OUT_OF_RANGE", "Invalid source normal reference.", location);
            }
            if (corner.texcoord_index != -1 && !validIndex(corner.texcoord_index, attributes.texcoords.size() / 2))
            {
                return fail(diagnostics, "OBJ_TEXCOORD_INDEX_OUT_OF_RANGE", "Invalid source UV reference.", location);
            }
            normalsPresent += corner.normal_index != -1;
            uvsPresent += corner.texcoord_index != -1;
            colorsPresent += source.authoredColors[corner.vertex_index];
        }
        const auto complete = [&](size_t present, const char* semantic, const char* code)
        {
            if (present > 0 && present != corners.size())
            {
                warn(diagnostics,
                     code,
                     std::string("Partially populated ") + semantic + " omitted from this primitive.",
                     location);
            }
            return present == corners.size();
        };
        const bool             normals = complete(normalsPresent, "Normal[0]", "OBJ_PARTIAL_NORMALS");
        const bool             uvs     = complete(uvsPresent, "TexCoord[0]", "OBJ_PARTIAL_UVS");
        const bool             colors  = complete(colorsPresent, "Color[0]", "OBJ_PARTIAL_COLORS");
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normalValues;
        std::vector<glm::vec2> uvValues;
        std::vector<glm::vec4> colorValues;
        // OBJ colors belong to position records, so position identity also
        // preserves color identity - keep the full tuple even if a stream is omitted
        std::map<std::array<int, 3>, uint32_t> vertices;
        for (const auto& corner : corners)
        {
            const std::array<int, 3> key{corner.vertex_index, corner.normal_index, corner.texcoord_index};
            if (const auto found = vertices.find(key); found != vertices.end())
            {
                primitive.indices.push_back(found->second);
                continue;
            }
            if (positions.size() >= std::numeric_limits<uint32_t>::max())
            {
                return fail(
                    diagnostics, "OBJ_TOO_MANY_VERTICES", "Primitive exceeds canonical index capacity.", location);
            }
            const auto index = static_cast<uint32_t>(positions.size());
            vertices.emplace(key, index);
            primitive.indices.push_back(index);
            const size_t p = static_cast<size_t>(corner.vertex_index) * 3;
            positions.push_back({attributes.vertices[p], attributes.vertices[p + 1], attributes.vertices[p + 2]});
            if (normals)
            {
                const size_t n = static_cast<size_t>(corner.normal_index) * 3;
                normalValues.push_back({attributes.normals[n], attributes.normals[n + 1], attributes.normals[n + 2]});
            }
            if (uvs)
            {
                const size_t uv = static_cast<size_t>(corner.texcoord_index) * 2;
                // preserve the existing OBJ import convention at the source boundary
                uvValues.push_back({attributes.texcoords[uv], 1.0f - attributes.texcoords[uv + 1]});
            }
            if (colors)
            {
                if (p + 2 >= attributes.colors.size())
                {
                    return fail(diagnostics,
                                "OBJ_COLOR_DATA_INVALID",
                                "Authored color data is missing from parser output.",
                                location);
                }
                colorValues.push_back({attributes.colors[p], attributes.colors[p + 1], attributes.colors[p + 2], 1});
            }
        }
        primitive.attributes.push_back({GtsVertexSemantic::Position, 0, std::move(positions)});
        if (normals)
            primitive.attributes.push_back({GtsVertexSemantic::Normal, 0, std::move(normalValues)});
        if (uvs)
            primitive.attributes.push_back({GtsVertexSemantic::TexCoord, 0, std::move(uvValues)});
        if (colors)
            primitive.attributes.push_back({GtsVertexSemantic::Color, 0, std::move(colorValues)});
        return true;
    }
} // namespace

GtsModelImportResult GtsObjModelImporter::importAsset(const GtsModelImportRequest& request) const
{
    std::vector<GtsModelDiagnostic> diagnostics;
    std::error_code                 pathError;
    const auto                      path = std::filesystem::absolute(request.sourcePath, pathError).lexically_normal();
    if (request.sourcePath.empty() || pathError)
    {
        fail(diagnostics, "OBJ_SOURCE_INVALID", "A valid source path is required.", request.sourcePath.string());
        return GtsModelImportResult::failure(std::move(diagnostics));
    }
    gts::obj::SourceData source;
    if (!gts::obj::readSource(path, source, diagnostics))
    {
        return GtsModelImportResult::failure(std::move(diagnostics));
    }
    GtsModelAsset asset;
    importMaterials(source, asset, diagnostics);
    for (size_t shapeIndex = 0; shapeIndex < source.shapes.size(); ++shapeIndex)
    {
        const auto& shape = source.shapes[shapeIndex];
        if (shape.mesh.num_face_vertices.empty())
            continue;
        GtsModelMesh mesh;
        mesh.name     = shape.name.empty() ? path.stem().string() : shape.name;
        size_t face   = 0;
        size_t offset = 0;
        while (face < shape.mesh.num_face_vertices.size())
        {
            const std::string location =
                path.string() + ":shape[" + std::to_string(shapeIndex) + "].face[" + std::to_string(face) + "]";
            if (shape.mesh.material_ids.size() != shape.mesh.num_face_vertices.size())
            {
                fail(diagnostics, "OBJ_MATERIAL_STREAM_INVALID", "Source face/material counts disagree.", location);
                return GtsModelImportResult::failure(std::move(diagnostics));
            }
            const int material = shape.mesh.material_ids[face];
            size_t    count    = 0;
            do
            {
                if (shape.mesh.num_face_vertices[face] != 3)
                {
                    fail(diagnostics, "OBJ_TRIANGULATION_FAILED", "Expected triangulated source face.", location);
                    return GtsModelImportResult::failure(std::move(diagnostics));
                }
                count += 3;
                ++face;
            } while (face < shape.mesh.num_face_vertices.size() && shape.mesh.material_ids[face] == material);
            if (count > shape.mesh.indices.size() - offset)
            {
                fail(diagnostics, "OBJ_INDEX_STREAM_INVALID", "Source corner stream is truncated.", location);
                return GtsModelImportResult::failure(std::move(diagnostics));
            }
            GtsModelPrimitive primitive;
            if (!buildPrimitive(std::span(shape.mesh.indices).subspan(offset, count),
                                material,
                                source,
                                primitive,
                                location,
                                diagnostics))
            {
                return GtsModelImportResult::failure(std::move(diagnostics));
            }
            mesh.primitives.push_back(std::move(primitive));
            offset += count;
        }
        if (asset.meshes.size() >= std::numeric_limits<uint32_t>::max())
        {
            fail(diagnostics, "OBJ_TOO_MANY_SHAPES", "Model exceeds canonical mesh index capacity.", path.string());
            return GtsModelImportResult::failure(std::move(diagnostics));
        }
        GtsModelNode node;
        node.name      = mesh.name;
        node.meshIndex = static_cast<uint32_t>(asset.meshes.size());
        asset.rootNodes.push_back(static_cast<uint32_t>(asset.nodes.size()));
        asset.nodes.push_back(std::move(node));
        asset.meshes.push_back(std::move(mesh));
    }
    if (asset.meshes.empty())
    {
        fail(diagnostics, "OBJ_EMPTY_MODEL", "OBJ contains no supported face geometry.", path.string());
        return GtsModelImportResult::failure(std::move(diagnostics));
    }
    return GtsModelImportResult::success(std::move(asset), std::move(diagnostics));
}
