#pragma once

#include "EcsControllerContext.hpp"

class UiSystem;

namespace gts::ui
{
    struct UiControllerContext
    {
        UiSystem* ui = nullptr;
    };

    inline const UiControllerContext& controllerContext(const EcsControllerContext& ctx)
    {
        return ctx.frameData.get<UiControllerContext>();
    }

    inline UiControllerContext& controllerContext(EcsControllerContext& ctx)
    {
        return ctx.frameData.edit<UiControllerContext>();
    }
} // namespace gts::ui
