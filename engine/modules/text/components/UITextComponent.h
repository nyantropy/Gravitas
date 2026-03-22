#pragma once

#include <string>

#include "BitmapFont.h"

// Screen-space text component for 2D UI overlays.
// The entity does NOT need a TransformComponent.
//
// Position (x, y) is in normalised viewport coordinates:
//   (0, 0) = top-left corner, (1, 1) = bottom-right corner.
//
// Scale is in viewport-height units per font lineHeight:
//   e.g., scale = 0.05 → one line occupies 5 % of the viewport height.
//
// Workflow:
//   UICommandExtractor reads all visible UITextComponents each frame,
//   lays glyphs out in screen space (Y increases downward), and batches
//   all glyphs sharing the same atlas into one TextCommandList.
struct UITextComponent
{
    std::string  text;
    BitmapFont*  font    = nullptr;
    float        x       = 0.0f;   // [0..1] from left
    float        y       = 0.0f;   // [0..1] from top
    float        scale   = 0.05f;  // viewport-height units per lineHeight
    bool         visible = true;
};
