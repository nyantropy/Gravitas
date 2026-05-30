#pragma once

#include "EcsControllerContext.hpp"

class ECSWorld;
class InputBindingRegistry;
class IResourceProvider;
class UiSystem;

namespace gts::tools
{
    struct EngineToolContext
    {
        ECSWorld& world;
        UiSystem& ui;
        InputBindingRegistry* input = nullptr;
        IResourceProvider* resources = nullptr;
        const TimeContext* time = nullptr;
        float windowPixelWidth = 1.0f;
        float windowPixelHeight = 1.0f;

        static EngineToolContext fromController(const EcsControllerContext& ctx)
        {
            return {
                ctx.world,
                *ctx.ui,
                ctx.input,
                ctx.resources,
                ctx.time,
                ctx.windowPixelWidth,
                ctx.windowPixelHeight
            };
        }
    };
}
