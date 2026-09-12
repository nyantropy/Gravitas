#include "GltfAssetImporter.h"
#include "GtsJsonParser.h"
#include "assets/importer/gltf/GltfSourceUtilities.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>

#include <gtc/quaternion.hpp>
#include <gtx/quaternion.hpp>

#include "MeshGeometryProcessor.h"

namespace gts::rendering
{
namespace
{
    using namespace gts::gltf;

    glm::vec3 vec3Value(const GtsJsonValue* value, glm::vec3 fallback = {})
    {
        if (value == nullptr || !value->isArray() || value->asArray().size() < 3u)
            return fallback;
        return {
            value->asArray()[0].tryFloat().value_or(fallback.x),
            value->asArray()[1].tryFloat().value_or(fallback.y),
            value->asArray()[2].tryFloat().value_or(fallback.z)
        };
    }

    glm::vec4 vec4Value(const GtsJsonValue* value, glm::vec4 fallback = {})
    {
        if (value == nullptr || !value->isArray() || value->asArray().size() < 4u)
            return fallback;
        return {
            value->asArray()[0].tryFloat().value_or(fallback.x),
            value->asArray()[1].tryFloat().value_or(fallback.y),
            value->asArray()[2].tryFloat().value_or(fallback.z),
            value->asArray()[3].tryFloat().value_or(fallback.w)
        };
    }

    void addDiagnostic(AssetImportResult& result,
                       AssetDiagnosticSeverity severity,
                       std::string code,
                       std::string message,
                       const std::filesystem::path& sourcePath)
    {
        result.diagnostics.push_back({
            severity,
            std::move(code),
            std::move(message),
            sourcePath,
            0
        });
    }

    void addDependency(AssetImportResult& result,
                       std::filesystem::path path,
                       AssetDependencyType type,
                       bool required = true)
    {
        path = path.lexically_normal();
        for (const AssetDependency& dependency : result.dependencies)
        {
            if (dependency.type == type && dependency.path == path)
                return;
        }
        result.dependencies.push_back({std::move(path), type, required});
    }

    std::string readFileText(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        std::ostringstream stream;
        stream << file.rdbuf();
        return stream.str();
    }

    std::string extensionForMimeType(const std::string& mimeType)
    {
        if (mimeType == "image/jpeg")
            return ".jpg";
        if (mimeType == "image/png")
            return ".png";
        if (mimeType == "image/webp")
            return ".webp";
        return ".bin";
    }

    std::filesystem::path resolveRelativeTo(const std::filesystem::path& baseDirectory,
                                            const std::string& uri)
    {
        if (uri.empty())
            return {};
        std::filesystem::path path(uri);
        if (path.is_absolute())
            return path.lexically_normal();
        return (baseDirectory / path).lexically_normal();
    }

    struct BufferView
    {
        int32_t buffer = -1;
        size_t byteOffset = 0;
        size_t byteLength = 0;
        size_t byteStride = 0;
    };

    struct Accessor
    {
        int32_t bufferView = -1;
        size_t byteOffset = 0;
        size_t count = 0;
        int32_t componentType = 0;
        std::string type;
        bool normalized = false;
    };

    struct GltfData
    {
        GtsJsonValue root;
        std::filesystem::path sourcePath;
        std::filesystem::path baseDirectory;
        std::vector<std::vector<uint8_t>> buffers;
        std::vector<BufferView> bufferViews;
        std::vector<Accessor> accessors;
    };

    bool readAccessorComponent(const GltfData& data,
                               const Accessor& accessor,
                               size_t elementIndex,
                               size_t componentIndex,
                               float& value)
    {
        if (accessor.bufferView < 0 || static_cast<size_t>(accessor.bufferView) >= data.bufferViews.size())
            return false;
        const BufferView& view = data.bufferViews[static_cast<size_t>(accessor.bufferView)];
        if (view.buffer < 0 || static_cast<size_t>(view.buffer) >= data.buffers.size())
            return false;

        const size_t componentBytes = componentSize(accessor.componentType);
        const size_t componentCount = componentCountForType(accessor.type);
        if (componentBytes == 0 || componentCount == 0 || componentIndex >= componentCount)
            return false;

        const size_t stride = view.byteStride != 0 ? view.byteStride : componentBytes * componentCount;
        const size_t offset =
            view.byteOffset + accessor.byteOffset + elementIndex * stride + componentIndex * componentBytes;
        const std::vector<uint8_t>& buffer = data.buffers[static_cast<size_t>(view.buffer)];
        if (offset + componentBytes > buffer.size())
            return false;

        const uint8_t* ptr = buffer.data() + offset;
        switch (accessor.componentType)
        {
            case 5120:
            {
                int8_t raw = 0;
                std::memcpy(&raw, ptr, sizeof(raw));
                value = accessor.normalized ? normalizedIntegerValue(raw, accessor.componentType)
                                            : static_cast<float>(raw);
                return true;
            }
            case 5121:
            {
                uint8_t raw = 0;
                std::memcpy(&raw, ptr, sizeof(raw));
                value = accessor.normalized ? normalizedIntegerValue(raw, accessor.componentType)
                                            : static_cast<float>(raw);
                return true;
            }
            case 5122:
            {
                int16_t raw = 0;
                std::memcpy(&raw, ptr, sizeof(raw));
                value = accessor.normalized ? normalizedIntegerValue(raw, accessor.componentType)
                                            : static_cast<float>(raw);
                return true;
            }
            case 5123:
            {
                uint16_t raw = 0;
                std::memcpy(&raw, ptr, sizeof(raw));
                value = accessor.normalized ? normalizedIntegerValue(raw, accessor.componentType)
                                            : static_cast<float>(raw);
                return true;
            }
            case 5125:
            {
                uint32_t raw = 0;
                std::memcpy(&raw, ptr, sizeof(raw));
                value = static_cast<float>(raw);
                return true;
            }
            case 5126:
                std::memcpy(&value, ptr, sizeof(value));
                return true;
        }
        return false;
    }

