#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "ToolWorkspace.h"

namespace gts::tools
{
    enum class ToolCommandType
    {
        SetWorkspace,
        LoadScene,
        ScenePagePrevious,
        ScenePageNext,
        OpenParticleEffect,
        EffectPagePrevious,
        EffectPageNext,
        SelectAsset,
        AssetPagePrevious,
        AssetPageNext,
        SaveParticleEffect,
        ReloadParticleEffect,
        ToggleParticlePlayback,
        RestartParticlePreview,
        SelectEmitter,
        SelectModule,
        EditProperty
    };

    struct ToolCommand
    {
        ToolCommand() = default;

        ToolCommand(ToolCommandType inType)
            : type(inType)
        {
        }

        ToolCommand(ToolCommandType inType, ToolWorkspace inWorkspace)
            : type(inType),
              workspace(inWorkspace)
        {
        }

        ToolCommandType type = ToolCommandType::SetWorkspace;
        ToolWorkspace workspace = ToolWorkspace::Particles;
        std::string value;
        size_t index = 0;
        bool boolValue = false;
        int32_t intValue = 0;
        uint32_t uintValue = 0;
        float floatValue = 0.0f;
    };

    class ToolCommandQueue
    {
    public:
        void push(ToolCommand command)
        {
            commands.push_back(std::move(command));
        }

        std::vector<ToolCommand> consume()
        {
            std::vector<ToolCommand> result = std::move(commands);
            commands.clear();
            return result;
        }

    private:
        std::vector<ToolCommand> commands;
    };
} // namespace gts::tools
