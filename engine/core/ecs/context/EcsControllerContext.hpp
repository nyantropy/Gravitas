#pragma once

#include "GtsAction.h"
#include "InputBindingRegistry.h"
#include "IResourceProvider.hpp"
#include "IInputSource.hpp"
#include "InputActionManager.hpp"
#include "TimeContext.h"
#include "GtsCommandBuffer.h"

class ECSWorld;
class RenderCommandExtractor;
class UiSystem;
class IGtsPhysicsModule;

// Context passed to ECSControllerSystem::update every rendered frame.
// Contains all frame-dependent dependencies. NOT available to simulation systems.
//
// Pointer lifetime: all pointers are valid for the duration of the update()
// call only. Systems must not cache them across frames — the engine may
// rebuild subsystems between frames (e.g., on device loss or window resize).
//
// Null pointers:
//   inputSource — null when the game is paused.
//   All other pointers are guaranteed non-null during normal scene execution.
//
// actions is always live (engine-level bindings, never paused).
struct EcsControllerContext
{
    ECSWorld&                       world;

    IResourceProvider*              resources         = nullptr;
    InputBindingRegistry*           input             = nullptr;
    IInputSource*                   inputSource       = nullptr; // pause-gated
    InputActionManager<GtsAction>*  actions           = nullptr; // always live
    const TimeContext*              time              = nullptr;
    GtsCommandBuffer*               engineCommands    = nullptr;
    RenderCommandExtractor*         extractor         = nullptr;
    UiSystem*                       ui                = nullptr;
    IGtsPhysicsModule*              physics           = nullptr;
    float                           windowAspectRatio  = 1.0f;
    float                           windowPixelWidth   = 1.0f;
    float                           windowPixelHeight  = 1.0f;
};
