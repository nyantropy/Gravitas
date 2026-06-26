#pragma once

#include <string>
#include "GlmConfig.h"

// gameplay-facing material description
// contains appearance properties — no GPU handles
// binding systems read this alongside mesh components and drive MaterialGpuComponent

// to be used in conjunction with StaticMeshComponent, QuadMeshComponent, or DynamicMeshComponent
// NOT needed for WorldTextComponent — the font atlas is implicit
enum class MaterialBlendMode
{
    Alpha,
    Additive
};

struct MaterialComponent
{
    std::string texturePath;
    glm::vec4   tint             = {1.0f, 1.0f, 1.0f, 1.0f};
    MaterialBlendMode blendMode  = MaterialBlendMode::Alpha;
    bool        doubleSided      = false;
    bool        vertexColorOnly  = false;
    bool        depthWrite       = true;
};
