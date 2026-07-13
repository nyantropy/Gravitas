#include "AssetSerializers.h"

#include <cstring>
#include <fstream>
#include <limits>
#include <utility>

namespace gts::rendering
{
namespace
{
    void setError(std::string* error, const std::string& message)
    {
        if (error != nullptr)
            *error = message;
    }

    class ByteWriter
    {
    public:
        explicit ByteWriter(std::vector<uint8_t>& bytes)
            : bytes(bytes)
        {
        }

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
            : bytes(bytes)
            , cursor(begin)
            , end(begin + size)
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
            value = static_cast<uint16_t>(bytes[cursor + 0u])
                | static_cast<uint16_t>(bytes[cursor + 1u] << 8u);
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
            value.assign(
                reinterpret_cast<const char*>(bytes.data() + cursor),
                static_cast<size_t>(size));
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

    private:
        bool canRead(size_t size) const
        {
            return cursor <= end && size <= end - cursor;
        }

        const std::vector<uint8_t>& bytes;
        size_t cursor = 0;
        size_t end = 0;
    };

    struct Header
    {
        uint32_t magic = 0;
        uint16_t version = 0;
        uint16_t assetType = 0;
        uint32_t flags = 0;
        uint32_t headerSize = 0;
        uint64_t assetId = 0;
        uint64_t payloadOffset = 0;
        uint64_t payloadSize = 0;
        uint64_t dependencyTableOffset = 0;
        uint64_t dependencyTableSize = 0;
        uint64_t checksum = 0;
    };

