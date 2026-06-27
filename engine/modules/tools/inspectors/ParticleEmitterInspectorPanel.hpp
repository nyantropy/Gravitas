#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "EngineToolPanel.h"
#include "EngineToolSelectionHelpers.h"
#include "ParticleEffectAssetIO.h"
#include "ParticleEmitterComponent.h"
#include "ParticleEmitterRuntimeComponent.h"
#include "ToolTheme.h"
#include "ToolWidgets.h"

namespace gts::tools
{
    class ParticleEmitterInspectorPanel : public EngineToolPanel
    {
        public:
        std::string_view id() const override
        {
            return "particles";
        }
        std::string_view title() const override
        {
            return "Particles";
        }

        void build(EngineToolContext& ctx, UiHandle parent, BitmapFont* font) override
        {
            root    = createContainerRelative(ctx.ui, parent, {0.0f, 0.0f, 1.0f, 1.0f});
            header  = createTextRelative(ctx.ui,
                                         root,
                                         {0.0f, 0.000f, 1.0f, 0.036f},
                                         font,
                                         "PARTICLE EMITTER",
                                         ToolTheme::text,
                                         ToolTheme::headerTextScale);
            summary = createTextRelative(ctx.ui,
                                         root,
                                         {0.0f, 0.042f, 1.0f, 0.036f},
                                         font,
                                         "NO EMITTER",
                                         ToolTheme::mutedText,
                                         ToolTheme::smallTextScale);

            addButtonRow(ctx.ui,
                         font,
                         0.088f,
                         {{ButtonAction::PreviousEmitter, "PREV"},
                          {ButtonAction::NextEmitter, "NEXT"},
                          {ButtonAction::Reload, "RELOAD"},
                          {ButtonAction::Save, "SAVE"},
                          {ButtonAction::Restart, "RESET"}});
            addButtonRow(ctx.ui,
                         font,
                         0.140f,
                         {{ButtonAction::ToggleEnabled, "ON"},
                          {ButtonAction::ToggleLocalSpace, "LOCAL"},
                          {ButtonAction::CycleShape, "SHAPE"},
                          {ButtonAction::CycleBlend, "BLEND"}});

            float y = 0.202f;
            addSlider(ctx.ui, font, y, FloatField::EmissionRate, "RATE", 0.0f, 180.0f);
            y += 0.035f;
            addSlider(ctx.ui, font, y, FloatField::MaxParticles, "MAX", 8.0f, 512.0f, true);
            y += 0.035f;
            addSlider(ctx.ui, font, y, FloatField::Intensity, "INTENSITY", 0.0f, 2.5f);
            y += 0.035f;
            addSlider(ctx.ui, font, y, FloatField::LifetimeMin, "LIFE MIN", 0.05f, 6.0f);
            y += 0.035f;
            addSlider(ctx.ui, font, y, FloatField::LifetimeMax, "LIFE MAX", 0.05f, 8.0f);
            y += 0.035f;
            addSlider(ctx.ui, font, y, FloatField::VelocitySpread, "SPREAD", 0.0f, 2.0f);
            y += 0.035f;
            addSlider(ctx.ui, font, y, FloatField::Drag, "DRAG", 0.0f, 3.0f);
            y += 0.035f;
            addSlider(ctx.ui, font, y, FloatField::Softness, "SOFT", 0.0f, 240.0f);
            y += 0.035f;
            addSlider(ctx.ui, font, y, FloatField::Vortex, "VORTEX", -2.0f, 2.0f);
            y += 0.035f;
            addSlider(ctx.ui, font, y, FloatField::NoiseStrength, "NOISE", 0.0f, 1.0f);
            y += 0.045f;

            addSlider(ctx.ui, font, y, FloatField::TintR, "TINT R", 0.0f, 1.0f);
            y += 0.034f;
            addSlider(ctx.ui, font, y, FloatField::TintG, "TINT G", 0.0f, 1.0f);
            y += 0.034f;
            addSlider(ctx.ui, font, y, FloatField::TintB, "TINT B", 0.0f, 1.0f);
            y += 0.034f;
            addSlider(ctx.ui, font, y, FloatField::TintA, "TINT A", 0.0f, 1.0f);
            y += 0.046f;

            addSlider(ctx.ui, font, y, FloatField::SizeStart, "SIZE IN", 0.02f, 2.0f);
            y += 0.034f;
            addSlider(ctx.ui, font, y, FloatField::SizeEnd, "SIZE OUT", 0.02f, 2.0f);
            y += 0.034f;
            addSlider(ctx.ui, font, y, FloatField::AlphaPeak, "ALPHA PEAK", 0.0f, 1.0f);

            footer = createTextRelative(ctx.ui,
                                        root,
                                        {0.0f, 0.940f, 1.0f, 0.040f},
                                        font,
                                        "F6 TO HIDE",
                                        ToolTheme::statusText,
                                        ToolTheme::smallTextScale);
        }

