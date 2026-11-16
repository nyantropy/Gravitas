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

} // namespace gts::rendering
