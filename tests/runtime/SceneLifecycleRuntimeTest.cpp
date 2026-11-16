#include "SceneLifecycleRuntimeTest.h"

#include <iostream>
#include <stdexcept>
#include <utility>

#include "GravitasEngine.h"
#include "UiControllerContext.h"
#include "UiSystem.h"

namespace
{
    struct Ping
    {
    };

    void require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }
} // namespace

LifecycleBorrowingComposition::LifecycleBorrowingComposition(LifecycleRecord& record) : record(record) {}

void LifecycleBorrowingComposition::build(UiCompositionContext&) {}

void LifecycleBorrowingComposition::destroy(UiCompositionContext&)
{
    require(record.resourcesCreated > record.resourcesDestroyed,
            "Retained UI outlived the scene resource it borrowed");
    ++record.compositionsDestroyed;
}

LifecyclePayload::LifecyclePayload(LifecycleRecord& record, std::string label) : record(record), label(std::move(label))
{
    ++record.alivePayloads;
}

LifecyclePayload::~LifecyclePayload()
{
    --record.alivePayloads;
    ++record.destroyedPayloads;
}

LifecycleResource::LifecycleResource(LifecycleRecord& record) : record(record)
{
    ++record.resourcesCreated;
}

LifecycleResource::~LifecycleResource()
{
    ++record.resourcesDestroyed;
}

LifecycleScene::LifecycleScene(LifecycleRecord& record, std::string label) : record(record), label(std::move(label)) {}

LifecycleScene::~LifecycleScene()
{
    ++record.destroyedScenes;
}

void LifecycleScene::onLoad(EcsControllerContext& ctx, const GtsSceneTransitionData* data)
{
    for (const auto& subscription : record.subscriptions)
        require(subscription.expired(), "Prior scene subscription survived teardown");
    require(record.resourcesCreated == record.resourcesDestroyed, "Prior scene resource survived scene replacement");
    record.loads.push_back(label);
    createSceneResource<LifecycleResource>(record);
    UiMountDesc mount;
    mount.name = "scene-lifetime-borrow";
    gts::ui::controllerContext(ctx).ui->mountComposition(
        std::make_unique<LifecycleBorrowingComposition>(record), mount);
    auto subscription = std::make_shared<int>(0);
    record.subscriptions.push_back(subscription);
    pingToken = ctx.world.subscribe<Ping>(
        [subscription, &record = record](const Ping&)
        {
            ++record.pingDeliveries;
        });
    if (const auto* payload = dynamic_cast<const LifecyclePayload*>(data))
        require(record.alivePayloads > 0 && payload->label == label, "Transition payload lost ownership/type");

    if (label == "B" && record.runScenario)
        ctx.engineCommands->requestChangeScene("D", std::make_unique<LifecyclePayload>(record, "D"));
}

void LifecycleScene::onUpdateSimulation(const EcsSimulationContext&) {}

void LifecycleScene::onUpdateControllers(const EcsControllerContext& ctx)
{
    if (!record.runScenario)
        return;
    if (label == "A")
    {
        ++record.aFrames;
        if (record.aFrames == 1)
        {
            ctx.engineCommands->requestChangeScene("unregistered",
                std::make_unique<LifecyclePayload>(record, "never-loaded"));
            ctx.engineCommands->requestChangeScene("B", std::make_unique<LifecyclePayload>(record, "B"));
            ctx.engineCommands->requestChangeScene("C", std::make_unique<LifecyclePayload>(record, "C"));
        }
        else if (record.aFrames == 2)
            ctx.engineCommands->requestChangeScene("still-unregistered");
        else
        {
            require(record.loads == std::vector<std::string>{"A", "B", "C", "D", "A"},
                    "FIFO transitions or deferred onLoad requests changed");
            require(record.alivePayloads == 0 && record.destroyedPayloads == 4,
                    "Transition payloads survived their dispatch batches");
            ctx.engineCommands->requestQuit();
        }
    }
    else if (label == "C")
    {
        require(record.loads == std::vector<std::string>{"A", "B", "C"},
                "onLoad request was executed recursively in the same batch");
        ctx.engineCommands->requestChangeScene("A");
    }
}

void LifecycleScene::onUnload(EcsControllerContext& ctx)
{
    require(record.compositionsDestroyed == record.resourcesCreated, "Scene unload retained UI borrowers");
    record.unloads.push_back(label);
    ctx.world.publish(Ping{});
}

int main()
try
{
    EngineConfig config;
    config.graphics.startup.headless                          = true;
    config.graphics.startup.enableValidationLayers            = false;
    config.graphics.settings.rendering.resolution.mode        = RenderResolutionMode::Fixed;
    config.graphics.settings.rendering.resolution.fixedExtent = {64, 64};
    config.tools.enabled                                      = false;
    config.tools.debugOverlayEnabledByDefault                 = false;
    config.graphics.settings.framePacing.maxFrameRate         = 0;

    LifecycleRecord record;
    {
        GravitasEngine engine(config);
        for (const std::string label : {"A", "B", "C", "D"})
            engine.registerScene(label,
                                 [&record, label]
                                 {
                                     return std::make_unique<LifecycleScene>(record, label);
                                 });
        require(engine.setActiveScene("A"), "Registered initial scene rejected");
        require(!engine.setActiveScene("missing-before-start"), "Unregistered direct request was accepted");
        require(record.loads.size() == 1 && record.unloads.empty(), "Rejected scene destroyed the active scene");
        engine.start();
    }
    require(record.loads == record.unloads && record.destroyedScenes == 5,
            "Scenes did not unload/destroy exactly once");
    require(record.resourcesCreated == 5 && record.resourcesDestroyed == 5 && record.pingDeliveries == 5,
            "Scene resources/subscriptions leaked across cycles");
    for (const auto& subscription : record.subscriptions)
        require(subscription.expired(), "Final scene subscription leaked");

    LifecycleRecord noStart;
    noStart.runScenario = false;
    {
        GravitasEngine engine(config);
        engine.registerScene("A",
                             [&noStart]
                             {
                                 return std::make_unique<LifecycleScene>(noStart, "A");
                             });
        require(engine.setActiveScene("A"), "Pre-start scene load failed");
    }
    require(noStart.unloads.size() == 1 && noStart.resourcesDestroyed == 1 && noStart.destroyedScenes == 1,
            "Engine destruction before start bypassed scene teardown");

    {
        GravitasEngine engine(config);
        require(!engine.setActiveScene("missing"), "Unknown startup scene accepted");
        bool rejected = false;
        try
        {
            engine.start();
        }
        catch (const std::logic_error&)
        {
            rejected = true;
        }
        require(rejected, "Starting without a scene must fail explicitly");
    }
    std::cout << "PASS FIFO, unknown IDs, deferred requests, payloads, scene cycles and pre-start teardown\n";
    return 0;
}
catch (const std::runtime_error& error)
{
    if (std::string(error.what()).find("failed to find GPUs with Vulkan support") != std::string::npos)
        return 77;
    std::cerr << error.what() << '\n';
    return 1;
}
