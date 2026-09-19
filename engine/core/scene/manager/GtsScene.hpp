#pragma once

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

#include "ECSWorld.hpp"
#include "EcsSimulationContext.hpp"
#include "EcsControllerContext.hpp"
#include "GtsSceneTransitionData.h"

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

    ECSWorld& getWorld()
    {
        return ecsWorld;
    }

    bool markSceneFeatureInstalled(const std::string& featureId)
    {
        return installedSceneFeatures.insert(featureId).second;
    }

    void registerSceneResetHook(std::function<void(ECSWorld&)> hook)
    {
        resetHooks.push_back(std::move(hook));
    }

    // One scene-owned resource per exact, unqualified object type. No base-type
    // matching or implicit creation. Duplicate creation throws before construction.
    template<typename Resource, typename... Args>
    Resource& createSceneResource(Args&&... args)
    {
        if (findSceneResource<Resource>() != nullptr)
            throw std::logic_error("Scene resource already exists");

        auto resource = std::make_unique<Resource>(std::forward<Args>(args)...);
        sceneResources.push_back(SceneResource{
            std::type_index(typeid(Resource)),
            resource.get(),
            [](void* ptr)
            {
                delete static_cast<Resource*>(ptr);
            }
        });
        return *resource.release();
    }

    // Borrowed access lasts until scene reset/destruction. Clearing only the ECS
    // world does not release scene resources. Missing lookup returns nullptr.
    template<typename Resource>
    Resource* findSceneResource()
    {
        return const_cast<Resource*>(std::as_const(*this).findSceneResource<Resource>());
    }

    template<typename Resource>
    const Resource* findSceneResource() const
    {
        static_assert(std::is_object_v<Resource> && !std::is_array_v<Resource> &&
                      std::is_same_v<Resource, std::remove_cv_t<Resource>>,
                      "Scene resource types must be unqualified, non-array object types");
        for (const SceneResource& entry : sceneResources)
        {
            if (entry.type == std::type_index(typeid(Resource)))
                return static_cast<const Resource*>(entry.resource);
        }
        return nullptr;
    }

    template<typename Resource>
    Resource& requireSceneResource()
    {
        return const_cast<Resource&>(std::as_const(*this).requireSceneResource<Resource>());
    }

    template<typename Resource>
    const Resource& requireSceneResource() const
    {
        const Resource* resource = findSceneResource<Resource>();
        if (resource == nullptr)
            throw std::logic_error("Required scene resource is missing");
        return *resource;
    }

    private:
    struct SceneResource
    {
        std::type_index type;
        void* resource = nullptr;
        void (*destroy)(void*) = nullptr;
    };

    std::vector<SceneResource> sceneResources;
    std::vector<std::function<void(ECSWorld&)>> resetHooks;
    std::unordered_set<std::string> installedSceneFeatures;

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
