#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cctype>
#include <filesystem>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "EngineToolPanel.h"
#include "EngineToolInputCaptureComponent.h"
#include "GtsKey.h"
#include "InputBindingRegistry.h"
#include "ParticleEffectAsset.h"
#include "ParticleEffectAssetIO.h"
#include "ParticleEmitterComponent.h"
#include "ParticleEmitterRuntimeComponent.h"
#include "ToolTheme.h"
#include "ToolWidgets.h"

namespace gts::tools
{
    using gts::particles::buildModulesFromEmitterDescriptor;
    using gts::particles::executionStageLabel;
    using gts::particles::executionStageShortLabel;
    using gts::particles::findParticleModuleDefinition;
    using gts::particles::ParticleModuleDefinition;
    using gts::particles::ParticleModuleEnumOption;
    using gts::particles::ParticleModuleInstance;
    using gts::particles::ParticleModuleParameter;
    using gts::particles::ParticleModuleParameterDefinition;
    using gts::particles::ParticleModuleParameterType;

    class ParticleEffectEditorPanel : public EngineToolPanel
    {
        public:
        std::string_view id() const override
        {
            return "particle_effects";
        }

        std::string_view title() const override
        {
            return "Particle FX";
        }

        void build(EngineToolContext& ctx, UiHandle parent, BitmapFont* font) override
        {
            root            = createContainerRelative(ctx.ui, parent, {0.0f, 0.0f, 1.0f, 1.0f});
            header          = createTextRelative(ctx.ui,
                                                 root,
                                                 {0.0f, 0.000f, 1.0f, 0.026f},
                                                 font,
                                                 "PARTICLE EFFECTS",
                                                 ToolTheme::text,
                                                 ToolTheme::headerTextScale);
            summary         = createTextRelative(ctx.ui,
                                                 root,
                                                 {0.0f, 0.030f, 1.0f, 0.028f},
                                                 font,
                                                 "NO EFFECT",
                                                 ToolTheme::mutedText,
                                                 ToolTheme::smallTextScale);
            effectNameField = createTextField(ctx.ui, root, 0.058f, "EFFECT", font);

            for (size_t i = 0; i < EffectRowCount; ++i)
            {
                const float y = 0.092f + static_cast<float>(i) * 0.031f;
                effectRows.push_back(createButtonRelative(
                    ctx.ui, root, {0.0f, y, 1.0f, 0.029f}, font, "EMPTY", ToolTheme::buttonTextScale));
            }

            addEffectButtonRow(ctx.ui,
                               font,
                               0.224f,
                               {{EffectAction::Save, "SAVE"},
                                {EffectAction::SaveAs, "SAVE AS"},
                                {EffectAction::Duplicate, "COPY"},
                                {EffectAction::Reload, "RELOAD"}});

            emitterHeader    = createTextRelative(ctx.ui,
                                                  root,
                                                  {0.0f, 0.270f, 1.0f, 0.026f},
                                                  font,
                                                  "EMITTERS",
                                                  ToolTheme::text,
                                                  ToolTheme::headerTextScale);
            emitterNameField = createTextField(ctx.ui, root, 0.298f, "NAME", font);
            for (size_t i = 0; i < EmitterRowCount; ++i)
            {
                const float y = 0.333f + static_cast<float>(i) * 0.031f;
                emitterRows.push_back(createButtonRelative(
                    ctx.ui, root, {0.0f, y, 1.0f, 0.029f}, font, "EMPTY", ToolTheme::buttonTextScale));
            }

            addEmitterButtonRow(ctx.ui,
                                font,
                                0.430f,
                                {{EmitterAction::Add, "ADD"},
                                 {EmitterAction::Delete, "DEL"},
                                 {EmitterAction::Duplicate, "COPY"},
                                 {EmitterAction::Rename, "NAME"}});
            addEmitterButtonRow(ctx.ui,
                                font,
                                0.473f,
                                {{EmitterAction::MoveUp, "UP"},
                                 {EmitterAction::MoveDown, "DOWN"},
                                 {EmitterAction::Copy, "COPY"},
                                 {EmitterAction::Paste, "PASTE"}});
            addEmitterButtonRow(
                ctx.ui, font, 0.516f, {{EmitterAction::ToggleEnabled, "ON"}, {EmitterAction::ToggleSelection, "SEL"}});

            previewSwatch =
                createRectRelative(ctx.ui, root, {0.0f, 0.560f, 1.0f, 0.042f}, ToolTheme::paneSurface, true);
            previewText = createTextRelative(ctx.ui,
                                             previewSwatch,
                                             {0.040f, 0.120f, 0.920f, 0.760f},
                                             font,
                                             "LIVE PREVIEW",
                                             ToolTheme::text,
                                             ToolTheme::smallTextScale);
            setTextAlignment(ctx.ui, previewText, UiHorizontalAlign::Center, UiVerticalAlign::Middle);

            addPlaybackButtonRow(ctx.ui,
                                 font,
                                 0.612f,
                                 {{PlaybackAction::PlayPause, "PAUSE"},
                                  {PlaybackAction::Restart, "RESET"},
                                  {PlaybackAction::Background, "BG"},
                                  {PlaybackAction::CameraReset, "CAM"}});

            addSlider(ctx.ui, font, 0.655f, FloatField::TimeScale, "TIME", 0.0f, 2.0f);
            addSlider(ctx.ui, font, 0.686f, FloatField::OrbitYaw, "ORBIT", -180.0f, 180.0f);

            inspectorHeader = createTextRelative(ctx.ui,
                                                 root,
                                                 {0.0f, 0.718f, 1.0f, 0.024f},
                                                 font,
                                                 "MODULES",
                                                 ToolTheme::text,
                                                 ToolTheme::headerTextScale);
            for (size_t i = 0; i < ModuleRowCount; ++i)
            {
                const float rowY = 0.745f + static_cast<float>(i) * 0.030f;
                moduleRows.push_back(createButtonRelative(
                    ctx.ui, root, {0.0f, rowY, 1.0f, 0.028f}, font, "MODULE", ToolTheme::buttonTextScale));
            }

            addModuleButtonRow(ctx.ui,
                               font,
                               0.835f,
                               {{ModuleAction::Copy, "M COPY"},
                                {ModuleAction::Paste, "M PASTE"},
                                {ModuleAction::Undo, "UNDO"},
                                {ModuleAction::Redo, "REDO"}});

            for (size_t i = 0; i < ParameterControlRowCount; ++i)
            {
                const float rowY = 0.866f + static_cast<float>(i) * 0.029f;
                parameterControls.push_back(
                    {createSlider(ctx.ui, root, rowY, "PARAM", 0.0f, 1.0f, false, font),
                     createButtonRelative(
                         ctx.ui, root, {0.0f, rowY, 1.0f, 0.027f}, font, "PARAM", ToolTheme::buttonTextScale),
                     createButtonRelative(
                         ctx.ui, root, {0.0f, rowY, 0.820f, 0.027f}, font, "PARAM", ToolTheme::buttonTextScale),
                     createButtonRelative(
                         ctx.ui, root, {0.835f, rowY, 0.075f, 0.027f}, font, "-", ToolTheme::buttonTextScale),
                     createButtonRelative(
                         ctx.ui, root, {0.925f, rowY, 0.075f, 0.027f}, font, "+", ToolTheme::buttonTextScale)});
            }

            footer = createTextRelative(ctx.ui,
                                        root,
                                        {0.0f, 0.963f, 1.0f, 0.034f},
                                        font,
                                        "ASSET EDITOR",
                                        ToolTheme::statusText,
                                        ToolTheme::smallTextScale);
        }

        void
        update(EngineToolContext& ctx, EngineToolStateComponent& state, const UiInteractionResult& interaction) override
        {
            releaseUndoCapture(interaction);

            std::vector<std::string> effectPaths = collectEffectPaths(ctx.world);
            includeCurrentPath(effectPaths);
            clampEffectBrowserOffset(effectPaths);

            if (!hasAsset && !effectPaths.empty())
                openEffect(ctx.world, effectPaths.front(), state, false);

            applyEffectBrowser(ctx.world, state, effectPaths, interaction);
            applyTextFieldFocus(ctx.world, state, interaction);
            if (hasAsset)
            {
                applyTextInput(ctx, state);
                applyKeyboardShortcuts(ctx, state);
                applyEffectButtons(ctx.world, state, interaction);
                applyEmitterRows(ctx.world, state, interaction);
                applyEmitterButtons(ctx.world, state, interaction);
                applyModuleRows(state, interaction);
                applyModuleButtons(ctx.world, state, interaction);
                applyPlaybackButtons(ctx.world, state, interaction);
                applyPreviewOrbitDrag(state, interaction);
                applySliders(ctx.ui, ctx.world, state, interaction);
                applyModuleParameterControls(ctx.ui, ctx.world, state, interaction);
                applyPlaybackToLive(ctx.world);
            }
            claimKeyboardIfEditing(ctx.world);

            updateDisplay(ctx, state, effectPaths);
        }

        void setVisible(UiSystem& ui, bool visible) override
        {
            setVisibleRecursive(ui, root, visible);
        }

        void destroy(UiSystem& ui) override
        {
            if (root != UI_INVALID_HANDLE)
                ui.removeNode(root);
            root = UI_INVALID_HANDLE;
            effectRows.clear();
            emitterRows.clear();
            effectButtons.clear();
            emitterButtons.clear();
            moduleButtons.clear();
            playbackButtons.clear();
            moduleRows.clear();
            parameterControls.clear();
            sliders.clear();
        }

        private:
        enum class EffectAction
        {
            Save,
            SaveAs,
            Duplicate,
            Reload
        };

        enum class EmitterAction
        {
            Add,
            Delete,
            Duplicate,
            Rename,
            MoveUp,
            MoveDown,
            ToggleEnabled,
            ToggleSelection,
            Copy,
            Paste
        };

        enum class ModuleAction
        {
            Copy,
            Paste,
            Undo,
            Redo
        };

        enum class PlaybackAction
        {
            PlayPause,
            Restart,
            Background,
            CameraReset
        };

        enum class FloatField
        {
            TimeScale,
            OrbitYaw
        };

        enum class ActiveTextField
        {
            None,
            EffectName,
            EmitterName
        };

        struct EffectButtonBinding
        {
            EffectAction action = EffectAction::Save;
            ToolButton   button;
        };

        struct EmitterButtonBinding
        {
            EmitterAction action = EmitterAction::Add;
            ToolButton    button;
        };

        struct ModuleButtonBinding
        {
            ModuleAction action = ModuleAction::Copy;
            ToolButton   button;
        };

        struct PlaybackButtonBinding
        {
            PlaybackAction action = PlaybackAction::PlayPause;
            ToolButton     button;
        };

        struct SliderBinding
        {
            FloatField field = FloatField::TimeScale;
            ToolSlider slider;
        };

        struct ParameterControl
        {
            ToolSlider slider;
            ToolButton button;
            ToolButton selector;
            ToolButton decrement;
            ToolButton increment;
        };

        struct ParameterView
        {
            const ParticleModuleParameterDefinition* primary   = nullptr;
            const ParticleModuleParameterDefinition* secondary = nullptr;

            bool isRange() const
            {
                return primary != nullptr && secondary != nullptr;
            }
        };

        static constexpr size_t EffectRowCount           = 4;
        static constexpr size_t EmitterRowCount          = 3;
        static constexpr size_t ModuleRowCount           = 3;
        static constexpr size_t ParameterControlRowCount = 3;
        static constexpr size_t MaxUndoSnapshots         = 32;

        UiHandle root            = UI_INVALID_HANDLE;
        UiHandle header          = UI_INVALID_HANDLE;
        UiHandle summary         = UI_INVALID_HANDLE;
        UiHandle emitterHeader   = UI_INVALID_HANDLE;
        UiHandle previewSwatch   = UI_INVALID_HANDLE;
        UiHandle previewText     = UI_INVALID_HANDLE;
        UiHandle inspectorHeader = UI_INVALID_HANDLE;
        UiHandle footer          = UI_INVALID_HANDLE;

        ToolTextField effectNameField;
        ToolTextField emitterNameField;

        std::vector<ToolButton>            effectRows;
        std::vector<ToolButton>            emitterRows;
        std::vector<EffectButtonBinding>   effectButtons;
        std::vector<EmitterButtonBinding>  emitterButtons;
        std::vector<ModuleButtonBinding>   moduleButtons;
        std::vector<PlaybackButtonBinding> playbackButtons;
        std::vector<ToolButton>            moduleRows;
        std::vector<ParameterControl>      parameterControls;
        std::vector<SliderBinding>         sliders;

        ParticleEffectAsset                   currentAsset;
        std::string                           currentPath;
        bool                                  hasAsset               = false;
        bool                                  dirty                  = false;
        size_t                                effectBrowserOffset    = 0;
        size_t                                selectedEmitterIndex   = 0;
        size_t                                selectedModuleIndex    = 0;
        size_t                                moduleBrowserOffset    = 0;
        size_t                                parameterControlOffset = 0;
        bool                                  playbackPaused         = false;
        float                                 playbackTimeScale      = 1.0f;
        uint32_t                              backgroundPreset       = 0;
        ActiveTextField                       activeTextField        = ActiveTextField::None;
        std::string                           effectNameDraft;
        std::string                           emitterNameDraft;
        std::vector<ParticleEffectAsset>      undoStack;
        std::vector<ParticleEffectAsset>      redoStack;
        std::optional<ParticleModuleInstance> moduleClipboard;
        std::vector<ParticleEffectEmitter>    emitterClipboard;
        std::vector<std::string>              selectedEmitterIds;
        std::string                           selectedRichParameterId;
        size_t                                selectedRichKeyIndex = 0;
        uint32_t                              selectedRichField    = 0;
        std::string                           selectedRangeParameterId;
        bool                                  selectedRangeMax = false;
        UiHandle                              activeUndoHandle = UI_INVALID_HANDLE;
        bool                                  previewOrbitDragActive = false;
        float                                 previewOrbitDragLastX  = 0.0f;

        void addEffectButtonRow(UiSystem&                                                ui,
                                BitmapFont*                                              font,
                                float                                                    y,
                                const std::vector<std::pair<EffectAction, std::string>>& rowButtons)
        {
            const float gap = 0.015f;
            const float width =
                (1.0f - gap * static_cast<float>(rowButtons.size() - 1)) / static_cast<float>(rowButtons.size());

            for (size_t i = 0; i < rowButtons.size(); ++i)
            {
                const float x = static_cast<float>(i) * (width + gap);
                effectButtons.push_back(
                    {rowButtons[i].first,
                     createButtonRelative(
                         ui, root, {x, y, width, 0.038f}, font, rowButtons[i].second, ToolTheme::buttonTextScale)});
            }
        }

