#pragma once

#include "InputManager.hpp"
#include "GtsKey.h"

// expose input manager in a global manner, to make things easier
// currently not used, but i will leave it here for now in case i need to rethink my architecture
namespace gtsinput
{
    inline InputManager* glbInput = nullptr;

    inline void SetInputManager(InputManager* mgr)
    {
        glbInput = mgr;
    }

    inline InputManager* getInputManager()
    {
        return glbInput;
    }

    // query functions
    inline bool isKeyDown(GtsKey key) 
    {
        if (!glbInput) return false;
        return glbInput->isKeyDown(key);
    }

    inline bool isKeyPressed(GtsKey key) 
    {
        if (!glbInput) return false;
        return glbInput->isKeyPressed(key);
    }

    inline bool isKeyReleased(GtsKey key) 
    {
        if (!glbInput) return false;
        return glbInput->isKeyReleased(key);
    }
}
