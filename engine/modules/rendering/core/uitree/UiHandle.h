#pragma once

#include <cstdint>

// Opaque handle to a UI element in the UiTree.
// Returned by UiTree::addImage / addText.
// Use to update or remove elements after creation.
// A zero-value handle is invalid.
using UiHandle = uint32_t;
static constexpr UiHandle UI_INVALID_HANDLE = 0;
