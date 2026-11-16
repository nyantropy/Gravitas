#pragma once
#include "IUiResourceProvider.h"

class TestUiResourceProvider : public IUiResourceProvider
{
  public:
    texture_id_type requestTexture(const std::string& path) override;
    texture_id_type requestClampedTexture(const std::string& path) override;
    texture_id_type requestPixelTexture(const std::string& path) override;
    TextureDimensions getTextureDimensions(texture_id_type id) const override;
    font_id_type requestFont(const std::string& path) override;
    const BitmapFont* getFont(font_id_type id) const override;
    std::string lastPath;
};