    bool readAccessorElement(const GltfData& data,
                             int32_t accessorIndex,
                             size_t elementIndex,
                             std::vector<float>& values)
    {
        if (accessorIndex < 0 || static_cast<size_t>(accessorIndex) >= data.accessors.size())
            return false;
        const Accessor& accessor = data.accessors[static_cast<size_t>(accessorIndex)];
        const size_t componentCount = componentCountForType(accessor.type);
        if (elementIndex >= accessor.count || componentCount == 0)
            return false;

        values.assign(componentCount, 0.0f);
        for (size_t component = 0; component < componentCount; ++component)
        {
            if (!readAccessorComponent(data, accessor, elementIndex, component, values[component]))
                return false;
        }
        return true;
    }

    bool readIndex(const GltfData& data,
                   int32_t accessorIndex,
                   size_t elementIndex,
                   uint32_t& value)
    {
        std::vector<float> components;
        if (!readAccessorElement(data, accessorIndex, elementIndex, components) || components.empty())
            return false;
        value = static_cast<uint32_t>(components[0]);
        return true;
    }

    int32_t attributeAccessor(const GtsJsonValue& attributes, const std::string& name)
    {
        return attributes.findInt32(name).value_or(-1);
    }

    bool appendAccessorArray(const GtsJsonValue& array,
                             std::vector<Accessor>& accessors)
    {
        if (!array.isArray())
            return true;
        accessors.reserve(array.asArray().size());
        for (const GtsJsonValue& value : array.asArray())
        {
            Accessor accessor;
            accessor.bufferView = value.findInt32("bufferView").value_or(-1);
            accessor.byteOffset = value.findUInt32("byteOffset").value_or(0);
            accessor.count = value.findUInt32("count").value_or(0);
            accessor.componentType = value.findInt32("componentType").value_or(0);
            accessor.type = value.findString("type").value_or("");
            accessor.normalized = value.findBool("normalized").value_or(false);
            accessors.push_back(std::move(accessor));
        }
        return true;
    }

    bool appendBufferViews(const GtsJsonValue& array,
                           std::vector<BufferView>& bufferViews)
    {
        if (!array.isArray())
            return true;
        bufferViews.reserve(array.asArray().size());
        for (const GtsJsonValue& value : array.asArray())
        {
            BufferView view;
            view.buffer = value.findInt32("buffer").value_or(-1);
            view.byteOffset = value.findUInt32("byteOffset").value_or(0);
            view.byteLength = value.findUInt32("byteLength").value_or(0);
            view.byteStride = value.findUInt32("byteStride").value_or(0);
            bufferViews.push_back(view);
        }
        return true;
    }

    bool loadBuffers(GltfData& data,
                     const GtsJsonValue& buffersValue,
                     const std::vector<uint8_t>& binChunk,
                     AssetImportResult& result)
    {
        if (!buffersValue.isArray())
            return true;
        data.buffers.resize(buffersValue.asArray().size());
        for (size_t i = 0; i < buffersValue.asArray().size(); ++i)
        {
            const GtsJsonValue& buffer = buffersValue.asArray()[i];
            const std::string uri = buffer.findString("uri").value_or("");
            if (uri.empty() && i == 0 && !binChunk.empty())
            {
                data.buffers[i] = binChunk;
                continue;
            }
            if (uri.starts_with("data:"))
            {
                std::string mimeType;
                if (!decodeDataUri(uri, mimeType, data.buffers[i]))
                {
                    addDiagnostic(result, AssetDiagnosticSeverity::Error, "GLTF_BUFFER_DATA_URI_INVALID",
                                  "glTF buffer data URI could not be decoded", data.sourcePath);
                    return false;
                }
                continue;
            }

            const std::filesystem::path bufferPath = resolveRelativeTo(data.baseDirectory, uri);
            if (!readFileBytes(bufferPath, data.buffers[i]))
            {
                addDiagnostic(result, AssetDiagnosticSeverity::Error, "GLTF_BUFFER_MISSING",
                              "glTF external buffer could not be read: " + bufferPath.string(),
                              data.sourcePath);
                return false;
            }
            addDependency(result, bufferPath, AssetDependencyType::SourceFile);
        }
        return true;
    }

