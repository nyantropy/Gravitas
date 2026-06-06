#pragma once

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "EngineToolPanel.h"
#include "EngineToolSelectionHelpers.h"
#include "ToolTheme.h"
#include "ToolWidgets.h"
#include "TransformComponent.h"
#include "TransformDirtyHelpers.h"

namespace gts::tools
{
    class EntityInspectorPanel : public EngineToolPanel
    {
        public:
        std::string_view id() const override
        {
            return "entity";
        }
        std::string_view title() const override
        {
            return "Entity";
        }

        void build(EngineToolContext& ctx, UiHandle parent, BitmapFont* font) override
        {
            root    = createContainerRelative(ctx.ui, parent, {0.0f, 0.0f, 1.0f, 1.0f});
            header  = createTextRelative(ctx.ui,
                                         root,
                                         {0.0f, 0.000f, 1.0f, 0.036f},
                                         font,
                                         "ENTITY INSPECTOR",
                                         ToolTheme::text,
                                         ToolTheme::headerTextScale);
            summary = createTextRelative(ctx.ui,
                                         root,
                                         {0.0f, 0.042f, 1.0f, 0.036f},
                                         font,
                                         "NO TRANSFORMS",
                                         ToolTheme::mutedText,
                                         ToolTheme::smallTextScale);

            previousButton = createButtonRelative(
                ctx.ui, root, {0.000f, 0.088f, 0.235f, 0.042f}, font, "PREV", ToolTheme::buttonTextScale);
            nextButton = createButtonRelative(
                ctx.ui, root, {0.255f, 0.088f, 0.235f, 0.042f}, font, "NEXT", ToolTheme::buttonTextScale);
            resetScaleButton = createButtonRelative(
                ctx.ui, root, {0.510f, 0.088f, 0.235f, 0.042f}, font, "UNIT", ToolTheme::buttonTextScale);
            zeroRotationButton = createButtonRelative(
                ctx.ui, root, {0.765f, 0.088f, 0.235f, 0.042f}, font, "ZERO", ToolTheme::buttonTextScale);

            float y = 0.158f;
            addSlider(ctx.ui, font, y, Field::PosX, "POS X", -30.0f, 30.0f);
            y += 0.048f;
            addSlider(ctx.ui, font, y, Field::PosY, "POS Y", -10.0f, 20.0f);
            y += 0.048f;
            addSlider(ctx.ui, font, y, Field::PosZ, "POS Z", -30.0f, 30.0f);
            y += 0.064f;
            addSlider(ctx.ui, font, y, Field::RotX, "ROT X", -3.2f, 3.2f);
            y += 0.048f;
            addSlider(ctx.ui, font, y, Field::RotY, "ROT Y", -3.2f, 3.2f);
            y += 0.048f;
            addSlider(ctx.ui, font, y, Field::RotZ, "ROT Z", -3.2f, 3.2f);
            y += 0.064f;
            addSlider(ctx.ui, font, y, Field::ScaleX, "SCALE X", 0.05f, 5.0f);
            y += 0.048f;
            addSlider(ctx.ui, font, y, Field::ScaleY, "SCALE Y", 0.05f, 5.0f);
            y += 0.048f;
            addSlider(ctx.ui, font, y, Field::ScaleZ, "SCALE Z", 0.05f, 5.0f);

            footer = createTextRelative(ctx.ui,
                                        root,
                                        {0.0f, 0.940f, 1.0f, 0.040f},
                                        font,
                                        "TRANSFORM COMPONENTS",
                                        ToolTheme::statusText,
                                        ToolTheme::smallTextScale);
        }

