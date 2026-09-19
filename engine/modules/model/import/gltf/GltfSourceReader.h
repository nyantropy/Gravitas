#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "GtsJsonValue.h"
#include "model/domain/model/GtsModelDiagnostic.h"

namespace gts::gltf
{
    // Private decoder failure, caught at the importer boundary.
    struct DecodeError
    {
        GtsModelDiagnostic diagnostic;
    };

    [[noreturn]] void          fail(std::string code, std::string message, std::string location);
    void                       object(const GtsJsonValue& value, const std::string& location);
    const GtsJsonValue::Array& array(const GtsJsonValue* value, const std::string& location);
    uint32_t                   integer(const GtsJsonValue& value, const std::string& location);
    uint32_t                   uintField(const GtsJsonValue&     value,
                                         const char*             key,
                                         const std::string&      location,
                                         std::optional<uint32_t> fallback = std::nullopt);
    std::string                stringField(const GtsJsonValue& value,
                                           const char*         key,
                                           const std::string&  location,
                                           const std::string&  fallback = {});
    bool  boolField(const GtsJsonValue& value, const char* key, const std::string& location, bool fallback = false);
    float floatField(const GtsJsonValue& value, const char* key, const std::string& location, float fallback);
    std::vector<float>    vectorValue(const GtsJsonValue& value, size_t count, const std::string& location);
    std::filesystem::path externalPath(const std::filesystem::path& directory, const std::string& uri);
    std::vector<uint8_t>  dataUri(const std::string& uri, std::string& mime, const std::string& location);

    struct BufferView
    {
        uint32_t buffer = 0;
        size_t   offset = 0, length = 0, stride = 0;
    };

    struct Accessor
    {
        uint32_t    view = 0, componentType = 0;
        size_t      offset = 0, count = 0, components = 0, stride = 0;
        std::string type;
        bool        normalized = false;
    };

    struct SourceDocument
    {
        GtsJsonValue                      root;
        std::filesystem::path             directory;
        std::vector<std::vector<uint8_t>> buffers;
        std::vector<BufferView>           views;
        std::vector<Accessor>             accessors;

        const Accessor&      accessor(uint32_t index) const;
        uint32_t             unsignedComponent(const Accessor& accessor, size_t element, size_t component = 0) const;
        float                floatComponent(const Accessor& accessor, size_t element, size_t component) const;
        std::vector<uint8_t> viewBytes(uint32_t view) const;
    };
    
    SourceDocument readSource(const std::filesystem::path& path, std::vector<GtsModelDiagnostic>& diagnostics);
} // namespace gts::gltf
