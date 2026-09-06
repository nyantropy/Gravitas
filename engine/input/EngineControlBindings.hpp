#pragma once

#include "InputBindingRegistry.h"
#include "GtsKey.h"

namespace gts::input::defaults::engine
{
    inline void install(InputBindingRegistry& registry)
    {
        const InputBinding bindings[]{
            InputBinding{"engine.pause",
                         InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::X)},
                         ActivationMode::Pressed,
                         "",
                         PausePolicy::AlwaysActive},
            InputBinding{"engine.close",
                         InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::Escape)},
                         ActivationMode::Pressed,
                         "",
                         PausePolicy::AlwaysActive},
            InputBinding{"engine.debug_overlay",
                         InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::F3)},
                         ActivationMode::Pressed,
                         "",
                         PausePolicy::AlwaysActive},
            InputBinding{"engine.debug_overlay_page",
                         InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::Tab)},
                         ActivationMode::Pressed,
                         "",
                         PausePolicy::AlwaysActive},
            InputBinding{"engine.screenshot",
                         InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::F12)},
                         ActivationMode::Pressed,
                         "",
                         PausePolicy::AlwaysActive},
        };
        registry.bindDefaults(bindings);
    }
}
