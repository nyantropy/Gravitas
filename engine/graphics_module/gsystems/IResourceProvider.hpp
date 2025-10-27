#pragma once

#include <string>

#include "Types.h"

class IResourceProvider 
{
    public:
        virtual mesh_id_type requestMesh(const std::string& path) = 0;
        virtual texture_id_type requestTexture(const std::string& path) = 0;
        virtual uniform_id_type requestUniformBuffer() = 0;
};
