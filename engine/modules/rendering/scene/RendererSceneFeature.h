#pragma once

#include "RenderingControllerContext.h"
#include "EcsControllerContext.hpp"
#include "GtsScene.hpp"
#include "RendererCameraSceneFeature.h"
#include "RendererGeometrySceneFeature.h"
#include "RendererParticleSceneFeature.h"
#include "TransformSceneFeature.h"

namespace gts::rendering
{
    inline void installRendererFeature(GtsScene& scene, const EcsControllerContext& ctx)
    {
        gts::transform::installTransformFeature(scene);

        if (!scene.markSceneFeatureInstalled("renderer"))
            return;

        ECSWorld& world = scene.getWorld();
        scene.registerSceneResetHook(
            [](ECSWorld& world)
            {
                resetRendererGeometrySceneFeature(world);
                resetRendererCameraSceneFeature(world);
            });

        installRendererGeometrySceneFeature(world, gts::rendering::controllerContext(ctx).resources);
        installRendererCameraSceneFeature(world, gts::rendering::controllerContext(ctx).resources);
        installRendererParticleSceneFeature(world);
    }
}