    std::vector<uint8_t> bytesFromBufferView(const GltfData& data, int32_t bufferViewIndex)
    {
        if (bufferViewIndex < 0 || static_cast<size_t>(bufferViewIndex) >= data.bufferViews.size())
            return {};
        const BufferView& view = data.bufferViews[static_cast<size_t>(bufferViewIndex)];
        if (view.buffer < 0 || static_cast<size_t>(view.buffer) >= data.buffers.size())
            return {};
        const std::vector<uint8_t>& buffer = data.buffers[static_cast<size_t>(view.buffer)];
        if (view.byteOffset > buffer.size() || view.byteLength > buffer.size() - view.byteOffset)
            return {};
        return {
            buffer.begin() + static_cast<std::ptrdiff_t>(view.byteOffset),
            buffer.begin() + static_cast<std::ptrdiff_t>(view.byteOffset + view.byteLength)
        };
    }

    void addTextureDependency(AssetImportResult& result,
                              const ImportedTexture& texture,
                              const std::filesystem::path& sourcePath)
    {
        if (texture.source == ImportedTextureSource::ExternalFile && !texture.sourcePath.empty())
        {
            addDependency(result, texture.sourcePath, AssetDependencyType::Texture);
            if (!std::filesystem::exists(texture.sourcePath))
            {
                addDiagnostic(result, AssetDiagnosticSeverity::Warning, "ASSET_DEPENDENCY_MISSING",
                              "Referenced glTF texture dependency is missing: " + texture.sourcePath.string(),
                              sourcePath);
            }
        }
    }

    void importTextures(const GltfData& data, AssetImportResult& result)
    {
        const GtsJsonValue* textures = data.root.find("textures");
        const GtsJsonValue* images = data.root.find("images");
        if (textures == nullptr || !textures->isArray())
            return;

        result.textures.resize(textures->asArray().size());
        for (size_t textureIndex = 0; textureIndex < textures->asArray().size(); ++textureIndex)
        {
            const GtsJsonValue& textureValue = textures->asArray()[textureIndex];
            const int32_t imageIndex = textureValue.findInt32("source").value_or(-1);
            ImportedTexture imported;
            imported.debugName = "texture_" + std::to_string(textureIndex);
            imported.logicalPath = imported.debugName;

            const GtsJsonValue* image = images == nullptr ? nullptr : images->at(static_cast<size_t>(imageIndex));
            if (image != nullptr)
            {
                imported.debugName = image->findString("name").value_or(imported.debugName);
                const std::string uri = image->findString("uri").value_or("");
                imported.mimeType = image->findString("mimeType").value_or("");
                if (!uri.empty() && uri.starts_with("data:"))
                {
                    imported.source = ImportedTextureSource::EmbeddedBytes;
                    if (!decodeDataUri(uri, imported.mimeType, imported.embeddedBytes))
                    {
                        addDiagnostic(result, AssetDiagnosticSeverity::Warning, "GLTF_EMBEDDED_IMAGE_INVALID",
                                      "Embedded glTF image data URI could not be decoded", data.sourcePath);
                    }
                    imported.logicalPath =
                        data.sourcePath.stem().string() + "_image_" + std::to_string(imageIndex)
                        + extensionForMimeType(imported.mimeType);
                }
                else if (!uri.empty())
                {
                    imported.source = ImportedTextureSource::ExternalFile;
                    imported.sourcePath = resolveRelativeTo(data.baseDirectory, uri);
                    imported.logicalPath = std::filesystem::path(uri).generic_string();
                    if (imported.mimeType.empty())
                        imported.mimeType = imported.sourcePath.extension().string();
                }
                else
                {
                    const int32_t bufferView = image->findInt32("bufferView").value_or(-1);
                    imported.source = ImportedTextureSource::EmbeddedBytes;
                    imported.embeddedBytes = bytesFromBufferView(data, bufferView);
                    imported.logicalPath =
                        data.sourcePath.stem().string() + "_image_" + std::to_string(imageIndex)
                        + extensionForMimeType(imported.mimeType);
                    if (imported.embeddedBytes.empty())
                    {
                        addDiagnostic(result, AssetDiagnosticSeverity::Warning, "GLTF_EMBEDDED_IMAGE_INVALID",
                                      "Embedded glTF image bufferView could not be read", data.sourcePath);
                    }
                }
            }

            addTextureDependency(result, imported, data.sourcePath);
            result.textures[textureIndex] = std::move(imported);
        }
    }