        void addEmitterButtonRow(UiSystem&                                                 ui,
                                 BitmapFont*                                               font,
                                 float                                                     y,
                                 const std::vector<std::pair<EmitterAction, std::string>>& rowButtons)
        {
            const float gap = 0.015f;
            const float width =
                (1.0f - gap * static_cast<float>(rowButtons.size() - 1)) / static_cast<float>(rowButtons.size());

            for (size_t i = 0; i < rowButtons.size(); ++i)
            {
                const float x = static_cast<float>(i) * (width + gap);
                emitterButtons.push_back(
                    {rowButtons[i].first,
                     createButtonRelative(
                         ui, root, {x, y, width, 0.038f}, font, rowButtons[i].second, ToolTheme::buttonTextScale)});
            }
        }

        void addModuleButtonRow(UiSystem&                                                ui,
                                BitmapFont*                                              font,
                                float                                                    y,
                                const std::vector<std::pair<ModuleAction, std::string>>& rowButtons)
        {
            const float gap = 0.015f;
            const float width =
                (1.0f - gap * static_cast<float>(rowButtons.size() - 1)) / static_cast<float>(rowButtons.size());

            for (size_t i = 0; i < rowButtons.size(); ++i)
            {
                const float x = static_cast<float>(i) * (width + gap);
                moduleButtons.push_back(
                    {rowButtons[i].first,
                     createButtonRelative(
                         ui, root, {x, y, width, 0.028f}, font, rowButtons[i].second, ToolTheme::buttonTextScale)});
            }
        }

        void addPlaybackButtonRow(UiSystem&                                                  ui,
                                  BitmapFont*                                                font,
                                  float                                                      y,
                                  const std::vector<std::pair<PlaybackAction, std::string>>& rowButtons)
        {
            const float gap = 0.015f;
            const float width =
                (1.0f - gap * static_cast<float>(rowButtons.size() - 1)) / static_cast<float>(rowButtons.size());

            for (size_t i = 0; i < rowButtons.size(); ++i)
            {
                const float x = static_cast<float>(i) * (width + gap);
                playbackButtons.push_back(
                    {rowButtons[i].first,
                     createButtonRelative(
                         ui, root, {x, y, width, 0.038f}, font, rowButtons[i].second, ToolTheme::buttonTextScale)});
            }
        }

        void addSlider(UiSystem&          ui,
                       BitmapFont*        font,
                       float              y,
                       FloatField         field,
                       const std::string& name,
                       float              minValue,
                       float              maxValue,
                       bool               wholeNumber = false)
        {
            sliders.push_back({field, createSlider(ui, root, y, name, minValue, maxValue, wholeNumber, font)});
        }

        static std::vector<std::string> collectEffectPaths(ECSWorld& world)
        {
            std::vector<std::string> paths;
            if (world.hasAny<ParticleEffectRegistryComponent>())
            {
                const ParticleEffectRegistryComponent& registry = world.getSingleton<ParticleEffectRegistryComponent>();
                for (const auto& registryEntry : registry.effects)
                {
                    const std::string& path = registryEntry.first;
                    if (!path.empty())
                        paths.push_back(path);
                }
            }

            world.forEach<ParticleEmitterComponent>(
                [&](Entity, ParticleEmitterComponent& emitter)
                {
                    if (!emitter.effectPath.empty())
                        paths.push_back(emitter.effectPath);
                });

            scanParticleEffectDirectory(paths, "resources/particles");
            scanParticleEffectDirectory(paths, "../../resources/particles");
            scanParticleEffectDirectory(paths, "../resources/particles");

            sortUnique(paths);
            return paths;
        }

        void includeCurrentPath(std::vector<std::string>& paths) const
        {
            if (!currentPath.empty())
            {
                paths.push_back(currentPath);
                sortUnique(paths);
            }
        }

        static void sortUnique(std::vector<std::string>& paths)
        {
            std::sort(paths.begin(), paths.end());
            paths.erase(std::unique(paths.begin(), paths.end()), paths.end());
        }

        static void scanParticleEffectDirectory(std::vector<std::string>& paths, const std::filesystem::path& rootPath)
        {
            std::error_code ec;
            if (!std::filesystem::exists(rootPath, ec) || !std::filesystem::is_directory(rootPath, ec))
                return;

            const std::filesystem::recursive_directory_iterator end;
            std::filesystem::recursive_directory_iterator       it(
                rootPath, std::filesystem::directory_options::skip_permission_denied, ec);
            while (!ec && it != end)
            {
                const std::filesystem::directory_entry& entry = *it;
                if (entry.is_regular_file(ec) && entry.path().extension() == ".json")
                    paths.push_back(entry.path().generic_string());
                it.increment(ec);
            }
        }

        void applyEffectBrowser(ECSWorld&                       world,
                                EngineToolStateComponent&       state,
                                const std::vector<std::string>& paths,
                                const UiInteractionResult&      interaction)
        {
            if (!paths.empty() && interaction.scrollY != 0.0f && pointerOverEffectRows(interaction))
            {
                if (interaction.scrollY < 0.0f)
                    effectBrowserOffset = std::min(effectBrowserOffset + 1, maxEffectBrowserOffset(paths));
                else if (effectBrowserOffset > 0)
                    effectBrowserOffset -= 1;
            }

            for (size_t i = 0; i < effectRows.size() && effectBrowserOffset + i < paths.size(); ++i)
            {
                if (wasClicked(interaction, effectRows[i].rect))
                {
                    openEffect(world, paths[effectBrowserOffset + i], state, true);
                    return;
                }
            }
        }

        bool pointerOverEffectRows(const UiInteractionResult& interaction) const
        {
            for (const ToolButton& row : effectRows)
            {
                if (interaction.hovered == row.rect || interaction.pressed == row.rect)
                    return true;
            }
            return false;
        }

        void clampEffectBrowserOffset(const std::vector<std::string>& paths)
        {
            effectBrowserOffset = std::min(effectBrowserOffset, maxEffectBrowserOffset(paths));
        }

        size_t maxEffectBrowserOffset(const std::vector<std::string>& paths) const
        {
            return paths.size() <= EffectRowCount ? 0 : paths.size() - EffectRowCount;
        }

        void revealEffectPath(const std::vector<std::string>& paths, const std::string& path)
        {
            const auto it = std::find(paths.begin(), paths.end(), path);
            if (it == paths.end())
                return;

            const size_t index = static_cast<size_t>(std::distance(paths.begin(), it));
            if (index < effectBrowserOffset)
                effectBrowserOffset = index;
            else if (index >= effectBrowserOffset + EffectRowCount)
                effectBrowserOffset = index - EffectRowCount + 1;
        }

        void
        applyTextFieldFocus(ECSWorld& world, EngineToolStateComponent& state, const UiInteractionResult& interaction)
        {
            if (wasClicked(interaction, effectNameField.rect))
            {
                if (!hasAsset)
                {
                    state.status = "NO EFFECT";
                    return;
                }

                pushUndoSnapshot();
                activeTextField = ActiveTextField::EffectName;
                effectNameDraft = currentAsset.metadata.name;
                state.status    = "EDIT EFFECT NAME";
                claimKeyboardIfEditing(world);
                return;
            }

            if (wasClicked(interaction, emitterNameField.rect))
            {
                if (selectedEmitter() == nullptr)
                {
                    state.status = "NO EMITTER";
                    return;
                }

                pushUndoSnapshot();
                activeTextField = ActiveTextField::EmitterName;
                syncEmitterNameDraft();
                state.status = "EDIT EMITTER NAME";
                claimKeyboardIfEditing(world);
                return;
            }

            if (interaction.clicked != UI_INVALID_HANDLE && activeTextField != ActiveTextField::None)
                finishActiveTextField(state);
        }

        void applyTextInput(EngineToolContext& ctx, EngineToolStateComponent& state)
        {
            if (activeTextField == ActiveTextField::None || ctx.input == nullptr)
                return;

            const std::optional<InputTrigger> trigger = ctx.input->getLastPressedTrigger();
            if (!trigger.has_value() || trigger->type != InputTrigger::Type::Key)
                return;
            if (has(trigger->modifiers, ModifierFlags::Ctrl) || has(trigger->modifiers, ModifierFlags::Alt) ||
                has(trigger->modifiers, ModifierFlags::Super))
            {
                return;
            }

            const GtsKey key = static_cast<GtsKey>(trigger->code);
            if (key == GtsKey::Escape)
            {
                cancelActiveTextField();
                state.status = "EDIT CANCELED";
                return;
            }
            if (key == GtsKey::Enter || key == GtsKey::Tab)
            {
                finishActiveTextField(state);
                return;
            }
            if (key == GtsKey::Backspace)
            {
                std::string& draft = activeTextDraft();
                if (!draft.empty())
                {
                    draft.pop_back();
                    applyActiveTextDraft(state);
                }
                return;
            }

            const bool shift = has(trigger->modifiers, ModifierFlags::Shift);
            const char typed = characterForKey(key, shift);
            if (typed == '\0')
                return;

            std::string& draft = activeTextDraft();
            if (draft.size() >= 48)
                return;
            draft.push_back(typed);
            applyActiveTextDraft(state);
        }

        void claimKeyboardIfEditing(ECSWorld& world)
        {
            if (activeTextField == ActiveTextField::None)
                return;

            EngineToolInputCaptureComponent* capture = nullptr;
            if (world.hasAny<EngineToolInputCaptureComponent>())
                capture = &world.getSingleton<EngineToolInputCaptureComponent>();
            else
                capture = &world.createSingleton<EngineToolInputCaptureComponent>();
            capture->keyboardCaptured = true;
        }

        std::string& activeTextDraft()
        {
            return activeTextField == ActiveTextField::EffectName ? effectNameDraft : emitterNameDraft;
        }

        void applyActiveTextDraft(EngineToolStateComponent& state)
        {
            if (activeTextField == ActiveTextField::EffectName)
            {
                currentAsset.metadata.name = effectNameDraft;
                markDirty(state, "EFFECT NAME UPDATED");
                return;
            }

            ParticleEffectEmitter* selected = selectedEmitter();
            if (selected == nullptr)
                return;

            selected->name = emitterNameDraft;
            markDirty(state, "EMITTER NAME UPDATED");
        }

        void finishActiveTextField(EngineToolStateComponent& state)
        {
            if (activeTextField == ActiveTextField::EffectName && effectNameDraft.empty())
            {
                effectNameDraft            = std::filesystem::path(currentPath).stem().string();
                currentAsset.metadata.name = effectNameDraft;
                dirty                      = true;
            }
            else if (activeTextField == ActiveTextField::EmitterName && emitterNameDraft.empty())
            {
                emitterNameDraft = "Emitter " + std::to_string(selectedEmitterIndex + 1);
                if (ParticleEffectEmitter* selected = selectedEmitter())
                {
                    selected->name = emitterNameDraft;
                    dirty          = true;
                }
            }

            activeTextField = ActiveTextField::None;
            state.status    = dirty ? "EDIT APPLIED" : "EDIT DONE";
        }

        void cancelActiveTextField()
        {
            if (activeTextField == ActiveTextField::EffectName)
                effectNameDraft = currentAsset.metadata.name;
            else if (activeTextField == ActiveTextField::EmitterName)
                syncEmitterNameDraft();
            activeTextField = ActiveTextField::None;
        }

        static char characterForKey(GtsKey key, bool shift)
        {
            if (key >= GtsKey::A && key <= GtsKey::Z)
            {
                const int  offset = static_cast<int>(key) - static_cast<int>(GtsKey::A);
                const char base   = shift ? 'A' : 'a';
                return static_cast<char>(base + offset);
            }
            if (key >= GtsKey::Digit0 && key <= GtsKey::Digit9)
            {
                const int offset = static_cast<int>(key) - static_cast<int>(GtsKey::Digit0);
                return static_cast<char>('0' + offset);
            }
            if (key == GtsKey::Space)
                return ' ';
            return '\0';
        }

        bool openEffect(ECSWorld&                 world,
                        const std::string&        path,
                        EngineToolStateComponent& state,
                        bool                      protectDirty,
                        bool                      forceReload = false)
        {
            if (protectDirty && dirty && path != currentPath)
            {
                state.status = "SAVE OR RELOAD FIRST";
                return false;
            }
            if (!forceReload && path == currentPath && hasAsset)
                return true;

            clearPlaybackForPath(world, currentPath);

            ParticleEffectAsset loaded;
            if (!gts::particles::loadParticleEffectAsset(path, loaded))
            {
                state.status = "OPEN FAILED";
                return false;
            }

            currentAsset           = std::move(loaded);
            currentPath            = path;
            hasAsset               = true;
            dirty                  = false;
            selectedEmitterIndex   = 0;
            selectedModuleIndex    = 0;
            moduleBrowserOffset    = 0;
            parameterControlOffset = 0;
            selectedEmitterIds.clear();
            selectedRichParameterId.clear();
            selectedRichKeyIndex = 0;
            selectedRichField    = 0;
            selectedRangeParameterId.clear();
            selectedRangeMax = false;
            undoStack.clear();
            redoStack.clear();
            activeUndoHandle  = UI_INVALID_HANDLE;
            playbackPaused    = false;
            playbackTimeScale = 1.0f;
            backgroundPreset  = closestBackgroundPreset(currentAsset.preview.backgroundColor);
            activeTextField   = ActiveTextField::None;
            effectNameDraft   = currentAsset.metadata.name;
            syncEmitterNameDraft();
            state.status = "OPENED " + fileName(path);
            return true;
        }

        void
        applyEffectButtons(ECSWorld& world, EngineToolStateComponent& state, const UiInteractionResult& interaction)
        {
            for (const EffectButtonBinding& binding : effectButtons)
            {
                if (!wasClicked(interaction, binding.button.rect))
                    continue;

                switch (binding.action)
                {
                case EffectAction::Save:
                    saveCurrent(state, currentPath);
                    applySelectedEmitterToLive(world, false);
                    break;
                case EffectAction::SaveAs:
                    saveCurrentAs(world, state, "_edited");
                    break;
                case EffectAction::Duplicate:
                    saveCurrentAs(world, state, "_copy");
                    break;
                case EffectAction::Reload:
                    reloadCurrent(world, state);
                    break;
                }
                return;
            }
        }

        void saveCurrent(EngineToolStateComponent& state, const std::string& path)
        {
            if (!hasAsset || path.empty())
            {
                state.status = "NO EFFECT";
                return;
            }

            ParticleEffectAsset asset = assetForSave();
            if (!gts::particles::saveParticleEffectAsset(path, asset))
            {
                state.status = "SAVE FAILED";
                return;
            }

            currentAsset = std::move(asset);
            currentPath  = path;
            dirty        = false;
            state.status = "SAVED " + fileName(path);
        }

        void saveCurrentAs(ECSWorld& world, EngineToolStateComponent& state, const std::string& suffix)
        {
            if (!hasAsset)
            {
                state.status = "NO EFFECT";
                return;
            }

            const std::string oldPath = currentPath;
            const std::string path    = generatedSiblingPath(suffix);
            saveCurrent(state, path);
            if (state.status.rfind("SAVED", 0) == 0)
            {
                if (oldPath != path)
                    clearPlaybackForPath(world, oldPath);
                openEffect(world, path, state, false);
            }
        }

