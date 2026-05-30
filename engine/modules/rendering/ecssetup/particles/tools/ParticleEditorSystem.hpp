#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iterator>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "BitmapFontLoader.h"
#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "GraphicsConstants.h"
#include "InputBindingRegistry.h"
#include "ParticleEffectAssetIO.h"
#include "ParticleEmitterComponent.h"
#include "ParticleEmitterRuntimeComponent.h"
#include "RetroFontAtlas.h"
#include "UiSystem.h"
#include "widgets/UiToolWidgets.h"

class ParticleEditorSystem : public ECSControllerSystem
{
public:
    void update(const EcsControllerContext& ctx) override
    {
        if (ctx.input != nullptr && ctx.input->isPressed("engine.particle_editor"))
        {
            visible = !visible;
            if (!visible)
                destroyUi(ctx);
        }

        if (!visible || ctx.ui == nullptr)
            return;

        if (!ensureFont(ctx) || !ensureUi(ctx))
            return;

        cachedUi = ctx.ui;
        std::vector<Entity> emitters = collectEmitters(ctx.world);
        resolveSelection(emitters);

        ParticleEmitterComponent* emitter = selectedEmitter(ctx.world);
        UiInteractionResult interaction = updateInteraction(ctx);
        if (emitter != nullptr)
        {
            applyControls(ctx.world, *emitter, interaction);
            emitter = selectedEmitter(ctx.world);
        }

        updatePanel(ctx.world, emitters, emitter);
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

    struct SliderControl
    {
        FloatField field = FloatField::EmissionRate;
        UiHandle label = UI_INVALID_HANDLE;
        UiHandle value = UI_INVALID_HANDLE;
        UiHandle track = UI_INVALID_HANDLE;
        UiHandle fill = UI_INVALID_HANDLE;
        float minValue = 0.0f;
        float maxValue = 1.0f;
        bool wholeNumber = false;
        std::string name;
    };

    struct ButtonControl
    {
        ButtonAction action = ButtonAction::Restart;
        UiHandle rect = UI_INVALID_HANDLE;
        UiHandle label = UI_INVALID_HANDLE;
        std::string text;
    };

    static constexpr entity_id_type InvalidEntityId = std::numeric_limits<entity_id_type>::max();
    static constexpr float PanelX = 0.018f;
    static constexpr float PanelY = 0.070f;
    static constexpr float PanelW = 0.365f;
    static constexpr float PanelH = 0.900f;
    static constexpr float Pad = 0.014f;
    static constexpr float HeaderScale = 0.020f;
    static constexpr float SmallTextScale = 0.0125f;

    bool visible = false;
    UiHandle root = UI_INVALID_HANDLE;
    UiHandle background = UI_INVALID_HANDLE;
    UiHandle title = UI_INVALID_HANDLE;
    UiHandle subtitle = UI_INVALID_HANDLE;
    UiHandle status = UI_INVALID_HANDLE;
    std::vector<SliderControl> sliders;
    std::vector<ButtonControl> buttons;
    BitmapFont editorFont;
    bool fontReady = false;
    Entity selected = {InvalidEntityId};
    std::string statusText = "F6 TO HIDE";

    static UiColor color(float r, float g, float b, float a = 1.0f)
    {
        return gts::ui::color(r, g, b, a);
    }

    bool ensureFont(const EcsControllerContext& ctx)
    {
        if (fontReady)
            return true;
        if (ctx.resources == nullptr)
            return false;

        editorFont = BitmapFontLoader::load(
            ctx.resources,
            GraphicsConstants::ENGINE_RESOURCES + "/fonts/retrofont.png",
            RetroFontAtlas::ATLAS_W,
            RetroFontAtlas::ATLAS_H,
            RetroFontAtlas::CELL_W,
            RetroFontAtlas::CELL_H,
            RetroFontAtlas::ATLAS_COLS,
            std::string(RetroFontAtlas::CHAR_ORDER),
            RetroFontAtlas::LINE_HEIGHT,
            true);
        fontReady = true;
        return true;
    }

    bool ensureUi(const EcsControllerContext& ctx)
    {
        if (root != UI_INVALID_HANDLE && ctx.ui->findNode(root) != nullptr)
        {
            cachedUi = ctx.ui;
            return true;
        }

        buildUi(*ctx.ui);
        cachedUi = ctx.ui;
        return root != UI_INVALID_HANDLE;
    }

    void buildUi(UiSystem& ui)
    {
        using namespace gts::ui;

        root = ui.createNode(UiNodeType::Container);
        UiLayoutSpec rootLayout = fixedLayout({PanelX, PanelY, PanelW, PanelH});
        rootLayout.clipMode = UiClipMode::ClipChildren;
        ui.setLayout(root, rootLayout);
        ui.setState(root, UiStateFlags{
            .visible = true,
            .enabled = true,
            .interactable = false
        });

        background = createRect(ui, root, {0.0f, 0.0f, PanelW, PanelH}, color(0.025f, 0.030f, 0.036f, 0.92f));
        title = createText(ui, root, {Pad, Pad, 0.2f, 0.03f}, &editorFont,
                           "PARTICLE EDITOR", color(0.95f, 0.98f, 1.0f), HeaderScale);
        subtitle = createText(ui, root, {Pad, 0.050f, 0.3f, 0.03f}, &editorFont,
                              "NO EMITTER", color(0.66f, 0.74f, 0.80f), SmallTextScale);

        float y = 0.087f;
        addButtonRow(ui, y, {
            {ButtonAction::PreviousEmitter, "PREV"},
            {ButtonAction::NextEmitter, "NEXT"},
            {ButtonAction::Reload, "RELOAD"},
            {ButtonAction::Save, "SAVE"},
            {ButtonAction::Restart, "RESET"}
        });
        y += 0.044f;
        addButtonRow(ui, y, {
            {ButtonAction::ToggleEnabled, "ON"},
            {ButtonAction::ToggleLocalSpace, "LOCAL"},
            {ButtonAction::CycleShape, "SHAPE"},
            {ButtonAction::CycleBlend, "BLEND"}
        });

        y += 0.058f;
        addSlider(ui, y, FloatField::EmissionRate, "RATE", 0.0f, 180.0f); y += 0.044f;
        addSlider(ui, y, FloatField::MaxParticles, "MAX", 8.0f, 512.0f, true); y += 0.044f;
        addSlider(ui, y, FloatField::Intensity, "INTENSITY", 0.0f, 2.5f); y += 0.044f;
        addSlider(ui, y, FloatField::LifetimeMin, "LIFE MIN", 0.05f, 6.0f); y += 0.044f;
        addSlider(ui, y, FloatField::LifetimeMax, "LIFE MAX", 0.05f, 8.0f); y += 0.044f;
        addSlider(ui, y, FloatField::VelocitySpread, "SPREAD", 0.0f, 2.0f); y += 0.044f;
        addSlider(ui, y, FloatField::Drag, "DRAG", 0.0f, 3.0f); y += 0.044f;
        addSlider(ui, y, FloatField::Softness, "SOFT", 0.0f, 240.0f); y += 0.044f;
        addSlider(ui, y, FloatField::Vortex, "VORTEX", -2.0f, 2.0f); y += 0.044f;
        addSlider(ui, y, FloatField::NoiseStrength, "NOISE", 0.0f, 1.0f); y += 0.050f;

        addSlider(ui, y, FloatField::TintR, "TINT R", 0.0f, 1.0f); y += 0.038f;
        addSlider(ui, y, FloatField::TintG, "TINT G", 0.0f, 1.0f); y += 0.038f;
        addSlider(ui, y, FloatField::TintB, "TINT B", 0.0f, 1.0f); y += 0.038f;
        addSlider(ui, y, FloatField::TintA, "TINT A", 0.0f, 1.0f); y += 0.045f;

        addSlider(ui, y, FloatField::SizeStart, "SIZE IN", 0.02f, 2.0f); y += 0.038f;
        addSlider(ui, y, FloatField::SizeEnd, "SIZE OUT", 0.02f, 2.0f); y += 0.038f;
        addSlider(ui, y, FloatField::AlphaPeak, "ALPHA PEAK", 0.0f, 1.0f);

        status = createText(ui, root, {Pad, PanelH - 0.040f, 0.32f, 0.025f}, &editorFont,
                            statusText, color(0.80f, 0.86f, 0.92f), SmallTextScale);
    }

    void addButtonRow(UiSystem& ui,
                      float y,
                      const std::vector<std::pair<ButtonAction, std::string>>& rowButtons)
    {
        const float gap = 0.007f;
        const float width = (PanelW - Pad * 2.0f - gap * static_cast<float>(rowButtons.size() - 1))
            / static_cast<float>(rowButtons.size());

        for (size_t i = 0; i < rowButtons.size(); ++i)
        {
            const float x = Pad + static_cast<float>(i) * (width + gap);
            addButton(ui, y, x, width, rowButtons[i].first, rowButtons[i].second);
        }
    }

    void addButton(UiSystem& ui,
                   float y,
                   float x,
                   float width,
                   ButtonAction action,
                   const std::string& text)
    {
        using namespace gts::ui;

        ButtonControl button;
        button.action = action;
        button.text = text;
        button.rect = createRect(ui, root, {x, y, width, 0.032f}, color(0.105f, 0.125f, 0.145f, 0.95f), true);
        button.label = createText(ui, button.rect, {0.007f, 0.008f, width - 0.014f, 0.020f},
                                  &editorFont, text, color(0.90f, 0.94f, 0.98f), SmallTextScale);
        buttons.push_back(button);
    }

    void addSlider(UiSystem& ui,
                   float y,
                   FloatField field,
                   const std::string& name,
                   float minValue,
                   float maxValue,
                   bool wholeNumber = false)
    {
        using namespace gts::ui;

        SliderControl slider;
        slider.field = field;
        slider.minValue = minValue;
        slider.maxValue = maxValue;
        slider.wholeNumber = wholeNumber;
        slider.name = name;
        slider.label = createText(ui, root, {Pad, y, 0.120f, 0.026f}, &editorFont,
                                  name, color(0.72f, 0.79f, 0.85f), SmallTextScale);
        slider.value = createText(ui, root, {PanelW - Pad - 0.072f, y, 0.070f, 0.026f}, &editorFont,
                                  "0", color(0.90f, 0.94f, 0.98f), SmallTextScale);
        slider.track = createRect(ui, root, {0.128f, y + 0.008f, 0.145f, 0.012f},
                                  color(0.080f, 0.095f, 0.110f, 1.0f), true);
        slider.fill = createRect(ui, slider.track, {0.0f, 0.0f, 0.001f, 0.012f},
                                 color(0.25f, 0.62f, 0.92f, 1.0f));
        sliders.push_back(slider);
    }

    void destroyUi(const EcsControllerContext& ctx)
    {
        if (ctx.ui != nullptr && root != UI_INVALID_HANDLE)
            ctx.ui->removeNode(root);

        root = UI_INVALID_HANDLE;
        background = UI_INVALID_HANDLE;
        title = UI_INVALID_HANDLE;
        subtitle = UI_INVALID_HANDLE;
        status = UI_INVALID_HANDLE;
        sliders.clear();
        buttons.clear();
        cachedUi = nullptr;
    }

    static std::vector<Entity> collectEmitters(ECSWorld& world)
    {
        std::vector<Entity> emitters;
        world.forEach<ParticleEmitterComponent>(
            [&](Entity entity, ParticleEmitterComponent&)
            {
                emitters.push_back(entity);
            });
        std::sort(emitters.begin(), emitters.end(),
            [](Entity lhs, Entity rhs)
            {
                return lhs.id < rhs.id;
            });
        return emitters;
    }

    void resolveSelection(const std::vector<Entity>& emitters)
    {
        if (emitters.empty())
        {
            selected = {InvalidEntityId};
            return;
        }

        const auto it = std::find(emitters.begin(), emitters.end(), selected);
        if (it == emitters.end())
            selected = emitters.front();
    }

    ParticleEmitterComponent* selectedEmitter(ECSWorld& world)
    {
        if (selected.id == InvalidEntityId || !world.hasComponent<ParticleEmitterComponent>(selected))
            return nullptr;
        return &world.getComponent<ParticleEmitterComponent>(selected);
    }

    UiInteractionResult updateInteraction(const EcsControllerContext& ctx)
    {
        if (ctx.input == nullptr || ctx.ui == nullptr)
            return {};

        const float width = std::max(1.0f, ctx.windowPixelWidth);
        const float height = std::max(1.0f, ctx.windowPixelHeight);

        UiInputFrame frame;
        frame.pointerX = glm::clamp(static_cast<float>(ctx.input->mouseX()) / width, 0.0f, 1.0f);
        frame.pointerY = glm::clamp(static_cast<float>(ctx.input->mouseY()) / height, 0.0f, 1.0f);
        frame.primaryDown = ctx.input->isHeld("engine.ui_primary");
        frame.primaryPressed = ctx.input->isPressed("engine.ui_primary");
        frame.primaryReleased = ctx.input->isReleased("engine.ui_primary");
        frame.scrollX = static_cast<float>(ctx.input->scrollX());
        frame.scrollY = static_cast<float>(ctx.input->scrollY());
        return ctx.ui->updateInteraction(frame);
    }

    void applyControls(ECSWorld& world,
                       ParticleEmitterComponent& emitter,
                       const UiInteractionResult& interaction)
    {
        if (interaction.clicked != UI_INVALID_HANDLE)
        {
            for (const ButtonControl& button : buttons)
            {
                if (button.rect == interaction.clicked)
                {
                    applyButton(world, emitter, button.action);
                    break;
                }
            }
        }

        if (interaction.pressed == UI_INVALID_HANDLE)
            return;

        for (const SliderControl& slider : sliders)
        {
            if (slider.track != interaction.pressed)
                continue;

            setSliderFromPointer(emitter, slider, interaction.pointerX);
            break;
        }
    }

    void setSliderFromPointer(ParticleEmitterComponent& emitter,
                              const SliderControl& slider,
                              float pointerX)
    {
        if (root == UI_INVALID_HANDLE)
            return;

        const UiNode* node = nullptr;
        node = currentUiNode(slider.track);
        if (node == nullptr)
            return;

        const UiRect& bounds = node->computedLayout.bounds;
        const float t = bounds.width <= 0.0f
            ? 0.0f
            : glm::clamp((pointerX - bounds.x) / bounds.width, 0.0f, 1.0f);
        float value = slider.minValue + (slider.maxValue - slider.minValue) * t;
        if (slider.wholeNumber)
            value = std::round(value);
        writeField(emitter, slider.field, value);
    }

    const UiNode* currentUiNode(UiHandle handle) const
    {
        return cachedUi == nullptr ? nullptr : cachedUi->findNode(handle);
    }

    void applyButton(ECSWorld& world,
                     ParticleEmitterComponent& emitter,
                     ButtonAction action)
    {
        std::vector<Entity> emitters = collectEmitters(world);
        switch (action)
        {
            case ButtonAction::PreviousEmitter:
                stepSelection(emitters, -1);
                break;
            case ButtonAction::NextEmitter:
                stepSelection(emitters, 1);
                break;
            case ButtonAction::ToggleEnabled:
                emitter.enabled = !emitter.enabled;
                statusText = emitter.enabled ? "EMITTER ENABLED" : "EMITTER DISABLED";
                break;
            case ButtonAction::ToggleLocalSpace:
                emitter.localSpace = !emitter.localSpace;
                restartEmitter(world);
                statusText = emitter.localSpace ? "LOCAL SPACE" : "WORLD SPACE";
                break;
            case ButtonAction::CycleShape:
                cycleShape(emitter);
                restartEmitter(world);
                statusText = "SHAPE " + shapeName(emitter.shape);
                break;
            case ButtonAction::CycleBlend:
                emitter.blend = emitter.blend == ParticleBlendMode::Alpha
                    ? ParticleBlendMode::Additive
                    : ParticleBlendMode::Alpha;
                statusText = "BLEND " + blendName(emitter.blend);
                break;
            case ButtonAction::Reload:
                reloadEmitter(world, emitter);
                break;
            case ButtonAction::Save:
                saveEmitter(emitter);
                break;
            case ButtonAction::Restart:
                restartEmitter(world);
                statusText = "RUNTIME RESET";
                break;
        }
    }

    void stepSelection(const std::vector<Entity>& emitters, int delta)
    {
        if (emitters.empty())
            return;

        auto it = std::find(emitters.begin(), emitters.end(), selected);
        const int current = it == emitters.end()
            ? 0
            : static_cast<int>(std::distance(emitters.begin(), it));
        const int count = static_cast<int>(emitters.size());
        int next = (current + delta) % count;
        if (next < 0)
            next += count;
        selected = emitters[static_cast<size_t>(next)];
        statusText = "SELECTED ENTITY " + std::to_string(selected.id);
    }

    void restartEmitter(ECSWorld& world)
    {
        if (selected.id == InvalidEntityId
            || !world.hasComponent<ParticleEmitterRuntimeComponent>(selected))
        {
            return;
        }

        ParticleEmitterRuntimeComponent& runtime =
            world.getComponent<ParticleEmitterRuntimeComponent>(selected);
        runtime.particles.clear();
        runtime.spawnAccumulator = 0.0f;
        runtime.emitterAge = 0.0f;
        runtime.burstRepeatCounts.clear();
    }

    void reloadEmitter(ECSWorld& world, ParticleEmitterComponent& emitter)
    {
        if (emitter.effectPath.empty())
        {
            statusText = "NO EFFECT PATH";
            return;
        }

        const std::string path = emitter.effectPath;
        const uint32_t seed = emitter.randomSeed;
        if (!gts::particles::loadParticleEffect(path, emitter))
        {
            statusText = "RELOAD FAILED";
            return;
        }

        emitter.effectPath = path;
        emitter.randomSeed = seed;
        restartEmitter(world);
        statusText = "RELOADED";
    }

    void saveEmitter(const ParticleEmitterComponent& emitter)
    {
        if (emitter.effectPath.empty())
        {
            statusText = "NO EFFECT PATH";
            return;
        }

        statusText = gts::particles::saveParticleEffect(emitter.effectPath, emitter)
            ? "SAVED"
            : "SAVE FAILED";
    }

    static void cycleShape(ParticleEmitterComponent& emitter)
    {
        const int next = (static_cast<int>(emitter.shape) + 1) % 5;
        emitter.shape = static_cast<ParticleEmitterShape>(next);
    }

    void updatePanel(ECSWorld& world,
                     const std::vector<Entity>& emitters,
                     ParticleEmitterComponent* emitter)
    {
        if (cachedUi == nullptr)
            return;

        gts::ui::setText(*cachedUi, subtitle, emitterSummary(emitters, emitter));
        std::string footer = statusText;

        for (const ButtonControl& button : buttons)
            updateButton(*cachedUi, button, emitter);

        if (emitter == nullptr)
        {
            for (const SliderControl& slider : sliders)
            {
                gts::ui::setText(*cachedUi, slider.value, "--");
                gts::ui::setRect(*cachedUi, slider.fill, {0.0f, 0.0f, 0.001f, 0.012f});
            }
            gts::ui::setText(*cachedUi, status, footer);
            return;
        }

        for (const SliderControl& slider : sliders)
        {
            const float value = readField(*emitter, slider.field);
            const float t = slider.maxValue <= slider.minValue
                ? 0.0f
                : glm::clamp((value - slider.minValue) / (slider.maxValue - slider.minValue), 0.0f, 1.0f);
            gts::ui::setRect(*cachedUi, slider.fill, {0.0f, 0.0f, 0.145f * t, 0.012f});
            gts::ui::setText(*cachedUi, slider.value, formatValue(value, slider.wholeNumber));
            gts::ui::setRectColor(*cachedUi, slider.fill, sliderColor(slider.field));
        }

        if (selected.id != InvalidEntityId
            && world.hasComponent<ParticleEmitterRuntimeComponent>(selected))
        {
            const auto& runtime = world.getComponent<ParticleEmitterRuntimeComponent>(selected);
            footer += "  LIVE " + std::to_string(runtime.particles.size()) + " P";
        }
        gts::ui::setText(*cachedUi, status, footer);
    }

    void updateButton(UiSystem& ui,
                      const ButtonControl& button,
                      const ParticleEmitterComponent* emitter)
    {
        std::string label = button.text;
        if (emitter != nullptr)
        {
            switch (button.action)
            {
                case ButtonAction::ToggleEnabled:
                    label = emitter->enabled ? "ON" : "OFF";
                    break;
                case ButtonAction::ToggleLocalSpace:
                    label = emitter->localSpace ? "LOCAL" : "WORLD";
                    break;
                case ButtonAction::CycleShape:
                    label = shapeName(emitter->shape);
                    break;
                case ButtonAction::CycleBlend:
                    label = blendName(emitter->blend);
                    break;
                default:
                    break;
            }
        }

        const UiNode* node = ui.findNode(button.rect);
        const bool hovered = node != nullptr && node->state.hovered;
        const bool pressed = node != nullptr && node->state.pressed;
        const UiColor buttonColor = pressed
            ? color(0.19f, 0.28f, 0.34f, 1.0f)
            : (hovered ? color(0.14f, 0.19f, 0.23f, 1.0f) : color(0.105f, 0.125f, 0.145f, 0.95f));
        gts::ui::setRectColor(ui, button.rect, buttonColor);
        gts::ui::setText(ui, button.label, label);
    }

    std::string emitterSummary(const std::vector<Entity>& emitters,
                               const ParticleEmitterComponent* emitter) const
    {
        if (emitter == nullptr)
            return "NO PARTICLE EMITTERS";

        std::string path = emitter->effectPath;
        const size_t slash = path.find_last_of("/\\");
        if (slash != std::string::npos)
            path = path.substr(slash + 1);
        if (path.empty())
            path = "DESCRIPTOR ONLY";

        std::ostringstream out;
        out << "ENTITY " << selected.id << " / " << emitters.size() << "  " << path;
        return out.str();
    }

    static std::string formatValue(float value, bool wholeNumber)
    {
        std::ostringstream out;
        if (wholeNumber)
            out << static_cast<int>(std::round(value));
        else
            out << std::fixed << std::setprecision(value >= 10.0f ? 1 : 2) << value;
        return out.str();
    }

    static UiColor sliderColor(FloatField field)
    {
        switch (field)
        {
            case FloatField::TintR: return color(0.88f, 0.22f, 0.24f, 1.0f);
            case FloatField::TintG: return color(0.20f, 0.72f, 0.36f, 1.0f);
            case FloatField::TintB: return color(0.22f, 0.50f, 0.92f, 1.0f);
            case FloatField::TintA:
            case FloatField::AlphaPeak: return color(0.72f, 0.76f, 0.82f, 1.0f);
            case FloatField::Vortex: return color(0.96f, 0.50f, 0.20f, 1.0f);
            case FloatField::NoiseStrength: return color(0.40f, 0.76f, 0.86f, 1.0f);
            default: return color(0.25f, 0.62f, 0.92f, 1.0f);
        }
    }

    static std::string shapeName(ParticleEmitterShape shape)
    {
        switch (shape)
        {
            case ParticleEmitterShape::Sphere: return "SPHERE";
            case ParticleEmitterShape::Box: return "BOX";
            case ParticleEmitterShape::Disc: return "DISC";
            case ParticleEmitterShape::Cylinder: return "CYL";
            case ParticleEmitterShape::Ring: return "RING";
        }
        return "SHAPE";
    }

    static std::string blendName(ParticleBlendMode blend)
    {
        return blend == ParticleBlendMode::Additive ? "ADD" : "ALPHA";
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
            emitter.alphaOverLifetime = {
                {0.0f, 0.0f},
                {0.2f, 1.0f},
                {1.0f, 0.0f}
            };
        }
    }