    void setTextureRole(AssetImportResult& result,
                        int32_t textureIndex,
                        MaterialTextureRole role,
                        TextureColorSpace colorSpace)
    {
        if (textureIndex < 0 || static_cast<size_t>(textureIndex) >= result.textures.size())
            return;
        ImportedTexture& texture = result.textures[static_cast<size_t>(textureIndex)];
        texture.intendedRole = role;
        texture.colorSpace = colorSpace;
    }

    void importMaterials(const GltfData& data, AssetImportResult& result)
    {
        const GtsJsonValue* materials = data.root.find("materials");
        if (materials == nullptr || !materials->isArray())
            return;

        result.materials.reserve(materials->asArray().size());
        for (size_t materialIndex = 0; materialIndex < materials->asArray().size(); ++materialIndex)
        {
            const GtsJsonValue& materialValue = materials->asArray()[materialIndex];
            ImportedMaterial material;
            material.name = materialValue.findString("name").value_or("material_" + std::to_string(materialIndex));

            const GtsJsonValue* pbr = materialValue.find("pbrMetallicRoughness");
            if (pbr != nullptr)
            {
                material.baseColor = vec4Value(pbr->find("baseColorFactor"), {1.0f, 1.0f, 1.0f, 1.0f});
                material.metallic = pbr->findFloat("metallicFactor").value_or(1.0f);
                material.roughness = pbr->findFloat("roughnessFactor").value_or(1.0f);
                if (const GtsJsonValue* texture = pbr->find("baseColorTexture"))
                {
                    material.baseColorTextureIndex = texture->findInt32("index").value_or(-1);
                    setTextureRole(result, material.baseColorTextureIndex,
                                   MaterialTextureRole::BaseColor, TextureColorSpace::SRgb);
                }
                if (const GtsJsonValue* texture = pbr->find("metallicRoughnessTexture"))
                {
                    material.metallicRoughnessTextureIndex = texture->findInt32("index").value_or(-1);
                    setTextureRole(result, material.metallicRoughnessTextureIndex,
                                   MaterialTextureRole::MetallicRoughness, TextureColorSpace::Linear);
                }
            }

            if (const GtsJsonValue* texture = materialValue.find("normalTexture"))
            {
                material.normalTextureIndex = texture->findInt32("index").value_or(-1);
                material.normalScale = texture->findFloat("scale").value_or(1.0f);
                setTextureRole(result, material.normalTextureIndex,
                               MaterialTextureRole::Normal, TextureColorSpace::Linear);
            }
            if (const GtsJsonValue* texture = materialValue.find("occlusionTexture"))
            {
                material.ambientOcclusionTextureIndex = texture->findInt32("index").value_or(-1);
                material.ambientOcclusionStrength = texture->findFloat("strength").value_or(1.0f);
                setTextureRole(result, material.ambientOcclusionTextureIndex,
                               MaterialTextureRole::AmbientOcclusion, TextureColorSpace::Linear);
            }
            if (const GtsJsonValue* texture = materialValue.find("emissiveTexture"))
            {
                material.emissiveTextureIndex = texture->findInt32("index").value_or(-1);
                setTextureRole(result, material.emissiveTextureIndex,
                               MaterialTextureRole::Emissive, TextureColorSpace::SRgb);
            }

            material.emissiveFactor =
                vec3Value(materialValue.find("emissiveFactor"), {0.0f, 0.0f, 0.0f});
            if (const GtsJsonValue* extensions = materialValue.find("extensions"))
            {
                if (const GtsJsonValue* emissiveStrength = extensions->find("KHR_materials_emissive_strength"))
                    material.emissiveStrength = emissiveStrength->findFloat("emissiveStrength").value_or(1.0f);
            }

            const std::string alphaMode = materialValue.findString("alphaMode").value_or("OPAQUE");
            material.renderState.alphaCutoff = materialValue.findFloat("alphaCutoff").value_or(0.5f);
            if (alphaMode == "BLEND")
            {
                material.renderState.alphaMode = MaterialAlphaMode::Blend;
                material.renderState.depthWrite = false;
            }
            else if (alphaMode == "MASK")
            {
                material.renderState.alphaMode = MaterialAlphaMode::Mask;
            }
            material.renderState.doubleSided = materialValue.findBool("doubleSided").value_or(false);
            result.materials.push_back(std::move(material));
        }
    }

