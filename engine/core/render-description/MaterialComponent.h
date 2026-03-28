#pragma once

#include <string>
#include "GlmConfig.h"

// Gameplay-facing material description.
// Contains appearance properties — no GPU handles.
// Binding systems read this alongside mesh components and drive MaterialGpuComponent.
//
// Used with StaticMeshComponent or ProceduralMeshComponent.
// NOT needed for WorldTextComponent — the font atlas is implicit.
struct MaterialComponent
{
    std::string texturePath;
    glm::vec4   tint        = {1.0f, 1.0f, 1.0f, 1.0f};
    float       alpha       = 1.0f;
    bool        doubleSided = false;
};
