#pragma once
#include "Entity.h"
#include "ECSWorld.hpp"

// idea: introduce the ability to attach scripts to any given entity, and provide a few overridables
class ScriptBehaviour 
{
    public:
        Entity owner;

        virtual ~ScriptBehaviour() = default;

        virtual void onCreate(ECSWorld& world) {}
        virtual void onUpdate(ECSWorld& world, float dt) {}
        virtual void onDestroy(ECSWorld& world) {}
};
