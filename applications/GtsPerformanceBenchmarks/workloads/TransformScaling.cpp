#include "TransformScaling.h"

#include <chrono>
#include "AnimationSceneFeature.h"
#include "SceneExecutionPolicy.h"
#include "TransformSceneFeature.h"
#include "TransformSystem.hpp"
#include "TransformDirtyHelpers.h"
#include "ModelFrameExtraction.h"

namespace gts::performance
{
    namespace
    {
        using Clock = std::chrono::steady_clock;
        double milliseconds(Clock::time_point start, Clock::time_point end)
        {
            return std::chrono::duration<double, std::milli>(end - start).count();
        }

        class MoveTransforms : public ECSSimulationSystem
        {
            public:
            void update(const EcsSimulationContext& context) override
            {
                context.world.forEach<TransformComponent>(
                    [&](Entity entity, TransformComponent& transform)
                    {
                        transform.rotation.y += context.dt;
                        gts::transform::markDirty(context.world, entity);
                    });
            }
        };

        class TransformWorkload : public Workload
        {
            public:
            TransformWorkload(const std::string& name, Parameters parameters)
                : parameters(parameters), animated(name == "transform-animation")
            {
                installTransformWorkload(world, name == "moving-transforms", animated);
                for (uint32_t i = 0; i < parameters.count; ++i)
                    addTransformEntity(world, i, parameters.seed, animated);
                world.updateControllers(EcsControllerContext{world});
                counters                                = emptyCounters();
                counters["entities"]                    = world.getEntityCount();
                counters["model_instances"]             = 0;
                counters["authored_triangles"]          = 0;
                counters["distinct_override_materials"] = 0;
            }

            Sample step(uint32_t tick) override
            {
                return measureWorld(world, parameters, tick, animated, false, counters);
            }

            private:
            Parameters parameters;
            bool       animated;
            ECSWorld   world;
            Counters   counters;
        };
    } // namespace

    void installTransformWorkload(ECSWorld& world, bool moving, bool animated)
    {
        const auto selection = SceneExecutionProfile::gameplay();
        gts::transform::installTransformFeature(world, selection, gts::execution::groups::RenderPrep);
        if (moving)
            world.addSimulationSystem<MoveTransforms>(gts::execution::groups::Animation);
        if (animated)
            gts::animation::installAnimationFeature(world, selection, gts::execution::groups::Animation);
        gts::transform::TransformSystem::setDetailedMetricsEnabled(false);
    }

    Entity addTransformEntity(ECSWorld& world, uint32_t index, uint32_t seed, bool animated)
    {
        const Entity       entity = world.createEntity();
        TransformComponent transform;
        transform.position   = {float(index % 1000), float(index / 1000), 0.0f};
        transform.rotation.y = float((index ^ seed) % 1024) / 1024.0f;
        world.addComponent(entity, transform);
        if (animated)
        {
            TransformAnimationComponent animation;
            animation.enableMode(TransformAnimationMode::Rotate);
            animation.rotationEulerFactors = {0.0f, 1.0f, 0.0f};
            animation.rotationSpeed        = 1.0f;
            world.addComponent(entity, animation);
        }
        return entity;
    }

    Sample measureWorld(ECSWorld&         world,
                        const Parameters& parameters,
                        uint32_t          tick,
                        bool              animated,
                        bool              extractModels,
                        const Counters&   fixedCounters)
    {
        Sample     sample{tick, emptyTimings(), fixedCounters};
        const auto start = Clock::now();
        world.updateSimulation(EcsSimulationContext{world, static_cast<float>(parameters.dt)});
        const auto simulationEnd = Clock::now();
        world.updateControllers(EcsControllerContext{world});
        const auto controllerEnd = Clock::now();
        const auto transform     = gts::transform::TransformSystem::getLastMetrics();
        uint64_t   staticDraws = 0, skinnedDraws = 0;
        const auto extractionStart = Clock::now();
        if (extractModels)
        {
            auto frame   = extractModelFrame(world, 0, nullptr);
            staticDraws  = frame.staticDraws.size();
            skinnedDraws = frame.skinnedDraws.size();
        } // Include release of the CPU frame's ownership in extraction time.
        const auto end                  = Clock::now();
        sample.timings["simulation_ms"] = milliseconds(start, simulationEnd);
        sample.timings["controller_ms"] = milliseconds(simulationEnd, controllerEnd);
        sample.timings["transform_ms"]  = transform.cpuTimeMs;
        if (animated)
            sample.timings["animation_ms"] = sample.timings["simulation_ms"];
        if (extractModels)
        {
            sample.timings["model_extraction_ms"]      = milliseconds(extractionStart, end);
            sample.counters["static_draw_candidates"]  = staticDraws;
            sample.counters["skinned_draw_candidates"] = skinnedDraws;
        }
        sample.timings["cpu_frame_ms"]              = milliseconds(start, end);
        sample.counters["transforms_queued"]        = transform.queuedTransforms;
        sample.counters["transforms_processed"]     = transform.processedTransforms;
        sample.counters["world_transforms_updated"] = transform.updatedWorldTransforms;
        return sample;
    }

    std::unique_ptr<Workload> makeTransformWorkload(const std::string& name, const Parameters& parameters)
    {
        return std::make_unique<TransformWorkload>(name, parameters);
    }
} // namespace gts::performance
