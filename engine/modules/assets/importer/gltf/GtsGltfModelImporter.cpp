#include "GtsGltfModelImporter.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <utility>

#include "GlmConfig.h"

#include <gtc/matrix_transform.hpp>
#include <gtc/quaternion.hpp>

#include "GltfSourceReader.h"
#include "GltfSkinImporter.h"
#include "GltfAnimationImporter.h"
#include "GltfSourceUtilities.h"
#include "assets/model/GtsModelImportResult.h"

namespace
{
    using namespace gts::gltf;
    using Severity = GtsModelDiagnosticSeverity;

    void extensions(const GtsJsonValue& root, std::vector<GtsModelDiagnostic>& diagnostics)
    {
        const std::string     supported = "KHR_materials_emissive_strength";
        std::set<std::string> warned;
        for (const auto& value : array(root.find("extensionsRequired"), "extensionsRequired"))
        {
            if (!value.isString())
                fail("GLTF_SCHEMA", "Extension name must be a string", "extensionsRequired");
            if (value.asString() != supported)
                fail("GLTF_REQUIRED_EXTENSION",
                     "Required extension is not supported: " + value.asString(),
                     "extensionsRequired");
        }
        auto warn = [&](const std::string& name, const std::string& location)
        {
            if (name != supported && warned.insert(name).second)
                diagnostics.push_back({Severity::Warning,
                                       "GLTF_OPTIONAL_EXTENSION",
                                       "Optional extension is not represented: " + name,
                                       location});
        };
        for (const auto& value : array(root.find("extensionsUsed"), "extensionsUsed"))
        {
            if (!value.isString())
                fail("GLTF_SCHEMA", "Extension name must be a string", "extensionsUsed");
            warn(value.asString(), "extensionsUsed");
        }
        std::vector<std::pair<const GtsJsonValue*, std::string>> pending{{&root, "root"}};
        while (!pending.empty())
        {
            const auto [value, location] = pending.back();
            pending.pop_back();
            if (value->isObject())
                for (const auto& [key, child] : value->asObject())
                {
                    if (key == "extras")
                        continue;
                    if (key == "extensions")
                    {
                        object(child, location + ".extensions");
                        for (const auto& [name, extension] : child.asObject())
                        {
                            object(extension, location + ".extensions." + name);
                            warn(name, location + ".extensions");
                        }
                    }
                    else
                        pending.emplace_back(&child, location + "." + key);
                }
            else if (value->isArray())
                for (size_t i = 0; i < value->asArray().size(); ++i)
                    pending.emplace_back(&value->asArray()[i], location + "[" + std::to_string(i) + "]");
        }
        if (!array(root.find("cameras"), "cameras").empty())
            diagnostics.push_back({Severity::Warning,
                                   "GLTF_CAMERAS_UNSUPPORTED",
                                   "Camera objects are not part of the model domain",
                                   "cameras"});
    }

