#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <tiny_obj_loader.h>

#include "model/domain/model/GtsModelDiagnostic.h"

namespace gts::obj
{
    struct MaterialSource
    {
        std::filesystem::path              directory;
        std::map<std::string, std::string> properties;
    };

    struct SourceData
    {
        tinyobj::attrib_t                attributes;
        std::vector<tinyobj::shape_t>    shapes;
        std::vector<tinyobj::material_t> materials;
        std::vector<MaterialSource>      materialSources;
        std::vector<bool>                authoredColors;
    };

    bool
    readSource(const std::filesystem::path& path, SourceData& source, std::vector<GtsModelDiagnostic>& diagnostics);
} // namespace gts::obj
