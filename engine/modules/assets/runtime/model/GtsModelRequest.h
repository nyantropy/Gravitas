#pragma once

#include <filesystem>

struct GtsModelCapabilities
{
    bool geometry     = false;
    bool hierarchy    = false;
    bool skeletons    = false;
    bool skinBindings = false;
    bool animations   = false;

    bool satisfies(const GtsModelCapabilities& required) const
    {
        return (!required.geometry || geometry) && (!required.hierarchy || hierarchy) &&
               (!required.skeletons || skeletons) && (!required.skinBindings || skinBindings) &&
               (!required.animations || animations);
    }
};

struct GtsModelRequest
{
    std::filesystem::path path;
    GtsModelCapabilities  requiredCapabilities;
};
