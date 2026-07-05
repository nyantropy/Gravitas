#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "EditorPreviewRenderData.h"
#include "EngineToolInputCaptureComponent.h"
#include "EngineToolStateComponent.h"
#include "EngineToolWorkspaceComponent.h"
#include "GraphicsConstants.h"
#include "InputBindingRegistry.h"
#include "ParticleEffectAsset.h"
#include "ParticleEffectAssetIO.h"
#include "ParticleEmitterComponent.h"
#include "ParticleModuleAuthoring.h"
#include "ParticlePreviewWorld.hpp"
#include "ParticleProgramCompiler.h"
#include "RegisteredSceneInfo.h"
#include "RenderViewportComponent.h"
#include "TimeContext.h"
#include "ToolTheme.h"
#include "UiNode.h"
#include "UiSystem.h"
#include "UiWidget.h"

namespace gts::tools
{
    namespace detail
    {
        inline UiColor color(float r, float g, float b, float a = 1.0f)
        {
            return {r, g, b, a};
        }

        inline UiLayoutSpec rect(float x, float y, float w, float h)
        {
            UiLayoutSpec layout = gts::ui::fillLayout();
            layout.anchorMin = {x, y};
            layout.anchorMax = {x + w, y + h};
            return layout;
        }

        inline UiStateFlags visibleState(bool visible, bool enabled, bool interactable)
        {
            UiStateFlags state;
            state.visible = visible;
            state.enabled = enabled;
            state.interactable = interactable;
            return state;
        }

        inline std::string fileName(const std::string& path)
        {
            if (path.empty())
                return {};
            std::filesystem::path fsPath(path);
            const std::string name = fsPath.filename().string();
            return name.empty() ? path : name;
        }

        inline std::string stemName(const std::string& path)
        {
            if (path.empty())
                return {};
            std::filesystem::path fsPath(path);
            const std::string name = fsPath.stem().string();
            return name.empty() ? fileName(path) : name;
        }

        inline std::string compact(const std::string& text, size_t maxChars)
        {
            if (text.size() <= maxChars || maxChars < 8)
                return text;
            const size_t left = (maxChars - 3) / 2;
            const size_t right = maxChars - 3 - left;
            return text.substr(0, left) + "..." + text.substr(text.size() - right);
        }

        inline std::string fixed(float value, int precision = 2)
        {
            std::ostringstream out;
            out << std::fixed << std::setprecision(precision) << value;
            return out.str();
        }

        inline void setRectPayload(UiSystem& ui, UiSurfaceId surface, UiHandle handle, UiColor color)
        {
            if (handle != UI_INVALID_HANDLE)
                ui.setPayload(surface, handle, UiRectData{color});
        }

        inline void setTextColor(UiSystem& ui, UiSurfaceId surface, UiHandle handle, UiColor color)
        {
            if (handle == UI_INVALID_HANDLE)
                return;

            const UiNode* node = ui.findNode(surface, handle);
            if (node == nullptr || node->type != UiNodeType::Text)
                return;

            UiTextData text = std::get<UiTextData>(node->payload);
            text.color = color;
            ui.setPayload(surface, handle, text);
        }
    } // namespace detail

    enum class EngineToolTab
    {
        Scenes,
        Particles
    };

    enum class EngineToolCommandType
    {
        SetTab,
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

    struct EngineToolCommand
    {
        EngineToolCommandType type = EngineToolCommandType::SetTab;
        EngineToolTab tab = EngineToolTab::Particles;
        std::string value;
        size_t index = 0;
    };

    struct EngineToolSceneRow
    {
        std::string id;
        std::string label;
        bool active = false;
    };

    struct EngineToolEffectRow
    {
        std::string path;
        std::string label;
        bool active = false;
    };

    struct EngineToolEmitterRow
    {
        size_t index = 0;
        std::string label;
        std::string summary;
        bool active = false;
        bool enabled = true;
    };

    struct EngineToolModuleRow
    {
        size_t index = 0;
        std::string label;
        std::string summary;
        bool active = false;
        bool enabled = true;
    };

    struct EngineToolShellView
    {
        EngineToolTab activeTab = EngineToolTab::Particles;

        float topBarHeight = 0.032f;
        float viewportToolbarHeight = 0.030f;
        float leftPaneWidth = 0.220f;
        float rightPaneWidth = 0.300f;
        float bottomBarHeight = 0.030f;
        float viewportX = 0.220f;
        float viewportY = 0.062f;
        float viewportWidth = 0.480f;
        float viewportHeight = 0.908f;

        std::string activeSceneName;
        std::string status = "F6 TO HIDE";
        std::vector<EngineToolSceneRow> scenes;
        size_t sceneOffset = 0;
        size_t sceneTotal = 0;

        std::vector<EngineToolEffectRow> effects;
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

        std::vector<EngineToolEmitterRow> emitters;
        std::vector<EngineToolModuleRow> modules;
        std::vector<std::string> detailLines;
    };

    class EngineToolShellComposition : public UiComposition
    {
    public:
        explicit EngineToolShellComposition(BitmapFont* inFont)
            : font(inFont)
        {
        }

        void setView(EngineToolShellView inView)
        {
            view = std::move(inView);
        }

        std::vector<EngineToolCommand> consumeCommands()
        {
            std::vector<EngineToolCommand> result = std::move(commands);
            commands.clear();
            return result;
        }

        UiHandle previewImageHandle() const
        {
            return previewImage.root();
        }

        void build(UiCompositionContext& context) override
        {
            gts::ui::UiWidgetContext widgetContext(context);
            context.ui.setLayout(context.surface, context.root, gts::ui::fillLayout());
            context.ui.setState(context.surface,
                                context.root,
                                detail::visibleState(true, true, false));

            buildPanels(widgetContext, context.root);
            buildTopBar(widgetContext);
            buildViewport(widgetContext);
            buildLeftPane(widgetContext);
            buildRightPane(widgetContext);
            buildBottomBar(widgetContext);
        }

        void update(UiCompositionContext& context) override
        {
            gts::ui::UiWidgetContext widgetContext(context);
            context.ui.setState(context.surface,
                                context.root,
                                detail::visibleState(true, true, false));

            syncPanelLayouts(widgetContext);
            syncTopBar(widgetContext);
            syncViewport(widgetContext);
            syncLeftPane(widgetContext);
            syncRightPane(widgetContext);
            syncBottomBar(widgetContext);
        }

        void onEvent(UiCompositionContext& context, UiEvent& event) override
        {
            gts::ui::UiWidgetContext widgetContext(context);

            sceneTabButton.onEvent(widgetContext, event);
            particleTabButton.onEvent(widgetContext, event);
            scenePrevButton.onEvent(widgetContext, event);
            sceneNextButton.onEvent(widgetContext, event);
            effectPrevButton.onEvent(widgetContext, event);
            effectNextButton.onEvent(widgetContext, event);
            saveButton.onEvent(widgetContext, event);
            reloadButton.onEvent(widgetContext, event);
            playButton.onEvent(widgetContext, event);
            restartButton.onEvent(widgetContext, event);
            emitterToggleButton.onEvent(widgetContext, event);
            moduleToggleButton.onEvent(widgetContext, event);

            for (auto& button : sceneRows)
                button.onEvent(widgetContext, event);
            for (auto& button : effectRows)
                button.onEvent(widgetContext, event);
            for (auto& button : emitterRows)
                button.onEvent(widgetContext, event);
            for (auto& button : moduleRows)
                button.onEvent(widgetContext, event);

            if (sceneTabButton.consumePressed())
                push({EngineToolCommandType::SetTab, EngineToolTab::Scenes});
            if (particleTabButton.consumePressed())
                push({EngineToolCommandType::SetTab, EngineToolTab::Particles});
            if (scenePrevButton.consumePressed())
                push({EngineToolCommandType::ScenePagePrevious});
            if (sceneNextButton.consumePressed())
                push({EngineToolCommandType::ScenePageNext});
            if (effectPrevButton.consumePressed())
                push({EngineToolCommandType::EffectPagePrevious});
            if (effectNextButton.consumePressed())
                push({EngineToolCommandType::EffectPageNext});
            if (saveButton.consumePressed())
                push({EngineToolCommandType::SaveParticleEffect});
            if (reloadButton.consumePressed())
                push({EngineToolCommandType::ReloadParticleEffect});
            if (playButton.consumePressed())
                push({EngineToolCommandType::ToggleParticlePlayback});
            if (restartButton.consumePressed())
                push({EngineToolCommandType::RestartParticlePreview});
            if (emitterToggleButton.consumePressed())
                push({EngineToolCommandType::ToggleEmitterEnabled});
            if (moduleToggleButton.consumePressed())
                push({EngineToolCommandType::ToggleModuleEnabled});

            for (size_t i = 0; i < sceneRows.size(); ++i)
            {
                if (sceneRows[i].consumePressed() && i < view.scenes.size())
                {
                    EngineToolCommand command;
                    command.type = EngineToolCommandType::LoadScene;
                    command.value = view.scenes[i].id;
                    push(std::move(command));
                }
            }

            for (size_t i = 0; i < effectRows.size(); ++i)
            {
                if (effectRows[i].consumePressed() && i < view.effects.size())
                {
                    EngineToolCommand command;
                    command.type = EngineToolCommandType::OpenParticleEffect;
                    command.value = view.effects[i].path;
                    push(std::move(command));
                }
            }

            for (size_t i = 0; i < emitterRows.size(); ++i)
            {
                if (emitterRows[i].consumePressed() && i < view.emitters.size())
                {
                    EngineToolCommand command;
                    command.type = EngineToolCommandType::SelectEmitter;
                    command.index = view.emitters[i].index;
                    push(command);
                }
            }

            for (size_t i = 0; i < moduleRows.size(); ++i)
            {
                if (moduleRows[i].consumePressed() && i < view.modules.size())
                {
                    EngineToolCommand command;
                    command.type = EngineToolCommandType::SelectModule;
                    command.index = view.modules[i].index;
                    push(command);
                }
            }
        }

