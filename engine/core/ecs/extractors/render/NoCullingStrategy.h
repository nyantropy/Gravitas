#pragma once

#include "IVisibilityStrategy.h"

class NoCullingStrategy : public IVisibilityStrategy
{
public:
    void filter(RenderExtractionSnapshot&) override
    {
    }
};
