#pragma once
#include "GtsKey.h"

// Abstract frame-based digital input source.
// InputBindingRegistry reads this interface rather than the concrete
// InputManager, keeping the action layer decoupled from the device layer.
class IInputSource
{
public:
    virtual ~IInputSource() = default;

    virtual bool isKeyDown(GtsKey key) const = 0;
    virtual bool isKeyPressed(GtsKey key) const = 0;
    virtual bool isKeyReleased(GtsKey key) const = 0;
};
