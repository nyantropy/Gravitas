#pragma once

#include "EcsControllerContext.hpp"

class GtsModelRegistry;
class GtsModelRealizationCache;

namespace gts::model
{
    struct ModelControllerContext
    {
        GtsModelRegistry*         models            = nullptr;
        GtsModelRealizationCache* modelRealizations = nullptr;
    };

    inline const ModelControllerContext& controllerContext(const EcsControllerContext& ctx)
    {
        return ctx.frameData.get<ModelControllerContext>();
    }

    inline ModelControllerContext& controllerContext(EcsControllerContext& ctx)
    {
        return ctx.frameData.edit<ModelControllerContext>();
    }
} // namespace gts::model
