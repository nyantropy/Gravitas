#pragma once

#include <cstdint>

#include "GlmConfig.h"

enum class TextureAnimationMode
{
    None = 0,
    UvScroll,
    FlipbookAtlas
};

// Gameplay-facing material animation descriptor.
// Pair with MaterialComponent and a scene renderable descriptor.
// The engine owns runtime time accumulation and writes UV state into object GPU data.
struct TextureAnimationComponent
{
    bool enabled = true;
    TextureAnimationMode mode = TextureAnimationMode::UvScroll;

    // Applied for every mode before frame/scroll offsets.
    glm::vec2 uvScale = {1.0f, 1.0f};
    glm::vec2 uvOffset = {0.0f, 0.0f};

    // UV units per second. Used by UvScroll and added on top of FlipbookAtlas
    // so authored atlas frames can still drift subtly.
    glm::vec2 scrollSpeed = {0.0f, 0.0f};

    // Flipbook atlas layout. Frames are selected row-major from top-left UV
    // space, matching the particle flipbook convention.
    uint32_t columns = 1;
    uint32_t rows = 1;
    uint32_t frameCount = 1;
    float frameRate = 12.0f;
    bool loop = true;

    // Initial time offset lets several objects share the same descriptor values
    // without visibly synchronizing.
    float phaseSeconds = 0.0f;
};
