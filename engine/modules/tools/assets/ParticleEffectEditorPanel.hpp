#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
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
            root = createContainerRelative(ctx.ui, parent, {0.0f, 0.0f, 1.0f, 1.0f});
            header = createTextRelative(ctx.ui,
                                        root,
                                        {0.0f, 0.000f, 1.0f, 0.026f},
                                        font,
                                        "PARTICLE EFFECTS",
                                        ToolTheme::text,
                                        ToolTheme::headerTextScale);
            summary = createTextRelative(ctx.ui,
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

            emitterHeader = createTextRelative(ctx.ui,
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
                                 {EmitterAction::ToggleEnabled, "ON"}});

            previewSwatch =
                createRectRelative(ctx.ui, root, {0.0f, 0.522f, 1.0f, 0.056f}, ToolTheme::paneSurface, false);
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
                                 0.589f,
                                 {{PlaybackAction::PlayPause, "PAUSE"},
                                  {PlaybackAction::Restart, "RESET"},
                                  {PlaybackAction::Background, "BG"},
                                  {PlaybackAction::CameraReset, "CAM"}});

            addSlider(ctx.ui, font, 0.637f, FloatField::TimeScale, "TIME", 0.0f, 2.0f);
            addSlider(ctx.ui, font, 0.668f, FloatField::OrbitYaw, "ORBIT", -180.0f, 180.0f);

            inspectorHeader = createTextRelative(ctx.ui,
                                                 root,
                                                 {0.0f, 0.704f, 1.0f, 0.024f},
                                                 font,
                                                 "INSPECTOR",
                                                 ToolTheme::text,
                                                 ToolTheme::headerTextScale);
            float y = 0.731f;
            addSlider(ctx.ui, font, y, FloatField::EmissionRate, "RATE", 0.0f, 180.0f);
            y += 0.030f;
            addSlider(ctx.ui, font, y, FloatField::MaxParticles, "MAX", 8.0f, 512.0f, true);
            y += 0.030f;
            addSlider(ctx.ui, font, y, FloatField::Intensity, "INTENSITY", 0.0f, 2.5f);
            y += 0.030f;
            addSlider(ctx.ui, font, y, FloatField::LifetimeMin, "LIFE MIN", 0.05f, 6.0f);
            y += 0.030f;
            addSlider(ctx.ui, font, y, FloatField::LifetimeMax, "LIFE MAX", 0.05f, 8.0f);
            y += 0.030f;
            addSlider(ctx.ui, font, y, FloatField::SizeStart, "SIZE IN", 0.02f, 2.0f);
            y += 0.030f;
            addSlider(ctx.ui, font, y, FloatField::SizeEnd, "SIZE OUT", 0.02f, 2.0f);
            y += 0.030f;
            addSlider(ctx.ui, font, y, FloatField::AlphaPeak, "ALPHA", 0.0f, 1.0f);

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
                applyEffectButtons(ctx.world, state, interaction);
                applyEmitterRows(ctx.world, state, interaction);
                applyEmitterButtons(ctx.world, state, interaction);
                applyPlaybackButtons(ctx.world, state, interaction);
                applySliders(ctx.ui, ctx.world, state, interaction);
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
            playbackButtons.clear();
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
            ToggleEnabled
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
            OrbitYaw,
            EmissionRate,
            MaxParticles,
            Intensity,
            LifetimeMin,
            LifetimeMax,
            SizeStart,
            SizeEnd,
            AlphaPeak
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

        struct PlaybackButtonBinding
        {
            PlaybackAction action = PlaybackAction::PlayPause;
            ToolButton     button;
        };

        struct SliderBinding
        {
            FloatField field = FloatField::EmissionRate;
            ToolSlider slider;
        };

        static constexpr size_t EffectRowCount = 4;
        static constexpr size_t EmitterRowCount = 3;

        UiHandle root = UI_INVALID_HANDLE;
        UiHandle header = UI_INVALID_HANDLE;
        UiHandle summary = UI_INVALID_HANDLE;
        UiHandle emitterHeader = UI_INVALID_HANDLE;
        UiHandle previewSwatch = UI_INVALID_HANDLE;
        UiHandle previewText = UI_INVALID_HANDLE;
        UiHandle inspectorHeader = UI_INVALID_HANDLE;
        UiHandle footer = UI_INVALID_HANDLE;

        ToolTextField effectNameField;
        ToolTextField emitterNameField;

        std::vector<ToolButton>             effectRows;
        std::vector<ToolButton>             emitterRows;
        std::vector<EffectButtonBinding>    effectButtons;
        std::vector<EmitterButtonBinding>   emitterButtons;
        std::vector<PlaybackButtonBinding>  playbackButtons;
        std::vector<SliderBinding>          sliders;

        ParticleEffectAsset currentAsset;
        std::string         currentPath;
        bool                hasAsset = false;
        bool                dirty = false;
        size_t              effectBrowserOffset = 0;
        size_t              selectedEmitterIndex = 0;
        bool                playbackPaused = false;
        float               playbackTimeScale = 1.0f;
        uint32_t            backgroundPreset = 0;
        ActiveTextField     activeTextField = ActiveTextField::None;
        std::string         effectNameDraft;
        std::string         emitterNameDraft;

        void addEffectButtonRow(UiSystem&                                                    ui,
                                BitmapFont*                                                  font,
                                float                                                        y,
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

        void addEmitterButtonRow(UiSystem&                                                     ui,
                                 BitmapFont*                                                   font,
                                 float                                                         y,
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

        void addPlaybackButtonRow(UiSystem&                                                      ui,
                                  BitmapFont*                                                    font,
                                  float                                                          y,
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
                const ParticleEffectRegistryComponent& registry =
                    world.getSingleton<ParticleEffectRegistryComponent>();
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
            std::filesystem::recursive_directory_iterator it(
                rootPath, std::filesystem::directory_options::skip_permission_denied, ec);
            while (!ec && it != end)
            {
                const std::filesystem::directory_entry& entry = *it;
                if (entry.is_regular_file(ec) && entry.path().extension() == ".json")
                    paths.push_back(entry.path().generic_string());
                it.increment(ec);
            }
        }

        void applyEffectBrowser(ECSWorld&                  world,
                                EngineToolStateComponent&  state,
                                const std::vector<std::string>& paths,
                                const UiInteractionResult& interaction)
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

        void applyTextFieldFocus(ECSWorld&                 world,
                                 EngineToolStateComponent& state,
                                 const UiInteractionResult& interaction)
        {
            if (wasClicked(interaction, effectNameField.rect))
            {
                if (!hasAsset)
                {
                    state.status = "NO EFFECT";
                    return;
                }

                activeTextField = ActiveTextField::EffectName;
                effectNameDraft = currentAsset.metadata.name;
                state.status = "EDIT EFFECT NAME";
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
            if (has(trigger->modifiers, ModifierFlags::Ctrl) ||
                has(trigger->modifiers, ModifierFlags::Alt) ||
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
                effectNameDraft = std::filesystem::path(currentPath).stem().string();
                currentAsset.metadata.name = effectNameDraft;
                dirty = true;
            }
            else if (activeTextField == ActiveTextField::EmitterName && emitterNameDraft.empty())
            {
                emitterNameDraft = "Emitter " + std::to_string(selectedEmitterIndex + 1);
                if (ParticleEffectEmitter* selected = selectedEmitter())
                {
                    selected->name = emitterNameDraft;
                    dirty = true;
                }
            }

            activeTextField = ActiveTextField::None;
            state.status = dirty ? "EDIT APPLIED" : "EDIT DONE";
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
                const int offset = static_cast<int>(key) - static_cast<int>(GtsKey::A);
                const char base = shift ? 'A' : 'a';
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

            currentAsset = std::move(loaded);
            currentPath = path;
            hasAsset = true;
            dirty = false;
            selectedEmitterIndex = 0;
            playbackPaused = false;
            playbackTimeScale = 1.0f;
            backgroundPreset = closestBackgroundPreset(currentAsset.preview.backgroundColor);
            activeTextField = ActiveTextField::None;
            effectNameDraft = currentAsset.metadata.name;
            syncEmitterNameDraft();
            state.status = "OPENED " + fileName(path);
            return true;
        }

        void applyEffectButtons(ECSWorld& world, EngineToolStateComponent& state, const UiInteractionResult& interaction)
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
            currentPath = path;
            dirty = false;
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
            const std::string path = generatedSiblingPath(suffix);
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
            asset.schemaVersion = CurrentParticleEffectSchemaVersion;
            if (asset.metadata.name.empty())
                asset.metadata.name = std::filesystem::path(currentPath).stem().string();

            for (ParticleEffectEmitter& emitter : asset.emitters)
            {
                emitter.descriptor.effectPath.clear();
                emitter.descriptor.effectEmitterId.clear();
                emitter.descriptor.schemaVersion = CurrentParticleEmitterSchemaVersion;
            }
            return asset;
        }

        void applyEmitterRows(ECSWorld&                 world,
                              EngineToolStateComponent& state,
                              const UiInteractionResult& interaction)
        {
            if (!hasAsset)
                return;

            const size_t rowOffset = emitterRowOffset();
            for (size_t i = 0; i < emitterRows.size() && rowOffset + i < currentAsset.emitters.size(); ++i)
            {
                if (wasClicked(interaction, emitterRows[i].rect))
                {
                    selectedEmitterIndex = rowOffset + i;
                    syncEmitterNameDraft();
                    applySelectedEmitterToLive(world, false);
                    state.status = "SELECTED " + currentAsset.emitters[selectedEmitterIndex].name;
                    return;
                }
            }
        }

        void applyEmitterButtons(ECSWorld&                 world,
                                 EngineToolStateComponent& state,
                                 const UiInteractionResult& interaction)
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
                }
                return;
            }
        }

        void addEmitter(EngineToolStateComponent& state)
        {
            ParticleEffectEmitter emitter;
            emitter.stableId = uniqueEmitterId("emitter");
            emitter.name = "Emitter " + std::to_string(currentAsset.emitters.size() + 1);
            emitter.descriptor.effectEmitterId.clear();
            emitter.descriptor.effectPath.clear();
            currentAsset.emitters.push_back(std::move(emitter));
            selectedEmitterIndex = currentAsset.emitters.size() - 1;
            syncEmitterNameDraft();
            markDirty(state, "EMITTER ADDED");
        }

        void deleteEmitter(EngineToolStateComponent& state)
        {
            if (currentAsset.emitters.size() <= 1)
            {
                state.status = "KEEP ONE EMITTER";
                return;
            }
            selectedEmitterIndex = std::min(selectedEmitterIndex, currentAsset.emitters.size() - 1);
            currentAsset.emitters.erase(currentAsset.emitters.begin() +
                                        static_cast<std::ptrdiff_t>(selectedEmitterIndex));
            if (selectedEmitterIndex >= currentAsset.emitters.size())
                selectedEmitterIndex = currentAsset.emitters.size() - 1;
            syncEmitterNameDraft();
            markDirty(state, "EMITTER DELETED");
        }

        void duplicateEmitter(EngineToolStateComponent& state)
        {
            ParticleEffectEmitter* selected = selectedEmitter();
            if (selected == nullptr)
            {
                state.status = "NO EMITTER";
                return;
            }

            ParticleEffectEmitter copy = *selected;
            copy.stableId = uniqueEmitterId(selected->stableId.empty() ? "emitter" : selected->stableId);
            copy.name = selected->name + " Copy";
            currentAsset.emitters.insert(currentAsset.emitters.begin() +
                                             static_cast<std::ptrdiff_t>(selectedEmitterIndex + 1),
                                         std::move(copy));
            selectedEmitterIndex += 1;
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

            syncEmitterNameDraft();
            activeTextField = ActiveTextField::EmitterName;
            state.status = "EDIT EMITTER NAME";
        }

        void moveEmitter(EngineToolStateComponent& state, int delta)
        {
            if (currentAsset.emitters.empty())
                return;

            const int current = static_cast<int>(selectedEmitterIndex);
            const int next = std::clamp(current + delta, 0, static_cast<int>(currentAsset.emitters.size()) - 1);
            if (next == current)
            {
                state.status = delta < 0 ? "ALREADY FIRST" : "ALREADY LAST";
                return;
            }

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

            selected->descriptor.enabled = !selected->descriptor.enabled;
            markDirty(state, selected->descriptor.enabled ? "EMITTER ENABLED" : "EMITTER DISABLED");
        }

        void applyPlaybackButtons(ECSWorld&                 world,
                                  EngineToolStateComponent& state,
                                  const UiInteractionResult& interaction)
        {
            for (const PlaybackButtonBinding& binding : playbackButtons)
            {
                if (!wasClicked(interaction, binding.button.rect))
                    continue;

                switch (binding.action)
                {
                case PlaybackAction::PlayPause:
                    playbackPaused = !playbackPaused;
                    state.status = playbackPaused ? "PREVIEW PAUSED" : "PREVIEW PLAYING";
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

        void applySliders(UiSystem&                 ui,
                          ECSWorld&                 world,
                          EngineToolStateComponent& state,
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
                    setPreviewOrbitYawDegrees(value);
                    markDirty(state, "PREVIEW CAMERA");
                    return;
                }

                ParticleEffectEmitter* selected = selectedEmitter();
                if (selected == nullptr)
                    return;

                writeField(selected->descriptor, binding.field, value);
                markDirty(state, "EMITTER UPDATED");
                applySelectedEmitterToLive(world, false);
                return;
            }
        }

        void cycleBackground(EngineToolStateComponent& state)
        {
            backgroundPreset = (backgroundPreset + 1u) % static_cast<uint32_t>(backgroundPresets().size());
            currentAsset.preview.backgroundColor = backgroundPresets()[backgroundPreset];
            markDirty(state, "BACKGROUND UPDATED");
        }

        void resetPreviewCamera(EngineToolStateComponent& state)
        {
            currentAsset.preview.cameraPosition = {0.0f, 1.0f, 4.0f};
            currentAsset.preview.cameraTarget = {0.0f, 0.6f, 0.0f};
            currentAsset.preview.orbitDistance = 4.0f;
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

                    runtime.playbackPaused = playbackPaused;
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

                    runtime.playbackPaused = false;
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

                    const std::string effectPath = emitter.effectPath;
                    const uint32_t randomSeed = emitter.randomSeed;
                    const bool reloadFromEffect = emitter.reloadFromEffect;
                    emitter = selected->descriptor;
                    emitter.effectPath = effectPath;
                    emitter.effectEmitterId = selected->stableId;
                    emitter.randomSeed = randomSeed;
                    emitter.reloadFromEffect = reloadFromEffect;

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
            runtime.emitterAge = 0.0f;
            runtime.burstRepeatCounts.clear();
            runtime.textureID = 0;
            runtime.meshID = 0;
            runtime.boundTexturePath.clear();
            runtime.boundMeshPath.clear();
        }

        void updateDisplay(EngineToolContext&                ctx,
                           EngineToolStateComponent&         state,
                           const std::vector<std::string>& effectPaths)
        {
            setText(ctx.ui, summary, effectSummary());
            setRectColor(ctx.ui, previewSwatch, previewColor());
            setText(ctx.ui, previewText, previewSummary(ctx.world));
            setText(ctx.ui, footer, state.status);
            syncTextDraftsForDisplay();
            updateTextField(ctx.ui,
                            effectNameField,
                            textFieldDisplay(hasAsset ? effectNameDraft : "--",
                                             activeTextField == ActiveTextField::EffectName),
                            activeTextField == ActiveTextField::EffectName);
            updateTextField(ctx.ui,
                            emitterNameField,
                            textFieldDisplay(selectedEmitter() == nullptr ? "--" : emitterNameDraft,
                                             activeTextField == ActiveTextField::EmitterName),
                            activeTextField == ActiveTextField::EmitterName);

            for (size_t i = 0; i < effectRows.size(); ++i)
            {
                const size_t effectIndex = effectBrowserOffset + i;
                const bool hasRow = effectIndex < effectPaths.size();
                const std::string label = hasRow ? fileName(effectPaths[effectIndex]) : "EMPTY";
                updateButton(ctx.ui, effectRows[i], label);
                if (hasRow && effectPaths[effectIndex] == currentPath)
                    setRectColor(ctx.ui, effectRows[i].rect, ToolTheme::buttonActive);
            }

            const size_t rowOffset = emitterRowOffset();
            for (size_t i = 0; i < emitterRows.size(); ++i)
            {
                const size_t assetIndex = rowOffset + i;
                const bool hasRow = hasAsset && assetIndex < currentAsset.emitters.size();
                const std::string label = hasRow ? emitterRowLabel(currentAsset.emitters[assetIndex]) : "EMPTY";
                updateButton(ctx.ui, emitterRows[i], label);
                if (hasRow && assetIndex == selectedEmitterIndex)
                    setRectColor(ctx.ui, emitterRows[i].rect, ToolTheme::buttonActive);
            }

            for (const EffectButtonBinding& binding : effectButtons)
                updateButton(ctx.ui, binding.button, effectButtonLabel(binding.action));
            for (const EmitterButtonBinding& binding : emitterButtons)
                updateButton(ctx.ui, binding.button, emitterButtonLabel(binding.action));
            for (const PlaybackButtonBinding& binding : playbackButtons)
                updateButton(ctx.ui, binding.button, playbackButtonLabel(binding.action));

            for (const SliderBinding& binding : sliders)
                updateSlider(ctx.ui, binding.slider, readSliderValue(binding.field), sliderColor(binding.field));
        }

        std::string effectSummary() const
        {
            if (!hasAsset)
                return "OPEN AN EFFECT FROM SCENE";

            std::ostringstream out;
            const std::string name =
                currentAsset.metadata.name.empty() ? fileName(currentPath) : currentAsset.metadata.name;
            out << name << (dirty ? " *" : "") << "  " << currentAsset.emitters.size() << " EMIT";
            return out.str();
        }

        std::string previewSummary(ECSWorld& world) const
        {
            if (!hasAsset)
                return "NO LIVE PREVIEW";

            size_t liveEmitters = 0;
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

        std::string emitterRowLabel(const ParticleEffectEmitter& emitter) const
        {
            std::string label = emitter.name.empty() ? emitter.stableId : emitter.name;
            if (!emitter.descriptor.enabled)
                label = "OFF " + label;
            return compact(label, 24);
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
                return "COPY";
            case EmitterAction::Rename:
                return "NAME";
            case EmitterAction::MoveUp:
                return "UP";
            case EmitterAction::MoveDown:
                return "DOWN";
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
            const ParticleEffectEmitter* selected = selectedEmitter();
            switch (field)
            {
            case FloatField::TimeScale:
                return playbackTimeScale;
            case FloatField::OrbitYaw:
                return previewOrbitYawDegrees();
            case FloatField::EmissionRate:
                return selected == nullptr ? 0.0f : selected->descriptor.emissionRate;
            case FloatField::MaxParticles:
                return selected == nullptr ? 0.0f : static_cast<float>(selected->descriptor.maxParticles);
            case FloatField::Intensity:
                return selected == nullptr ? 0.0f : selected->descriptor.intensity;
            case FloatField::LifetimeMin:
                return selected == nullptr ? 0.0f : selected->descriptor.lifetimeMin;
            case FloatField::LifetimeMax:
                return selected == nullptr ? 0.0f : selected->descriptor.lifetimeMax;
            case FloatField::SizeStart:
                return selected == nullptr || selected->descriptor.sizeOverLifetime.empty()
                           ? 0.0f
                           : selected->descriptor.sizeOverLifetime.front().value;
            case FloatField::SizeEnd:
                return selected == nullptr || selected->descriptor.sizeOverLifetime.empty()
                           ? 0.0f
                           : selected->descriptor.sizeOverLifetime.back().value;
            case FloatField::AlphaPeak:
                return selected == nullptr ? 0.0f : alphaPeak(selected->descriptor);
            }
            return 0.0f;
        }

        static void writeField(ParticleEmitterComponent& emitter, FloatField field, float value)
        {
            switch (field)
            {
            case FloatField::EmissionRate:
                emitter.emissionRate = std::max(0.0f, value);
                break;
            case FloatField::MaxParticles:
                emitter.maxParticles = static_cast<uint32_t>(std::max(1.0f, value));
                break;
            case FloatField::Intensity:
                emitter.intensity = std::max(0.0f, value);
                break;
            case FloatField::LifetimeMin:
                emitter.lifetimeMin = std::clamp(value, 0.01f, std::max(0.01f, emitter.lifetimeMax));
                break;
            case FloatField::LifetimeMax:
                emitter.lifetimeMax = std::max(emitter.lifetimeMin, value);
                break;
            case FloatField::SizeStart:
                ensureSizeCurve(emitter);
                emitter.sizeOverLifetime.front().value = std::max(0.001f, value);
                break;
            case FloatField::SizeEnd:
                ensureSizeCurve(emitter);
                emitter.sizeOverLifetime.back().value = std::max(0.001f, value);
                break;
            case FloatField::AlphaPeak:
                ensureAlphaCurve(emitter);
                for (ParticleFloatKey& key : emitter.alphaOverLifetime)
                {
                    if (key.t > 0.0f && key.t < 1.0f)
                        key.value = std::clamp(value, 0.0f, 1.0f);
                }
                break;
            case FloatField::TimeScale:
            case FloatField::OrbitYaw:
                break;
            }
        }

        static void ensureSizeCurve(ParticleEmitterComponent& emitter)
        {
            if (emitter.sizeOverLifetime.empty())
            {
                emitter.sizeOverLifetime.push_back({0.0f, 0.4f});
                emitter.sizeOverLifetime.push_back({1.0f, 0.8f});
                return;
            }
            if (emitter.sizeOverLifetime.size() == 1)
                emitter.sizeOverLifetime.push_back({1.0f, emitter.sizeOverLifetime.front().value});
        }

        static void ensureAlphaCurve(ParticleEmitterComponent& emitter)
        {
            if (emitter.alphaOverLifetime.empty())
                emitter.alphaOverLifetime = {{0.0f, 0.0f}, {0.2f, 1.0f}, {1.0f, 0.0f}};
        }

        static float alphaPeak(const ParticleEmitterComponent& emitter)
        {
            float peak = 0.0f;
            for (const ParticleFloatKey& key : emitter.alphaOverLifetime)
                peak = std::max(peak, key.value);
            return peak;
        }

        float previewOrbitYawDegrees() const
        {
            const glm::vec3 offset = currentAsset.preview.cameraPosition - currentAsset.preview.cameraTarget;
            return glm::degrees(std::atan2(offset.x, offset.z));
        }

        void setPreviewOrbitYawDegrees(float degrees)
        {
            const float yawRadians = glm::radians(degrees);
            const glm::vec3 offset = currentAsset.preview.cameraPosition - currentAsset.preview.cameraTarget;
            const float planarDistance = std::sqrt(offset.x * offset.x + offset.z * offset.z);
            const float radius = std::max(0.1f, planarDistance > 0.001f ? planarDistance
                                                                         : currentAsset.preview.orbitDistance);
            currentAsset.preview.orbitDistance = radius;
            currentAsset.preview.cameraPosition = currentAsset.preview.cameraTarget +
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
            case FloatField::AlphaPeak:
                return color(0.72f, 0.76f, 0.82f, 1.0f);
            default:
                return ToolTheme::accent;
            }
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
            emitterNameDraft = selected == nullptr ? std::string{} : selected->name;
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
            return std::min(selectedEmitterIndex - EmitterRowCount + 1,
                            currentAsset.emitters.size() - EmitterRowCount);
        }

        void markDirty(EngineToolStateComponent& state, const std::string& status)
        {
            dirty = true;
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
            std::filesystem::path source = currentPath.empty() ? std::filesystem::path("particle_effect.json")
                                                               : std::filesystem::path(currentPath);
            const std::filesystem::path parent = source.parent_path();
            const std::string stem = source.stem().string().empty() ? "particle_effect" : source.stem().string();
            const std::string extension = source.extension().string().empty() ? ".json" : source.extension().string();

            for (uint32_t i = 1; i < 1000u; ++i)
            {
                const std::string index = i == 1u ? "" : std::to_string(i);
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
            const std::vector<glm::vec4>& presets = backgroundPresets();
            size_t best = 0;
            float  bestDistance = std::numeric_limits<float>::max();
            for (size_t i = 0; i < presets.size(); ++i)
            {
                const glm::vec4 delta = presets[i] - value;
                const float distance = delta.r * delta.r + delta.g * delta.g + delta.b * delta.b;
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    best = i;
                }
            }
            return static_cast<uint32_t>(best);
        }
    };
} // namespace gts::tools