        void reloadCurrent(ECSWorld& world, EngineToolStateComponent& state)
        {
            if (currentPath.empty())
            {
                state.status = "NO EFFECT";
                return;
            }
            openEffect(world, currentPath, state, false, true);
            applySelectedEmitterToLive(world, true);
        }

        ParticleEffectAsset assetForSave() const
        {
            ParticleEffectAsset asset = currentAsset;
            asset.schemaVersion       = CurrentParticleEffectSchemaVersion;
            if (asset.metadata.name.empty())
                asset.metadata.name = std::filesystem::path(currentPath).stem().string();

            for (ParticleEffectEmitter& emitter : asset.emitters)
            {
                gts::particles::migrateParticleEmitterModules(emitter.modules, emitter.descriptor);
                emitter.descriptor.effectPath.clear();
                emitter.descriptor.effectEmitterId.clear();
                emitter.descriptor.schemaVersion = CurrentParticleEmitterSchemaVersion;
            }
            return asset;
        }

        void applyEmitterRows(ECSWorld& world, EngineToolStateComponent& state, const UiInteractionResult& interaction)
        {
            if (!hasAsset)
                return;

            const size_t rowOffset = emitterRowOffset();
            for (size_t i = 0; i < emitterRows.size() && rowOffset + i < currentAsset.emitters.size(); ++i)
            {
                if (wasClicked(interaction, emitterRows[i].rect))
                {
                    selectedEmitterIndex   = rowOffset + i;
                    selectedModuleIndex    = 0;
                    moduleBrowserOffset    = 0;
                    parameterControlOffset = 0;
                    clearParameterSelectionState();
                    syncEmitterNameDraft();
                    applySelectedEmitterToLive(world, false);
                    state.status = "SELECTED " + currentAsset.emitters[selectedEmitterIndex].name;
                    return;
                }
            }
        }

        void
        applyEmitterButtons(ECSWorld& world, EngineToolStateComponent& state, const UiInteractionResult& interaction)
        {
            for (const EmitterButtonBinding& binding : emitterButtons)
            {
                if (!wasClicked(interaction, binding.button.rect))
                    continue;

                switch (binding.action)
                {
                case EmitterAction::Add:
                    addEmitter(state);
                    applySelectedEmitterToLive(world, true);
                    break;
                case EmitterAction::Delete:
                    deleteEmitter(state);
                    applySelectedEmitterToLive(world, true);
                    break;
                case EmitterAction::Duplicate:
                    duplicateEmitter(state);
                    applySelectedEmitterToLive(world, true);
                    break;
                case EmitterAction::Rename:
                    renameEmitter(state);
                    break;
                case EmitterAction::MoveUp:
                    moveEmitter(state, -1);
                    break;
                case EmitterAction::MoveDown:
                    moveEmitter(state, 1);
                    break;
                case EmitterAction::ToggleEnabled:
                    toggleEmitterEnabled(state);
                    applySelectedEmitterToLive(world, false);
                    break;
                case EmitterAction::ToggleSelection:
                    toggleSelectedEmitterSelection(state);
                    break;
                case EmitterAction::Copy:
                    copyEmitters(state);
                    break;
                case EmitterAction::Paste:
                    pasteEmitters(state);
                    applySelectedEmitterToLive(world, true);
                    break;
                }
                return;
            }
        }

        void applyModuleRows(EngineToolStateComponent& state, const UiInteractionResult& interaction)
        {
            ParticleEffectEmitter* emitter = selectedEmitter();
            if (emitter == nullptr)
                return;

            clampModuleSelection(*emitter);
            if (interaction.scrollY != 0.0f && pointerOverModuleRows(interaction))
            {
                const size_t maxOffset = maxModuleBrowserOffset(*emitter);
                if (interaction.scrollY < 0.0f)
                    moduleBrowserOffset = std::min(moduleBrowserOffset + 1, maxOffset);
                else if (moduleBrowserOffset > 0)
                    moduleBrowserOffset -= 1;
            }

            for (size_t i = 0; i < moduleRows.size() && moduleBrowserOffset + i < emitter->modules.size(); ++i)
            {
                if (!wasClicked(interaction, moduleRows[i].rect))
                    continue;

                selectedModuleIndex    = moduleBrowserOffset + i;
                parameterControlOffset = 0;
                clearParameterSelectionState();
                state.status = selectedModuleStatus(emitter->modules[selectedModuleIndex]);
                return;
            }
        }

        void
        applyModuleButtons(ECSWorld& world, EngineToolStateComponent& state, const UiInteractionResult& interaction)
        {
            for (const ModuleButtonBinding& binding : moduleButtons)
            {
                if (!wasClicked(interaction, binding.button.rect))
                    continue;

                switch (binding.action)
                {
                case ModuleAction::Copy:
                    copyModule(state);
                    break;
                case ModuleAction::Paste:
                    pasteModule(world, state);
                    break;
                case ModuleAction::Undo:
                    undo(world, state);
                    break;
                case ModuleAction::Redo:
                    redo(world, state);
                    break;
                }
                return;
            }
        }

        void applyModuleParameterControls(UiSystem&                  ui,
                                          ECSWorld&                  world,
                                          EngineToolStateComponent&  state,
                                          const UiInteractionResult& interaction)
        {
            ParticleEffectEmitter*  emitter = selectedEmitter();
            ParticleModuleInstance* module  = selectedModule();
            if (emitter == nullptr || module == nullptr)
                return;

            const ParticleModuleDefinition* definition = findParticleModuleDefinition(module->typeId);
            if (definition == nullptr)
                return;

            const std::vector<ParameterView> parameters = editableParameters(*definition);
            clampParameterControlOffset(parameters);
            if (interaction.scrollY != 0.0f && pointerOverParameterControls(interaction))
            {
                const size_t maxOffset = maxParameterControlOffset(parameters);
                if (interaction.scrollY < 0.0f)
                    parameterControlOffset = std::min(parameterControlOffset + 1, maxOffset);
                else if (parameterControlOffset > 0)
                    parameterControlOffset -= 1;
            }

            for (size_t row = 0; row < parameterControls.size() && parameterControlOffset + row < parameters.size();
                 ++row)
            {
                ParameterControl&    control = parameterControls[row];
                const ParameterView& view    = parameters[parameterControlOffset + row];
                if (applyParameterViewControl(ui, world, state, interaction, *emitter, *module, view, control))
                    return;
            }
        }

        void addEmitter(EngineToolStateComponent& state)
        {
            pushUndoSnapshot();
            ParticleEffectEmitter emitter;
            emitter.stableId = uniqueEmitterId("emitter");
            emitter.name     = "Emitter " + std::to_string(currentAsset.emitters.size() + 1);
            emitter.descriptor.effectEmitterId.clear();
            emitter.descriptor.effectPath.clear();
            emitter.modules = buildModulesFromEmitterDescriptor(emitter.descriptor);
            currentAsset.emitters.push_back(std::move(emitter));
            selectedEmitterIndex = currentAsset.emitters.size() - 1;
            selectedEmitterIds.clear();
            selectedModuleIndex    = 0;
            moduleBrowserOffset    = 0;
            parameterControlOffset = 0;
            clearParameterSelectionState();
            syncEmitterNameDraft();
            markDirty(state, "EMITTER ADDED");
        }

        void deleteEmitter(EngineToolStateComponent& state)
        {
            const std::vector<size_t> targets = selectedEmitterTargetIndices();
            if (!targets.empty())
            {
                deleteSelectedEmitters(state, targets);
                return;
            }

            if (currentAsset.emitters.size() <= 1)
            {
                state.status = "KEEP ONE EMITTER";
                return;
            }
            pushUndoSnapshot();
            selectedEmitterIndex = std::min(selectedEmitterIndex, currentAsset.emitters.size() - 1);
            currentAsset.emitters.erase(currentAsset.emitters.begin() +
                                        static_cast<std::ptrdiff_t>(selectedEmitterIndex));
            if (selectedEmitterIndex >= currentAsset.emitters.size())
                selectedEmitterIndex = currentAsset.emitters.size() - 1;
            selectedModuleIndex    = 0;
            moduleBrowserOffset    = 0;
            parameterControlOffset = 0;
            clearParameterSelectionState();
            syncEmitterNameDraft();
            markDirty(state, "EMITTER DELETED");
        }

        void duplicateEmitter(EngineToolStateComponent& state)
        {
            const std::vector<size_t> targets = selectedEmitterTargetIndices();
            if (!targets.empty())
            {
                duplicateSelectedEmitters(state, targets);
                return;
            }

            ParticleEffectEmitter* selected = selectedEmitter();
            if (selected == nullptr)
            {
                state.status = "NO EMITTER";
                return;
            }

            pushUndoSnapshot();
            ParticleEffectEmitter copy = *selected;
            copy.stableId              = uniqueEmitterId(selected->stableId.empty() ? "emitter" : selected->stableId);
            copy.name                  = selected->name + " Copy";
            currentAsset.emitters.insert(
                currentAsset.emitters.begin() + static_cast<std::ptrdiff_t>(selectedEmitterIndex + 1), std::move(copy));
            selectedEmitterIndex += 1;
            selectedEmitterIds.clear();
            selectedModuleIndex    = 0;
            moduleBrowserOffset    = 0;
            parameterControlOffset = 0;
            clearParameterSelectionState();
            syncEmitterNameDraft();
            markDirty(state, "EMITTER COPIED");
        }

        void renameEmitter(EngineToolStateComponent& state)
        {
            if (selectedEmitter() == nullptr)
            {
                state.status = "NO EMITTER";
                return;
            }

            pushUndoSnapshot();
            syncEmitterNameDraft();
            activeTextField = ActiveTextField::EmitterName;
            state.status    = "EDIT EMITTER NAME";
        }

        void moveEmitter(EngineToolStateComponent& state, int delta)
        {
            if (currentAsset.emitters.empty())
                return;

            const int current = static_cast<int>(selectedEmitterIndex);
            const int next    = std::clamp(current + delta, 0, static_cast<int>(currentAsset.emitters.size()) - 1);
            if (next == current)
            {
                state.status = delta < 0 ? "ALREADY FIRST" : "ALREADY LAST";
                return;
            }

            if (!selectedEmitterIds.empty())
            {
                state.status = "SELECT ONE TO MOVE";
                return;
            }

            pushUndoSnapshot();
            std::swap(currentAsset.emitters[static_cast<size_t>(current)],
                      currentAsset.emitters[static_cast<size_t>(next)]);
            selectedEmitterIndex = static_cast<size_t>(next);
            syncEmitterNameDraft();
            markDirty(state, "EMITTER MOVED");
        }

        void toggleEmitterEnabled(EngineToolStateComponent& state)
        {
            ParticleEffectEmitter* selected = selectedEmitter();
            if (selected == nullptr)
            {
                state.status = "NO EMITTER";
                return;
            }

            const std::vector<size_t> targets = selectedEmitterTargetIndices();
            pushUndoSnapshot();
            const bool enabled = !selected->descriptor.enabled;
            if (targets.empty())
            {
                selected->descriptor.enabled = enabled;
                setEmitterEnabledModuleParameter(*selected, selected->descriptor.enabled);
            }
            else
            {
                for (size_t index : targets)
                {
                    currentAsset.emitters[index].descriptor.enabled = enabled;
                    setEmitterEnabledModuleParameter(currentAsset.emitters[index], enabled);
                }
            }
            markDirty(state, enabled ? "EMITTER ENABLED" : "EMITTER DISABLED");
        }

        void
        applyPlaybackButtons(ECSWorld& world, EngineToolStateComponent& state, const UiInteractionResult& interaction)
        {
            for (const PlaybackButtonBinding& binding : playbackButtons)
            {
                if (!wasClicked(interaction, binding.button.rect))
                    continue;

                switch (binding.action)
                {
                case PlaybackAction::PlayPause:
                    playbackPaused = !playbackPaused;
                    state.status   = playbackPaused ? "PREVIEW PAUSED" : "PREVIEW PLAYING";
                    applyPlaybackToLive(world);
                    break;
                case PlaybackAction::Restart:
                    restartLiveEffect(world);
                    state.status = "PREVIEW RESTARTED";
                    break;
                case PlaybackAction::Background:
                    cycleBackground(state);
                    break;
                case PlaybackAction::CameraReset:
                    resetPreviewCamera(state);
                    break;
                }
                return;
            }
        }

        void applyPreviewOrbitDrag(EngineToolStateComponent& state, const UiInteractionResult& interaction)
        {
            if (interaction.pressed != previewSwatch)
            {
                previewOrbitDragActive = false;
                return;
            }

            if (!previewOrbitDragActive)
            {
                previewOrbitDragActive = true;
                previewOrbitDragLastX  = interaction.pointerX;
                return;
            }

            const float deltaX = interaction.pointerX - previewOrbitDragLastX;
            previewOrbitDragLastX = interaction.pointerX;
            if (std::abs(deltaX) < 0.0001f)
                return;

            beginUndoForHandle(previewSwatch);
            setPreviewOrbitYawDegrees(previewOrbitYawDegrees() + deltaX * 240.0f);
            markDirty(state, "PREVIEW ORBIT");
        }

        void applySliders(UiSystem&                  ui,
                          ECSWorld&                  world,
                          EngineToolStateComponent&  state,
                          const UiInteractionResult& interaction)
        {
            for (const SliderBinding& binding : sliders)
            {
                if (!isPressed(interaction, binding.slider.track))
                    continue;

                const float value = valueFromSliderPointer(ui, binding.slider, interaction.pointerX);
                if (binding.field == FloatField::TimeScale)
                {
                    playbackTimeScale = std::max(0.0f, value);
                    applyPlaybackToLive(world);
                    state.status = "TIME SCALE";
                    return;
                }
                if (binding.field == FloatField::OrbitYaw)
                {
                    beginUndoForHandle(binding.slider.track);
                    setPreviewOrbitYawDegrees(value);
                    markDirty(state, "PREVIEW CAMERA");
                    return;
                }
            }
        }

        void cycleBackground(EngineToolStateComponent& state)
        {
            pushUndoSnapshot();
            backgroundPreset = (backgroundPreset + 1u) % static_cast<uint32_t>(backgroundPresets().size());
            currentAsset.preview.backgroundColor = backgroundPresets()[backgroundPreset];
            markDirty(state, "BACKGROUND UPDATED");
        }

        void resetPreviewCamera(EngineToolStateComponent& state)
        {
            pushUndoSnapshot();
            currentAsset.preview.cameraPosition = {0.0f, 1.0f, 4.0f};
            currentAsset.preview.cameraTarget   = {0.0f, 0.6f, 0.0f};
            currentAsset.preview.orbitDistance  = 4.0f;
            markDirty(state, "CAMERA RESET");
        }