        void destroy(UiCompositionContext& context) override
        {
            gts::ui::UiWidgetContext widgetContext(context);
            destroyWidgets(widgetContext);
        }

    private:
        static constexpr size_t MaxSceneRows = 6;
        static constexpr size_t MaxEffectRows = 8;
        static constexpr size_t MaxEmitterRows = 6;
        static constexpr size_t MaxModuleRows = 6;
        static constexpr size_t MaxDetailLines = 9;

        static constexpr UiColor ShellBackground = {0.020f, 0.023f, 0.028f, 0.0f};
        static constexpr UiColor BarBackground = {0.032f, 0.035f, 0.041f, 0.94f};
        static constexpr UiColor PaneBackground = {0.041f, 0.045f, 0.052f, 0.96f};
        static constexpr UiColor PaneInset = {0.028f, 0.031f, 0.038f, 0.95f};
        static constexpr UiColor ViewportFrame = {0.010f, 0.012f, 0.016f, 0.0f};
        static constexpr UiColor Separator = {0.145f, 0.158f, 0.170f, 0.95f};
        static constexpr UiColor Text = {0.890f, 0.910f, 0.920f, 1.0f};
        static constexpr UiColor MutedText = {0.560f, 0.600f, 0.620f, 1.0f};
        static constexpr UiColor ActiveText = {0.960f, 0.940f, 0.820f, 1.0f};
        static constexpr UiColor Accent = {0.270f, 0.380f, 0.460f, 1.0f};
        static constexpr UiColor AccentStrong = {0.420f, 0.540f, 0.580f, 1.0f};
        static constexpr UiColor Button = {0.070f, 0.078f, 0.090f, 1.0f};
        static constexpr UiColor ButtonHover = {0.100f, 0.112f, 0.128f, 1.0f};
        static constexpr UiColor ButtonPressed = {0.150f, 0.168f, 0.188f, 1.0f};
        static constexpr UiColor ButtonDisabled = {0.040f, 0.044f, 0.050f, 0.74f};
        static constexpr UiColor ButtonActive = {0.190f, 0.260f, 0.300f, 1.0f};
        static constexpr UiColor Danger = {0.620f, 0.260f, 0.220f, 1.0f};

        BitmapFont* font = nullptr;
        EngineToolShellView view;
        std::vector<EngineToolCommand> commands;

        gts::ui::UiPanelWidget rootTint;
        gts::ui::UiPanelWidget topBar;
        gts::ui::UiPanelWidget viewportToolbar;
        gts::ui::UiPanelWidget leftPane;
        gts::ui::UiPanelWidget rightPane;
        gts::ui::UiPanelWidget bottomBar;
        gts::ui::UiPanelWidget viewportFrame;
        gts::ui::UiPanelWidget previewPanel;

        gts::ui::UiLabelWidget titleLabel;
        gts::ui::UiLabelWidget viewportLabel;
        gts::ui::UiLabelWidget activeSceneLabel;
        gts::ui::UiLabelWidget sceneHeader;
        gts::ui::UiLabelWidget sceneCount;
        gts::ui::UiLabelWidget effectHeader;
        gts::ui::UiLabelWidget effectCount;
        gts::ui::UiLabelWidget particleTitle;
        gts::ui::UiLabelWidget particlePath;
        gts::ui::UiLabelWidget previewPlaceholder;
        gts::ui::UiLabelWidget emitterHeader;
        gts::ui::UiLabelWidget moduleHeader;
        gts::ui::UiLabelWidget detailHeader;
        gts::ui::UiLabelWidget statusLabel;

        std::array<gts::ui::UiLabelWidget, MaxDetailLines> detailLines;

        gts::ui::UiButtonWidget sceneTabButton;
        gts::ui::UiButtonWidget particleTabButton;
        gts::ui::UiButtonWidget scenePrevButton;
        gts::ui::UiButtonWidget sceneNextButton;
        gts::ui::UiButtonWidget effectPrevButton;
        gts::ui::UiButtonWidget effectNextButton;
        gts::ui::UiButtonWidget saveButton;
        gts::ui::UiButtonWidget reloadButton;
        gts::ui::UiButtonWidget playButton;
        gts::ui::UiButtonWidget restartButton;
        gts::ui::UiButtonWidget emitterToggleButton;
        gts::ui::UiButtonWidget moduleToggleButton;

        std::array<gts::ui::UiButtonWidget, MaxSceneRows> sceneRows;
        std::array<gts::ui::UiButtonWidget, MaxEffectRows> effectRows;
        std::array<gts::ui::UiButtonWidget, MaxEmitterRows> emitterRows;
        std::array<gts::ui::UiButtonWidget, MaxModuleRows> moduleRows;
        gts::ui::UiImageWidget previewImage;

        void push(EngineToolCommand command)
        {
            commands.push_back(std::move(command));
        }

        void buildPanels(gts::ui::UiWidgetContext& context, UiHandle parent)
        {
            buildPanel(rootTint, context, parent, gts::ui::fillLayout(), ShellBackground, false);
            buildPanel(topBar, context, parent, detail::rect(0.0f, 0.0f, 1.0f, view.topBarHeight), BarBackground, true);
            buildPanel(viewportToolbar,
                       context,
                       parent,
                       detail::rect(0.0f, view.topBarHeight, 1.0f, view.viewportToolbarHeight),
                       BarBackground,
                       true);
            buildPanel(leftPane,
                       context,
                       parent,
                       detail::rect(0.0f,
                                    view.viewportY,
                                    view.leftPaneWidth,
                                    1.0f - view.viewportY - view.bottomBarHeight),
                       PaneBackground,
                       true);
            buildPanel(rightPane,
                       context,
                       parent,
                       detail::rect(1.0f - view.rightPaneWidth,
                                    view.topBarHeight,
                                    view.rightPaneWidth,
                                    1.0f - view.topBarHeight - view.bottomBarHeight),
                       PaneBackground,
                       true);
            buildPanel(bottomBar,
                       context,
                       parent,
                       detail::rect(0.0f, 1.0f - view.bottomBarHeight, 1.0f, view.bottomBarHeight),
                       BarBackground,
                       true);
            buildPanel(viewportFrame,
                       context,
                       parent,
                       detail::rect(view.viewportX, view.viewportY, view.viewportWidth, view.viewportHeight),
                       ViewportFrame,
                       false);
        }

        void buildTopBar(gts::ui::UiWidgetContext& context)
        {
            titleLabel.build(context,
                             topBar.content(),
                             label("GRAVITAS TOOLS", detail::rect(0.012f, 0.120f, 0.150f, 0.760f), Text, ToolTheme::titleTextScale));
            sceneTabButton.build(context,
                                 topBar.content(),
                                 button("SCENES", detail::rect(0.190f, 0.150f, 0.095f, 0.700f), ToolTheme::buttonTextScale));
            particleTabButton.build(context,
                                    topBar.content(),
                                    button("PARTICLES", detail::rect(0.292f, 0.150f, 0.125f, 0.700f), ToolTheme::buttonTextScale));
        }

        void buildViewport(gts::ui::UiWidgetContext& context)
        {
            viewportLabel.build(context,
                                viewportToolbar.content(),
                                label("VIEWPORT", detail::rect(0.012f, 0.120f, 0.100f, 0.760f), MutedText, ToolTheme::smallTextScale));
            activeSceneLabel.build(context,
                                   viewportToolbar.content(),
                                   label("Scene", detail::rect(0.115f, 0.120f, 0.560f, 0.760f), Text, ToolTheme::smallTextScale));
        }

