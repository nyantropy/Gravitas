#pragma once
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "GtsCommand.h"
#include "GtsSceneTransitionData.h"
#include "GraphicsConfig.h"

// the command buffer struct that the engine uses to execute custom pre defined commands
struct GtsCommandBuffer
{
    // pauses all simulation systems, controller systems keep running during the pause
    void requestTogglePause()
    {
        commands.emplace_back(GtsTogglePauseCommand{});
    }

    // change from one scene to another or load a scene
    void requestChangeScene(std::string name,
                            std::unique_ptr<GtsSceneTransitionData> data = nullptr)
    {
        commands.emplace_back(GtsChangeSceneCommand{ std::move(name), std::move(data) });
    }

    // take a screenshot
    void requestScreenshot()
    {
        commands.emplace_back(GtsScreenshotCommand{});
    }

    // enable/disable frustum cull
    void requestSetFrustumCullingEnabled(bool enabled)
    {
        commands.emplace_back(GtsSetFrustumCullingEnabledCommand{ enabled });
    }

    // freeze the frustum at the current location
    void requestSetFrustumFreeze(bool frozen)
    {
        commands.emplace_back(GtsSetFrustumFreezeCommand{ frozen });
    }

    // apply different graphic settings to the engine at runtime
    void requestApplyGraphicsSettings(const RuntimeGraphicsSettings& settings)
    {
        commands.emplace_back(GtsApplyGraphicsSettingsCommand{ settings });
    }

    // quit/shutdown the engine
    void requestQuit()
    {
        commands.emplace_back(GtsQuitCommand{});
    }

    // should only be used by the engine
    std::vector<GtsCommand> commands;
};
