#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "BoundsComponent.h"
#include "CameraDescriptionComponent.h"
#include "DirectionalLightComponent.h"
#include "DynamicMeshComponent.h"
#include "DynamicMeshBindingSystem.hpp"
#include "EngineConfig.h"
#include "GeometryBindingLifecycle.h"
#include "GlmConfig.h"
#include "GraphicsConstants.h"
#include "GravitasEngine.hpp"
#include "GtsFrameStats.h"
#include "GtsScene.hpp"
#include "MaterialReferenceComponent.h"
#include "MaterialRuntime.h"
#include "ParticleEmitterComponent.h"
#include "PointLightComponent.h"
#include "QuadMeshComponent.h"
#include "RendererSceneFeature.h"
#include "RenderGpuSystem.hpp"
#include "RenderingBenchmark.h"
#include "SpotLightComponent.h"
#include "TransformComponent.h"
#include "TransformDirtyHelpers.h"
#include "TransformHierarchyHelpers.h"
#include "TransformSystem.hpp"
#include "Vertex.h"
#include "WorldTextComponent.h"

namespace
{
    using gts::rendering::benchmarks::BenchmarkEnvironment;
    using gts::rendering::benchmarks::BenchmarkPreset;
    using gts::rendering::benchmarks::BenchmarkRunMode;
    using gts::rendering::benchmarks::BenchmarkRunResult;
    using gts::rendering::benchmarks::RenderingBenchmarkConfig;

    constexpr int GpuBenchmarkSkipExitCode = 77;
    constexpr uint32_t TimestampWaitFrameBudget = 240;

    struct CliOptions
    {
        std::string preset = "static_geometry_small";
        std::string outputPath;
        bool listPresets = false;
        bool checkInvariants = false;
        bool quiet = false;
        std::vector<std::pair<std::string, std::string>> overrides;
    };

    struct RuntimeBenchmarkWorld
    {
        std::vector<Entity> renderables;
        std::vector<Entity> movingLights;
        std::vector<MaterialInstanceHandle> materials;
    };

    struct RuntimeBenchmarkCollector
    {
        RenderingBenchmarkConfig config;
        std::map<std::string, std::vector<double>> cpuSamples;
        std::map<std::string, std::vector<double>> gpuSamples;
        std::map<std::string, std::vector<double>> controllerSamples;
        std::map<std::string, std::vector<double>> controllerFlushSamples;
        std::map<std::string, std::vector<double>> controllerSubstageSamples;
        std::map<std::string, std::string> controllerGroups;
        std::map<std::string, uint64_t> counters;
        std::vector<std::string> warnings;
        bool gpuTimingSupported = false;
        bool gpuTimingAvailable = false;
        std::string gpuTimingStatus = "GPU timestamp samples were not collected";
        uint32_t renderedFrames = 0;
        uint32_t measuredFrames = 0;
    };

    void printUsage()
    {
        std::cout
            << "Usage: GtsRenderingBenchmarks [options]\n"
            << "\n"
            << "Options:\n"
            << "  --list-presets\n"
            << "  --preset <name>\n"
            << "  --output <path>\n"
            << "  --check-invariants\n"
            << "  --quiet\n"
            << "  --set <key=value>\n"
            << "  --mode <cpu_smoke|gpu_runtime>\n"
            << "  --warmup-frames <n>\n"
            << "  --measured-frames <n>\n"
            << "  --renderable-count <n>\n"
            << "  --visible-renderable-count <n>\n"
            << "  --unique-mesh-count <n>\n"
            << "  --unique-material-count <n>\n"
            << "  --seed <n>\n";
    }

    bool splitAssignment(std::string_view text, std::string& key, std::string& value)
    {
        const size_t equals = text.find('=');
        if (equals == std::string_view::npos || equals == 0 || equals + 1 >= text.size())
            return false;

        key = std::string(text.substr(0, equals));
        value = std::string(text.substr(equals + 1));
        return true;
    }

    bool parseArgs(int argc, char** argv, CliOptions& options)
    {
        for (int i = 1; i < argc; ++i)
        {
            const std::string_view arg = argv[i];
            auto requireValue = [&](const char* option) -> std::optional<std::string>
            {
                if (i + 1 >= argc)
                {
                    std::cerr << option << " requires a value\n";
                    return std::nullopt;
                }
                ++i;
                return std::string(argv[i]);
            };

            if (arg == "--help" || arg == "-h")
            {
                printUsage();
                std::exit(EXIT_SUCCESS);
            }
            if (arg == "--list-presets")
            {
                options.listPresets = true;
                continue;
            }
            if (arg == "--check-invariants")
            {
                options.checkInvariants = true;
                continue;
            }
            if (arg == "--quiet")
            {
                options.quiet = true;
                continue;
            }
            if (arg == "--preset")
            {
                auto value = requireValue("--preset");
                if (!value)
                    return false;
                options.preset = *value;
                continue;
            }
            if (arg == "--output")
            {
                auto value = requireValue("--output");
                if (!value)
                    return false;
                options.outputPath = *value;
                continue;
            }
            if (arg == "--set")
            {
                auto assignment = requireValue("--set");
                if (!assignment)
                    return false;
                std::string key;
                std::string value;
                if (!splitAssignment(*assignment, key, value))
                {
                    std::cerr << "--set requires key=value\n";
                    return false;
                }
                options.overrides.emplace_back(std::move(key), std::move(value));
                continue;
            }

            if (arg.rfind("--", 0) == 0)
            {
                auto value = requireValue(std::string(arg).c_str());
                if (!value)
                    return false;
                options.overrides.emplace_back(std::string(arg.substr(2)), *value);
                continue;
            }

            std::cerr << "unknown argument: " << arg << "\n";
            return false;
        }

        return true;
    }

