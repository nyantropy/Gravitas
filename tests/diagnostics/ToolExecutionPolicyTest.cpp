#include "SceneExecutionPolicy.h"
#include "BuiltinExecutionGroups.h"
#include "EngineToolRuntime.hpp"
#include "ECSWorld.hpp"
#include <stdexcept>
#include <string_view>

int main()
{
    ECSWorld world;
    auto     none       = SceneExecutionProfile::gameplay();
    none.enabledSystems = 0;
    world.pushExecutionSelection(none);
    EcsControllerContext          ctx{world};
    gts::tools::EngineToolRuntime tools;
    world.updateControllers(ctx);
    tools.update(ctx);
    const auto& samples = world.getLastControllerTimingSamples();
    if (samples.size() != 7 || world.shouldExecuteGroup(gts::execution::groups::Tools))
        throw std::runtime_error("External tools must bypass world filtering");
    const char* names[] = {"EngineToolShellSystem",
                           "EngineToolCameraSystem",
                           "EngineGizmoSystem",
                           "EngineToolDebugDrawSystem",
                           "EngineToolWorldPickerSystem",
                           "EngineToolSelectionHighlightSystem",
                           "DebugDrawSystem"};
    for (size_t i = 0; i < samples.size(); ++i)
        if (!samples[i].name.ends_with(names[i]) || samples[i].group != gts::execution::groups::Tools ||
            samples[i].instanceIndex != 0)
            throw std::runtime_error("External tool order or timing identity changed");
    tools.shutdown();
}
