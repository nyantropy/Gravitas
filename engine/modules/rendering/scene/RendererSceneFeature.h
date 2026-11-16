#pragma once

#include "RendererExecutionInputs.h"

class GtsScene;
struct EcsControllerContext;

namespace gts::rendering
{
    void
    installRendererFeature(GtsScene& scene, const EcsControllerContext& ctx, const RendererExecutionInputs& execution);
}
