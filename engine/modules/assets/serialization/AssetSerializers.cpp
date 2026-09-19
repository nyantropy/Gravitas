#include "CookedAssetEncoding.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>
#include <utility>

namespace gts::rendering
{
using namespace detail;
namespace
{
    bool validateMeshAsset(const MeshAssetData& asset, std::string* error)
    {
        if (asset.vertices.size() > MaxCookedMeshVertices ||
            asset.indices.size() > MaxCookedMeshIndices ||
            asset.submeshes.size() > MaxCookedMeshSubmeshes ||
            asset.dependencies.size() > MaxCookedAssetDependencies)
        {
            setError(error, "Cooked mesh count exceeds the supported limit");
            return false;
        }

        for (uint32_t index : asset.indices)
        {
            if (index >= asset.vertices.size())
            {
                setError(error, "Cooked mesh contains an index outside the vertex range");
                return false;
            }
        }

        for (const SubmeshAssetData& submesh : asset.submeshes)
        {
            if (submesh.firstIndex > asset.indices.size() ||
                submesh.indexCount > asset.indices.size() - submesh.firstIndex)
            {
                setError(error, "Cooked mesh contains an invalid submesh range");
                return false;
            }
        }

        return true;
    }

    bool validateMaterialAsset(const MaterialAssetData& asset, std::string* error)
    {
        if (asset.dependencies.size() > MaxCookedAssetDependencies)
        {
            setError(error, "Cooked material dependency count exceeds the supported limit");
            return false;
        }
        return true;
    }

    uint32_t bytesPerPixel(TextureAssetFormat format)
    {
        switch (format)
        {
            case TextureAssetFormat::RGBA8_UNorm:
            case TextureAssetFormat::RGBA8_SRgb:
                return 4u;
            default:
                return 0u;
        }
    }

    bool supportedTextureFormat(TextureAssetFormat format)
    {
        return format == TextureAssetFormat::RGBA8_UNorm ||
            format == TextureAssetFormat::RGBA8_SRgb;
    }

    bool validTextureEnumValues(const TextureAssetData& asset)
    {
        if (!supportedTextureFormat(asset.format))
            return false;
        if (asset.colorSpace != TextureColorSpace::SRgb &&
            asset.colorSpace != TextureColorSpace::Linear)
            return false;
        if ((asset.format == TextureAssetFormat::RGBA8_SRgb) !=
            (asset.colorSpace == TextureColorSpace::SRgb))
            return false;
        if (asset.defaultSampler.minFilter != TextureFilter::Nearest &&
            asset.defaultSampler.minFilter != TextureFilter::Linear)
            return false;
        if (asset.defaultSampler.magFilter != TextureFilter::Nearest &&
            asset.defaultSampler.magFilter != TextureFilter::Linear)
            return false;
        if (asset.defaultSampler.mipmapMode != TextureMipmapMode::Nearest &&
            asset.defaultSampler.mipmapMode != TextureMipmapMode::Linear)
            return false;
        if (asset.defaultSampler.addressU != TextureAddressMode::Repeat &&
            asset.defaultSampler.addressU != TextureAddressMode::ClampToEdge)
            return false;
        if (asset.defaultSampler.addressV != TextureAddressMode::Repeat &&
            asset.defaultSampler.addressV != TextureAddressMode::ClampToEdge)
            return false;
        if (asset.defaultSampler.addressW != TextureAddressMode::Repeat &&
            asset.defaultSampler.addressW != TextureAddressMode::ClampToEdge)
            return false;
        return asset.defaultSampler.maxAnisotropy >= 1.0f;
    }

