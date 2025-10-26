#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

#include "TextureResource.h"

// material, which only contains a texture for now, behaves similarly to a mesh, with the path being a key to the texture
struct MaterialComponent 
{
    std::string textureKey;
    TextureResource* texturePtr;
};
