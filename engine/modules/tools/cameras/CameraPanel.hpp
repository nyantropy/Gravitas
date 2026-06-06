#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

#include "ActiveCameraViewStateComponent.h"
#include "CameraDescriptionComponent.h"
#include "ECSWorld.hpp"
#include "EngineToolCameraStateComponent.h"
#include "EngineToolPanel.h"
#include "EngineToolSelectionHelpers.h"
#include "GlmConfig.h"
#include "ToolWidgets.h"
#include "TransformComponent.h"

namespace gts::tools
{
    class CameraPanel : public EngineToolPanel
    {
        public:
        std::string_view id() const override
        {
            return "camera";
        }
        std::string_view title() const override
        {
            return "Camera";
        }

        void build(EngineToolContext& ctx, UiHandle parent, BitmapFont* font) override
        {
            root   = createContainer(ctx.ui, parent, {0.0f, 0.0f, 0.390f, 0.760f});
            header = createText(
                ctx.ui, root, {0.014f, 0.006f, 0.300f, 0.030f}, font, "CAMERAS", color(0.95f, 0.98f, 1.0f), 0.016f);
            summary = createText(ctx.ui,
                                 root,
                                 {0.014f, 0.038f, 0.350f, 0.028f},
                                 font,
                                 "NO CAMERAS",
                                 color(0.66f, 0.74f, 0.80f),
                                 0.0125f);

            rows.clear();
            float y = 0.080f;
            for (size_t i = 0; i < MAX_ROWS; ++i)
            {
                rows.push_back(createButton(ctx.ui, root, {0.014f, y, 0.360f, 0.038f}, font, "--", 0.0115f));
                y += 0.044f;
            }

            detail = createText(ctx.ui,
                                root,
                                {0.014f, 0.650f, 0.355f, 0.078f},
                                font,
                                "SELECT A CAMERA",
                                color(0.80f, 0.86f, 0.92f),
                                0.0125f);
        }

        void
        update(EngineToolContext& ctx, EngineToolStateComponent& state, const UiInteractionResult& interaction) override
        {
            const std::vector<Entity> cameras = collectCameras(ctx.world);
            for (size_t i = 0; i < rows.size(); ++i)
            {
                if (i >= cameras.size())
                    continue;

                if (wasClicked(interaction, rows[i].rect))
                {
                    state.selectedEntity            = cameras[i];
                    state.selectionSource           = EngineToolSelectionSource::Inspector;
                    state.selectionChangedThisFrame = true;
                    state.status                    = "SELECTED " + entityDisplayName(ctx.world, cameras[i]);
                }
            }

            updateDisplay(ctx, state, cameras);
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
            rows.clear();
        }

        private:
        static constexpr size_t MAX_ROWS = 10;

        UiHandle                root    = UI_INVALID_HANDLE;
        UiHandle                header  = UI_INVALID_HANDLE;
        UiHandle                summary = UI_INVALID_HANDLE;
        UiHandle                detail  = UI_INVALID_HANDLE;
        std::vector<ToolButton> rows;

        static std::vector<Entity> collectCameras(ECSWorld& world)
        {
            std::vector<Entity> cameras;
            world.forEach<CameraDescriptionComponent>(
                [&](Entity entity, CameraDescriptionComponent&)
                {
                    cameras.push_back(entity);
                });
            std::sort(cameras.begin(),
                      cameras.end(),
                      [](Entity lhs, Entity rhs)
                      {
                          return lhs.id < rhs.id;
                      });
            return cameras;
        }

        void
        updateDisplay(EngineToolContext& ctx, const EngineToolStateComponent& state, const std::vector<Entity>& cameras)
        {
            setText(
                ctx.ui, summary, cameras.empty() ? "NO CAMERAS" : std::to_string(cameras.size()) + " CAMERA ENTITIES");

            for (size_t i = 0; i < rows.size(); ++i)
            {
                const bool visible = i < cameras.size();
                setVisibleRecursive(ctx.ui, rows[i].rect, visible);
                if (!visible)
                    continue;

                const Entity camera = cameras[i];
                updateButton(ctx.ui, rows[i], rowText(ctx.world, state, camera));
                setRectColor(ctx.ui, rows[i].rect, rowColor(ctx.world, state, camera));
            }

            setText(ctx.ui, detail, detailText(ctx.world, state));
        }

        static std::string rowText(ECSWorld& world, const EngineToolStateComponent& state, Entity camera)
        {
            std::string label = cameraStatus(world, state, camera) + " ";
            label += truncate(entityDisplayName(world, camera), 22);
            label += " #";
            label += std::to_string(camera.id);
            return label;
        }

        static UiColor rowColor(ECSWorld& world, const EngineToolStateComponent& state, Entity camera)
        {
            if (state.selectedEntity.id == camera.id)
                return color(0.34f, 0.28f, 0.11f, 1.0f);
            if (isToolCamera(world, camera))
                return color(0.08f, 0.26f, 0.30f, 1.0f);
            if (isActiveCamera(world, camera))
                return color(0.12f, 0.25f, 0.16f, 1.0f);
            return color(0.105f, 0.125f, 0.145f, 0.95f);
        }

        static std::string cameraStatus(ECSWorld& world, const EngineToolStateComponent& state, Entity camera)
        {
            if (state.selectedEntity.id == camera.id)
                return "SEL";
            if (isToolCamera(world, camera))
                return "TOOL";
            if (isActiveCamera(world, camera))
                return "VIEW";
            return "CAM";
        }

        static bool isToolCamera(ECSWorld& world, Entity camera)
        {
            return world.hasAny<EngineToolCameraStateComponent>() &&
                   world.getSingleton<EngineToolCameraStateComponent>().toolCameraEntity.id == camera.id;
        }

        static bool isActiveCamera(ECSWorld& world, Entity camera)
        {
            if (world.hasAny<ActiveCameraViewStateComponent>())
                return world.getSingleton<ActiveCameraViewStateComponent>().activeCamera.id == camera.id;

            if (!world.hasComponent<CameraDescriptionComponent>(camera))
                return false;

            return world.getComponent<CameraDescriptionComponent>(camera).active;
        }

        static std::string detailText(ECSWorld& world, const EngineToolStateComponent& state)
        {
            if (!isValidToolEntity(state.selectedEntity) ||
                !world.hasComponent<CameraDescriptionComponent>(state.selectedEntity))
            {
                return "SELECT A CAMERA";
            }

            const CameraDescriptionComponent& camera =
                world.getComponent<CameraDescriptionComponent>(state.selectedEntity);
            std::ostringstream out;
            out << entityDisplayName(world, state.selectedEntity) << " | FOV "
                << static_cast<int>(std::round(glm::degrees(camera.fov))) << " | NEAR " << camera.nearClip << " | FAR "
                << camera.farClip;

            if (world.hasComponent<TransformComponent>(state.selectedEntity))
            {
                const glm::vec3 position = world.getComponent<TransformComponent>(state.selectedEntity).position;
                out << " | POS " << static_cast<int>(std::round(position.x)) << ","
                    << static_cast<int>(std::round(position.y)) << "," << static_cast<int>(std::round(position.z));
            }
            return out.str();
        }

        static std::string truncate(const std::string& value, size_t maxLength)
        {
            if (value.size() <= maxLength)
                return value;
            if (maxLength <= 3)
                return value.substr(0, maxLength);
            return value.substr(0, maxLength - 3) + "...";
        }
    };
} // namespace gts::tools
