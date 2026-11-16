#pragma once

#include "ResourceTypes.h"
#include <string>
enum class TextureColorSpace;

struct BitmapFont;

struct TextureDimensions
{
    int width = 0;
    int height = 0;

    bool isValid() const;
};

// Borrowed asset acquisition only. Renderer/backend owns returned resources;
// callers must not retain this service beyond the supplying context's lifetime.
class IUiResourceProvider
{
  public:
    virtual ~IUiResourceProvider();
    // request a texture from the resource provider, no nearest neighbor
    virtual texture_id_type requestTexture(const std::string& path) = 0;
    virtual texture_id_type requestTexture(const std::string& path, TextureColorSpace colorSpace);
    // Like requestTexture but clamps UVs to the texture edge while preserving linear filtering.
    virtual texture_id_type requestClampedTexture(const std::string& path) = 0;
    // Like requestTexture but forces NEAREST-neighbor sampling and no anisotropy.
    // Use for pixel-art or bitmap-font atlases that must render crisp.
    virtual texture_id_type requestPixelTexture(const std::string& path) = 0;
    virtual texture_id_type requestPixelTexture(const std::string& path, TextureColorSpace colorSpace);
    virtual TextureDimensions getTextureDimensions(texture_id_type id) const = 0;

    // request a bitmap font asset; the font manager owns metadata and uses
    // the texture manager for the atlas texture
    virtual font_id_type requestFont(const std::string& path) = 0;
    virtual const BitmapFont* getFont(font_id_type id) const = 0;
};
