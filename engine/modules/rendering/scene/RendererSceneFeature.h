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
    inline void
    installRendererFeature(GtsScene& scene, const EcsControllerContext& ctx, const RendererExecutionInputs& execution)
    {
        gts::transform::installTransformFeature(scene, execution.defaultSelection, execution.preparation);

        if (!scene.markSceneFeatureInstalled("renderer"))
            return;

        ECSWorld& world = scene.getWorld();
        installRendererGeometrySceneFeature(world, gts::rendering::controllerContext(ctx).resources, execution);
        installRendererCameraSceneFeature(world, gts::rendering::controllerContext(ctx).resources, execution);
        installRendererParticleSceneFeature(world, execution);
    }
}
