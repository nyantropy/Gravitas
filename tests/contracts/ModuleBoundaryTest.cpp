#include "TransformSceneFeature.h"
#include "ECSWorld.hpp"
#include "GtsFrameEndedEvent.h"

#include <type_traits>

#if __has_include("GravitasEngine.hpp") || __has_include("EngineConfig.h") || \
    __has_include("GtsGameLoop.h") || __has_include("GtsPlatform.h") || \
    __has_include("input/EngineControlBindings.hpp")
#error "Module targets must not expose runtime composition"
#endif

static_assert(std::is_same_v<decltype(GtsFrameEndedEvent::imageIndex), uint32_t>);
static_assert(std::is_same_v<decltype(GtsFrameEndedEvent::dt), float>);

int main()
{
    ECSWorld world;
    gts::transform::installTransformFeature(world);
    return world.getControllerSystemCount() == 1 ? 0 : 1;
}
