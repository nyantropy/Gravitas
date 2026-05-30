#pragma once
#include "InputBinding.h"
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
    virtual bool isMouseButtonDown(int button) const = 0;
    virtual bool isMouseButtonPressed(int button) const = 0;
    virtual bool isMouseButtonReleased(int button) const = 0;
    virtual double mouseX() const = 0;
    virtual double mouseY() const = 0;
    virtual double scrollX() const = 0;
    virtual double scrollY() const = 0;
    virtual ModifierFlags getModifiers() const = 0;
};
