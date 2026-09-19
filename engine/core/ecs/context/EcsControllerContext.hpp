#pragma once

#include <string>
#include <vector>

#include "InputBindingRegistry.h"
#include "TimeContext.h"
#include "GtsCommandBuffer.h"
#include "RegisteredSceneInfo.h"
#include "ControllerFrameData.h"

class ECSWorld;

// this is the context object passed into each ecs controller system
// contains all frame-dependent dependencies, and is thus NOT available in simulation systems
// pointers are valid for the duration of the current call, so systems may not cache them across frames,
// since some subsystems may be rebuilt between frames (e.g window resize)
struct EcsControllerContext
{
    ECSWorld&                       world;

    InputBindingRegistry*           input             = nullptr;
    const TimeContext*              time              = nullptr;
    GtsCommandBuffer*               engineCommands    = nullptr;
    const std::vector<RegisteredSceneInfo>* registeredScenes = nullptr;
    const std::string*              activeSceneName   = nullptr;
    ControllerFrameData             frameData;
};
