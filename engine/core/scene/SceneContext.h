#pragma once
#include "IResourceProvider.hpp"
#include "InputManager.hpp"
#include "TimeContext.h"
#include "GtsCommandBuffer.h"

// contains all the important stuff the scene needs for its logic to work out
struct SceneContext
{
    IResourceProvider* resources;
    InputManager* input;
    const TimeContext* time;
    GtsCommandBuffer* engineCommands;
};
