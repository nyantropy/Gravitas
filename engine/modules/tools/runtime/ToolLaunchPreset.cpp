#include "ToolLaunchPreset.h"
#include "GtsJsonParser.h"

#include <algorithm>
#include <limits>
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

        GtsJsonValue root;
        std::string parseError;
        if (!GtsJsonParser::parse(buffer.str(), root, &parseError) || !root.isObject())
        {
            if (outError != nullptr)
                *outError = parseError.empty() ? "Invalid tooling preset JSON" : parseError;
            return false;
        }

        ToolLaunchPreset preset;

        if (const GtsJsonValue* tools = root.find("tools"))
        {
            if (const auto visible = tools->findBool("visible"))
            {
                preset.tools.hasVisible = true;
                preset.tools.visible = *visible;
            }
            if (const auto workspace = tools->findString("workspace"))
            {
                preset.tools.hasWorkspace = true;
                const auto parsedWorkspace = gts::enumValue(toolWorkspaceNames, lowerCopy(*workspace));
                if (!parsedWorkspace)
                {
                    if (outError != nullptr)
                        *outError = "Unknown tooling workspace: " + *workspace;
                    return false;
                }
                preset.tools.workspace = *parsedWorkspace;
            }
            if (const auto visualEvaluation = tools->findBool("visualEvaluation"))
            {
                preset.tools.hasVisualEvaluation = true;
                preset.tools.visualEvaluation = *visualEvaluation;
            }
            if (const auto debugDraw = tools->findBool("debugDraw"))
            {
                preset.tools.hasDebugDraw = true;
                preset.tools.debugDrawEnabled = *debugDraw;
            }
            if (const auto gizmos = tools->findBool("gizmos"))
            {
                preset.tools.hasGizmos = true;
                preset.tools.gizmosEnabled = *gizmos;
            }
            if (const auto scene = tools->findString("scene"))
                preset.tools.scene = *scene;
            if (const auto particleEffect = tools->findString("particleEffect"))
                preset.tools.particleEffect = *particleEffect;
            if (const auto assetManifest = tools->findString("assetManifest"))
                preset.tools.assetManifest = *assetManifest;
            if (const auto selectedEmitter = tools->findUInt64("selectedEmitter");
                selectedEmitter && *selectedEmitter <= std::numeric_limits<size_t>::max())
            {
                preset.tools.hasSelectedEmitter = true;
                preset.tools.selectedEmitter = static_cast<size_t>(*selectedEmitter);
            }
            else if (const auto number = tools->findNumber("selectedEmitter"); number && *number < 0)
            {
                preset.tools.hasSelectedEmitter = true;
                preset.tools.selectedEmitter = 0;
            }
            if (const auto selectedModule = tools->findUInt64("selectedModule");
                selectedModule && *selectedModule <= std::numeric_limits<size_t>::max())
            {
                preset.tools.hasSelectedModule = true;
                preset.tools.selectedModule = static_cast<size_t>(*selectedModule);
            }
            else if (const auto number = tools->findNumber("selectedModule"); number && *number < 0)
            {
                preset.tools.hasSelectedModule = true;
                preset.tools.selectedModule = 0;
            }
        }

        if (const GtsJsonValue* screenshots = root.find("screenshots"))
        {
            if (const auto enabled = screenshots->findBool("enabled"))
                preset.screenshots.enabled = *enabled;
            if (const auto afterSeconds = screenshots->findFloat("afterSeconds"))
                preset.screenshots.afterSeconds = std::max(0.0f, *afterSeconds);
            if (const auto intervalSeconds = screenshots->findFloat("intervalSeconds"))
                preset.screenshots.intervalSeconds = std::max(0.0f, *intervalSeconds);
            if (const auto count = screenshots->findUInt32("count"))
                preset.screenshots.count = *count;
            else if (const auto number = screenshots->findNumber("count"); number && *number < 0)
                preset.screenshots.count = 0;
            if (const auto directory = screenshots->findString("directory"))
                preset.screenshots.directory = *directory;
            if (const auto exitAfterCapture = screenshots->findBool("exitAfterCapture"))
                preset.screenshots.exitAfterCapture = *exitAfterCapture;
        }

        outPreset = std::move(preset);
        return true;
    }
} // namespace gts::tools