        void applyPlaybackToLive(ECSWorld& world)
        {
            if (!hasAsset || currentPath.empty())
                return;

            world.forEach<ParticleEmitterComponent, ParticleEmitterRuntimeComponent>(
                [&](Entity, ParticleEmitterComponent& emitter, ParticleEmitterRuntimeComponent& runtime)
                {
                    if (emitter.effectPath != currentPath)
                        return;

                    runtime.playbackPaused    = playbackPaused;
                    runtime.playbackTimeScale = playbackTimeScale;
                });
        }

        void clearPlaybackForPath(ECSWorld& world, const std::string& path)
        {
            if (path.empty())
                return;

            world.forEach<ParticleEmitterComponent, ParticleEmitterRuntimeComponent>(
                [&](Entity, ParticleEmitterComponent& emitter, ParticleEmitterRuntimeComponent& runtime)
                {
                    if (emitter.effectPath != path)
                        return;

                    runtime.playbackPaused    = false;
                    runtime.playbackTimeScale = 1.0f;
                });
        }

        void restartLiveEffect(ECSWorld& world)
        {
            if (currentPath.empty())
                return;

            world.forEach<ParticleEmitterComponent, ParticleEmitterRuntimeComponent>(
                [&](Entity, ParticleEmitterComponent& emitter, ParticleEmitterRuntimeComponent& runtime)
                {
                    if (emitter.effectPath == currentPath)
                        resetRuntime(runtime);
                });
        }

        void applySelectedEmitterToLive(ECSWorld& world, bool restart)
        {
            ParticleEffectEmitter* selected = selectedEmitter();
            if (selected == nullptr || currentPath.empty())
                return;

            world.forEach<ParticleEmitterComponent, ParticleEmitterRuntimeComponent>(
                [&](Entity, ParticleEmitterComponent& emitter, ParticleEmitterRuntimeComponent& runtime)
                {
                    if (emitter.effectPath != currentPath)
                        return;
                    if (!liveEmitterCanUseSelection(emitter, selected->stableId))
                        return;

                    const std::string effectPath       = emitter.effectPath;
                    const uint32_t    randomSeed       = emitter.randomSeed;
                    const bool        reloadFromEffect = emitter.reloadFromEffect;
                    emitter                            = selected->descriptor;
                    emitter.effectPath                 = effectPath;
                    emitter.effectEmitterId            = selected->stableId;
                    emitter.randomSeed                 = randomSeed;
                    emitter.reloadFromEffect           = reloadFromEffect;

                    if (restart)
                        resetRuntime(runtime);
                });
        }

        bool liveEmitterCanUseSelection(const ParticleEmitterComponent& emitter, const std::string& stableId) const
        {
            if (emitter.effectEmitterId.empty())
                return selectedEmitterIndex == 0;
            if (emitter.effectEmitterId == stableId)
                return true;
            return !assetHasEmitter(emitter.effectEmitterId);
        }

        bool assetHasEmitter(const std::string& stableId) const
        {
            for (const ParticleEffectEmitter& emitter : currentAsset.emitters)
            {
                if (emitter.stableId == stableId)
                    return true;
            }
            return false;
        }

        static void resetRuntime(ParticleEmitterRuntimeComponent& runtime)
        {
            runtime.particles.clear();
            runtime.spawnAccumulator = 0.0f;
            runtime.emitterAge       = 0.0f;
            runtime.burstRepeatCounts.clear();
            runtime.textureID = 0;
            runtime.meshID    = 0;
            runtime.boundTexturePath.clear();
            runtime.boundMeshPath.clear();
        }

        void updateDisplay(EngineToolContext&              ctx,
                           EngineToolStateComponent&       state,
                           const std::vector<std::string>& effectPaths)
        {
            setText(ctx.ui, summary, effectSummary());
            setRectColor(ctx.ui, previewSwatch, previewColor());
            setText(ctx.ui, previewText, previewSummary(ctx.world));
            setText(ctx.ui, inspectorHeader, moduleInspectorTitle());
            setText(ctx.ui, footer, state.status);
            syncTextDraftsForDisplay();
            updateTextField(
                ctx.ui,
                effectNameField,
                textFieldDisplay(hasAsset ? effectNameDraft : "--", activeTextField == ActiveTextField::EffectName),
                activeTextField == ActiveTextField::EffectName);
            updateTextField(ctx.ui,
                            emitterNameField,
                            textFieldDisplay(selectedEmitter() == nullptr ? "--" : emitterNameDraft,
                                             activeTextField == ActiveTextField::EmitterName),
                            activeTextField == ActiveTextField::EmitterName);

            for (size_t i = 0; i < effectRows.size(); ++i)
            {
                const size_t      effectIndex = effectBrowserOffset + i;
                const bool        hasRow      = effectIndex < effectPaths.size();
                const std::string label       = hasRow ? fileName(effectPaths[effectIndex]) : "EMPTY";
                updateButton(ctx.ui, effectRows[i], label);
                if (hasRow && effectPaths[effectIndex] == currentPath)
                    setRectColor(ctx.ui, effectRows[i].rect, ToolTheme::buttonActive);
            }

            const size_t rowOffset = emitterRowOffset();
            for (size_t i = 0; i < emitterRows.size(); ++i)
            {
                const size_t      assetIndex = rowOffset + i;
                const bool        hasRow     = hasAsset && assetIndex < currentAsset.emitters.size();
                const std::string label      = hasRow ? emitterRowLabel(currentAsset.emitters[assetIndex]) : "EMPTY";
                updateButton(ctx.ui, emitterRows[i], label);
                if (hasRow && assetIndex == selectedEmitterIndex)
                    setRectColor(ctx.ui, emitterRows[i].rect, ToolTheme::buttonActive);
            }

            updateModuleRows(ctx.ui);
            updateParameterControls(ctx.ui);

            for (const EffectButtonBinding& binding : effectButtons)
                updateButton(ctx.ui, binding.button, effectButtonLabel(binding.action));
            for (const EmitterButtonBinding& binding : emitterButtons)
                updateButton(ctx.ui, binding.button, emitterButtonLabel(binding.action));
            for (const ModuleButtonBinding& binding : moduleButtons)
                updateButton(ctx.ui, binding.button, moduleButtonLabel(binding.action));
            for (const PlaybackButtonBinding& binding : playbackButtons)
                updateButton(ctx.ui, binding.button, playbackButtonLabel(binding.action));

            for (const SliderBinding& binding : sliders)
                updateSlider(ctx.ui, binding.slider, readSliderValue(binding.field), sliderColor(binding.field));
        }

        void updateModuleRows(UiSystem& ui)
        {
            ParticleEffectEmitter* emitter = selectedEmitter();
            if (emitter == nullptr)
            {
                for (ToolButton& row : moduleRows)
                    updateButton(ui, row, "NO MODULE");
                return;
            }

            clampModuleSelection(*emitter);
            const size_t rowOffset = moduleRowOffset(*emitter);
            for (size_t i = 0; i < moduleRows.size(); ++i)
            {
                const size_t      moduleIndex = rowOffset + i;
                const bool        hasRow      = moduleIndex < emitter->modules.size();
                const std::string label       = hasRow ? moduleDisplayName(emitter->modules[moduleIndex]) : "EMPTY";
                updateButton(ui, moduleRows[i], label);
                if (hasRow && moduleIndex == selectedModuleIndex)
                    setRectColor(ui, moduleRows[i].rect, ToolTheme::buttonActive);
            }
        }

        void updateParameterControls(UiSystem& ui)
        {
            ParticleModuleInstance*         module = selectedModule();
            const ParticleModuleDefinition* definition =
                module == nullptr ? nullptr : findParticleModuleDefinition(module->typeId);
            if (module == nullptr || definition == nullptr)
            {
                for (ParameterControl& control : parameterControls)
                    setParameterControlVisible(ui, control, false, false, false, false, false, false);
                return;
            }

            const std::vector<ParameterView> parameters = editableParameters(*definition);
            clampParameterControlOffset(parameters);
            for (size_t row = 0; row < parameterControls.size(); ++row)
            {
                const size_t      parameterIndex = parameterControlOffset + row;
                ParameterControl& control        = parameterControls[row];
                if (parameterIndex >= parameters.size())
                {
                    setParameterControlVisible(ui, control, false, false, false, false, false, false);
                    continue;
                }

                updateParameterViewControl(ui, *module, parameters[parameterIndex], control);
            }
        }

        std::string effectSummary() const
        {
            if (!hasAsset)
                return "OPEN AN EFFECT FROM SCENE";

            std::ostringstream out;
            const std::string  name =
                currentAsset.metadata.name.empty() ? fileName(currentPath) : currentAsset.metadata.name;
            out << name << (dirty ? " *" : "") << "  " << currentAsset.emitters.size() << " EMIT  "
                << graphStatusLabel();
            return out.str();
        }

        std::string previewSummary(ECSWorld& world) const
        {
            if (!hasAsset)
                return "NO LIVE PREVIEW";

            size_t liveEmitters  = 0;
            size_t liveParticles = 0;
            world.forEach<ParticleEmitterComponent, ParticleEmitterRuntimeComponent>(
                [&](Entity, ParticleEmitterComponent& emitter, ParticleEmitterRuntimeComponent& runtime)
                {
                    if (emitter.effectPath != currentPath)
                        return;
                    liveEmitters += 1;
                    liveParticles += runtime.particles.size();
                });

            std::ostringstream out;
            out << (playbackPaused ? "PAUSED" : "LIVE") << "  " << liveEmitters << " E  " << liveParticles << " P";
            return out.str();
        }

        std::string moduleInspectorTitle() const
        {
            return "MODULES " + graphStatusLabel();
        }

        struct GraphDiagnosticCounts
        {
            size_t errors   = 0;
            size_t warnings = 0;
        };

        std::string graphStatusLabel() const
        {
            const GraphDiagnosticCounts counts = selectedGraphDiagnosticCounts();
            if (counts.errors > 0)
                return "GRAPH " + std::to_string(counts.errors) + " ERR";
            if (counts.warnings > 0)
                return "GRAPH " + std::to_string(counts.warnings) + " WARN";
            return "GRAPH OK";
        }

        GraphDiagnosticCounts selectedGraphDiagnosticCounts() const
        {
            const ParticleEffectEmitter* emitter = selectedEmitter();
            if (emitter == nullptr)
                return {};

            std::vector<gts::particles::ParticleModuleGraphDiagnostic> diagnostics;
            gts::particles::validateParticleModuleStack(emitter->modules, &diagnostics);

            GraphDiagnosticCounts counts;
            for (const gts::particles::ParticleModuleGraphDiagnostic& diagnostic : diagnostics)
            {
                if (diagnostic.severity == gts::particles::ParticleModuleGraphDiagnosticSeverity::Error)
                    counts.errors += 1;
                else
                    counts.warnings += 1;
            }
            return counts;
        }

        std::string emitterRowLabel(const ParticleEffectEmitter& emitter) const
        {
            std::string label = emitter.name.empty() ? emitter.stableId : emitter.name;
            if (isEmitterSelected(emitter))
                label = "* " + label;
            if (!emitter.descriptor.enabled)
                label = "OFF " + label;
            return compact(label, 24);
        }

        std::string moduleDisplayName(const ParticleModuleInstance& module) const
        {
            const ParticleModuleDefinition* definition = findParticleModuleDefinition(module.typeId);
            std::string label = module.displayName.empty()
                                    ? (definition == nullptr ? module.typeId : definition->displayName)
                                    : module.displayName;
            if (definition != nullptr && !definition->category.empty())
                label = std::string(executionStageShortLabel(definition->executionStage)) + " " + definition->category +
                        " " + label;
            if (!module.enabled)
                label = "OFF " + label;
            return compact(label, 24);
        }

        std::string selectedModuleStatus(const ParticleModuleInstance& module) const
        {
            const ParticleModuleDefinition* definition = findParticleModuleDefinition(module.typeId);
            if (definition == nullptr)
                return "MODULE UNKNOWN";

            std::string label = "MODULE ";
            label += executionStageLabel(definition->executionStage);
            label += " ";
            label += gts::particles::executionCategoryLabel(definition->executionCategory);
            label += " ";
            label += module.displayName.empty() ? definition->displayName : module.displayName;
            return compact(label, 48);
        }

        bool pointerOverModuleRows(const UiInteractionResult& interaction) const
        {
            for (const ToolButton& row : moduleRows)
            {
                if (interaction.hovered == row.rect || interaction.pressed == row.rect)
                    return true;
            }
            return false;
        }

        bool pointerOverParameterControls(const UiInteractionResult& interaction) const
        {
            for (const ParameterControl& control : parameterControls)
            {
                if (interaction.hovered == control.button.rect || interaction.pressed == control.button.rect ||
                    interaction.hovered == control.selector.rect || interaction.pressed == control.selector.rect ||
                    interaction.hovered == control.decrement.rect || interaction.pressed == control.decrement.rect ||
                    interaction.hovered == control.increment.rect || interaction.pressed == control.increment.rect ||
                    interaction.hovered == control.slider.track || interaction.pressed == control.slider.track)
                {
                    return true;
                }
            }
            return false;
        }

        void clampModuleSelection(ParticleEffectEmitter& emitter)
        {
            gts::particles::migrateParticleEmitterModules(emitter.modules, emitter.descriptor);
            if (emitter.modules.empty())
            {
                selectedModuleIndex    = 0;
                moduleBrowserOffset    = 0;
                parameterControlOffset = 0;
                return;
            }

            selectedModuleIndex = std::min(selectedModuleIndex, emitter.modules.size() - 1);
            moduleBrowserOffset = std::min(moduleBrowserOffset, maxModuleBrowserOffset(emitter));
            if (selectedModuleIndex < moduleBrowserOffset)
                moduleBrowserOffset = selectedModuleIndex;
            else if (selectedModuleIndex >= moduleBrowserOffset + ModuleRowCount)
                moduleBrowserOffset = selectedModuleIndex - ModuleRowCount + 1;
        }

        size_t moduleRowOffset(const ParticleEffectEmitter& emitter) const
        {
            if (emitter.modules.size() <= ModuleRowCount || selectedModuleIndex < ModuleRowCount)
                return 0;
            return std::min(selectedModuleIndex - ModuleRowCount + 1, emitter.modules.size() - ModuleRowCount);
        }

        size_t maxModuleBrowserOffset(const ParticleEffectEmitter& emitter) const
        {
            return emitter.modules.size() <= ModuleRowCount ? 0 : emitter.modules.size() - ModuleRowCount;
        }

        std::vector<ParameterView> editableParameters(const ParticleModuleDefinition& definition) const
        {
            std::vector<ParameterView> parameters;
            std::vector<std::string>   consumed;
            parameters.reserve(definition.parameters.size());
            for (const ParticleModuleParameterDefinition& parameter : definition.parameters)
            {
                if (std::find(consumed.begin(), consumed.end(), parameter.id) != consumed.end())
                    continue;

                const ParticleModuleParameterDefinition* partner = findRangePartner(definition, parameter);
                if (partner != nullptr)
                {
                    parameters.push_back({&parameter, partner});
                    consumed.push_back(partner->id);
                    continue;
                }

                parameters.push_back({&parameter, nullptr});
            }
            return parameters;
        }

