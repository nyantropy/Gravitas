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
    gts::tools::EngineToolRuntime tools({gts::execution::groups::Tools, gts::execution::rendererExecutionInputs()});
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
    std::string sceneName = "replacement";
    ctx.activeSceneName   = &sceneName;
    world.updateControllers(ctx);
    tools.update(ctx);
    const auto& recreated = world.getLastControllerTimingSamples();
    if (recreated.size() != 7)
        throw std::runtime_error("Tool recreation must preserve controller count");
    for (size_t i = 0; i < recreated.size(); ++i)
        if (!recreated[i].name.ends_with(names[i]) || recreated[i].group != gts::execution::groups::Tools ||
            recreated[i].instanceIndex != 0)
            throw std::runtime_error("Tool recreation must preserve execution inputs");
    tools.shutdown();

    const auto                              customGroup = static_cast<EcsSystemGroup>(1ull << 48);
    gts::rendering::RendererExecutionInputs preview{
        {"custom-preview", toMask(customGroup)}, customGroup, customGroup, customGroup, customGroup};
    gts::tools::EngineToolRuntime customTools({customGroup, preview});
    preview.defaultSelection.id = "changed-after-construction";
    for (const char* name : {"first", "second", "first"})
    {
        sceneName = name;
        world.updateControllers(ctx);
        customTools.update(ctx);
        const auto& customSamples = world.getLastControllerTimingSamples();
        if (customSamples.size() != 7)
            throw std::runtime_error("Custom tool policy lost controllers during reset");
        for (size_t i = 0; i < customSamples.size(); ++i)
            if (customSamples[i].group != customGroup || customSamples[i].instanceIndex != 0 ||
                !customSamples[i].name.ends_with(names[i]))
                throw std::runtime_error("Tool reset must preserve supplied timing identity");
    }
    customTools.shutdown();
}
