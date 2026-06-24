#pragma once

#include <string>
#include <vector>

#include "DialogueContextHelpers.h"
#include "DialogueEvents.h"
#include "DialogueRuntimeComponent.h"
#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"

namespace gts::dialogue
{
    class DialogueSystem : public ECSControllerSystem
    {
    public:
        void update(const EcsControllerContext& ctx) override
        {
            DialogueRuntimeComponent& runtimeComponent = ensureRuntime(ctx.world);
            processStartRequest(ctx, runtimeComponent.runtime);
            publishStateChanges(ctx.world, runtimeComponent.runtime);
        }

    private:
        bool wasActive = false;
        uint64_t lastRevision = 0;
        std::string lastGraphId;

        static DialogueRuntimeComponent& ensureRuntime(ECSWorld& world)
        {
            if (!world.hasAny<DialogueRuntimeComponent>())
                world.createSingleton<DialogueRuntimeComponent>();

            return world.getSingleton<DialogueRuntimeComponent>();
        }

        static void processStartRequest(const EcsControllerContext& ctx, DialogueRuntime& runtime)
        {
            if (!ctx.world.hasAny<DialogueStartRequestComponent>())
                return;

            DialogueStartRequestComponent& request = ctx.world.getSingleton<DialogueStartRequestComponent>();
            if (!request.requested)
                return;

            DialogueContext context = makeDialogueContext(ctx, runtime);
            if (request.useInlineGraph)
            {
                runtime.start(request.graph, context);
            }
            else if (ctx.world.hasAny<DialogueGraphRegistryComponent>())
            {
                const DialogueGraphRegistryComponent& registry =
                    ctx.world.getSingleton<DialogueGraphRegistryComponent>();
                if (const DialogueGraph* graph = registry.findGraph(request.graphId))
                    runtime.start(*graph, context);
            }

            request.requested = false;
            request.graphId.clear();
            request.graph = DialogueGraph{};
            request.useInlineGraph = false;
        }

        void publishStateChanges(ECSWorld& world, const DialogueRuntime& runtime)
        {
            const bool active = runtime.isActive();
            const uint64_t revision = runtime.getRevision();

            if (active && !wasActive)
            {
                lastGraphId = runtime.getGraph().id;
                world.publish(DialogueStartedEvent{
                    runtime.getGraph().id,
                    runtime.getCurrentNodeId()
                });
            }

            if (active && revision != lastRevision)
            {
                publishNodeAndChoices(world, runtime);
            }

            if (!active && wasActive)
            {
                world.publish(DialogueEndedEvent{lastGraphId});
                lastGraphId.clear();
            }

            wasActive = active;
            lastRevision = revision;
        }

        static void publishNodeAndChoices(ECSWorld& world, const DialogueRuntime& runtime)
        {
            const DialogueNode* node = runtime.getCurrentNode();
            if (node == nullptr)
                return;

            world.publish(DialogueNodeChangedEvent{
                runtime.getGraph().id,
                node->id,
                node->speaker,
                node->text
            });
            world.publish(DialogueChoicesChangedEvent{
                runtime.getGraph().id,
                node->id,
                runtime.getVisibleChoices()
            });
        }
    };
} // namespace gts::dialogue
