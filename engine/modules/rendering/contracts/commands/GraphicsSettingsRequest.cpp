#include "GraphicsSettingsRequest.h"
#include "GraphicsSettingsRequestCompletion.h"
#include "RenderEngineCommands.h"
#include <utility>

namespace gts::rendering
{
    GraphicsSettingsRequest requestApplyGraphicsSettings(GtsCommandBuffer& commands,
                                                         const GraphicsSettings& settings)
    {
        auto result = std::make_shared<GraphicsSettingsRequestResult>();
        result->requested = settings;
        ApplyGraphicsSettingsCommand command;
        command.settings = settings;
        command.completion = std::make_shared<detail::GraphicsSettingsRequestCompletion>(result);
        commands.requestExtensionCommand(APPLY_GRAPHICS_SETTINGS_COMMAND, std::move(command));
        return result;
    }
} // namespace gts::rendering
