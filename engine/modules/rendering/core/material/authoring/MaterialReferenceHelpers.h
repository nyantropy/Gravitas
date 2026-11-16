#pragma once

#include <string>
#include "MaterialReferenceComponent.h"

class ECSWorld;

namespace gts::rendering
{
    struct UnlitMaterialDescriptor
    {
        std::string       texturePath;
        glm::vec4         baseColor       = {1.0f, 1.0f, 1.0f, 1.0f};
        MaterialBlendMode blendMode       = MaterialBlendMode::Alpha;
        bool              doubleSided     = false;
        bool              vertexColorOnly = false;
        bool              depthWrite      = true;
    };

    bool                   operator==(const UnlitMaterialDescriptor& lhs, const UnlitMaterialDescriptor& rhs);
    MaterialInstanceHandle createUnlitMaterial(ECSWorld& world, const UnlitMaterialDescriptor& descriptor);
    MaterialInstanceHandle getOrCreateSharedUnlitMaterial(ECSWorld& world, const UnlitMaterialDescriptor& descriptor);
    MaterialInstanceHandle getOrCreateSharedCookedMaterial(ECSWorld& world, const std::string& path);
    MaterialReferenceComponent sharedUnlitMaterialReference(ECSWorld& world, const UnlitMaterialDescriptor& descriptor);
} // namespace gts::rendering
