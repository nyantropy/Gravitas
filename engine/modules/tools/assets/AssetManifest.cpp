#include "AssetManifest.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "UiSerialization.h"

namespace gts::tools
{
namespace
{
    bool fail(std::string* error, const std::string& message)
    {
        if (error != nullptr)
            *error = message;
        return false;
    }

    std::string valuePath(const std::string& key)
    {
        return "$." + key;
    }

    std::string lowerCopy(std::string value)
    {
        std::transform(value.begin(),
                       value.end(),
                       value.begin(),
                       [](unsigned char ch)
                       {
                           return static_cast<char>(std::tolower(ch));
                       });
        return value;
    }

    std::string cookedMeshPath(std::string path)
    {
        const std::string::size_type dot = path.find_last_of('.');
        if (dot == std::string::npos)
            return path;

        const std::string extension = lowerCopy(path.substr(dot));
        if (extension == ".obj" || extension == ".gltf" || extension == ".glb")
            path.replace(dot, std::string::npos, ".gmesh");
        return path;
    }

    const UiJsonValue* requireMember(const UiJsonValue& object,
                                     const std::string& key,
                                     std::string* error)
    {
        const UiJsonValue* value = object.find(key);
        if (value == nullptr)
            fail(error, "missing required manifest field '" + valuePath(key) + "'");
        return value;
    }

    bool readFile(const std::string& path, std::string& contents, std::string* error)
    {
        std::ifstream file(path);
        if (!file)
            return fail(error, "could not open asset manifest '" + path + "'");

        std::ostringstream buffer;
        buffer << file.rdbuf();
        contents = buffer.str();
        return true;
    }

    bool readRequiredString(const UiJsonValue& object,
                            const std::string& key,
                            std::string& value,
                            std::string* error)
    {
        const UiJsonValue* json = requireMember(object, key, error);
        if (json == nullptr)
            return false;
        if (!json->isString())
            return fail(error, "manifest field '" + valuePath(key) + "' must be a string");

        value = std::get<std::string>(json->value);
        if (value.empty())
            return fail(error, "manifest field '" + valuePath(key) + "' must not be empty");
        return true;
    }

    bool readOptionalString(const UiJsonValue& object,
                            const std::string& key,
                            std::string& value,
                            std::string* error)
    {
        const UiJsonValue* json = object.find(key);
        if (json == nullptr || json->isNull())
            return true;
        if (!json->isString())
            return fail(error, "manifest field '" + valuePath(key) + "' must be a string");

        value = std::get<std::string>(json->value);
        return true;
    }

    bool readRequiredInt(const UiJsonValue& object,
                         const std::string& key,
                         int& value,
                         std::string* error)
    {
        const UiJsonValue* json = requireMember(object, key, error);
        if (json == nullptr)
            return false;
        if (!json->isNumber())
            return fail(error, "manifest field '" + valuePath(key) + "' must be a number");

        const double number = std::get<double>(json->value);
        if (!std::isfinite(number))
            return fail(error, "manifest field '" + valuePath(key) + "' must be finite");

        value = static_cast<int>(number);
        if (static_cast<double>(value) != number)
            return fail(error, "manifest field '" + valuePath(key) + "' must be an integer");
        return true;
    }

    bool readRequiredBool(const UiJsonValue& object,
                          const std::string& key,
                          bool& value,
                          std::string* error)
    {
        const UiJsonValue* json = requireMember(object, key, error);
        if (json == nullptr)
            return false;
        if (!json->isBool())
            return fail(error, "manifest field '" + valuePath(key) + "' must be a boolean");

        value = std::get<bool>(json->value);
        return true;
    }

    bool readOptionalFloat(const UiJsonValue& object,
                           const std::string& key,
                           float& value,
                           std::string* error)
    {
        const UiJsonValue* json = object.find(key);
        if (json == nullptr || json->isNull())
            return true;
        if (!json->isNumber())
            return fail(error, "manifest field '" + valuePath(key) + "' must be a number");

        const double number = std::get<double>(json->value);
        if (!std::isfinite(number))
            return fail(error, "manifest field '" + valuePath(key) + "' must be finite");

        value = static_cast<float>(number);
        return true;
    }

