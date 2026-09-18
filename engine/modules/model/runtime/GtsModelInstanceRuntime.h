#pragma once

#include "GtsModelInstance.h"
#include "assets/realization/model/GtsModelRealizationCache.h"

struct GtsModelInstanceCreationResult
{
    std::unique_ptr<GtsModelInstance> instance;
    std::vector<GtsModelDiagnostic>   diagnostics;
    bool                              succeeded() const
    {
        return static_cast<bool>(instance);
    }
};

// World-owned creation facade, not an instance registry. It retains no mutable instances.
// Engine realization cache and resource provider must outlive the world service.
class GtsModelInstanceRuntime
{
    public:
    GtsModelInstanceRuntime(ECSWorld& world, GtsModelRealizationCache& geometry, IResourceProvider* resources)
        : world(world), geometry(geometry), resources(resources)
    {
    }
    [[nodiscard]] GtsModelInstanceCreationResult create(GtsModelHandle model);

    private:
    ECSWorld&                 world;
    GtsModelRealizationCache& geometry;
    IResourceProvider*        resources;
};

// Configure once during world setup; later entity creation uses modelInstances(world).create(handle).
GtsModelInstanceRuntime&
modelInstances(ECSWorld& world, GtsModelRealizationCache& geometry, IResourceProvider* resources);
GtsModelInstanceRuntime& modelInstances(ECSWorld& world);
