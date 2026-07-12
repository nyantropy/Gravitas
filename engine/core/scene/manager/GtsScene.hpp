#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "ECSWorld.hpp"
#include "EcsSimulationContext.hpp"
#include "EcsControllerContext.hpp"
#include "GtsSceneTransitionData.h"
#include "GtsFrameStats.h"

class IGtsPhysicsModule;

// Override this class to define a reusable engine scene in terms of entities,
// components, and engine systems.
class GtsScene
{
    protected:
    ECSWorld ecsWorld;

    void resetSceneWorld()
    {
        for (const auto& hook : resetHooks)
            hook(ecsWorld);

        ecsWorld.clear();
        clearSceneResources();
        physicsModule = nullptr;
        installedSceneFeatures.clear();
        resetHooks.clear();
    }

    public:
    virtual ~GtsScene()
    {
        clearSceneResources();
    }

    // Called once whenever the scene is loaded.
    virtual void onLoad(EcsControllerContext& ctx, const GtsSceneTransitionData* data = nullptr) = 0;

    // Called once per simulation tick at the fixed tick rate.
    // Deterministic scene simulation goes here.
    virtual void onUpdateSimulation(const EcsSimulationContext& ctx) = 0;

    // Called once per rendered frame regardless of tick rate.
    // Input processing, visual updates, and rendering prep go here.
    virtual void onUpdateControllers(const EcsControllerContext& ctx) {}

    virtual void onUnload(EcsControllerContext& /*ctx*/) {}

    void unload(EcsControllerContext& ctx)
    {
        onUnload(ctx);
        resetSceneWorld();
    }

    virtual void populateFrameStats(GtsFrameStats& /*stats*/) const {}
    virtual void onFrameStats(const GtsFrameStats& /*stats*/) {}

    ECSWorld& getWorld()
    {
        return ecsWorld;
    }

    IGtsPhysicsModule* getPhysicsModule()
    {
        return physicsModule;
    }

    const IGtsPhysicsModule* getPhysicsModule() const
    {
        return physicsModule;
    }

    void setPhysicsModule(IGtsPhysicsModule* module)
    {
        physicsModule = module;
    }

    bool markSceneFeatureInstalled(const std::string& featureId)
    {
        return installedSceneFeatures.insert(featureId).second;
    }

    void registerSceneResetHook(std::function<void(ECSWorld&)> hook)
    {
        resetHooks.push_back(std::move(hook));
    }

    template<typename Resource, typename... Args>
    Resource& createSceneResource(Args&&... args)
    {
        Resource* resource = new Resource(std::forward<Args>(args)...);
        sceneResources.push_back(SceneResource{
            resource,
            [](void* ptr)
            {
                delete static_cast<Resource*>(ptr);
            }
        });
        return *resource;
    }

    private:
    struct SceneResource
    {
        void* resource = nullptr;
        void (*destroy)(void*) = nullptr;
    };

    std::vector<SceneResource> sceneResources;
    std::vector<std::function<void(ECSWorld&)>> resetHooks;
    std::unordered_set<std::string> installedSceneFeatures;
    IGtsPhysicsModule* physicsModule = nullptr;

    void clearSceneResources()
    {
        for (SceneResource& sceneResource : sceneResources)
        {
            if (sceneResource.destroy != nullptr)
                sceneResource.destroy(sceneResource.resource);
        }
        sceneResources.clear();
    }
};
