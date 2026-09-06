#include "ToolLaunchPreset.h"

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
#include "UiSerialization.h"

namespace gts::tools
{
    namespace
    {
        std::string lowerCopy(std::string value)
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

        const UiJsonValue* objectMember(const UiJsonValue& value, const char* key)
        {
            if (!value.isObject())
                return nullptr;
            return value.find(key);
        }

        std::optional<std::string> stringMember(const UiJsonValue& value, const char* key)
        {
            const UiJsonValue* member = objectMember(value, key);
            if (member == nullptr || !member->isString())
                return std::nullopt;
            return std::get<std::string>(member->value);
        }

        std::optional<bool> boolMember(const UiJsonValue& value, const char* key)
        {
            const UiJsonValue* member = objectMember(value, key);
            if (member == nullptr || !member->isBool())
                return std::nullopt;
            return std::get<bool>(member->value);
        }

        std::optional<double> numberMember(const UiJsonValue& value, const char* key)
        {
            const UiJsonValue* member = objectMember(value, key);
            if (member == nullptr || !member->isNumber())
                return std::nullopt;
            return std::get<double>(member->value);
        }

        bool parseWorkspace(const std::string& value, ToolWorkspace& outWorkspace)
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

        std::filesystem::path resolvePresetPath(const std::string& path)
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
    } // namespace

    bool loadToolLaunchPreset(const std::string& path,
                                     ToolLaunchPreset& outPreset,
                                     std::string* outError)
    {
        namespace fs = std::filesystem;

        const fs::path resolvedPath = resolvePresetPath(path);
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

        if (const UiJsonValue* tools = objectMember(root, "tools"))
        {
            if (const auto visible = boolMember(*tools, "visible"))
            {
                preset.tools.hasVisible = true;
                preset.tools.visible = *visible;
            }
            if (const auto workspace = stringMember(*tools, "workspace"))
            {
                preset.tools.hasWorkspace = true;
                if (!parseWorkspace(*workspace, preset.tools.workspace))
                {
                    if (outError != nullptr)
                        *outError = "Unknown tooling workspace: " + *workspace;
                    return false;
                }
            }
            if (const auto visualEvaluation = boolMember(*tools, "visualEvaluation"))
            {
                preset.tools.hasVisualEvaluation = true;
                preset.tools.visualEvaluation = *visualEvaluation;
            }
            if (const auto debugDraw = boolMember(*tools, "debugDraw"))
            {
                preset.tools.hasDebugDraw = true;
                preset.tools.debugDrawEnabled = *debugDraw;
            }
            if (const auto gizmos = boolMember(*tools, "gizmos"))
            {
                preset.tools.hasGizmos = true;
                preset.tools.gizmosEnabled = *gizmos;
            }
            if (const auto scene = stringMember(*tools, "scene"))
                preset.tools.scene = *scene;
            if (const auto particleEffect = stringMember(*tools, "particleEffect"))
                preset.tools.particleEffect = *particleEffect;
            if (const auto assetManifest = stringMember(*tools, "assetManifest"))
                preset.tools.assetManifest = *assetManifest;
            if (const auto selectedEmitter = numberMember(*tools, "selectedEmitter"))
            {
                preset.tools.hasSelectedEmitter = true;
                preset.tools.selectedEmitter = static_cast<size_t>(std::max(0.0, *selectedEmitter));
            }
            if (const auto selectedModule = numberMember(*tools, "selectedModule"))
            {
                preset.tools.hasSelectedModule = true;
                preset.tools.selectedModule = static_cast<size_t>(std::max(0.0, *selectedModule));
            }
        }

        if (const UiJsonValue* screenshots = objectMember(root, "screenshots"))
        {
            if (const auto enabled = boolMember(*screenshots, "enabled"))
                preset.screenshots.enabled = *enabled;
            if (const auto afterSeconds = numberMember(*screenshots, "afterSeconds"))
                preset.screenshots.afterSeconds = static_cast<float>(std::max(0.0, *afterSeconds));
            if (const auto intervalSeconds = numberMember(*screenshots, "intervalSeconds"))
                preset.screenshots.intervalSeconds = static_cast<float>(std::max(0.0, *intervalSeconds));
            if (const auto count = numberMember(*screenshots, "count"))
                preset.screenshots.count = static_cast<uint32_t>(std::max(0.0, *count));
            if (const auto directory = stringMember(*screenshots, "directory"))
                preset.screenshots.directory = *directory;
            if (const auto exitAfterCapture = boolMember(*screenshots, "exitAfterCapture"))
                preset.screenshots.exitAfterCapture = *exitAfterCapture;
        }

        outPreset = std::move(preset);
        return true;
    }
} // namespace gts::tools
