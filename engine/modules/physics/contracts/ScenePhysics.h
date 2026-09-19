#pragma once

#include "GtsScene.hpp"
#include "IGtsPhysicsModule.h"

namespace gts::physics
{
    namespace detail
    {
        // Installed alongside the scene-owned implementation. This binding only
        // borrows it; destruction never dereferences or deletes the implementation.
        struct ScenePhysicsBinding
        {
            IGtsPhysicsModule& physics;
        };
    } // namespace detail

    // Borrowed access until scene reset/destruction; lookup never installs physics.
    inline IGtsPhysicsModule* findScenePhysics(GtsScene& scene)
    {
        auto* binding = scene.findSceneResource<detail::ScenePhysicsBinding>();
        return binding != nullptr ? &binding->physics : nullptr;
    }

    inline const IGtsPhysicsModule* findScenePhysics(const GtsScene& scene)
    {
        const auto* binding = scene.findSceneResource<detail::ScenePhysicsBinding>();
        return binding != nullptr ? &binding->physics : nullptr;
    }

    inline IGtsPhysicsModule& requireScenePhysics(GtsScene& scene)
    {
        return scene.requireSceneResource<detail::ScenePhysicsBinding>().physics;
    }

    inline const IGtsPhysicsModule& requireScenePhysics(const GtsScene& scene)
    {
        return scene.requireSceneResource<detail::ScenePhysicsBinding>().physics;
    }
} // namespace gts::physics