        void buildLeftPane(gts::ui::UiWidgetContext& context)
        {
            sceneHeader.build(context,
                              leftPane.content(),
                              label("Scenes", detail::rect(0.050f, 0.030f, 0.430f, 0.035f), Text, ToolTheme::headerTextScale));
            sceneCount.build(context,
                             leftPane.content(),
                             label("0", detail::rect(0.500f, 0.032f, 0.450f, 0.032f), MutedText, ToolTheme::smallTextScale,
                                   UiHorizontalAlign::Right));

            float rowY = 0.082f;
            for (auto& row : sceneRows)
            {
                row.build(context,
                          leftPane.content(),
                          button("--", detail::rect(0.050f, rowY, 0.900f, 0.044f), ToolTheme::smallTextScale, UiHorizontalAlign::Left));
                rowY += 0.050f;
            }

            scenePrevButton.build(context,
                                  leftPane.content(),
                                  button("PREV", detail::rect(0.050f, 0.392f, 0.430f, 0.038f), ToolTheme::buttonTextScale));
            sceneNextButton.build(context,
                                  leftPane.content(),
                                  button("NEXT", detail::rect(0.520f, 0.392f, 0.430f, 0.038f), ToolTheme::buttonTextScale));

            effectHeader.build(context,
                               leftPane.content(),
                               label("Particle Assets", detail::rect(0.050f, 0.470f, 0.560f, 0.035f), Text, ToolTheme::headerTextScale));
            effectCount.build(context,
                              leftPane.content(),
                              label("0", detail::rect(0.620f, 0.472f, 0.330f, 0.032f), MutedText, ToolTheme::smallTextScale,
                                    UiHorizontalAlign::Right));

            rowY = 0.522f;
            for (auto& row : effectRows)
            {
                row.build(context,
                          leftPane.content(),
                          button("--", detail::rect(0.050f, rowY, 0.900f, 0.038f), ToolTheme::smallTextScale, UiHorizontalAlign::Left));
                rowY += 0.043f;
            }

            effectPrevButton.build(context,
                                   leftPane.content(),
                                   button("PREV", detail::rect(0.050f, 0.890f, 0.430f, 0.040f), ToolTheme::buttonTextScale));
            effectNextButton.build(context,
                                   leftPane.content(),
                                   button("NEXT", detail::rect(0.520f, 0.890f, 0.430f, 0.040f), ToolTheme::buttonTextScale));
        }

        void buildRightPane(gts::ui::UiWidgetContext& context)
        {
            particleTitle.build(context,
                                rightPane.content(),
                                label("No Effect", detail::rect(0.050f, 0.022f, 0.900f, 0.044f), Text, ToolTheme::headerTextScale));
            particlePath.build(context,
                               rightPane.content(),
                               label("Select a particle asset", detail::rect(0.050f, 0.066f, 0.900f, 0.034f),
                                     MutedText, ToolTheme::smallTextScale));

            buildPanel(previewPanel,
                       context,
                       rightPane.content(),
                       detail::rect(0.050f, 0.116f, 0.900f, 0.230f),
                       PaneInset,
                       false);
            previewImage.build(context,
                               previewPanel.content(),
                               image(detail::rect(0.018f, 0.025f, 0.964f, 0.950f)));
            previewPlaceholder.build(context,
                                     previewPanel.content(),
                                     label("No preview", detail::rect(0.050f, 0.430f, 0.900f, 0.140f), MutedText,
                                           ToolTheme::smallTextScale, UiHorizontalAlign::Center));

            saveButton.build(context,
                             rightPane.content(),
                             button("SAVE", detail::rect(0.050f, 0.370f, 0.205f, 0.042f), ToolTheme::buttonTextScale));
            reloadButton.build(context,
                               rightPane.content(),
                               button("RELOAD", detail::rect(0.275f, 0.370f, 0.205f, 0.042f), ToolTheme::buttonTextScale));
            playButton.build(context,
                             rightPane.content(),
                             button("PAUSE", detail::rect(0.500f, 0.370f, 0.205f, 0.042f), ToolTheme::buttonTextScale));
            restartButton.build(context,
                                rightPane.content(),
                                button("RESTART", detail::rect(0.725f, 0.370f, 0.225f, 0.042f), ToolTheme::buttonTextScale));

            emitterToggleButton.build(context,
                                      rightPane.content(),
                                      button("ENABLE EMITTER", detail::rect(0.050f, 0.420f, 0.900f, 0.034f), ToolTheme::buttonTextScale));

            emitterHeader.build(context,
                                rightPane.content(),
                                label("Emitters", detail::rect(0.050f, 0.466f, 0.900f, 0.030f), Text, ToolTheme::smallTextScale));
            float rowY = 0.502f;
            for (auto& row : emitterRows)
            {
                row.build(context,
                          rightPane.content(),
                          button("--", detail::rect(0.050f, rowY, 0.900f, 0.034f), ToolTheme::smallTextScale, UiHorizontalAlign::Left));
                rowY += 0.039f;
            }

            moduleHeader.build(context,
                               rightPane.content(),
                               label("Modules", detail::rect(0.050f, 0.748f, 0.400f, 0.030f), Text, ToolTheme::smallTextScale));
            moduleToggleButton.build(context,
                                     rightPane.content(),
                                     button("ENABLE MODULE", detail::rect(0.520f, 0.744f, 0.430f, 0.034f), ToolTheme::buttonTextScale));
            rowY = 0.790f;
            for (auto& row : moduleRows)
            {
                row.build(context,
                          rightPane.content(),
                          button("--", detail::rect(0.050f, rowY, 0.900f, 0.030f), ToolTheme::smallTextScale, UiHorizontalAlign::Left));
                rowY += 0.034f;
            }

            detailHeader.build(context,
                               viewportFrame.content(),
                               label("Selection", detail::rect(0.018f, 0.014f, 0.350f, 0.030f), MutedText, ToolTheme::smallTextScale));
            float detailY = 0.052f;
            for (auto& line : detailLines)
            {
                line.build(context,
                           viewportFrame.content(),
                           label("", detail::rect(0.018f, detailY, 0.600f, 0.026f), MutedText, ToolTheme::smallTextScale));
                detailY += 0.030f;
            }
        }

        void buildBottomBar(gts::ui::UiWidgetContext& context)
        {
            statusLabel.build(context,
                              bottomBar.content(),
                              label("F6 TO HIDE", detail::rect(0.012f, 0.120f, 0.960f, 0.760f), MutedText, ToolTheme::smallTextScale));
        }

        void buildPanel(gts::ui::UiPanelWidget& panel,
                        gts::ui::UiWidgetContext& context,
                        UiHandle parent,
                        const UiLayoutSpec& layout,
                        UiColor color,
                        bool interactable)
        {
            gts::ui::UiPanelDesc desc;
            desc.layout = layout;
            desc.styleClass.clear();
            desc.enabled = true;
            desc.interactable = interactable;
            panel.build(context, parent, desc);
            detail::setRectPayload(context.ui, context.surface, panel.root(), color);
        }

        gts::ui::UiLabelDesc label(const std::string& text,
                                   const UiLayoutSpec& layout,
                                   UiColor color,
                                   float scale,
                                   UiHorizontalAlign align = UiHorizontalAlign::Left) const
        {
            gts::ui::UiLabelDesc desc;
            desc.layout = layout;
            desc.text = text;
            desc.font = font;
            desc.styleClass.clear();
            desc.color = color;
            desc.scale = scale;
            desc.horizontalAlign = align;
            desc.verticalAlign = UiVerticalAlign::Middle;
            desc.wrapMode = UiTextWrapMode::Word;
            desc.maxLines = 2;
            return desc;
        }

        gts::ui::UiButtonDesc button(const std::string& text,
                                     const UiLayoutSpec& layout,
                                     float scale,
                                     UiHorizontalAlign align = UiHorizontalAlign::Center) const
        {
            gts::ui::UiButtonDesc desc;
            desc.layout = layout;
            desc.text = text;
            desc.font = font;
            desc.styleClass.clear();
            desc.labelStyleClass.clear();
            desc.textColor = Text;
            desc.textScale = scale;
            desc.horizontalAlign = align;
            desc.verticalAlign = UiVerticalAlign::Middle;
            desc.wrapMode = UiTextWrapMode::Word;
            desc.maxLines = 2;
            return desc;
        }

        gts::ui::UiImageDesc image(const UiLayoutSpec& layout) const
        {
            gts::ui::UiImageDesc desc;
            desc.layout = layout;
            desc.tint = {1.0f, 1.0f, 1.0f, 0.0f};
            desc.visible = false;
            return desc;
        }

