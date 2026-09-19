#include "UiSurface.h"

#include <algorithm>

UiSurface::UiSurface()
{
    reset(UI_INVALID_SURFACE, UiSurfaceDesc{});
}

UiSurface::UiSurface(UiSurfaceId inSurfaceId, const UiSurfaceDesc& desc)
{
    reset(inSurfaceId, desc);
}

void UiSurface::reset(UiSurfaceId inSurfaceId, const UiSurfaceDesc& desc)
{
    surfaceId = inSurfaceId;
    surfaceDesc = desc;
    if (surfaceDesc.name.empty())
        surfaceDesc.name = surfaceId == UI_DEFAULT_SURFACE ? "default-screen" : "surface";
    themeState = defaultUiTheme();

    clear();
}

void UiSurface::clear()
{
    documentState.clear();
    focusState.clear();
    modalState.clear();
    mountState.reset(documentState);
    dragDropState.clear();
    navigationState.clear();
    animationState.clear();
    accessibilityState.clear();
    bindingState.clear();
    dispatcher.clear();
}

void UiSurface::clearInteractionState()
{
    modalState.clear(documentState, focusState);
    focusState.clear(documentState);
    dragDropState.clear();
    dispatcher.clear();
}

bool UiSurface::setDesc(const UiSurfaceDesc& desc)
{
    if (surfaceDesc.name == desc.name &&
        surfaceDesc.kind == desc.kind &&
        surfaceDesc.order == desc.order &&
        surfaceDesc.rect == desc.rect &&
        surfaceDesc.visible == desc.visible &&
        surfaceDesc.enabled == desc.enabled &&
        surfaceDesc.inputEnabled == desc.inputEnabled &&
        surfaceDesc.renderEnabled == desc.renderEnabled)
    {
        return false;
    }

    surfaceDesc = desc;
    if (surfaceDesc.name.empty())
        surfaceDesc.name = surfaceId == UI_DEFAULT_SURFACE ? "default-screen" : "surface";

    if (!participatesInInput())
        clearInteractionState();

    return true;
}

bool UiSurface::participatesInInput() const
{
    return surfaceDesc.visible &&
           surfaceDesc.enabled &&
           surfaceDesc.inputEnabled &&
           surfaceDesc.rect.width > 0.0f &&
           surfaceDesc.rect.height > 0.0f;
}

bool UiSurface::participatesInRendering() const
{
    return surfaceDesc.visible &&
           surfaceDesc.enabled &&
           surfaceDesc.renderEnabled &&
           surfaceDesc.rect.width > 0.0f &&
           surfaceDesc.rect.height > 0.0f;
}

bool UiSurface::containsScreenPoint(float x, float y) const
{
    const UiRect& r = surfaceDesc.rect;
    return r.width > 0.0f &&
           r.height > 0.0f &&
           x >= r.x &&
           y >= r.y &&
           x <= r.x + r.width &&
           y <= r.y + r.height;
}

bool UiSurface::hasPointerOwnership(UiPointerId pointerId) const
{
    return focusState.capturedNode(pointerId) != UI_INVALID_HANDLE ||
           focusState.activePointerNode(pointerId) != UI_INVALID_HANDLE;
}

bool UiSurface::hasKeyboardOwnership() const
{
    return focusState.keyboardFocusedNode() != UI_INVALID_HANDLE ||
           focusState.textInputFocusedNode() != UI_INVALID_HANDLE ||
           focusState.navigationFocusedNode(UI_PRIMARY_INPUT_DEVICE) != UI_INVALID_HANDLE ||
           modalState.hasModal();
}

UiInputFrame UiSurface::toLocalInput(const UiInputFrame& input) const
{
    UiInputFrame local = input;
    const UiRect& r = surfaceDesc.rect;
    if (r.width > 0.0f)
        local.pointerX = (input.pointerX - r.x) / r.width;
    if (r.height > 0.0f)
        local.pointerY = (input.pointerY - r.y) / r.height;
    return local;
}

void UiSurface::setTheme(const UiTheme& theme)
{
    themeState = theme;
}
