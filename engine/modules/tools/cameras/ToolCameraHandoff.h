#pragma once

#include "Entity.h"

class ECSWorld;

namespace gts::tools
{
    bool isToolCameraActive(const ECSWorld& world);
    void replaceToolCameraReturnTarget(ECSWorld& world, Entity retiringCamera, Entity replacement);
} // namespace gts::tools
