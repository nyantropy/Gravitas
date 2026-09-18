#include "GtsImageDecode.h"
#include <memory>
#include <limits>
#include <stb_image.h>
namespace
{
    void setError(std::string* error, const std::string& message)
    {
        if (error)
            *error = message;
    }
    bool assignDecoded(
        stbi_uc* pixels, int width, int height, int sourceChannels, GtsDecodedImage& texture, std::string* error)
    {
        if (pixels == nullptr || width <= 0 || height <= 0)
        {
            setError(error, "Image decode failed");
            return false;
        }

        const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
        if (pixelCount == 0 || pixelCount > (static_cast<size_t>(1u) << 31u))
        {
            setError(error, "Image dimensions are invalid or too large");
            return false;
        }

        texture.width              = static_cast<uint32_t>(width);
        texture.height             = static_cast<uint32_t>(height);
        texture.sourceChannelCount = sourceChannels > 0 ? static_cast<uint32_t>(sourceChannels) : 4u;
        texture.rgba8Pixels.assign(pixels, pixels + pixelCount * 4u);
        return true;
    }
} // namespace
bool decodeGtsImage(const std::filesystem::path& path, GtsDecodedImage& texture, std::string* error)
{
    int width          = 0;
    int height         = 0;
    int sourceChannels = 0;

    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(
        stbi_load(path.string().c_str(), &width, &height, &sourceChannels, STBI_rgb_alpha), stbi_image_free);

    if (!pixels)
    {
        setError(error, "Could not decode image source: " + path.string());
        return false;
    }

    return assignDecoded(pixels.get(), width, height, sourceChannels, texture, error);
}

bool decodeGtsImage(std::span<const uint8_t> bytes, GtsDecodedImage& texture, std::string* error)
{
    if (bytes.empty())
    {
        setError(error, "Embedded image payload is empty");
        return false;
    }

    if (bytes.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
    {
        setError(error, "Embedded image payload exceeds decoder capacity");
        return false;
    }
    int width          = 0;
    int height         = 0;
    int sourceChannels = 0;

    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(
        stbi_load_from_memory(
            bytes.data(), static_cast<int>(bytes.size()), &width, &height, &sourceChannels, STBI_rgb_alpha),
        stbi_image_free);

    if (!pixels)
    {
        setError(error, "Could not decode embedded image payload");
        return false;
    }

    return assignDecoded(pixels.get(), width, height, sourceChannels, texture, error);
}
