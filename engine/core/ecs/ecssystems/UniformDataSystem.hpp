#pragma once
#include "ECSSystem.hpp"
#include "UniformBufferComponent.h"
#include "TransformComponent.h"
#include "CameraComponent.h"
#include "UniformBufferObject.h"

// update the uniform buffer object
class UniformDataSystem : public ECSSystem
{
    public:
        void update(ECSWorld& world, float dt) override
        {
            // get the main camera, or rather any camera right now, needs rework later
            Entity camera;
            for (Entity e : world.getAllEntitiesWith<CameraComponent, TransformComponent>())
                camera = e;

            auto& camComp = world.getComponent<CameraComponent>(camera);

            // update for every object in the ecs world
            for (Entity e : world.getAllEntitiesWith<UniformBufferComponent, TransformComponent>())
            {
                auto& uboComp = world.getComponent<UniformBufferComponent>(e);
                auto& transform = world.getComponent<TransformComponent>(e);

                uboComp.uniformBufferObject.model = transform.getModelMatrix();
                uboComp.uniformBufferObject.view  = camComp.view;
                uboComp.uniformBufferObject.proj  = camComp.projection;
            }
        }
};