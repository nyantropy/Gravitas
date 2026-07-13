#include "TextureCooker.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

namespace gts::rendering
{
namespace
{
    void addDiagnostic(TextureCookResult& result,
                       AssetDiagnosticSeverity severity,
                       std::string code,
                       std::string message,
                       const std::filesystem::path& sourcePath)
    {
        result.diagnostics.push_back({
            severity,
            std::move(code),
            std::move(message),
            sourcePath,
            0
        });
    }

    bool isNormalRole(TextureCookRole role)
    {
        return role == TextureCookRole::Normal;
    }

    bool isFontAtlasRole(TextureCookRole role)
    {
        return role == TextureCookRole::FontAtlas;
    }

    uint8_t averageChannel(uint32_t sum, uint32_t count)
    {
        return static_cast<uint8_t>((sum + count / 2u) / count);
    }

    void convertGrayscaleToWhiteAlpha(std::vector<uint8_t>& pixels)
    {
        for (size_t i = 0; i + 3u < pixels.size(); i += 4u)
        {
            const uint8_t luminance = pixels[i];
            pixels[i + 0u] = 255u;
            pixels[i + 1u] = 255u;
            pixels[i + 2u] = 255u;
            pixels[i + 3u] = luminance;
        }
    }

    std::vector<uint8_t> downsampleColor(const TextureMipData& previous)
    {
        const uint32_t nextWidth = std::max(1u, previous.width / 2u);
        const uint32_t nextHeight = std::max(1u, previous.height / 2u);
        std::vector<uint8_t> next(static_cast<size_t>(nextWidth) * nextHeight * 4u);

        for (uint32_t y = 0; y < nextHeight; ++y)
        {
            for (uint32_t x = 0; x < nextWidth; ++x)
            {
                uint32_t sums[4] = {};
                uint32_t count = 0;
                for (uint32_t oy = 0; oy < 2u; ++oy)
                {
                    const uint32_t sy = std::min(previous.height - 1u, y * 2u + oy);
                    for (uint32_t ox = 0; ox < 2u; ++ox)
                    {
                        const uint32_t sx = std::min(previous.width - 1u, x * 2u + ox);
                        const size_t sourceIndex = (static_cast<size_t>(sy) * previous.width + sx) * 4u;
                        for (uint32_t channel = 0; channel < 4u; ++channel)
                            sums[channel] += previous.bytes[sourceIndex + channel];
                        ++count;
                    }
                }

                const size_t destinationIndex = (static_cast<size_t>(y) * nextWidth + x) * 4u;
                for (uint32_t channel = 0; channel < 4u; ++channel)
                    next[destinationIndex + channel] = averageChannel(sums[channel], count);
            }
        }

        return next;
    }

    std::vector<uint8_t> downsampleNormal(const TextureMipData& previous)
    {
        const uint32_t nextWidth = std::max(1u, previous.width / 2u);
        const uint32_t nextHeight = std::max(1u, previous.height / 2u);
        std::vector<uint8_t> next(static_cast<size_t>(nextWidth) * nextHeight * 4u);

        for (uint32_t y = 0; y < nextHeight; ++y)
        {
            for (uint32_t x = 0; x < nextWidth; ++x)
            {
                glm::vec3 normalSum(0.0f);
                uint32_t alphaSum = 0;
                uint32_t count = 0;
                for (uint32_t oy = 0; oy < 2u; ++oy)
                {
                    const uint32_t sy = std::min(previous.height - 1u, y * 2u + oy);
                    for (uint32_t ox = 0; ox < 2u; ++ox)
                    {
                        const uint32_t sx = std::min(previous.width - 1u, x * 2u + ox);
                        const size_t sourceIndex = (static_cast<size_t>(sy) * previous.width + sx) * 4u;
                        normalSum += glm::vec3(
                            previous.bytes[sourceIndex + 0u] / 127.5f - 1.0f,
                            previous.bytes[sourceIndex + 1u] / 127.5f - 1.0f,
                            previous.bytes[sourceIndex + 2u] / 127.5f - 1.0f);
                        alphaSum += previous.bytes[sourceIndex + 3u];
                        ++count;
                    }
                }

                glm::vec3 normal = glm::length(normalSum) > 0.00001f
                    ? glm::normalize(normalSum)
                    : glm::vec3(0.0f, 0.0f, 1.0f);

                normal = glm::clamp(normal * 0.5f + glm::vec3(0.5f), glm::vec3(0.0f), glm::vec3(1.0f));
                const size_t destinationIndex = (static_cast<size_t>(y) * nextWidth + x) * 4u;
                next[destinationIndex + 0u] = static_cast<uint8_t>(std::round(normal.x * 255.0f));
                next[destinationIndex + 1u] = static_cast<uint8_t>(std::round(normal.y * 255.0f));
                next[destinationIndex + 2u] = static_cast<uint8_t>(std::round(normal.z * 255.0f));
                next[destinationIndex + 3u] = averageChannel(alphaSum, count);
            }
        }

        return next;
    }

