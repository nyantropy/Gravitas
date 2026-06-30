#include "UiFocusManager.h"

#include "UiNode.h"

UiFocusManager::UiFocusManager()
{
    clear();
}

void UiFocusManager::clear()
{
    pointers.clear();
    navigationFocus.clear();
    scopes.clear();
    scopeStack.clear();
    restorationStack.clear();

    keyboardFocus      = UI_INVALID_HANDLE;
    keyboardFocusScope = UI_INVALID_FOCUS_SCOPE;
    textInputFocus     = UI_INVALID_HANDLE;
    nextScopeId        = UI_ROOT_FOCUS_SCOPE + 1;

    UiFocusScope root;
    root.id = UI_ROOT_FOCUS_SCOPE;
    scopes.emplace(root.id, root);
    scopeStack.push_back(root.id);
}

void UiFocusManager::clear(UiDocument& document)
{
    for (const auto& [_, pointer] : pointers)
    {
        reflectHovered(document, pointer.hovered, false);
        reflectPressed(document, pointer.active, false);
    }

    reflectFocused(document, keyboardFocus, false);
    clear();
}

UiFocusScopeId UiFocusManager::pushScope(UiHandle owner)
{
    UiFocusScope scope;
    scope.id           = nextScopeId++;
    scope.parent       = activeScope();
    scope.owner        = owner;
    scope.restoreFocus = keyboardFocus;
    scope.restoreScope = keyboardFocusScope;
    scope.lastFocused  = keyboardFocus;

    scopes.emplace(scope.id, scope);
    scopeStack.push_back(scope.id);
    return scope.id;
}

bool UiFocusManager::popScope(UiDocument& document, UiFocusScopeId scopeId)
{
    if (scopeId == UI_INVALID_FOCUS_SCOPE || scopeId == UI_ROOT_FOCUS_SCOPE || scopeStack.empty() ||
        scopeStack.back() != scopeId)
    {
        return false;
    }

    const UiFocusScope scope = scopes[scopeId];
    scopeStack.pop_back();
    scopes.erase(scopeId);

    if (keyboardFocusScope != scopeId && keyboardFocus != UI_INVALID_HANDLE)
        return true;

    const UiFocusScopeId restoreScope = isScopeAlive(scope.restoreScope) ? scope.restoreScope : activeScope();
    if (scope.restoreFocus != UI_INVALID_HANDLE && isNodeAvailable(document, scope.restoreFocus))
        return setKeyboardFocus(document, scope.restoreFocus, UiFocusReason::ScopePopped, false, restoreScope);

    if (restoreFocus(document))
        return true;

    return clearFocus(document, UiFocusReason::ScopePopped);
}

UiFocusScopeId UiFocusManager::activeScope() const
{
    return scopeStack.empty() ? UI_INVALID_FOCUS_SCOPE : scopeStack.back();
}

const UiFocusScope* UiFocusManager::findScope(UiFocusScopeId scopeId) const
{
    const auto it = scopes.find(scopeId);
    return it == scopes.end() ? nullptr : &it->second;
}

bool UiFocusManager::requestFocus(UiDocument& document, UiHandle handle, UiFocusReason reason)
{
    if (handle == UI_INVALID_HANDLE)
        return clearFocus(document, reason);

    if (!isNodeAvailable(document, handle))
        return false;

    return setKeyboardFocus(document, handle, reason, true, activeScope());
}

bool UiFocusManager::clearFocus(UiDocument& document, UiFocusReason /*reason*/)
{
    reflectFocused(document, keyboardFocus, false);
    keyboardFocus      = UI_INVALID_HANDLE;
    keyboardFocusScope = UI_INVALID_FOCUS_SCOPE;
    restorationStack.clear();
    return true;
}

bool UiFocusManager::restoreFocus(UiDocument& document)
{
    while (!restorationStack.empty())
    {
        const UiFocusRestoreEntry restore = restorationStack.back();
        restorationStack.pop_back();

        if (restore.handle == UI_INVALID_HANDLE || !isNodeAvailable(document, restore.handle))
            continue;

        const UiFocusScopeId scopeId = isScopeAlive(restore.scope) ? restore.scope : activeScope();
        return setKeyboardFocus(document, restore.handle, UiFocusReason::Restore, false, scopeId);
    }

    reflectFocused(document, keyboardFocus, false);
    keyboardFocus      = UI_INVALID_HANDLE;
    keyboardFocusScope = UI_INVALID_FOCUS_SCOPE;
    return false;
}

bool UiFocusManager::requestTextInputFocus(UiDocument& document, UiHandle handle, UiFocusReason /*reason*/)
{
    if (handle == UI_INVALID_HANDLE)
        return clearTextInputFocus(document);

    if (!isNodeAvailable(document, handle))
        return false;

    textInputFocus = handle;
    return true;
}

bool UiFocusManager::clearTextInputFocus(UiDocument& /*document*/, UiFocusReason /*reason*/)
{
    textInputFocus = UI_INVALID_HANDLE;
    return true;
}