    void warnUnsupportedTopLevel(const GltfData& data, AssetImportResult& result)
    {
        const std::unordered_set<std::string> supportedExtensions = {
            "KHR_materials_emissive_strength"
        };
        const GtsJsonValue* extensionsRequired = data.root.find("extensionsRequired");
        if (extensionsRequired != nullptr && extensionsRequired->isArray())
        {
            for (const GtsJsonValue& extension : extensionsRequired->asArray())
            {
                const std::string name = extension.tryString().value_or("");
                if (!name.empty() && !supportedExtensions.contains(name))
                {
                    addDiagnostic(result, AssetDiagnosticSeverity::Error, "GLTF_UNSUPPORTED_EXTENSION",
                                  "Required glTF extension is unsupported: " + name, data.sourcePath);
                }
            }
        }

        const GtsJsonValue* extensionsUsed = data.root.find("extensionsUsed");
        if (extensionsUsed != nullptr && extensionsUsed->isArray())
        {
            for (const GtsJsonValue& extension : extensionsUsed->asArray())
            {
                const std::string name = extension.tryString().value_or("");
                if (name == "KHR_lights_punctual")
                {
                    addDiagnostic(result, AssetDiagnosticSeverity::Warning, "GLTF_LIGHT_SKIPPED",
                                  "glTF punctual lights are not imported", data.sourcePath);
                }
                else if (!name.empty() && !supportedExtensions.contains(name))
                {
                    addDiagnostic(result, AssetDiagnosticSeverity::Warning, "GLTF_UNSUPPORTED_EXTENSION",
                                  "Optional glTF extension is not imported: " + name, data.sourcePath);
                }
            }
        }

        if (const GtsJsonValue* skins = data.root.find("skins"); skins != nullptr && skins->isArray() && !skins->asArray().empty())
        {
            addDiagnostic(result, AssetDiagnosticSeverity::Warning, "GLTF_SKINNING_NOT_IMPLEMENTED",
                          "glTF skins are not imported", data.sourcePath);
        }
        if (const GtsJsonValue* animations = data.root.find("animations");
            animations != nullptr && animations->isArray() && !animations->asArray().empty())
        {
            addDiagnostic(result, AssetDiagnosticSeverity::Warning, "GLTF_ANIMATION_SKIPPED",
                          "glTF animations are not imported", data.sourcePath);
        }
        if (const GtsJsonValue* cameras = data.root.find("cameras"); cameras != nullptr && cameras->isArray() && !cameras->asArray().empty())
        {
            addDiagnostic(result, AssetDiagnosticSeverity::Warning, "GLTF_CAMERA_SKIPPED",
                          "glTF cameras are not imported", data.sourcePath);
        }
    }