    TextureMipData makeMip(uint32_t width, uint32_t height, std::vector<uint8_t> bytes)
    {
        TextureMipData mip;
        mip.width = width;
        mip.height = height;
        mip.rowPitch = width * 4u;
        mip.slicePitch = mip.rowPitch * height;
        mip.bytes = std::move(bytes);
        return mip;
    }
}

TextureColorSpace colorSpaceForTextureCookRole(TextureCookRole role)
{
    switch (role)
    {
        case TextureCookRole::BaseColor:
        case TextureCookRole::Emissive:
        case TextureCookRole::UiColor:
        case TextureCookRole::ParticleColor:
            return TextureColorSpace::SRgb;
        case TextureCookRole::MetallicRoughness:
        case TextureCookRole::Normal:
        case TextureCookRole::AmbientOcclusion:
        case TextureCookRole::FontAtlas:
        case TextureCookRole::Data:
            return TextureColorSpace::Linear;
    }
    return TextureColorSpace::SRgb;
}

const char* textureCookRoleName(TextureCookRole role)
{
    switch (role)
    {
        case TextureCookRole::BaseColor: return "base-color";
        case TextureCookRole::MetallicRoughness: return "metallic-roughness";
        case TextureCookRole::Normal: return "normal";
        case TextureCookRole::AmbientOcclusion: return "ambient-occlusion";
        case TextureCookRole::Emissive: return "emissive";
        case TextureCookRole::UiColor: return "ui";
        case TextureCookRole::FontAtlas: return "font-atlas";
        case TextureCookRole::ParticleColor: return "particle";
        case TextureCookRole::Data: return "data";
    }
    return "base-color";
}

bool parseTextureCookRole(const std::string& value, TextureCookRole& role)
{
    if (value == "base-color" || value == "basecolor" || value == "srgb")
        role = TextureCookRole::BaseColor;
    else if (value == "metallic-roughness" || value == "metallicroughness")
        role = TextureCookRole::MetallicRoughness;
    else if (value == "normal")
        role = TextureCookRole::Normal;
    else if (value == "ambient-occlusion" || value == "ao")
        role = TextureCookRole::AmbientOcclusion;
    else if (value == "emissive")
        role = TextureCookRole::Emissive;
    else if (value == "ui")
        role = TextureCookRole::UiColor;
    else if (value == "font-atlas" || value == "font")
        role = TextureCookRole::FontAtlas;
    else if (value == "particle")
        role = TextureCookRole::ParticleColor;
    else if (value == "data" || value == "linear")
        role = TextureCookRole::Data;
    else
        return false;
    return true;
}

TextureSamplerDesc defaultSamplerForTextureCookRole(TextureCookRole role, bool generateMipmaps)
{
    TextureSamplerDesc sampler;
    sampler.mipmapMode = generateMipmaps ? TextureMipmapMode::Linear : TextureMipmapMode::Nearest;
    if (role == TextureCookRole::FontAtlas)
    {
        sampler.minFilter = TextureFilter::Nearest;
        sampler.magFilter = TextureFilter::Nearest;
        sampler.mipmapMode = TextureMipmapMode::Nearest;
        sampler.addressU = TextureAddressMode::ClampToEdge;
        sampler.addressV = TextureAddressMode::ClampToEdge;
        sampler.addressW = TextureAddressMode::ClampToEdge;
        sampler.maxAnisotropy = 1.0f;
    }
    return sampler;
}

TextureCookResult TextureCooker::cookImportedTexture(const ImportedTexture& imported,
                                                     AssetId assetId,
                                                     const TextureCookerOptions& options,
                                                     const std::filesystem::path& diagnosticSource)
{
    TextureCookResult result;
    const std::filesystem::path sourcePath =
        diagnosticSource.empty() ? imported.sourcePath : diagnosticSource;

    if (!imported.decoded())
    {
        addDiagnostic(
            result,
            AssetDiagnosticSeverity::Error,
            "TEXTURE_DECODE_FAILED",
            "Imported texture does not contain decoded RGBA8 pixels",
            sourcePath);
        return result;
    }

    const uint64_t expectedBytes =
        static_cast<uint64_t>(imported.width) * imported.height * 4u;
    if (imported.width == 0 ||
        imported.height == 0 ||
        imported.rgba8Pixels.size() != expectedBytes)
    {
        addDiagnostic(
            result,
            AssetDiagnosticSeverity::Error,
            "TEXTURE_INVALID_DIMENSIONS",
            "Imported texture dimensions or byte count are invalid",
            sourcePath);
        return result;
    }

    TextureColorSpace colorSpace = colorSpaceForTextureCookRole(options.role);
    result.texture.id = assetId;
    result.texture.debugName = !options.debugName.empty()
        ? options.debugName
        : imported.debugName;
    result.texture.width = imported.width;
    result.texture.height = imported.height;
    result.texture.colorSpace = colorSpace;
    result.texture.format = colorSpace == TextureColorSpace::SRgb
        ? TextureAssetFormat::RGBA8_SRgb
        : TextureAssetFormat::RGBA8_UNorm;
    result.texture.defaultSampler = options.sampler;

    std::vector<uint8_t> basePixels = imported.rgba8Pixels;
    if (isFontAtlasRole(options.role) && imported.sourceChannelCount == 1u)
        convertGrayscaleToWhiteAlpha(basePixels);

    result.texture.mips.push_back(makeMip(imported.width, imported.height, std::move(basePixels)));
    if (options.generateMipmaps)
    {
        while (result.texture.mips.back().width > 1u ||
               result.texture.mips.back().height > 1u)
        {
            const TextureMipData& previous = result.texture.mips.back();
            const uint32_t nextWidth = std::max(1u, previous.width / 2u);
            const uint32_t nextHeight = std::max(1u, previous.height / 2u);
            std::vector<uint8_t> nextBytes = isNormalRole(options.role)
                ? downsampleNormal(previous)
                : downsampleColor(previous);
            result.texture.mips.push_back(makeMip(nextWidth, nextHeight, std::move(nextBytes)));
        }
    }

    result.texture.mipCount = static_cast<uint32_t>(result.texture.mips.size());
    if (!imported.sourcePath.empty())
        result.texture.dependencies.push_back(AssetReference::fromLogicalPath(imported.sourcePath.generic_string()));
    return result;
}
}
