#pragma once

#include <algorithm>
#include <array>
#include <sstream>
#include <string>
#include <vector>

#include "EngineToolPanel.h"
#include "ParticleEffectAsset.h"
#include "ParticleEmitterComponent.h"
#include "ToolWidgets.h"

namespace gts::tools
{
    class AssetStatusPanel : public EngineToolPanel
    {
    public:
        std::string_view id() const override { return "assets"; }
        std::string_view title() const override { return "Assets"; }

        void build(EngineToolContext& ctx, UiHandle parent, BitmapFont* font) override
        {
            root = createContainer(ctx.ui, parent, {0.0f, 0.0f, 0.390f, 0.760f});
            header = createText(ctx.ui, root, {0.014f, 0.006f, 0.300f, 0.030f}, font,
                                "ASSET STATUS", color(0.95f, 0.98f, 1.0f), 0.016f);
            summary = createText(ctx.ui, root, {0.014f, 0.038f, 0.350f, 0.028f}, font,
                                 "NO ASSETS", color(0.66f, 0.74f, 0.80f), 0.0125f);

            float y = 0.086f;
            for (size_t i = 0; i < lines.size(); ++i)
            {
                lines[i] = createText(ctx.ui, root, {0.014f, y, 0.360f, 0.024f}, font,
                                      "", color(0.82f, 0.88f, 0.92f), 0.0125f);
                y += 0.032f;
            }

            footer = createText(ctx.ui, root, {0.014f, 0.724f, 0.350f, 0.024f}, font,
                                "PARTICLE EFFECT REGISTRY", color(0.80f, 0.86f, 0.92f), 0.0125f);
        }

        void update(EngineToolContext& ctx,
                    EngineToolStateComponent& state,
                    const UiInteractionResult&) override
        {
            size_t emitterCount = 0;
            ctx.world.forEach<ParticleEmitterComponent>(
                [&](Entity, ParticleEmitterComponent&)
                {
                    ++emitterCount;
                });

            const ParticleEffectRegistryComponent* registry = nullptr;
            if (ctx.world.hasAny<ParticleEffectRegistryComponent>())
                registry = &ctx.world.getSingleton<ParticleEffectRegistryComponent>();

            const size_t effectCount = registry == nullptr ? 0 : registry->effects.size();
            std::ostringstream summaryText;
            summaryText << emitterCount << " EMITTERS  " << effectCount << " EFFECT FILES";
            setText(ctx.ui, summary, summaryText.str());

            std::vector<std::string> rows;
            rows.reserve(effectCount);
            if (registry != nullptr)
            {
                for (const auto& [path, asset] : registry->effects)
                {
                    std::string file = shorten(path);
                    std::ostringstream row;
                    row << (asset.loaded ? "OK " : "NO ")
                        << "V" << asset.version << "  "
                        << file;
                    rows.push_back(row.str());
                }
            }
            std::sort(rows.begin(), rows.end());

            for (size_t i = 0; i < lines.size(); ++i)
            {
                if (i < rows.size())
                    setText(ctx.ui, lines[i], rows[i]);
                else
                    setText(ctx.ui, lines[i], "");
            }

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
            lines.fill(UI_INVALID_HANDLE);
        }

    private:
        UiHandle root = UI_INVALID_HANDLE;
        UiHandle header = UI_INVALID_HANDLE;
        UiHandle summary = UI_INVALID_HANDLE;
        UiHandle footer = UI_INVALID_HANDLE;
        std::array<UiHandle, 16> lines{};

        static std::string shorten(const std::string& path)
        {
            const size_t slash = path.find_last_of("/\\");
            std::string file = slash == std::string::npos ? path : path.substr(slash + 1);
            if (file.size() <= 30)
                return file;
            return file.substr(0, 27) + "...";
        }
    };
}
