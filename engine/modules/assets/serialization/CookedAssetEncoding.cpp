#include "assets/serialization/CookedAssetEncoding.h"

namespace gts::rendering::detail
{
    void setError(std::string* error, const std::string& message)
    {
        if (error != nullptr)
            *error = message;
    }

    bool readHeader(const std::vector<uint8_t>& bytes,
                    CookedAssetType             expectedType,
                    uint16_t                    expectedVersion,
                    Header&                     header,
                    std::string*                error)
    {
        if (bytes.size() < CookedAssetHeaderSize)
        {
            setError(error, "Cooked asset file is smaller than the header");
            return false;
        }

        ByteReader reader(bytes, 0, CookedAssetHeaderSize);
        if (!reader.readU32(header.magic) || !reader.readU16(header.version) || !reader.readU16(header.assetType) ||
            !reader.readU32(header.flags) || !reader.readU32(header.headerSize) || !reader.readU64(header.assetId) ||
            !reader.readU64(header.payloadOffset) || !reader.readU64(header.payloadSize) ||
            !reader.readU64(header.dependencyTableOffset) || !reader.readU64(header.dependencyTableSize) ||
            !reader.readU64(header.checksum))
        {
            setError(error, "Cooked asset header is truncated");
            return false;
        }

        if (header.magic != CookedAssetMagic)
        {
            setError(error, "Cooked asset magic number is invalid");
            return false;
        }
        if (header.version != expectedVersion)
        {
            setError(error, "Cooked asset format version is unsupported");
            return false;
        }
        if (header.assetType != static_cast<uint16_t>(expectedType))
        {
            setError(error, "Cooked asset type does not match serializer");
            return false;
        }
        if (header.headerSize != CookedAssetHeaderSize || header.payloadOffset < header.headerSize)
        {
            setError(error, "Cooked asset header size or payload offset is invalid");
            return false;
        }
        if (header.payloadOffset > bytes.size() ||
            header.payloadSize > bytes.size() - static_cast<size_t>(header.payloadOffset))
        {
            setError(error, "Cooked asset payload range is invalid");
            return false;
        }
        if (header.dependencyTableSize == 0)
        {
            if (header.dependencyTableOffset != 0)
            {
                setError(error, "Cooked asset dependency table offset is invalid");
                return false;
            }
        }
        else if (header.dependencyTableOffset > bytes.size() ||
                 header.dependencyTableSize > bytes.size() - static_cast<size_t>(header.dependencyTableOffset))
        {
            setError(error, "Cooked asset dependency table range is invalid");
            return false;
        }

        if (header.dependencyTableSize != 0)
        {
            const uint64_t payloadEnd = header.payloadOffset + header.payloadSize;
            if (header.dependencyTableOffset < payloadEnd)
            {
                setError(error, "Cooked asset dependency table overlaps the payload");
                return false;
            }
        }

        return true;
    }