    std::vector<uint32_t>
    images(const SourceDocument& data, GtsModelAsset& model, std::vector<GtsModelDiagnostic>& diagnostics)
    {
        std::vector<uint32_t> mapping;
        for (const auto& value : array(data.root.find("images"), "images"))
        {
            const auto loc = "images[" + std::to_string(mapping.size()) + "]";
            object(value, loc);
            GtsModelImage image;
            image.name     = stringField(value, "name", loc);
            const auto uri = stringField(value, "uri", loc);
            if (value.find("uri") && value.find("bufferView"))
                fail("GLTF_IMAGE_SOURCE", "Image must select URI or bufferView, not both", loc);
            if (value.find("uri"))
            {
                if (uri.starts_with("data:"))
                {
                    GtsModelEmbeddedImage embedded;
                    embedded.bytes    = dataUri(uri, embedded.mimeType, loc);
                    embedded.mimeType = stringField(value, "mimeType", loc, embedded.mimeType);
                    image.source      = std::move(embedded);
                }
                else
                {
                    const auto path = externalPath(data.directory, uri);
                    if (!std::filesystem::exists(path))
                        diagnostics.push_back({Severity::Warning,
                                               "GLTF_IMAGE_MISSING",
                                               "External image does not exist: " + path.string(),
                                               loc});
                    image.source = path;
                }
            }
            else
            {
                GtsModelEmbeddedImage embedded;
                const auto            view = uintField(value, "bufferView", loc);
                embedded.bytes             = data.viewBytes(view);
                if (data.views[view].stride)
                    fail("GLTF_IMAGE_SOURCE", "Image bufferView cannot have a vertex stride", loc);
                embedded.mimeType = stringField(value, "mimeType", loc);
                if (embedded.mimeType.empty())
                    fail("GLTF_IMAGE_SOURCE", "bufferView images require mimeType", loc);
                image.source = std::move(embedded);
            }
            auto same = [&](const GtsModelImage& other)
            {
                if (other.source.index() != image.source.index())
                    return false;
                if (const auto* path = std::get_if<std::filesystem::path>(&image.source))
                    return *path == std::get<std::filesystem::path>(other.source);
                const auto& a = std::get<GtsModelEmbeddedImage>(image.source);
                const auto& b = std::get<GtsModelEmbeddedImage>(other.source);
                return a.mimeType == b.mimeType && a.bytes == b.bytes;
            };
            const auto found = std::find_if(model.images.begin(), model.images.end(), same);
            mapping.push_back(static_cast<uint32_t>(found - model.images.begin()));
            if (found == model.images.end())
                model.images.push_back(std::move(image));
        }
        return mapping;
    }