        void syncPanelLayouts(gts::ui::UiWidgetContext& context)
        {
            context.ui.setLayout(context.surface,
                                 topBar.root(),
                                 detail::rect(0.0f, 0.0f, 1.0f, view.topBarHeight));
            context.ui.setLayout(context.surface,
                                 viewportToolbar.root(),
                                 detail::rect(0.0f, view.topBarHeight, 1.0f, view.viewportToolbarHeight));
            context.ui.setLayout(context.surface,
                                 leftPane.root(),
                                 detail::rect(0.0f,
                                              view.viewportY,
                                              view.leftPaneWidth,
                                              1.0f - view.viewportY - view.bottomBarHeight));
            context.ui.setLayout(context.surface,
                                 rightPane.root(),
                                 detail::rect(1.0f - view.rightPaneWidth,
                                              view.topBarHeight,
                                              view.rightPaneWidth,
                                              1.0f - view.topBarHeight - view.bottomBarHeight));
            context.ui.setLayout(context.surface,
                                 bottomBar.root(),
                                 detail::rect(0.0f, 1.0f - view.bottomBarHeight, 1.0f, view.bottomBarHeight));
            context.ui.setLayout(context.surface,
                                 viewportFrame.root(),
                                 detail::rect(view.viewportX, view.viewportY, view.viewportWidth, view.viewportHeight));
        }

        void syncTopBar(gts::ui::UiWidgetContext& context)
        {
            sceneTabButton.setText(context, "SCENES");
            particleTabButton.setText(context, "PARTICLES");
            paintButton(context, sceneTabButton, view.activeTab == EngineToolTab::Scenes);
            paintButton(context, particleTabButton, view.activeTab == EngineToolTab::Particles);
        }

        void syncViewport(gts::ui::UiWidgetContext& context)
        {
            activeSceneLabel.setText(context,
                                     "Scene: " + (view.activeSceneName.empty()
                                         ? std::string("none")
                                         : detail::compact(view.activeSceneName, 48)));
            detailHeader.setText(context, view.activeTab == EngineToolTab::Scenes ? "Scene Selection" : "Particle Selection");
            for (size_t i = 0; i < detailLines.size(); ++i)
            {
                const bool visible = i < view.detailLines.size();
                detailLines[i].setVisible(context, visible);
                if (visible)
                    detailLines[i].setText(context, view.detailLines[i]);
            }
        }

        void syncLeftPane(gts::ui::UiWidgetContext& context)
        {
            sceneCount.setText(context, pageText(view.sceneOffset, view.scenes.size(), view.sceneTotal));
            for (size_t i = 0; i < sceneRows.size(); ++i)
            {
                const bool visible = i < view.scenes.size();
                syncButton(context,
                           sceneRows[i],
                           visible,
                           visible ? view.scenes[i].label : "--",
                           visible && !view.scenes[i].active,
                           visible && view.scenes[i].active);
            }
            syncButton(context, scenePrevButton, view.sceneOffset > 0, "PREV", view.sceneOffset > 0, false);
            syncButton(context,
                       sceneNextButton,
                       view.sceneOffset + view.scenes.size() < view.sceneTotal,
                       "NEXT",
                       view.sceneOffset + view.scenes.size() < view.sceneTotal,
                       false);

            effectCount.setText(context, pageText(view.effectOffset, view.effects.size(), view.effectTotal));
            for (size_t i = 0; i < effectRows.size(); ++i)
            {
                const bool visible = i < view.effects.size();
                syncButton(context,
                           effectRows[i],
                           visible,
                           visible ? view.effects[i].label : "--",
                           visible,
                           visible && view.effects[i].active);
            }
            syncButton(context, effectPrevButton, view.effectOffset > 0, "PREV", view.effectOffset > 0, false);
            syncButton(context,
                       effectNextButton,
                       view.effectOffset + view.effects.size() < view.effectTotal,
                       "NEXT",
                       view.effectOffset + view.effects.size() < view.effectTotal,
                       false);
        }

        void syncRightPane(gts::ui::UiWidgetContext& context)
        {
            particleTitle.setText(context, view.particleTitle + (view.particleDirty ? " *" : ""));
            particlePath.setText(context,
                                 view.particlePath.empty()
                                     ? std::string("Select a particle asset")
                                     : detail::compact(view.particlePath, 62));

            const bool hasPreview = view.previewTexture != 0;
            previewImage.setVisible(context, hasPreview);
            previewPlaceholder.setVisible(context, !hasPreview);
            UiImageData image;
            image.textureID = view.previewTexture;
            image.tint = {1.0f, 1.0f, 1.0f, hasPreview ? 1.0f : 0.0f};
            image.imageAspect = 1.0f;
            context.ui.setPayload(context.surface, previewImage.root(), image);

            syncButton(context, saveButton, true, "SAVE", view.particleLoaded, false);
            syncButton(context, reloadButton, true, "RELOAD", view.particleLoaded, false);
            syncButton(context, playButton, true, view.particlePlaying ? "PAUSE" : "PLAY", view.particleLoaded, false);
            syncButton(context, restartButton, true, "RESTART", view.particleLoaded, false);
            syncButton(context,
                       emitterToggleButton,
                       true,
                       view.selectedEmitterEnabled ? "DISABLE EMITTER" : "ENABLE EMITTER",
                       view.hasSelectedEmitter,
                       false);
            syncButton(context,
                       moduleToggleButton,
                       true,
                       view.selectedModuleEnabled ? "DISABLE MODULE" : "ENABLE MODULE",
                       view.hasSelectedModule,
                       false);

            for (size_t i = 0; i < emitterRows.size(); ++i)
            {
                const bool visible = i < view.emitters.size();
                const std::string text = visible
                    ? view.emitters[i].label + "  " + view.emitters[i].summary
                    : "--";
                syncButton(context, emitterRows[i], visible, text, visible, visible && view.emitters[i].active);
                if (visible && !view.emitters[i].enabled)
                    detail::setTextColor(context.ui, context.surface, emitterRows[i].label(), Danger);
            }

            for (size_t i = 0; i < moduleRows.size(); ++i)
            {
                const bool visible = i < view.modules.size();
                const std::string text = visible
                    ? view.modules[i].label + "  " + view.modules[i].summary
                    : "--";
                syncButton(context, moduleRows[i], visible, text, visible, visible && view.modules[i].active);
                if (visible && !view.modules[i].enabled)
                    detail::setTextColor(context.ui, context.surface, moduleRows[i].label(), Danger);
            }
        }

        void syncBottomBar(gts::ui::UiWidgetContext& context)
        {
            statusLabel.setText(context, view.status.empty() ? "F6 TO HIDE" : view.status);
        }

        void syncButton(gts::ui::UiWidgetContext& context,
                        gts::ui::UiButtonWidget& widget,
                        bool visible,
                        const std::string& text,
                        bool enabled,
                        bool active)
        {
            widget.setVisible(context, visible);
            widget.setEnabled(context, enabled);
            if (visible)
                widget.setText(context, text);
            paintButton(context, widget, active);
        }

        void paintButton(gts::ui::UiWidgetContext& context,
                         gts::ui::UiButtonWidget& widget,
                         bool active)
        {
            const UiNode* node = context.ui.findNode(context.surface, widget.root());
            if (node == nullptr)
                return;

            UiColor fill = active ? ButtonActive : Button;
            UiColor text = active ? ActiveText : Text;
            if (!node->state.enabled)
            {
                fill = ButtonDisabled;
                text = MutedText;
            }
            else if (node->state.pressed)
            {
                fill = ButtonPressed;
            }
            else if (node->state.hovered)
            {
                fill = active ? AccentStrong : ButtonHover;
            }

            detail::setRectPayload(context.ui, context.surface, widget.root(), fill);
            detail::setTextColor(context.ui, context.surface, widget.label(), text);
        }

        static std::string pageText(size_t offset, size_t visible, size_t total)
        {
            if (total == 0)
                return "0";
            return std::to_string(offset + 1) + "-" + std::to_string(offset + visible) + " / " + std::to_string(total);
        }

