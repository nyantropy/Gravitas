#pragma once

#include <optional>
#include <string>

#include "GraphicsSettings.h"

// snapshot of what the renderer currently uses
struct GraphicsRuntimeState
{
    std::optional<WindowSettings> window;
    Extent2D outputExtent;
    Extent2D renderExtent;
    std::optional<PresentModePreference> presentMode;
    bool pending = false;
};

// used when applying new graphics settings
enum class GraphicsSettingsApplyStatus
{
    Applied,
    Pending,
    Rejected
};

// used to query the result of a graphics settings application
struct GraphicsSettingsApplyResult
{
    GraphicsSettingsApplyStatus status = GraphicsSettingsApplyStatus::Rejected;
    std::string message;

    bool accepted() const { return status != GraphicsSettingsApplyStatus::Rejected; }
};