        void clampParameterControlOffset(const std::vector<ParameterView>& parameters)
        {
            parameterControlOffset = std::min(parameterControlOffset, maxParameterControlOffset(parameters));
        }

        size_t maxParameterControlOffset(const std::vector<ParameterView>& parameters) const
        {
            return parameters.size() <= ParameterControlRowCount ? 0 : parameters.size() - ParameterControlRowCount;
        }

        const ParticleModuleParameterDefinition*
        findRangePartner(const ParticleModuleDefinition&          definition,
                         const ParticleModuleParameterDefinition& parameter) const
        {
            const std::string partnerId = rangePartnerId(parameter.id);
            if (partnerId.empty())
                return nullptr;

            const auto it = std::find_if(definition.parameters.begin(),
                                         definition.parameters.end(),
                                         [&](const ParticleModuleParameterDefinition& candidate)
                                         {
                                             return candidate.id == partnerId && candidate.type == parameter.type &&
                                                    isNumericType(candidate.type);
                                         });
            return it == definition.parameters.end() ? nullptr : &*it;
        }

        static std::string rangePartnerId(const std::string& id)
        {
            if (!endsWith(id, "Min"))
                return {};
            return id.substr(0, id.size() - 3) + "Max";
        }

        static bool endsWith(const std::string& value, const std::string& suffix)
        {
            return value.size() >= suffix.size() &&
                   value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
        }

        ParticleModuleParameter* ensureModuleParameter(ParticleModuleInstance&                  module,
                                                       const ParticleModuleParameterDefinition& definition) const
        {
            if (ParticleModuleParameter* parameter = gts::particles::findParameter(module, definition.id))
            {
                parameter->type = definition.type;
                return parameter;
            }

            module.parameters.push_back(gts::particles::makeDefaultParameter(definition));
            return &module.parameters.back();
        }

        bool applyParameterViewControl(UiSystem&                  ui,
                                       ECSWorld&                  world,
                                       EngineToolStateComponent&  state,
                                       const UiInteractionResult& interaction,
                                       ParticleEffectEmitter&     emitter,
                                       ParticleModuleInstance&    module,
                                       const ParameterView&       view,
                                       ParameterControl&          control)
        {
            if (view.primary == nullptr)
                return false;
            if (view.isRange())
                return applyRangeControl(ui, world, state, interaction, emitter, module, view, control);

            const ParticleModuleParameterDefinition& definition = *view.primary;
            if (definition.type == ParticleModuleParameterType::Float ||
                definition.type == ParticleModuleParameterType::UInt)
            {
                return applyNumericControl(ui, world, state, interaction, emitter, module, definition, control);
            }
            if (definition.type == ParticleModuleParameterType::FloatCurve ||
                definition.type == ParticleModuleParameterType::ColorGradient ||
                definition.type == ParticleModuleParameterType::BurstTimeline)
            {
                return applyRichControl(ui, world, state, interaction, emitter, module, definition, control);
            }
            return applyButtonControl(world, state, interaction, emitter, module, definition, control);
        }

        bool applyNumericControl(UiSystem&                                ui,
                                 ECSWorld&                                world,
                                 EngineToolStateComponent&                state,
                                 const UiInteractionResult&               interaction,
                                 ParticleEffectEmitter&                   emitter,
                                 ParticleModuleInstance&                  module,
                                 const ParticleModuleParameterDefinition& definition,
                                 const ParameterControl&                  control)
        {
            ParticleModuleParameter* parameter = ensureModuleParameter(module, definition);
            bool                     changed   = false;
            if (isPressed(interaction, control.slider.track))
            {
                beginUndoForHandle(control.slider.track);
                setNumericParameterValue(
                    *parameter, definition, valueFromSliderPointer(ui, control.slider, interaction.pointerX));
                changed = true;
            }
            else if (wasClicked(interaction, control.decrement.rect) || wasClicked(interaction, control.increment.rect))
            {
                pushUndoSnapshot();
                const float direction = wasClicked(interaction, control.increment.rect) ? 1.0f : -1.0f;
                setNumericParameterValue(*parameter,
                                         definition,
                                         numericParameterValue(*parameter, definition) +
                                             direction * numericStep(definition));
                changed = true;
            }

            if (!changed)
                return false;

            commitModuleEdit(world, state, emitter, "MODULE UPDATED");
            return true;
        }

        bool applyRangeControl(UiSystem&                  ui,
                               ECSWorld&                  world,
                               EngineToolStateComponent&  state,
                               const UiInteractionResult& interaction,
                               ParticleEffectEmitter&     emitter,
                               ParticleModuleInstance&    module,
                               const ParameterView&       view,
                               const ParameterControl&    control)
        {
            const std::string key = rangeViewKey(view);
            if (selectedRangeParameterId != key)
            {
                selectedRangeParameterId = key;
                selectedRangeMax         = false;
            }

            if (wasClicked(interaction, control.selector.rect))
            {
                selectedRangeMax = !selectedRangeMax;
                state.status     = selectedRangeMax ? "RANGE MAX" : "RANGE MIN";
                return true;
            }

            ParticleModuleParameter*                 minParameter = ensureModuleParameter(module, *view.primary);
            ParticleModuleParameter*                 maxParameter = ensureModuleParameter(module, *view.secondary);
            const ParticleModuleParameterDefinition& activeDefinition =
                selectedRangeMax ? *view.secondary : *view.primary;
            ParticleModuleParameter& activeParameter = selectedRangeMax ? *maxParameter : *minParameter;

            bool changed = false;
            if (isPressed(interaction, control.slider.track))
            {
                beginUndoForHandle(control.slider.track);
                setNumericParameterValue(activeParameter,
                                         activeDefinition,
                                         valueFromSliderPointer(ui, control.slider, interaction.pointerX));
                changed = true;
            }
            else if (wasClicked(interaction, control.decrement.rect) || wasClicked(interaction, control.increment.rect))
            {
                pushUndoSnapshot();
                const float direction = wasClicked(interaction, control.increment.rect) ? 1.0f : -1.0f;
                setNumericParameterValue(activeParameter,
                                         activeDefinition,
                                         numericParameterValue(activeParameter, activeDefinition) +
                                             direction * numericStep(activeDefinition));
                changed = true;
            }

            if (!changed)
                return false;

            normalizeRange(*minParameter, *maxParameter, *view.primary, *view.secondary);
            commitModuleEdit(world, state, emitter, "RANGE UPDATED");
            return true;
        }

        bool applyButtonControl(ECSWorld&                                world,
                                EngineToolStateComponent&                state,
                                const UiInteractionResult&               interaction,
                                ParticleEffectEmitter&                   emitter,
                                ParticleModuleInstance&                  module,
                                const ParticleModuleParameterDefinition& definition,
                                const ParameterControl&                  control)
        {
            const bool buttonClicked    = wasClicked(interaction, control.button.rect);
            const bool selectorClicked  = wasClicked(interaction, control.selector.rect);
            const bool decrementClicked = wasClicked(interaction, control.decrement.rect);
            const bool incrementClicked = wasClicked(interaction, control.increment.rect);
            if (!buttonClicked && !selectorClicked && !decrementClicked && !incrementClicked)
                return false;

            ParticleModuleParameter* parameter = ensureModuleParameter(module, definition);
            std::string              status    = "MODULE UPDATED";
            if (definition.type == ParticleModuleParameterType::Bool)
            {
                if (!buttonClicked)
                    return false;
                pushUndoSnapshot();
                parameter->boolValue = !parameter->boolValue;
            }
            else if (definition.type == ParticleModuleParameterType::Enum)
            {
                if (!buttonClicked)
                    return false;
                pushUndoSnapshot();
                parameter->uintValue = nextEnumValue(definition, parameter->uintValue);
            }
            else if (definition.type == ParticleModuleParameterType::String)
            {
                if (definition.assetPicker == gts::particles::ParticleModuleAssetPicker::None)
                {
                    if (!buttonClicked)
                        return false;
                    state.status = "NO PICKER";
                    return true;
                }

                pushUndoSnapshot();
                status = "ASSET PICKED";
                if (selectorClicked)
                {
                    parameter->stringValue =
                        parameter->stringValue.empty() ? assetPathAtOffset(definition.assetPicker,
                                                                           parameter->stringValue,
                                                                           1)
                                                       : std::string{};
                }
                else
                {
                    parameter->stringValue = assetPathAtOffset(
                        definition.assetPicker, parameter->stringValue, incrementClicked ? 1 : -1);
                }
            }
            else
            {
                return false;
            }

            commitModuleEdit(world, state, emitter, status);
            return true;
        }

        bool applyRichControl(UiSystem&                                ui,
                              ECSWorld&                                world,
                              EngineToolStateComponent&                state,
                              const UiInteractionResult&               interaction,
                              ParticleEffectEmitter&                   emitter,
                              ParticleModuleInstance&                  module,
                              const ParticleModuleParameterDefinition& definition,
                              const ParameterControl&                  control)
        {
            ParticleModuleParameter* parameter = ensureModuleParameter(module, definition);
            ensureRichParameterValue(*parameter, definition);
            clampRichSelection(*parameter, definition);

            if (wasClicked(interaction, control.selector.rect))
            {
                cycleRichSelection(*parameter, definition);
                state.status = richStatusLabel(definition);
                return true;
            }

            if (wasClicked(interaction, control.decrement.rect))
            {
                pushUndoSnapshot();
                deleteRichKey(*parameter, definition);
                commitModuleEdit(world, state, emitter, "KEY DELETED");
                return true;
            }

            if (wasClicked(interaction, control.increment.rect))
            {
                pushUndoSnapshot();
                addRichKey(*parameter, definition);
                commitModuleEdit(world, state, emitter, "KEY ADDED");
                return true;
            }

            if (!isPressed(interaction, control.slider.track))
                return false;

            beginUndoForHandle(control.slider.track);
            setRichSliderValue(
                *parameter, definition, valueFromSliderPointer(ui, control.slider, interaction.pointerX));
            commitModuleEdit(world, state, emitter, "CURVE UPDATED");
            return true;
        }

        void commitModuleEdit(ECSWorld&                 world,
                              EngineToolStateComponent& state,
                              ParticleEffectEmitter&    emitter,
                              const std::string&        status)
        {
            syncSelectedEmitterDescriptor(emitter);
            markDirty(state, status);
            applySelectedEmitterToLive(world, false);
        }

        void updateParameterViewControl(UiSystem&               ui,
                                        ParticleModuleInstance& module,
                                        const ParameterView&    view,
                                        ParameterControl&       control)
        {
            if (view.primary == nullptr)
            {
                setParameterControlVisible(ui, control, false, false, false, false, false, false);
                return;
            }
            if (view.isRange())
            {
                updateRangeControl(ui, module, view, control);
                return;
            }

            const ParticleModuleParameterDefinition& definition = *view.primary;
            ParticleModuleParameter*                 parameter  = ensureModuleParameter(module, definition);
            if (definition.type == ParticleModuleParameterType::Float ||
                definition.type == ParticleModuleParameterType::UInt)
            {
                updateNumericControl(ui, *parameter, definition, control);
            }
            else if (definition.type == ParticleModuleParameterType::FloatCurve ||
                     definition.type == ParticleModuleParameterType::ColorGradient ||
                     definition.type == ParticleModuleParameterType::BurstTimeline)
            {
                updateRichControl(ui, *parameter, definition, control);
            }
            else if (definition.type == ParticleModuleParameterType::String &&
                     definition.assetPicker != gts::particles::ParticleModuleAssetPicker::None)
            {
                setParameterControlVisible(ui, control, false, false, false, true, true, false);
                updateButton(ui, control.selector, assetPickerButtonLabel(*parameter, definition));
                updateButton(ui, control.decrement, "<");
                updateButton(ui, control.increment, ">");
            }
            else
            {
                setParameterControlVisible(ui, control, false, false, false, false, false, true);
                updateButton(ui, control.button, parameterButtonLabel(*parameter, definition));
            }
        }

        void updateNumericControl(UiSystem&                                ui,
                                  const ParticleModuleParameter&           parameter,
                                  const ParticleModuleParameterDefinition& definition,
                                  ParameterControl&                        control)
        {
            configureSlider(control.slider, definition, false);
            const float value = numericParameterValue(parameter, definition);
            setParameterControlVisible(ui, control, true, true, false, false, true, false);
            setText(ui, control.slider.label, definition.label + " " + formatValue(value, control.slider.wholeNumber));
            updateSlider(ui, control.slider, value, parameterSliderColor(definition));
            updateButton(ui, control.decrement, "-");
            updateButton(ui, control.increment, "+");
        }

        void updateRangeControl(UiSystem&               ui,
                                ParticleModuleInstance& module,
                                const ParameterView&    view,
                                ParameterControl&       control)
        {
            ParticleModuleParameter* minParameter = ensureModuleParameter(module, *view.primary);
            ParticleModuleParameter* maxParameter = ensureModuleParameter(module, *view.secondary);
            normalizeRange(*minParameter, *maxParameter, *view.primary, *view.secondary);

            const std::string key = rangeViewKey(view);
            if (selectedRangeParameterId != key)
            {
                selectedRangeParameterId = key;
                selectedRangeMax         = false;
            }

            const ParticleModuleParameterDefinition& activeDefinition =
                selectedRangeMax ? *view.secondary : *view.primary;
            const ParticleModuleParameter& activeParameter = selectedRangeMax ? *maxParameter : *minParameter;
            configureSlider(control.slider, activeDefinition, false);
            setParameterControlVisible(ui, control, true, false, false, true, true, false);
            updateSlider(ui,
                         control.slider,
                         numericParameterValue(activeParameter, activeDefinition),
                         parameterSliderColor(activeDefinition));
            updateButton(
                ui, control.selector, rangeButtonLabel(*minParameter, *maxParameter, *view.primary, *view.secondary));
            updateButton(ui, control.decrement, "-");
            updateButton(ui, control.increment, "+");
        }

        void updateRichControl(UiSystem&                                ui,
                               ParticleModuleParameter&                 parameter,
                               const ParticleModuleParameterDefinition& definition,
                               ParameterControl&                        control)
        {
            ensureRichParameterValue(parameter, definition);
            clampRichSelection(parameter, definition);
            configureRichSlider(control.slider, definition);
            setParameterControlVisible(ui, control, true, false, false, true, true, false);
            updateSlider(ui, control.slider, richSliderValue(parameter, definition), richSliderColor(definition));
            updateButton(ui, control.selector, richButtonLabel(parameter, definition));
            updateButton(ui, control.decrement, "-");
            updateButton(ui, control.increment, "+");
        }

