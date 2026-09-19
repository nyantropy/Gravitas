#pragma once

#include "EcsControllerContext.hpp"

class IGtsPhysicsModule;

namespace gts::physics
{
    struct PhysicsControllerContext
    {
        IGtsPhysicsModule* physics = nullptr;
    };

    inline const PhysicsControllerContext& controllerContext(const EcsControllerContext& ctx)
    {
        return ctx.frameData.get<PhysicsControllerContext>();
    }

    inline PhysicsControllerContext& controllerContext(EcsControllerContext& ctx)
    {
        return ctx.frameData.edit<PhysicsControllerContext>();
    }
} // namespace gts::physics
