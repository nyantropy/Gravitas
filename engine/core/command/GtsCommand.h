#pragma once
#include <string>

// idea: let different architectural layers send commands back to the engine
struct GtsCommand
{
    enum class Type
    {
        TogglePause,
        LoadScene,
        Quit
    };

    Type type;
    std::string stringArg;
    float floatArg = 0.0f;
};