    bool readVec3Value(const UiJsonValue& json,
                       const std::string& path,
                       glm::vec3& value,
                       std::string* error)
    {
        if (!json.isArray())
            return fail(error, "manifest field '" + path + "' must be a 3-number array");

        const auto& array = std::get<UiJsonValue::Array>(json.value);
        if (array.size() != 3u)
            return fail(error, "manifest field '" + path + "' must contain exactly 3 numbers");

        glm::vec3 parsed;
        for (size_t i = 0; i < array.size(); ++i)
        {
            if (!array[i].isNumber())
                return fail(error, "manifest field '" + path + "' must contain only numbers");
            const double number = std::get<double>(array[i].value);
            if (!std::isfinite(number))
                return fail(error, "manifest field '" + path + "' must contain finite numbers");
            parsed[static_cast<glm::length_t>(i)] = static_cast<float>(number);
        }

        value = parsed;
        return true;
    }

    bool readRequiredVec3(const UiJsonValue& object,
                          const std::string& key,
                          glm::vec3& value,
                          std::string* error)
    {
        const UiJsonValue* json = requireMember(object, key, error);
        if (json == nullptr)
            return false;
        return readVec3Value(*json, valuePath(key), value, error);
    }

    bool readBounds(const UiJsonValue& object, BoundsComponent& bounds, std::string* error)
    {
        const UiJsonValue* json = requireMember(object, "bounds", error);
        if (json == nullptr)
            return false;
        if (!json->isObject())
            return fail(error, "manifest field '$.bounds' must be an object");

        const UiJsonValue* minValue = requireMember(*json, "min", error);
        if (minValue == nullptr)
            return false;
        if (!readVec3Value(*minValue, "$.bounds.min", bounds.min, error))
            return false;

        const UiJsonValue* maxValue = requireMember(*json, "max", error);
        if (maxValue == nullptr)
            return false;
        if (!readVec3Value(*maxValue, "$.bounds.max", bounds.max, error))
            return false;

        if (bounds.min.x > bounds.max.x || bounds.min.y > bounds.max.y || bounds.min.z > bounds.max.z)
            return fail(error, "manifest bounds min must not exceed max");
        return true;
    }

    bool readMaterialMode(const UiJsonValue& object,
                          AssetMaterialMode& materialMode,
                          std::string* error)
    {
        std::string value;
        if (!readRequiredString(object, "materialMode", value, error))
            return false;

        if (value == "unlit_texture_override")
        {
            materialMode = AssetMaterialMode::UnlitTextureOverride;
            return true;
        }
        if (value == "cooked_mesh_materials")
        {
            materialMode = AssetMaterialMode::CookedMeshMaterials;
            return true;
        }

        return fail(error, "unknown asset material mode '" + value + "'");
    }

    std::vector<std::filesystem::path> pathParts(const std::filesystem::path& path)
    {
        std::vector<std::filesystem::path> parts;
        for (const std::filesystem::path& part : path.lexically_normal())
        {
            if (part == ".")
                continue;
            parts.push_back(part);
        }
        return parts;
    }

    std::filesystem::path assetsRelativeDirectoryForManifest(const std::filesystem::path& manifestPath)
    {
        const std::vector<std::filesystem::path> parts =
            pathParts(manifestPath.parent_path());
        for (size_t i = 0; i + 1u < parts.size(); ++i)
        {
            if (parts[i] == "resources" && parts[i + 1u] == "assets")
            {
                std::filesystem::path relative;
                for (size_t j = i + 2u; j < parts.size(); ++j)
                    relative /= parts[j];
                return relative;
            }
        }

        return manifestPath.parent_path().filename();
    }

    bool startsWithParentTraversal(const std::filesystem::path& path)
    {
        for (const std::filesystem::path& part : path.lexically_normal())
        {
            if (part == ".")
                continue;
            return part == "..";
        }
        return false;
    }

