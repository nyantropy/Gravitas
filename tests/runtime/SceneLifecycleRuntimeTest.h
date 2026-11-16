#pragma once

#include <memory>
#include <string>
#include <vector>

#include "GtsScene.hpp"
#include "SubscriptionToken.hpp"
#include "UiComposition.h"

struct LifecycleRecord
{
    std::vector<std::string>        loads;
    std::vector<std::string>        unloads;
    std::vector<std::weak_ptr<int>> subscriptions;
    int                             alivePayloads      = 0;
    int                             destroyedPayloads  = 0;
    int                             resourcesCreated   = 0;
    int                             resourcesDestroyed = 0;
    int                             destroyedScenes    = 0;
    int                             pingDeliveries     = 0;
    int                             aFrames            = 0;
    int                             compositionsDestroyed = 0;
    bool                            runScenario        = true;
};

struct LifecyclePayload : GtsSceneTransitionData
{
    LifecyclePayload(LifecycleRecord& record, std::string label);
    ~LifecyclePayload() override;
    LifecycleRecord& record;
    std::string      label;
};

struct LifecycleResource
{
    explicit LifecycleResource(LifecycleRecord& record);
    ~LifecycleResource();
    LifecycleRecord& record;
};

class LifecycleScene : public GtsScene
{
    public:
    LifecycleScene(LifecycleRecord& record, std::string label);
    ~LifecycleScene() override;
    void onLoad(EcsControllerContext& ctx, const GtsSceneTransitionData* data) override;
    void onUpdateSimulation(const EcsSimulationContext& ctx) override;
    void onUpdateControllers(const EcsControllerContext& ctx) override;
    void onUnload(EcsControllerContext& ctx) override;

    private:
    LifecycleRecord&  record;
    std::string       label;
    SubscriptionToken pingToken;
};

class LifecycleBorrowingComposition : public UiComposition
{
public:
    explicit LifecycleBorrowingComposition(LifecycleRecord& record);
    void build(UiCompositionContext& context) override;
    void destroy(UiCompositionContext& context) override;
private:
    LifecycleRecord& record;
};