        void destroyWidgets(gts::ui::UiWidgetContext& context)
        {
            titleLabel.destroy(context);
            viewportLabel.destroy(context);
            activeSceneLabel.destroy(context);
            sceneHeader.destroy(context);
            sceneCount.destroy(context);
            effectHeader.destroy(context);
            effectCount.destroy(context);
            particleTitle.destroy(context);
            particlePath.destroy(context);
            previewPlaceholder.destroy(context);
            emitterHeader.destroy(context);
            moduleHeader.destroy(context);
            detailHeader.destroy(context);
            statusLabel.destroy(context);

            for (auto& line : detailLines)
                line.destroy(context);
            sceneTabButton.destroy(context);
            particleTabButton.destroy(context);
            scenePrevButton.destroy(context);
            sceneNextButton.destroy(context);
            effectPrevButton.destroy(context);
            effectNextButton.destroy(context);
            saveButton.destroy(context);
            reloadButton.destroy(context);
            playButton.destroy(context);
            restartButton.destroy(context);
            emitterToggleButton.destroy(context);
            moduleToggleButton.destroy(context);
            for (auto& row : sceneRows)
                row.destroy(context);
            for (auto& row : effectRows)
                row.destroy(context);
            for (auto& row : emitterRows)
                row.destroy(context);
            for (auto& row : moduleRows)
                row.destroy(context);
            previewImage.destroy(context);
            previewPanel.destroy(context);
            viewportFrame.destroy(context);
            bottomBar.destroy(context);
            rightPane.destroy(context);
            leftPane.destroy(context);
            viewportToolbar.destroy(context);
            topBar.destroy(context);
            rootTint.destroy(context);
        }
    };

    class EngineToolShellSystem : public ECSControllerSystem
    {
    public:
        void update(const EcsControllerContext& ctx) override
        {
            if (ctx.ui == nullptr)
                return;

            EngineToolStateComponent& state = ensureState(ctx.world);
            state.selectionChangedThisFrame = false;
            processVisibilityToggle(ctx, state);

            if (!state.visible)
            {
                deactivate(ctx, state);
                return;
            }

            setToolsInputContext(ctx, true);
            state.editorMode = EditorMode::Runtime;
            const EngineToolWorkspaceComponent& workspace = publishWorkspace(ctx, true);

            if (!ensureFont(ctx))
            {
                state.status = "TOOLS FONT NOT READY";
                return;
            }

            EngineToolShellComposition* shell = ensureComposition(*ctx.ui);
            if (shell == nullptr)
            {
                state.status = "TOOLS UI NOT READY";
                return;
            }

            processCommands(ctx, state, *shell);
            syncParticlePreviewWorld(ctx);

            EngineToolShellView view = buildView(ctx, state, workspace);
            shell->setView(view);
            ctx.ui->updateComposition(shellComposition);

            publishParticlePreview(ctx, *shell);
            if (view.previewTexture != lastPreviewTexture)
            {
                view.previewTexture = lastPreviewTexture;
                shell->setView(view);
                ctx.ui->updateComposition(shellComposition);
            }

            updateInputCapture(ctx.world, ctx);
        }

        void prepareVisibility(const EcsControllerContext& ctx)
        {
            EngineToolStateComponent& state = ensureState(ctx.world);
            state.selectionChangedThisFrame = false;
            processVisibilityToggle(ctx, state);
        }

        void shutdown()
        {
            if (previewWorld)
                previewWorld->destroy();
        }

    private:
        static constexpr const char* ToolsInputContext = "engine.tools";
        static constexpr const char* ToolsSelectAction = "engine.tools_select";

        static constexpr size_t ScenePageSize = 6;
        static constexpr size_t EffectPageSize = 8;
        static constexpr size_t EmitterPageSize = 6;
        static constexpr size_t ModulePageSize = 6;

        BitmapFont font;
        bool fontReady = false;
        bool toolsContextRequested = false;
        uint64_t lastVisibilityToggleFrame = std::numeric_limits<uint64_t>::max();
        UiCompositionId shellComposition = UI_INVALID_COMPOSITION;
        EngineToolTab activeTab = EngineToolTab::Particles;

        std::vector<std::string> effectPaths;
        size_t sceneOffset = 0;
        size_t effectOffset = 0;
        size_t selectedEmitterIndex = 0;
        size_t selectedModuleIndex = 0;
        bool hasParticleAsset = false;
        bool particleDirty = false;
        bool playbackPaused = false;
        float playbackTimeScale = 1.0f;
        std::string currentParticlePath;
        ParticleEffectAsset currentParticleAsset;
        texture_id_type lastPreviewTexture = 0;
        std::unique_ptr<ParticlePreviewWorld> previewWorld = std::make_unique<ParticlePreviewWorld>();

        static EngineToolStateComponent& ensureState(ECSWorld& world)
        {
            if (!world.hasAny<EngineToolStateComponent>())
                return world.createSingleton<EngineToolStateComponent>();
            return world.getSingleton<EngineToolStateComponent>();
        }

        static EngineToolInputCaptureComponent& ensureInputCapture(ECSWorld& world)
        {
            if (!world.hasAny<EngineToolInputCaptureComponent>())
                return world.createSingleton<EngineToolInputCaptureComponent>();
            return world.getSingleton<EngineToolInputCaptureComponent>();
        }

        bool processVisibilityToggle(const EcsControllerContext& ctx, EngineToolStateComponent& state)
        {
            if (ctx.input == nullptr || !ctx.input->isPressed("engine.tools_toggle"))
                return false;

            const uint64_t frame = ctx.time == nullptr ? 0u : ctx.time->frame;
            if (lastVisibilityToggleFrame == frame)
                return false;

            lastVisibilityToggleFrame = frame;
            state.visible = !state.visible;
            state.status = state.visible ? "TOOLS VISIBLE" : "TOOLS HIDDEN";
            setToolsInputContext(ctx, state.visible);
            if (!state.visible)
                deactivate(ctx, state);
            return true;
        }

        void deactivate(const EcsControllerContext& ctx, EngineToolStateComponent& state)
        {
            state.editorMode = EditorMode::Runtime;
            setToolsInputContext(ctx, false);
            clearInputCapture(ctx.world);
            clearPreviewRender(ctx.world);
            if (previewWorld)
                previewWorld->destroy();
            if (ctx.ui != nullptr)
                destroyComposition(*ctx.ui);
            publishWorkspace(ctx, false);
        }

        void setToolsInputContext(const EcsControllerContext& ctx, bool enabled)
        {
            if (ctx.input == nullptr)
                return;

            if (enabled)
            {
                if (!toolsContextRequested && !ctx.input->isContextActive(ToolsInputContext))
                    ctx.input->pushContext(ToolsInputContext);
                toolsContextRequested = true;
                return;
            }

            if (toolsContextRequested || ctx.input->isContextActive(ToolsInputContext))
                ctx.input->popContext(ToolsInputContext);
            toolsContextRequested = false;
        }

        static void clearInputCapture(ECSWorld& world)
        {
            EngineToolInputCaptureComponent& capture = ensureInputCapture(world);
            capture = {};
        }

        static EngineToolWorkspaceComponent& publishWorkspace(const EcsControllerContext& ctx, bool active)
        {
            const int width = std::max(1, static_cast<int>(std::round(ctx.windowPixelWidth)));
            const int height = std::max(1, static_cast<int>(std::round(ctx.windowPixelHeight)));
            return publishEngineToolWorkspace(ctx.world, width, height, active);
        }

        bool ensureFont(const EcsControllerContext& ctx)
        {
            if (fontReady)
                return true;
            if (ctx.resources == nullptr)
                return false;

            const font_id_type fontID =
                ctx.resources->requestFont(GraphicsConstants::ENGINE_RESOURCES + "/fonts/gravitasfont.font.json");
            const BitmapFont* loadedFont = ctx.resources->getFont(fontID);
            if (loadedFont == nullptr)
                return false;

            font = *loadedFont;
            tuneEditorFont(font);
            fontReady = true;
            return true;
        }

        static void tuneEditorFont(BitmapFont& inFont)
        {
            inFont.lineHeight = 0.92f;
            for (auto& glyphEntry : inFont.glyphs)
            {
                GlyphInfo& glyph = glyphEntry.second;
                glyph.advance = 0.62f;
                glyph.size.x = 0.82f;
                glyph.size.y = 0.92f;

                const char ch = glyphEntry.first;
                if (ch == ' ')
                    glyph.advance = 0.34f;
                else if (ch == 'i' || ch == 'l' || ch == 'I' || ch == '!' || ch == '.' || ch == ',' || ch == ':' ||
                         ch == ';' || ch == '\'' || ch == '`')
                    glyph.advance = 0.38f;
                else if (ch == 'm' || ch == 'w' || ch == 'M' || ch == 'W')
                    glyph.advance = 0.76f;
                else if (ch >= 'A' && ch <= 'Z')
                    glyph.advance = 0.66f;
            }
        }

