#pragma once

#include <filesystem>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "assets/realization/model/GtsRealizedModel.h"
#include "assets/model/GtsModelDiagnostic.h"
#include "MaterialRuntime.h"

class GtsRealizedModelMaterials
{
    public:
    // Associations are scoped by the exact realized model, never by a naked slot from another model.
    MaterialInstanceHandle materialFor(const GtsRealizedModel& owner, uint32_t geometry, uint32_t primitive) const;
    MaterialFrameState     frameStateFor(const GtsRealizedModel& owner, uint32_t geometry, uint32_t primitive) const;
    bool                   valid() const;
    bool belongsTo(const GtsRealizedModel& owner) const { return model.get() == &owner && valid(); }

    private:
    friend class GtsModelMaterialRealization;
    std::shared_ptr<const GtsRealizedModel>          model;
    gts::rendering::MaterialRuntime*                 runtime = nullptr;
    std::weak_ptr<const int>                         lifetime;
    std::vector<std::vector<MaterialInstanceHandle>> bindings;
};

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
