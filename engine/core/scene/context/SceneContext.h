#pragma once
#include "IResourceProvider.hpp"
#include "IInputSource.hpp"
#include "InputActionManager.hpp"
#include "GtsAction.h"
#include "TimeContext.h"
#include "GtsCommandBuffer.h"

class RenderCommandExtractor;  // forward declaration — include RenderCommandExtractor.hpp for full access

// contains all the important stuff the scene needs for its logic to work out
struct SceneContext
{
    IResourceProvider*              resources;
    IInputSource*                   inputSource;       // abstract device layer — for updating local action managers
    InputActionManager<GtsAction>*  actions;           // engine-level semantic actions
    const TimeContext*              time;
    GtsCommandBuffer*               engineCommands;
    float                           windowAspectRatio = 1.0f;
    RenderCommandExtractor*         extractor         = nullptr;  // query visible/total counts, freeze frustum
};
