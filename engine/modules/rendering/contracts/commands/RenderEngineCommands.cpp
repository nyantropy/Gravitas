#include "RenderEngineCommands.h"

namespace gts::rendering
{
    void requestSetFrustumCullingEnabled(GtsCommandBuffer& commands, bool enabled)
    {
        commands.requestExtensionCommand(SET_FRUSTUM_CULLING_ENABLED_COMMAND, SetFrustumCullingEnabledCommand{enabled});
    }

    void requestSetFrustumFreeze(GtsCommandBuffer& commands, bool frozen)
    {
        commands.requestExtensionCommand(SET_FRUSTUM_FREEZE_COMMAND, SetFrustumFreezeCommand{frozen});
    }

    void requestApplyGraphicsSettings(GtsCommandBuffer& commands, const GraphicsSettings& settings)
    {
        commands.requestExtensionCommand(APPLY_GRAPHICS_SETTINGS_COMMAND, ApplyGraphicsSettingsCommand{settings});
    }
} // namespace gts::rendering