bool UiFocusManager::setNavigationFocus(UiDocument&     document,
                                        UiInputDeviceId deviceId,
                                        UiHandle        handle,
                                        UiFocusReason /*reason*/)
{
    if (deviceId == 0)
        return false;

    if (handle == UI_INVALID_HANDLE)
        return clearNavigationFocus(deviceId);

    if (!isNodeAvailable(document, handle))
        return false;

    navigationFocus[deviceId] = handle;
    return true;
}

bool UiFocusManager::clearNavigationFocus(UiInputDeviceId deviceId)
{
    return navigationFocus.erase(deviceId) > 0;
}

UiHandle UiFocusManager::navigationFocusedNode(UiInputDeviceId deviceId) const
{
    const auto it = navigationFocus.find(deviceId);
    return it == navigationFocus.end() ? UI_INVALID_HANDLE : it->second;
}

bool UiFocusManager::setHovered(UiDocument& document, UiPointerId pointerId, UiHandle handle)
{
    UiPointerFocusState& state = pointerState(pointerId);
    const UiHandle       next =
        (handle != UI_INVALID_HANDLE && isNodeAvailable(document, handle)) ? handle : UI_INVALID_HANDLE;

    if (state.hovered == next)
        return true;

    const UiHandle previous = state.hovered;
    state.hovered           = next;

    reflectHovered(document, previous, anyPointerHovered(previous));
    reflectHovered(document, next, next != UI_INVALID_HANDLE);
    return true;
}

UiHandle UiFocusManager::hoveredNode(UiPointerId pointerId) const
{
    const UiPointerFocusState* state = findPointerState(pointerId);
    return state == nullptr ? UI_INVALID_HANDLE : state->hovered;
}

bool UiFocusManager::capturePointer(UiDocument& document, UiPointerId pointerId, UiHandle handle)
{
    if (handle == UI_INVALID_HANDLE || !isNodeAvailable(document, handle))
        return false;

    pointerState(pointerId).captured = handle;
    return true;
}

bool UiFocusManager::releasePointer(UiPointerId pointerId)
{
    const auto it = pointers.find(pointerId);
    if (it == pointers.end() || it->second.captured == UI_INVALID_HANDLE)
        return false;

    it->second.captured = UI_INVALID_HANDLE;
    return true;
}

UiHandle UiFocusManager::capturedNode(UiPointerId pointerId) const
{
    const UiPointerFocusState* state = findPointerState(pointerId);
    return state == nullptr ? UI_INVALID_HANDLE : state->captured;
}

bool UiFocusManager::setActivePointer(UiDocument& document, UiPointerId pointerId, UiHandle handle)
{
    if (handle == UI_INVALID_HANDLE)
        return clearActivePointer(document, pointerId);

    if (!isNodeAvailable(document, handle))
        return false;

    UiPointerFocusState& state = pointerState(pointerId);
    if (state.active == handle)
        return true;

    const UiHandle previous = state.active;
    state.active            = handle;

    reflectPressed(document, previous, anyPointerActive(previous));
    reflectPressed(document, handle, true);
    return true;
}

bool UiFocusManager::clearActivePointer(UiDocument& document, UiPointerId pointerId)
{
    UiPointerFocusState& state = pointerState(pointerId);
    if (state.active == UI_INVALID_HANDLE)
        return false;

    const UiHandle previous = state.active;
    state.active            = UI_INVALID_HANDLE;
    reflectPressed(document, previous, anyPointerActive(previous));
    return true;
}

UiHandle UiFocusManager::activePointerNode(UiPointerId pointerId) const
{
    const UiPointerFocusState* state = findPointerState(pointerId);
    return state == nullptr ? UI_INVALID_HANDLE : state->active;
}

void UiFocusManager::clearPointer(UiDocument& document, UiPointerId pointerId)
{
    UiPointerFocusState& state   = pointerState(pointerId);
    const UiHandle       hovered = state.hovered;
    const UiHandle       active  = state.active;

    state.hovered  = UI_INVALID_HANDLE;
    state.captured = UI_INVALID_HANDLE;
    state.active   = UI_INVALID_HANDLE;

    reflectHovered(document, hovered, anyPointerHovered(hovered));
    reflectPressed(document, active, anyPointerActive(active));
}