    void materials(const SourceDocument& data, GtsModelAsset& model, std::vector<GtsModelDiagnostic>& diagnostics)
    {
        const auto  imageMapping = images(data, model, diagnostics);
        const auto& textures     = array(data.root.find("textures"), "textures");
        const auto& samplers     = array(data.root.find("samplers"), "samplers");
        for (size_t i = 0; i < samplers.size(); ++i)
        {
            const auto loc = "samplers[" + std::to_string(i) + "]";
            object(samplers[i], loc);
            for (const auto key : {"wrapS", "wrapT"})
            {
                const auto value = uintField(samplers[i], key, loc, 10497);
                if (value != 10497 && value != 33071 && value != 33648)
                    fail("GLTF_SAMPLER", "Invalid wrapping mode", loc);
            }
            if (samplers[i].find("magFilter"))
            {
                const auto value = uintField(samplers[i], "magFilter", loc);
                if (value != 9728 && value != 9729)
                    fail("GLTF_SAMPLER", "Invalid magnification filter", loc);
            }
            if (samplers[i].find("minFilter"))
            {
                const auto value = uintField(samplers[i], "minFilter", loc);
                if (value != 9728 && value != 9729 && (value < 9984 || value > 9987))
                    fail("GLTF_SAMPLER", "Invalid minification filter", loc);
            }
        }
        std::vector<uint32_t> textureImages;
        for (const auto& texture : textures)
        {
            const auto loc = "textures[" + std::to_string(textureImages.size()) + "]";
            object(texture, loc);
            const auto source = uintField(texture, "source", loc);
            if (source >= imageMapping.size())
                fail("GLTF_IMAGE_REFERENCE", "Texture source is not a valid image", loc);
            textureImages.push_back(imageMapping[source]);
            if (texture.find("sampler"))
            {
                if (uintField(texture, "sampler", loc) >= samplers.size())
                    fail("GLTF_SAMPLER", "Invalid sampler index", loc);
                diagnostics.push_back({Severity::Warning,
                                       "GLTF_SAMPLER_UNSUPPORTED",
                                       "Authored sampler state cannot be preserved by canonical image bindings",
                                       loc});
            }
        }
        auto binding = [&](const GtsJsonValue* value, const std::string& loc) -> std::optional<GtsModelImageBinding>
        {
            if (!value)
                return std::nullopt;
            object(*value, loc);
            const auto index = uintField(*value, "index", loc);
            if (index >= textureImages.size())
                fail("GLTF_TEXTURE_REFERENCE", "Invalid texture index", loc);
            return GtsModelImageBinding{textureImages[index], uintField(*value, "texCoord", loc, 0)};
        };
        for (const auto& value : array(data.root.find("materials"), "materials"))
        {
            const auto loc = "materials[" + std::to_string(model.materials.size()) + "]";
            object(value, loc);
            GtsModelMaterial material;
            material.name = stringField(value, "name", loc);
            // glTF's omitted metallic factor is one, independent of engine defaults.
            material.metallic = 1;
            if (const auto* pbr = value.find("pbrMetallicRoughness"))
            {
                object(*pbr, loc + ".pbrMetallicRoughness");
                if (const auto* factor = pbr->find("baseColorFactor"))
                {
                    const auto components = vectorValue(*factor, 4, loc + ".baseColorFactor");
                    material.baseColor    = {components[0], components[1], components[2], components[3]};
                }
                material.metallic       = floatField(*pbr, "metallicFactor", loc, 1);
                material.roughness      = floatField(*pbr, "roughnessFactor", loc, 1);
                material.baseColorImage = binding(pbr->find("baseColorTexture"), loc + ".baseColorTexture");
                if (auto image = binding(pbr->find("metallicRoughnessTexture"), loc + ".metallicRoughnessTexture"))
                {
                    material.metallicImage  = GtsModelScalarImageBinding{*image, GtsModelTextureChannel::Blue};
                    material.roughnessImage = GtsModelScalarImageBinding{*image, GtsModelTextureChannel::Green};
                }
            }
            material.normalImage = binding(value.find("normalTexture"), loc + ".normalTexture");
            if (const auto* normal = value.find("normalTexture"))
                material.normalScale = floatField(*normal, "scale", loc, 1);
            if (auto image = binding(value.find("occlusionTexture"), loc + ".occlusionTexture"))
            {
                material.ambientOcclusionImage    = GtsModelScalarImageBinding{*image, GtsModelTextureChannel::Red};
                material.ambientOcclusionStrength = floatField(*value.find("occlusionTexture"), "strength", loc, 1);
            }
            material.emissiveImage = binding(value.find("emissiveTexture"), loc + ".emissiveTexture");
            if (const auto* factor = value.find("emissiveFactor"))
            {
                const auto components   = vectorValue(*factor, 3, loc + ".emissiveFactor");
                material.emissiveFactor = {components[0], components[1], components[2]};
            }
            if (const auto* ext = value.find("extensions"))
                if (const auto* strength = ext->find("KHR_materials_emissive_strength"))
                    material.emissiveStrength = floatField(*strength, "emissiveStrength", loc, 1);
            const auto alpha = stringField(value, "alphaMode", loc, "OPAQUE");
            if (alpha == "MASK")
                material.alphaMode = GtsModelAlphaMode::Mask;
            else if (alpha == "BLEND")
                material.alphaMode = GtsModelAlphaMode::Blend;
            else if (alpha != "OPAQUE")
                fail("GLTF_ALPHA_MODE", "Unknown alpha mode", loc);
            material.alphaCutoff = floatField(value, "alphaCutoff", loc, 0.5f);
            material.doubleSided = boolField(value, "doubleSided", loc);
            model.materials.push_back(std::move(material));
        }
    }