        void
        update(EngineToolContext& ctx, EngineToolStateComponent& state, const UiInteractionResult& interaction) override
        {
            std::vector<Entity> emitters = collectEmitters(ctx.world);
            resolveSelection(state, emitters);
            ParticleEmitterComponent* emitter = selectedEmitter(ctx.world, state);

            if (emitter != nullptr)
            {
                applyButtons(ctx.world, state, *emitter, emitters, interaction);
                emitter = selectedEmitter(ctx.world, state);
                if (emitter != nullptr)
                    applySliders(ctx.ui, *emitter, interaction);
            }

            updateDisplay(ctx, state, emitters, emitter);
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
            sliders.clear();
            buttons.clear();
        }

        private:
        enum class FloatField
        {
            EmissionRate,
            MaxParticles,
            Intensity,
            LifetimeMin,
            LifetimeMax,
            VelocitySpread,
            Drag,
            Softness,
            Vortex,
            NoiseStrength,
            TintR,
            TintG,
            TintB,
            TintA,
            SizeStart,
            SizeEnd,
            AlphaPeak
        };

        enum class ButtonAction
        {
            PreviousEmitter,
            NextEmitter,
            ToggleEnabled,
            ToggleLocalSpace,
            CycleShape,
            CycleBlend,
            Reload,
            Save,
            Restart
        };

        struct SliderBinding
        {
            FloatField field = FloatField::EmissionRate;
            ToolSlider slider;
        };

        struct ButtonBinding
        {
            ButtonAction action = ButtonAction::Restart;
            ToolButton   button;
        };

        UiHandle                   root    = UI_INVALID_HANDLE;
        UiHandle                   header  = UI_INVALID_HANDLE;
        UiHandle                   summary = UI_INVALID_HANDLE;
        UiHandle                   footer  = UI_INVALID_HANDLE;
        std::vector<SliderBinding> sliders;
        std::vector<ButtonBinding> buttons;

