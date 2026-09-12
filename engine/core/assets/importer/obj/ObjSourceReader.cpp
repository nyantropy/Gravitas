#include "ObjSourceReader.h"

#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <sstream>
#include <string_view>
#include <utility>

namespace gts::obj
{
    namespace
    {
        bool sourceError(std::vector<GtsModelDiagnostic>& diagnostics,
                         const std::string&               location,
                         const std::string&               code,
                         const std::string&               message)
        {
            diagnostics.push_back({GtsModelDiagnosticSeverity::Error, code, message, location});
            return false;
        }

        bool validIndex(std::string_view token, size_t count)
        {
            if (token.starts_with('+'))
                token.remove_prefix(1);
            int64_t    index  = 0;
            const auto parsed = std::from_chars(token.data(), token.data() + token.size(), index);
            if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size() || index == 0)
            {
                return false;
            }
            const int64_t limit = static_cast<int64_t>(count);
            return index >= -limit && index <= limit;
        }

        bool checkCorner(const std::string&               token,
                         const std::array<size_t, 3>&     counts,
                         const std::string&               location,
                         std::vector<GtsModelDiagnostic>& diagnostics)
        {
            const std::array<const char*, 3> codes = {
                "OBJ_POSITION_INDEX_OUT_OF_RANGE", "OBJ_TEXCOORD_INDEX_OUT_OF_RANGE", "OBJ_NORMAL_INDEX_OUT_OF_RANGE"};
            size_t start = 0;
            for (size_t component = 0; component < 3; ++component)
            {
                const size_t           end = token.find('/', start);
                const std::string_view part(token.data() + start,
                                            (end == std::string::npos ? token.size() : end) - start);
                if (part.empty())
                {
                    if (component != 1 || end == std::string::npos)
                    {
                        return sourceError(
                            diagnostics, location, codes[component], "Missing required OBJ index in " + token);
                    }
                }
                else if (!validIndex(part, counts[component]))
                {
                    return sourceError(diagnostics, location, codes[component], "Invalid OBJ index in " + token);
                }
                if (end == std::string::npos)
                    return true;
                start = end + 1;
            }
            return sourceError(diagnostics, location, "OBJ_INDEX_INVALID", "Too many index components in " + token);
        }

        // TinyOBJ inserts white colors and may drop malformed polygons during
        // triangulation - so we record color presence and reject invalid indices first
        bool inspectSource(std::istream&                    input,
                           const std::filesystem::path&     path,
                           SourceData&                      source,
                           std::string&                     text,
                           size_t&                          triangleCount,
                           std::vector<GtsModelDiagnostic>& diagnostics)
        {
            std::array<size_t, 3> counts{}; // position, texcoord, normal
            std::string           line;
            size_t                lineNumber = 0;
            while (std::getline(input, line))
            {
                ++lineNumber;
                if (const size_t comment = line.find('#'); comment != std::string::npos)
                {
                    line.erase(comment);
                }
                text += line + '\n';
                std::istringstream words(line);
                std::string        keyword;
                words >> keyword;
                const std::string location = path.string() + ":" + std::to_string(lineNumber);
                if (keyword == "v" || keyword == "vn" || keyword == "vt")
                {
                    size_t      components = 0;
                    std::string token;
                    while (words >> token)
                    {
                        std::string_view valueToken(token);
                        if (valueToken.starts_with('+'))
                            valueToken.remove_prefix(1);
                        float      value = 0;
                        const auto parsed =
                            std::from_chars(valueToken.data(), valueToken.data() + valueToken.size(), value);
                        if (parsed.ec != std::errc{} || parsed.ptr != valueToken.data() + valueToken.size() ||
                            !std::isfinite(value))
                        {
                            return sourceError(diagnostics,
                                               location,
                                               "OBJ_ATTRIBUTE_INVALID",
                                               "OBJ attribute contains an invalid numeric value.");
                        }
                        ++components;
                    }
                    if (components < (keyword == "vt" ? 1u : 3u))
                    {
                        return sourceError(
                            diagnostics, location, "OBJ_ATTRIBUTE_INVALID", "OBJ attribute has too few components.");
                    }
                    const size_t kind = keyword == "v" ? 0 : keyword == "vt" ? 1 : 2;
                    if (++counts[kind] > static_cast<size_t>(std::numeric_limits<int>::max()))
                    {
                        return sourceError(
                            diagnostics, location, "OBJ_TOO_LARGE", "OBJ exceeds TinyOBJ index capacity.");
                    }
                    if (keyword == "v")
                        source.authoredColors.push_back(components >= 6);
                }
                else if (keyword == "f")
                {
                    size_t      corners = 0;
                    std::string token;
                    while (words >> token)
                    {
                        if (!checkCorner(token, counts, location, diagnostics))
                            return false;
                        ++corners;
                    }
                    if (corners < 3)
                    {
                        return sourceError(
                            diagnostics, location, "OBJ_FACE_INVALID", "A face requires at least three corners.");
                    }
                    triangleCount += corners - 2;
                }
                else if (keyword == "l" || keyword == "p")
                {
                    diagnostics.push_back({GtsModelDiagnosticSeverity::Warning,
                                           "OBJ_UNSUPPORTED_PRIMITIVE",
                                           "Only OBJ faces are imported; line/point records are omitted.",
                                           location});
                }
            }
            if (input.bad())
                return sourceError(diagnostics, path.string(), "OBJ_READ_FAILED", "Could not read OBJ source.");
            return true;
        }

