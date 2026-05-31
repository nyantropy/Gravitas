#pragma once
#include <memory>
#include <string>

#include "GraphicsConfig.h"
#include "GtsSceneTransitionData.h"

// idea: let different architectural layers send commands back to the engine
struct GtsCommand
{
    enum class Type
    {
        TogglePause,
        Screenshot,
        SetFrustumCullingEnabled,
        SetFrustumFreeze,
        ApplyGraphicsSettings,
        LoadScene,
        ChangeScene,
        Quit
    };

    Type type;
    std::string stringArg;
    float floatArg = 0.0f;
    RuntimeGraphicsSettings graphicsSettings;
    std::shared_ptr<GtsSceneTransitionData> transitionData;
};