        static void configureSlider(ToolSlider&                              slider,
                                    const ParticleModuleParameterDefinition& definition,
                                    bool                                     wholeNumberOverride)
        {
            slider.minValue = definition.minValue;
            slider.maxValue = definition.maxValue;
            slider.wholeNumber =
                wholeNumberOverride || definition.wholeNumber || definition.type == ParticleModuleParameterType::UInt;
        }

        static void setParameterControlVisible(UiSystem&               ui,
                                               const ParameterControl& control,
                                               bool                    sliderVisible,
                                               bool                    sliderLabelVisible,
                                               bool                    sliderValueVisible,
                                               bool                    selectorVisible,
                                               bool                    stepVisible,
                                               bool                    buttonVisible)
        {
            setVisibleRecursive(ui, control.slider.label, sliderVisible && sliderLabelVisible);
            setVisibleRecursive(ui, control.slider.value, sliderVisible && sliderValueVisible);
            setVisibleRecursive(ui, control.slider.track, sliderVisible);
            setVisibleRecursive(ui, control.selector.rect, selectorVisible);
            setVisibleRecursive(ui, control.decrement.rect, stepVisible);
            setVisibleRecursive(ui, control.increment.rect, stepVisible);
            setVisibleRecursive(ui, control.button.rect, buttonVisible);
        }

        static bool isNumericType(ParticleModuleParameterType type)
        {
            return type == ParticleModuleParameterType::Float || type == ParticleModuleParameterType::UInt;
        }

        static float numericParameterValue(const ParticleModuleParameter&           parameter,
                                           const ParticleModuleParameterDefinition& definition)
        {
            if (definition.type == ParticleModuleParameterType::UInt)
                return static_cast<float>(parameter.uintValue);
            return parameter.floatValue;
        }

        static void setNumericParameterValue(ParticleModuleParameter&                 parameter,
                                             const ParticleModuleParameterDefinition& definition,
                                             float                                    value)
        {
            value = std::clamp(value, definition.minValue, definition.maxValue);
            if (definition.type == ParticleModuleParameterType::UInt)
                parameter.uintValue = static_cast<uint32_t>(std::max(0.0f, std::round(value)));
            else
                parameter.floatValue = value;
        }

        static float numericStep(const ParticleModuleParameterDefinition& definition)
        {
            if (definition.type == ParticleModuleParameterType::UInt || definition.wholeNumber)
                return 1.0f;
            return std::max(0.01f, (definition.maxValue - definition.minValue) * 0.025f);
        }

        static void normalizeRange(ParticleModuleParameter&                 minParameter,
                                   ParticleModuleParameter&                 maxParameter,
                                   const ParticleModuleParameterDefinition& minDefinition,
                                   const ParticleModuleParameterDefinition& maxDefinition)
        {
            const float minValue = numericParameterValue(minParameter, minDefinition);
            const float maxValue = numericParameterValue(maxParameter, maxDefinition);
            if (maxValue < minValue)
                setNumericParameterValue(maxParameter, maxDefinition, minValue);
        }

        std::string rangeViewKey(const ParameterView& view) const
        {
            if (view.primary == nullptr || view.secondary == nullptr)
                return {};
            return view.primary->id + ":" + view.secondary->id;
        }

        std::string rangeButtonLabel(const ParticleModuleParameter&           minParameter,
                                     const ParticleModuleParameter&           maxParameter,
                                     const ParticleModuleParameterDefinition& minDefinition,
                                     const ParticleModuleParameterDefinition& maxDefinition) const
        {
            std::string  label = minDefinition.label;
            const size_t pos   = label.rfind(" MIN");
            if (pos != std::string::npos)
                label = label.substr(0, pos);
            label += " " + formatValue(numericParameterValue(minParameter, minDefinition), minDefinition.wholeNumber);
            label += ".." + formatValue(numericParameterValue(maxParameter, maxDefinition), maxDefinition.wholeNumber);
            label += selectedRangeMax ? " MAX" : " MIN";
            return compact(label, 24);
        }

        void ensureRichParameterValue(ParticleModuleParameter&                 parameter,
                                      const ParticleModuleParameterDefinition& definition) const
        {
            if (definition.type == ParticleModuleParameterType::FloatCurve)
            {
                if (parameter.floatCurveValue.empty())
                    parameter.floatCurveValue =
                        definition.defaultFloatCurve.empty()
                            ? ParticleFloatCurve{{0.0f, definition.minValue}, {1.0f, definition.maxValue}}
                            : definition.defaultFloatCurve;
                normalizeFloatCurve(parameter.floatCurveValue, definition.minValue, definition.maxValue);
            }
            else if (definition.type == ParticleModuleParameterType::ColorGradient)
            {
                if (parameter.colorGradientValue.empty())
                    parameter.colorGradientValue =
                        definition.defaultColorGradient.empty()
                            ? ParticleColorCurve{{0.0f, {1.0f, 1.0f, 1.0f, 1.0f}}, {1.0f, {1.0f, 1.0f, 1.0f, 1.0f}}}
                            : definition.defaultColorGradient;
                normalizeColorGradient(parameter.colorGradientValue);
            }
        }

        void clampRichSelection(const ParticleModuleParameter&           parameter,
                                const ParticleModuleParameterDefinition& definition)
        {
            if (selectedRichParameterId != definition.id)
            {
                selectedRichParameterId = definition.id;
                selectedRichKeyIndex    = 0;
                selectedRichField       = 0;
            }

            const size_t keyCount = richKeyCount(parameter, definition);
            if (keyCount == 0)
                selectedRichKeyIndex = 0;
            else
                selectedRichKeyIndex = std::min(selectedRichKeyIndex, keyCount - 1);
            selectedRichField = selectedRichField % richFieldCount(definition);
        }

        void cycleRichSelection(const ParticleModuleParameter&           parameter,
                                const ParticleModuleParameterDefinition& definition)
        {
            clampRichSelection(parameter, definition);
            selectedRichField += 1;
            if (selectedRichField < richFieldCount(definition))
                return;

            selectedRichField     = 0;
            const size_t keyCount = richKeyCount(parameter, definition);
            selectedRichKeyIndex  = keyCount == 0 ? 0 : (selectedRichKeyIndex + 1) % keyCount;
        }

        void configureRichSlider(ToolSlider& slider, const ParticleModuleParameterDefinition& definition) const
        {
            slider.wholeNumber = false;
            if (definition.type == ParticleModuleParameterType::FloatCurve)
            {
                slider.minValue = selectedRichField == 0 ? 0.0f : definition.minValue;
                slider.maxValue = selectedRichField == 0 ? 1.0f : definition.maxValue;
                return;
            }
            if (definition.type == ParticleModuleParameterType::ColorGradient)
            {
                slider.minValue = 0.0f;
                slider.maxValue = 1.0f;
                return;
            }
            if (definition.type == ParticleModuleParameterType::BurstTimeline)
            {
                slider.minValue = 0.0f;
                slider.maxValue = selectedRichField == 4 ? 64.0f : (selectedRichField == 3 ? 12.0f : 512.0f);
                if (selectedRichField == 0)
                    slider.maxValue = 12.0f;
                slider.wholeNumber = selectedRichField == 1 || selectedRichField == 2 || selectedRichField == 4;
            }
        }

        float richSliderValue(const ParticleModuleParameter&           parameter,
                              const ParticleModuleParameterDefinition& definition) const
        {
            if (definition.type == ParticleModuleParameterType::FloatCurve)
            {
                if (parameter.floatCurveValue.empty())
                    return 0.0f;
                const ParticleFloatKey& key = parameter.floatCurveValue[selectedRichKeyIndex];
                return selectedRichField == 0 ? key.t : key.value;
            }
            if (definition.type == ParticleModuleParameterType::ColorGradient)
            {
                if (parameter.colorGradientValue.empty())
                    return 0.0f;
                const ParticleColorKey& key = parameter.colorGradientValue[selectedRichKeyIndex];
                if (selectedRichField == 0)
                    return key.t;
                return key.color[static_cast<int>(selectedRichField - 1)];
            }
            if (definition.type == ParticleModuleParameterType::BurstTimeline)
            {
                if (parameter.burstTimelineValue.empty())
                    return 0.0f;
                const ParticleBurst& burst = parameter.burstTimelineValue[selectedRichKeyIndex];
                switch (selectedRichField)
                {
                case 0:
                    return burst.time;
                case 1:
                    return static_cast<float>(burst.countMin);
                case 2:
                    return static_cast<float>(burst.countMax);
                case 3:
                    return burst.repeatInterval;
                case 4:
                    return static_cast<float>(burst.repeatCount);
                }
            }
            return 0.0f;
        }

        void setRichSliderValue(ParticleModuleParameter&                 parameter,
                                const ParticleModuleParameterDefinition& definition,
                                float                                    value)
        {
            ensureRichParameterValue(parameter, definition);
            clampRichSelection(parameter, definition);

            if (definition.type == ParticleModuleParameterType::FloatCurve)
            {
                ParticleFloatKey& key = parameter.floatCurveValue[selectedRichKeyIndex];
                if (selectedRichField == 0)
                    key.t = std::clamp(value, 0.0f, 1.0f);
                else
                    key.value = std::clamp(value, definition.minValue, definition.maxValue);
                normalizeFloatCurve(parameter.floatCurveValue, definition.minValue, definition.maxValue);
            }
            else if (definition.type == ParticleModuleParameterType::ColorGradient)
            {
                ParticleColorKey& key = parameter.colorGradientValue[selectedRichKeyIndex];
                if (selectedRichField == 0)
                    key.t = std::clamp(value, 0.0f, 1.0f);
                else
                    key.color[static_cast<int>(selectedRichField - 1)] = std::clamp(value, 0.0f, 1.0f);
                normalizeColorGradient(parameter.colorGradientValue);
            }
            else if (definition.type == ParticleModuleParameterType::BurstTimeline)
            {
                if (parameter.burstTimelineValue.empty())
                    parameter.burstTimelineValue.push_back({0.0f, 8u, 8u, 0.0f, 0u});
                ParticleBurst& burst = parameter.burstTimelineValue[selectedRichKeyIndex];
                switch (selectedRichField)
                {
                case 0:
                    burst.time = std::clamp(value, 0.0f, 12.0f);
                    break;
                case 1:
                    burst.countMin = static_cast<uint32_t>(std::max(0.0f, std::round(value)));
                    break;
                case 2:
                    burst.countMax = static_cast<uint32_t>(std::max(0.0f, std::round(value)));
                    break;
                case 3:
                    burst.repeatInterval = std::clamp(value, 0.0f, 12.0f);
                    break;
                case 4:
                    burst.repeatCount = static_cast<uint32_t>(std::max(0.0f, std::round(value)));
                    break;
                }
                normalizeBursts(parameter.burstTimelineValue);
            }
        }

        void addRichKey(ParticleModuleParameter& parameter, const ParticleModuleParameterDefinition& definition)
        {
            ensureRichParameterValue(parameter, definition);
            clampRichSelection(parameter, definition);

            if (definition.type == ParticleModuleParameterType::FloatCurve)
            {
                const ParticleFloatKey base = parameter.floatCurveValue[selectedRichKeyIndex];
                ParticleFloatKey       key  = base;
                key.t                       = nextInsertedTime(parameter.floatCurveValue, selectedRichKeyIndex);
                parameter.floatCurveValue.push_back(key);
                normalizeFloatCurve(parameter.floatCurveValue, definition.minValue, definition.maxValue);
                selectedRichKeyIndex = nearestFloatKey(parameter.floatCurveValue, key.t);
            }
            else if (definition.type == ParticleModuleParameterType::ColorGradient)
            {
                const ParticleColorKey base = parameter.colorGradientValue[selectedRichKeyIndex];
                ParticleColorKey       key  = base;
                key.t                       = nextInsertedTime(parameter.colorGradientValue, selectedRichKeyIndex);
                parameter.colorGradientValue.push_back(key);
                normalizeColorGradient(parameter.colorGradientValue);
                selectedRichKeyIndex = nearestColorKey(parameter.colorGradientValue, key.t);
            }
            else if (definition.type == ParticleModuleParameterType::BurstTimeline)
            {
                ParticleBurst burst = parameter.burstTimelineValue.empty()
                                          ? ParticleBurst{0.0f, 8u, 8u, 0.0f, 0u}
                                          : parameter.burstTimelineValue[selectedRichKeyIndex];
                burst.time          = std::min(12.0f, burst.time + 0.1f);
                parameter.burstTimelineValue.push_back(burst);
                normalizeBursts(parameter.burstTimelineValue);
                selectedRichKeyIndex = nearestBurst(parameter.burstTimelineValue, burst.time);
            }
        }

        void deleteRichKey(ParticleModuleParameter& parameter, const ParticleModuleParameterDefinition& definition)
        {
            ensureRichParameterValue(parameter, definition);
            clampRichSelection(parameter, definition);

            if (definition.type == ParticleModuleParameterType::FloatCurve && parameter.floatCurveValue.size() > 2)
                parameter.floatCurveValue.erase(parameter.floatCurveValue.begin() +
                                                static_cast<std::ptrdiff_t>(selectedRichKeyIndex));
            else if (definition.type == ParticleModuleParameterType::ColorGradient &&
                     parameter.colorGradientValue.size() > 2)
                parameter.colorGradientValue.erase(parameter.colorGradientValue.begin() +
                                                   static_cast<std::ptrdiff_t>(selectedRichKeyIndex));
            else if (definition.type == ParticleModuleParameterType::BurstTimeline &&
                     !parameter.burstTimelineValue.empty())
                parameter.burstTimelineValue.erase(parameter.burstTimelineValue.begin() +
                                                   static_cast<std::ptrdiff_t>(selectedRichKeyIndex));

            clampRichSelection(parameter, definition);
        }

        std::string richButtonLabel(const ParticleModuleParameter&           parameter,
                                    const ParticleModuleParameterDefinition& definition) const
        {
            if (definition.type == ParticleModuleParameterType::BurstTimeline && parameter.burstTimelineValue.empty())
            {
                return compact(definition.label + " EMPTY", 24);
            }

            std::string label = definition.label + " K" + std::to_string(selectedRichKeyIndex + 1) + " ";
            label += richFieldLabel(definition);
            label += " " + formatValue(richSliderValue(parameter, definition), isRichFieldWholeNumber(definition));
            return compact(label, 24);
        }

        std::string richStatusLabel(const ParticleModuleParameterDefinition& definition) const
        {
            return definition.label + " " + richFieldLabel(definition);
        }

        std::string richFieldLabel(const ParticleModuleParameterDefinition& definition) const
        {
            if (definition.type == ParticleModuleParameterType::FloatCurve)
                return selectedRichField == 0 ? "T" : "V";
            if (definition.type == ParticleModuleParameterType::ColorGradient)
            {
                static const char* labels[] = {"T", "R", "G", "B", "A"};
                return labels[std::min<uint32_t>(selectedRichField, 4u)];
            }
            if (definition.type == ParticleModuleParameterType::BurstTimeline)
            {
                static const char* labels[] = {"TIME", "MIN", "MAX", "REPEAT", "LOOPS"};
                return labels[std::min<uint32_t>(selectedRichField, 4u)];
            }
            return "VALUE";
        }

