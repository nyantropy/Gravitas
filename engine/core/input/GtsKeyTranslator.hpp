#pragma once

#include "GtsKey.h"

// Abstract base class for translating platform-specific key codes
class GtsKeyTranslator
{
    public:
        virtual ~GtsKeyTranslator() = default;

        // Converts a platform key code to a GtsKey
        virtual GtsKey fromPlatformKey(int platformKey) const = 0;

        // Converts a GtsKey back to a platform key code
        virtual int toPlatformKey(GtsKey key) const = 0;
};

