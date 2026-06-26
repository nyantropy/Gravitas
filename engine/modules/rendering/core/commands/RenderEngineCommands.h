#pragma once

#include <string>

#include "GraphicsConfig.h"
#include "GtsCommandBuffer.h"

namespace gts::rendering
{
    inline constexpr const char* SET_FRUSTUM_CULLING_ENABLED_COMMAND =
        "gts.rendering.set_frustum_culling_enabled";
    inline constexpr const char* SET_FRUSTUM_FREEZE_COMMAND =
        "gts.rendering.set_frustum_freeze";
    inline constexpr const char* APPLY_GRAPHICS_SETTINGS_COMMAND =
        "gts.rendering.apply_graphics_settings";

    struct SetFrustumCullingEnabledCommand
    {
        bool enabled = false;
    };

    struct SetFrustumFreezeCommand
    {
        bool frozen = false;
    };

    struct ApplyGraphicsSettingsCommand
    {
        RuntimeGraphicsSettings settings;
    };

    inline void requestSetFrustumCullingEnabled(GtsCommandBuffer& commands, bool enabled)
    {
        commands.requestExtensionCommand(
            SET_FRUSTUM_CULLING_ENABLED_COMMAND,
            SetFrustumCullingEnabledCommand{enabled});
    }

    inline void requestSetFrustumFreeze(GtsCommandBuffer& commands, bool frozen)
    {
        commands.requestExtensionCommand(
            SET_FRUSTUM_FREEZE_COMMAND,
            SetFrustumFreezeCommand{frozen});
    }

    inline void requestApplyGraphicsSettings(GtsCommandBuffer& commands, const RuntimeGraphicsSettings& settings)
    {
        commands.requestExtensionCommand(
            APPLY_GRAPHICS_SETTINGS_COMMAND,
            ApplyGraphicsSettingsCommand{settings});
    }
}