    bool resolveManifestRelativeAssetPath(const std::filesystem::path& manifestPath,
                                          const std::string& sourceValue,
                                          bool cookMesh,
                                          std::string& resolved,
                                          std::string* error)
    {
        if (sourceValue.empty())
        {
            resolved.clear();
            return true;
        }

        std::filesystem::path pathValue(sourceValue);
        if (pathValue.is_absolute())
            return fail(error, "manifest asset paths must be relative: '" + sourceValue + "'");

        std::filesystem::path relative =
            (assetsRelativeDirectoryForManifest(manifestPath) / pathValue).lexically_normal();
        if (startsWithParentTraversal(relative))
            return fail(error, "manifest asset path escapes resources/assets: '" + sourceValue + "'");

        resolved = relative.generic_string();
        if (cookMesh)
            resolved = cookedMeshPath(resolved);
        return true;
    }

    bool readPreview(const UiJsonValue& object,
                     AssetPreviewSettings& preview,
                     std::string* error)
    {
        const UiJsonValue* json = object.find("preview");
        if (json == nullptr || json->isNull())
            return true;
        if (!json->isObject())
            return fail(error, "manifest field '$.preview' must be an object");

        if (!readOptionalFloat(*json, "cameraDistance", preview.cameraDistance, error))
            return false;
        if (preview.cameraDistance <= 0.0f)
            return fail(error, "manifest field '$.preview.cameraDistance' must be positive");
        return true;
    }

    bool parseManifest(const std::filesystem::path& path,
                       const UiJsonValue& root,
                       AssetManifest& manifest,
                       std::string* error)
    {
        if (!root.isObject())
            return fail(error, "asset manifest root must be an object");

        AssetManifest parsed;
        std::string modelValue;
        std::string fallbackTextureValue;
        std::string sourceModelValue;
        if (!readRequiredInt(root, "version", parsed.version, error)
            || !readRequiredString(root, "id", parsed.id, error)
            || !readRequiredString(root, "displayName", parsed.displayName, error)
            || !readRequiredString(root, "model", modelValue, error)
            || !readOptionalString(root, "fallbackTexture", fallbackTextureValue, error)
            || !readOptionalString(root, "sourceModel", sourceModelValue, error)
            || !readMaterialMode(root, parsed.materialMode, error)
            || !readRequiredVec3(root, "scale", parsed.scale, error)
            || !readBounds(root, parsed.bounds, error)
            || !readRequiredBool(root, "baseAtGround", parsed.baseAtGround, error)
            || !readPreview(root, parsed.preview, error))
        {
            return false;
        }

        if (parsed.version != 1)
            return fail(error, "unsupported asset manifest version " + std::to_string(parsed.version));
        if (!resolveManifestRelativeAssetPath(path, modelValue, true, parsed.modelPath, error)
            || !resolveManifestRelativeAssetPath(
                path, fallbackTextureValue, false, parsed.fallbackTexturePath, error)
            || !resolveManifestRelativeAssetPath(
                path, sourceModelValue, false, parsed.sourceModelPath, error))
        {
            return false;
        }
        if (parsed.modelPath.empty())
            return fail(error, "manifest field '$.model' must not be empty");

        manifest = std::move(parsed);
        return true;
    }
}

const char* assetMaterialModeName(AssetMaterialMode mode)
{
    switch (mode)
    {
        case AssetMaterialMode::CookedMeshMaterials:
            return "cooked_mesh_materials";
        case AssetMaterialMode::UnlitTextureOverride:
        default:
            return "unlit_texture_override";
    }
}

bool loadAssetManifest(const std::string& path,
                       AssetManifest& manifest,
                       std::string* error)
{
    std::string contents;
    if (!readFile(path, contents, error))
        return false;

    UiJsonValue root;
    std::string parseError;
    if (!parseUiJson(contents, root, &parseError))
        return fail(error, "invalid JSON in asset manifest '" + path + "': " + parseError);

    std::string manifestError;
    if (!parseManifest(path, root, manifest, &manifestError))
        return fail(error, "invalid asset manifest '" + path + "': " + manifestError);

    return true;
}

AssetManifest loadAssetManifestOrThrow(const std::string& path)
{
    AssetManifest manifest;
    std::string error;
    if (!loadAssetManifest(path, manifest, &error))
        throw std::runtime_error(error);
    return manifest;
}
}