    std::pair<GtsVertexSemantic, uint32_t> semantic(const std::string& name, const std::string& loc)
    {
        if (name == "POSITION")
            return {GtsVertexSemantic::Position, 0};
        if (name == "NORMAL")
            return {GtsVertexSemantic::Normal, 0};
        if (name == "TANGENT")
            return {GtsVertexSemantic::Tangent, 0};
        for (const auto& [prefix, kind] : std::initializer_list<std::pair<std::string, GtsVertexSemantic>>{
                 {"TEXCOORD_", GtsVertexSemantic::TexCoord},
                 {"COLOR_", GtsVertexSemantic::Color},
                 {"JOINTS_", GtsVertexSemantic::Joints},
                 {"WEIGHTS_", GtsVertexSemantic::Weights}})
        {
            if (!name.starts_with(prefix))
                continue;
            const auto digits       = std::string_view(name).substr(prefix.size());
            uint32_t   set          = 0;
            const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), set);
            if (digits.empty() || (digits.size() > 1 && digits[0] == '0') || error != std::errc{} ||
                end != digits.data() + digits.size())
                fail("GLTF_ATTRIBUTE_NAME", "Invalid numbered attribute set: " + name, loc);
            return {kind, set};
        }
        fail("GLTF_ATTRIBUTE_UNSUPPORTED", "Canonical semantic is not available for: " + name, loc);
    }

    GtsVertexAttribute attribute(
        const SourceDocument& data, const Accessor& a, GtsVertexSemantic kind, uint32_t set, const std::string& loc)
    {
        const bool   floating           = a.componentType == 5126 && !a.normalized;
        const bool   normalizedUnsigned = (a.componentType == 5121 || a.componentType == 5123) && a.normalized;
        const bool   joints             = kind == GtsVertexSemantic::Joints;
        const bool   vec3               = kind == GtsVertexSemantic::Position || kind == GtsVertexSemantic::Normal;
        const bool   color              = kind == GtsVertexSemantic::Color;
        const size_t components         = vec3 ? 3 : kind == GtsVertexSemantic::TexCoord ? 2 : 4;
        if ((a.components != components && !(color && a.type == "VEC3")) ||
            (a.type != "VEC2" && a.type != "VEC3" && a.type != "VEC4"))
            fail("GLTF_ATTRIBUTE_TYPE", "Wrong accessor vector type", loc);
        if (joints ? (a.normalized || (a.componentType != 5121 && a.componentType != 5123))
                   : ((vec3 || kind == GtsVertexSemantic::Tangent) ? !floating : !(floating || normalizedUnsigned)))
            fail("GLTF_ATTRIBUTE_ENCODING", "Unsupported component type/normalization for semantic", loc);
        const auto& view = data.views[a.view];
        if ((view.offset + a.offset) % 4 || a.offset % 4 || (a.count > 1 && a.stride % 4))
            fail("GLTF_ATTRIBUTE_ALIGNMENT", "Vertex attributes must have four-byte element alignment", loc);
        GtsVertexAttribute result;
        result.semantic = kind;
        result.setIndex = set;
        if (joints)
        {
            std::vector<glm::uvec4> values(a.count);
            for (size_t i = 0; i < a.count; ++i)
                for (size_t c = 0; c < 4; ++c)
                    values[i][c] = data.unsignedComponent(a, i, c);
            result.values = std::move(values);
        }
        else
        {
            auto read = [&](size_t i, size_t c)
            {
                const float value = data.floatComponent(a, i, c);
                if (!std::isfinite(value))
                    fail("GLTF_ATTRIBUTE_VALUE", "Attribute contains non-finite data", loc);
                if ((color || kind == GtsVertexSemantic::Weights) && (value < 0 || value > 1))
                    fail("GLTF_ATTRIBUTE_VALUE", "Colors/weights must be in [0,1]", loc);
                return value;
            };
            if (vec3)
            {
                std::vector<glm::vec3> values(a.count);
                for (size_t i = 0; i < a.count; ++i)
                    values[i] = {read(i, 0), read(i, 1), read(i, 2)};
                result.values = std::move(values);
            }
            else if (kind == GtsVertexSemantic::TexCoord)
            {
                std::vector<glm::vec2> values(a.count);
                for (size_t i = 0; i < a.count; ++i)
                    values[i] = {read(i, 0), 1 - read(i, 1)};
                result.values = std::move(values);
            }
            else
            {
                std::vector<glm::vec4> values(a.count);
                for (size_t i = 0; i < a.count; ++i)
                {
                    values[i] = {read(i, 0), read(i, 1), read(i, 2), a.components == 3 ? 1 : read(i, 3)};
                    if (kind == GtsVertexSemantic::Tangent)
                    {
                        if (values[i].w != 1 && values[i].w != -1)
                            fail("GLTF_TANGENT_W", "Tangent handedness must be +1 or -1", loc);
                        values[i].w *= -1;
                    }
                }
                result.values = std::move(values);
            }
        }
        return result;
    }

    void meshes(const SourceDocument& data, GtsModelAsset& model)
    {
        for (const auto& value : array(data.root.find("meshes"), "meshes"))
        {
            const auto loc = "meshes[" + std::to_string(model.meshes.size()) + "]";
            object(value, loc);
            if (value.find("weights"))
                fail("GLTF_MORPH_UNSUPPORTED", "Morph weights require a future canonical morph domain", loc);
            GtsModelMesh mesh;
            mesh.name              = stringField(value, "name", loc);
            const auto& primitives = array(value.find("primitives"), loc + ".primitives");
            if (primitives.empty())
                fail("GLTF_MESH_EMPTY", "Mesh must contain primitives", loc);
            for (const auto& source : primitives)
            {
                const auto ploc = loc + ".primitives[" + std::to_string(mesh.primitives.size()) + "]";
                object(source, ploc);
                if (source.find("targets"))
                    fail("GLTF_MORPH_UNSUPPORTED",
                         "Morph targets cannot be discarded; canonical morph storage is not yet supported",
                         ploc);
                if (uintField(source, "mode", ploc, 4) != 4)
                    fail("GLTF_TOPOLOGY_UNSUPPORTED", "This importer supports TRIANGLES only", ploc);
                const auto* attributes = source.find("attributes");
                if (!attributes)
                    fail("GLTF_POSITION_REQUIRED", "Primitive requires POSITION", ploc);
                object(*attributes, ploc + ".attributes");
                if (!attributes->find("POSITION"))
                    fail("GLTF_POSITION_REQUIRED", "Primitive requires POSITION", ploc);
                GtsModelPrimitive primitive;
                const auto        positionIndex = integer(*attributes->find("POSITION"), ploc + ".POSITION");
                const size_t      count         = data.accessor(positionIndex).count;
                std::map<GtsVertexSemantic, std::set<uint32_t>> sets;
                for (const auto& [name, index] : attributes->asObject())
                {
                    const auto aloc        = ploc + ".attributes." + name;
                    const auto [kind, set] = semantic(name, aloc);
                    if (!sets[kind].insert(set).second)
                        fail("GLTF_DUPLICATE_ATTRIBUTE", "Duplicate attribute", aloc);
                    const auto& accessor = data.accessor(integer(index, aloc));
                    if (accessor.count != count)
                        fail("GLTF_ATTRIBUTE_COUNT", "All attribute counts must match POSITION", aloc);
                    primitive.attributes.push_back(attribute(data, accessor, kind, set, aloc));
                }
                for (const auto& [kind, indices] : sets)
                {
                    uint32_t expected = 0;
                    for (const auto index : indices)
                        if (index != expected++)
                            fail("GLTF_ATTRIBUTE_SETS",
                                 "Numbered semantic sets must start at zero and be consecutive",
                                 ploc);
                }
                if (sets[GtsVertexSemantic::Joints] != sets[GtsVertexSemantic::Weights])
                    fail("GLTF_INFLUENCE_PAIR", "Each JOINTS set must have a matching WEIGHTS set", ploc);
                std::sort(primitive.attributes.begin(),
                          primitive.attributes.end(),
                          [](const auto& a, const auto& b)
                          {
                              return std::pair(a.semantic, a.setIndex) < std::pair(b.semantic, b.setIndex);
                          });
                if (const auto* indices = source.find("indices"))
                {
                    const auto& accessor = data.accessor(integer(*indices, ploc + ".indices"));
                    if (accessor.type != "SCALAR" || accessor.normalized || data.views[accessor.view].stride ||
                        (accessor.componentType != 5121 && accessor.componentType != 5123 &&
                         accessor.componentType != 5125))
                        fail("GLTF_INDEX_TYPE",
                             "Indices require a tightly packed, non-normalized unsigned SCALAR accessor",
                             ploc);
                    const uint32_t restart = accessor.componentType == 5121   ? 255
                                             : accessor.componentType == 5123 ? 65535
                                                                              : UINT32_MAX;
                    primitive.indices.reserve(accessor.count);
                    for (size_t i = 0; i < accessor.count; ++i)
                    {
                        const auto index = data.unsignedComponent(accessor, i);
                        if (index >= count || index == restart)
                            fail("GLTF_INDEX_RANGE", "Index is out of range or a reserved restart value", ploc);
                        primitive.indices.push_back(index);
                    }
                }
                else
                {
                    primitive.indices.resize(count);
                    std::iota(primitive.indices.begin(), primitive.indices.end(), 0u);
                }
                if (source.find("material"))
                {
                    primitive.materialIndex = uintField(source, "material", ploc);
                    if (*primitive.materialIndex >= model.materials.size())
                        fail("GLTF_MATERIAL_REFERENCE", "Invalid primitive material index", ploc);
                }
                mesh.primitives.push_back(std::move(primitive));
            }
            model.meshes.push_back(std::move(mesh));
        }
        // resolve the source default here; downstream defaults are not glTF defaults
        std::optional<uint32_t> defaultMaterial;
        for (auto& mesh : model.meshes)
            for (auto& primitive : mesh.primitives)
                if (!primitive.materialIndex)
                {
                    if (!defaultMaterial)
                    {
                        defaultMaterial = static_cast<uint32_t>(model.materials.size());
                        GtsModelMaterial material;
                        material.name     = "default";
                        material.metallic = 1;
                        model.materials.push_back(std::move(material));
                    }
                    primitive.materialIndex = defaultMaterial;
                }
    }

    GltfSkinImportResult
    nodes(const SourceDocument& data, GtsModelAsset& model, std::vector<GtsModelDiagnostic>& diagnostics)
    {
        const auto&                            sourceNodes = array(data.root.find("nodes"), "nodes");
        std::vector<uint32_t>                  parents(sourceNodes.size(), 0);
        std::vector<uint32_t>                  nodeSkins(sourceNodes.size(), UINT32_MAX);
        std::vector<GtsSkeletonLocalTransform> localTransforms(sourceNodes.size());
        for (size_t i = 0; i < sourceNodes.size(); ++i)
        {
            const auto  loc   = "nodes[" + std::to_string(i) + "]";
            const auto& value = sourceNodes[i];
            object(value, loc);
            if (value.find("skin"))
            {
                const auto index = uintField(value, "skin", loc);
                if (index >= array(data.root.find("skins"), "skins").size())
                    fail("GLTF_SKIN_REFERENCE", "Invalid skin index", loc);
                if (!value.find("mesh"))
                    fail("GLTF_SKIN_MESH_REQUIRED", "A node selecting a skin must also select a mesh", loc);
                nodeSkins[i] = index;
            }
            if (value.find("weights"))
                fail("GLTF_MORPH_UNSUPPORTED", "Node morph weights are not representable", loc);
            if (value.find("camera"))
            {
                if (uintField(value, "camera", loc) >= array(data.root.find("cameras"), "cameras").size())
                    fail("GLTF_CAMERA_REFERENCE", "Invalid camera index", loc);
            }
            GtsModelNode node;
            node.name = stringField(value, "name", loc);
            if (value.find("mesh"))
            {
                node.meshIndex = uintField(value, "mesh", loc);
                if (*node.meshIndex >= model.meshes.size())
                    fail("GLTF_MESH_REFERENCE", "Invalid node mesh index", loc);
            }
            if (const auto* matrix = value.find("matrix"))
            {
                if (value.find("translation") || value.find("rotation") || value.find("scale"))
                    fail("GLTF_NODE_TRANSFORM", "Node cannot author both matrix and TRS", loc);
                const auto components = vectorValue(*matrix, 16, loc + ".matrix");
                for (size_t c = 0; c < 4; ++c)
                    for (size_t r = 0; r < 4; ++r)
                        node.localTransform[c][r] = components[c * 4 + r];
                if (node.localTransform[0][3] != 0 || node.localTransform[1][3] != 0 ||
                    node.localTransform[2][3] != 0 || node.localTransform[3][3] != 1)
                    fail("GLTF_NODE_TRANSFORM", "Node matrix must be affine", loc);
                localTransforms[i] = node.localTransform;
            }
            else
            {
                glm::vec3 translation(0), scale(1);
                glm::vec4 rotation(0, 0, 0, 1);
                if (const auto* field = value.find("translation"))
                {
                    const auto values = vectorValue(*field, 3, loc + ".translation");
                    translation       = {values[0], values[1], values[2]};
                }
                if (const auto* field = value.find("scale"))
                {
                    const auto values = vectorValue(*field, 3, loc + ".scale");
                    scale             = {values[0], values[1], values[2]};
                }
                if (const auto* field = value.find("rotation"))
                {
                    const auto values = vectorValue(*field, 4, loc + ".rotation");
                    rotation          = {values[0], values[1], values[2], values[3]};
                    if (std::abs(glm::dot(rotation, rotation) - 1) > 0.0001f)
                        fail("GLTF_NODE_TRANSFORM", "Rotation quaternion must have unit length", loc);
                }
                localTransforms[i] =
                    GtsSkeletonTrs{translation, glm::quat(rotation.w, rotation.x, rotation.y, rotation.z), scale};
                node.localTransform = glm::translate(glm::mat4(1), translation) *
                                      glm::mat4_cast(glm::quat(rotation.w, rotation.x, rotation.y, rotation.z)) *
                                      glm::scale(glm::mat4(1), scale);
            }
            for (const auto& child : array(value.find("children"), loc + ".children"))
            {
                const auto index = integer(child, loc + ".children");
                if (index >= sourceNodes.size())
                    fail("GLTF_CHILD_REFERENCE", "Invalid child index", loc);
                if (++parents[index] != 1)
                    fail("GLTF_MULTIPLE_PARENT", "Duplicate child or multiple parents", loc);
                node.children.push_back(index);
            }
            for (size_t column = 0; column < 4; ++column)
                for (size_t row = 0; row < 4; ++row)
                    if (!std::isfinite(node.localTransform[column][row]))
                        fail("GLTF_NODE_TRANSFORM", "Transform composition overflowed", loc);
            model.nodes.push_back(std::move(node));
        }
        std::vector<uint32_t> forestRoots;
        for (uint32_t i = 0; i < parents.size(); ++i)
            if (!parents[i])
                forestRoots.push_back(i);
        auto reachable = [&](const std::vector<uint32_t>& roots)
        {
            std::vector<bool> visited(model.nodes.size(), false);
            auto              pending = roots;
            while (!pending.empty())
            {
                const auto index = pending.back();
                pending.pop_back();
                if (visited[index])
                    fail("GLTF_NODE_CYCLE", "Repeated node in hierarchy", "nodes");
                visited[index] = true;
                pending.insert(pending.end(), model.nodes[index].children.begin(), model.nodes[index].children.end());
            }
            return visited;
        };
        const auto forest = reachable(forestRoots);
        if (std::find(forest.begin(), forest.end(), false) != forest.end())
            fail("GLTF_NODE_CYCLE", "Node hierarchy contains a cycle", "nodes");
        const auto&                        scenes = array(data.root.find("scenes"), "scenes");
        std::vector<std::vector<uint32_t>> sceneRoots;
        for (const auto& scene : scenes)
        {
            object(scene, "scenes");
            std::vector<uint32_t> roots;
            std::set<uint32_t>    seen;
            for (const auto& value : array(scene.find("nodes"), "scenes.nodes"))
            {
                const auto index = integer(value, "scenes.nodes");
                if (index >= model.nodes.size() || parents[index] || !seen.insert(index).second)
                    fail("GLTF_SCENE_ROOT", "Scene roots must be unique, valid, parentless nodes", "scenes");
                roots.push_back(index);
            }
            sceneRoots.push_back(std::move(roots));
        }
        std::vector<uint32_t> roots = forestRoots;
        if (data.root.find("scene") || !scenes.empty())
        {
            const auto selected = uintField(data.root, "scene", "root", 0);
            if (selected >= scenes.size())
                fail("GLTF_SCENE_REFERENCE", "Invalid default scene", "scene");
            if (!data.root.find("scene"))
                diagnostics.push_back(
                    {Severity::Warning, "GLTF_SCENE_DEFAULT", "No default scene; selecting scene 0", "scenes[0]"});
            roots = sceneRoots[selected];
        }
        else if (!model.nodes.empty())
            diagnostics.push_back({Severity::Warning,
                                   "GLTF_SCENE_LIBRARY",
                                   "No scenes; preserving all node trees as a model library",
                                   "nodes"});
        const auto                active    = reachable(roots);
        auto                      skeletons = importSkins(data, model, localTransforms, nodeSkins, active, diagnostics);
        std::vector<uint32_t>     mapping(model.nodes.size(), UINT32_MAX);
        std::vector<GtsModelNode> selected;
        for (uint32_t i = 0; i < model.nodes.size(); ++i)
            if (active[i])
            {
                mapping[i] = static_cast<uint32_t>(selected.size());
                selected.push_back(std::move(model.nodes[i]));
            }
        if (selected.size() != model.nodes.size())
            diagnostics.push_back(
                {Severity::Warning,
                 "GLTF_SCENE_NODES_EXCLUDED",
                 "Nodes outside the selected scene are excluded; meshes/materials remain a shared library",
                 "nodes"});
        for (auto& node : selected)
            for (auto& child : node.children)
                child = mapping[child];
        for (const auto root : roots)
            model.rootNodes.push_back(mapping[root]);
        for (auto& use : model.skeletonUses)
            for (auto& nodeIndex : use.modelNodeIndices)
                nodeIndex = mapping[nodeIndex];
        model.nodes = std::move(selected);
        return skeletons;
    }
} // namespace

GtsModelImportResult GtsGltfModelImporter::importAsset(const GtsModelImportRequest& request) const
{
    std::vector<GtsModelDiagnostic> diagnostics;
    try
    {
        const auto data = gts::gltf::readSource(request.sourcePath, diagnostics);
        extensions(data.root, diagnostics);
        GtsModelAsset model;
        materials(data, model, diagnostics);
        meshes(data, model);
        auto skins = nodes(data, model, diagnostics);
        auto clips = gts::gltf::importAnimations(data, skins);
        return GtsModelImportResult::success(
            GtsModelImportBundle{std::move(model), std::move(skins.skeletons), std::move(clips)},
            std::move(diagnostics));
    }
    catch (const gts::gltf::DecodeError& error)
    {
        diagnostics.push_back(error.diagnostic);
    }
    catch (const std::filesystem::filesystem_error& error)
    {
        diagnostics.push_back({Severity::Error, "GLTF_IO", error.what(), request.sourcePath.string()});
    }
    return GtsModelImportResult::failure(std::move(diagnostics));
}
