#pragma once

#include "ECSWorld.hpp"
#include "AiComponent.h"
#include "TetrisAiSystem.h"

// Call once from TetrisScene::onLoad, after TetrisInputSystem and TetrisGameSystem
// have both been registered.  TetrisAiSystem is inserted as the second controller
// so it executes right after TetrisInputSystem each frame.
inline void RegisterAi(ECSWorld& world, TetrisGameSystem& gameSystem)
{
    world.createSingleton<AiComponent>();
    world.addControllerSystem<TetrisAiSystem>(gameSystem);
}