        void
        update(EngineToolContext& ctx, EngineToolStateComponent& state, const UiInteractionResult& interaction) override
        {
            std::vector<Entity> entities = collectEntities(ctx.world);
            resolveSelection(state, entities);
            TransformComponent* transform = selectedTransform(ctx.world, state);

            if (wasClicked(interaction, previousButton.rect))
                stepSelection(state, entities, -1);
            if (wasClicked(interaction, nextButton.rect))
                stepSelection(state, entities, 1);

            transform = selectedTransform(ctx.world, state);
            if (transform != nullptr)
            {
                if (wasClicked(interaction, resetScaleButton.rect))
                {
                    transform->scale = {1.0f, 1.0f, 1.0f};
                    gts::transform::markDirty(ctx.world, state.selectedEntity);
                    state.status = "SCALE RESET";
                }
                if (wasClicked(interaction, zeroRotationButton.rect))
                {
                    transform->rotation = {0.0f, 0.0f, 0.0f};
                    gts::transform::markDirty(ctx.world, state.selectedEntity);
                    state.status = "ROTATION ZEROED";
                }

                for (const SliderBinding& binding : sliders)
                {
                    if (!isPressed(interaction, binding.slider.track))
                        continue;

                    writeField(*transform,
                               binding.field,
                               valueFromSliderPointer(ctx.ui, binding.slider, interaction.pointerX));
                    gts::transform::markDirty(ctx.world, state.selectedEntity);
                    state.status = "TRANSFORM EDITED";
                    break;
                }
            }

            updateDisplay(ctx, state, entities, transform);
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
        }

        private:
        enum class Field
        {
            PosX,
            PosY,
            PosZ,
            RotX,
            RotY,
            RotZ,
            ScaleX,
            ScaleY,
            ScaleZ
        };

        struct SliderBinding
        {
            Field      field = Field::PosX;
            ToolSlider slider;
        };

        UiHandle                   root    = UI_INVALID_HANDLE;
        UiHandle                   header  = UI_INVALID_HANDLE;
        UiHandle                   summary = UI_INVALID_HANDLE;
        UiHandle                   footer  = UI_INVALID_HANDLE;
        ToolButton                 previousButton;
        ToolButton                 nextButton;
        ToolButton                 resetScaleButton;
        ToolButton                 zeroRotationButton;
        std::vector<SliderBinding> sliders;

        void addSlider(UiSystem&          ui,
                       BitmapFont*        font,
                       float              y,
                       Field              field,
                       const std::string& name,
                       float              minValue,
                       float              maxValue)
        {
            sliders.push_back({field, createSlider(ui, root, y, name, minValue, maxValue, false, font)});
        }

        static std::vector<Entity> collectEntities(ECSWorld& world)
        {
            std::vector<Entity> entities;
            world.forEach<TransformComponent>(
                [&](Entity entity, TransformComponent&)
                {
                    if (isToolInternalEntity(world, entity))
                        return;
                    entities.push_back(entity);
                });
            std::sort(entities.begin(),
                      entities.end(),
                      [](Entity lhs, Entity rhs)
                      {
                          return lhs.id < rhs.id;
                      });
            return entities;
        }

        static void resolveSelection(EngineToolStateComponent& state, const std::vector<Entity>& entities)
        {
            if (entities.empty())
                return;

            const auto it = std::find(entities.begin(), entities.end(), state.selectedEntity);
            if (it == entities.end())
            {
                state.selectedEntity            = entities.front();
                state.selectionSource           = EngineToolSelectionSource::Inspector;
                state.selectionChangedThisFrame = true;
            }
        }

        static TransformComponent* selectedTransform(ECSWorld& world, const EngineToolStateComponent& state)
        {
            if (!isValidToolEntity(state.selectedEntity) ||
                !world.hasComponent<TransformComponent>(state.selectedEntity))
            {
                return nullptr;
            }
            return &world.getComponent<TransformComponent>(state.selectedEntity);
        }

        static void stepSelection(EngineToolStateComponent& state, const std::vector<Entity>& entities, int delta)
        {
            if (entities.empty())
                return;

            auto      it      = std::find(entities.begin(), entities.end(), state.selectedEntity);
            const int current = it == entities.end() ? 0 : static_cast<int>(std::distance(entities.begin(), it));
            const int count   = static_cast<int>(entities.size());
            int       next    = (current + delta) % count;
            if (next < 0)
                next += count;
            state.selectedEntity            = entities[static_cast<size_t>(next)];
            state.selectionSource           = EngineToolSelectionSource::Inspector;
            state.selectionChangedThisFrame = true;
            state.status                    = "SELECTED " + entityDisplayNameForStatus(state);
        }

