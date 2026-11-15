#pragma once

#include <string>

#include "GraphicsConfig.h"
#include "GtsCommandBuffer.h"

namespace gts::rendering
{
    inline constexpr const char* SET_FRUSTUM_CULLING_ENABLED_COMMAND = "gts.rendering.set_frustum_culling_enabled";
    inline constexpr const char* SET_FRUSTUM_FREEZE_COMMAND          = "gts.rendering.set_frustum_freeze";
    inline constexpr const char* APPLY_GRAPHICS_SETTINGS_COMMAND     = "gts.rendering.apply_graphics_settings";

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
        GraphicsSettings settings;
    };

    void requestSetFrustumCullingEnabled(GtsCommandBuffer& commands, bool enabled);
    void requestSetFrustumFreeze(GtsCommandBuffer& commands, bool frozen);
    void requestApplyGraphicsSettings(GtsCommandBuffer& commands, const GraphicsSettings& settings);
} // namespace gts::rendering
