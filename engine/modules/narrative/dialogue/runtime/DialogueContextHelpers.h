#pragma once

#include "DialogueEvents.h"
#include "DialogueRegistry.h"
#include "DialogueRuntime.h"
#include "EcsControllerContext.hpp"
#include "ECSWorld.hpp"

namespace gts::dialogue
{
    inline DialogueContext makeDialogueContext(const EcsControllerContext& ctx,
                                               const DialogueRuntime& runtime)
    {
        DialogueContext context;
        context.world = &ctx.world;
        context.frame = &ctx;
        context.graphId = runtime.getGraph().id;
        context.nodeId = runtime.getCurrentNodeId();
        context.actionRequested =
            [&world = ctx.world](const DialogueAction& action, const DialogueContext& actionContext)
            {
                world.publish(DialogueActionRequestedEvent{
                    actionContext.graphId,
                    actionContext.nodeId,
                    action
                });
            };
        return context;
    }
} // namespace gts::dialogue
