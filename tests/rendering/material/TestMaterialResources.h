#pragma once
#include "model/world/GtsModelMaterialRealization.h"
#include <map>
#include <stdexcept>

struct Resources : IResourceProvider
{
    struct Upload
    {
        std::shared_ptr<const GtsDecodedImage> image;
        TextureColorSpace                      space;
        texture_id_type                        id;
    };
    std::vector<Upload>                    uploads;
    std::map<std::string, texture_id_type> paths;
    texture_id_type                        next         = 10;
    bool                                   fail         = false;
    bool                                   failFallback = false;
    texture_id_type requestMemoryTexture(std::shared_ptr<const GtsDecodedImage> image, TextureColorSpace space) override
    {
        if (fail)
            return 0;
        uploads.push_back({image, space, next});
        return next++;
    }
    texture_id_type requestTexture(const std::string& path) override
    {
        return requestTexture(path, TextureColorSpace::SRgb);
    }
    texture_id_type requestTexture(const std::string& path, TextureColorSpace space) override
    {
        if (!path.empty() && !std::filesystem::exists(path))
            throw std::runtime_error("missing texture: " + path);
        auto key            = path + std::to_string(static_cast<int>(space));
        auto [it, inserted] = paths.emplace(key, next);
        if (inserted)
            ++next;
        return fail ? 0 : it->second;
    }
    texture_id_type requestMaterialFallbackTexture(MaterialTextureRole role) override
    {
        return failFallback ? 0 : 1 + static_cast<int>(role);
    }
    mesh_id_type requestMesh(const std::string&) override
    {
        return 0;
    }
    mesh_id_type getSharedQuadMesh(float, float) override
    {
        return 0;
    }
    mesh_id_type uploadProceduralMesh(mesh_id_type,
                                      const std::vector<GtsStaticVertex>&,
                                      const std::vector<uint32_t>&,
                                      VertexAttributeFlags) override
    {
        return 0;
    }
    void            releaseProceduralMesh(mesh_id_type) override {}
    texture_id_type requestClampedTexture(const std::string& p) override
    {
        return requestTexture(p);
    }
    texture_id_type requestPixelTexture(const std::string& p) override
    {
        return requestTexture(p);
    }
    TextureDimensions getTextureDimensions(texture_id_type) const override
    {
        return {};
    }
    font_id_type requestFont(const std::string&) override
    {
        return 0;
    }
    const BitmapFont* getFont(font_id_type) const override
    {
        return nullptr;
    }
    view_id_type requestCameraBuffer() override
    {
        return 0;
    }
    void         releaseCameraBuffer(view_id_type) override {}
    void         uploadCameraView(view_id_type, const glm::mat4&, const glm::mat4&) override {}
    ssbo_id_type requestObjectSlot() override
    {
        return 0;
    }
    void          releaseObjectSlot(ssbo_id_type) override {}
    const Upload& upload(texture_id_type id) const
    {
        for (const auto& item : uploads)
            if (item.id == id)
                return item;
        throw std::runtime_error("missing captured upload");
    }
};