        void updateDisplay(EngineToolContext&              ctx,
                           const EngineToolStateComponent& state,
                           const std::vector<Entity>&      entities,
                           const TransformComponent*       transform)
        {
            updateButton(ctx.ui, previousButton, "PREV");
            updateButton(ctx.ui, nextButton, "NEXT");
            updateButton(ctx.ui, resetScaleButton, "UNIT");
            updateButton(ctx.ui, zeroRotationButton, "ZERO");

            if (transform == nullptr)
            {
                setText(ctx.ui, summary, "NO TRANSFORM COMPONENTS");
                for (const SliderBinding& binding : sliders)
                {
                    setText(ctx.ui, binding.slider.value, "--");
                    setRelativeRect(ctx.ui, binding.slider.fill, {0.0f, 0.0f, 0.001f, 1.0f});
                }
                setText(ctx.ui, footer, footerText(ctx, state));
                return;
            }

            std::ostringstream label;
            label << entityDisplayName(ctx.world, state.selectedEntity) << " #" << state.selectedEntity.id << " / "
                  << entities.size();
            setText(ctx.ui, summary, label.str());

            for (const SliderBinding& binding : sliders)
            {
                updateSlider(ctx.ui, binding.slider, readField(*transform, binding.field), sliderColor(binding.field));
            }
            const std::string badges = entityComponentBadges(ctx.world, state.selectedEntity);
            if (isValidToolEntity(state.hoveredEntity) && state.hoveredEntity.id != state.selectedEntity.id)
                setText(ctx.ui, footer, footerText(ctx, state));
            else
                setText(ctx.ui, footer, badges.empty() ? footerText(ctx, state) : badges);
        }

        static std::string entityDisplayNameForStatus(const EngineToolStateComponent& state)
        {
            return "ENTITY " + std::to_string(state.selectedEntity.id);
        }

        static std::string footerText(EngineToolContext& ctx, const EngineToolStateComponent& state)
        {
            if (isValidToolEntity(state.hoveredEntity))
                return "HOVER " + entityDisplayName(ctx.world, state.hoveredEntity);
            return state.status;
        }

        static float readField(const TransformComponent& transform, Field field)
        {
            switch (field)
            {
            case Field::PosX:
                return transform.position.x;
            case Field::PosY:
                return transform.position.y;
            case Field::PosZ:
                return transform.position.z;
            case Field::RotX:
                return transform.rotation.x;
            case Field::RotY:
                return transform.rotation.y;
            case Field::RotZ:
                return transform.rotation.z;
            case Field::ScaleX:
                return transform.scale.x;
            case Field::ScaleY:
                return transform.scale.y;
            case Field::ScaleZ:
                return transform.scale.z;
            }
            return 0.0f;
        }

        static void writeField(TransformComponent& transform, Field field, float value)
        {
            switch (field)
            {
            case Field::PosX:
                transform.position.x = value;
                break;
            case Field::PosY:
                transform.position.y = value;
                break;
            case Field::PosZ:
                transform.position.z = value;
                break;
            case Field::RotX:
                transform.rotation.x = value;
                break;
            case Field::RotY:
                transform.rotation.y = value;
                break;
            case Field::RotZ:
                transform.rotation.z = value;
                break;
            case Field::ScaleX:
                transform.scale.x = std::max(0.001f, value);
                break;
            case Field::ScaleY:
                transform.scale.y = std::max(0.001f, value);
                break;
            case Field::ScaleZ:
                transform.scale.z = std::max(0.001f, value);
                break;
            }
        }

        static UiColor sliderColor(Field field)
        {
            switch (field)
            {
            case Field::PosX:
                return color(0.88f, 0.28f, 0.30f, 1.0f);
            case Field::PosY:
                return color(0.30f, 0.76f, 0.42f, 1.0f);
            case Field::PosZ:
                return color(0.25f, 0.52f, 0.92f, 1.0f);
            case Field::RotX:
            case Field::RotY:
            case Field::RotZ:
                return color(0.80f, 0.58f, 0.24f, 1.0f);
            case Field::ScaleX:
            case Field::ScaleY:
            case Field::ScaleZ:
                return color(0.58f, 0.66f, 0.86f, 1.0f);
            }
            return color(0.25f, 0.62f, 0.92f, 1.0f);
        }
    };
} // namespace gts::tools
