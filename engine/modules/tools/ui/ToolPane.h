#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "Types.h"
#include "UiWidget.h"

namespace gts::tools
{
    enum class ToolWorkspace
    {
        World,
        Particles
    };

    enum class ToolPaneId
    {
        MenuBar,
        WorkspaceTabs,
        ToolToolbar,
        WorldViewport,
        SceneBrowser,
        EffectHierarchy,
        EmitterDetails,
        ParticlePreviewViewport,
        PropertyInspector,
        CurveTimeline,
        Diagnostics,
        StatusBar
    };

    enum class ToolDockArea
    {
        Top,
        Toolbar,
        Left,
        Center,
        Right,
        Bottom,
        Status
    };

    struct PaneDescriptor
    {
        ToolPaneId id = ToolPaneId::WorldViewport;
        ToolDockArea dockArea = ToolDockArea::Center;
        const char* title = "";
        bool visibleInWorld = true;
        bool visibleInParticles = true;
        int order = 0;
    };

    inline bool paneVisibleInWorkspace(const PaneDescriptor& descriptor, ToolWorkspace workspace)
    {
        return workspace == ToolWorkspace::World ? descriptor.visibleInWorld : descriptor.visibleInParticles;
    }

    enum class ToolCommandType
    {
        SetWorkspace,
        LoadScene,
        ScenePagePrevious,
        ScenePageNext,
        OpenParticleEffect,
        EffectPagePrevious,
        EffectPageNext,
        SaveParticleEffect,
        ReloadParticleEffect,
        ToggleParticlePlayback,
        RestartParticlePreview,
        ToggleEmitterEnabled,
        ToggleModuleEnabled,
        SelectEmitter,
        SelectModule
    };

    struct ToolCommand
    {
        ToolCommandType type = ToolCommandType::SetWorkspace;
        ToolWorkspace workspace = ToolWorkspace::Particles;
        std::string value;
        size_t index = 0;
    };

    class ToolCommandQueue
    {
    public:
        void push(ToolCommand command)
        {
            commands.push_back(std::move(command));
        }

        std::vector<ToolCommand> consume()
        {
            std::vector<ToolCommand> result = std::move(commands);
            commands.clear();
            return result;
        }

    private:
        std::vector<ToolCommand> commands;
    };

    struct ToolSceneRow
    {
        std::string id;
        std::string label;
        bool active = false;
    };

    struct ToolEffectRow
    {
        std::string path;
        std::string label;
        bool active = false;
    };

    struct ToolEmitterRow
    {
        size_t index = 0;
        std::string label;
        std::string summary;
        bool active = false;
        bool enabled = true;
    };

    struct ToolModuleRow
    {
        size_t index = 0;
        std::string label;
        std::string summary;
        bool active = false;
        bool enabled = true;
    };

    struct ToolShellView
    {
        ToolWorkspace activeWorkspace = ToolWorkspace::Particles;

        float topBarHeight = 0.032f;
        float viewportToolbarHeight = 0.030f;
        float leftPaneWidth = 0.220f;
        float rightPaneWidth = 0.300f;
        float bottomDockHeight = 0.140f;
        float bottomBarHeight = 0.030f;
        float viewportX = 0.220f;
        float viewportY = 0.062f;
        float viewportWidth = 0.480f;
        float viewportHeight = 0.768f;

        std::string activeSceneName;
        std::string status = "F6 TO HIDE";
        std::vector<ToolSceneRow> scenes;
        size_t sceneOffset = 0;
        size_t sceneTotal = 0;

        std::vector<ToolEffectRow> effects;
        size_t effectOffset = 0;
        size_t effectTotal = 0;

        bool particleLoaded = false;
        bool particleDirty = false;
        bool particlePlaying = true;
        bool hasSelectedEmitter = false;
        bool selectedEmitterEnabled = false;
        bool hasSelectedModule = false;
        bool selectedModuleEnabled = false;
        std::string particleTitle = "No Effect";
        std::string particlePath;
        texture_id_type previewTexture = 0;

        std::vector<ToolEmitterRow> emitters;
        std::vector<ToolModuleRow> modules;
        std::vector<std::string> inspectorLines;
        std::vector<std::string> diagnostics;
    };

    class ToolPane
    {
    public:
        virtual ~ToolPane() = default;

        virtual const PaneDescriptor& descriptor() const = 0;
        virtual UiHandle root() const = 0;
        virtual UiHandle previewImageHandle() const { return UI_INVALID_HANDLE; }

        virtual void build(gts::ui::UiWidgetContext& context,
                           UiHandle parent,
                           BitmapFont* font,
                           const UiLayoutSpec& layout) = 0;
        virtual void update(gts::ui::UiWidgetContext& context,
                            const ToolShellView& view,
                            const UiLayoutSpec& layout,
                            bool visible) = 0;
        virtual void onEvent(gts::ui::UiWidgetContext& context,
                             UiEvent& event,
                             ToolCommandQueue& commands,
                             const ToolShellView& view) = 0;
        virtual void destroy(gts::ui::UiWidgetContext& context) = 0;
    };
} // namespace gts::tools