        static size_t richKeyCount(const ParticleModuleParameter&           parameter,
                                   const ParticleModuleParameterDefinition& definition)
        {
            if (definition.type == ParticleModuleParameterType::FloatCurve)
                return parameter.floatCurveValue.size();
            if (definition.type == ParticleModuleParameterType::ColorGradient)
                return parameter.colorGradientValue.size();
            if (definition.type == ParticleModuleParameterType::BurstTimeline)
                return parameter.burstTimelineValue.size();
            return 0;
        }

        static uint32_t richFieldCount(const ParticleModuleParameterDefinition& definition)
        {
            if (definition.type == ParticleModuleParameterType::FloatCurve)
                return 2u;
            if (definition.type == ParticleModuleParameterType::ColorGradient)
                return 5u;
            if (definition.type == ParticleModuleParameterType::BurstTimeline)
                return 5u;
            return 1u;
        }

        bool isRichFieldWholeNumber(const ParticleModuleParameterDefinition& definition) const
        {
            return definition.type == ParticleModuleParameterType::BurstTimeline &&
                   (selectedRichField == 1 || selectedRichField == 2 || selectedRichField == 4);
        }

        UiColor richSliderColor(const ParticleModuleParameterDefinition& definition) const
        {
            if (definition.type == ParticleModuleParameterType::ColorGradient)
            {
                if (selectedRichField == 1)
                    return color(0.88f, 0.22f, 0.24f, 1.0f);
                if (selectedRichField == 2)
                    return color(0.20f, 0.72f, 0.36f, 1.0f);
                if (selectedRichField == 3)
                    return color(0.22f, 0.50f, 0.92f, 1.0f);
                if (selectedRichField == 4)
                    return color(0.72f, 0.76f, 0.82f, 1.0f);
            }
            if (definition.type == ParticleModuleParameterType::BurstTimeline)
                return color(0.88f, 0.62f, 0.22f, 1.0f);
            return parameterSliderColor(definition);
        }

        template <typename Curve> static float nextInsertedTime(const Curve& curve, size_t index)
        {
            if (curve.empty())
                return 0.0f;
            if (index + 1 < curve.size())
                return std::clamp((curve[index].t + curve[index + 1].t) * 0.5f, 0.0f, 1.0f);
            return std::clamp((curve[index].t + 1.0f) * 0.5f, 0.0f, 1.0f);
        }

        static void normalizeFloatCurve(ParticleFloatCurve& curve, float minValue, float maxValue)
        {
            for (ParticleFloatKey& key : curve)
            {
                key.t     = std::clamp(key.t, 0.0f, 1.0f);
                key.value = std::clamp(key.value, minValue, maxValue);
            }
            std::sort(curve.begin(),
                      curve.end(),
                      [](const ParticleFloatKey& lhs, const ParticleFloatKey& rhs)
                      {
                          return lhs.t < rhs.t;
                      });
        }

        static void normalizeColorGradient(ParticleColorCurve& curve)
        {
            for (ParticleColorKey& key : curve)
            {
                key.t     = std::clamp(key.t, 0.0f, 1.0f);
                key.color = glm::clamp(key.color, glm::vec4(0.0f), glm::vec4(1.0f));
            }
            std::sort(curve.begin(),
                      curve.end(),
                      [](const ParticleColorKey& lhs, const ParticleColorKey& rhs)
                      {
                          return lhs.t < rhs.t;
                      });
        }

        static void normalizeBursts(std::vector<ParticleBurst>& bursts)
        {
            for (ParticleBurst& burst : bursts)
            {
                burst.time           = std::clamp(burst.time, 0.0f, 12.0f);
                burst.countMax       = std::max(burst.countMin, burst.countMax);
                burst.repeatInterval = std::max(0.0f, burst.repeatInterval);
            }
            std::sort(bursts.begin(),
                      bursts.end(),
                      [](const ParticleBurst& lhs, const ParticleBurst& rhs)
                      {
                          return lhs.time < rhs.time;
                      });
        }

        static size_t nearestFloatKey(const ParticleFloatCurve& curve, float t)
        {
            return nearestByTime(curve, t);
        }

        static size_t nearestColorKey(const ParticleColorCurve& curve, float t)
        {
            return nearestByTime(curve, t);
        }

        static size_t nearestBurst(const std::vector<ParticleBurst>& bursts, float time)
        {
            if (bursts.empty())
                return 0;
            size_t best         = 0;
            float  bestDistance = std::numeric_limits<float>::max();
            for (size_t i = 0; i < bursts.size(); ++i)
            {
                const float distance = std::abs(bursts[i].time - time);
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    best         = i;
                }
            }
            return best;
        }

