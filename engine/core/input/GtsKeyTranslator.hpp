#pragma once

#include "GtsKey.h"

// Abstract base for translating platform scancodes to and from GtsKey.
// Implementations must only map scancodes — never raw GLFW_KEY_* or OS
// virtual-key codes — so that translation is layout-independent.
class GtsKeyTranslator
{
public:
    virtual ~GtsKeyTranslator() = default;

    // Converts a platform scancode (as supplied by the OS key callback) to a GtsKey.
    // Returns GtsKey::Unknown for any unmapped scancode.
    virtual GtsKey fromPlatformScancode(int scancode) const = 0;

    // Converts a GtsKey back to the platform scancode for that physical key position.
    // Returns -1 for GtsKey::Unknown or any key that has no platform mapping.
    // Useful for displaying bindings or future rebinding UI.
    virtual int toPlatformScancode(GtsKey key) const = 0;
};