    bool importMeshes(const GltfData& data, bool flipTexCoordV, AssetImportResult& result)
    {
        const GtsJsonValue* meshes = data.root.find("meshes");
        if (meshes == nullptr || !meshes->isArray())
            return true;

        result.meshes.reserve(meshes->asArray().size());
        for (size_t meshIndex = 0; meshIndex < meshes->asArray().size(); ++meshIndex)
        {
            const GtsJsonValue& meshValue = meshes->asArray()[meshIndex];
            ImportedMesh mesh;
            mesh.debugName = meshValue.findString("name").value_or("mesh_" + std::to_string(meshIndex));
            mesh.sourcePath = data.sourcePath;

            bool allNormalsPresent = true;
            bool allTangentsPresent = true;
            bool allTexCoordsPresent = true;
            const GtsJsonValue* primitives = meshValue.find("primitives");
            if (primitives == nullptr || !primitives->isArray())
                continue;

            for (size_t primitiveIndex = 0; primitiveIndex < primitives->asArray().size(); ++primitiveIndex)
            {
                const GtsJsonValue& primitive = primitives->asArray()[primitiveIndex];
                const uint32_t mode = primitive.findUInt32("mode").value_or(4u);
                if (mode != 4u)
                {
                    addDiagnostic(result, AssetDiagnosticSeverity::Error, "GLTF_PRIMITIVE_MODE_UNSUPPORTED",
                                  "Only glTF triangle primitives are supported", data.sourcePath);
                    return false;
                }

                const GtsJsonValue* attributes = primitive.find("attributes");
                if (attributes == nullptr || !attributes->isObject())
                {
                    addDiagnostic(result, AssetDiagnosticSeverity::Error, "GLTF_PRIMITIVE_MISSING_ATTRIBUTES",
                                  "glTF primitive is missing attributes", data.sourcePath);
                    return false;
                }

                const int32_t positionAccessor = attributeAccessor(*attributes, "POSITION");
                if (positionAccessor < 0 || static_cast<size_t>(positionAccessor) >= data.accessors.size())
                {
                    addDiagnostic(result, AssetDiagnosticSeverity::Error, "GLTF_POSITION_MISSING",
                                  "glTF primitive is missing POSITION data", data.sourcePath);
                    return false;
                }

                const int32_t normalAccessor = attributeAccessor(*attributes, "NORMAL");
                const int32_t tangentAccessor = attributeAccessor(*attributes, "TANGENT");
                const int32_t texCoordAccessor = attributeAccessor(*attributes, "TEXCOORD_0");
                const int32_t colorAccessor = attributeAccessor(*attributes, "COLOR_0");
                const int32_t indexAccessor = primitive.findInt32("indices").value_or(-1);
                const Accessor& positions = data.accessors[static_cast<size_t>(positionAccessor)];
                const size_t vertexOffset = mesh.vertices.size();
                const uint32_t firstIndex = static_cast<uint32_t>(mesh.indices.size());

                if (normalAccessor < 0)
                    allNormalsPresent = false;
                if (tangentAccessor < 0)
                    allTangentsPresent = false;
                if (texCoordAccessor < 0)
                    allTexCoordsPresent = false;

                if (attributes->find("TEXCOORD_1") != nullptr)
                {
                    addDiagnostic(result, AssetDiagnosticSeverity::Warning, "GLTF_MULTIPLE_UV_SETS_IGNORED",
                                  "Additional glTF UV sets are not imported", data.sourcePath);
                }
                if (attributes->find("JOINTS_0") != nullptr || attributes->find("WEIGHTS_0") != nullptr)
                {
                    addDiagnostic(result, AssetDiagnosticSeverity::Warning, "GLTF_SKINNING_NOT_IMPLEMENTED",
                                  "glTF joint and weight attributes are not imported", data.sourcePath);
                }
                if (const GtsJsonValue* targets = primitive.find("targets");
                    targets != nullptr && targets->isArray() && !targets->asArray().empty())
                {
                    addDiagnostic(result, AssetDiagnosticSeverity::Warning, "GLTF_MORPH_TARGETS_NOT_IMPLEMENTED",
                                  "glTF morph targets are not imported", data.sourcePath);
                }

                std::vector<float> components;
                for (size_t vertexIndex = 0; vertexIndex < positions.count; ++vertexIndex)
                {
                    Vertex vertex;
                    if (!readAccessorElement(data, positionAccessor, vertexIndex, components) ||
                        components.size() < 3u)
                    {
                        addDiagnostic(result, AssetDiagnosticSeverity::Error, "GLTF_POSITION_INVALID",
                                      "glTF POSITION accessor could not be read", data.sourcePath);
                        return false;
                    }
                    vertex.pos = {components[0], components[1], components[2]};

                    if (normalAccessor >= 0 &&
                        readAccessorElement(data, normalAccessor, vertexIndex, components) &&
                        components.size() >= 3u)
                    {
                        vertex.normal = safeNormalize(
                            {components[0], components[1], components[2]},
                            {0.0f, 0.0f, 1.0f});
                    }

                    if (tangentAccessor >= 0 &&
                        readAccessorElement(data, tangentAccessor, vertexIndex, components) &&
                        components.size() >= 4u)
                    {
                        vertex.tangent = {
                            components[0],
                            components[1],
                            components[2],
                            flipTexCoordV ? (components[3] * -1.0f) : components[3]
                        };
                    }

                    if (colorAccessor >= 0 &&
                        readAccessorElement(data, colorAccessor, vertexIndex, components))
                    {
                        vertex.color = components.size() >= 4u
                            ? glm::vec4(components[0], components[1], components[2], components[3])
                            : glm::vec4(components[0], components[1], components[2], 1.0f);
                    }

                    if (texCoordAccessor >= 0 &&
                        readAccessorElement(data, texCoordAccessor, vertexIndex, components) &&
                        components.size() >= 2u)
                    {
                        const float v = components[1];
                        vertex.texCoord = {
                            components[0],
                            flipTexCoordV ? 1.0f - v : v
                        };
                    }

                    mesh.vertices.push_back(vertex);
                }

                if (indexAccessor >= 0)
                {
                    if (static_cast<size_t>(indexAccessor) >= data.accessors.size())
                    {
                        addDiagnostic(result, AssetDiagnosticSeverity::Error, "GLTF_INDEX_INVALID",
                                      "glTF index accessor is invalid", data.sourcePath);
                        return false;
                    }
                    const Accessor& indices = data.accessors[static_cast<size_t>(indexAccessor)];
                    for (size_t index = 0; index < indices.count; ++index)
                    {
                        uint32_t value = 0;
                        if (!readIndex(data, indexAccessor, index, value) || value >= positions.count)
                        {
                            addDiagnostic(result, AssetDiagnosticSeverity::Error, "GLTF_INDEX_INVALID",
                                          "glTF index data is invalid", data.sourcePath);
                            return false;
                        }
                        mesh.indices.push_back(static_cast<uint32_t>(vertexOffset) + value);
                    }
                }
                else
                {
                    for (size_t index = 0; index < positions.count; ++index)
                        mesh.indices.push_back(static_cast<uint32_t>(vertexOffset + index));
                }

                ImportedMeshPrimitive importedPrimitive;
                importedPrimitive.name = mesh.debugName + "_primitive_" + std::to_string(primitiveIndex);
                importedPrimitive.firstIndex = firstIndex;
                importedPrimitive.indexCount = static_cast<uint32_t>(mesh.indices.size()) - firstIndex;
                importedPrimitive.materialIndex = primitive.findInt32("material").value_or(-1);
                if (importedPrimitive.materialIndex >= 0 &&
                    static_cast<size_t>(importedPrimitive.materialIndex) < result.materials.size())
                {
                    importedPrimitive.materialName =
                        result.materials[static_cast<size_t>(importedPrimitive.materialIndex)].name;
                }
                mesh.primitives.push_back(std::move(importedPrimitive));
            }

            mesh.hadMissingNormals = !allNormalsPresent;
            mesh.hadMissingTexCoords = !allTexCoordsPresent;
            mesh.sourceAttributes = VertexAttributeFlags::Position | VertexAttributeFlags::Color;
            if (allNormalsPresent && !mesh.indices.empty())
                mesh.sourceAttributes |= VertexAttributeFlags::Normal;
            if (allTangentsPresent && !mesh.indices.empty())
                mesh.sourceAttributes |= VertexAttributeFlags::Tangent;
            if (allTexCoordsPresent && !mesh.indices.empty())
                mesh.sourceAttributes |= VertexAttributeFlags::UV0;

            if (mesh.hadMissingNormals)
            {
                addDiagnostic(result, AssetDiagnosticSeverity::Warning, "GLTF_MISSING_NORMALS",
                              "glTF mesh has primitives without NORMAL data", data.sourcePath);
            }
            if (!allTangentsPresent)
            {
                addDiagnostic(result, AssetDiagnosticSeverity::Warning, "GLTF_MISSING_TANGENTS",
                              "glTF mesh has primitives without TANGENT data", data.sourcePath);
            }
            if (mesh.hadMissingTexCoords)
            {
                addDiagnostic(result, AssetDiagnosticSeverity::Warning, "GLTF_MISSING_UV0",
                              "glTF mesh has primitives without TEXCOORD_0 data", data.sourcePath);
            }

            result.meshes.push_back(std::move(mesh));
        }
        return true;
    }