    bool validateTextureAsset(const TextureAssetData& asset, std::string* error)
    {
        if (!validTextureEnumValues(asset))
        {
            setError(error, "Cooked texture contains unsupported format, color space, or sampler values");
            return false;
        }
        if (asset.width == 0 || asset.height == 0 ||
            asset.width > MaxCookedTextureDimension ||
            asset.height > MaxCookedTextureDimension)
        {
            setError(error, "Cooked texture dimensions are invalid");
            return false;
        }
        if (asset.mipCount == 0 ||
            asset.mipCount != asset.mips.size() ||
            asset.mipCount > MaxCookedTextureMipLevels)
        {
            setError(error, "Cooked texture mip count is invalid");
            return false;
        }
        if (asset.dependencies.size() > MaxCookedAssetDependencies)
        {
            setError(error, "Cooked texture dependency count exceeds the supported limit");
            return false;
        }

        const uint32_t bpp = bytesPerPixel(asset.format);
        if (bpp == 0)
        {
            setError(error, "Cooked texture format is not supported by this runtime");
            return false;
        }

        uint64_t totalBytes = 0;
        uint32_t expectedWidth = asset.width;
        uint32_t expectedHeight = asset.height;
        for (size_t mipIndex = 0; mipIndex < asset.mips.size(); ++mipIndex)
        {
            const TextureMipData& mip = asset.mips[mipIndex];
            if (mip.width != expectedWidth || mip.height != expectedHeight)
            {
                setError(error, "Cooked texture mip dimensions are invalid");
                return false;
            }

            const uint64_t expectedRowPitch =
                static_cast<uint64_t>(mip.width) * bpp;
            const uint64_t expectedSlicePitch =
                expectedRowPitch * static_cast<uint64_t>(mip.height);
            if (expectedRowPitch > std::numeric_limits<uint32_t>::max() ||
                expectedSlicePitch > std::numeric_limits<uint32_t>::max() ||
                mip.rowPitch != expectedRowPitch ||
                mip.slicePitch != expectedSlicePitch ||
                mip.bytes.size() != expectedSlicePitch)
            {
                setError(error, "Cooked texture mip pitch or payload size is invalid");
                return false;
            }

            totalBytes += mip.bytes.size();
            if (totalBytes > MaxCookedTextureBytes)
            {
                setError(error, "Cooked texture payload exceeds the supported limit");
                return false;
            }

            expectedWidth = std::max(1u, expectedWidth / 2u);
            expectedHeight = std::max(1u, expectedHeight / 2u);
        }

        return true;
    }
}

bool MeshAssetSerializer::serialize(const MeshAssetData& asset,
                                    std::vector<uint8_t>& bytes,
                                    std::string* error)
{
    if (!validateMeshAsset(asset, error))
        return false;

    bytes.clear();
    ByteWriter writer(bytes);
    writeHeader(writer, MeshAssetFormatVersion, CookedAssetType::Mesh, asset.id);

    const size_t payloadBegin = writer.position();
    writer.writeU64(asset.id);
    writer.writeString(asset.debugName);
    writer.writeU32(static_cast<uint32_t>(asset.attributes));
    uint32_t flags = 0;
    flags |= asset.generatedNormals ? 1u << 0u : 0u;
    flags |= asset.generatedTangents ? 1u << 1u : 0u;
    flags |= asset.bounds.valid ? 1u << 2u : 0u;
    writer.writeU32(flags);
    writeVec3(writer, asset.bounds.min);
    writeVec3(writer, asset.bounds.max);
    writer.writeU32(static_cast<uint32_t>(asset.vertices.size()));
    writer.writeU32(static_cast<uint32_t>(asset.indices.size()));
    writer.writeU32(static_cast<uint32_t>(asset.submeshes.size()));

    for (const GtsStaticVertex& vertex : asset.vertices)
        writeVertex(writer, vertex);
    for (uint32_t index : asset.indices)
        writer.writeU32(index);
    for (const SubmeshAssetData& submesh : asset.submeshes)
    {
        writer.writeU32(submesh.firstIndex);
        writer.writeU32(submesh.indexCount);
        writer.writeReference(submesh.material);
        writer.writeString(submesh.debugName);
    }

    const size_t payloadSize = writer.position() - payloadBegin;
    const size_t dependencyBegin = writer.position();
    writeDependencies(writer, asset.dependencies);
    const size_t dependencySize = writer.position() - dependencyBegin;
    finalizeHeader(writer, payloadSize, dependencyBegin, dependencySize);
    return true;
}

bool MeshAssetSerializer::deserialize(const std::vector<uint8_t>& bytes,
                                      MeshAssetData& asset,
                                      std::string* error)
{
    Header header;
    if (!readHeader(bytes, CookedAssetType::Mesh, MeshAssetFormatVersion, header, error))
        return false;

    MeshAssetData next;
    ByteReader reader(
        bytes,
        static_cast<size_t>(header.payloadOffset),
        static_cast<size_t>(header.payloadSize));

    uint32_t attributes = 0;
    uint32_t flags = 0;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    uint32_t submeshCount = 0;
    if (!reader.readU64(next.id) ||
        !reader.readString(next.debugName) ||
        !reader.readU32(attributes) ||
        !reader.readU32(flags) ||
        !readVec3(reader, next.bounds.min) ||
        !readVec3(reader, next.bounds.max) ||
        !reader.readU32(vertexCount) ||
        !reader.readU32(indexCount) ||
        !reader.readU32(submeshCount))
    {
        setError(error, "Cooked mesh payload is truncated");
        return false;
    }

    if (vertexCount > MaxCookedMeshVertices ||
        indexCount > MaxCookedMeshIndices ||
        submeshCount > MaxCookedMeshSubmeshes)
    {
        setError(error, "Cooked mesh payload contains an invalid count");
        return false;
    }

    next.attributes = static_cast<VertexAttributeFlags>(attributes);
    next.generatedNormals = (flags & (1u << 0u)) != 0u;
    next.generatedTangents = (flags & (1u << 1u)) != 0u;
    next.bounds.valid = (flags & (1u << 2u)) != 0u;

    next.vertices.resize(vertexCount);
    for (GtsStaticVertex& vertex : next.vertices)
    {
        if (!readVertex(reader, vertex))
        {
            setError(error, "Cooked mesh vertex data is truncated");
            return false;
        }
    }

    next.indices.resize(indexCount);
    for (uint32_t& index : next.indices)
    {
        if (!reader.readU32(index))
        {
            setError(error, "Cooked mesh index data is truncated");
            return false;
        }
    }

    next.submeshes.resize(submeshCount);
    for (SubmeshAssetData& submesh : next.submeshes)
    {
        if (!reader.readU32(submesh.firstIndex) ||
            !reader.readU32(submesh.indexCount) ||
            !reader.readReference(submesh.material) ||
            !reader.readString(submesh.debugName))
        {
            setError(error, "Cooked mesh submesh data is truncated");
            return false;
        }
    }

    if (!reader.consumed())
    {
        setError(error, "Cooked mesh payload has trailing bytes");
        return false;
    }

    if (header.dependencyTableSize == 0)
    {
        next.dependencies.clear();
    }
    else
    {
        ByteReader dependencyReader(
            bytes,
            static_cast<size_t>(header.dependencyTableOffset),
            static_cast<size_t>(header.dependencyTableSize));
        if (!readDependencies(dependencyReader, next.dependencies, error))
            return false;
    }

    if (!validateMeshAsset(next, error))
        return false;

    asset = std::move(next);
    return true;
}

bool MeshAssetSerializer::writeFile(const MeshAssetData& asset,
                                    const std::filesystem::path& path,
                                    std::string* error)
{
    std::vector<uint8_t> bytes;
    return serialize(asset, bytes, error) && writeFileBytes(path, bytes, error);
}

bool MeshAssetSerializer::readFile(const std::filesystem::path& path,
                                   MeshAssetData& asset,
                                   std::string* error)
{
    std::vector<uint8_t> bytes;
    return readFileBytes(path, bytes, error) && deserialize(bytes, asset, error);
}

bool MaterialAssetSerializer::serialize(const MaterialAssetData& asset,
                                        std::vector<uint8_t>& bytes,
                                        std::string* error)
{
    if (!validateMaterialAsset(asset, error))
        return false;

    bytes.clear();
    ByteWriter writer(bytes);
    writeHeader(writer, MaterialAssetFormatVersion, CookedAssetType::Material, asset.id);

    const size_t payloadBegin = writer.position();
    writer.writeU64(asset.id);
    writer.writeString(asset.debugName);
    writer.writeU32(static_cast<uint32_t>(asset.shaderFamily));
    writeVec4(writer, asset.baseColor);
    writer.writeFloat(asset.metallic);
    writer.writeFloat(asset.roughness);
    writer.writeFloat(asset.normalScale);
    writer.writeFloat(asset.ambientOcclusionStrength);
    writeVec3(writer, asset.emissiveFactor);
    writer.writeFloat(asset.emissiveStrength);
    writer.writeU32(static_cast<uint32_t>(asset.renderState.alphaMode));
    writer.writeFloat(asset.renderState.alphaCutoff);
    writer.writeU8(asset.renderState.doubleSided ? 1u : 0u);
    writer.writeU8(asset.renderState.depthWrite ? 1u : 0u);
    writer.writeU32(static_cast<uint32_t>(asset.renderState.blendMode));
    writer.writeU8(asset.vertexColorOnly ? 1u : 0u);
    writer.writeReference(asset.baseColorTexture);
    writer.writeReference(asset.metallicRoughnessTexture);
    writer.writeReference(asset.normalTexture);
    writer.writeReference(asset.ambientOcclusionTexture);
    writer.writeReference(asset.emissiveTexture);

    const size_t payloadSize = writer.position() - payloadBegin;
    const size_t dependencyBegin = writer.position();
    writeDependencies(writer, asset.dependencies);
    const size_t dependencySize = writer.position() - dependencyBegin;
    finalizeHeader(writer, payloadSize, dependencyBegin, dependencySize);
    return true;
}

bool MaterialAssetSerializer::deserialize(const std::vector<uint8_t>& bytes,
                                          MaterialAssetData& asset,
                                          std::string* error)
{
    Header header;
    if (!readHeader(bytes, CookedAssetType::Material, MaterialAssetFormatVersion, header, error))
        return false;

    MaterialAssetData next;
    ByteReader reader(
        bytes,
        static_cast<size_t>(header.payloadOffset),
        static_cast<size_t>(header.payloadSize));

    uint32_t shaderFamily = 0;
    uint32_t alphaMode = 0;
    uint32_t blendMode = 0;
    uint8_t doubleSided = 0;
    uint8_t depthWrite = 0;
    uint8_t vertexColorOnly = 0;
    if (!reader.readU64(next.id) ||
        !reader.readString(next.debugName) ||
        !reader.readU32(shaderFamily) ||
        !readVec4(reader, next.baseColor) ||
        !reader.readFloat(next.metallic) ||
        !reader.readFloat(next.roughness) ||
        !reader.readFloat(next.normalScale) ||
        !reader.readFloat(next.ambientOcclusionStrength) ||
        !readVec3(reader, next.emissiveFactor) ||
        !reader.readFloat(next.emissiveStrength) ||
        !reader.readU32(alphaMode) ||
        !reader.readFloat(next.renderState.alphaCutoff) ||
        !reader.readU8(doubleSided) ||
        !reader.readU8(depthWrite) ||
        !reader.readU32(blendMode) ||
        !reader.readU8(vertexColorOnly) ||
        !reader.readReference(next.baseColorTexture) ||
        !reader.readReference(next.metallicRoughnessTexture) ||
        !reader.readReference(next.normalTexture) ||
        !reader.readReference(next.ambientOcclusionTexture) ||
        !reader.readReference(next.emissiveTexture))
    {
        setError(error, "Cooked material payload is truncated");
        return false;
    }

    if (shaderFamily > static_cast<uint32_t>(MaterialShaderFamily::StandardSurface) ||
        alphaMode > static_cast<uint32_t>(MaterialAlphaMode::Blend) ||
        blendMode > static_cast<uint32_t>(MaterialBlendMode::Additive) ||
        doubleSided > 1u ||
        depthWrite > 1u ||
        vertexColorOnly > 1u)
    {
        setError(error, "Cooked material contains invalid enum or boolean values");
        return false;
    }

    next.shaderFamily = static_cast<MaterialShaderFamily>(shaderFamily);
    next.renderState.alphaMode = static_cast<MaterialAlphaMode>(alphaMode);
    next.renderState.doubleSided = doubleSided != 0u;
    next.renderState.depthWrite = depthWrite != 0u;
    next.renderState.blendMode = static_cast<MaterialBlendMode>(blendMode);
    next.vertexColorOnly = vertexColorOnly != 0u;

    if (!reader.consumed())
    {
        setError(error, "Cooked material payload has trailing bytes");
        return false;
    }

    if (header.dependencyTableSize == 0)
    {
        next.dependencies.clear();
    }
    else
    {
        ByteReader dependencyReader(
            bytes,
            static_cast<size_t>(header.dependencyTableOffset),
            static_cast<size_t>(header.dependencyTableSize));
        if (!readDependencies(dependencyReader, next.dependencies, error))
            return false;
    }

    if (!validateMaterialAsset(next, error))
        return false;

    asset = std::move(next);
    return true;
}

bool MaterialAssetSerializer::writeFile(const MaterialAssetData& asset,
                                        const std::filesystem::path& path,
                                        std::string* error)
{
    std::vector<uint8_t> bytes;
    return serialize(asset, bytes, error) && writeFileBytes(path, bytes, error);
}

bool MaterialAssetSerializer::readFile(const std::filesystem::path& path,
                                       MaterialAssetData& asset,
                                       std::string* error)
{
    std::vector<uint8_t> bytes;
    return readFileBytes(path, bytes, error) && deserialize(bytes, asset, error);
}

bool TextureAssetSerializer::serialize(const TextureAssetData& asset,
                                       std::vector<uint8_t>& bytes,
                                       std::string* error)
{
    if (!validateTextureAsset(asset, error))
        return false;

    bytes.clear();
    ByteWriter writer(bytes);
    writeHeader(writer, TextureAssetFormatVersion, CookedAssetType::Texture, asset.id);

    const size_t payloadBegin = writer.position();
    writer.writeU64(asset.id);
    writer.writeString(asset.debugName);
    writer.writeU32(asset.width);
    writer.writeU32(asset.height);
    writer.writeU32(asset.mipCount);
    writer.writeU32(static_cast<uint32_t>(asset.format));
    writer.writeU32(static_cast<uint32_t>(asset.colorSpace));
    writer.writeU32(static_cast<uint32_t>(asset.defaultSampler.minFilter));
    writer.writeU32(static_cast<uint32_t>(asset.defaultSampler.magFilter));
    writer.writeU32(static_cast<uint32_t>(asset.defaultSampler.mipmapMode));
    writer.writeU32(static_cast<uint32_t>(asset.defaultSampler.addressU));
    writer.writeU32(static_cast<uint32_t>(asset.defaultSampler.addressV));
    writer.writeU32(static_cast<uint32_t>(asset.defaultSampler.addressW));
    writer.writeFloat(asset.defaultSampler.maxAnisotropy);

    std::vector<size_t> mipOffsetPatchOffsets;
    mipOffsetPatchOffsets.reserve(asset.mips.size());
    for (const TextureMipData& mip : asset.mips)
    {
        writer.writeU32(mip.width);
        writer.writeU32(mip.height);
        writer.writeU32(mip.rowPitch);
        writer.writeU32(mip.slicePitch);
        mipOffsetPatchOffsets.push_back(writer.position());
        writer.writeU64(0);
        writer.writeU64(static_cast<uint64_t>(mip.bytes.size()));
    }

    for (size_t mipIndex = 0; mipIndex < asset.mips.size(); ++mipIndex)
    {
        writer.patchU64(mipOffsetPatchOffsets[mipIndex], static_cast<uint64_t>(writer.position()));
        const std::vector<uint8_t>& mipBytes = asset.mips[mipIndex].bytes;
        bytes.insert(bytes.end(), mipBytes.begin(), mipBytes.end());
    }

    const size_t payloadSize = writer.position() - payloadBegin;
    const size_t dependencyBegin = writer.position();
    writeDependencies(writer, asset.dependencies);
    const size_t dependencySize = writer.position() - dependencyBegin;
    finalizeHeader(writer, payloadSize, dependencyBegin, dependencySize);
    return true;
}

bool TextureAssetSerializer::deserialize(const std::vector<uint8_t>& bytes,
                                         TextureAssetData& asset,
                                         std::string* error)
{
    Header header;
    if (!readHeader(bytes, CookedAssetType::Texture, TextureAssetFormatVersion, header, error))
        return false;

    TextureAssetData next;
    ByteReader reader(
        bytes,
        static_cast<size_t>(header.payloadOffset),
        static_cast<size_t>(header.payloadSize));

    uint32_t format = 0;
    uint32_t colorSpace = 0;
    uint32_t minFilter = 0;
    uint32_t magFilter = 0;
    uint32_t mipmapMode = 0;
    uint32_t addressU = 0;
    uint32_t addressV = 0;
    uint32_t addressW = 0;
    if (!reader.readU64(next.id) ||
        !reader.readString(next.debugName) ||
        !reader.readU32(next.width) ||
        !reader.readU32(next.height) ||
        !reader.readU32(next.mipCount) ||
        !reader.readU32(format) ||
        !reader.readU32(colorSpace) ||
        !reader.readU32(minFilter) ||
        !reader.readU32(magFilter) ||
        !reader.readU32(mipmapMode) ||
        !reader.readU32(addressU) ||
        !reader.readU32(addressV) ||
        !reader.readU32(addressW) ||
        !reader.readFloat(next.defaultSampler.maxAnisotropy))
    {
        setError(error, "Cooked texture payload is truncated");
        return false;
    }

    if (next.mipCount == 0 || next.mipCount > MaxCookedTextureMipLevels)
    {
        setError(error, "Cooked texture payload contains an invalid mip count");
        return false;
    }

    next.format = static_cast<TextureAssetFormat>(format);
    next.colorSpace = static_cast<TextureColorSpace>(colorSpace);
    next.defaultSampler.minFilter = static_cast<TextureFilter>(minFilter);
    next.defaultSampler.magFilter = static_cast<TextureFilter>(magFilter);
    next.defaultSampler.mipmapMode = static_cast<TextureMipmapMode>(mipmapMode);
    next.defaultSampler.addressU = static_cast<TextureAddressMode>(addressU);
    next.defaultSampler.addressV = static_cast<TextureAddressMode>(addressV);
    next.defaultSampler.addressW = static_cast<TextureAddressMode>(addressW);

    struct MipRange
    {
        uint64_t offset = 0;
        uint64_t size = 0;
    };
    std::vector<MipRange> ranges;
    ranges.reserve(next.mipCount);

    next.mips.resize(next.mipCount);
    for (TextureMipData& mip : next.mips)
    {
        uint64_t mipOffset = 0;
        uint64_t mipSize = 0;
        if (!reader.readU32(mip.width) ||
            !reader.readU32(mip.height) ||
            !reader.readU32(mip.rowPitch) ||
            !reader.readU32(mip.slicePitch) ||
            !reader.readU64(mipOffset) ||
            !reader.readU64(mipSize))
        {
            setError(error, "Cooked texture mip descriptor is truncated");
            return false;
        }

        const uint64_t payloadEnd = header.payloadOffset + header.payloadSize;
        if (mipOffset < header.payloadOffset ||
            mipOffset > payloadEnd ||
            mipSize > payloadEnd - mipOffset ||
            mipSize > MaxCookedTextureBytes)
        {
            setError(error, "Cooked texture mip payload range is invalid");
            return false;
        }

        mip.bytes.assign(bytes.begin() + static_cast<std::ptrdiff_t>(mipOffset),
                         bytes.begin() + static_cast<std::ptrdiff_t>(mipOffset + mipSize));
        ranges.push_back({mipOffset, mipSize});
    }

    const uint64_t minimumMipPayloadOffset = static_cast<uint64_t>(reader.position());
    std::sort(ranges.begin(), ranges.end(), [](const MipRange& lhs, const MipRange& rhs)
    {
        return lhs.offset < rhs.offset;
    });
    if (!ranges.empty() && ranges.front().offset < minimumMipPayloadOffset)
    {
        setError(error, "Cooked texture mip payload overlaps texture descriptors");
        return false;
    }
    for (size_t i = 1; i < ranges.size(); ++i)
    {
        if (ranges[i].offset < ranges[i - 1u].offset + ranges[i - 1u].size)
        {
            setError(error, "Cooked texture mip payload ranges overlap");
            return false;
        }
    }

    if (header.dependencyTableSize == 0)
    {
        next.dependencies.clear();
    }
    else
    {
        ByteReader dependencyReader(
            bytes,
            static_cast<size_t>(header.dependencyTableOffset),
            static_cast<size_t>(header.dependencyTableSize));
        if (!readDependencies(dependencyReader, next.dependencies, error))
            return false;
    }

    if (!validateTextureAsset(next, error))
        return false;

    asset = std::move(next);
    return true;
}

bool TextureAssetSerializer::writeFile(const TextureAssetData& asset,
                                       const std::filesystem::path& path,
                                       std::string* error)
{
    std::vector<uint8_t> bytes;
    return serialize(asset, bytes, error) && writeFileBytes(path, bytes, error);
}

bool TextureAssetSerializer::readFile(const std::filesystem::path& path,
                                      TextureAssetData& asset,
                                      std::string* error)
{
    std::vector<uint8_t> bytes;
    return readFileBytes(path, bytes, error) && deserialize(bytes, asset, error);
}
}
