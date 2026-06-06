#pragma once

#include "RenderViewport.h"

struct RenderViewportComponent
{
    RenderViewportRect sceneViewport;
    bool               constrained = false;
};
