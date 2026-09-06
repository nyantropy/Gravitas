#pragma once

// settings for our shitty tooling
struct ToolSettings
{
    // whether engine-owned editor/tool controllers run around the active scene
    bool enabled = true;

    // whether the F3 debug overlay is visible by default on startup
    bool debugOverlayEnabledByDefault = false;
};
