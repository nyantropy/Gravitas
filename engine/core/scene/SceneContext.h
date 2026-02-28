#pragma once
#include "IResourceProvider.hpp"
#include "InputManager.hpp"
#include "InputActionManager.hpp"
#include "GtsAction.h"
#include "TimeContext.h"
#include "GtsCommandBuffer.h"

// contains all the important stuff the scene needs for its logic to work out
struct SceneContext
{
    IResourceProvider*              resources;
    InputManager*                   input;    // raw key states — use for low-level or legacy queries
    InputActionManager<GtsAction>*  actions;  // engine-level semantic actions — preferred for game systems
    const TimeContext*              time;
    GtsCommandBuffer*               engineCommands;
};
