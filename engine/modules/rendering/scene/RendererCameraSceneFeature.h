#pragma once

#include "BuiltinExecutionGroups.h"
#include "SceneExecutionPolicy.h"

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

    inline void installRendererCameraSceneFeature(ECSWorld& world, IResourceProvider* resources)
    {
        gts::execution::ensureExecutionPolicy(world);
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

        world.addControllerSystem<CameraLifecycleSystem>(gts::execution::groups::Camera);
        world.addControllerSystem<DefaultCameraControlSystem>(gts::execution::groups::Camera);
        world.addControllerSystem<CameraGpuSystem>(gts::execution::groups::Camera);
        world.addControllerSystem<CameraBindingSystem>(gts::execution::groups::Camera);
        world.addControllerSystem<ActiveCameraViewSystem>(gts::execution::groups::Camera);

        world.forEachSnapshot<CameraDescriptionComponent>(
            [&world](Entity entity, CameraDescriptionComponent&)
            {
                queueCameraRefresh(world, entity);
            });
    }
}