    glm::mat4 nodeTransform(const GtsJsonValue& node)
    {
        if (const GtsJsonValue* matrix = node.find("matrix");
            matrix != nullptr && matrix->isArray() && matrix->asArray().size() >= 16u)
        {
            glm::mat4 result(1.0f);
            for (size_t col = 0; col < 4u; ++col)
            {
                for (size_t row = 0; row < 4u; ++row)
                    result[static_cast<int>(col)][static_cast<int>(row)] =
                        matrix->asArray()[col * 4u + row].tryFloat().value_or(col == row ? 1.0f : 0.0f);
            }
            return result;
        }

        const glm::vec3 translation = vec3Value(node.find("translation"), {0.0f, 0.0f, 0.0f});
        const glm::vec4 rotationValue = vec4Value(node.find("rotation"), {0.0f, 0.0f, 0.0f, 1.0f});
        const glm::vec3 scale = vec3Value(node.find("scale"), {1.0f, 1.0f, 1.0f});
        const glm::quat rotation(rotationValue.w, rotationValue.x, rotationValue.y, rotationValue.z);
        return glm::translate(glm::mat4(1.0f), translation)
            * glm::mat4_cast(rotation)
            * glm::scale(glm::mat4(1.0f), scale);
    }

    void importNodes(const GltfData& data, AssetImportResult& result)
    {
        const GtsJsonValue* nodes = data.root.find("nodes");
        if (nodes == nullptr || !nodes->isArray())
            return;

        result.nodes.resize(nodes->asArray().size());
        for (size_t nodeIndex = 0; nodeIndex < nodes->asArray().size(); ++nodeIndex)
        {
            const GtsJsonValue& node = nodes->asArray()[nodeIndex];
            ImportedNode imported;
            imported.name = node.findString("name").value_or("node_" + std::to_string(nodeIndex));
            imported.meshIndex = node.findInt32("mesh").value_or(-1);
            imported.localTransform = nodeTransform(node);
            result.nodes[nodeIndex] = std::move(imported);

            if (node.find("skin") != nullptr)
            {
                addDiagnostic(result, AssetDiagnosticSeverity::Warning, "GLTF_SKINNING_NOT_IMPLEMENTED",
                              "glTF node skin binding is not imported", data.sourcePath);
            }
            if (node.find("camera") != nullptr)
            {
                addDiagnostic(result, AssetDiagnosticSeverity::Warning, "GLTF_CAMERA_SKIPPED",
                              "glTF node camera binding is not imported", data.sourcePath);
            }
        }

        for (size_t nodeIndex = 0; nodeIndex < nodes->asArray().size(); ++nodeIndex)
        {
            const GtsJsonValue* children = nodes->asArray()[nodeIndex].find("children");
            if (children == nullptr || !children->isArray())
                continue;
            for (const GtsJsonValue& child : children->asArray())
            {
                const int32_t childIndex = child.tryInt32().value_or(-1);
                if (childIndex >= 0 && static_cast<size_t>(childIndex) < result.nodes.size())
                    result.nodes[static_cast<size_t>(childIndex)].parentIndex = static_cast<int32_t>(nodeIndex);
            }
        }
    }
}

std::string_view GltfAssetImporter::name() const
{
    return "gltf";
}

uint32_t GltfAssetImporter::version() const
{
    return 1;
}

AssetImportCapability GltfAssetImporter::capabilities() const
{
    return AssetImportCapability::Meshes
        | AssetImportCapability::Materials
        | AssetImportCapability::Textures
        | AssetImportCapability::Nodes;
}

bool GltfAssetImporter::supports(const std::filesystem::path& sourcePath) const
{
    std::string extension = sourcePath.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch)
    {
        return static_cast<char>(std::tolower(ch));
    });
    return extension == ".gltf" || extension == ".glb";
}