        void addButtonRow(UiSystem&                                                ui,
                          BitmapFont*                                              font,
                          float                                                    y,
                          const std::vector<std::pair<ButtonAction, std::string>>& rowButtons)
        {
            const float gap = 0.015f;
            const float width =
                (1.0f - gap * static_cast<float>(rowButtons.size() - 1)) / static_cast<float>(rowButtons.size());

            for (size_t i = 0; i < rowButtons.size(); ++i)
            {
                const float x = static_cast<float>(i) * (width + gap);
                buttons.push_back(
                    {rowButtons[i].first,
                     createButtonRelative(
                         ui, root, {x, y, width, 0.042f}, font, rowButtons[i].second, ToolTheme::buttonTextScale)});
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

        static std::vector<Entity> collectEmitters(ECSWorld& world)
        {
            std::vector<Entity> emitters;
            world.forEach<ParticleEmitterComponent>(
                [&](Entity entity, ParticleEmitterComponent&)
                {
                    emitters.push_back(entity);
                });
            std::sort(emitters.begin(),
                      emitters.end(),
                      [](Entity lhs, Entity rhs)
                      {
                          return lhs.id < rhs.id;
                      });
            return emitters;
        }

        static void resolveSelection(EngineToolStateComponent& state, const std::vector<Entity>& emitters)
        {
            if (emitters.empty())
            {
                if (!isValidToolEntity(state.selectedEntity))
                    return;
                return;
            }

            const auto it = std::find(emitters.begin(), emitters.end(), state.selectedEntity);
            if (it == emitters.end())
            {
                state.selectedEntity            = emitters.front();
                state.selectionSource           = EngineToolSelectionSource::Inspector;
                state.selectionChangedThisFrame = true;
            }
        }

        static ParticleEmitterComponent* selectedEmitter(ECSWorld& world, const EngineToolStateComponent& state)
        {
            if (!isValidToolEntity(state.selectedEntity) ||
                !world.hasComponent<ParticleEmitterComponent>(state.selectedEntity))
            {
                return nullptr;
            }
            return &world.getComponent<ParticleEmitterComponent>(state.selectedEntity);
        }

        void applyButtons(ECSWorld&                  world,
                          EngineToolStateComponent&  state,
                          ParticleEmitterComponent&  emitter,
                          const std::vector<Entity>& emitters,
                          const UiInteractionResult& interaction)
        {
            for (const ButtonBinding& binding : buttons)
            {
                if (!wasClicked(interaction, binding.button.rect))
                    continue;

                switch (binding.action)
                {
                case ButtonAction::PreviousEmitter:
                    stepSelection(state, emitters, -1);
                    break;
                case ButtonAction::NextEmitter:
                    stepSelection(state, emitters, 1);
                    break;
                case ButtonAction::ToggleEnabled:
                    emitter.enabled = !emitter.enabled;
                    state.status    = emitter.enabled ? "EMITTER ENABLED" : "EMITTER DISABLED";
                    break;
                case ButtonAction::ToggleLocalSpace:
                    emitter.localSpace = !emitter.localSpace;
                    restartEmitter(world, state);
                    state.status = emitter.localSpace ? "LOCAL SPACE" : "WORLD SPACE";
                    break;
                case ButtonAction::CycleShape:
                    cycleShape(emitter);
                    restartEmitter(world, state);
                    state.status = "SHAPE " + shapeName(emitter.shape);
                    break;
                case ButtonAction::CycleBlend:
                    emitter.blend = emitter.blend == ParticleBlendMode::Alpha ? ParticleBlendMode::Additive
                                                                              : ParticleBlendMode::Alpha;
                    state.status  = "BLEND " + blendName(emitter.blend);
                    break;
                case ButtonAction::Reload:
                    reloadEmitter(world, state, emitter);
                    break;
                case ButtonAction::Save:
                    saveEmitter(state, emitter);
                    break;
                case ButtonAction::Restart:
                    restartEmitter(world, state);
                    state.status = "RUNTIME RESET";
                    break;
                }
                return;
            }
        }

        void applySliders(UiSystem& ui, ParticleEmitterComponent& emitter, const UiInteractionResult& interaction)
        {
            for (const SliderBinding& binding : sliders)
            {
                if (!isPressed(interaction, binding.slider.track))
                    continue;

                writeField(emitter, binding.field, valueFromSliderPointer(ui, binding.slider, interaction.pointerX));
                return;
            }
        }

        static void stepSelection(EngineToolStateComponent& state, const std::vector<Entity>& emitters, int delta)
        {
            if (emitters.empty())
                return;

            auto      it      = std::find(emitters.begin(), emitters.end(), state.selectedEntity);
            const int current = it == emitters.end() ? 0 : static_cast<int>(std::distance(emitters.begin(), it));
            const int count   = static_cast<int>(emitters.size());
            int       next    = (current + delta) % count;
            if (next < 0)
                next += count;
            state.selectedEntity            = emitters[static_cast<size_t>(next)];
            state.selectionSource           = EngineToolSelectionSource::Inspector;
            state.selectionChangedThisFrame = true;
            state.status                    = "SELECTED " + entityDisplayNameForStatus(state);
        }

        static void restartEmitter(ECSWorld& world, const EngineToolStateComponent& state)
        {
            if (!isValidToolEntity(state.selectedEntity) ||
                !world.hasComponent<ParticleEmitterRuntimeComponent>(state.selectedEntity))
            {
                return;
            }

            ParticleEmitterRuntimeComponent& runtime =
                world.getComponent<ParticleEmitterRuntimeComponent>(state.selectedEntity);
            runtime.particles.clear();
            runtime.spawnAccumulator = 0.0f;
            runtime.emitterAge       = 0.0f;
            runtime.burstRepeatCounts.clear();
        }

        static void reloadEmitter(ECSWorld& world, EngineToolStateComponent& state, ParticleEmitterComponent& emitter)
        {
            if (emitter.effectPath.empty())
            {
                state.status = "NO EFFECT PATH";
                return;
            }

            const std::string path = emitter.effectPath;
            const uint32_t    seed = emitter.randomSeed;
            if (!gts::particles::loadParticleEffect(path, emitter))
            {
                state.status = "RELOAD FAILED";
                return;
            }

            emitter.effectPath = path;
            emitter.randomSeed = seed;
            restartEmitter(world, state);
            state.status = "RELOADED";
        }

        static void saveEmitter(EngineToolStateComponent& state, const ParticleEmitterComponent& emitter)
        {
            if (emitter.effectPath.empty())
            {
                state.status = "NO EFFECT PATH";
                return;
            }

            state.status = gts::particles::saveParticleEffect(emitter.effectPath, emitter) ? "SAVED" : "SAVE FAILED";
        }

        void updateDisplay(EngineToolContext&         ctx,
                           EngineToolStateComponent&  state,
                           const std::vector<Entity>& emitters,
                           ParticleEmitterComponent*  emitter)
        {
            setText(ctx.ui, summary, emitterSummary(ctx.world, state, emitters, emitter));

            for (const ButtonBinding& binding : buttons)
                updateButton(ctx.ui, binding.button, buttonLabel(binding.action, emitter));

            if (emitter == nullptr)
            {
                for (const SliderBinding& binding : sliders)
                {
                    setText(ctx.ui, binding.slider.value, "--");
                    setRelativeRect(ctx.ui, binding.slider.fill, {0.0f, 0.0f, 0.001f, 1.0f});
                }
                setText(ctx.ui, footer, state.status);
                return;
            }

            for (const SliderBinding& binding : sliders)
            {
                updateSlider(ctx.ui, binding.slider, readField(*emitter, binding.field), sliderColor(binding.field));
            }

            std::string footerText = isValidToolEntity(state.hoveredEntity)
                                         ? "HOVER " + entityDisplayName(ctx.world, state.hoveredEntity)
                                         : state.status;
            if (isValidToolEntity(state.selectedEntity) &&
                ctx.world.hasComponent<ParticleEmitterRuntimeComponent>(state.selectedEntity))
            {
                const auto& runtime = ctx.world.getComponent<ParticleEmitterRuntimeComponent>(state.selectedEntity);
                footerText += "  LIVE " + std::to_string(runtime.particles.size()) + " P";
            }
            setText(ctx.ui, footer, footerText);
        }

        static std::string emitterSummary(ECSWorld&                       world,
                                          const EngineToolStateComponent& state,
                                          const std::vector<Entity>&      emitters,
                                          const ParticleEmitterComponent* emitter)
        {
            if (emitter == nullptr)
                return "NO PARTICLE EMITTERS";

            std::string  path  = emitter->effectPath;
            const size_t slash = path.find_last_of("/\\");
            if (slash != std::string::npos)
                path = path.substr(slash + 1);
            if (path.empty())
                path = "DESCRIPTOR ONLY";

            std::ostringstream out;
            out << entityDisplayName(world, state.selectedEntity) << " #" << state.selectedEntity.id << " / "
                << emitters.size() << "  " << path;
            return out.str();
        }

        static std::string entityDisplayNameForStatus(const EngineToolStateComponent& state)
        {
            return "ENTITY " + std::to_string(state.selectedEntity.id);
        }

        static std::string buttonLabel(ButtonAction action, const ParticleEmitterComponent* emitter)
        {
            if (emitter == nullptr)
            {
                switch (action)
                {
                case ButtonAction::PreviousEmitter:
                    return "PREV";
                case ButtonAction::NextEmitter:
                    return "NEXT";
                case ButtonAction::Reload:
                    return "RELOAD";
                case ButtonAction::Save:
                    return "SAVE";
                case ButtonAction::Restart:
                    return "RESET";
                case ButtonAction::ToggleEnabled:
                    return "ON";
                case ButtonAction::ToggleLocalSpace:
                    return "LOCAL";
                case ButtonAction::CycleShape:
                    return "SHAPE";
                case ButtonAction::CycleBlend:
                    return "BLEND";
                }
            }

            switch (action)
            {
            case ButtonAction::ToggleEnabled:
                return emitter->enabled ? "ON" : "OFF";
            case ButtonAction::ToggleLocalSpace:
                return emitter->localSpace ? "LOCAL" : "WORLD";
            case ButtonAction::CycleShape:
                return shapeName(emitter->shape);
            case ButtonAction::CycleBlend:
                return blendName(emitter->blend);
            case ButtonAction::PreviousEmitter:
                return "PREV";
            case ButtonAction::NextEmitter:
                return "NEXT";
            case ButtonAction::Reload:
                return "RELOAD";
            case ButtonAction::Save:
                return "SAVE";
            case ButtonAction::Restart:
                return "RESET";
            }
            return "";
        }

        static std::string shapeName(ParticleEmitterShape shape)
        {
            switch (shape)
            {
            case ParticleEmitterShape::Sphere:
                return "SPHERE";
            case ParticleEmitterShape::Box:
                return "BOX";
            case ParticleEmitterShape::Disc:
                return "DISC";
            case ParticleEmitterShape::Cylinder:
                return "CYL";
            case ParticleEmitterShape::Ring:
                return "RING";
            }
            return "SHAPE";
        }

        static std::string blendName(ParticleBlendMode blend)
        {
            return blend == ParticleBlendMode::Additive ? "ADD" : "ALPHA";
        }

        static void cycleShape(ParticleEmitterComponent& emitter)
        {
            const int next = (static_cast<int>(emitter.shape) + 1) % 5;
            emitter.shape  = static_cast<ParticleEmitterShape>(next);
        }

        static UiColor sliderColor(FloatField field)
        {
            switch (field)
            {
            case FloatField::TintR:
                return ToolTheme::error;
            case FloatField::TintG:
                return ToolTheme::success;
            case FloatField::TintB:
                return ToolTheme::axisZ;
            case FloatField::TintA:
            case FloatField::AlphaPeak:
                return ToolTheme::alpha;
            case FloatField::Vortex:
                return ToolTheme::warning;
            case FloatField::NoiseStrength:
                return ToolTheme::info;
            default:
                return ToolTheme::accent;
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
            {
                emitter.alphaOverLifetime = {{0.0f, 0.0f}, {0.2f, 1.0f}, {1.0f, 0.0f}};
            }
        }

        static float readField(const ParticleEmitterComponent& emitter, FloatField field)
        {
            switch (field)
            {
            case FloatField::EmissionRate:
                return emitter.emissionRate;
            case FloatField::MaxParticles:
                return static_cast<float>(emitter.maxParticles);
            case FloatField::Intensity:
                return emitter.intensity;
            case FloatField::LifetimeMin:
                return emitter.lifetimeMin;
            case FloatField::LifetimeMax:
                return emitter.lifetimeMax;
            case FloatField::VelocitySpread:
                return emitter.velocitySpread;
            case FloatField::Drag:
                return emitter.drag;
            case FloatField::Softness:
                return emitter.softness;
            case FloatField::Vortex:
                return emitter.forces.vortex;
            case FloatField::NoiseStrength:
                return emitter.forces.noiseStrength;
            case FloatField::TintR:
                return emitter.baseTint.r;
            case FloatField::TintG:
                return emitter.baseTint.g;
            case FloatField::TintB:
                return emitter.baseTint.b;
            case FloatField::TintA:
                return emitter.baseTint.a;
            case FloatField::SizeStart:
                return emitter.sizeOverLifetime.empty() ? 0.0f : emitter.sizeOverLifetime.front().value;
            case FloatField::SizeEnd:
                return emitter.sizeOverLifetime.empty() ? 0.0f : emitter.sizeOverLifetime.back().value;
            case FloatField::AlphaPeak:
            {
                float peak = 0.0f;
                for (const ParticleFloatKey& key : emitter.alphaOverLifetime)
                    peak = std::max(peak, key.value);
                return peak;
            }
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
            case FloatField::VelocitySpread:
                emitter.velocitySpread = std::max(0.0f, value);
                break;
            case FloatField::Drag:
                emitter.drag = std::max(0.0f, value);
                break;
            case FloatField::Softness:
                emitter.softness = std::max(0.0f, value);
                break;
            case FloatField::Vortex:
                emitter.forces.vortex = value;
                break;
            case FloatField::NoiseStrength:
                emitter.forces.noiseStrength = std::max(0.0f, value);
                break;
            case FloatField::TintR:
                emitter.baseTint.r = std::clamp(value, 0.0f, 1.0f);
                break;
            case FloatField::TintG:
                emitter.baseTint.g = std::clamp(value, 0.0f, 1.0f);
                break;
            case FloatField::TintB:
                emitter.baseTint.b = std::clamp(value, 0.0f, 1.0f);
                break;
            case FloatField::TintA:
                emitter.baseTint.a = std::clamp(value, 0.0f, 1.0f);
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
            }
        }
    };
} // namespace gts::tools