        EngineToolShellComposition* ensureComposition(UiSystem& ui)
        {
            if (shellComposition != UI_INVALID_COMPOSITION)
            {
                if (auto* shell = dynamic_cast<EngineToolShellComposition*>(ui.findComposition(shellComposition)))
                    return shell;
                shellComposition = UI_INVALID_COMPOSITION;
            }

            UiMountDesc desc;
            desc.name = "engine-tool-shell";
            shellComposition = ui.mountComposition(std::make_unique<EngineToolShellComposition>(&font), desc);
            return dynamic_cast<EngineToolShellComposition*>(ui.findComposition(shellComposition));
        }

        void destroyComposition(UiSystem& ui)
        {
            if (shellComposition == UI_INVALID_COMPOSITION)
                return;
            ui.destroyComposition(shellComposition);
            shellComposition = UI_INVALID_COMPOSITION;
        }

        void processCommands(const EcsControllerContext& ctx,
                             EngineToolStateComponent& state,
                             EngineToolShellComposition& shell)
        {
            for (const EngineToolCommand& command : shell.consumeCommands())
            {
                switch (command.type)
                {
                    case EngineToolCommandType::SetTab:
                        activeTab = command.tab;
                        state.activePanelIndex = activeTab == EngineToolTab::Particles ? 1u : 0u;
                        state.status = activeTab == EngineToolTab::Particles ? "PARTICLE EDITOR" : "SCENE SELECTION";
                        break;

                    case EngineToolCommandType::LoadScene:
                        requestSceneChange(ctx, state, command.value);
                        break;

                    case EngineToolCommandType::ScenePagePrevious:
                        sceneOffset = sceneOffset < ScenePageSize ? 0 : sceneOffset - ScenePageSize;
                        break;

                    case EngineToolCommandType::ScenePageNext:
                        sceneOffset += ScenePageSize;
                        break;

                    case EngineToolCommandType::OpenParticleEffect:
                        openParticleEffect(command.value, state, true, false);
                        activeTab = EngineToolTab::Particles;
                        break;

                    case EngineToolCommandType::EffectPagePrevious:
                        effectOffset = effectOffset < EffectPageSize ? 0 : effectOffset - EffectPageSize;
                        break;

                    case EngineToolCommandType::EffectPageNext:
                        effectOffset += EffectPageSize;
                        break;

                    case EngineToolCommandType::SaveParticleEffect:
                        saveParticleEffect(state);
                        break;

                    case EngineToolCommandType::ReloadParticleEffect:
                        reloadParticleEffect(state);
                        break;

                    case EngineToolCommandType::ToggleParticlePlayback:
                        if (hasParticleAsset)
                        {
                            playbackPaused = !playbackPaused;
                            state.status = playbackPaused ? "PARTICLE PREVIEW PAUSED" : "PARTICLE PREVIEW PLAYING";
                        }
                        break;

                    case EngineToolCommandType::RestartParticlePreview:
                        if (previewWorld)
                            previewWorld->resetSimulation();
                        state.status = "PARTICLE PREVIEW RESTARTED";
                        break;

                    case EngineToolCommandType::ToggleEmitterEnabled:
                        toggleSelectedEmitterEnabled(state);
                        break;

                    case EngineToolCommandType::ToggleModuleEnabled:
                        toggleSelectedModuleEnabled(state);
                        break;

                    case EngineToolCommandType::SelectEmitter:
                        selectedEmitterIndex = command.index;
                        selectedModuleIndex = 0;
                        clampParticleSelection();
                        break;

                    case EngineToolCommandType::SelectModule:
                        selectedModuleIndex = command.index;
                        clampParticleSelection();
                        break;
                }
            }
        }

        void requestSceneChange(const EcsControllerContext& ctx,
                                EngineToolStateComponent& state,
                                const std::string& sceneId)
        {
            const std::string activeScene = ctx.activeSceneName == nullptr ? "" : *ctx.activeSceneName;
            if (sceneId.empty() || sceneId == activeScene)
            {
                state.status = "SCENE ACTIVE";
                return;
            }
            if (ctx.engineCommands == nullptr)
            {
                state.status = "NO SCENE COMMANDS";
                return;
            }

            ctx.engineCommands->requestChangeScene(sceneId);
            state.status = "LOADING " + sceneId;
        }

        void openParticleEffect(const std::string& path,
                                EngineToolStateComponent& state,
                                bool protectDirty,
                                bool forceReload)
        {
            if (path.empty())
                return;
            if (protectDirty && particleDirty && path != currentParticlePath)
            {
                state.status = "SAVE OR RELOAD PARTICLE EFFECT FIRST";
                return;
            }
            if (!forceReload && path == currentParticlePath && hasParticleAsset)
                return;

            ParticleEffectAsset loaded;
            if (!gts::particles::loadParticleEffectAsset(path, loaded))
            {
                state.status = "OPEN PARTICLE EFFECT FAILED";
                return;
            }

            gts::particles::compileParticleEffectAsset(loaded);
            currentParticleAsset = std::move(loaded);
            currentParticlePath = path;
            hasParticleAsset = true;
            particleDirty = false;
            playbackPaused = false;
            playbackTimeScale = 1.0f;
            selectedEmitterIndex = 0;
            selectedModuleIndex = 0;
            if (previewWorld)
                previewWorld->resetSimulation();
            state.status = "OPENED " + detail::fileName(path);
        }

        void saveParticleEffect(EngineToolStateComponent& state)
        {
            if (!hasParticleAsset || currentParticlePath.empty())
            {
                state.status = "NO PARTICLE EFFECT SELECTED";
                return;
            }

            ParticleEffectAsset asset = particleAssetForSave();
            if (!gts::particles::saveParticleEffectAsset(currentParticlePath, asset))
            {
                state.status = "SAVE PARTICLE EFFECT FAILED";
                return;
            }

            currentParticleAsset = std::move(asset);
            particleDirty = false;
            state.status = "SAVED " + detail::fileName(currentParticlePath);
        }

        void reloadParticleEffect(EngineToolStateComponent& state)
        {
            if (currentParticlePath.empty())
            {
                state.status = "NO PARTICLE EFFECT SELECTED";
                return;
            }
            const std::string path = currentParticlePath;
            openParticleEffect(path, state, false, true);
            state.status = "RELOADED " + detail::fileName(path);
        }

        void toggleSelectedEmitterEnabled(EngineToolStateComponent& state)
        {
            ParticleEffectEmitter* emitter = selectedEmitter();
            if (emitter == nullptr)
            {
                state.status = "NO EMITTER SELECTED";
                return;
            }

            emitter->descriptor.enabled = !emitter->descriptor.enabled;
            recompileSelectedEmitter();
            particleDirty = true;
            if (previewWorld)
                previewWorld->resetSimulation();
            state.status = emitter->descriptor.enabled ? "EMITTER ENABLED" : "EMITTER DISABLED";
        }

        void toggleSelectedModuleEnabled(EngineToolStateComponent& state)
        {
            ParticleEffectEmitter* emitter = selectedEmitter();
            if (emitter == nullptr || selectedModuleIndex >= emitter->modules.size())
            {
                state.status = "NO MODULE SELECTED";
                return;
            }

            gts::particles::ParticleModuleInstance& module = emitter->modules[selectedModuleIndex];
            module.enabled = !module.enabled;
            recompileSelectedEmitter();
            particleDirty = true;
            if (previewWorld)
                previewWorld->resetSimulation();
            state.status = module.enabled ? "MODULE ENABLED" : "MODULE DISABLED";
        }

        ParticleEffectEmitter* selectedEmitter()
        {
            if (!hasParticleAsset || selectedEmitterIndex >= currentParticleAsset.emitters.size())
                return nullptr;
            return &currentParticleAsset.emitters[selectedEmitterIndex];
        }

        const ParticleEffectEmitter* selectedEmitter() const
        {
            if (!hasParticleAsset || selectedEmitterIndex >= currentParticleAsset.emitters.size())
                return nullptr;
            return &currentParticleAsset.emitters[selectedEmitterIndex];
        }

        void recompileSelectedEmitter()
        {
            ParticleEffectEmitter* emitter = selectedEmitter();
            if (emitter == nullptr)
                return;

            gts::particles::migrateParticleEmitterModules(emitter->modules, emitter->descriptor);
            gts::particles::syncParticleGraphWithModules(*emitter);
            emitter->compiledProgram = gts::particles::compileParticleEffectEmitter(*emitter);
            if (emitter->compiledProgram.valid)
                emitter->descriptor = emitter->compiledProgram.runtimeDescriptor;
            emitter->descriptor.schemaVersion = CurrentParticleEmitterSchemaVersion;
        }

