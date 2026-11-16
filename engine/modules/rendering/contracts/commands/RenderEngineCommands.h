#pragma once

#include <string>

#include "GraphicsConfig.h"
#include "GraphicsSettingsRequest.h"
#include "GtsCommandBuffer.h"

namespace gts::rendering
{
    class RenderingRuntime;
    namespace detail { class GraphicsSettingsRequestCompletion; }
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
    private:
        std::shared_ptr<detail::GraphicsSettingsRequestCompletion> completion;
        friend class RenderingRuntime;
        friend GraphicsSettingsRequest requestApplyGraphicsSettings(GtsCommandBuffer&, const GraphicsSettings&);
    };

    void requestSetFrustumCullingEnabled(GtsCommandBuffer& commands, bool enabled);
    void requestSetFrustumFreeze(GtsCommandBuffer& commands, bool frozen);
} // namespace gts::rendering
