#include "IUiResourceProvider.h"

bool TextureDimensions::isValid() const
{
    return width > 0 && height > 0;
}

IUiResourceProvider::~IUiResourceProvider() = default;

texture_id_type IUiResourceProvider::requestTexture(const std::string& path, TextureColorSpace)
{
    return requestTexture(path);
}

texture_id_type IUiResourceProvider::requestPixelTexture(const std::string& path, TextureColorSpace)
{
    return requestPixelTexture(path);
}
