#include "TransformSceneFeature.h"
#include "ECSWorld.hpp"
#include "GtsFrameEndedEvent.h"

#include <type_traits>

#if __has_include("GravitasEngine.hpp") ||                                                                             \
                  __has_include(                                                                                       \
                      "EngineConfig.h") ||                                                                             \
                      __has_include(                                                                                   \
                          "GtsGameLoop.h") ||                                                                          \
                          __has_include(                                                                               \
                              "GtsPlatform.h") ||                                                                      \
                              __has_include("input/EngineControlBindings.hpp") ||                                      \
                                            __has_include("BuiltinExecutionGroups.h") ||                               \
                                                          __has_include("SceneExecutionPolicy.h") ||                   \
                                                                        __has_include("VNExecutionProfiles.h")
#error "Module targets must not expose runtime composition"
#endif

static_assert(std::is_same_v<decltype(GtsFrameEndedEvent::imageIndex), uint32_t>);
static_assert(std::is_same_v<decltype(GtsFrameEndedEvent::dt), float>);

int main()
{
    ECSWorld world;
    const auto customGroup = static_cast<EcsSystemGroup>(1ull << 41);
    world.configureDefaultExecutionSelection({"custom", toMask(customGroup)});
    world.pushExecutionSelection({"overlay", 0});
    gts::transform::installTransformFeature(world, {"requested", 0}, customGroup);
    if (world.getCurrentExecutionSelection().id != "overlay")
        return 1;
    world.clear();
    if (world.getCurrentExecutionSelection().id != "custom" ||
        world.getCurrentExecutionSelection().enabledSystems != toMask(customGroup))
        return 1;
    gts::transform::installTransformFeature(world, {"requested", 0}, customGroup);
    return world.getControllerSystemCount() == 1 ? 0 : 1;
}