        class MaterialReader final : public tinyobj::MaterialReader
        {
            public:
            MaterialReader(std::filesystem::path directory, std::vector<MaterialSource>& sources)
                : baseDirectory(std::move(directory)), materialSources(sources)
            {
            }

            bool operator()(const std::string&                name,
                            std::vector<tinyobj::material_t>* materials,
                            std::map<std::string, int>*       materialMap,
                            std::string*                      warning,
                            std::string*                      error) override
            {
                const std::filesystem::path path = (baseDirectory / name).lexically_normal();
                std::ifstream               input(path);
                if (!input)
                {
                    *warning += "Cannot read material library: " + path.string() + "\n";
                    return false;
                }
                std::string                 text;
                std::string                 line;
                std::string                 materialName;
                MaterialSource              metadata{path.parent_path(), {}};
                std::vector<MaterialSource> nextSources;
                while (std::getline(input, line))
                {
                    text += line + '\n';
                    std::istringstream words(line);
                    std::string        keyword;
                    words >> keyword;
                    if (keyword.empty() || keyword.starts_with('#'))
                        continue;
                    std::string value;
                    std::getline(words >> std::ws, value);
                    if (keyword == "newmtl")
                    {
                        if (!materialName.empty())
                            nextSources.push_back(std::move(metadata));
                        metadata     = {path.parent_path(), {}};
                        materialName = value;
                    }
                    else
                    {
                        metadata.properties[keyword] = value;
                    }
                }
                if (input.bad())
                {
                    *error += "Could not read material library: " + path.string() + "\n";
                    return false;
                }
                nextSources.push_back(std::move(metadata));
                const size_t       firstMaterial = materials->size();
                std::istringstream materialInput(text);
                tinyobj::LoadMtl(materialMap, materials, &materialInput, warning, error);
                if (materials->size() - firstMaterial != nextSources.size())
                {
                    *error += "Material source metadata does not match parsed materials: " + path.string() + "\n";
                    return false;
                }
                for (auto& next : nextSources)
                    materialSources.push_back(std::move(next));
                return true;
            }

            private:
            std::filesystem::path        baseDirectory;
            std::vector<MaterialSource>& materialSources;
        };
    } // namespace

    bool readSource(const std::filesystem::path& path, SourceData& source, std::vector<GtsModelDiagnostic>& diagnostics)
    {
        std::ifstream input(path);
        if (!input)
            return sourceError(diagnostics, path.string(), "OBJ_READ_FAILED", "Cannot open OBJ source.");
        std::string text;
        size_t      expectedTriangles = 0;
        if (!inspectSource(input, path, source, text, expectedTriangles, diagnostics))
            return false;

        MaterialReader     materialReader(path.parent_path(), source.materialSources);
        std::istringstream objInput(text);
        std::string        warning;
        std::string        error;
        const bool         loaded = tinyobj::LoadObj(&source.attributes,
                                                     &source.shapes,
                                                     &source.materials,
                                                     &warning,
                                                     &error,
                                                     &objInput,
                                                     &materialReader,
                                                     true,
                                                     true);
        if (!warning.empty())
        {
            diagnostics.push_back({GtsModelDiagnosticSeverity::Warning, "OBJ_PARSE_WARNING", warning, path.string()});
        }
        if (!loaded || !error.empty())
        {
            return sourceError(
                diagnostics, path.string(), "OBJ_PARSE_FAILED", error.empty() ? "TinyOBJ parsing failed." : error);
        }
        size_t triangles = 0;
        for (const auto& shape : source.shapes)
            triangles += shape.mesh.num_face_vertices.size();
        if (triangles != expectedTriangles)
        {
            return sourceError(diagnostics,
                               path.string(),
                               "OBJ_TRIANGULATION_FAILED",
                               "TinyOBJ did not preserve all source faces during triangulation.");
        }
        if (source.authoredColors.size() != source.attributes.vertices.size() / 3 ||
            source.materialSources.size() != source.materials.size())
        {
            return sourceError(diagnostics,
                               path.string(),
                               "OBJ_SOURCE_METADATA_INVALID",
                               "Source metadata does not match TinyOBJ output.");
        }
        return true;
    }
} // namespace gts::obj
