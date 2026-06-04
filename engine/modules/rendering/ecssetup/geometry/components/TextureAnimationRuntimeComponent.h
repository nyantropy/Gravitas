#pragma once

#include "TextureAnimationComponent.h"

// engine-owned runtime companion for TextureAnimationComponent
// application code should author TextureAnimationComponent and leave this alone
struct TextureAnimationRuntimeComponent
{
    float elapsedSeconds = 0.0f;
    bool hasLastDescriptor = false;
    TextureAnimationComponent lastDescriptor;
};
