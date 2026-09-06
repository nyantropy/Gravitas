#pragma once

#include "InputBindingRegistry.h"
#include "GtsKey.h"

namespace gts::input::defaults::camera
{
    inline void install(InputBindingRegistry& registry)
    {
        const InputBinding bindings[]{
            InputBinding{"engine.zoom_in",
                         InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::ArrowUp)},
                         ActivationMode::Held,
                         "",
                         PausePolicy::AlwaysActive},
            InputBinding{"engine.zoom_out",
                         InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::ArrowDown)},
                         ActivationMode::Held,
                         "",
                         PausePolicy::AlwaysActive},
            InputBinding{"engine.orbit_left",
                         InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::ArrowLeft)},
                         ActivationMode::Held,
                         "",
                         PausePolicy::AlwaysActive},
            InputBinding{"engine.orbit_right",
                         InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::ArrowRight)},
                         ActivationMode::Held,
                         "",
                         PausePolicy::AlwaysActive},
        };
        registry.bindDefaults(bindings);
    }
}
