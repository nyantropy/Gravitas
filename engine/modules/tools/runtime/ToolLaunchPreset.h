#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "ToolWorkspace.h"

namespace gts::tools
{
    struct ToolStartupOptions
    {
        bool hasVisible = false;
        bool visible = false;

        bool hasWorkspace = false;
        ToolWorkspace workspace = ToolWorkspace::Particles;

        bool hasVisualEvaluation = false;
        bool visualEvaluation = false;

        bool hasDebugDraw = false;
        bool debugDrawEnabled = true;

        bool hasGizmos = false;
        bool gizmosEnabled = true;

        std::string scene;
        std::string particleEffect;
        std::string assetManifest;

        bool hasSelectedEmitter = false;
        size_t selectedEmitter = 0;

        bool hasSelectedModule = false;
        size_t selectedModule = 0;

        bool hasAnyToolState() const
        {
            return hasVisible || hasWorkspace || !particleEffect.empty() ||
                !assetManifest.empty() ||
                hasVisualEvaluation || hasDebugDraw || hasGizmos ||
                hasSelectedEmitter || hasSelectedModule;
        }
    };

    struct ToolScreenshotPreset
    {
        bool enabled = false;
        float afterSeconds = 2.0f;
        float intervalSeconds = 1.0f;
        uint32_t count = 1;
        std::string directory = "screenshots/tooling";
        bool exitAfterCapture = false;
    };

    struct ToolLaunchPreset
    {
        ToolStartupOptions tools;
        ToolScreenshotPreset screenshots;
    };

    bool loadToolLaunchPreset(const std::string& path,
                              ToolLaunchPreset& outPreset,
                              std::string* outError = nullptr);
} // namespace gts::tools
