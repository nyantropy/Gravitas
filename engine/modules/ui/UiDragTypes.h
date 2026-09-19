#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "UiHandle.h"

enum class UiDragState : uint8_t
{
    Idle = 0,
    Armed,
    Dragging
};

struct UiDragPayload
{
    std::string type;
    uint64_t id = 0;
    std::string label;
    std::shared_ptr<void> object;

    bool empty() const
    {
        return type.empty() && id == 0 && !object;
    }

    explicit operator bool() const
    {
        return !empty();
    }

    template<typename T>
    std::shared_ptr<T> as() const
    {
        return std::static_pointer_cast<T>(object);
    }
};

struct UiDragSourceDesc
{
    bool enabled = true;
    UiDragPayload payload;
    float startThreshold = 0.004f;
    bool capturePointer = true;
};

struct UiDropTargetDesc
{
    bool enabled = true;
    bool acceptsAnyPayload = false;
    std::vector<std::string> acceptedPayloadTypes;
};

struct UiDragFrameResult
{
    bool active = false;
    bool armed = false;
    bool dragging = false;
    bool started = false;
    bool moved = false;
    bool targetChanged = false;
    bool enteredTarget = false;
    bool leftTarget = false;
    bool dropped = false;
    bool accepted = false;
    bool rejected = false;
    bool cancelled = false;
    bool ended = false;

    uint32_t pointerId = 1;
    UiHandle source = UI_INVALID_HANDLE;
    UiHandle target = UI_INVALID_HANDLE;
    UiHandle previousTarget = UI_INVALID_HANDLE;
    UiDragPayload payload;

    float startX = 0.0f;
    float startY = 0.0f;
    float pointerX = 0.0f;
    float pointerY = 0.0f;
    float deltaX = 0.0f;
    float deltaY = 0.0f;
};
