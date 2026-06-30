#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "UiDocument.h"
#include "UiInteraction.h"

using UiFocusScopeId = uint32_t;

static constexpr UiFocusScopeId UI_INVALID_FOCUS_SCOPE = 0;
static constexpr UiFocusScopeId UI_ROOT_FOCUS_SCOPE    = 1;

enum class UiFocusReason : uint8_t
{
    Programmatic = 0,
    Pointer,
    Keyboard,
    Navigation,
    Restore,
    Clear,
    NodeRemoved,
    ScopePushed,
    ScopePopped
};

struct UiPointerFocusState
{
    UiHandle hovered  = UI_INVALID_HANDLE;
    UiHandle captured = UI_INVALID_HANDLE;
    UiHandle active   = UI_INVALID_HANDLE;
};

struct UiFocusScope
{
    UiFocusScopeId id           = UI_INVALID_FOCUS_SCOPE;
    UiFocusScopeId parent       = UI_INVALID_FOCUS_SCOPE;
    UiHandle       owner        = UI_INVALID_HANDLE;
    UiHandle       restoreFocus = UI_INVALID_HANDLE;
    UiFocusScopeId restoreScope = UI_INVALID_FOCUS_SCOPE;
    UiHandle       lastFocused  = UI_INVALID_HANDLE;
};

class UiFocusManager
{
public:
    UiFocusManager();

    void clear();
    void clear(UiDocument& document);

    UiFocusScopeId      pushScope(UiHandle owner = UI_INVALID_HANDLE);
    bool                popScope(UiDocument& document, UiFocusScopeId scopeId);
    UiFocusScopeId      activeScope() const;
    const UiFocusScope* findScope(UiFocusScopeId scopeId) const;

    bool     requestFocus(UiDocument& document, UiHandle handle, UiFocusReason reason = UiFocusReason::Programmatic);
    bool     clearFocus(UiDocument& document, UiFocusReason reason = UiFocusReason::Clear);
    bool     restoreFocus(UiDocument& document);
    UiHandle focusedNode() const
    {
        return keyboardFocus;
    }
    UiHandle keyboardFocusedNode() const
    {
        return keyboardFocus;
    }

    bool
    requestTextInputFocus(UiDocument& document, UiHandle handle, UiFocusReason reason = UiFocusReason::Programmatic);
    bool     clearTextInputFocus(UiDocument& document, UiFocusReason reason = UiFocusReason::Clear);
    UiHandle textInputFocusedNode() const
    {
        return textInputFocus;
    }

    bool     setNavigationFocus(UiDocument&     document,
                                UiInputDeviceId deviceId,
                                UiHandle        handle,
                                UiFocusReason   reason = UiFocusReason::Navigation);
    bool     clearNavigationFocus(UiInputDeviceId deviceId);
    UiHandle navigationFocusedNode(UiInputDeviceId deviceId = UI_PRIMARY_INPUT_DEVICE) const;

    bool     setHovered(UiDocument& document, UiPointerId pointerId, UiHandle handle);
    UiHandle hoveredNode(UiPointerId pointerId = UI_PRIMARY_POINTER) const;

    bool     capturePointer(UiDocument& document, UiPointerId pointerId, UiHandle handle);
    bool     releasePointer(UiPointerId pointerId);
    UiHandle capturedNode(UiPointerId pointerId = UI_PRIMARY_POINTER) const;

    bool     setActivePointer(UiDocument& document, UiPointerId pointerId, UiHandle handle);
    bool     clearActivePointer(UiDocument& document, UiPointerId pointerId);
    UiHandle activePointerNode(UiPointerId pointerId = UI_PRIMARY_POINTER) const;

    void clearPointer(UiDocument& document, UiPointerId pointerId);
    void pruneInvalidHandles(UiDocument& document);

private:
    struct UiFocusRestoreEntry
    {
        UiHandle       handle = UI_INVALID_HANDLE;
        UiFocusScopeId scope  = UI_INVALID_FOCUS_SCOPE;
    };

    UiPointerFocusState&       pointerState(UiPointerId pointerId);
    const UiPointerFocusState* findPointerState(UiPointerId pointerId) const;

    bool setKeyboardFocus(
        UiDocument& document, UiHandle handle, UiFocusReason reason, bool pushPrevious, UiFocusScopeId scopeId);
    void reflectHovered(UiDocument& document, UiHandle handle, bool hovered);
    void reflectPressed(UiDocument& document, UiHandle handle, bool pressed);
    void reflectFocused(UiDocument& document, UiHandle handle, bool focused);
    bool anyPointerHovered(UiHandle handle) const;
    bool anyPointerActive(UiHandle handle) const;
    bool isNodeAvailable(const UiDocument& document, UiHandle handle) const;
    bool isScopeAlive(UiFocusScopeId scopeId) const;

    std::unordered_map<UiPointerId, UiPointerFocusState> pointers;
    std::unordered_map<UiInputDeviceId, UiHandle>        navigationFocus;
    std::unordered_map<UiFocusScopeId, UiFocusScope>     scopes;
    std::vector<UiFocusScopeId>                          scopeStack;
    std::vector<UiFocusRestoreEntry>                     restorationStack;

    UiHandle       keyboardFocus      = UI_INVALID_HANDLE;
    UiFocusScopeId keyboardFocusScope = UI_INVALID_FOCUS_SCOPE;
    UiHandle       textInputFocus     = UI_INVALID_HANDLE;
    UiFocusScopeId nextScopeId        = UI_ROOT_FOCUS_SCOPE + 1;
};
