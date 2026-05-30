#pragma once

#include <string_view>

#include "BitmapFont.h"
#include "ECSWorld.hpp"
#include "EngineToolContext.h"
#include "EngineToolStateComponent.h"
#include "UiInteraction.h"
#include "UiHandle.h"
#include "UiSystem.h"

namespace gts::tools
{
    class EngineToolPanel
    {
    public:
        virtual ~EngineToolPanel() = default;

        virtual std::string_view id() const = 0;
        virtual std::string_view title() const = 0;

        virtual void build(EngineToolContext& ctx, UiHandle parent, BitmapFont* font) = 0;
        virtual void update(EngineToolContext& ctx,
                            EngineToolStateComponent& state,
                            const UiInteractionResult& interaction) = 0;
        virtual void setVisible(UiSystem& ui, bool visible) = 0;
        virtual void destroy(UiSystem& ui) = 0;
    };
}
