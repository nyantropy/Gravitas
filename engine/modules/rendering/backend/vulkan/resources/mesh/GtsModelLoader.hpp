#pragma once

#include <vector>
#include <string>

#include "MeshGeometryProcessor.h"
#include "Vertex.h"

class GtsModelLoader
{
    public:
        static MeshGeometryMetadata loadModel(const std::string& modelPath,
                                              std::vector<Vertex>& vertices,
                                              std::vector<uint32_t>& indices);
};
