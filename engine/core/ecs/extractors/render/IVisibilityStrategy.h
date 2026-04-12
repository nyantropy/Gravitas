#pragma once

#include "RenderExtractionSnapshot.h"

class IVisibilityStrategy
{
public:
    virtual ~IVisibilityStrategy() = default;

    virtual void filter(RenderExtractionSnapshot& snapshot) = 0;

    virtual void setEnabled(bool enabled)
    {
        (void)enabled;
    }

    virtual bool isEnabled() const
    {
        return true;
    }

    virtual void toggleFreeze() {}

    virtual bool isFrozen() const
    {
        return false;
    }
};
