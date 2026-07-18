#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <variant>

#include "GtsPaths.h"
#include "ToolCommand.h"
#include "UiSerialization.h"

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

    namespace detail
    {
        inline std::string lowerCopy(std::string value)
        {
            std::transform(value.begin(),
                           value.end(),
                           value.begin(),
                           [](unsigned char ch)
                           {
                               return static_cast<char>(std::tolower(ch));
                           });
            return value;
        }

        inline const UiJsonValue* objectMember(const UiJsonValue& value, const char* key)
        {
            if (!value.isObject())
                return nullptr;
            return value.find(key);
        }

        inline std::optional<std::string> stringMember(const UiJsonValue& value, const char* key)
        {
            const UiJsonValue* member = objectMember(value, key);
            if (member == nullptr || !member->isString())
                return std::nullopt;
            return std::get<std::string>(member->value);
        }

        inline std::optional<bool> boolMember(const UiJsonValue& value, const char* key)
        {
            const UiJsonValue* member = objectMember(value, key);
            if (member == nullptr || !member->isBool())
                return std::nullopt;
            return std::get<bool>(member->value);
        }

        inline std::optional<double> numberMember(const UiJsonValue& value, const char* key)
        {
            const UiJsonValue* member = objectMember(value, key);
            if (member == nullptr || !member->isNumber())
                return std::nullopt;
            return std::get<double>(member->value);
        }

        inline bool parseWorkspace(const std::string& value, ToolWorkspace& outWorkspace)
        {
            const std::string normalized = lowerCopy(value);
            if (normalized == "world" || normalized == "world_viewer" || normalized == "viewer")
            {
                outWorkspace = ToolWorkspace::World;
                return true;
            }
            if (normalized == "particles" || normalized == "particle" || normalized == "particle_editor")
            {
                outWorkspace = ToolWorkspace::Particles;
                return true;
            }
            if (normalized == "assets" || normalized == "asset" || normalized == "asset_browser")
            {
                outWorkspace = ToolWorkspace::Assets;
                return true;
            }
            return false;
        }

        inline std::filesystem::path resolvePresetPath(const std::string& path)
        {
            namespace fs = std::filesystem;

            fs::path candidate(path);
            if (fs::exists(candidate))
                return candidate;

            if (candidate.is_relative())
            {
                fs::path rooted = GtsPaths::GetProjectRoot() / candidate;
                if (fs::exists(rooted))
                    return rooted;
            }

            return candidate;
        }
    } // namespace detail

    inline bool loadToolLaunchPreset(const std::string& path,
                                     ToolLaunchPreset& outPreset,
                                     std::string* outError = nullptr)
    {
        namespace fs = std::filesystem;

        const fs::path resolvedPath = detail::resolvePresetPath(path);
        std::ifstream file(resolvedPath);
        if (!file)
        {
            if (outError != nullptr)
                *outError = "Could not open tooling preset: " + resolvedPath.string();
            return false;
        }

        std::ostringstream buffer;
        buffer << file.rdbuf();

        UiJsonValue root;
        std::string parseError;
        if (!parseUiJson(buffer.str(), root, &parseError) || !root.isObject())
        {
            if (outError != nullptr)
                *outError = parseError.empty() ? "Invalid tooling preset JSON" : parseError;
            return false;
        }

        ToolLaunchPreset preset;

        if (const UiJsonValue* tools = detail::objectMember(root, "tools"))
        {
            if (const auto visible = detail::boolMember(*tools, "visible"))
            {
                preset.tools.hasVisible = true;
                preset.tools.visible = *visible;
            }
            if (const auto workspace = detail::stringMember(*tools, "workspace"))
            {
                preset.tools.hasWorkspace = true;
                if (!detail::parseWorkspace(*workspace, preset.tools.workspace))
                {
                    if (outError != nullptr)
                        *outError = "Unknown tooling workspace: " + *workspace;
                    return false;
                }
            }
            if (const auto visualEvaluation = detail::boolMember(*tools, "visualEvaluation"))
            {
                preset.tools.hasVisualEvaluation = true;
                preset.tools.visualEvaluation = *visualEvaluation;
            }
            if (const auto debugDraw = detail::boolMember(*tools, "debugDraw"))
            {
                preset.tools.hasDebugDraw = true;
                preset.tools.debugDrawEnabled = *debugDraw;
            }
            if (const auto gizmos = detail::boolMember(*tools, "gizmos"))
            {
                preset.tools.hasGizmos = true;
                preset.tools.gizmosEnabled = *gizmos;
            }
            if (const auto scene = detail::stringMember(*tools, "scene"))
                preset.tools.scene = *scene;
            if (const auto particleEffect = detail::stringMember(*tools, "particleEffect"))
                preset.tools.particleEffect = *particleEffect;
            if (const auto assetManifest = detail::stringMember(*tools, "assetManifest"))
                preset.tools.assetManifest = *assetManifest;
            if (const auto selectedEmitter = detail::numberMember(*tools, "selectedEmitter"))
            {
                preset.tools.hasSelectedEmitter = true;
                preset.tools.selectedEmitter = static_cast<size_t>(std::max(0.0, *selectedEmitter));
            }
            if (const auto selectedModule = detail::numberMember(*tools, "selectedModule"))
            {
                preset.tools.hasSelectedModule = true;
                preset.tools.selectedModule = static_cast<size_t>(std::max(0.0, *selectedModule));
            }
        }

        if (const UiJsonValue* screenshots = detail::objectMember(root, "screenshots"))
        {
            if (const auto enabled = detail::boolMember(*screenshots, "enabled"))
                preset.screenshots.enabled = *enabled;
            if (const auto afterSeconds = detail::numberMember(*screenshots, "afterSeconds"))
                preset.screenshots.afterSeconds = static_cast<float>(std::max(0.0, *afterSeconds));
            if (const auto intervalSeconds = detail::numberMember(*screenshots, "intervalSeconds"))
                preset.screenshots.intervalSeconds = static_cast<float>(std::max(0.0, *intervalSeconds));
            if (const auto count = detail::numberMember(*screenshots, "count"))
                preset.screenshots.count = static_cast<uint32_t>(std::max(0.0, *count));
            if (const auto directory = detail::stringMember(*screenshots, "directory"))
                preset.screenshots.directory = *directory;
            if (const auto exitAfterCapture = detail::boolMember(*screenshots, "exitAfterCapture"))
                preset.screenshots.exitAfterCapture = *exitAfterCapture;
        }

        outPreset = std::move(preset);
        return true;
    }
} // namespace gts::tools
