#pragma once
#include <memory>
#include <string>
#include <variant>

#include "GraphicsConfig.h"
#include "GtsSceneTransitionData.h"

// a collection of engine command structs that may be used by scenes to ask the engine to entertain certain tasks

struct GtsTogglePauseCommand
{
};

struct GtsScreenshotCommand
{
};

struct GtsQuitCommand
{
};

struct GtsSetFrustumCullingEnabledCommand
{
    bool enabled = false;
};

struct GtsSetFrustumFreezeCommand
{
    bool frozen = false;
};

struct GtsApplyGraphicsSettingsCommand
{
    RuntimeGraphicsSettings settings;
};

struct GtsChangeSceneCommand
{
    std::string name;
    std::unique_ptr<GtsSceneTransitionData> transitionData;
};

// all of these should map as gtscommand, but with different payloads
using GtsCommand = std::variant<GtsTogglePauseCommand,
                                GtsScreenshotCommand,
                                GtsQuitCommand,
                                GtsSetFrustumCullingEnabledCommand,
                                GtsSetFrustumFreezeCommand,
                                GtsApplyGraphicsSettingsCommand,
                                GtsChangeSceneCommand>;
