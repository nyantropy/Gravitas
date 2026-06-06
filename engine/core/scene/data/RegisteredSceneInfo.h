#pragma once

#include <string>

// Metadata for scenes registered with the engine and exposed to tooling panels.
struct RegisteredSceneInfo
{
    std::string id;
    std::string displayName;
    bool        devOnly = false;
};
