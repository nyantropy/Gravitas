#pragma once

#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <string>

#include "GlmConfig.h"

#include <tiny_obj_loader.h>

#include "MeshGeometryProcessor.h"
#include "Vertex.h"

class GtsModelLoader
{
    public:
        static MeshGeometryMetadata loadModel(const std::string& modelPath,
                                              std::vector<Vertex>& vertices,
                                              std::vector<uint32_t>& indices);
};