AssetImportResult GltfAssetImporter::importAsset(const AssetImportRequest& request) const
{
    AssetImportResult result;
    const std::filesystem::path sourcePath = request.sourcePath;
    addDependency(result, sourcePath, AssetDependencyType::SourceFile);

    if (sourcePath.empty() || !std::filesystem::exists(sourcePath))
    {
        addDiagnostic(result, AssetDiagnosticSeverity::Error, "ASSET_SOURCE_MISSING",
                      "glTF source file does not exist: " + sourcePath.string(), sourcePath);
        return result;
    }

    std::string jsonText;
    std::vector<uint8_t> binChunk;
    std::string error;
    std::string extension = sourcePath.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch)
    {
        return static_cast<char>(std::tolower(ch));
    });

    if (extension == ".glb")
    {
        if (!readGlb(sourcePath, jsonText, binChunk, error))
        {
            addDiagnostic(result, AssetDiagnosticSeverity::Error, "GLTF_READ_FAILED", error, sourcePath);
            return result;
        }
    }
    else
    {
        jsonText = readFileText(sourcePath);
        if (jsonText.empty())
        {
            addDiagnostic(result, AssetDiagnosticSeverity::Error, "GLTF_READ_FAILED",
                          "Could not read glTF JSON file", sourcePath);
            return result;
        }
    }

    GltfData data;
    data.sourcePath = sourcePath;
    data.baseDirectory = sourcePath.parent_path();
    if (!GtsJsonParser::parse(jsonText, data.root, &error))
    {
        addDiagnostic(result, AssetDiagnosticSeverity::Error, "GLTF_JSON_INVALID", error, sourcePath);
        return result;
    }

    warnUnsupportedTopLevel(data, result);
    if (result.hasErrors())
        return result;

    if (const GtsJsonValue* bufferViews = data.root.find("bufferViews"))
        appendBufferViews(*bufferViews, data.bufferViews);
    if (const GtsJsonValue* accessors = data.root.find("accessors"))
        appendAccessorArray(*accessors, data.accessors);
    const GtsJsonValue* buffers = data.root.find("buffers");
    if (buffers != nullptr && !loadBuffers(data, *buffers, binChunk, result))
        return result;

    if (request.options.includeSourceMaterials &&
        hasImportCapability(request.requestedCapabilities, AssetImportCapability::Textures))
    {
        importTextures(data, result);
    }
    if (request.options.includeSourceMaterials &&
        hasImportCapability(request.requestedCapabilities, AssetImportCapability::Materials))
    {
        importMaterials(data, result);
    }
    if (hasImportCapability(request.requestedCapabilities, AssetImportCapability::Meshes))
    {
        if (!importMeshes(data, request.options.flipTexCoordV, result))
            return result;
    }
    if (hasImportCapability(request.requestedCapabilities, AssetImportCapability::Nodes))
        importNodes(data, result);

    return result;
}
}
