#include "SceneExecutionPolicy.h"
#include "GtsGameLoop.h"
#include "ECSWorld.hpp"
#include <cmath>
#include <stdexcept>

void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
int main()
{
    EngineConfig config;
    config.simulation.tickRate = 10;
    GtsGameLoop loop;
    loop.init(config);
    require(loop.advance(0.05f) == 0 && std::abs(loop.alpha() - 0.5f) < 0.001f, "Accumulator remainder");
    ECSWorld world;
    gts::execution::ensureExecutionPolicy(world);
    world.pushExecutionSelection(SceneExecutionProfile::pauseMenu());
    require(loop.advance(0.05f) == 1 && loop.simulationDt() == 0.1f, "Execution profile must not pause engine clock");
    loop.advance(0.05f);
    loop.paused = true;
    require(loop.advance(10.0f) == 0 && loop.simulationDt() == 0 && std::abs(loop.alpha() - 0.5f) < 0.001f,
            "Engine pause must stop ticks without accumulating paused time");
    require(world.getCurrentExecutionSelection().id == "pause_menu", "Engine pause must not change profile");
    loop.paused = false;
    require(loop.advance(0.05f) == 1 && loop.simulationDt() == 0.1f, "Resume retains accumulator remainder");
}
