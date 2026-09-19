#pragma once

#include "assets/serialization/AssetSerializers.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>
#include <utility>

namespace gts::rendering::detail
{
    void setError(std::string* error, const std::string& message);

    class ByteWriter
    {
        public:
        explicit ByteWriter(std::vector<uint8_t>& bytes) : bytes(bytes) {}

        size_t position() const
        {
            return bytes.size();
        }

        void writeU8(uint8_t value)
        {
            bytes.push_back(value);
        }

        void writeU16(uint16_t value)
        {
            bytes.push_back(static_cast<uint8_t>(value & 0xFFu));
            bytes.push_back(static_cast<uint8_t>((value >> 8u) & 0xFFu));
        }

        void writeU32(uint32_t value)
        {
            for (uint32_t shift = 0; shift < 32u; shift += 8u)
                bytes.push_back(static_cast<uint8_t>((value >> shift) & 0xFFu));
        }

        void writeU64(uint64_t value)
        {
            for (uint32_t shift = 0; shift < 64u; shift += 8u)
                bytes.push_back(static_cast<uint8_t>((value >> shift) & 0xFFull));
        }

        void writeFloat(float value)
        {
            uint32_t bits = 0;
            std::memcpy(&bits, &value, sizeof(float));
            writeU32(bits);
        }

        void writeString(const std::string& value)
        {
            writeU32(static_cast<uint32_t>(value.size()));
            bytes.insert(bytes.end(), value.begin(), value.end());
        }

        void writeReference(const AssetReference& reference)
        {
            writeU64(reference.id);
            writeString(reference.logicalPath);
        }

        void patchU16(size_t offset, uint16_t value)
        {
            bytes[offset + 0u] = static_cast<uint8_t>(value & 0xFFu);
            bytes[offset + 1u] = static_cast<uint8_t>((value >> 8u) & 0xFFu);
        }

        void patchU32(size_t offset, uint32_t value)
        {
            for (uint32_t i = 0; i < 4u; ++i)
                bytes[offset + i] = static_cast<uint8_t>((value >> (i * 8u)) & 0xFFu);
        }

        void patchU64(size_t offset, uint64_t value)
        {
            for (uint32_t i = 0; i < 8u; ++i)
                bytes[offset + i] = static_cast<uint8_t>((value >> (i * 8u)) & 0xFFull);
        }

        private:
        std::vector<uint8_t>& bytes;
    };

    class ByteReader
    {
        public:
        ByteReader(const std::vector<uint8_t>& bytes, size_t begin, size_t size)
            : bytes(bytes), cursor(begin), end(begin + size)
        {
        }

        bool readU8(uint8_t& value)
        {
            if (!canRead(1u))
                return false;
            value = bytes[cursor++];
            return true;
        }

        bool readU16(uint16_t& value)
        {
            if (!canRead(2u))
                return false;
            value = static_cast<uint16_t>(bytes[cursor + 0u]) | static_cast<uint16_t>(bytes[cursor + 1u] << 8u);
            cursor += 2u;
            return true;
        }

        bool readU32(uint32_t& value)
        {
            if (!canRead(4u))
                return false;
            value = 0;
            for (uint32_t i = 0; i < 4u; ++i)
                value |= static_cast<uint32_t>(bytes[cursor + i]) << (i * 8u);
            cursor += 4u;
            return true;
        }

        bool readU64(uint64_t& value)
        {
            if (!canRead(8u))
                return false;
            value = 0;
            for (uint32_t i = 0; i < 8u; ++i)
                value |= static_cast<uint64_t>(bytes[cursor + i]) << (i * 8u);
            cursor += 8u;
            return true;
        }

        bool readFloat(float& value)
        {
            uint32_t bits = 0;
            if (!readU32(bits))
                return false;
            std::memcpy(&value, &bits, sizeof(float));
            return true;
        }

        bool readString(std::string& value)
        {
            uint32_t size = 0;
            if (!readU32(size) || size > MaxCookedAssetStringBytes || !canRead(size))
                return false;
            value.assign(reinterpret_cast<const char*>(bytes.data() + cursor), static_cast<size_t>(size));
            cursor += size;
            return true;
        }

        bool readReference(AssetReference& reference)
        {
            return readU64(reference.id) && readString(reference.logicalPath);
        }

        bool consumed() const
        {
            return cursor == end;
        }

        size_t position() const
        {
            return cursor;
        }

        private:
        bool canRead(size_t size) const
        {
            return cursor <= end && size <= end - cursor;
        }

        const std::vector<uint8_t>& bytes;
        size_t                      cursor = 0;
        size_t                      end    = 0;
    };

    struct Header
    {
        uint32_t magic                 = 0;
        uint16_t version               = 0;
        uint16_t assetType             = 0;
        uint32_t flags                 = 0;
        uint32_t headerSize            = 0;
        uint64_t assetId               = 0;
        uint64_t payloadOffset         = 0;
        uint64_t payloadSize           = 0;
        uint64_t dependencyTableOffset = 0;
        uint64_t dependencyTableSize   = 0;
        uint64_t checksum              = 0;
    };

    bool readHeader(const std::vector<uint8_t>& bytes,
                    CookedAssetType             expectedType,
                    uint16_t                    expectedVersion,
                    Header&                     header,
                    std::string*                error);

    bool readFileBytes(const std::filesystem::path& path, std::vector<uint8_t>& bytes, std::string* error);

    bool writeFileBytes(const std::filesystem::path& path, const std::vector<uint8_t>& bytes, std::string* error);

    void writeHeader(ByteWriter& writer, uint16_t version, CookedAssetType assetType, AssetId assetId);

    void
    finalizeHeader(ByteWriter& writer, size_t payloadSize, size_t dependencyTableOffset, size_t dependencyTableSize);

    void writeVec2(ByteWriter& writer, const glm::vec2& value);

    void writeVec3(ByteWriter& writer, const glm::vec3& value);

    void writeVec4(ByteWriter& writer, const glm::vec4& value);

    bool readVec2(ByteReader& reader, glm::vec2& value);

    bool readVec3(ByteReader& reader, glm::vec3& value);

    bool readVec4(ByteReader& reader, glm::vec4& value);

    void writeMat4(ByteWriter& writer, const glm::mat4& value);

    bool readMat4(ByteReader& reader, glm::mat4& value);

    void writeVertex(ByteWriter& writer, const GtsStaticVertex& vertex);

    bool readVertex(ByteReader& reader, GtsStaticVertex& vertex);

    void writeDependencies(ByteWriter& writer, const std::vector<AssetReference>& dependencies);

    bool readDependencies(ByteReader& reader, std::vector<AssetReference>& dependencies, std::string* error);

} // namespace gts::rendering::detail
