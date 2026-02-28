#pragma once
#include <memory>
#include "ScriptBehaviour.hpp"

// a script component only holds a ptr to a certain script the user can attach to any given entity
struct ScriptComponent 
{
    std::shared_ptr<ScriptBehaviour> script;
};
