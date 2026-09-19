#pragma once

#include "RendererExecutionInputs.h"

#include "ActiveCameraViewSystem.hpp"
#include "CameraBindingLifecycle.h"
#include "CameraBindingSystem.hpp"
#include "CameraDescriptionComponent.h"
#include "CameraGpuComponent.h"
#include "CameraGpuSystem.hpp"
#include "CameraLifecycleSystem.hpp"
#include "DefaultCameraControlSystem.hpp"
#include "ECSWorld.hpp"
#include "IResourceProvider.hpp"

namespace gts::rendering
{
    inline void resetRendererCameraSceneFeature(ECSWorld& world)
    {
        resetCameraBindingLifecycleState(world);
    }

    inline void installRendererCameraSceneFeature(ECSWorld&                      world,
                                                  IResourceProvider*             resources,
                                                  const RendererExecutionInputs& execution)
    {
        if (!world.hasConfiguredDefaultExecutionSelection())
            world.configureDefaultExecutionSelection(execution.defaultSelection);
        world.registerRemoveCallback<CameraGpuComponent>(
            [resources](ECSWorld&, Entity, CameraGpuComponent& cameraGpu)
            {
                if (cameraGpu.viewID == 0 || resources == nullptr)
                    return;

                resources->releaseCameraBuffer(cameraGpu.viewID);
                cameraGpu.viewID = 0;
            });
        world.registerAddCallback<CameraDescriptionComponent>(
            [](ECSWorld& world, Entity entity, CameraDescriptionComponent&)
            {
                queueCameraRefresh(world, entity);
            });
        world.registerRemoveCallback<CameraDescriptionComponent>(
            [](ECSWorld& world, Entity entity, CameraDescriptionComponent&)
            {
                queueCameraCleanup(world, entity);
            });

        world.addControllerSystem<CameraLifecycleSystem>(execution.camera);
        world.addControllerSystem<DefaultCameraControlSystem>(execution.camera);
        world.addControllerSystem<CameraGpuSystem>(execution.camera);
        world.addControllerSystem<CameraBindingSystem>(execution.camera);
        world.addControllerSystem<ActiveCameraViewSystem>(execution.camera);

        world.forEachSnapshot<CameraDescriptionComponent>(
            [&world](Entity entity, CameraDescriptionComponent&)
            {
                queueCameraRefresh(world, entity);
            });
    }
}