        ParticleEffectAsset particleAssetForSave() const
        {
            ParticleEffectAsset asset = currentParticleAsset;
            asset.schemaVersion = CurrentParticleEffectSchemaVersion;
            if (asset.metadata.name.empty())
                asset.metadata.name = std::filesystem::path(currentParticlePath).stem().string();

            for (ParticleEffectEmitter& emitter : asset.emitters)
            {
                gts::particles::migrateParticleEmitterModules(emitter.modules, emitter.descriptor);
                gts::particles::syncParticleGraphWithModules(emitter);
                emitter.compiledProgram = gts::particles::compileParticleEffectEmitter(emitter);
                if (emitter.compiledProgram.valid)
                    emitter.descriptor = emitter.compiledProgram.runtimeDescriptor;
                emitter.descriptor.effectPath.clear();
                emitter.descriptor.effectEmitterId.clear();
                emitter.descriptor.schemaVersion = CurrentParticleEmitterSchemaVersion;
                emitter.compiledProgram.runtimeDescriptor.effectPath.clear();
                emitter.compiledProgram.runtimeDescriptor.effectEmitterId.clear();
            }
            return asset;
        }

        EngineToolShellView buildView(const EcsControllerContext& ctx,
                                      EngineToolStateComponent& state,
                                      const EngineToolWorkspaceComponent& workspace)
        {
            collectEffectPaths(ctx.world);
            clampPaging(ctx);
            clampParticleSelection();

            EngineToolShellView view;
            view.activeTab = activeTab;
            view.topBarHeight = workspace.topBarHeight;
            view.viewportToolbarHeight = workspace.viewportToolbarHeight;
            view.leftPaneWidth = workspace.leftRailWidth;
            view.rightPaneWidth = workspace.rightPaneWidth;
            view.bottomBarHeight = workspace.bottomBarHeight;
            view.viewportX = workspace.viewportX;
            view.viewportY = workspace.viewportY;
            view.viewportWidth = workspace.viewportWidth;
            view.viewportHeight = workspace.viewportHeight;
            view.activeSceneName = ctx.activeSceneName == nullptr ? "" : *ctx.activeSceneName;
            view.status = state.status;
            view.sceneOffset = sceneOffset;
            view.effectOffset = effectOffset;
            view.previewTexture = lastPreviewTexture;
            view.particleLoaded = hasParticleAsset;
            view.particleDirty = particleDirty;
            view.particlePlaying = !playbackPaused;
            view.particlePath = currentParticlePath;
            view.particleTitle = particleTitleText();
            if (const ParticleEffectEmitter* emitter = selectedEmitter())
            {
                view.hasSelectedEmitter = true;
                view.selectedEmitterEnabled = emitter->descriptor.enabled;
                if (selectedModuleIndex < emitter->modules.size())
                {
                    view.hasSelectedModule = true;
                    view.selectedModuleEnabled = emitter->modules[selectedModuleIndex].enabled;
                }
            }

            fillSceneRows(ctx, view);
            fillEffectRows(view);
            fillParticleRows(view);
            fillDetailLines(view);
            return view;
        }

        void fillSceneRows(const EcsControllerContext& ctx, EngineToolShellView& view)
        {
            const std::vector<RegisteredSceneInfo>* scenes = ctx.registeredScenes;
            const std::string activeScene = ctx.activeSceneName == nullptr ? "" : *ctx.activeSceneName;
            view.sceneTotal = scenes == nullptr ? 0 : scenes->size();
            if (scenes == nullptr)
                return;

            const size_t end = std::min(scenes->size(), sceneOffset + ScenePageSize);
            for (size_t i = sceneOffset; i < end; ++i)
            {
                const RegisteredSceneInfo& scene = (*scenes)[i];
                const std::string display = scene.displayName.empty() ? scene.id : scene.displayName;
                EngineToolSceneRow row;
                row.id = scene.id;
                row.active = scene.id == activeScene;
                row.label = (row.active ? "ACTIVE  " : "LOAD    ") + detail::compact(display, 28);
                view.scenes.push_back(std::move(row));
            }
        }

        void fillEffectRows(EngineToolShellView& view)
        {
            view.effectTotal = effectPaths.size();
            const size_t end = std::min(effectPaths.size(), effectOffset + EffectPageSize);
            for (size_t i = effectOffset; i < end; ++i)
            {
                EngineToolEffectRow row;
                row.path = effectPaths[i];
                row.active = row.path == currentParticlePath;
                row.label = (row.active ? "OPEN    " : "LOAD    ") + detail::compact(detail::stemName(row.path), 30);
                view.effects.push_back(std::move(row));
            }
        }

        void fillParticleRows(EngineToolShellView& view) const
        {
            if (!hasParticleAsset)
                return;

            const size_t emitterEnd = std::min(currentParticleAsset.emitters.size(), EmitterPageSize);
            for (size_t i = 0; i < emitterEnd; ++i)
            {
                const ParticleEffectEmitter& emitter = currentParticleAsset.emitters[i];
                EngineToolEmitterRow row;
                row.index = i;
                row.active = i == selectedEmitterIndex;
                row.enabled = emitter.descriptor.enabled;
                row.label = detail::compact(emitter.name.empty() ? emitter.stableId : emitter.name, 22);
                row.summary = emitter.descriptor.enabled ? "on" : "off";
                if (!emitter.compiledProgram.valid)
                    row.summary += " invalid";
                else
                    row.summary += " " + std::to_string(emitter.compiledProgram.modules.size()) + " modules";
                view.emitters.push_back(std::move(row));
            }

            if (selectedEmitterIndex >= currentParticleAsset.emitters.size())
                return;

            const ParticleEffectEmitter& emitter = currentParticleAsset.emitters[selectedEmitterIndex];
            const size_t moduleEnd = std::min(emitter.modules.size(), ModulePageSize);
            for (size_t i = 0; i < moduleEnd; ++i)
            {
                const gts::particles::ParticleModuleInstance& module = emitter.modules[i];
                const gts::particles::ParticleModuleDefinition* definition =
                    gts::particles::findParticleModuleDefinition(module.typeId);

                EngineToolModuleRow row;
                row.index = i;
                row.active = i == selectedModuleIndex;
                row.enabled = module.enabled;
                row.label = detail::compact(module.displayName.empty() ? module.typeId : module.displayName, 22);
                row.summary = definition == nullptr
                    ? std::string("custom")
                    : std::string(gts::particles::executionStageShortLabel(definition->executionStage));
                row.summary += " " + std::to_string(module.parameters.size()) + " params";
                view.modules.push_back(std::move(row));
            }
        }

        void fillDetailLines(EngineToolShellView& view) const
        {
            if (activeTab == EngineToolTab::Scenes)
            {
                view.detailLines.push_back("Active scene: " + (view.activeSceneName.empty() ? "none" : view.activeSceneName));
                view.detailLines.push_back("Registered scenes: " + std::to_string(view.sceneTotal));
                view.detailLines.push_back("Select a scene from the left browser to request a scene change.");
                view.detailLines.push_back("The center viewport renders the selected runtime scene.");
                return;
            }

            if (!hasParticleAsset)
            {
                view.detailLines.push_back("Select a particle asset from the left browser.");
                view.detailLines.push_back("The scene viewport remains live while the preview renders in the inspector.");
                return;
            }

            view.detailLines.push_back("Effect: " + particleTitleText());
            view.detailLines.push_back("Emitters: " + std::to_string(currentParticleAsset.emitters.size()));

            if (selectedEmitterIndex >= currentParticleAsset.emitters.size())
                return;

            const ParticleEffectEmitter& emitter = currentParticleAsset.emitters[selectedEmitterIndex];
            view.detailLines.push_back("Emitter: " + (emitter.name.empty() ? emitter.stableId : emitter.name));
            view.detailLines.push_back("Max particles: " + std::to_string(emitter.descriptor.maxParticles));
            view.detailLines.push_back("Emission rate: " + detail::fixed(emitter.descriptor.emissionRate));
            view.detailLines.push_back("Lifetime: " + detail::fixed(emitter.descriptor.lifetimeMin) + "-"
                                       + detail::fixed(emitter.descriptor.lifetimeMax));

            if (selectedModuleIndex < emitter.modules.size())
            {
                const gts::particles::ParticleModuleInstance& module = emitter.modules[selectedModuleIndex];
                view.detailLines.push_back("Module: " + (module.displayName.empty() ? module.typeId : module.displayName));
                for (size_t i = 0; i < std::min<size_t>(module.parameters.size(), 3); ++i)
                    view.detailLines.push_back(parameterSummary(module.parameters[i]));
            }
        }

        std::string particleTitleText() const
        {
            if (!hasParticleAsset)
                return "No Effect";
            if (!currentParticleAsset.metadata.name.empty())
                return detail::compact(currentParticleAsset.metadata.name, 36);
            return detail::compact(detail::stemName(currentParticlePath), 36);
        }

