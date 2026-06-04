#pragma once

// Engine-owned runtime companion for TextureAnimationComponent.
// Application code should author TextureAnimationComponent and leave this alone.
struct TextureAnimationRuntimeComponent
{
    float elapsedSeconds = 0.0f;
};
