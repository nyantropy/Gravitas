#include "ModelScaling.h"

#include "TransformScaling.h"
#include "GtsModelRegistry.h"
#include "GtsModelResource.h"
#include "GtsModelRealizationCache.h"
#include "GtsModelInstanceRuntime.h"
#include "ModelInstanceComponent.h"
#include "MaterialRuntime.h"
#include <stdexcept>

namespace gts::performance
{
    namespace
    {
        class ModelWorkload : public Workload
        {
            public:
            ModelWorkload(const std::string& name, Parameters parameters) : parameters(parameters)
            {
                installTransformWorkload(world, name == "moving-model-instances", false);
                auto loaded = registry.requestModel(GtsModelRequest{GTS_PERF_MODEL_FIXTURE, {.geometry = true}});
                if (!loaded.succeeded())
                    throw std::runtime_error(loaded.diagnostics().empty() ? "Model fixture load failed"
                                                                          : loaded.diagnostics().front().message);
                if (!loaded.handle()->canonicalModel())
                    throw std::runtime_error("Benchmark expects the source fixture; remove colocated cooked artifacts");
                auto&                  instances = modelInstances(world, geometry, nullptr);
                auto&                  materials = gts::rendering::materialRuntime(world);
                MaterialInstanceHandle shared;
                const bool             sharedOverride = name == "shared-material-model-instances";
                const bool             variedOverride = name == "varied-material-model-instances";
                if (sharedOverride)
                    shared = materials.createInstance(MaterialInstance{});
                uint64_t trianglesPerInstance = 0;
                for (uint32_t i = 0; i < parameters.count; ++i)
                {
                    auto created = instances.create(loaded.handle());
                    if (!created.succeeded())
                        throw std::runtime_error(created.diagnostics.empty() ? "Model realization failed"
                                                                             : created.diagnostics.front().message);
                    if (i == 0)
                        for (const auto& occurrence : created.instance->geometry()->occurrences)
                            for (const auto& primitive :
                                 created.instance->geometry()->geometry[occurrence.geometryIndex].primitives())
                                trianglesPerInstance += primitive.indexCount / 3;
                    if (variedOverride || sharedOverride)
                    {
                        auto handle = shared;
                        if (variedOverride)
                        {
                            MaterialInstance material;
                            material.baseColor = {float((i ^ parameters.seed) % 251) / 250.0f, 0.5f, 0.25f, 1.0f};
                            handle             = materials.createInstance(material);
                        }
                        if (!created.instance->setMaterialOverride(handle, materials.lifetimeToken()).succeeded())
                            throw std::runtime_error("Model material override failed");
                    }
                    Entity entity = addTransformEntity(world, i, parameters.seed, false);
                    world.addComponent(entity, ModelInstanceComponent{std::move(created.instance)});
                }
                world.updateControllers(EcsControllerContext{world});
                counters                       = emptyCounters();
                counters["entities"]           = world.getEntityCount();
                counters["model_instances"]    = parameters.count;
                counters["authored_triangles"] = trianglesPerInstance * parameters.count;
                counters["distinct_override_materials"] =
                    variedOverride ? parameters.count : (sharedOverride ? 1u : 0u);
            }

            Sample step(uint32_t tick) override
            {
                return measureWorld(world, parameters, tick, false, true, counters);
            }

            private:
            Parameters parameters;
            // Services outlive the world and its scene-owned model/material access.
            GtsModelRegistry         registry;
            GtsModelRealizationCache geometry;
            ECSWorld                 world;
            Counters                 counters;
        };
    } // namespace

    std::unique_ptr<Workload> makeModelWorkload(const std::string& name, const Parameters& parameters)
    {
        return std::make_unique<ModelWorkload>(name, parameters);
    }
} // namespace gts::performance
