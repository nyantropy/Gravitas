#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include "ToolLaunchPreset.h"

static_assert(gts::enumValue(gts::tools::toolWorkspaceNames, "particle_editor") == gts::tools::ToolWorkspace::Particles);
static_assert(gts::enumName(gts::tools::toolWorkspaceNames, gts::tools::ToolWorkspace::Particles) == "particles");

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

int main()
{
    const auto directory = std::filesystem::temp_directory_path() /
        ("gravitas-preset-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(directory);
    const auto path = directory / "preset.json";
    try
    {
        auto write = [&](const char* json)
        {
            std::ofstream file(path);
            file << json;
            require(static_cast<bool>(file), "Could not write test preset");
        };
        gts::tools::ToolLaunchPreset preset;
        std::string error;
        write("{}");
        require(gts::tools::loadToolLaunchPreset(path.string(), preset, &error), "Empty preset failed");
        require(!preset.tools.hasAnyToolState() && !preset.screenshots.enabled
                && preset.screenshots.count == 1 && preset.screenshots.afterSeconds == 2.0f,
                "Defaults changed");

        write(R"({"tools":{"visible":false,"workspace":"WoRlD_ViEwEr",
            "visualEvaluation":true,"debugDraw":false,"gizmos":true,
            "scene":"room","particleEffect":"effect.json","assetManifest":"asset.json",
            "selectedEmitter":3,"selectedModule":2},
            "screenshots":{"enabled":true,"afterSeconds":0.5,"intervalSeconds":1.5,
            "count":4,"directory":"captures","exitAfterCapture":true}})");
        require(gts::tools::loadToolLaunchPreset(path.string(), preset, &error), "Full preset failed");
        const auto& tools = preset.tools;
        require(tools.hasVisible && !tools.visible && tools.hasWorkspace
                && tools.workspace == gts::tools::ToolWorkspace::World
                && tools.hasVisualEvaluation && tools.visualEvaluation
                && tools.hasDebugDraw && !tools.debugDrawEnabled && tools.hasGizmos && tools.gizmosEnabled
                && tools.scene == "room" && tools.particleEffect == "effect.json" && tools.assetManifest == "asset.json"
                && tools.hasSelectedEmitter && tools.selectedEmitter == 3
                && tools.hasSelectedModule && tools.selectedModule == 2, "Tool fields changed");
        require(preset.screenshots.enabled && preset.screenshots.afterSeconds == 0.5f
                && preset.screenshots.intervalSeconds == 1.5f && preset.screenshots.count == 4
                && preset.screenshots.directory == "captures" && preset.screenshots.exitAfterCapture,
                "Screenshot fields changed");

        for (const char* invalid : {"{", "[]", R"({"tools":{"workspace":"unknown"}})"})
        {
            write(invalid);
            error.clear();
            require(!gts::tools::loadToolLaunchPreset(path.string(), preset, &error) && !error.empty(),
                    "Invalid input did not report an error");
            require(preset.tools.scene == "room" && preset.screenshots.count == 4,
                    "Failed load replaced output");
        }
        require(!gts::tools::loadToolLaunchPreset((directory / "missing.json").string(), preset),
                "Missing file unexpectedly loaded");

        write(R"({"tools":{"workspace":"asset_browser","selectedEmitter":-2,"visible":"wrong-type"},
                  "screenshots":{"afterSeconds":-1,"intervalSeconds":-2,"count":-3}})");
        require(gts::tools::loadToolLaunchPreset(path.string(), preset), "Clamped preset failed");
        require(preset.tools.workspace == gts::tools::ToolWorkspace::Assets && !preset.tools.hasVisible
                && preset.tools.selectedEmitter == 0 && preset.screenshots.afterSeconds == 0
                && preset.screenshots.intervalSeconds == 0 && preset.screenshots.count == 0,
                "Clamping or optional-field behavior changed");
        write(R"({"tools":{"selectedEmitter":1.5,"selectedModule":1e100},
                  "screenshots":{"count":4294967296,"afterSeconds":1e100}})");
        require(gts::tools::loadToolLaunchPreset(path.string(), preset), "Invalid optional numbers failed load");
        require(!preset.tools.hasSelectedEmitter && !preset.tools.hasSelectedModule
                && preset.screenshots.count == 1 && preset.screenshots.afterSeconds == 2.0f,
                "Invalid optional numbers did not retain defaults");
    }
    catch (const std::exception& error)
    {
        std::filesystem::remove_all(directory);
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::filesystem::remove_all(directory);
    return 0;
}