void UiFocusManager::pruneInvalidHandles(UiDocument& document)
{
    for (auto& [_, pointer] : pointers)
    {
        if (pointer.hovered != UI_INVALID_HANDLE && !isNodeAvailable(document, pointer.hovered))
        {
            const UiHandle previous = pointer.hovered;
            pointer.hovered         = UI_INVALID_HANDLE;
            reflectHovered(document, previous, anyPointerHovered(previous));
        }

        if (pointer.captured != UI_INVALID_HANDLE && !isNodeAvailable(document, pointer.captured))
            pointer.captured = UI_INVALID_HANDLE;

        if (pointer.active != UI_INVALID_HANDLE && !isNodeAvailable(document, pointer.active))
        {
            const UiHandle previous = pointer.active;
            pointer.active          = UI_INVALID_HANDLE;
            reflectPressed(document, previous, anyPointerActive(previous));
        }
    }

    if (keyboardFocus != UI_INVALID_HANDLE && !isNodeAvailable(document, keyboardFocus))
    {
        reflectFocused(document, keyboardFocus, false);
        keyboardFocus      = UI_INVALID_HANDLE;
        keyboardFocusScope = UI_INVALID_FOCUS_SCOPE;
        restoreFocus(document);
    }

    if (textInputFocus != UI_INVALID_HANDLE && !isNodeAvailable(document, textInputFocus))
        textInputFocus = UI_INVALID_HANDLE;

    for (auto it = navigationFocus.begin(); it != navigationFocus.end();)
    {
        if (it->second != UI_INVALID_HANDLE && !isNodeAvailable(document, it->second))
            it = navigationFocus.erase(it);
        else
            ++it;
    }
}

UiPointerFocusState& UiFocusManager::pointerState(UiPointerId pointerId)
{
    return pointers[pointerId];
}

const UiPointerFocusState* UiFocusManager::findPointerState(UiPointerId pointerId) const
{
    const auto it = pointers.find(pointerId);
    return it == pointers.end() ? nullptr : &it->second;
}

bool UiFocusManager::setKeyboardFocus(
    UiDocument& document, UiHandle handle, UiFocusReason /*reason*/, bool pushPrevious, UiFocusScopeId scopeId)
{
    if (handle == UI_INVALID_HANDLE)
        return clearFocus(document);

    if (!isNodeAvailable(document, handle))
        return false;

    const UiFocusScopeId resolvedScope = isScopeAlive(scopeId) ? scopeId : activeScope();
    if (keyboardFocus == handle)
    {
        keyboardFocusScope = resolvedScope;
        if (auto* scope = scopes.contains(resolvedScope) ? &scopes[resolvedScope] : nullptr)
            scope->lastFocused = handle;
        reflectFocused(document, handle, true);
        return true;
    }

    if (pushPrevious && keyboardFocus != UI_INVALID_HANDLE)
        restorationStack.push_back(UiFocusRestoreEntry{keyboardFocus, keyboardFocusScope});

    const UiHandle previous = keyboardFocus;
    keyboardFocus           = handle;
    keyboardFocusScope      = resolvedScope;

    reflectFocused(document, previous, false);
    reflectFocused(document, handle, true);

    if (auto* scope = scopes.contains(resolvedScope) ? &scopes[resolvedScope] : nullptr)
        scope->lastFocused = handle;

    return true;
}

void UiFocusManager::reflectHovered(UiDocument& document, UiHandle handle, bool hovered)
{
    if (handle == UI_INVALID_HANDLE)
        return;

    UiNode* node = document.findNode(handle);
    if (node == nullptr || node->state.hovered == hovered)
        return;

    UiStateFlags state = node->state;
    state.hovered      = hovered;
    document.setState(handle, state);
}

void UiFocusManager::reflectPressed(UiDocument& document, UiHandle handle, bool pressed)
{
    if (handle == UI_INVALID_HANDLE)
        return;

    UiNode* node = document.findNode(handle);
    if (node == nullptr || node->state.pressed == pressed)
        return;

    UiStateFlags state = node->state;
    state.pressed      = pressed;
    document.setState(handle, state);
}

void UiFocusManager::reflectFocused(UiDocument& document, UiHandle handle, bool focused)
{
    if (handle == UI_INVALID_HANDLE)
        return;

    UiNode* node = document.findNode(handle);
    if (node == nullptr || node->state.focused == focused)
        return;

    UiStateFlags state = node->state;
    state.focused      = focused;
    document.setState(handle, state);
}

bool UiFocusManager::anyPointerHovered(UiHandle handle) const
{
    if (handle == UI_INVALID_HANDLE)
        return false;

    for (const auto& [_, pointer] : pointers)
    {
        if (pointer.hovered == handle)
            return true;
    }

    return false;
}

bool UiFocusManager::anyPointerActive(UiHandle handle) const
{
    if (handle == UI_INVALID_HANDLE)
        return false;

    for (const auto& [_, pointer] : pointers)
    {
        if (pointer.active == handle)
            return true;
    }

    return false;
}

bool UiFocusManager::isNodeAvailable(const UiDocument& document, UiHandle handle) const
{
    const UiNode* node = document.findNode(handle);
    while (node != nullptr)
    {
        if (!node->state.visible || !node->state.enabled)
            return false;

        if (node->parent == UI_INVALID_HANDLE)
            return true;

        node = document.findNode(node->parent);
    }

    return false;
}

bool UiFocusManager::isScopeAlive(UiFocusScopeId scopeId) const
{
    return scopeId != UI_INVALID_FOCUS_SCOPE && scopes.find(scopeId) != scopes.end();
}
