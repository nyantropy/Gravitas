#pragma once
#include <any>
#include <memory>
#include <string>
#include <variant>

#include "GtsSceneTransitionData.h"

// a collection of engine command structs that may be used by scenes to ask the engine to entertain certain tasks

struct GtsTogglePauseCommand
{
};

struct GtsScreenshotCommand
{
    std::string directory;
};

struct GtsQuitCommand
{
};

struct GtsExtensionCommand
{
    std::string name;
    std::any payload;
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
                                GtsExtensionCommand,
                                GtsChangeSceneCommand>;