    static float readField(const ParticleEmitterComponent& emitter, FloatField field)
    {
        switch (field)
        {
            case FloatField::EmissionRate: return emitter.emissionRate;
            case FloatField::MaxParticles: return static_cast<float>(emitter.maxParticles);
            case FloatField::Intensity: return emitter.intensity;
            case FloatField::LifetimeMin: return emitter.lifetimeMin;
            case FloatField::LifetimeMax: return emitter.lifetimeMax;
            case FloatField::VelocitySpread: return emitter.velocitySpread;
            case FloatField::Drag: return emitter.drag;
            case FloatField::Softness: return emitter.softness;
            case FloatField::Vortex: return emitter.forces.vortex;
            case FloatField::NoiseStrength: return emitter.forces.noiseStrength;
            case FloatField::TintR: return emitter.baseTint.r;
            case FloatField::TintG: return emitter.baseTint.g;
            case FloatField::TintB: return emitter.baseTint.b;
            case FloatField::TintA: return emitter.baseTint.a;
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
                emitter.lifetimeMin = glm::clamp(value, 0.01f, std::max(0.01f, emitter.lifetimeMax));
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
                emitter.baseTint.r = glm::clamp(value, 0.0f, 1.0f);
                break;
            case FloatField::TintG:
                emitter.baseTint.g = glm::clamp(value, 0.0f, 1.0f);
                break;
            case FloatField::TintB:
                emitter.baseTint.b = glm::clamp(value, 0.0f, 1.0f);
                break;
            case FloatField::TintA:
                emitter.baseTint.a = glm::clamp(value, 0.0f, 1.0f);
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
                        key.value = glm::clamp(value, 0.0f, 1.0f);
                }
                break;
        }
    }

    UiSystem* cachedUi = nullptr;
};
