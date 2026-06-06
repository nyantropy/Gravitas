#pragma once

#include <string>
#include <vector>

#include "EcsControllerContext.hpp"
#include "GtsCommandBuffer.h"
#include "RegisteredSceneInfo.h"

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
        GtsCommandBuffer* engineCommands = nullptr;
        const std::vector<RegisteredSceneInfo>* registeredScenes = nullptr;
        const std::string* activeSceneName = nullptr;
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
                ctx.engineCommands,
                ctx.registeredScenes,
                ctx.activeSceneName,
                ctx.windowPixelWidth,
                ctx.windowPixelHeight
            };
        }
    };
}
