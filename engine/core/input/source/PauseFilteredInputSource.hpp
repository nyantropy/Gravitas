#pragma once

#include "IInputSource.hpp"

// Wraps any IInputSource and gates all key queries to false when the
// simulation is paused.
//
// Purpose: systems that drive scene simulation
// call InputActionManager::update(*ctx.inputSource), so filtering here
// prevents held keys from being seen by simulation-coupled controllers
// while the simulation is paused.
//
// Systems that use ctx.actions (the engine's InputActionManager<GtsAction>,
// updated directly from the raw InputManager) are NOT affected — camera
// controls, UI, and the pause toggle itself continue to respond normally.
class PauseFilteredInputSource : public IInputSource
{
    const IInputSource* inner = nullptr;
    bool paused = false;

public:
    PauseFilteredInputSource() = default;

    // Called once after the concrete IInputSource is constructed.
    void setSource(const IInputSource& src) { inner = &src; }

    void setSimulationPaused(bool p) { paused = p; }

    bool isKeyDown(GtsKey key)     const override { return inner && !paused && inner->isKeyDown(key);     }
    bool isKeyPressed(GtsKey key)  const override { return inner && !paused && inner->isKeyPressed(key);  }
    bool isKeyReleased(GtsKey key) const override { return inner && !paused && inner->isKeyReleased(key); }
};
