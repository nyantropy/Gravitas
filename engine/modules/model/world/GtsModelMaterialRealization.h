#pragma once

#include <filesystem>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "GtsRealizedModel.h"
#include "GtsModelDiagnostic.h"
#include "MaterialRuntime.h"
#include "GtsRealizedModelMaterials.h"

struct GtsModelMaterialResult
{
    std::shared_ptr<const GtsRealizedModelMaterials> materials;
    std::vector<GtsModelDiagnostic>                  diagnostics;
    bool                                             succeeded() const
    {
        return static_cast<bool>(materials);
    }
};

// One world/runtime and stable resource provider. Shared model definitions contain no handles.
class GtsModelMaterialRealization
{
    public:
    GtsModelMaterialRealization(gts::rendering::MaterialRuntime& runtime, IResourceProvider* resources);
    GtsModelMaterialResult realize(std::shared_ptr<const GtsRealizedModel> model);
    bool belongsTo(const gts::rendering::MaterialRuntime& owner, const IResourceProvider* provider) const;

    private:
    gts::rendering::MaterialRuntime& runtime;
    IResourceProvider*               resources;
    std::weak_ptr<const int>         lifetime;
    using Image         = std::shared_ptr<const GtsDecodedImage>;
    using ImageSlots    = std::map<uint32_t, Image>;
    using PackedImages  = std::map<std::string, Image>;
    using MaterialSlots = std::map<uint32_t, MaterialInstanceHandle>;

    std::map<std::shared_ptr<const GtsRealizedModel>,
             GtsModelMaterialResult,
             std::owner_less<std::shared_ptr<const GtsRealizedModel>>>
        models;

    std::map<GtsModelHandle, MaterialSlots, std::owner_less<GtsModelHandle>> canonicalHandles;
    std::map<std::string, MaterialInstanceHandle>                            externalHandles;

    std::map<GtsModelHandle, ImageSlots, std::owner_less<GtsModelHandle>> images;
    std::map<std::filesystem::path, Image>                                externalImages;

    std::map<GtsModelHandle, PackedImages, std::owner_less<GtsModelHandle>> packedImages;
    std::map<std::pair<Image, TextureColorSpace>, texture_id_type>          textures;

    std::shared_ptr<const GtsDecodedImage> image(const GtsModelHandle& model, uint32_t index);
    MaterialTextureBinding texture(std::shared_ptr<const GtsDecodedImage> image, TextureColorSpace space);
    MaterialInstance       canonicalInstance(const GtsModelHandle& model, uint32_t slot);
};

// ECS owns the service; MaterialRuntime lifetime tokens invalidate associations on reset/destruction.
GtsModelMaterialRealization& modelMaterialRealization(ECSWorld& world, IResourceProvider* resources);