        template <typename Curve> static size_t nearestByTime(const Curve& curve, float t)
        {
            if (curve.empty())
                return 0;
            size_t best         = 0;
            float  bestDistance = std::numeric_limits<float>::max();
            for (size_t i = 0; i < curve.size(); ++i)
            {
                const float distance = std::abs(curve[i].t - t);
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    best         = i;
                }
            }
            return best;
        }

        static UiColor parameterSliderColor(const ParticleModuleParameterDefinition& definition)
        {
            if (definition.id.find("TintR") != std::string::npos ||
                definition.id.find("baseTintR") != std::string::npos)
                return color(0.88f, 0.22f, 0.24f, 1.0f);
            if (definition.id.find("TintG") != std::string::npos ||
                definition.id.find("baseTintG") != std::string::npos)
                return color(0.20f, 0.72f, 0.36f, 1.0f);
            if (definition.id.find("TintB") != std::string::npos ||
                definition.id.find("baseTintB") != std::string::npos)
                return color(0.22f, 0.50f, 0.92f, 1.0f);
            if (definition.id.find("alpha") != std::string::npos || definition.id.find("Alpha") != std::string::npos)
                return color(0.72f, 0.76f, 0.82f, 1.0f);
            return ToolTheme::accent;
        }

        std::string parameterButtonLabel(const ParticleModuleParameter&           parameter,
                                         const ParticleModuleParameterDefinition& definition) const
        {
            if (definition.type == ParticleModuleParameterType::Bool)
                return definition.label + (parameter.boolValue ? " ON" : " OFF");
            if (definition.type == ParticleModuleParameterType::Enum)
                return definition.label + " " + enumLabel(definition, parameter.uintValue);
            if (definition.type == ParticleModuleParameterType::String)
                return definition.label + " " +
                       compact(parameter.stringValue.empty() ? "-" : fileName(parameter.stringValue), 14);
            return definition.label;
        }

        std::string assetPickerButtonLabel(const ParticleModuleParameter&           parameter,
                                           const ParticleModuleParameterDefinition& definition) const
        {
            return definition.label + " " +
                   compact(parameter.stringValue.empty() ? "NONE" : fileName(parameter.stringValue), 22);
        }

        std::string assetPathAtOffset(gts::particles::ParticleModuleAssetPicker picker,
                                      const std::string&                        current,
                                      int                                      offset) const
        {
            std::vector<std::string> paths = pickerChoices(picker, current);
            if (paths.empty())
                return current;

            auto it = std::find(paths.begin(), paths.end(), current);
            if (it == paths.end())
                it = paths.begin();

            const int count = static_cast<int>(paths.size());
            int       index = static_cast<int>(std::distance(paths.begin(), it));
            if (current.empty() && offset > 0 && paths.size() > 1u)
                index = 0;
            index = (index + offset) % count;
            if (index < 0)
                index += count;
            return paths[static_cast<size_t>(index)];
        }

        static std::vector<std::string> pickerChoices(gts::particles::ParticleModuleAssetPicker picker,
                                                      const std::string&                        current)
        {
            std::vector<std::string> paths = collectPickerPaths(picker);
            if (!current.empty())
                paths.push_back(current);
            paths.insert(paths.begin(), {});
            sortUnique(paths);
            return paths;
        }

        static std::vector<std::string> collectPickerPaths(gts::particles::ParticleModuleAssetPicker picker)
        {
            std::vector<std::string> paths;
            if (picker == gts::particles::ParticleModuleAssetPicker::None)
                return paths;

            const std::vector<std::string> extensions =
                picker == gts::particles::ParticleModuleAssetPicker::Texture
                    ? std::vector<std::string>{".png", ".jpg", ".jpeg", ".ktx", ".ktx2"}
                    : std::vector<std::string>{".obj", ".gltf", ".glb"};
            const std::vector<std::filesystem::path> roots = {
                "resources",
                "../resources",
                "../../resources",
                "../../../resources",
                "engine/resources",
                "../engine/resources",
                "../../engine/resources",
            };

            for (const std::filesystem::path& rootPath : roots)
                scanPickerDirectory(paths, rootPath, extensions);
            sortUnique(paths);
            return paths;
        }

        static void scanPickerDirectory(std::vector<std::string>&       paths,
                                        const std::filesystem::path&    rootPath,
                                        const std::vector<std::string>& extensions)
        {
            std::error_code ec;
            if (!std::filesystem::exists(rootPath, ec) || !std::filesystem::is_directory(rootPath, ec))
                return;

            const std::filesystem::recursive_directory_iterator end;
            std::filesystem::recursive_directory_iterator       it(
                rootPath, std::filesystem::directory_options::skip_permission_denied, ec);
            while (!ec && it != end)
            {
                const std::filesystem::directory_entry& entry = *it;
                if (entry.is_regular_file(ec) && extensionAllowed(entry.path().extension().string(), extensions))
                    paths.push_back(entry.path().generic_string());
                it.increment(ec);
            }
        }

        static bool extensionAllowed(std::string extension, const std::vector<std::string>& allowed)
        {
            std::transform(extension.begin(),
                           extension.end(),
                           extension.begin(),
                           [](unsigned char ch)
                           {
                               return static_cast<char>(std::tolower(ch));
                           });
            return std::find(allowed.begin(), allowed.end(), extension) != allowed.end();
        }

        static std::string enumLabel(const ParticleModuleParameterDefinition& definition, uint32_t value)
        {
            for (const ParticleModuleEnumOption& option : definition.enumOptions)
            {
                if (option.value == value)
                    return option.label;
            }
            return std::to_string(value);
        }

        static uint32_t nextEnumValue(const ParticleModuleParameterDefinition& definition, uint32_t current)
        {
            if (definition.enumOptions.empty())
                return current;

            for (size_t i = 0; i < definition.enumOptions.size(); ++i)
            {
                if (definition.enumOptions[i].value != current)
                    continue;
                return definition.enumOptions[(i + 1) % definition.enumOptions.size()].value;
            }
            return definition.enumOptions.front().value;
        }

        void syncSelectedEmitterDescriptor(ParticleEffectEmitter& emitter)
        {
            gts::particles::migrateParticleEmitterModules(emitter.modules, emitter.descriptor);
            gts::particles::applyParticleModulesToEmitterDescriptor(emitter.modules, emitter.descriptor);
        }

        static void setEmitterEnabledModuleParameter(ParticleEffectEmitter& emitter, bool enabled)
        {
            for (ParticleModuleInstance& module : emitter.modules)
            {
                if (module.typeId != "spawn.basic")
                    continue;
                if (ParticleModuleParameter* parameter = gts::particles::findParameter(module, "emitterEnabled"))
                    parameter->boolValue = enabled;
                return;
            }
        }

        void applyKeyboardShortcuts(EngineToolContext& ctx, EngineToolStateComponent& state)
        {
            if (activeTextField != ActiveTextField::None || ctx.input == nullptr)
                return;

            const std::optional<InputTrigger> trigger = ctx.input->getLastPressedTrigger();
            if (!trigger.has_value() || trigger->type != InputTrigger::Type::Key ||
                !has(trigger->modifiers, ModifierFlags::Ctrl))
            {
                return;
            }

            const GtsKey key = static_cast<GtsKey>(trigger->code);
            if (key == GtsKey::Z)
                undo(ctx.world, state);
            else if (key == GtsKey::Y)
                redo(ctx.world, state);
            else if (key == GtsKey::C)
                copyModule(state);
            else if (key == GtsKey::V)
                pasteModule(ctx.world, state);
        }

        void releaseUndoCapture(const UiInteractionResult& interaction)
        {
            if (interaction.pressed == UI_INVALID_HANDLE)
                activeUndoHandle = UI_INVALID_HANDLE;
        }

        void beginUndoForHandle(UiHandle handle)
        {
            if (handle == UI_INVALID_HANDLE || activeUndoHandle == handle)
                return;
            pushUndoSnapshot();
            activeUndoHandle = handle;
        }

        void pushUndoSnapshot()
        {
            if (!hasAsset)
                return;
            undoStack.push_back(currentAsset);
            if (undoStack.size() > MaxUndoSnapshots)
                undoStack.erase(undoStack.begin());
            redoStack.clear();
        }

        void undo(ECSWorld& world, EngineToolStateComponent& state)
        {
            if (undoStack.empty())
            {
                state.status = "NOTHING TO UNDO";
                return;
            }

            redoStack.push_back(currentAsset);
            currentAsset = undoStack.back();
            undoStack.pop_back();
            restoreAfterHistory(world, state, "UNDO");
        }

        void redo(ECSWorld& world, EngineToolStateComponent& state)
        {
            if (redoStack.empty())
            {
                state.status = "NOTHING TO REDO";
                return;
            }

            undoStack.push_back(currentAsset);
            currentAsset = redoStack.back();
            redoStack.pop_back();
            restoreAfterHistory(world, state, "REDO");
        }

        void restoreAfterHistory(ECSWorld& world, EngineToolStateComponent& state, const std::string& status)
        {
            dirty = true;
            selectedEmitterIndex =
                std::min(selectedEmitterIndex, currentAsset.emitters.empty() ? 0 : currentAsset.emitters.size() - 1);
            selectedModuleIndex    = 0;
            moduleBrowserOffset    = 0;
            parameterControlOffset = 0;
            selectedEmitterIds.clear();
            clearParameterSelectionState();
            syncEmitterNameDraft();
            state.status = status;
            applySelectedEmitterToLive(world, true);
        }

        void copyModule(EngineToolStateComponent& state)
        {
            const ParticleModuleInstance* module = selectedModule();
            if (module == nullptr)
            {
                state.status = "NO MODULE";
                return;
            }

            moduleClipboard = *module;
            state.status    = "MODULE COPIED";
        }

        void pasteModule(ECSWorld& world, EngineToolStateComponent& state)
        {
            ParticleEffectEmitter*  emitter = selectedEmitter();
            ParticleModuleInstance* module  = selectedModule();
            if (emitter == nullptr || module == nullptr)
            {
                state.status = "NO MODULE";
                return;
            }
            if (!moduleClipboard.has_value())
            {
                state.status = "NO MODULE COPY";
                return;
            }
            if (moduleClipboard->typeId != module->typeId)
            {
                state.status = "MODULE TYPE MISMATCH";
                return;
            }

            pushUndoSnapshot();
            const std::string stableId = module->stableId;
            *module                    = *moduleClipboard;
            module->stableId           = stableId;
            syncSelectedEmitterDescriptor(*emitter);
            markDirty(state, "MODULE PASTED");
            applySelectedEmitterToLive(world, false);
        }

        void copyEmitters(EngineToolStateComponent& state)
        {
            emitterClipboard.clear();
            const std::vector<size_t> targets = selectedEmitterTargetIndices();
            if (!targets.empty())
            {
                for (size_t index : targets)
                    emitterClipboard.push_back(currentAsset.emitters[index]);
            }
            else if (const ParticleEffectEmitter* emitter = selectedEmitter())
            {
                emitterClipboard.push_back(*emitter);
            }

            state.status = emitterClipboard.empty() ? "NO EMITTER" : "EMITTER COPIED";
        }

        void pasteEmitters(EngineToolStateComponent& state)
        {
            if (emitterClipboard.empty())
            {
                state.status = "NO EMITTER COPY";
                return;
            }

            pushUndoSnapshot();
            size_t insertIndex = std::min(selectedEmitterIndex + 1, currentAsset.emitters.size());
            selectedEmitterIds.clear();
            for (const ParticleEffectEmitter& copied : emitterClipboard)
            {
                ParticleEffectEmitter pasted = copied;
                pasted.stableId              = uniqueEmitterId(copied.stableId.empty() ? "emitter" : copied.stableId);
                pasted.name                  = copied.name.empty() ? "Emitter Copy" : copied.name + " Copy";
                currentAsset.emitters.insert(currentAsset.emitters.begin() + static_cast<std::ptrdiff_t>(insertIndex),
                                             std::move(pasted));
                selectedEmitterIndex = insertIndex;
                insertIndex += 1;
            }
            selectedModuleIndex    = 0;
            moduleBrowserOffset    = 0;
            parameterControlOffset = 0;
            clearParameterSelectionState();
            syncEmitterNameDraft();
            markDirty(state, "EMITTER PASTED");
        }

        void toggleSelectedEmitterSelection(EngineToolStateComponent& state)
        {
            ParticleEffectEmitter* emitter = selectedEmitter();
            if (emitter == nullptr)
            {
                state.status = "NO EMITTER";
                return;
            }

            const auto it = std::find(selectedEmitterIds.begin(), selectedEmitterIds.end(), emitter->stableId);
            if (it == selectedEmitterIds.end())
            {
                selectedEmitterIds.push_back(emitter->stableId);
                state.status = "EMITTER SELECTED";
            }
            else
            {
                selectedEmitterIds.erase(it);
                state.status = "EMITTER UNSELECTED";
            }
        }

        bool isEmitterSelected(const ParticleEffectEmitter& emitter) const
        {
            return std::find(selectedEmitterIds.begin(), selectedEmitterIds.end(), emitter.stableId) !=
                   selectedEmitterIds.end();
        }

        std::vector<size_t> selectedEmitterTargetIndices() const
        {
            std::vector<size_t> targets;
            for (size_t i = 0; i < currentAsset.emitters.size(); ++i)
            {
                if (isEmitterSelected(currentAsset.emitters[i]))
                    targets.push_back(i);
            }
            return targets;
        }

        void deleteSelectedEmitters(EngineToolStateComponent& state, std::vector<size_t> targets)
        {
            if (targets.empty())
                return;
            if (targets.size() >= currentAsset.emitters.size())
            {
                state.status = "KEEP ONE EMITTER";
                return;
            }

            pushUndoSnapshot();
            std::sort(targets.rbegin(), targets.rend());
            for (size_t index : targets)
                currentAsset.emitters.erase(currentAsset.emitters.begin() + static_cast<std::ptrdiff_t>(index));
            selectedEmitterIds.clear();
            selectedEmitterIndex   = std::min(selectedEmitterIndex, currentAsset.emitters.size() - 1);
            selectedModuleIndex    = 0;
            moduleBrowserOffset    = 0;
            parameterControlOffset = 0;
            clearParameterSelectionState();
            syncEmitterNameDraft();
            markDirty(state, "EMITTERS DELETED");
        }

        void duplicateSelectedEmitters(EngineToolStateComponent& state, const std::vector<size_t>& targets)
        {
            if (targets.empty())
                return;

            pushUndoSnapshot();
            size_t insertIndex = std::min(selectedEmitterIndex + 1, currentAsset.emitters.size());
            std::vector<ParticleEffectEmitter> copies;
            copies.reserve(targets.size());
            for (size_t index : targets)
                copies.push_back(currentAsset.emitters[index]);

            selectedEmitterIds.clear();
            for (ParticleEffectEmitter& copy : copies)
            {
                copy.stableId = uniqueEmitterId(copy.stableId.empty() ? "emitter" : copy.stableId);
                copy.name     = copy.name.empty() ? "Emitter Copy" : copy.name + " Copy";
                currentAsset.emitters.insert(currentAsset.emitters.begin() + static_cast<std::ptrdiff_t>(insertIndex),
                                             std::move(copy));
                selectedEmitterIndex = insertIndex;
                insertIndex += 1;
            }
            selectedModuleIndex    = 0;
            moduleBrowserOffset    = 0;
            parameterControlOffset = 0;
            clearParameterSelectionState();
            syncEmitterNameDraft();
            markDirty(state, "EMITTERS COPIED");
        }

        std::string effectButtonLabel(EffectAction action) const
        {
            switch (action)
            {
            case EffectAction::Save:
                return dirty ? "SAVE *" : "SAVE";
            case EffectAction::SaveAs:
                return "SAVE AS";
            case EffectAction::Duplicate:
                return "COPY";
            case EffectAction::Reload:
                return "RELOAD";
            }
            return "";
        }

        std::string emitterButtonLabel(EmitterAction action) const
        {
            switch (action)
            {
            case EmitterAction::ToggleEnabled:
            {
                const ParticleEffectEmitter* selected = selectedEmitter();
                return selected != nullptr && selected->descriptor.enabled ? "ON" : "OFF";
            }
            case EmitterAction::Add:
                return "ADD";
            case EmitterAction::Delete:
                return "DEL";
            case EmitterAction::Duplicate:
                return "DUP";
            case EmitterAction::Rename:
                return "NAME";
            case EmitterAction::MoveUp:
                return "UP";
            case EmitterAction::MoveDown:
                return "DOWN";
            case EmitterAction::ToggleSelection:
                return selectedEmitter() != nullptr && isEmitterSelected(*selectedEmitter()) ? "SEL *" : "SEL";
            case EmitterAction::Copy:
                return selectedEmitterIds.empty() ? "COPY" : "COPY " + std::to_string(selectedEmitterIds.size());
            case EmitterAction::Paste:
                return emitterClipboard.empty() ? "PASTE" : "PASTE *";
            }
            return "";
        }

        std::string moduleButtonLabel(ModuleAction action) const
        {
            switch (action)
            {
            case ModuleAction::Copy:
                return "M COPY";
            case ModuleAction::Paste:
                return moduleClipboard.has_value() ? "M PASTE *" : "M PASTE";
            case ModuleAction::Undo:
                return undoStack.empty() ? "UNDO" : "UNDO *";
            case ModuleAction::Redo:
                return redoStack.empty() ? "REDO" : "REDO *";
            }
            return "";
        }

        std::string playbackButtonLabel(PlaybackAction action) const
        {
            switch (action)
            {
            case PlaybackAction::PlayPause:
                return playbackPaused ? "PLAY" : "PAUSE";
            case PlaybackAction::Restart:
                return "RESET";
            case PlaybackAction::Background:
                return "BG";
            case PlaybackAction::CameraReset:
                return "CAM";
            }
            return "";
        }

        float readSliderValue(FloatField field) const
        {
            switch (field)
            {
            case FloatField::TimeScale:
                return playbackTimeScale;
            case FloatField::OrbitYaw:
                return previewOrbitYawDegrees();
            }
            return 0.0f;
        }

        float previewOrbitYawDegrees() const
        {
            const glm::vec3 offset = currentAsset.preview.cameraPosition - currentAsset.preview.cameraTarget;
            return glm::degrees(std::atan2(offset.x, offset.z));
        }

        void setPreviewOrbitYawDegrees(float degrees)
        {
            const float     yawRadians     = glm::radians(degrees);
            const glm::vec3 offset         = currentAsset.preview.cameraPosition - currentAsset.preview.cameraTarget;
            const float     planarDistance = std::sqrt(offset.x * offset.x + offset.z * offset.z);
            const float     radius =
                std::max(0.1f, planarDistance > 0.001f ? planarDistance : currentAsset.preview.orbitDistance);
            currentAsset.preview.orbitDistance = radius;
            currentAsset.preview.cameraPosition =
                currentAsset.preview.cameraTarget +
                glm::vec3(std::sin(yawRadians) * radius, offset.y, std::cos(yawRadians) * radius);
        }

        UiColor previewColor() const
        {
            if (!hasAsset)
                return ToolTheme::paneSurface;
            const glm::vec4& colorValue = currentAsset.preview.backgroundColor;
            return {colorValue.r, colorValue.g, colorValue.b, std::max(0.35f, colorValue.a)};
        }

        static UiColor sliderColor(FloatField field)
        {
            switch (field)
            {
            case FloatField::TimeScale:
                return color(0.30f, 0.68f, 0.86f, 1.0f);
            case FloatField::OrbitYaw:
                return color(0.70f, 0.54f, 0.86f, 1.0f);
            }
            return ToolTheme::accent;
        }

        ParticleEffectEmitter* selectedEmitter()
        {
            if (!hasAsset || currentAsset.emitters.empty())
                return nullptr;
            selectedEmitterIndex = std::min(selectedEmitterIndex, currentAsset.emitters.size() - 1);
            return &currentAsset.emitters[selectedEmitterIndex];
        }

        const ParticleEffectEmitter* selectedEmitter() const
        {
            if (!hasAsset || currentAsset.emitters.empty())
                return nullptr;
            return &currentAsset.emitters[std::min(selectedEmitterIndex, currentAsset.emitters.size() - 1)];
        }

        ParticleModuleInstance* selectedModule()
        {
            ParticleEffectEmitter* emitter = selectedEmitter();
            if (emitter == nullptr || emitter->modules.empty())
                return nullptr;
            clampModuleSelection(*emitter);
            return &emitter->modules[selectedModuleIndex];
        }

        const ParticleModuleInstance* selectedModule() const
        {
            const ParticleEffectEmitter* emitter = selectedEmitter();
            if (emitter == nullptr || emitter->modules.empty())
                return nullptr;
            return &emitter->modules[std::min(selectedModuleIndex, emitter->modules.size() - 1)];
        }

        void syncTextDraftsForDisplay()
        {
            if (!hasAsset)
            {
                effectNameDraft.clear();
                emitterNameDraft.clear();
                return;
            }

            if (activeTextField != ActiveTextField::EffectName)
                effectNameDraft = currentAsset.metadata.name;
            if (activeTextField != ActiveTextField::EmitterName)
                syncEmitterNameDraft();
        }

        void syncEmitterNameDraft()
        {
            const ParticleEffectEmitter* selected = selectedEmitter();
            emitterNameDraft                      = selected == nullptr ? std::string{} : selected->name;
        }

        void clearParameterSelectionState()
        {
            selectedRichParameterId.clear();
            selectedRichKeyIndex = 0;
            selectedRichField    = 0;
            selectedRangeParameterId.clear();
            selectedRangeMax = false;
        }

        static std::string textFieldDisplay(const std::string& text, bool focused)
        {
            std::string display = compact(text, focused ? 23 : 24);
            if (focused)
                display += "|";
            return display;
        }

        size_t emitterRowOffset() const
        {
            if (!hasAsset || currentAsset.emitters.size() <= EmitterRowCount || selectedEmitterIndex < EmitterRowCount)
                return 0;
            return std::min(selectedEmitterIndex - EmitterRowCount + 1, currentAsset.emitters.size() - EmitterRowCount);
        }

        void markDirty(EngineToolStateComponent& state, const std::string& status)
        {
            dirty        = true;
            state.status = status;
        }

        std::string uniqueEmitterId(const std::string& base) const
        {
            std::string stem = base.empty() ? "emitter" : base;
            for (char& c : stem)
            {
                if (c == ' ')
                    c = '_';
            }

            for (uint32_t i = 1; i < 1000u; ++i)
            {
                const std::string candidate = stem + "_" + std::to_string(i);
                if (!assetHasEmitter(candidate))
                    return candidate;
            }
            return stem + "_new";
        }

        std::string generatedSiblingPath(const std::string& suffix) const
        {
            std::filesystem::path       source = currentPath.empty() ? std::filesystem::path("particle_effect.json")
                                                                     : std::filesystem::path(currentPath);
            const std::filesystem::path parent = source.parent_path();
            const std::string stem      = source.stem().string().empty() ? "particle_effect" : source.stem().string();
            const std::string extension = source.extension().string().empty() ? ".json" : source.extension().string();

            for (uint32_t i = 1; i < 1000u; ++i)
            {
                const std::string           index     = i == 1u ? "" : std::to_string(i);
                const std::filesystem::path candidate = parent / (stem + suffix + index + extension);
                if (candidate.string() != currentPath && !std::filesystem::exists(candidate))
                    return candidate.string();
            }
            return (parent / (stem + suffix + "_new" + extension)).string();
        }

        static std::string fileName(const std::string& path)
        {
            const std::string name = std::filesystem::path(path).filename().string();
            return compact(name.empty() ? path : name, 24);
        }

        static std::string compact(const std::string& text, size_t limit)
        {
            if (text.size() <= limit)
                return text;
            if (limit <= 3)
                return text.substr(0, limit);
            return text.substr(0, limit - 3) + "...";
        }

        static const std::vector<glm::vec4>& backgroundPresets()
        {
            static const std::vector<glm::vec4> presets = {
                {0.02f, 0.025f, 0.030f, 1.0f},
                {0.08f, 0.09f, 0.10f, 1.0f},
                {0.18f, 0.17f, 0.14f, 1.0f},
                {0.48f, 0.54f, 0.60f, 1.0f},
            };
            return presets;
        }

        static uint32_t closestBackgroundPreset(const glm::vec4& value)
        {
            const std::vector<glm::vec4>& presets      = backgroundPresets();
            size_t                        best         = 0;
            float                         bestDistance = std::numeric_limits<float>::max();
            for (size_t i = 0; i < presets.size(); ++i)
            {
                const glm::vec4 delta    = presets[i] - value;
                const float     distance = delta.r * delta.r + delta.g * delta.g + delta.b * delta.b;
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    best         = i;
                }
            }
            return static_cast<uint32_t>(best);
        }
    };
} // namespace gts::tools
