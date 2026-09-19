#include "ModelAssetSerializer.h"
#include "CookedAssetEncoding.h"

namespace gts::rendering
{
    using namespace detail;
    namespace
    {
        bool validateModelAsset(const ModelAssetData& asset, std::string* error)
        {
            if (asset.nodes.size() > MaxCookedModelNodes || asset.meshes.size() > MaxCookedAssetDependencies ||
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

    } // namespace

    bool ModelAssetSerializer::serialize(const ModelAssetData& asset, std::vector<uint8_t>& bytes, std::string* error)
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
            writer.writeU32(node.parentIndex < 0 ? std::numeric_limits<uint32_t>::max()
                                                 : static_cast<uint32_t>(node.parentIndex));
            writer.writeReference(node.mesh);
            writeMat4(writer, node.localTransform);
        }
        for (const AssetReference& mesh : asset.meshes)
            writer.writeReference(mesh);
        for (const AssetReference& material : asset.materials)
            writer.writeReference(material);

        const size_t payloadSize     = writer.position() - payloadBegin;
        const size_t dependencyBegin = writer.position();
        writeDependencies(writer, asset.dependencies);
        const size_t dependencySize = writer.position() - dependencyBegin;
        finalizeHeader(writer, payloadSize, dependencyBegin, dependencySize);
        return true;
    }

    bool ModelAssetSerializer::deserialize(const std::vector<uint8_t>& bytes, ModelAssetData& asset, std::string* error)
    {
        Header header;
        if (!readHeader(bytes, CookedAssetType::Model, ModelAssetFormatVersion, header, error))
            return false;

        ModelAssetData next;
        ByteReader reader(bytes, static_cast<size_t>(header.payloadOffset), static_cast<size_t>(header.payloadSize));

        uint32_t nodeCount     = 0;
        uint32_t meshCount     = 0;
        uint32_t materialCount = 0;
        if (!reader.readU64(next.id) || !reader.readString(next.debugName) || !reader.readU32(nodeCount) ||
            !reader.readU32(meshCount) || !reader.readU32(materialCount))
        {
            setError(error, "Cooked model payload is truncated");
            return false;
        }

        if (nodeCount > MaxCookedModelNodes || meshCount > MaxCookedAssetDependencies ||
            materialCount > MaxCookedAssetDependencies)
        {
            setError(error, "Cooked model payload contains an invalid count");
            return false;
        }

        next.nodes.resize(nodeCount);
        for (ModelNodeAssetData& node : next.nodes)
        {
            uint32_t parent = 0;
            if (!reader.readString(node.name) || !reader.readU32(parent) || !reader.readReference(node.mesh) ||
                !readMat4(reader, node.localTransform))
            {
                setError(error, "Cooked model node data is truncated");
                return false;
            }
            node.parentIndex = parent == std::numeric_limits<uint32_t>::max() ? -1 : static_cast<int32_t>(parent);
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
            ByteReader dependencyReader(bytes,
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

    bool
    ModelAssetSerializer::writeFile(const ModelAssetData& asset, const std::filesystem::path& path, std::string* error)
    {
        std::vector<uint8_t> bytes;
        return serialize(asset, bytes, error) && writeFileBytes(path, bytes, error);
    }

    bool ModelAssetSerializer::readFile(const std::filesystem::path& path, ModelAssetData& asset, std::string* error)
    {
        std::vector<uint8_t> bytes;
        return readFileBytes(path, bytes, error) && deserialize(bytes, asset, error);
    }

} // namespace gts::rendering
