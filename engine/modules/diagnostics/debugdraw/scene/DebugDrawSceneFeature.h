#pragma once


#include "DebugDrawSystem.hpp"
#include "EcsControllerContext.hpp"
#include "GtsScene.hpp"

namespace gts::debugdraw
{
    inline void installDebugDrawFeature(GtsScene& scene,
                                        const EcsControllerContext&,
                                        const EcsExecutionSelection& defaultSelection,
                                        EcsSystemGroup               drawingGroup)
    {
        if (!scene.markSceneFeatureInstalled("debugdraw"))
            return;

        if (!scene.getWorld().hasConfiguredDefaultExecutionSelection())
            scene.getWorld().configureDefaultExecutionSelection(defaultSelection);
        scene.getWorld().addControllerSystem<DebugDrawSystem>(drawingGroup);
    }
}