        static std::string parameterSummary(const gts::particles::ParticleModuleParameter& parameter)
        {
            using Type = gts::particles::ParticleModuleParameterType;
            std::string value;
            switch (parameter.type)
            {
                case Type::Float:
                    value = detail::fixed(parameter.floatValue);
                    break;
                case Type::UInt:
                case Type::Enum:
                    value = std::to_string(parameter.uintValue);
                    break;
                case Type::Bool:
                    value = parameter.boolValue ? "true" : "false";
                    break;
                case Type::String:
                    value = parameter.stringValue.empty() ? "<empty>" : detail::compact(parameter.stringValue, 22);
                    break;
                case Type::FloatCurve:
                    value = std::to_string(parameter.floatCurveValue.size()) + " keys";
                    break;
                case Type::ColorGradient:
                    value = std::to_string(parameter.colorGradientValue.size()) + " colors";
                    break;
                case Type::BurstTimeline:
                    value = std::to_string(parameter.burstTimelineValue.size()) + " bursts";
                    break;
            }
            return detail::compact(parameter.id, 18) + ": " + value;
        }

        void collectEffectPaths(ECSWorld& world)
        {
            effectPaths.clear();

            if (world.hasAny<ParticleEffectRegistryComponent>())
            {
                const ParticleEffectRegistryComponent& registry = world.getSingleton<ParticleEffectRegistryComponent>();
                for (const auto& entry : registry.effects)
                    effectPaths.push_back(entry.first);
            }

            world.forEach<ParticleEmitterComponent>(
                [&](Entity, ParticleEmitterComponent& emitter)
                {
                    if (!emitter.effectPath.empty())
                        effectPaths.push_back(emitter.effectPath);
                });

            scanParticleEffectDirectory("resources/particles");
            scanParticleEffectDirectory("../resources/particles");
            scanParticleEffectDirectory("../../resources/particles");

            if (!currentParticlePath.empty())
                effectPaths.push_back(currentParticlePath);

            std::sort(effectPaths.begin(), effectPaths.end());
            effectPaths.erase(std::unique(effectPaths.begin(), effectPaths.end()), effectPaths.end());
        }

        void scanParticleEffectDirectory(const std::filesystem::path& rootPath)
        {
            std::error_code ec;
            if (!std::filesystem::exists(rootPath, ec) || !std::filesystem::is_directory(rootPath, ec))
                return;

            const std::filesystem::recursive_directory_iterator end;
            std::filesystem::recursive_directory_iterator it(
                rootPath,
                std::filesystem::directory_options::skip_permission_denied,
                ec);
            while (!ec && it != end)
            {
                const std::filesystem::directory_entry& entry = *it;
                if (entry.is_regular_file(ec) && entry.path().extension() == ".json")
                    effectPaths.push_back(entry.path().generic_string());
                it.increment(ec);
            }
        }

        void clampPaging(const EcsControllerContext& ctx)
        {
            const size_t sceneTotal = ctx.registeredScenes == nullptr ? 0 : ctx.registeredScenes->size();
            if (sceneTotal == 0)
                sceneOffset = 0;
            else if (sceneOffset >= sceneTotal)
                sceneOffset = ((sceneTotal - 1) / ScenePageSize) * ScenePageSize;

            if (effectPaths.empty())
                effectOffset = 0;
            else if (effectOffset >= effectPaths.size())
                effectOffset = ((effectPaths.size() - 1) / EffectPageSize) * EffectPageSize;
        }

        void clampParticleSelection()
        {
            if (!hasParticleAsset || currentParticleAsset.emitters.empty())
            {
                selectedEmitterIndex = 0;
                selectedModuleIndex = 0;
                return;
            }

            selectedEmitterIndex = std::min(selectedEmitterIndex, currentParticleAsset.emitters.size() - 1);
            const ParticleEffectEmitter& emitter = currentParticleAsset.emitters[selectedEmitterIndex];
            if (emitter.modules.empty())
                selectedModuleIndex = 0;
            else
                selectedModuleIndex = std::min(selectedModuleIndex, emitter.modules.size() - 1);
        }

        void syncParticlePreviewWorld(const EcsControllerContext& ctx)
        {
            if (!hasParticleAsset || currentParticlePath.empty() || !previewWorld)
                return;

            previewWorld->ensure(ctx.resources);
            previewWorld->syncAsset(currentParticlePath,
                                    currentParticleAsset,
                                    playbackTimeScale,
                                    playbackPaused);
        }

        void publishParticlePreview(const EcsControllerContext& ctx, EngineToolShellComposition& shell)
        {
            if (!hasParticleAsset || currentParticlePath.empty() || !previewWorld)
            {
                clearPreviewRender(ctx.world);
                lastPreviewTexture = 0;
                return;
            }

            uint32_t width = 320;
            uint32_t height = 240;
            if (ctx.ui != nullptr)
            {
                const UiNode* node = ctx.ui->findNode(shell.previewImageHandle());
                if (node != nullptr)
                {
                    width = std::max(1u,
                                     static_cast<uint32_t>(
                                         std::round(node->computedLayout.bounds.width *
                                                    std::max(1.0f, ctx.windowPixelWidth))));
                    height = std::max(1u,
                                      static_cast<uint32_t>(
                                          std::round(node->computedLayout.bounds.height *
                                                     std::max(1.0f, ctx.windowPixelHeight))));
                }
            }

            texture_id_type previousTexture = 0;
            if (ctx.world.hasAny<EditorPreviewRenderComponent>())
                previousTexture = ctx.world.getSingleton<EditorPreviewRenderComponent>().data.colorTextureID;

            const float dt = ctx.time == nullptr ? 1.0f / 60.0f : ctx.time->unscaledDeltaTime;
            EditorPreviewRenderData data = previewWorld->buildFrame(currentParticleAsset,
                                                                     dt,
                                                                     !playbackPaused,
                                                                     playbackTimeScale,
                                                                     width,
                                                                     height);
            data.colorTextureID = previousTexture;

            EditorPreviewRenderComponent* component = nullptr;
            if (ctx.world.hasAny<EditorPreviewRenderComponent>())
                component = &ctx.world.getSingleton<EditorPreviewRenderComponent>();
            else
                component = &ctx.world.createSingleton<EditorPreviewRenderComponent>();

            component->data = std::move(data);
            lastPreviewTexture = component->data.colorTextureID;
        }

        static void clearPreviewRender(ECSWorld& world)
        {
            if (world.hasAny<EditorPreviewRenderComponent>())
                world.getSingleton<EditorPreviewRenderComponent>().data.enabled = false;
        }

        void updateInputCapture(ECSWorld& world, const EcsControllerContext& ctx)
        {
            EngineToolInputCaptureComponent& capture = ensureInputCapture(world);
            const UiDispatchResult& dispatch = ctx.ui == nullptr ? UiDispatchResult{} : ctx.ui->dispatchResult();

            capture.pointerOverToolUi = dispatch.hovered != UI_INVALID_HANDLE;
            capture.toolUiPressed = dispatch.pressed != UI_INVALID_HANDLE;
            capture.keyboardCaptured = dispatch.keyboardConsumed || dispatch.navigationConsumed || dispatch.textInputConsumed;
            capture.worldConsumed = false;
            capture.pointerX = dispatch.pointerX;
            capture.pointerY = dispatch.pointerY;
            capture.hoveredUi = dispatch.hovered;
            capture.pressedUi = dispatch.pressed;

            if (ctx.input == nullptr)
            {
                capture.primaryDown = false;
                capture.primaryPressed = false;
                capture.primaryReleased = false;
                capture.pointerOverViewport = false;
                capture.viewportPointerX = 0.0f;
                capture.viewportPointerY = 0.0f;
                return;
            }

            RenderViewportRect viewport =
                RenderViewportRect::full(std::max(1, static_cast<int>(std::round(ctx.windowPixelWidth))),
                                         std::max(1, static_cast<int>(std::round(ctx.windowPixelHeight))));
            if (world.hasAny<RenderViewportComponent>())
                viewport = world.getSingleton<RenderViewportComponent>().sceneViewport;

            capture.pointerOverViewport = viewport.remapPointer(static_cast<float>(ctx.input->mouseX()),
                                                                static_cast<float>(ctx.input->mouseY()),
                                                                capture.viewportPointerX,
                                                                capture.viewportPointerY);

            const char* primaryAction =
                ctx.input->isContextActive(ToolsInputContext) ? ToolsSelectAction : "engine.ui_primary";
            capture.primaryDown = ctx.input->isHeld(primaryAction);
            capture.primaryPressed = ctx.input->isPressed(primaryAction);
            capture.primaryReleased = ctx.input->isReleased(primaryAction);
        }
    };
} // namespace gts::tools
