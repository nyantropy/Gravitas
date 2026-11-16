#include "UiResourceContractTest.h"
#include <iostream>
#include <stdexcept>

#if __has_include("IResourceProvider.hpp") || __has_include("CameraGpuComponent.h") ||                       \
                                                            __has_include("UiSystem.h")
#error UI asset acquisition must not expose backend allocation or the UI runtime
#endif

template <class T>
concept AllocatesObjects = requires(T& value) { value.requestObjectSlot(); };
template <class T>
concept AllocatesCameras = requires(T& value) { value.requestCameraBuffer(); };
static_assert(!AllocatesObjects<IUiResourceProvider>);
static_assert(!AllocatesCameras<IUiResourceProvider>);

texture_id_type TestUiResourceProvider::requestTexture(const std::string& path)
{
    lastPath = path;
    return 7;
}
texture_id_type TestUiResourceProvider::requestClampedTexture(const std::string& path)
{
    lastPath = path;
    return 8;
}
texture_id_type TestUiResourceProvider::requestPixelTexture(const std::string& path)
{
    lastPath = path;
    return 9;
}
TextureDimensions TestUiResourceProvider::getTextureDimensions(texture_id_type id) const
{
    return id == 9 ? TextureDimensions{512, 512} : TextureDimensions{};
}
font_id_type TestUiResourceProvider::requestFont(const std::string& path)
{
    lastPath = path;
    return 10;
}
const BitmapFont* TestUiResourceProvider::getFont(font_id_type) const
{
    return nullptr;
}
int main()
try
{
    TestUiResourceProvider provider;
    IUiResourceProvider& acquisition = provider;
    if (acquisition.requestTexture("image", static_cast<TextureColorSpace>(0)) != 7 ||
        provider.lastPath != "image" ||
        acquisition.requestPixelTexture("atlas", static_cast<TextureColorSpace>(1)) != 9 ||
        provider.lastPath != "atlas" || acquisition.requestClampedTexture("panel") != 8 ||
        provider.lastPath != "panel" || !acquisition.getTextureDimensions(9).isValid() ||
        acquisition.getTextureDimensions(0).isValid() || acquisition.requestFont("font") != 10 ||
        provider.lastPath != "font" || acquisition.getFont(10) != nullptr)
        throw std::runtime_error("UI resource default dispatch/metadata contract failed");
    std::cout << "Narrow UI acquisition dispatch and publication passed\n";
    return 0;
}
catch (const std::exception& error)
{
    std::cerr << error.what() << '\n';
    return 1;
}