    bool readFileBytes(const std::filesystem::path& path, std::vector<uint8_t>& bytes, std::string* error)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file)
        {
            setError(error, "Could not open cooked asset file for reading: " + path.string());
            return false;
        }

        const std::streamsize size = file.tellg();
        if (size < 0)
        {
            setError(error, "Could not determine cooked asset file size: " + path.string());
            return false;
        }

        file.seekg(0, std::ios::beg);
        bytes.resize(static_cast<size_t>(size));
        if (!bytes.empty() && !file.read(reinterpret_cast<char*>(bytes.data()), size))
        {
            setError(error, "Could not read cooked asset file: " + path.string());
            return false;
        }
        return true;
    }

    bool writeFileBytes(const std::filesystem::path& path, const std::vector<uint8_t>& bytes, std::string* error)
    {
        if (!path.parent_path().empty())
            std::filesystem::create_directories(path.parent_path());

        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file)
        {
            setError(error, "Could not open cooked asset file for writing: " + path.string());
            return false;
        }

        if (!bytes.empty())
            file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!file.good())
        {
            setError(error, "Could not write cooked asset file: " + path.string());
            return false;
        }
        return true;
    }

    void writeHeader(ByteWriter& writer, uint16_t version, CookedAssetType assetType, AssetId assetId)
    {
        for (uint32_t i = 0; i < CookedAssetHeaderSize; ++i)
            writer.writeU8(0);

        writer.patchU32(CookedAssetMagicOffset, CookedAssetMagic);
        writer.patchU16(CookedAssetVersionOffset, version);
        writer.patchU16(CookedAssetTypeOffset, static_cast<uint16_t>(assetType));
        writer.patchU32(8, 0);
        writer.patchU32(12, CookedAssetHeaderSize);
        writer.patchU64(16, assetId);
        writer.patchU64(24, CookedAssetHeaderSize);
    }

    void
    finalizeHeader(ByteWriter& writer, size_t payloadSize, size_t dependencyTableOffset, size_t dependencyTableSize)
    {
        writer.patchU64(CookedAssetPayloadSizeOffset, static_cast<uint64_t>(payloadSize));
        writer.patchU64(CookedAssetDependencyTableOffsetOffset,
                        dependencyTableSize == 0 ? 0 : static_cast<uint64_t>(dependencyTableOffset));
        writer.patchU64(CookedAssetDependencyTableSizeOffset, static_cast<uint64_t>(dependencyTableSize));
        writer.patchU64(56, 0);
    }

    void writeVec2(ByteWriter& writer, const glm::vec2& value)
    {
        writer.writeFloat(value.x);
        writer.writeFloat(value.y);
    }

    void writeVec3(ByteWriter& writer, const glm::vec3& value)
    {
        writer.writeFloat(value.x);
        writer.writeFloat(value.y);
        writer.writeFloat(value.z);
    }

    void writeVec4(ByteWriter& writer, const glm::vec4& value)
    {
        writer.writeFloat(value.x);
        writer.writeFloat(value.y);
        writer.writeFloat(value.z);
        writer.writeFloat(value.w);
    }

    bool readVec2(ByteReader& reader, glm::vec2& value)
    {
        return reader.readFloat(value.x) && reader.readFloat(value.y);
    }

    bool readVec3(ByteReader& reader, glm::vec3& value)
    {
        return reader.readFloat(value.x) && reader.readFloat(value.y) && reader.readFloat(value.z);
    }

    bool readVec4(ByteReader& reader, glm::vec4& value)
    {
        return reader.readFloat(value.x) && reader.readFloat(value.y) && reader.readFloat(value.z) &&
               reader.readFloat(value.w);
    }

    void writeMat4(ByteWriter& writer, const glm::mat4& value)
    {
        for (uint32_t col = 0; col < 4u; ++col)
        {
            for (uint32_t row = 0; row < 4u; ++row)
                writer.writeFloat(value[static_cast<int>(col)][static_cast<int>(row)]);
        }
    }

    bool readMat4(ByteReader& reader, glm::mat4& value)
    {
        for (uint32_t col = 0; col < 4u; ++col)
        {
            for (uint32_t row = 0; row < 4u; ++row)
            {
                if (!reader.readFloat(value[static_cast<int>(col)][static_cast<int>(row)]))
                    return false;
            }
        }
        return true;
    }

    void writeVertex(ByteWriter& writer, const GtsStaticVertex& vertex)
    {
        writeVec3(writer, vertex.pos);
        writeVec3(writer, vertex.normal);
        writeVec4(writer, vertex.tangent);
        writeVec4(writer, vertex.color);
        writeVec2(writer, vertex.texCoord);
    }

    bool readVertex(ByteReader& reader, GtsStaticVertex& vertex)
    {
        return readVec3(reader, vertex.pos) && readVec3(reader, vertex.normal) && readVec4(reader, vertex.tangent) &&
               readVec4(reader, vertex.color) && readVec2(reader, vertex.texCoord);
    }

    void writeDependencies(ByteWriter& writer, const std::vector<AssetReference>& dependencies)
    {
        writer.writeU32(static_cast<uint32_t>(dependencies.size()));
        for (const AssetReference& dependency : dependencies)
            writer.writeReference(dependency);
    }

    bool readDependencies(ByteReader& reader, std::vector<AssetReference>& dependencies, std::string* error)
    {
        uint32_t count = 0;
        if (!reader.readU32(count) || count > MaxCookedAssetDependencies)
        {
            setError(error, "Cooked asset dependency table count is invalid");
            return false;
        }

        dependencies.clear();
        dependencies.reserve(count);
        for (uint32_t i = 0; i < count; ++i)
        {
            AssetReference reference;
            if (!reader.readReference(reference))
            {
                setError(error, "Cooked asset dependency table is corrupted");
                return false;
            }
            dependencies.push_back(std::move(reference));
        }

        if (!reader.consumed())
        {
            setError(error, "Cooked asset dependency table has trailing bytes");
            return false;
        }
        return true;
    }
} // namespace gts::rendering::detail
