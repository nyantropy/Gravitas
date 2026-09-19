#include "DialogueRuntime.h"
#include "DialogueSystem.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    using namespace gts::dialogue;

    void require(bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "Dialogue runtime test failed: " << message << std::endl;
            std::exit(1);
        }
    }

    DialogueGraph makeGraph()
    {
        DialogueGraph graph;
        graph.id = "conversation";
        graph.startNode = "greeting";
        DialogueNode greeting;
        greeting.id = "greeting";
        greeting.text = "Hello";
        greeting.onEnterActions = {{"entered", {}}};
        greeting.choices = {
            {"Hidden", "farewell", {{"unregistered", {}}}, {}},
            {"Continue", "farewell", {{"allowed", {}}}, {{"selected", {}}}}
        };
        DialogueNode farewell;
        farewell.id = "farewell";
        farewell.text = "Goodbye";
        graph.nodes.emplace(greeting.id, greeting);
        graph.nodes.emplace(farewell.id, farewell);
        return graph;
    }

    void testHeadlessProgression()
    {
        DialogueRuntime runtime;
        std::vector<std::string> actions;
        runtime.conditions().registerCondition("allowed",
            [](const DialogueCondition&, const DialogueContext& context)
            {
                require(context.world == nullptr && context.frame == nullptr,
                        "standalone progression must not need ECS services");
                return true;
            });
        for (const std::string id : {"entered", "selected"})
        {
            runtime.actions().registerAction(id,
                [&actions](const DialogueAction& action, DialogueContext& context)
                {
                    require(context.graphId == "conversation" && context.nodeId == "greeting",
                            "actions must receive semantic graph/node context");
                    actions.push_back(action.id);
                });
        }

        require(runtime.start(makeGraph()), "graph did not start");
        require(actions == std::vector<std::string>{"entered"}, "entry action did not run");
        require(runtime.getVisibleChoices().size() == 1, "conditions did not filter choices");
        require(runtime.getVisibleChoices()[0].sourceIndex == 1, "visible choice lost source index");
        require(!runtime.advance(), "advance skipped a pending choice");
        require(runtime.selectChoice(0), "visible choice did not advance");
        require(actions == std::vector<std::string>{"entered", "selected"}, "choice action did not run");
        require(runtime.getCurrentNodeId() == "farewell", "choice selected the wrong node");
        require(runtime.advance() && !runtime.isActive(), "terminal progression did not end");
    }

    void testEcsRequestsAndEvents()
    {
        ECSWorld world;
        EcsControllerContext frame{world};
        DialogueSystem system;
        world.createSingleton<DialogueGraphRegistryComponent>();
        world.createSingleton<DialogueStartRequestComponent>();
        world.getSingleton<DialogueGraphRegistryComponent>().addGraph(makeGraph());
        auto& request = world.getSingleton<DialogueStartRequestComponent>();
        request.requestGraphId("conversation");

        std::vector<std::string> events;
        auto started = world.subscribe<DialogueStartedEvent>([&](const DialogueStartedEvent& event)
        {
            require(event.graphId == "conversation", "start event lost graph id");
            events.push_back("started");
        });
        auto changed = world.subscribe<DialogueNodeChangedEvent>([&](const DialogueNodeChangedEvent& event)
        {
            require(event.nodeId == "greeting", "node event lost node id");
            events.push_back("node");
        });
        auto choices = world.subscribe<DialogueChoicesChangedEvent>([&](const DialogueChoicesChangedEvent&)
        {
            events.push_back("choices");
        });
        auto action = world.subscribe<DialogueActionRequestedEvent>([&](const DialogueActionRequestedEvent& event)
        {
            require(event.graphId == "conversation" && event.nodeId == "greeting"
                        && event.action.id == "entered", "action bridge lost semantic context");
            events.push_back("action");
        });
        auto ended = world.subscribe<DialogueEndedEvent>([&](const DialogueEndedEvent& event)
        {
            require(event.graphId == "conversation", "end event lost graph id");
            events.push_back("ended");
        });

        system.update(frame);
        require(!request.requested && request.graphId.empty(), "start request was not consumed");
        require(events == std::vector<std::string>{"action", "started", "node", "choices"},
                "start/action/state event ordering changed");
        system.update(frame);
        require(events.size() == 4, "unchanged state emitted duplicate events");

        auto& runtime = world.getSingleton<DialogueRuntimeComponent>().runtime;
        DialogueContext context = makeDialogueContext(frame, runtime);
        require(context.world == &world && context.frame == &frame, "ECS context lost borrowed services");
        runtime.end(context);
        system.update(frame);
        require(events.size() == 5 && events.back() == "ended", "end event was not published");

        request.requestGraph(makeGraph());
        system.update(frame);
        require(runtime.isActive() && !request.requested && !request.useInlineGraph,
                "inline graph request was not consumed");
    }
}

int main()
{
    testHeadlessProgression();
    testEcsRequestsAndEvents();
    return 0;
}
