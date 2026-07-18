#pragma once

#include <algorithm>
#include <cmath>

#include "ECSWorld.hpp"
#include "EcsControllerContext.hpp"
#include "EngineToolInputCaptureComponent.h"
#include "RenderViewportComponent.h"
#include "UiSystem.h"

namespace gts::tools
{
    class EngineToolInputCaptureSystem
    {
    public:
        static constexpr const char* ContextName = "engine.tools";
        static constexpr const char* SelectAction = "engine.tools_select";
        static constexpr const char* OrbitAction = "engine.tool_camera_look";
        static constexpr const char* PanAction = "engine.tool_viewport_pan";

        void clear(ECSWorld& world)
        {
            EngineToolInputCaptureComponent& capture = ensure(world);
            capture = {};
            pointerReady = false;
        }

        void update(ECSWorld& world, const EcsControllerContext& ctx)
        {
            EngineToolInputCaptureComponent& capture = ensure(world);
            const UiDispatchResult& dispatch = ctx.ui == nullptr ? UiDispatchResult{} : ctx.ui->dispatchResult();

            capture.pointerOverToolUi = dispatch.hovered != UI_INVALID_HANDLE;
            capture.toolUiPressed = dispatch.pressed != UI_INVALID_HANDLE;
            capture.keyboardCaptured = dispatch.keyboardConsumed ||
                dispatch.navigationConsumed ||
                dispatch.textInputConsumed;
            capture.worldConsumed = false;

            if (ctx.input == nullptr)
            {
                capture.primaryDown = false;
                capture.primaryPressed = false;
                capture.primaryReleased = false;
                capture.orbitDown = false;
                capture.orbitPressed = false;
                capture.orbitReleased = false;
                capture.panDown = false;
                capture.panPressed = false;
                capture.panReleased = false;
                capture.pointerOverViewport = false;
                capture.pointerPixelX = 0.0f;
                capture.pointerPixelY = 0.0f;
                capture.pointerDeltaX = 0.0f;
                capture.pointerDeltaY = 0.0f;
                capture.viewportPointerX = 0.0f;
                capture.viewportPointerY = 0.0f;
                capture.scrollY = 0.0f;
                pointerReady = false;
                return;
            }

            const double mouseX = ctx.input->mouseX();
            const double mouseY = ctx.input->mouseY();
            capture.pointerPixelX = static_cast<float>(mouseX);
            capture.pointerPixelY = static_cast<float>(mouseY);
            capture.pointerDeltaX = pointerReady ? static_cast<float>(mouseX - previousMouseX) : 0.0f;
            capture.pointerDeltaY = pointerReady ? static_cast<float>(mouseY - previousMouseY) : 0.0f;
            previousMouseX = mouseX;
            previousMouseY = mouseY;
            pointerReady = true;
            capture.scrollY = static_cast<float>(ctx.input->scrollY());

            RenderViewportRect viewport =
                RenderViewportRect::full(std::max(1, static_cast<int>(std::round(ctx.windowPixelWidth))),
                                         std::max(1, static_cast<int>(std::round(ctx.windowPixelHeight))));
            if (world.hasAny<RenderViewportComponent>())
                viewport = world.getSingleton<RenderViewportComponent>().sceneViewport;

            capture.pointerOverViewport = viewport.remapPointer(static_cast<float>(mouseX),
                                                                static_cast<float>(mouseY),
                                                                capture.viewportPointerX,
                                                                capture.viewportPointerY);

            const char* primaryAction = ctx.input->isContextActive(ContextName) ? SelectAction : "engine.ui_primary";
            capture.primaryDown = ctx.input->isHeld(primaryAction);
            capture.primaryPressed = ctx.input->isPressed(primaryAction);
            capture.primaryReleased = ctx.input->isReleased(primaryAction);
            capture.orbitDown = ctx.input->isHeld(OrbitAction);
            capture.orbitPressed = ctx.input->isPressed(OrbitAction);
            capture.orbitReleased = ctx.input->isReleased(OrbitAction);
            capture.panDown = ctx.input->isHeld(PanAction);
            capture.panPressed = ctx.input->isPressed(PanAction);
            capture.panReleased = ctx.input->isReleased(PanAction);
        }

        static const EngineToolInputCaptureComponent* current(ECSWorld& world)
        {
            if (!world.hasAny<EngineToolInputCaptureComponent>())
                return nullptr;
            return &world.getSingleton<EngineToolInputCaptureComponent>();
        }

    private:
        static EngineToolInputCaptureComponent& ensure(ECSWorld& world)
        {
            if (!world.hasAny<EngineToolInputCaptureComponent>())
                return world.createSingleton<EngineToolInputCaptureComponent>();
            return world.getSingleton<EngineToolInputCaptureComponent>();
        }

        bool pointerReady = false;
        double previousMouseX = 0.0;
        double previousMouseY = 0.0;
    };
}
