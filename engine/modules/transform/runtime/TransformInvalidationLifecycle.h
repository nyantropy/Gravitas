#pragma once

#include "Entity.h"

class ECSWorld;

namespace gts::transform
{
    using WorldTransformPublishedCallback = void (*)(ECSWorld&, Entity);

    void resetTransformInvalidationState(ECSWorld& world);
    void registerWorldTransformPublishedCallback(ECSWorld& world, WorldTransformPublishedCallback callback);
    void queueTransformDirty(ECSWorld& world, Entity entity);
} // namespace gts::transform
