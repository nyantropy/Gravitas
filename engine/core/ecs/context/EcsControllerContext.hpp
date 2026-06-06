#pragma once

#include <string>
#include <vector>

#include "InputBindingRegistry.h"
#include "IResourceProvider.hpp"
#include "TimeContext.h"
#include "GtsCommandBuffer.h"
#include "UiSystem.h"
#include "IGtsPhysicsModule.h"
#include "RegisteredSceneInfo.h"

class ECSWorld;

// this is the context object passed into each ecs controller system
// contains all frame-dependent dependencies, and is thus NOT available in simulation systems
// pointers are valid for the duration of the current call, so systems may not cache them across frames,
// since some subsystems may be rebuilt between frames (e.g window resize)
struct EcsControllerContext
{
    ECSWorld&                       world;

    IResourceProvider*              resources         = nullptr;
    InputBindingRegistry*           input             = nullptr;
    const TimeContext*              time              = nullptr;
    GtsCommandBuffer*               engineCommands    = nullptr;
    UiSystem*                       ui                = nullptr;
    IGtsPhysicsModule*              physics           = nullptr;
    const std::vector<RegisteredSceneInfo>* registeredScenes = nullptr;
    const std::string*              activeSceneName   = nullptr;
    float                           windowAspectRatio  = 1.0f;
    float                           windowPixelWidth   = 1.0f;
    float                           windowPixelHeight  = 1.0f;
};
