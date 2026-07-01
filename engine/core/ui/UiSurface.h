#pragma once

#include <cstdint>
#include <string>

#include "UiAnimationManager.h"
#include "UiAccessibilityManager.h"
#include "UiBindingManager.h"
#include "UiDocument.h"
#include "UiDragDropManager.h"
#include "UiFocusManager.h"
#include "UiInputDispatcher.h"
#include "UiInteraction.h"
#include "UiModalManager.h"
#include "UiMount.h"
#include "UiNavigationGraph.h"
#include "UiTheme.h"
#include "UiTypes.h"

enum class UiSurfaceKind : uint8_t
{
    Screen = 0,
    Viewport,
    RenderTarget,
    World,
    Window,
    Custom
};

struct UiSurfaceDesc
{
    std::string name;
    UiSurfaceKind kind = UiSurfaceKind::Screen;
    int order = 0;
    UiRect rect = {0.0f, 0.0f, 1.0f, 1.0f};
    bool visible = true;
    bool enabled = true;
    bool inputEnabled = true;
    bool renderEnabled = true;
};

class UiSurface
{
public:
    UiSurface();
    UiSurface(UiSurfaceId surfaceId, const UiSurfaceDesc& desc);

    void reset(UiSurfaceId surfaceId, const UiSurfaceDesc& desc);
    void clear();
    void clearInteractionState();

    UiSurfaceId id() const { return surfaceId; }
    const UiSurfaceDesc& desc() const { return surfaceDesc; }
    const std::string& name() const { return surfaceDesc.name; }
    UiSurfaceKind kind() const { return surfaceDesc.kind; }
    int order() const { return surfaceDesc.order; }
    const UiRect& rect() const { return surfaceDesc.rect; }
    bool isVisible() const { return surfaceDesc.visible; }
    bool isEnabled() const { return surfaceDesc.enabled; }
    bool inputEnabled() const { return surfaceDesc.inputEnabled; }
    bool renderEnabled() const { return surfaceDesc.renderEnabled; }

    bool setDesc(const UiSurfaceDesc& desc);
    bool participatesInInput() const;
    bool participatesInRendering() const;
    bool containsScreenPoint(float x, float y) const;
    bool hasPointerOwnership(UiPointerId pointerId = UI_PRIMARY_POINTER) const;
    bool hasKeyboardOwnership() const;
    UiInputFrame toLocalInput(const UiInputFrame& input) const;

    UiDocument& document() { return documentState; }
    const UiDocument& document() const { return documentState; }
    UiFocusManager& focusManager() { return focusState; }
    const UiFocusManager& focusManager() const { return focusState; }
    UiModalManager& modalManager() { return modalState; }
    const UiModalManager& modalManager() const { return modalState; }
    UiMountManager& mountManager() { return mountState; }
    const UiMountManager& mountManager() const { return mountState; }
    UiInputDispatcher& inputDispatcher() { return dispatcher; }
    const UiInputDispatcher& inputDispatcher() const { return dispatcher; }
    UiDragDropManager& dragDropManager() { return dragDropState; }
    const UiDragDropManager& dragDropManager() const { return dragDropState; }
    UiNavigationGraph& navigationGraph() { return navigationState; }
    const UiNavigationGraph& navigationGraph() const { return navigationState; }
    UiAnimationManager& animationManager() { return animationState; }
    const UiAnimationManager& animationManager() const { return animationState; }
    UiAccessibilityManager& accessibilityManager() { return accessibilityState; }
    const UiAccessibilityManager& accessibilityManager() const { return accessibilityState; }
    UiBindingManager& bindingManager() { return bindingState; }
    const UiBindingManager& bindingManager() const { return bindingState; }
    UiTheme& theme() { return themeState; }
    const UiTheme& theme() const { return themeState; }
    void setTheme(const UiTheme& theme);

private:
    UiSurfaceId surfaceId = UI_INVALID_SURFACE;
    UiSurfaceDesc surfaceDesc;
    UiDocument documentState;
    UiFocusManager focusState;
    UiModalManager modalState;
    UiMountManager mountState;
    UiInputDispatcher dispatcher;
    UiDragDropManager dragDropState;
    UiNavigationGraph navigationState;
    UiAnimationManager animationState;
    UiAccessibilityManager accessibilityState;
    UiBindingManager bindingState;
    UiTheme themeState;
};
