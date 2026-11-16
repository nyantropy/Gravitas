#include "ToolCameraHandoff.h"

#include <limits>
#include "ECSWorld.hpp"
#include "EngineToolCameraStateComponent.h"

bool gts::tools::isToolCameraActive(const ECSWorld& world)
{
    return world.hasAny<EngineToolCameraStateComponent>() &&
           world.getSingleton<EngineToolCameraStateComponent>().active;
}

void gts::tools::replaceToolCameraReturnTarget(ECSWorld& world, const Entity retiringCamera, const Entity replacement)
{
    if (!isToolCameraActive(world))
        return;
    auto& state = world.getSingleton<EngineToolCameraStateComponent>();
    if (state.previousActiveCamera == retiringCamera ||
        state.previousActiveCamera.id == std::numeric_limits<entity_id_type>::max())
        state.previousActiveCamera = replacement;
}
