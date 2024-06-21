#pragma once

#include <vector>
#include <unordered_map>
#include <stdexcept>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtx/hash.hpp>

#include <tiny_obj_loader.h>

#include "Vertex.h"

namespace std 
{
    template<> struct hash<Vertex> 
    {
        size_t operator()(Vertex const& vertex) const 
        {
            return (std::hash<glm::vec3>()(vertex.pos) ^ (std::hash<glm::vec3>()(vertex.color) << 1)) >> 1 ^ (std::hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
}

class GtsModelLoader
{
    public:
        static void loadModel(std::string model_path, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);
};