    std::vector<Vertex> benchmarkQuadVertices(float size, uint32_t frame)
    {
        const float half = size * 0.5f;
        const float offset = static_cast<float>(frame % 17u) * 0.001f;
        const glm::vec3 normal = {0.0f, 0.0f, 1.0f};
        const glm::vec4 tangent = {1.0f, 0.0f, 0.0f, 1.0f};
        const glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};
        return {
            Vertex{{-half, -half + offset, 0.0f}, normal, tangent, color, {0.0f, 0.0f}},
            Vertex{{ half, -half,          0.0f}, normal, tangent, color, {1.0f, 0.0f}},
            Vertex{{ half,  half,          0.0f}, normal, tangent, color, {1.0f, 1.0f}},
            Vertex{{-half,  half,          0.0f}, normal, tangent, color, {0.0f, 1.0f}}
        };
    }

    std::vector<Vertex> benchmarkDoubleQuadVertices(float size, uint32_t frame)
    {
        std::vector<Vertex> vertices = benchmarkQuadVertices(size, frame);
        std::vector<Vertex> second = benchmarkQuadVertices(size * 0.75f, frame + 3u);
        for (Vertex& vertex : second)
            vertex.pos.x += size * 0.8f;
        vertices.insert(vertices.end(), second.begin(), second.end());
        return vertices;
    }

    bool dynamicMeshGrowthPreset(const RenderingBenchmarkConfig& config)
    {
        return config.presetName == "dynamic_mesh_growth";
    }

    bool dynamicMeshAttributeGenerationPreset(const RenderingBenchmarkConfig& config)
    {
        return config.presetName == "dynamic_mesh_attribute_generation";
    }

    bool dynamicMeshGrowthExpanded(const RenderingBenchmarkConfig& config,
                                   uint32_t frame)
    {
        if (!dynamicMeshGrowthPreset(config) || frame < config.warmupFrames)
            return false;
        return (((frame - config.warmupFrames) / 30u) % 2u) != 0u;
    }

    VertexAttributeFlags dynamicMeshSourceAttributes(const RenderingBenchmarkConfig& config)
    {
        return dynamicMeshAttributeGenerationPreset(config)
            ? LegacyUnlitVertexAttributes
            : StandardVertexAttributes;
    }

    std::vector<Vertex> dynamicMeshVerticesForFrame(const RenderingBenchmarkConfig& config,
                                                    uint32_t frame)
    {
        if (dynamicMeshGrowthExpanded(config, frame))
            return benchmarkDoubleQuadVertices(1.0f, frame);
        return benchmarkQuadVertices(1.0f, frame);
    }

    std::vector<uint32_t> dynamicMeshIndicesForFrame(const RenderingBenchmarkConfig& config,
                                                     uint32_t frame)
    {
        if (dynamicMeshGrowthExpanded(config, frame))
            return {0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7};
        return {0, 1, 2, 0, 2, 3};
    }

    MaterialInstanceHandle createRuntimeMaterial(ECSWorld& world,
                                                 const RenderingBenchmarkConfig& config,
                                                 uint32_t index)
    {
        gts::rendering::MaterialRuntime& runtime = gts::rendering::materialRuntime(world);
        const MaterialInstanceHandle source = config.enablePbr
            ? runtime.defaultStandardSurfaceMaterial()
            : runtime.defaultMaterial();
        MaterialInstanceHandle handle = runtime.cloneInstance(source);
        runtime.modifyInstance(
            handle,
            [&](MaterialInstance& instance)
            {
                const float r = 0.35f + 0.55f * static_cast<float>((index * 17u) % 101u) / 100.0f;
                const float g = 0.30f + 0.60f * static_cast<float>((index * 31u) % 101u) / 100.0f;
                const float b = 0.25f + 0.65f * static_cast<float>((index * 47u) % 101u) / 100.0f;
                instance.baseColor = {r, g, b, 1.0f};
                instance.metallic = config.enablePbr ? 0.15f : 0.0f;
                instance.roughness = config.enablePbr ? 0.65f : 1.0f;
                return true;
            });
        return handle;
    }

    void createRuntimeCamera(ECSWorld& world, const RenderingBenchmarkConfig& config)
    {
        Entity camera = world.createEntity();
        TransformComponent transform;
        transform.position = {0.0f, 0.0f, 80.0f};
        world.addComponent(camera, transform);
        CameraDescriptionComponent cameraDesc;
        cameraDesc.active = true;
        cameraDesc.target = {0.0f, 0.0f, 0.0f};
        cameraDesc.aspectRatio = config.renderHeight == 0
            ? 1.0f
            : static_cast<float>(config.renderWidth) / static_cast<float>(config.renderHeight);
        cameraDesc.farClip = 500.0f;
        world.addComponent(camera, cameraDesc);
    }

    void createRuntimeLights(ECSWorld& world,
                             const RenderingBenchmarkConfig& config,
                             RuntimeBenchmarkWorld& generated)
    {
        for (uint32_t i = 0; i < config.directionalLightCount; ++i)
        {
            Entity light = world.createEntity();
            TransformComponent transform;
            transform.rotation = {0.2f + 0.05f * static_cast<float>(i), 0.6f, 0.0f};
            world.addComponent(light, transform);
            DirectionalLightComponent desc;
            desc.active = true;
            desc.priority = static_cast<int>(config.directionalLightCount - i);
            desc.intensity = 1.0f + 0.1f * static_cast<float>(i % 4u);
            world.addComponent(light, desc);
            if (generated.movingLights.size() < config.movingLightCount)
                generated.movingLights.push_back(light);
        }

        for (uint32_t i = 0; i < config.pointLightCount; ++i)
        {
            Entity light = world.createEntity();
            TransformComponent transform;
            const float angle = static_cast<float>(i) * 0.37f;
            transform.position = {std::cos(angle) * 25.0f, std::sin(angle * 0.7f) * 6.0f,
                                  std::sin(angle) * 25.0f};
            world.addComponent(light, transform);
            PointLightComponent desc;
            desc.priority = static_cast<int>(i % 7u);
            desc.range = 30.0f;
            desc.intensity = 2.0f;
            world.addComponent(light, desc);
            if (generated.movingLights.size() < config.movingLightCount)
                generated.movingLights.push_back(light);
        }

        for (uint32_t i = 0; i < config.spotLightCount; ++i)
        {
            Entity light = world.createEntity();
            TransformComponent transform;
            const float angle = static_cast<float>(i) * 0.51f;
            transform.position = {std::cos(angle) * 18.0f, 10.0f, std::sin(angle) * 18.0f};
            transform.rotation = {0.7f, angle, 0.0f};
            world.addComponent(light, transform);
            SpotLightComponent desc;
            desc.priority = static_cast<int>(i % 9u);
            desc.range = 28.0f;
            desc.intensity = 3.0f;
            world.addComponent(light, desc);
            if (generated.movingLights.size() < config.movingLightCount)
                generated.movingLights.push_back(light);
        }
    }

    void createRuntimeRenderables(ECSWorld& world,
                                  const RenderingBenchmarkConfig& config,
                                  RuntimeBenchmarkWorld& generated)
    {
        gts::rendering::MaterialRuntime& runtime = gts::rendering::materialRuntime(world);
        generated.materials.reserve(config.uniqueMaterialCount);
        for (uint32_t i = 0; i < config.uniqueMaterialCount; ++i)
            generated.materials.push_back(createRuntimeMaterial(world, config, i));
        if (generated.materials.empty())
            generated.materials.push_back(runtime.defaultStandardSurfaceMaterial());

        std::mt19937 rng(config.seed);
        std::uniform_real_distribution<float> jitter(-0.35f, 0.35f);
        const uint32_t gridSide = static_cast<uint32_t>(
            std::ceil(std::sqrt(static_cast<double>(std::max(1u, config.renderableCount)))));
        const uint32_t requestedVisible = config.visibleRenderableCount == 0
            ? config.renderableCount
            : std::min(config.visibleRenderableCount, config.renderableCount);

        for (uint32_t i = 0; i < config.renderableCount; ++i)
        {
            Entity entity = world.createEntity();
            TransformComponent transform;
            const uint32_t x = i % gridSide;
            const uint32_t y = i / gridSide;
            const bool shouldBeVisible = i < requestedVisible;
            transform.position = {
                (static_cast<float>(x) - static_cast<float>(gridSide) * 0.5f) * 2.8f + jitter(rng),
                (static_cast<float>(y) - static_cast<float>(gridSide) * 0.5f) * 2.8f + jitter(rng),
                shouldBeVisible ? jitter(rng) : 900.0f + static_cast<float>(i)
            };
            world.addComponent(entity, transform);

            if (i < config.dynamicMeshCount)
            {
                DynamicMeshComponent dynamicMesh;
                dynamicMesh.vertices = dynamicMeshVerticesForFrame(config, 0);
                dynamicMesh.indices = dynamicMeshIndicesForFrame(config, 0);
                dynamicMesh.geometryVersion = 1;
                dynamicMesh.sourceAttributes = dynamicMeshSourceAttributes(config);
                world.addComponent(entity, dynamicMesh);
            }
            else
            {
                QuadMeshComponent quad;
                const uint32_t meshBucket = i % std::max(1u, config.uniqueMeshCount);
                quad.width = 0.95f + 0.05f * static_cast<float>(meshBucket % 8u);
                quad.height = 0.95f + 0.07f * static_cast<float>((meshBucket / 8u) % 8u);
                world.addComponent(entity, quad);
            }

            world.addComponent(entity, MaterialReferenceComponent{
                generated.materials[i % generated.materials.size()]
            });
            world.addComponent(entity, BoundsComponent{});
            generated.renderables.push_back(entity);
        }
    }

    bool isDeepHierarchyPreset(const RenderingBenchmarkConfig& config)
    {
        return config.presetName == "moving_deep_hierarchy";
    }

    bool isWideHierarchyPreset(const RenderingBenchmarkConfig& config)
    {
        return config.presetName == "moving_wide_hierarchy";
    }

    void attachRuntimeHierarchy(ECSWorld& world,
                                RuntimeBenchmarkWorld& generated,
                                const RenderingBenchmarkConfig& config)
    {
        if ((!isDeepHierarchyPreset(config) && !isWideHierarchyPreset(config)) ||
            generated.renderables.size() < 2)
        {
            return;
        }

        const uint32_t movingRoots =
            std::clamp<uint32_t>(config.movingObjectCount, 1u, static_cast<uint32_t>(generated.renderables.size()));
        const uint32_t rootSide = static_cast<uint32_t>(
            std::ceil(std::sqrt(static_cast<double>(movingRoots))));
        for (uint32_t i = 0; i < movingRoots; ++i)
        {
            TransformComponent& transform = world.getComponent<TransformComponent>(generated.renderables[i]);
            const uint32_t x = i % rootSide;
            const uint32_t y = i / rootSide;
            transform.position = {
                (static_cast<float>(x) - static_cast<float>(rootSide) * 0.5f) * 2.0f,
                (static_cast<float>(y) - static_cast<float>(rootSide) * 0.5f) * 2.0f,
                0.0f
            };
            gts::transform::markDirty(world, generated.renderables[i]);
        }

        if (isDeepHierarchyPreset(config))
        {
            for (uint32_t i = movingRoots; i < generated.renderables.size(); ++i)
            {
                Entity child = generated.renderables[i];
                Entity parent = generated.renderables[i - movingRoots];
                TransformComponent& transform = world.getComponent<TransformComponent>(child);
                transform.position = {0.0f, 0.85f, 0.0f};
                gts::transform::attachToParent(world, child, parent);
            }
            return;
        }

        for (uint32_t i = movingRoots; i < generated.renderables.size(); ++i)
        {
            Entity child = generated.renderables[i];
            Entity parent = generated.renderables[i % movingRoots];
            const uint32_t childOrdinal = i / movingRoots;
            const float angle = static_cast<float>(i % movingRoots) * 0.37f +
                static_cast<float>(childOrdinal) * 0.19f;
            TransformComponent& transform = world.getComponent<TransformComponent>(child);
            transform.position = {
                std::cos(angle) * (1.1f + 0.12f * static_cast<float>(childOrdinal % 5u)),
                std::sin(angle) * (1.1f + 0.12f * static_cast<float>(childOrdinal % 5u)),
                0.0f
            };
            gts::transform::attachToParent(world, child, parent);
        }
    }

    void createRuntimeWorldText(ECSWorld& world,
                                const RenderingBenchmarkConfig& config)
    {
        const std::string fontPath =
            std::string(GraphicsConstants::ENGINE_RESOURCES) + "/fonts/gravitasfont.font.json";
        for (uint32_t i = 0; i < config.worldTextCount; ++i)
        {
            Entity entity = world.createEntity();
            TransformComponent transform;
            transform.position = {
                -20.0f + static_cast<float>(i % 20u) * 2.0f,
                16.0f + static_cast<float>(i / 20u) * 1.5f,
                0.0f
            };
            world.addComponent(entity, transform);
            world.addComponent(entity, WorldTextComponent{
                "Bench " + std::to_string(i),
                fontPath,
                0.08f,
                {1.0f, 1.0f, 1.0f, 1.0f}
            });
            world.addComponent(entity, BoundsComponent{});
        }
    }

    void createRuntimeParticles(ECSWorld& world,
                                const RenderingBenchmarkConfig& config)
    {
        for (uint32_t i = 0; i < config.particleEmitterCount; ++i)
        {
            Entity entity = world.createEntity();
            TransformComponent transform;
            transform.position = {
                -15.0f + static_cast<float>(i % 10u) * 3.0f,
                -12.0f,
                static_cast<float>(i / 10u) * 2.0f
            };
            world.addComponent(entity, transform);
            ParticleEmitterComponent emitter;
            emitter.effectPath.clear();
            emitter.reloadFromEffect = false;
            emitter.randomSeed = config.seed + i;
            emitter.emissionRate = 24.0f;
            emitter.maxParticles = 96;
            emitter.texturePath =
                std::string(GraphicsConstants::ENGINE_RESOURCES) + "/textures/engine_particle_fallback.png";
            world.addComponent(entity, emitter);
        }
    }

    RuntimeBenchmarkWorld populateRuntimeBenchmarkWorld(ECSWorld& world,
                                                        const RenderingBenchmarkConfig& config)
    {
        RuntimeBenchmarkWorld generated;
        createRuntimeCamera(world, config);
        createRuntimeLights(world, config, generated);
        createRuntimeRenderables(world, config, generated);
        attachRuntimeHierarchy(world, generated, config);
        createRuntimeWorldText(world, config);
        createRuntimeParticles(world, config);
        return generated;
    }

    void mutateRuntimeBenchmarkWorld(ECSWorld& world,
                                     RuntimeBenchmarkWorld& generated,
                                     const RenderingBenchmarkConfig& config,
                                     uint64_t frameIndex)
    {
        const uint32_t movingObjects =
            std::min<uint32_t>(config.movingObjectCount, static_cast<uint32_t>(generated.renderables.size()));
        for (uint32_t i = 0; i < movingObjects; ++i)
        {
            Entity entity = generated.renderables[i];
            if (!world.hasComponent<TransformComponent>(entity))
                continue;

            TransformComponent& transform = world.getComponent<TransformComponent>(entity);
            const float phase = static_cast<float>(frameIndex) * 0.031f + static_cast<float>(i) * 0.13f;
            transform.position.x += std::sin(phase) * 0.015f;
            transform.position.y += std::cos(phase) * 0.010f;
            gts::transform::markDirty(world, entity);
        }

        const uint32_t movingLights =
            std::min<uint32_t>(config.movingLightCount, static_cast<uint32_t>(generated.movingLights.size()));
        for (uint32_t i = 0; i < movingLights; ++i)
        {
            Entity entity = generated.movingLights[i];
            if (!world.hasComponent<TransformComponent>(entity))
                continue;
            TransformComponent& transform = world.getComponent<TransformComponent>(entity);
            const float phase = static_cast<float>(frameIndex) * 0.021f + static_cast<float>(i);
            transform.position.x += std::sin(phase) * 0.04f;
            transform.position.z += std::cos(phase) * 0.04f;
            gts::transform::markDirty(world, entity);
        }

        gts::rendering::MaterialRuntime& runtime = gts::rendering::materialRuntime(world);
        const uint32_t materialMutations =
            std::min<uint32_t>(config.materialMutationCountPerFrame,
                               static_cast<uint32_t>(generated.materials.size()));
        for (uint32_t i = 0; i < materialMutations; ++i)
        {
            MaterialInstanceHandle handle =
                generated.materials[(frameIndex + i) % generated.materials.size()];
            runtime.modifyInstance(
                handle,
                [&](MaterialInstance& instance)
                {
                    const float phase = static_cast<float>((frameIndex + i * 13u) % 180u) / 180.0f;
                    instance.baseColor.r = 0.25f + 0.70f * phase;
                    instance.baseColor.g = 0.85f - 0.45f * phase;
                    instance.baseColor.b = 0.40f + 0.25f * std::sin(phase * 6.2831853f);
                    return true;
                });
        }

        const uint32_t topologyMutations =
            std::min<uint32_t>(config.topologyMutationCountPerFrame,
                               static_cast<uint32_t>(generated.materials.size()));
        for (uint32_t i = 0; i < topologyMutations; ++i)
        {
            MaterialInstanceHandle handle =
                generated.materials[(frameIndex + i * 3u) % generated.materials.size()];
            runtime.modifyInstance(
                handle,
                [&](MaterialInstance& instance)
                {
                    instance.renderState.alphaMode =
                        instance.renderState.alphaMode == MaterialAlphaMode::Opaque
                            ? MaterialAlphaMode::Blend
                            : MaterialAlphaMode::Opaque;
                    instance.renderState.depthWrite =
                        instance.renderState.alphaMode != MaterialAlphaMode::Blend;
                    return true;
                });
        }

        const uint32_t dynamicMeshes =
            std::min<uint32_t>(config.dynamicMeshMutationCountPerFrame,
                               std::min<uint32_t>(config.dynamicMeshCount,
                                                  static_cast<uint32_t>(generated.renderables.size())));
        for (uint32_t i = 0; i < dynamicMeshes; ++i)
        {
            Entity entity = generated.renderables[i];
            if (!world.hasComponent<DynamicMeshComponent>(entity))
                continue;

            DynamicMeshComponent& mesh = world.getComponent<DynamicMeshComponent>(entity);
            mesh.vertices = dynamicMeshVerticesForFrame(config, static_cast<uint32_t>(frameIndex + i));
            mesh.indices = dynamicMeshIndicesForFrame(config, static_cast<uint32_t>(frameIndex + i));
            gts::rendering::markDynamicMeshChanged(world, entity);
        }
    }

    void addSample(std::map<std::string, std::vector<double>>& samples,
                   const std::string& key,
                   double value)
    {
        samples[key].push_back(value);
    }

    void addCounter(std::map<std::string, uint64_t>& counters,
                    const std::string& key,
                    uint64_t value)
    {
        counters[key] += value;
    }

    void recordRuntimeFrameStats(RuntimeBenchmarkCollector& collector,
                                 const GtsFrameStats& stats,
                                 const std::vector<EcsSystemTimingSample>& controllerTimings)
    {
        const RenderingBenchmarkConfig& config = collector.config;

        addSample(collector.cpuSamples, "frame_cpu", stats.frameCpuMs);
        addSample(collector.cpuSamples, "controller_cpu", stats.controllerCpuMs);
        addSample(collector.cpuSamples, "simulation_cpu", stats.simulationCpuMs);
        addSample(collector.cpuSamples, "snapshot_build_cpu", stats.snapshotBuildCpuMs);
        addSample(collector.cpuSamples, "visibility_cpu", stats.visibilityCpuMs);
        addSample(collector.cpuSamples, "command_extract_cpu", stats.renderExtractCpuMs);
        addSample(collector.cpuSamples, "command_sort_cpu", stats.renderExtractSortCpuMs);
        addSample(collector.cpuSamples, "render_gpu_sync_cpu", stats.renderGpuCpuMs);
        addSample(collector.cpuSamples, "backend_frame_cpu", stats.backendFrameCpuMs);
        addSample(collector.cpuSamples, "backend_cmd_record_cpu", stats.backendCmdRecordCpuMs);
        addSample(collector.cpuSamples, "backend_queue_submit_cpu", stats.backendQueueSubmitCpuMs);
        addSample(collector.cpuSamples, "backend_present_cpu", stats.backendPresentCpuMs);
        addSample(collector.cpuSamples, "render_submit_cpu", stats.renderSubmitCpuMs);
        addSample(collector.cpuSamples, "ui_cpu", stats.uiCpuMs);

        for (const EcsSystemTimingSample& sample : controllerTimings)
        {
            std::string key(sample.name);
            if (sample.instanceIndex > 0)
            {
                key += "#";
                key += std::to_string(sample.instanceIndex);
            }
            addSample(collector.controllerSamples, key, sample.updateMs);
            addSample(collector.controllerFlushSamples, key, sample.commandFlushMs);
            collector.controllerGroups.emplace(key, ecsSystemGroupName(sample.group));
        }

        addSample(collector.controllerSubstageSamples,
                  "TransformSystem.queue_children",
                  stats.transformQueueChildrenCpuMs);
        addSample(collector.controllerSubstageSamples,
                  "TransformSystem.resolve_world",
                  stats.transformResolveWorldCpuMs);
        addSample(collector.controllerSubstageSamples,
                  "TransformSystem.publish_world_transform",
                  stats.transformPublishWorldCpuMs);
        addSample(collector.controllerSubstageSamples,
                  "RenderGpuSystem.scan_compare",
                  stats.renderGpuScanCompareCpuMs);
        addSample(collector.controllerSubstageSamples,
                  "RenderGpuSystem.model_sync_enqueue",
                  stats.renderGpuModelSyncCpuMs);
        addSample(collector.controllerSubstageSamples,
                  "ForwardRenderer.object_buffer_writes",
                  stats.backendObjectWriteCpuMs);
        addSample(collector.controllerSubstageSamples,
                  "DynamicMeshBindingSystem.candidate_discovery",
                  stats.dynamicMeshCandidateDiscoveryCpuMs);
        addSample(collector.controllerSubstageSamples,
                  "DynamicMeshBindingSystem.version_checks",
                  stats.dynamicMeshVersionCheckCpuMs);
        addSample(collector.controllerSubstageSamples,
                  "DynamicMeshBindingSystem.validation",
                  stats.dynamicMeshValidationCpuMs);
        addSample(collector.controllerSubstageSamples,
                  "DynamicMeshBindingSystem.bounds",
                  stats.dynamicMeshBoundsCpuMs);
        addSample(collector.controllerSubstageSamples,
                  "DynamicMeshBindingSystem.geometry_preparation",
                  stats.dynamicMeshGeometryPreparationCpuMs);
        addSample(collector.controllerSubstageSamples,
                  "DynamicMeshBindingSystem.temporary_copy",
                  stats.dynamicMeshTemporaryCopyCpuMs);
        addSample(collector.controllerSubstageSamples,
                  "DynamicMeshBindingSystem.resource_allocation",
                  stats.dynamicMeshResourceAllocationCpuMs);
        addSample(collector.controllerSubstageSamples,
                  "DynamicMeshBindingSystem.vertex_upload",
                  stats.dynamicMeshVertexUploadCpuMs);
        addSample(collector.controllerSubstageSamples,
                  "DynamicMeshBindingSystem.index_upload",
                  stats.dynamicMeshIndexUploadCpuMs);
        addSample(collector.controllerSubstageSamples,
                  "DynamicMeshBindingSystem.publication",
                  stats.dynamicMeshPublicationCpuMs);
        addSample(collector.controllerSubstageSamples,
                  "DynamicMeshBindingSystem.invalidation",
                  stats.dynamicMeshInvalidationCpuMs);
        addSample(collector.controllerSubstageSamples,
                  "DynamicMeshBindingSystem.cleanup",
                  stats.dynamicMeshCleanupCpuMs);

        if (stats.gpuTimingAvailable != 0)
        {
            collector.gpuTimingAvailable = true;
            collector.gpuTimingStatus = "available";
            addSample(collector.gpuSamples, "frame", stats.gpuFrameMs);
            addSample(collector.gpuSamples, "scene", stats.sceneGpuMs);
            addSample(collector.gpuSamples, "particles", stats.particleGpuMs);
            addSample(collector.gpuSamples, "ui", stats.uiGpuMs);
        }

        const uint64_t frames = 1;
        addCounter(collector.counters, "authored_renderables", config.renderableCount);
        addCounter(collector.counters, "snapshot_renderables", stats.totalObjects);
        addCounter(collector.counters, "visible_renderables", stats.visibleObjects);
        addCounter(collector.counters,
                   "culled_renderables",
                   stats.totalObjects >= stats.visibleObjects ? stats.totalObjects - stats.visibleObjects : 0u);
        addCounter(collector.counters, "transparent_renderables", 0);
        addCounter(collector.counters, "render_commands", stats.renderCommandTotalCount);
        addCounter(collector.counters, "command_cache_hits", stats.renderCommandTotalCount);
        addCounter(collector.counters, "command_updates", stats.renderCommandUpdatedCount);
        addCounter(collector.counters, "command_sorts", stats.renderCommandSortedCount);
        addCounter(collector.counters, "object_uploads", stats.objectUploadCommandCount);
        addCounter(collector.counters, "object_upload_commands", stats.objectUploadCommandCount);
        addCounter(collector.counters, "logical_object_updates", stats.renderGpuUpdatedCount);
        addCounter(collector.counters, "physical_object_buffer_writes", stats.backendObjectWrites);
        addCounter(collector.counters, "bytes_written_object_buffer", stats.backendObjectWriteBytes);
        addCounter(collector.counters, "object_write_contiguous_runs", stats.backendObjectWriteContiguousRuns);
        addCounter(collector.counters, "material_queued", stats.materialQueuedCount);
        addCounter(collector.counters, "material_synchronized", stats.materialSynchronizedCount);
        addCounter(collector.counters, "material_user_invalidations", stats.materialUserInvalidationCount);
        addCounter(collector.counters, "material_fallback_substitutions", stats.materialFallbackSubstitutionCount);
        addCounter(collector.counters, "material_reference_adds", stats.materialReferenceAddCount);
        addCounter(collector.counters, "material_reference_removes", stats.materialReferenceRemoveCount);
        addCounter(collector.counters, "material_full_scans", stats.materialFullScanCount);
        addCounter(collector.counters, "unique_referenced_materials", config.uniqueMaterialCount * frames);
        addCounter(collector.counters, "snapshot_dirty_entries", stats.snapshotDirtyCount);
        addCounter(collector.counters, "snapshot_reused_entries", stats.snapshotReusedCount);
        addCounter(collector.counters, "snapshot_static_renderables", stats.snapshotStaticCount);
        addCounter(collector.counters, "snapshot_dynamic_renderables", stats.snapshotDynamicCount);
        addCounter(collector.counters, "render_gpu_updated", stats.renderGpuUpdatedCount);
        addCounter(collector.counters, "render_gpu_total", stats.renderGpuTotalCount);
        addCounter(collector.counters, "render_gpu_version_matches", stats.renderGpuVersionMatchCount);
        addCounter(collector.counters, "render_gpu_snapshot_dirty_enqueues", stats.renderGpuSnapshotDirtyEnqueueCount);
        addCounter(collector.counters, "dynamic_mesh_queued", stats.dynamicMeshQueuedCount);
        addCounter(collector.counters, "dynamic_mesh_candidates", stats.dynamicMeshCandidateCount);
        addCounter(collector.counters, "dynamic_mesh_unchanged_skipped", stats.dynamicMeshUnchangedSkippedCount);
        addCounter(collector.counters, "dynamic_mesh_failed_version_skipped", stats.dynamicMeshFailedVersionSkippedCount);
        addCounter(collector.counters, "dynamic_mesh_changed", stats.dynamicMeshChangedCount);
        addCounter(collector.counters, "dynamic_mesh_invalid", stats.dynamicMeshInvalidCount);
        addCounter(collector.counters, "dynamic_mesh_gpu_allocations", stats.dynamicMeshGpuAllocationCount);
        addCounter(collector.counters, "dynamic_mesh_gpu_reallocations", stats.dynamicMeshGpuReallocationCount);
        addCounter(collector.counters, "dynamic_mesh_in_place_updates", stats.dynamicMeshInPlaceUpdateCount);
        addCounter(collector.counters, "dynamic_mesh_resources_recreated", stats.dynamicMeshResourceRecreatedCount);
        addCounter(collector.counters, "dynamic_mesh_renderable_invalidations", stats.dynamicMeshInvalidationCount);
        addCounter(collector.counters, "dynamic_mesh_bounds_recomputed", stats.dynamicMeshBoundsRecomputedCount);
        addCounter(collector.counters, "dynamic_mesh_normals_generated", stats.dynamicMeshNormalsGeneratedCount);
        addCounter(collector.counters, "dynamic_mesh_tangents_generated", stats.dynamicMeshTangentsGeneratedCount);
        addCounter(collector.counters, "dynamic_mesh_vertices_processed", stats.dynamicMeshVerticesProcessed);
        addCounter(collector.counters, "dynamic_mesh_indices_processed", stats.dynamicMeshIndicesProcessed);
        addCounter(collector.counters, "dynamic_mesh_cpu_bytes_copied", stats.dynamicMeshCpuBytesCopied);
        addCounter(collector.counters, "dynamic_mesh_vertex_bytes_uploaded", stats.dynamicMeshVertexBytesUploaded);
        addCounter(collector.counters, "dynamic_mesh_index_bytes_uploaded", stats.dynamicMeshIndexBytesUploaded);
        addCounter(collector.counters, "dynamic_mesh_vertex_bytes_allocated", stats.dynamicMeshVertexBytesAllocated);
        addCounter(collector.counters, "dynamic_mesh_index_bytes_allocated", stats.dynamicMeshIndexBytesAllocated);
        addCounter(collector.counters, "transform_queued", stats.transformQueuedCount);
        addCounter(collector.counters, "transform_processed", stats.transformProcessedCount);
        addCounter(collector.counters, "transform_updates", stats.transformUpdatedCount);
        addCounter(collector.counters, "particle_emitters", config.particleEmitterCount);
        addCounter(collector.counters, "world_text_blocks", config.worldTextCount);
        addCounter(collector.counters, "draw_calls", stats.drawCalls);
        addCounter(collector.counters, "pipeline_switches", stats.pipelineSwitches);
        addCounter(collector.counters, "descriptor_binds", stats.descriptorBinds);
        addCounter(collector.counters, "texture_switches", stats.textureSwitches);
        addCounter(collector.counters, "gpu_timing_available", stats.gpuTimingAvailable != 0 ? 1u : 0u);
    }

    BenchmarkRunResult finishRuntimeBenchmarkResult(const RuntimeBenchmarkCollector& collector)
    {
        BenchmarkRunResult result;
        result.config = collector.config;
        result.environment = gts::rendering::benchmarks::collectBenchmarkEnvironment(collector.config);
        result.gpuTimingSupported = collector.gpuTimingSupported;
        result.gpuTimingAvailable = collector.gpuTimingAvailable;
        result.gpuTimingStatus = collector.gpuTimingStatus;
        result.warnings = collector.warnings;

        for (auto [key, samples] : collector.cpuSamples)
            result.timingsMs[key] = gts::rendering::benchmarks::summarizeSamples(std::move(samples));
        for (auto [key, samples] : collector.gpuSamples)
            result.gpuTimingsMs[key] = gts::rendering::benchmarks::summarizeSamples(std::move(samples));
        for (auto [key, samples] : collector.controllerSamples)
            result.controllerTimingsMs[key] = gts::rendering::benchmarks::summarizeSamples(std::move(samples));
        for (auto [key, samples] : collector.controllerFlushSamples)
            result.controllerFlushTimingsMs[key] = gts::rendering::benchmarks::summarizeSamples(std::move(samples));
        for (auto [key, samples] : collector.controllerSubstageSamples)
            result.controllerSubstageTimingsMs[key] = gts::rendering::benchmarks::summarizeSamples(std::move(samples));
        result.controllerTimingGroups = collector.controllerGroups;

        result.counters = collector.counters;
        result.invariantFailures = gts::rendering::benchmarks::checkBenchmarkInvariants(result);
        return result;
    }

    class RuntimeBenchmarkScene final : public GtsScene
    {
    public:
        RuntimeBenchmarkScene(RenderingBenchmarkConfig config,
                              std::shared_ptr<RuntimeBenchmarkCollector> collector)
            : config(std::move(config))
            , collector(std::move(collector))
        {
        }

        void onLoad(EcsControllerContext& ctx, const GtsSceneTransitionData*) override
        {
            resetSceneWorld();
            gts::rendering::installRendererFeature(*this, ctx);
            generated = populateRuntimeBenchmarkWorld(ecsWorld, config);
        }

        void onUpdateSimulation(const EcsSimulationContext& ctx) override
        {
            ecsWorld.updateSimulation(ctx);
        }

        void onUpdateControllers(const EcsControllerContext& ctx) override
        {
            if (ctx.time != nullptr)
                mutateRuntimeBenchmarkWorld(ecsWorld, generated, config, ctx.time->frame);

            ecsWorld.updateControllers(ctx);
            lastControllerTimings = ecsWorld.getLastControllerTimingSamples();

            if (requestQuit && ctx.engineCommands != nullptr)
                ctx.engineCommands->requestQuit();
        }

        void onFrameStats(const GtsFrameStats& stats) override
        {
            if (!collector)
                return;

            collector->renderedFrames += 1;
            collector->gpuTimingSupported =
                collector->gpuTimingSupported || stats.gpuTimingSupported != 0;

            if (collector->renderedFrames <= config.warmupFrames)
                return;

            if (collector->measuredFrames >= config.measuredFrames)
            {
                requestQuit = true;
                return;
            }

            const bool supported = stats.gpuTimingSupported != 0;
            const bool available = stats.gpuTimingAvailable != 0;
            const uint32_t framesAfterWarmup = collector->renderedFrames - config.warmupFrames;
            const bool timedOutWaitingForGpu =
                supported && !available && framesAfterWarmup > TimestampWaitFrameBudget;

            if (supported && !available && !timedOutWaitingForGpu)
                return;

            if (!supported)
                collector->gpuTimingStatus = "Vulkan timestamp queries are unsupported by this device";
            else if (timedOutWaitingForGpu && !collector->gpuTimingAvailable)
                collector->gpuTimingStatus = "Vulkan timestamp query results were not produced";

            if (timedOutWaitingForGpu && !timeoutWarningAdded)
            {
                collector->warnings.push_back(
                    "GPU runtime continued with CPU/counter samples because timestamp results were unavailable");
                timeoutWarningAdded = true;
            }

            recordRuntimeFrameStats(*collector, stats, lastControllerTimings);
            collector->measuredFrames += 1;

            if (collector->measuredFrames >= config.measuredFrames)
                requestQuit = true;
        }

    private:
        RenderingBenchmarkConfig config;
        std::shared_ptr<RuntimeBenchmarkCollector> collector;
        RuntimeBenchmarkWorld generated;
        std::vector<EcsSystemTimingSample> lastControllerTimings;
        bool requestQuit = false;
        bool timeoutWarningAdded = false;
    };

    BenchmarkRunResult runGpuRuntimeBenchmark(const RenderingBenchmarkConfig& inputConfig)
    {
        RenderingBenchmarkConfig config = inputConfig;
        gts::rendering::benchmarks::sanitizeConfig(config);
        config.mode = BenchmarkRunMode::GpuRuntime;

        auto collector = std::make_shared<RuntimeBenchmarkCollector>();
        collector->config = config;

        EngineConfig engineConfig;
        engineConfig.frustumCullingEnabled = config.enableFrustumCulling;
        engineConfig.debugOverlayEnabledByDefault = false;
        engineConfig.engineToolsEnabled = false;
        engineConfig.graphics.headless = true;
        engineConfig.graphics.enableValidationLayers = false;
        engineConfig.graphics.renderWidth = config.renderWidth;
        engineConfig.graphics.renderHeight = config.renderHeight;
        engineConfig.graphics.window.width = static_cast<int>(config.renderWidth);
        engineConfig.graphics.window.height = static_cast<int>(config.renderHeight);
        engineConfig.graphics.window.vsync = false;
        engineConfig.graphics.presentModePreference = PresentModePreference::Immediate;
        engineConfig.graphics.maxFrameRate = 0;
        engineConfig.graphics.enableGpuTimestamps = true;
        gts::transform::TransformSystem::setDetailedMetricsEnabled(true);
        RenderGpuSystem::setDetailedMetricsEnabled(true);
        DynamicMeshBindingSystem::setDetailedMetricsEnabled(true);

        GravitasEngine engine(engineConfig);
        engine.registerScene(
            "rendering_benchmark",
            [config, collector]()
            {
                return std::make_unique<RuntimeBenchmarkScene>(config, collector);
            });
        engine.setActiveScene("rendering_benchmark");
        engine.start();

        gts::transform::TransformSystem::setDetailedMetricsEnabled(false);
        RenderGpuSystem::setDetailedMetricsEnabled(false);
        DynamicMeshBindingSystem::setDetailedMetricsEnabled(false);
        return finishRuntimeBenchmarkResult(*collector);
    }
}

