#include "GtsModelInstanceRuntime.h"
#include <stdexcept>

GtsModelInstanceCreationResult GtsModelInstanceRuntime::create(GtsModelHandle model)
{
    GtsModelInstanceCreationResult result;
    try
    {
        auto prepared      = geometry.realize(model);
        result.diagnostics = prepared.diagnostics();
        if (!prepared.succeeded())
            return result;
        auto materials = modelMaterialRealization(world, resources).realize(prepared.model());
        result.diagnostics.insert(result.diagnostics.end(), materials.diagnostics.begin(), materials.diagnostics.end());
        if (!materials.succeeded())
            return result;
        auto instance = std::unique_ptr<GtsModelInstance>(
            new GtsModelInstance(std::move(model), prepared.model(), materials.materials));
        auto initialized = instance->initialize();
        result.diagnostics.insert(
            result.diagnostics.end(), initialized.diagnostics.begin(), initialized.diagnostics.end());
        if (initialized.succeeded())
            result.instance = std::move(instance);
    }
    catch (const std::exception& error)
    {
        result.diagnostics.push_back(
            {GtsModelDiagnosticSeverity::Error, "model.instance.creation", error.what(), "model instance"});
    }
    return result;
}

namespace
{
    struct GtsModelInstanceRuntimeComponent
    {
        // ECS archetypes copy components during structural migration; this keeps service identity stable.
        std::shared_ptr<GtsModelInstanceRuntime> runtime;
        GtsModelRealizationCache*                geometry  = nullptr;
        IResourceProvider*                       resources = nullptr;
    };
} // namespace
GtsModelInstanceRuntime&
modelInstances(ECSWorld& world, GtsModelRealizationCache& geometry, IResourceProvider* resources)
{
    if (!world.hasAny<GtsModelInstanceRuntimeComponent>())
        world.createSingleton<GtsModelInstanceRuntimeComponent>();
    auto& service = world.getSingleton<GtsModelInstanceRuntimeComponent>();
    if (service.runtime && (service.geometry != &geometry || service.resources != resources))
        throw std::logic_error("Model instance service is already configured for this world");
    if (!service.runtime)
    {
        service.geometry  = &geometry;
        service.resources = resources;
        service.runtime   = std::make_shared<GtsModelInstanceRuntime>(world, geometry, resources);
    }
    return *service.runtime;
}
GtsModelInstanceRuntime& modelInstances(ECSWorld& world)
{
    if (!world.hasAny<GtsModelInstanceRuntimeComponent>())
        throw std::logic_error("Configure the model instance service during world setup first");
    return *world.getSingleton<GtsModelInstanceRuntimeComponent>().runtime;
}
