#pragma once

#include "EcsControllerContext.hpp"
#include "GtsScene.hpp"
#include "RendererCameraSceneFeature.h"
#include "RendererGeometrySceneFeature.h"
#include "RendererParticleSceneFeature.h"

namespace gts::rendering
{
    inline void installRendererFeature(GtsScene& scene, const EcsControllerContext& ctx)
    {
        if (!scene.markSceneFeatureInstalled("renderer"))
            return;

        ECSWorld& world = scene.getWorld();
        scene.registerSceneResetHook(
            [](ECSWorld& world)
            {
                resetRendererGeometrySceneFeature(world);
                resetRendererCameraSceneFeature(world);
            });

        installRendererGeometrySceneFeature(world, ctx.resources);
        installRendererCameraSceneFeature(world, ctx.resources);
        installRendererParticleSceneFeature(world);
    }
}