int main(int argc, char** argv)
{
    using namespace gts::rendering::benchmarks;

    CliOptions options;
    if (!parseArgs(argc, argv, options))
    {
        printUsage();
        return EXIT_FAILURE;
    }

    if (options.listPresets)
    {
        for (const BenchmarkPreset& preset : standardBenchmarkPresets())
            std::cout << preset.name << " v" << preset.version << " - " << preset.description << "\n";
        return EXIT_SUCCESS;
    }

    const BenchmarkPreset* preset = findBenchmarkPreset(options.preset);
    if (preset == nullptr)
    {
        std::cerr << "unknown benchmark preset: " << options.preset << "\n";
        return EXIT_FAILURE;
    }

    RenderingBenchmarkConfig config = preset->config;
    for (const auto& [key, value] : options.overrides)
    {
        std::string error;
        if (!applyConfigOverride(config, key, value, &error))
        {
            std::cerr << error << "\n";
            return EXIT_FAILURE;
        }
    }

    BenchmarkRunResult result;
    if (config.mode == BenchmarkRunMode::GpuRuntime)
    {
        try
        {
            result = runGpuRuntimeBenchmark(config);
        }
        catch (const std::exception& ex)
        {
            std::cerr << "gpu_runtime skipped: " << ex.what() << "\n";
            return GpuBenchmarkSkipExitCode;
        }
    }
    else
    {
        result = runRenderingBenchmark(config);
    }

    const std::vector<std::string> validationFailures = validateBenchmarkResult(result);
    if (!validationFailures.empty())
    {
        std::cerr << "benchmark result validation failed:\n";
        for (const std::string& failure : validationFailures)
            std::cerr << "  - " << failure << "\n";
        return EXIT_FAILURE;
    }

    if (!options.outputPath.empty())
    {
        std::string error;
        if (!writeBenchmarkResultJson(result, options.outputPath, &error))
        {
            std::cerr << error << "\n";
            return EXIT_FAILURE;
        }
    }

    if (!options.quiet)
        std::cout << benchmarkResultSummary(result);

    if (options.checkInvariants && !result.invariantFailures.empty())
    {
        std::cerr << "benchmark invariants failed:\n";
        for (const std::string& failure : result.invariantFailures)
            std::cerr << "  - " << failure << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
