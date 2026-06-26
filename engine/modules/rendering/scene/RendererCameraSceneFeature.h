#pragma once

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

        world.addControllerSystem<CameraLifecycleSystem>(EcsSystemGroup::Camera);
        world.addControllerSystem<DefaultCameraControlSystem>(EcsSystemGroup::Camera);
        world.addControllerSystem<CameraGpuSystem>(EcsSystemGroup::Camera);
        world.addControllerSystem<CameraBindingSystem>(EcsSystemGroup::Camera);
        world.addControllerSystem<ActiveCameraViewSystem>(EcsSystemGroup::Camera);

        world.forEachSnapshot<CameraDescriptionComponent>(
            [&world](Entity entity, CameraDescriptionComponent&)
            {
                queueCameraRefresh(world, entity);
            });
    }
}
