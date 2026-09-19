#pragma once

#include "ECSWorld.hpp"
#include "SceneExecutionProfile.h"
#include "RendererExecutionInputs.h"

namespace gts::execution
{
    // Establish the runtime default once, without replacing active selections or
    // an explicitly supplied default. World clear restores this same default.
    inline void ensureExecutionPolicy(ECSWorld& world)
    {
        if (!world.hasConfiguredDefaultExecutionSelection())
            world.configureDefaultExecutionSelection(SceneExecutionProfile::gameplay());
    }

    inline const SceneExecutionPolicy& sceneExecutionPolicy(const ECSWorld& world)
    {
        const auto*                       policy = world.getCurrentExecutionSelection().policy<SceneExecutionPolicy>();
        static const SceneExecutionPolicy defaultPolicy;
        return policy != nullptr ? *policy : defaultPolicy;
    }
    inline FrameBuildMode selectFrameBuildMode(const EcsExecutionSelection& selection)
    {
        const auto* policy = selection.policy<SceneExecutionPolicy>();
        return policy != nullptr ? policy->frameBuildMode : FrameBuildMode::FullWorld;
    }

    inline gts::rendering::RendererExecutionInputs rendererExecutionInputs()
    {
        return {SceneExecutionProfile::gameplay(), groups::RenderPrep, groups::Animation,
                groups::Camera, groups::Particles};
    }
} // namespace gts::execution
