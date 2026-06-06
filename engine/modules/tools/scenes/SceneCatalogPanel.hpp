#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "EngineToolPanel.h"
#include "RegisteredSceneInfo.h"
#include "ToolTheme.h"
#include "ToolWidgets.h"

namespace gts::tools
{
    // Tooling panel that lists registered scenes and requests engine scene changes.
    class SceneCatalogPanel : public EngineToolPanel
    {
        public:
        std::string_view id() const override
        {
            return "scenes";
        }

        std::string_view title() const override
        {
            return "Scenes";
        }

        void build(EngineToolContext& ctx, UiHandle parent, BitmapFont* font) override
        {
            root    = createContainerRelative(ctx.ui, parent, {0.0f, 0.0f, 1.0f, 1.0f});
            header  = createTextRelative(ctx.ui,
                                         root,
                                         {0.0f, 0.000f, 1.0f, 0.036f},
                                         font,
                                         "SCENE CATALOG",
                                         ToolTheme::text,
                                         ToolTheme::headerTextScale);
            summary = createTextRelative(ctx.ui,
                                         root,
                                         {0.0f, 0.042f, 1.0f, 0.036f},
                                         font,
                                         "NO SCENES",
                                         ToolTheme::mutedText,
                                         ToolTheme::smallTextScale);

            rows.clear();
            float y = 0.092f;
            for (size_t i = 0; i < MAX_ROWS; ++i)
            {
                rows.push_back(
                    createButtonRelative(ctx.ui, root, {0.0f, y, 1.0f, 0.040f}, font, "--", ToolTheme::smallTextScale));
                y += 0.046f;
            }

            footer = createTextRelative(ctx.ui,
                                        root,
                                        {0.0f, 0.940f, 1.0f, 0.040f},
                                        font,
                                        "REGISTERED ENGINE SCENES",
                                        ToolTheme::statusText,
                                        ToolTheme::smallTextScale);
        }

        void
        update(EngineToolContext& ctx, EngineToolStateComponent& state, const UiInteractionResult& interaction) override
        {
            const std::vector<RegisteredSceneInfo>* scenes = ctx.registeredScenes;
            const std::string activeScene                  = ctx.activeSceneName == nullptr ? "" : *ctx.activeSceneName;
            if (scenes == nullptr)
            {
                setText(ctx.ui, summary, "NO SCENE CATALOG");
                hideRows(ctx.ui);
                setText(ctx.ui, footer, state.status);
                return;
            }

            const size_t visibleCount = std::min(scenes->size(), rows.size());
            for (size_t i = 0; i < rows.size(); ++i)
            {
                const bool visible = i < visibleCount;
                setVisibleRecursive(ctx.ui, rows[i].rect, visible);
                if (!visible)
                    continue;

                const RegisteredSceneInfo& scene = (*scenes)[i];
                if (wasClicked(interaction, rows[i].rect))
                    requestSceneChange(ctx, state, scene, activeScene);

                updateButton(ctx.ui, rows[i], rowLabel(scene, activeScene));
                if (scene.id == activeScene)
                    setRectColor(ctx.ui, rows[i].rect, ToolTheme::buttonActive);
            }

            setText(ctx.ui, summary, std::to_string(scenes->size()) + " REGISTERED SCENES");
            setText(ctx.ui, footer, state.status);
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
        static constexpr size_t MAX_ROWS = 16;

        UiHandle                root    = UI_INVALID_HANDLE;
        UiHandle                header  = UI_INVALID_HANDLE;
        UiHandle                summary = UI_INVALID_HANDLE;
        UiHandle                footer  = UI_INVALID_HANDLE;
        std::vector<ToolButton> rows;

        void hideRows(UiSystem& ui)
        {
            for (const ToolButton& row : rows)
                setVisibleRecursive(ui, row.rect, false);
        }

        static std::string displayName(const RegisteredSceneInfo& scene)
        {
            return scene.displayName.empty() ? scene.id : scene.displayName;
        }

        static std::string rowLabel(const RegisteredSceneInfo& scene, const std::string& activeScene)
        {
            const std::string prefix = scene.id == activeScene ? "ACTIVE " : "LOAD ";
            return prefix + displayName(scene);
        }

        static void requestSceneChange(EngineToolContext&         ctx,
                                       EngineToolStateComponent&  state,
                                       const RegisteredSceneInfo& scene,
                                       const std::string&         activeScene)
        {
            if (scene.id.empty() || scene.id == activeScene)
            {
                state.status = "SCENE ACTIVE";
                return;
            }

            if (ctx.engineCommands == nullptr)
            {
                state.status = "NO SCENE COMMANDS";
                return;
            }

            ctx.engineCommands->requestChangeScene(scene.id);
            state.status = "LOADING " + displayName(scene);
        }
    };
} // namespace gts::tools
