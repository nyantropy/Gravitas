#pragma once
#include "ECSSystem.hpp"
#include "ScriptComponent.h"

// the script system runs update on every script component in the world
class ScriptSystem : public ECSSystem 
{
    public:
        void update(ECSWorld& world, float dt) override 
        {
            for (Entity e : world.getAllEntitiesWith<ScriptComponent>()) 
            {
                auto& script = world.getComponent<ScriptComponent>(e);
                if (script.script) 
                {
                    script.script->onUpdate(world, dt);
                }
            }
        }
};
