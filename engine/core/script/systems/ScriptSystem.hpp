#pragma once
#include "ECSSimulationSystem.hpp"
#include "ScriptComponent.h"

// the script system runs update on every script component in the world
class ScriptSystem : public ECSSimulationSystem
{
public:
    void update(const EcsSimulationContext& ctx) override
    {
        for (Entity e : ctx.world.getAllEntitiesWith<ScriptComponent>())
        {
            auto& script = ctx.world.getComponent<ScriptComponent>(e);
            if (script.script)
                script.script->onUpdate(ctx.world, ctx.dt);
        }
    }
};
