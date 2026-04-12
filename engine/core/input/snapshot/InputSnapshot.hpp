#pragma once

#include "InputBinding.h"
#include "IInputSource.hpp"
#include "GtsKey.h"

// Read-only capture of input state sampled once per frame before the
// simulation tick loop begins. The same snapshot is passed to every
// simulation tick that frame, ensuring determinism across multiple ticks.
//
// No memory is copied — the snapshot holds a non-owning pointer to the
// underlying IInputSource, which is stable for the lifetime of a frame
// (input is polled once via platform.beginFrame() and does not change
// until the next frame).
//
// Constructing with source=nullptr is valid. All query methods return false
// when source is null, so simulation systems do not need to guard against a
// null snapshot pointer — they only need to handle the null-source case if
// they treat "no input" differently from "all keys up".
struct InputSnapshot
{
    const IInputSource* source = nullptr;
    ModifierFlags       modifiers = ModifierFlags::None;

    InputSnapshot() = default;

    explicit InputSnapshot(const IInputSource* source)
        : source(source)
        , modifiers(source != nullptr ? source->getModifiers() : ModifierFlags::None)
    {
    }

    bool isKeyDown(GtsKey key) const     { return source != nullptr && source->isKeyDown(key); }
    bool isKeyPressed(GtsKey key) const  { return source != nullptr && source->isKeyPressed(key); }
    bool isKeyReleased(GtsKey key) const { return source != nullptr && source->isKeyReleased(key); }
};
