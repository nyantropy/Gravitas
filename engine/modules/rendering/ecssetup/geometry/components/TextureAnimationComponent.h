#pragma once

#include <cstdint>

#include "GlmConfig.h"

// two modes, flipbookatlas and uvscroll
enum class TextureAnimationMode
{
    UvScroll = 0,
    FlipbookAtlas
};

// you can choose if the animation uses scaled or unscaled dt
enum class TextureAnimationTimeMode
{
    Unscaled = 0,
    Scaled
};

// gameplay-facing material animation descriptor
// pair with MaterialComponent and a scene renderable descriptor
// the engine owns runtime time accumulation and writes UV state into object GPU data
struct TextureAnimationComponent
{
    bool enabled = true;
    TextureAnimationMode mode = TextureAnimationMode::UvScroll;
    TextureAnimationTimeMode timeMode = TextureAnimationTimeMode::Unscaled;

    // applied for every mode before frame/scroll offsets
    glm::vec2 uvScale = {1.0f, 1.0f};
    glm::vec2 uvOffset = {0.0f, 0.0f};

    // uv units per second
    // in flipbook mode this scrolls inside the selected atlas frame
    glm::vec2 scrollSpeed = {0.0f, 0.0f};

    // flipbook atlas layout
    // frames are selected row-major from top-left uv space
    uint32_t columns = 1;
    uint32_t rows = 1;
    uint32_t frameCount = 1;
    float frameRate = 12.0f;
    bool loop = true;

    // initial time offset keeps copies from syncing up visually
    float phaseSeconds = 0.0f;
};
