#include "GtsModelAsset.h"
#include "GtsModelImportResult.h"
#include "GtsModelValidation.h"

#include <cstdio>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
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

    void requireError(const GtsModelValidationResult& result, const std::string& code,
                      const std::string& location)
    {
        require(!result.isValid(), "Malformed material data must fail validation");
        for (const auto& diagnostic : result.diagnostics)
        {
            if (diagnostic.code == code && diagnostic.location == location)
            {
                require(diagnostic.severity == GtsModelDiagnosticSeverity::Error, "Validation reports errors");
                require(!diagnostic.message.empty(), "Error explains the violation");
                return;
            }
        }
        throw std::runtime_error("Expected diagnostic: " + code + " at " + location);
    }

    GtsModelAsset texturedModel()
    {
        GtsModelAsset asset;
        // The validator must not open these inputs or decode the deliberately opaque bytes.
        asset.images = {
            {"external", std::filesystem::absolute("nonexistent-model-material-test.png")},
            {"embedded", GtsModelEmbeddedImage{{1, 2, 3}, "image/png"}}
        };
        GtsModelMaterial material;
        material.name = "surface";
        material.baseColor = {0.2f, 0.4f, 0.6f, 0.8f};
        material.metallic = 0.7f;
        material.roughness = 0.3f;
        material.emissiveFactor = {0.5f, 2.0f, 3.0f};
        material.emissiveStrength = 4.0f;
        material.normalScale = -0.5f;
        material.ambientOcclusionStrength = 0.6f;
        material.baseColorImage = {0, 0};
        material.metallicImage = {{1, 1}, GtsModelTextureChannel::Blue};
        material.roughnessImage = {{1, 1}, GtsModelTextureChannel::Green};
        material.normalImage = {0, 0};
        material.ambientOcclusionImage = {{1, 1}, GtsModelTextureChannel::Red};
        material.emissiveImage = {0, 0};
        material.alphaMode = GtsModelAlphaMode::Mask;
        material.alphaCutoff = 0.25f;
        material.doubleSided = true;
        asset.materials = {material, GtsModelMaterial{"plain"}};

        GtsModelPrimitive primitive;
        primitive.attributes = {
            {GtsVertexSemantic::Position, 0, std::vector<glm::vec3>{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}}},
            {GtsVertexSemantic::TexCoord, 0, std::vector<glm::vec2>(3, glm::vec2(0))},
            {GtsVertexSemantic::TexCoord, 1, std::vector<glm::vec2>(3, glm::vec2(1))}
        };
        primitive.indices = {0, 1, 2};
        primitive.materialIndex = 0;
        asset.meshes = {{"mesh", {primitive, primitive, primitive, primitive}}};
        asset.meshes[0].primitives[2].materialIndex = 1;
        asset.meshes[0].primitives[3].materialIndex.reset();
        asset.meshes[0].primitives[3].attributes.resize(1);
        return asset;
    }

    void defaultsAndAppearance()
    {
        const GtsModelMaterial material;
        require(validateGtsModelMaterial(material).isValid(), "Default material is valid");
        require(material.name.empty() && material.baseColor == glm::vec4(1), "Default is unnamed white");
        require(material.metallic == 0 && material.roughness == 1, "Default is rough nonmetal");
        require(material.emissiveFactor == glm::vec3(0) && material.emissiveStrength == 1, "Default is nonemissive");
        require(material.normalScale == 1 && material.ambientOcclusionStrength == 1, "Default scales are one");
        require(material.alphaMode == GtsModelAlphaMode::Opaque && material.alphaCutoff == 0.5f
            && !material.doubleSided, "Default surface semantics are deterministic");
        require(!material.baseColorImage && !material.metallicImage && !material.roughnessImage
            && !material.normalImage && !material.ambientOcclusionImage && !material.emissiveImage,
            "Missing textures are absent bindings");

        auto asset = texturedModel();
        require(validateGtsModelAsset(asset).isValid(), "All roles, UV sets, shared and distinct materials are valid");
        const auto imported = GtsModelImportResult::success(asset);
        require(imported.succeeded(), "Appearance survives the import result contract");
        const auto& retained = imported.asset()->materials[0];
        require(retained.baseColor == glm::vec4(0.2f, 0.4f, 0.6f, 0.8f)
            && retained.metallic == 0.7f && retained.roughness == 0.3f
            && retained.emissiveFactor == glm::vec3(0.5f, 2, 3) && retained.emissiveStrength == 4
            && retained.normalScale == -0.5f && retained.ambientOcclusionStrength == 0.6f,
            "Validation retains authored factors without clamping");
        require(retained.doubleSided && retained.alphaCutoff == 0.25f, "Sidedness and cutoff survive import");
        require(retained.metallicImage->image.imageIndex == retained.roughnessImage->image.imageIndex
            && retained.metallicImage->channel == GtsModelTextureChannel::Blue
            && retained.roughnessImage->channel == GtsModelTextureChannel::Green,
            "Packed maps retain independent channel selections");
        asset.materials[0].roughnessImage->image.imageIndex = 0;
        asset.materials[0].roughnessImage->channel = GtsModelTextureChannel::Alpha;
        require(validateGtsModelAsset(asset).isValid(), "Separate scalar images and alpha channel are also valid");
        for (auto mode : {GtsModelAlphaMode::Opaque, GtsModelAlphaMode::Mask, GtsModelAlphaMode::Blend})
        {
            asset.materials[0].alphaMode = mode;
            require(validateGtsModelAsset(asset).isValid(), "Every alpha mode is representable without render state");
        }
        asset.materials[0].roughness = 0;
        asset.materials[0].metallic = 1;
        asset.materials[0].alphaCutoff = 0;
        require(validateGtsModelAsset(asset).isValid(), "Physical boundaries are valid without renderer roughness limits");
    }

    void invalidNumericData()
    {
        GtsModelMaterial material;
        const std::vector<std::pair<float*, std::string>> fields = {
            {&material.baseColor.x, "baseColor[0]"}, {&material.baseColor.y, "baseColor[1]"},
            {&material.baseColor.z, "baseColor[2]"}, {&material.baseColor.w, "baseColor[3]"},
            {&material.emissiveFactor.x, "emissiveFactor[0]"}, {&material.emissiveFactor.y, "emissiveFactor[1]"},
            {&material.emissiveFactor.z, "emissiveFactor[2]"}, {&material.metallic, "metallic"},
            {&material.roughness, "roughness"}, {&material.emissiveStrength, "emissiveStrength"},
            {&material.normalScale, "normalScale"}, {&material.ambientOcclusionStrength, "ambientOcclusionStrength"},
            {&material.alphaCutoff, "alphaCutoff"}
        };
        for (const auto& [value, field] : fields)
        {
            const float original = *value;
            for (float invalid : {std::numeric_limits<float>::quiet_NaN(),
                                  std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()})
            {
                *value = invalid;
                requireError(validateGtsModelMaterial(material), "MODEL_MATERIAL_NONFINITE", "material." + field);
            }
            if (field != "normalScale")
            {
                *value = -0.1f;
                requireError(validateGtsModelMaterial(material), "MODEL_MATERIAL_RANGE", "material." + field);
            }
            if (field.starts_with("baseColor") || field == "metallic" || field == "roughness"
                || field == "ambientOcclusionStrength" || field == "alphaCutoff")
            {
                *value = 1.1f;
                requireError(validateGtsModelMaterial(material), "MODEL_MATERIAL_RANGE", "material." + field);
            }
            *value = original;
        }
        material.alphaMode = static_cast<GtsModelAlphaMode>(99);
        requireError(validateGtsModelMaterial(material), "MODEL_ALPHA_MODE_INVALID", "material.alphaMode");
        auto asset = texturedModel();
        asset.materials[1].roughness = -1;
        requireError(validateGtsModelAsset(asset), "MODEL_MATERIAL_RANGE", "materials[1].roughness");
        const auto rejected = GtsModelImportResult::success(asset);
        require(!rejected.succeeded() && !rejected.asset(), "Invalid material cannot cross the import boundary");
    }

    void defaultImageReferences()
    {
        const GtsModelImageBinding binding;
        const GtsModelScalarImageBinding scalar;
        require(binding.imageIndex == GtsModelImageBinding::InvalidImageIndex && binding.imageIndex != 0,
            "Default image reference must not select image zero");
        require(scalar.image.imageIndex == GtsModelImageBinding::InvalidImageIndex,
            "Default scalar image reference is also invalid");

        auto asset = texturedModel();
        auto& material = asset.materials[0];
        const std::vector<std::pair<GtsModelImageBinding*, std::string>> bindings = {
            {&*material.baseColorImage, "baseColorImage"},
            {&material.metallicImage->image, "metallicImage"},
            {&material.roughnessImage->image, "roughnessImage"},
            {&*material.normalImage, "normalImage"},
            {&material.ambientOcclusionImage->image, "ambientOcclusionImage"},
            {&*material.emissiveImage, "emissiveImage"}
        };
        for (const auto& [image, field] : bindings)
        {
            *image = GtsModelImageBinding{};
            requireError(validateGtsModelMaterial(material), "MODEL_IMAGE_OUT_OF_RANGE", "material." + field);
            requireError(validateGtsModelAsset(asset), "MODEL_IMAGE_OUT_OF_RANGE", "materials[0]." + field);
            require(!GtsModelImportResult::success(asset).succeeded(), "Uninitialized binding cannot cross the import boundary");
            image->imageIndex = 0;
            image->texCoordSet = 1;
            require(validateGtsModelAsset(asset).isValid(), "Explicit image zero and UV set one are valid for every role");
        }
        const auto imported = GtsModelImportResult::success(asset);
        require(imported.succeeded(), "Explicit references pass import validation");
        const auto& retained = imported.asset()->materials[0];
        require(retained.metallicImage->image.imageIndex == 0 && retained.metallicImage->image.texCoordSet == 1
            && retained.metallicImage->channel == GtsModelTextureChannel::Blue
            && retained.roughnessImage->channel == GtsModelTextureChannel::Green,
            "Image references retain UV sets and independent scalar channels");
    }

    void referencesAndImages()
    {
        auto asset = texturedModel();
        asset.images.clear();
        for (const char* role : {"baseColorImage", "metallicImage", "roughnessImage",
                                 "normalImage", "ambientOcclusionImage", "emissiveImage"})
        {
            requireError(validateGtsModelAsset(asset), "MODEL_IMAGE_OUT_OF_RANGE", std::string("materials[0].") + role);
        }
        asset = texturedModel();
        asset.meshes[0].primitives[1].attributes.resize(1);
        for (const char* role : {"baseColorImage", "metallicImage", "roughnessImage",
                                 "normalImage", "ambientOcclusionImage", "emissiveImage"})
        {
            requireError(validateGtsModelAsset(asset), "MODEL_TEXCOORD_REQUIRED",
                std::string("meshes[0].primitives[1].material.") + role);
        }
        asset = texturedModel();
        const uint32_t highSet = std::numeric_limits<uint32_t>::max();
        asset.materials[0].baseColorImage->texCoordSet = highSet;
        requireError(validateGtsModelAsset(asset), "MODEL_TEXCOORD_REQUIRED", "meshes[0].primitives[0].material.baseColorImage");
        for (auto& primitive : asset.meshes[0].primitives)
        {
            primitive.attributes.push_back({GtsVertexSemantic::TexCoord, highSet, std::vector<glm::vec2>(3, glm::vec2(0))});
        }
        require(validateGtsModelAsset(asset).isValid(), "UV set identifiers are not limited by renderer capabilities");
        asset.meshes[0].primitives[0].materialIndex = 2;
        requireError(validateGtsModelAsset(asset), "MODEL_MATERIAL_OUT_OF_RANGE", "meshes[0].primitives[0]");

        asset = texturedModel();
        for (auto* binding : {&*asset.materials[0].metallicImage, &*asset.materials[0].roughnessImage,
                             &*asset.materials[0].ambientOcclusionImage})
        {
            binding->channel = static_cast<GtsModelTextureChannel>(99);
        }
        for (const char* role : {"metallicImage", "roughnessImage", "ambientOcclusionImage"})
        {
            requireError(validateGtsModelAsset(asset), "MODEL_TEXTURE_CHANNEL_INVALID", std::string("materials[0].") + role);
        }
        asset = texturedModel();
        for (const auto& path : {std::filesystem::path{}, std::filesystem::path("relative.png")})
        {
            asset.images[0].source = path;
            requireError(validateGtsModelAsset(asset), "MODEL_IMAGE_PATH_INVALID", "images[0]");
        }
        asset = texturedModel();
        std::get<GtsModelEmbeddedImage>(asset.images[1].source).bytes.clear();
        requireError(validateGtsModelAsset(asset), "MODEL_IMAGE_BYTES_REQUIRED", "images[1]");
        asset = texturedModel();
        std::get<GtsModelEmbeddedImage>(asset.images[1].source).mimeType.clear();
        require(validateGtsModelAsset(asset).isValid(), "MIME is an optional decoder hint");
        asset.meshes.clear();
        require(validateGtsModelAsset(asset).isValid(), "Unused materials need no geometry, but still validate their own data");
        asset.materials[0].baseColorImage->imageIndex = 99;
        requireError(validateGtsModelAsset(asset), "MODEL_IMAGE_OUT_OF_RANGE", "materials[0].baseColorImage");
    }
}

int main()
{
    try
    {
        defaultsAndAppearance();
        invalidNumericData();
        defaultImageReferences();
        referencesAndImages();
        std::puts("GtsModelMaterialTest passed");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