    bool readHeader(const std::vector<uint8_t>& bytes,
                    CookedAssetType expectedType,
                    uint16_t expectedVersion,
                    Header& header,
                    std::string* error)
    {
        if (bytes.size() < CookedAssetHeaderSize)
        {
            setError(error, "Cooked asset file is smaller than the header");
            return false;
        }

        ByteReader reader(bytes, 0, CookedAssetHeaderSize);
        if (!reader.readU32(header.magic) ||
            !reader.readU16(header.version) ||
            !reader.readU16(header.assetType) ||
            !reader.readU32(header.flags) ||
            !reader.readU32(header.headerSize) ||
            !reader.readU64(header.assetId) ||
            !reader.readU64(header.payloadOffset) ||
            !reader.readU64(header.payloadSize) ||
            !reader.readU64(header.dependencyTableOffset) ||
            !reader.readU64(header.dependencyTableSize) ||
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
                 header.dependencyTableSize >
                    bytes.size() - static_cast<size_t>(header.dependencyTableOffset))
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

    bool readFileBytes(const std::filesystem::path& path,
                       std::vector<uint8_t>& bytes,
                       std::string* error)
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

    bool writeFileBytes(const std::filesystem::path& path,
                        const std::vector<uint8_t>& bytes,
                        std::string* error)
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

    void writeHeader(ByteWriter& writer,
                     uint16_t version,
                     CookedAssetType assetType,
                     AssetId assetId)
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

    void finalizeHeader(ByteWriter& writer,
                        size_t payloadSize,
                        size_t dependencyTableOffset,
                        size_t dependencyTableSize)
    {
        writer.patchU64(CookedAssetPayloadSizeOffset, static_cast<uint64_t>(payloadSize));
        writer.patchU64(
            CookedAssetDependencyTableOffsetOffset,
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
        return reader.readFloat(value.x) &&
            reader.readFloat(value.y);
    }

    bool readVec3(ByteReader& reader, glm::vec3& value)
    {
        return reader.readFloat(value.x) &&
            reader.readFloat(value.y) &&
            reader.readFloat(value.z);
    }

    bool readVec4(ByteReader& reader, glm::vec4& value)
    {
        return reader.readFloat(value.x) &&
            reader.readFloat(value.y) &&
            reader.readFloat(value.z) &&
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

    void writeVertex(ByteWriter& writer, const Vertex& vertex)
    {
        writeVec3(writer, vertex.pos);
        writeVec3(writer, vertex.normal);
        writeVec4(writer, vertex.tangent);
        writeVec4(writer, vertex.color);
        writeVec2(writer, vertex.texCoord);
    }

    bool readVertex(ByteReader& reader, Vertex& vertex)
    {
        return readVec3(reader, vertex.pos) &&
            readVec3(reader, vertex.normal) &&
            readVec4(reader, vertex.tangent) &&
            readVec4(reader, vertex.color) &&
            readVec2(reader, vertex.texCoord);
    }

    void writeDependencies(ByteWriter& writer, const std::vector<AssetReference>& dependencies)
    {
        writer.writeU32(static_cast<uint32_t>(dependencies.size()));
        for (const AssetReference& dependency : dependencies)
            writer.writeReference(dependency);
    }

    bool readDependencies(ByteReader& reader,
                          std::vector<AssetReference>& dependencies,
                          std::string* error)
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

    bool validateModelAsset(const ModelAssetData& asset, std::string* error)
    {
        if (asset.nodes.size() > MaxCookedModelNodes ||
            asset.meshes.size() > MaxCookedAssetDependencies ||
            asset.materials.size() > MaxCookedAssetDependencies ||
            asset.dependencies.size() > MaxCookedAssetDependencies)
        {
            setError(error, "Cooked model count exceeds the supported limit");
            return false;
        }

        for (size_t i = 0; i < asset.nodes.size(); ++i)
        {
            const int32_t parent = asset.nodes[i].parentIndex;
            if (parent >= 0 && static_cast<size_t>(parent) >= asset.nodes.size())
            {
                setError(error, "Cooked model contains an invalid node parent index");
                return false;
            }
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

    for (const Vertex& vertex : asset.vertices)
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
    for (Vertex& vertex : next.vertices)
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

bool ModelAssetSerializer::serialize(const ModelAssetData& asset,
                                     std::vector<uint8_t>& bytes,
                                     std::string* error)
{
    if (!validateModelAsset(asset, error))
        return false;

    bytes.clear();
    ByteWriter writer(bytes);
    writeHeader(writer, ModelAssetFormatVersion, CookedAssetType::Model, asset.id);

    const size_t payloadBegin = writer.position();
    writer.writeU64(asset.id);
    writer.writeString(asset.debugName);
    writer.writeU32(static_cast<uint32_t>(asset.nodes.size()));
    writer.writeU32(static_cast<uint32_t>(asset.meshes.size()));
    writer.writeU32(static_cast<uint32_t>(asset.materials.size()));

    for (const ModelNodeAssetData& node : asset.nodes)
    {
        writer.writeString(node.name);
        writer.writeU32(node.parentIndex < 0
            ? std::numeric_limits<uint32_t>::max()
            : static_cast<uint32_t>(node.parentIndex));
        writer.writeReference(node.mesh);
        writeMat4(writer, node.localTransform);
    }
    for (const AssetReference& mesh : asset.meshes)
        writer.writeReference(mesh);
    for (const AssetReference& material : asset.materials)
        writer.writeReference(material);

    const size_t payloadSize = writer.position() - payloadBegin;
    const size_t dependencyBegin = writer.position();
    writeDependencies(writer, asset.dependencies);
    const size_t dependencySize = writer.position() - dependencyBegin;
    finalizeHeader(writer, payloadSize, dependencyBegin, dependencySize);
    return true;
}

bool ModelAssetSerializer::deserialize(const std::vector<uint8_t>& bytes,
                                       ModelAssetData& asset,
                                       std::string* error)
{
    Header header;
    if (!readHeader(bytes, CookedAssetType::Model, ModelAssetFormatVersion, header, error))
        return false;

    ModelAssetData next;
    ByteReader reader(
        bytes,
        static_cast<size_t>(header.payloadOffset),
        static_cast<size_t>(header.payloadSize));

    uint32_t nodeCount = 0;
    uint32_t meshCount = 0;
    uint32_t materialCount = 0;
    if (!reader.readU64(next.id) ||
        !reader.readString(next.debugName) ||
        !reader.readU32(nodeCount) ||
        !reader.readU32(meshCount) ||
        !reader.readU32(materialCount))
    {
        setError(error, "Cooked model payload is truncated");
        return false;
    }

    if (nodeCount > MaxCookedModelNodes ||
        meshCount > MaxCookedAssetDependencies ||
        materialCount > MaxCookedAssetDependencies)
    {
        setError(error, "Cooked model payload contains an invalid count");
        return false;
    }

    next.nodes.resize(nodeCount);
    for (ModelNodeAssetData& node : next.nodes)
    {
        uint32_t parent = 0;
        if (!reader.readString(node.name) ||
            !reader.readU32(parent) ||
            !reader.readReference(node.mesh) ||
            !readMat4(reader, node.localTransform))
        {
            setError(error, "Cooked model node data is truncated");
            return false;
        }
        node.parentIndex = parent == std::numeric_limits<uint32_t>::max()
            ? -1
            : static_cast<int32_t>(parent);
    }

    next.meshes.resize(meshCount);
    for (AssetReference& mesh : next.meshes)
    {
        if (!reader.readReference(mesh))
        {
            setError(error, "Cooked model mesh references are truncated");
            return false;
        }
    }

    next.materials.resize(materialCount);
    for (AssetReference& material : next.materials)
    {
        if (!reader.readReference(material))
        {
            setError(error, "Cooked model material references are truncated");
            return false;
        }
    }

    if (!reader.consumed())
    {
        setError(error, "Cooked model payload has trailing bytes");
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

    if (!validateModelAsset(next, error))
        return false;

    asset = std::move(next);
    return true;
}

bool ModelAssetSerializer::writeFile(const ModelAssetData& asset,
                                     const std::filesystem::path& path,
                                     std::string* error)
{
    std::vector<uint8_t> bytes;
    return serialize(asset, bytes, error) && writeFileBytes(path, bytes, error);
}

bool ModelAssetSerializer::readFile(const std::filesystem::path& path,
                                    ModelAssetData& asset,
                                    std::string* error)
{
    std::vector<uint8_t> bytes;
    return readFileBytes(path, bytes, error) && deserialize(bytes, asset, error);
}
}
