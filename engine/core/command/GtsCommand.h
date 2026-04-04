#pragma once
#include <memory>
#include <string>

#include "GtsSceneTransitionData.h"

// idea: let different architectural layers send commands back to the engine
struct GtsCommand
{
    enum class Type
    {
        TogglePause,
        LoadScene,
        ChangeScene,
        Quit
    };

    Type type;
    std::string stringArg;
    float floatArg = 0.0f;
    std::shared_ptr<GtsSceneTransitionData> transitionData;
};
