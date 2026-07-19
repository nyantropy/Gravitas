#include "RenderingBenchmark.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <limits>
#include <numeric>
#include <optional>
#include <random>
#include <sstream>
#include <thread>
#include <unordered_map>

#include "BitmapFont.h"
#include "BoundsComponent.h"
#include "CameraDescriptionComponent.h"
#include "DirectionalLightComponent.h"
#include "DynamicMeshComponent.h"
#include "DynamicMeshBindingSystem.hpp"
#include "ECSWorld.hpp"
#include "EcsControllerContext.hpp"
#include "FrustumCullingStrategy.h"
#include "GraphicsConstants.h"
#include "IResourceProvider.hpp"
#include "MaterialBindingSystem.hpp"
#include "MaterialReferenceComponent.h"
#include "MaterialRuntime.h"
#include "ParticleEmitterComponent.h"
#include "ParticleEmitterRuntimeComponent.h"
#include "ParticleEmitterSystem.hpp"
#include "ParticleFrameData.h"
#include "PointLightComponent.h"
#include "RenderCommandExtractor.hpp"
#include "RenderExtractionSnapshotBuilder.hpp"
#include "RenderGpuSystem.hpp"
#include "RenderPipeline.h"
#include "RendererCameraSceneFeature.h"
#include "RendererGeometrySceneFeature.h"
#include "RendererParticleSceneFeature.h"
#include "SpotLightComponent.h"
#include "StaticMeshComponent.h"
#include "TimeContext.h"
#include "TransformComponent.h"
#include "TransformDirtyHelpers.h"
#include "TransformHierarchyHelpers.h"
#include "TransformSceneFeature.h"
#include "TransformSystem.hpp"
#include "Vertex.h"
#include "WorldTextComponent.h"

namespace gts::rendering::benchmarks
{
    namespace
    {
        constexpr uint32_t MaxBenchmarkRenderables = GraphicsConstants::MAX_RENDERABLE_OBJECTS - 1u;
        constexpr uint32_t MaxBenchmarkMaterials = 60000;
        constexpr uint32_t MaxBenchmarkMeshes = 4096;
        constexpr uint32_t MaxBenchmarkLights = 20000;
        constexpr uint32_t MaxBenchmarkParticleEmitters = 4096;
        constexpr uint32_t MaxBenchmarkParticlesPerEmitter = 2048;
        constexpr float FixedDeltaSeconds = 1.0f / 60.0f;
        constexpr uint32_t GtsScene3CubeCount = 64000;
        constexpr uint32_t GtsScene3GridColumns = 40;
        constexpr uint32_t GtsScene3GridRows = 40;
        constexpr uint32_t GtsScene3GridLayers = 40;
        constexpr float GtsScene3GridSpacing = 2.5f;

        struct BenchmarkResourceProvider : IResourceProvider
        {
            mesh_id_type nextMesh = 100;
            texture_id_type nextTexture = 200;
            ssbo_id_type nextSlot = 0;
            font_id_type nextFont = 300;
            view_id_type nextView = 400;

            uint64_t meshRequests = 0;
            uint64_t textureRequests = 0;
            uint64_t proceduralMeshUploads = 0;
            uint64_t releasedProceduralMeshes = 0;
            uint64_t objectSlotRequests = 0;
            uint64_t objectSlotReleases = 0;
            uint64_t cameraBufferRequests = 0;
            uint64_t cameraBufferReleases = 0;

            std::unordered_map<std::string, mesh_id_type> meshes;
            std::unordered_map<std::string, texture_id_type> textures;
            std::unordered_map<std::string, font_id_type> fontsByPath;
            std::unordered_map<font_id_type, BitmapFont> fonts;

            mesh_id_type requestMesh(const std::string& path) override
            {
                ++meshRequests;
                auto [it, inserted] = meshes.emplace(path, nextMesh);
                if (inserted)
                    ++nextMesh;
                return it->second;
            }

            mesh_id_type getSharedQuadMesh(float width, float height) override
            {
                std::ostringstream key;
                key << "__quad/" << width << "x" << height;
                return requestMesh(key.str());
            }

            mesh_id_type uploadProceduralMesh(mesh_id_type existingId,
                                              const std::vector<Vertex>&,
                                              const std::vector<uint32_t>&,
                                              VertexAttributeFlags = UnlitVertexAttributes) override
            {
                ++proceduralMeshUploads;
                if (existingId != 0)
                    return existingId;
                return nextMesh++;
            }

            void releaseProceduralMesh(mesh_id_type) override
            {
                ++releasedProceduralMeshes;
            }

            texture_id_type requestTexture(const std::string& path) override
            {
                return requestTexture(path, TextureColorSpace::SRgb);
            }

            texture_id_type requestTexture(const std::string& path, TextureColorSpace colorSpace) override
            {
                ++textureRequests;
                std::string key = path;
                key += colorSpace == TextureColorSpace::SRgb ? ":srgb" : ":linear";
                auto [it, inserted] = textures.emplace(key, nextTexture);
                if (inserted)
                    ++nextTexture;
                return it->second;
            }

            texture_id_type requestMaterialFallbackTexture(MaterialTextureRole role) override
            {
                return requestTexture(std::string("__fallback/") + fallbackTextureNameForRole(role),
                                      textureColorSpaceForRole(role));
            }

            texture_id_type requestClampedTexture(const std::string& path) override
            {
                return requestTexture(path);
            }

            texture_id_type requestPixelTexture(const std::string& path) override
            {
                return requestTexture(path);
            }

            TextureDimensions getTextureDimensions(texture_id_type) const override
            {
                return {16, 16};
            }

            font_id_type requestFont(const std::string& path) override
            {
                auto [it, inserted] = fontsByPath.emplace(path, nextFont);
                if (inserted)
                {
                    BitmapFont font;
                    font.atlasTexture = 900 + nextFont;
                    font.lineHeight = 12.0f;
                    font.glyphs['B'] = GlyphInfo{
                        {0.0f, 0.0f},
                        {1.0f, 1.0f},
                        {8.0f, 10.0f},
                        {0.0f, 10.0f},
                        8.0f
                    };
                    font.glyphs['e'] = font.glyphs['B'];
                    font.glyphs['n'] = font.glyphs['B'];
                    font.glyphs['c'] = font.glyphs['B'];
                    font.glyphs['h'] = font.glyphs['B'];
                    font.glyphs[' '] = font.glyphs['B'];
                    fonts.emplace(nextFont, font);
                    ++nextFont;
                }
                return it->second;
            }

            const BitmapFont* getFont(font_id_type id) const override
            {
                auto it = fonts.find(id);
                return it == fonts.end() ? nullptr : &it->second;
            }

            view_id_type requestCameraBuffer() override
            {
                ++cameraBufferRequests;
                return nextView++;
            }

            void releaseCameraBuffer(view_id_type) override
            {
                ++cameraBufferReleases;
            }

            void uploadCameraView(view_id_type, const glm::mat4&, const glm::mat4&) override
            {
            }

            ssbo_id_type requestObjectSlot() override
            {
                ++objectSlotRequests;
                return nextSlot++;
            }

            void releaseObjectSlot(ssbo_id_type) override
            {
                ++objectSlotReleases;
            }
        };

        struct BenchmarkSceneState
        {
            ECSWorld world;
            BenchmarkResourceProvider resources;
            RenderPipeline pipeline{std::make_unique<FrustumCullingStrategy>(true)};
            std::vector<Entity> renderables;
            std::vector<Entity> movingLights;
            std::vector<MaterialInstanceHandle> materials;
            uint64_t previousMeshRequests = 0;
            uint64_t previousTextureRequests = 0;
            uint64_t previousProceduralUploads = 0;
            uint64_t previousObjectSlotRequests = 0;
            uint64_t previousCameraBufferRequests = 0;
        };

        uint32_t parseUnsigned(std::string_view text, bool& ok)
        {
            ok = false;
            if (text.empty())
                return 0;

            uint64_t value = 0;
            for (char c : text)
            {
                if (c < '0' || c > '9')
                    return 0;
                value = value * 10u + static_cast<uint32_t>(c - '0');
                if (value > std::numeric_limits<uint32_t>::max())
                    return 0;
            }

            ok = true;
            return static_cast<uint32_t>(value);
        }

        bool parseBool(std::string_view text, bool& ok)
        {
            ok = true;
            if (text == "1" || text == "true" || text == "yes" || text == "on")
                return true;
            if (text == "0" || text == "false" || text == "no" || text == "off")
                return false;
            ok = false;
            return false;
        }

        double parseDouble(std::string_view text, bool& ok)
        {
            ok = false;
            if (text.empty())
                return 0.0;

            std::string owned(text);
            char* end = nullptr;
            const double value = std::strtod(owned.c_str(), &end);
            if (end == owned.c_str() || end == nullptr || *end != '\0' || !std::isfinite(value))
                return 0.0;

            ok = true;
            return value;
        }

        double percentile(const std::vector<double>& sorted, double fraction)
        {
            if (sorted.empty())
                return 0.0;
            if (sorted.size() == 1)
                return sorted.front();

            const double rank = fraction * static_cast<double>(sorted.size() - 1u);
            const auto lo = static_cast<size_t>(std::floor(rank));
            const auto hi = static_cast<size_t>(std::ceil(rank));
            if (lo == hi)
                return sorted[lo];

            const double weight = rank - static_cast<double>(lo);
            return sorted[lo] * (1.0 - weight) + sorted[hi] * weight;
        }

        std::string escapedJson(std::string_view value)
        {
            std::ostringstream out;
            for (char c : value)
            {
                switch (c)
                {
                    case '\\': out << "\\\\"; break;
                    case '"': out << "\\\""; break;
                    case '\n': out << "\\n"; break;
                    case '\r': out << "\\r"; break;
                    case '\t': out << "\\t"; break;
                    default: out << c; break;
                }
            }
            return out.str();
        }

        std::string cpuModel()
        {
#if defined(__linux__)
            std::ifstream file("/proc/cpuinfo");
            std::string line;
            while (std::getline(file, line))
            {
                const std::string key = "model name";
                if (line.rfind(key, 0) == 0)
                {
                    const size_t colon = line.find(':');
                    if (colon != std::string::npos)
                    {
                        size_t start = colon + 1u;
                        while (start < line.size() && line[start] == ' ')
                            ++start;
                        return line.substr(start);
                    }
                }
            }
#endif
            return "unknown";
        }

        std::string compilerName()
        {
#if defined(__clang__)
            return std::string("clang ") + __clang_version__;
#elif defined(__GNUC__)
            return std::string("gcc ") + std::to_string(__GNUC__) + "." +
                std::to_string(__GNUC_MINOR__) + "." + std::to_string(__GNUC_PATCHLEVEL__);
#elif defined(_MSC_VER)
            return std::string("msvc ") + std::to_string(_MSC_VER);
#else
            return "unknown";
#endif
        }

        std::string osName()
        {
#if defined(_WIN32)
            return "windows";
#elif defined(__APPLE__)
            return "macos";
#elif defined(__linux__)
            return "linux";
#else
            return "unknown";
#endif
        }

        bool isGtsScene3StressPreset(const RenderingBenchmarkConfig& config)
        {
            return config.presetName == "gtsscene3_64k_moving_cubes" ||
                config.presetName == "moving_64k_independent" ||
                config.presetName == "static_64k_control";
        }

        glm::vec3 gtsScene3CubePosition(uint32_t index)
        {
            const uint32_t x = index % GtsScene3GridColumns;
            const uint32_t y = (index / GtsScene3GridColumns) % GtsScene3GridRows;
            const uint32_t z = index / (GtsScene3GridColumns * GtsScene3GridRows);

            const float halfColumns = (static_cast<float>(GtsScene3GridColumns) - 1.0f) * 0.5f;
            const float halfRows = (static_cast<float>(GtsScene3GridRows) - 1.0f) * 0.5f;
            const float halfLayers = (static_cast<float>(GtsScene3GridLayers) - 1.0f) * 0.5f;

            return {
                (static_cast<float>(x) - halfColumns) * GtsScene3GridSpacing,
                (static_cast<float>(y) - halfRows) * GtsScene3GridSpacing,
                (static_cast<float>(z) - halfLayers) * GtsScene3GridSpacing
            };
        }

        std::string gtsScene3TexturePath(uint32_t index)
        {
            static constexpr const char* TextureNames[] = {
                "engine_demo_cool_stone.png",
                "engine_demo_crystal_stone.png",
                "engine_demo_moss_floor.png",
                "engine_demo_neutral_stone.png",
                "engine_demo_lantern_stone.png",
                "engine_demo_arcane_stone.png",
                "engine_demo_rune_stone.png",
                "engine_demo_gilt_stone.png"
            };
            return std::string(GraphicsConstants::ENGINE_RESOURCES) + "/textures/" +
                TextureNames[index % (sizeof(TextureNames) / sizeof(TextureNames[0]))];
        }

        std::string buildTypeName()
        {
#if defined(NDEBUG)
            return "Release";
#else
            return "Debug";
#endif
        }

        BenchmarkEnvironment collectEnvironment(const RenderingBenchmarkConfig& config)
        {
            BenchmarkEnvironment env;
            const char* commit = std::getenv("GTS_BENCHMARK_COMMIT");
            env.values["engine_commit"] = commit != nullptr ? commit : "unknown";
            env.values["build_type"] = buildTypeName();
            env.values["compiler"] = compilerName();
            env.values["operating_system"] = osName();
            env.values["cpu"] = cpuModel();
            env.values["logical_core_count"] = std::to_string(std::thread::hardware_concurrency());
            env.values["gpu"] = config.mode == BenchmarkRunMode::CpuSmoke
                ? "unavailable in cpu_smoke mode"
                : "not queried";
            env.values["graphics_driver"] = "unavailable";
            env.values["vulkan_api_version"] = "unavailable";
            env.values["render_resolution"] =
                std::to_string(config.renderWidth) + "x" + std::to_string(config.renderHeight);
            env.values["vsync"] = "disabled";
            env.values["frame_rate_cap"] = "disabled";
            env.values["benchmark_seed"] = std::to_string(config.seed);
            env.values["preset_version"] = std::to_string(config.presetVersion);
            env.values["mode"] = benchmarkRunModeName(config.mode);
            return env;
        }

        std::vector<Vertex> benchmarkTriangleVertices(uint32_t frame)
        {
            const float offset = static_cast<float>(frame % 17u) * 0.001f;
            return {
                Vertex{{-0.5f, -0.5f + offset, 0.0f}, {0.0f, 0.0f, 1.0f},
                       {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
                Vertex{{ 0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f},
                       {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
                Vertex{{ 0.0f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f},
                       {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.5f, 1.0f}}
            };
        }

        std::vector<Vertex> benchmarkQuadVertices(uint32_t frame)
        {
            const float offset = static_cast<float>(frame % 17u) * 0.001f;
            const glm::vec3 normal = {0.0f, 0.0f, 1.0f};
            const glm::vec4 tangent = {1.0f, 0.0f, 0.0f, 1.0f};
            const glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};
            return {
                Vertex{{-0.5f, -0.5f + offset, 0.0f}, normal, tangent, color, {0.0f, 0.0f}},
                Vertex{{ 0.5f, -0.5f,          0.0f}, normal, tangent, color, {1.0f, 0.0f}},
                Vertex{{ 0.5f,  0.5f,          0.0f}, normal, tangent, color, {1.0f, 1.0f}},
                Vertex{{-0.5f,  0.5f,          0.0f}, normal, tangent, color, {0.0f, 1.0f}}
            };
        }

        std::vector<Vertex> benchmarkDoubleQuadVertices(uint32_t frame)
        {
            std::vector<Vertex> vertices = benchmarkQuadVertices(frame);
            std::vector<Vertex> second = benchmarkQuadVertices(frame + 3u);
            for (Vertex& vertex : second)
                vertex.pos.x += 0.8f;
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
                ? UnlitVertexAttributes
                : StandardVertexAttributes;
        }

        std::vector<Vertex> dynamicMeshVerticesForFrame(const RenderingBenchmarkConfig& config,
                                                        uint32_t frame)
        {
            if (dynamicMeshGrowthExpanded(config, frame))
                return benchmarkDoubleQuadVertices(frame);
            return benchmarkTriangleVertices(frame);
        }

        std::vector<uint32_t> dynamicMeshIndicesForFrame(const RenderingBenchmarkConfig& config,
                                                         uint32_t frame)
        {
            if (dynamicMeshGrowthExpanded(config, frame))
                return {0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7};
            return {0, 1, 2};
        }

        void installBenchmarkFeatures(BenchmarkSceneState& state, const RenderingBenchmarkConfig& config)
        {
            gts::transform::installTransformRuntime(state.world);
            installRendererGeometrySceneFeature(state.world, &state.resources);
            installRendererCameraSceneFeature(state.world, &state.resources);
            installRendererParticleSceneFeature(state.world);
            state.pipeline.setVisibilityEnabled(config.enableFrustumCulling);
        }

        MaterialInstanceHandle createBenchmarkMaterial(BenchmarkSceneState& state,
                                                       const RenderingBenchmarkConfig& config,
                                                       uint32_t index)
        {
            MaterialRuntime& runtime = materialRuntime(state.world);
            if (isGtsScene3StressPreset(config))
            {
                MaterialInstance instance;
                if (const MaterialInstance* defaultMaterial = runtime.getInstance(runtime.defaultMaterial()))
                    instance.definition = defaultMaterial->definition;
                instance.baseColorTexture = MaterialTextureBinding::assetPath(gtsScene3TexturePath(index));
                instance.renderState.blendMode = MaterialBlendMode::Alpha;
                instance.renderState.alphaMode =
                    alphaModeForBlendMode(MaterialBlendMode::Alpha, instance.baseColor.a, true);
                return runtime.createInstance(instance);
            }

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
                    if (config.enableNormalMaps && config.enablePbr)
                        instance.normalTexture = MaterialTextureBinding::dataAssetPath(
                            "benchmarks/generated_normal_" + std::to_string(index % 8u) + ".png");
                    return true;
                });
            return handle;
        }

        void createBenchmarkCamera(BenchmarkSceneState& state, const RenderingBenchmarkConfig& config)
        {
            Entity camera = state.world.createEntity();
            TransformComponent transform;
            transform.position = isGtsScene3StressPreset(config)
                ? glm::vec3(0.0f, 35.0f, 120.0f)
                : glm::vec3(0.0f, 0.0f, 80.0f);
            state.world.addComponent(camera, transform);
            CameraDescriptionComponent cameraDesc;
            cameraDesc.active = true;
            if (isGtsScene3StressPreset(config))
                cameraDesc.fov = glm::radians(70.0f);
            cameraDesc.target = {0.0f, 0.0f, 0.0f};
            cameraDesc.aspectRatio = config.renderHeight == 0
                ? 1.0f
                : static_cast<float>(config.renderWidth) / static_cast<float>(config.renderHeight);
            cameraDesc.farClip = 500.0f;
            state.world.addComponent(camera, cameraDesc);
        }

        void createBenchmarkLights(BenchmarkSceneState& state, const RenderingBenchmarkConfig& config)
        {
            for (uint32_t i = 0; i < config.directionalLightCount; ++i)
            {
                Entity light = state.world.createEntity();
                TransformComponent transform;
                transform.rotation = {0.2f + 0.05f * static_cast<float>(i), 0.6f, 0.0f};
                state.world.addComponent(light, transform);
                DirectionalLightComponent desc;
                desc.active = true;
                desc.priority = static_cast<int>(config.directionalLightCount - i);
                desc.intensity = 1.0f + 0.1f * static_cast<float>(i % 4u);
                state.world.addComponent(light, desc);
                if (state.movingLights.size() < config.movingLightCount)
                    state.movingLights.push_back(light);
            }

            for (uint32_t i = 0; i < config.pointLightCount; ++i)
            {
                Entity light = state.world.createEntity();
                TransformComponent transform;
                const float angle = static_cast<float>(i) * 0.37f;
                transform.position = {std::cos(angle) * 25.0f, std::sin(angle * 0.7f) * 6.0f,
                                      std::sin(angle) * 25.0f};
                state.world.addComponent(light, transform);
                PointLightComponent desc;
                desc.priority = static_cast<int>(i % 7u);
                desc.range = 30.0f;
                desc.intensity = 2.0f;
                state.world.addComponent(light, desc);
                if (state.movingLights.size() < config.movingLightCount)
                    state.movingLights.push_back(light);
            }

            for (uint32_t i = 0; i < config.spotLightCount; ++i)
            {
                Entity light = state.world.createEntity();
                TransformComponent transform;
                const float angle = static_cast<float>(i) * 0.51f;
                transform.position = {std::cos(angle) * 18.0f, 10.0f, std::sin(angle) * 18.0f};
                transform.rotation = {0.7f, angle, 0.0f};
                state.world.addComponent(light, transform);
                SpotLightComponent desc;
                desc.priority = static_cast<int>(i % 9u);
                desc.range = 28.0f;
                desc.intensity = 3.0f;
                state.world.addComponent(light, desc);
                if (state.movingLights.size() < config.movingLightCount)
                    state.movingLights.push_back(light);
            }
        }

        void createBenchmarkRenderables(BenchmarkSceneState& state, const RenderingBenchmarkConfig& config)
        {
            MaterialRuntime& runtime = materialRuntime(state.world);
            state.materials.reserve(config.uniqueMaterialCount);
            for (uint32_t i = 0; i < config.uniqueMaterialCount; ++i)
                state.materials.push_back(createBenchmarkMaterial(state, config, i));

            if (state.materials.empty())
                state.materials.push_back(runtime.defaultStandardSurfaceMaterial());

            std::mt19937 rng(config.seed);
            std::uniform_real_distribution<float> jitter(-0.35f, 0.35f);
            const uint32_t gridSide = static_cast<uint32_t>(
                std::ceil(std::sqrt(static_cast<double>(std::max(1u, config.renderableCount)))));
            const uint32_t requestedVisible = config.visibleRenderableCount == 0
                ? config.renderableCount
                : std::min(config.visibleRenderableCount, config.renderableCount);
            const bool gtsScene3Stress = isGtsScene3StressPreset(config);
            const std::string gtsScene3CubeMesh =
                std::string(GraphicsConstants::ENGINE_RESOURCES) + "/models/cube.gmesh";

            for (uint32_t i = 0; i < config.renderableCount; ++i)
            {
                Entity entity = state.world.createEntity();
                TransformComponent transform;
                if (gtsScene3Stress)
                {
                    transform.position = gtsScene3CubePosition(i);
                    transform.scale = glm::vec3(1.0f);
                }
                else
                {
                    const uint32_t x = i % gridSide;
                    const uint32_t y = i / gridSide;
                    const bool shouldBeVisible = i < requestedVisible;
                    transform.position = {
                        (static_cast<float>(x) - static_cast<float>(gridSide) * 0.5f) * 2.8f + jitter(rng),
                        (static_cast<float>(y) - static_cast<float>(gridSide) * 0.5f) * 2.8f + jitter(rng),
                        shouldBeVisible ? jitter(rng) : 900.0f + static_cast<float>(i)
                    };
                }
                state.world.addComponent(entity, transform);

                if (i < config.dynamicMeshCount)
                {
                    DynamicMeshComponent dynamicMesh;
                    dynamicMesh.vertices = dynamicMeshVerticesForFrame(config, 0);
                    dynamicMesh.indices = dynamicMeshIndicesForFrame(config, 0);
                    dynamicMesh.geometryVersion = 1;
                    dynamicMesh.sourceAttributes = dynamicMeshSourceAttributes(config);
                    state.world.addComponent(entity, dynamicMesh);
                }
                else
                {
                    StaticMeshComponent mesh;
                    mesh.meshPath = gtsScene3Stress
                        ? gtsScene3CubeMesh
                        : "benchmarks/generated_mesh_" +
                            std::to_string(i % std::max(1u, config.uniqueMeshCount)) + ".mesh";
                    state.world.addComponent(entity, mesh);
                }

                state.world.addComponent(entity, MaterialReferenceComponent{
                    state.materials[i % state.materials.size()]
                });
                state.world.addComponent(entity, BoundsComponent{});
                state.renderables.push_back(entity);
            }
        }

        bool isDeepHierarchyPreset(const RenderingBenchmarkConfig& config)
        {
            return config.presetName == "moving_deep_hierarchy" ||
                config.presetName == "deep_hierarchy";
        }

        bool isWideHierarchyPreset(const RenderingBenchmarkConfig& config)
        {
            return config.presetName == "moving_wide_hierarchy" ||
                config.presetName == "moving_64k_wide_hierarchy";
        }

        void attachBenchmarkHierarchy(BenchmarkSceneState& state, const RenderingBenchmarkConfig& config)
        {
            if ((!isDeepHierarchyPreset(config) && !isWideHierarchyPreset(config)) ||
                state.renderables.size() < 2)
            {
                return;
            }

            const uint32_t movingRoots =
                std::clamp<uint32_t>(config.movingObjectCount, 1u, static_cast<uint32_t>(state.renderables.size()));
            const uint32_t rootSide = static_cast<uint32_t>(
                std::ceil(std::sqrt(static_cast<double>(movingRoots))));
            for (uint32_t i = 0; i < movingRoots; ++i)
            {
                TransformComponent& transform = state.world.getComponent<TransformComponent>(state.renderables[i]);
                const uint32_t x = i % rootSide;
                const uint32_t y = i / rootSide;
                transform.position = {
                    (static_cast<float>(x) - static_cast<float>(rootSide) * 0.5f) * 2.0f,
                    (static_cast<float>(y) - static_cast<float>(rootSide) * 0.5f) * 2.0f,
                    0.0f
                };
                gts::transform::markDirty(state.world, state.renderables[i]);
            }

            if (isDeepHierarchyPreset(config))
            {
                for (uint32_t i = movingRoots; i < state.renderables.size(); ++i)
                {
                    Entity child = state.renderables[i];
                    Entity parent = state.renderables[i - movingRoots];
                    TransformComponent& transform = state.world.getComponent<TransformComponent>(child);
                    transform.position = {0.0f, 0.85f, 0.0f};
                    gts::transform::attachToParent(state.world, child, parent);
                }
                return;
            }

            for (uint32_t i = movingRoots; i < state.renderables.size(); ++i)
            {
                Entity child = state.renderables[i];
                Entity parent = state.renderables[i % movingRoots];
                const uint32_t childOrdinal = i / movingRoots;
                const float angle = static_cast<float>(i % movingRoots) * 0.37f +
                    static_cast<float>(childOrdinal) * 0.19f;
                TransformComponent& transform = state.world.getComponent<TransformComponent>(child);
                transform.position = {
                    std::cos(angle) * (1.1f + 0.12f * static_cast<float>(childOrdinal % 5u)),
                    std::sin(angle) * (1.1f + 0.12f * static_cast<float>(childOrdinal % 5u)),
                    0.0f
                };
                gts::transform::attachToParent(state.world, child, parent);
            }
        }

        void createBenchmarkWorldText(BenchmarkSceneState& state, const RenderingBenchmarkConfig& config)
        {
            for (uint32_t i = 0; i < config.worldTextCount; ++i)
            {
                Entity entity = state.world.createEntity();
                TransformComponent transform;
                transform.position = {
                    -20.0f + static_cast<float>(i % 20u) * 2.0f,
                    16.0f + static_cast<float>(i / 20u) * 1.5f,
                    0.0f
                };
                state.world.addComponent(entity, transform);
                state.world.addComponent(entity, WorldTextComponent{
                    "Bench " + std::to_string(i),
                    "resources/fonts/benchmark.fnt",
                    0.08f,
                    {1.0f, 1.0f, 1.0f, 1.0f}
                });
                state.world.addComponent(entity, BoundsComponent{});
            }
        }

        void configureParticleBenchmarkBudget(ECSWorld& world, const RenderingBenchmarkConfig& config)
        {
            if (config.particleMaxSimulatedParticles == 0u &&
                config.particleMaxRenderedParticles == 0u &&
                config.particleMaxSpawnedPerFrame == 0u)
            {
                return;
            }

            if (!world.hasAny<ParticleBudgetComponent>())
                world.createSingleton<ParticleBudgetComponent>();

            ParticleBudgetState& budget = world.getSingleton<ParticleBudgetComponent>().state;
            budget.maxSimulatedParticles = config.particleMaxSimulatedParticles;
            budget.maxRenderedParticles = config.particleMaxRenderedParticles;
            budget.maxSpawnedPerFrame = config.particleMaxSpawnedPerFrame;
        }

        void createBenchmarkParticles(BenchmarkSceneState& state, const RenderingBenchmarkConfig& config)
        {
            for (uint32_t i = 0; i < config.particleEmitterCount; ++i)
            {
                Entity entity = state.world.createEntity();
                TransformComponent transform;
                transform.position = {
                    -15.0f + static_cast<float>(i % 10u) * 3.0f,
                    -12.0f,
                    static_cast<float>(i / 10u) * 2.0f
                };
                state.world.addComponent(entity, transform);
                ParticleEmitterComponent emitter;
                emitter.effectPath.clear();
                emitter.reloadFromEffect = false;
                emitter.randomSeed = config.seed + i;
                emitter.emissionRate = static_cast<float>(config.particleEmissionRate);
                emitter.maxParticles = config.particleMaxParticles;
                emitter.texturePath = "benchmarks/generated_particle.png";
                emitter.shape = i % 3u == 0u ? ParticleEmitterShape::Ring : ParticleEmitterShape::Box;
                emitter.blend = ParticleBlendMode::Additive;
                emitter.lifetimeMin = 0.85f;
                emitter.lifetimeMax = 1.85f;
                emitter.tangentVelocity = i % 3u == 0u ? 0.42f : 0.18f;
                emitter.velocitySpread = 0.10f;
                emitter.drag = 0.04f;
                emitter.baseTint = {0.62f, 0.72f, 1.0f, 0.82f};
                if (config.presetName == "submit_particle_draw_pressure")
                {
                    ParticleBurst burst;
                    burst.time = 0.0f;
                    burst.countMin = 16;
                    burst.countMax = 16;
                    emitter.bursts.push_back(burst);
                }

                if (i < config.particleMeshEmitterCount)
                {
                    emitter.primitive = ParticlePrimitive::Mesh;
                    emitter.meshPath = "benchmarks/generated_particle_mesh.gmesh";
                    emitter.meshScale = {0.18f, 0.18f, 0.18f};
                }

                state.world.addComponent(entity, emitter);
            }
        }

        void generateScene(BenchmarkSceneState& state, const RenderingBenchmarkConfig& config)
        {
            installBenchmarkFeatures(state, config);
            configureParticleBenchmarkBudget(state.world, config);
            createBenchmarkCamera(state, config);
            createBenchmarkLights(state, config);
            createBenchmarkRenderables(state, config);
            attachBenchmarkHierarchy(state, config);
            createBenchmarkWorldText(state, config);
            createBenchmarkParticles(state, config);
        }

        void mutateFrame(BenchmarkSceneState& state,
                         const RenderingBenchmarkConfig& config,
                         uint64_t frameIndex)
        {
            const uint32_t movingObjects =
                std::min<uint32_t>(config.movingObjectCount, static_cast<uint32_t>(state.renderables.size()));
            for (uint32_t i = 0; i < movingObjects; ++i)
            {
                Entity entity = state.renderables[i];
                if (!state.world.hasComponent<TransformComponent>(entity))
                    continue;

                TransformComponent& transform = state.world.getComponent<TransformComponent>(entity);
                const float phase = static_cast<float>(frameIndex) * 0.031f + static_cast<float>(i) * 0.13f;
                if (isGtsScene3StressPreset(config))
                {
                    const glm::vec3 home = gtsScene3CubePosition(i);
                    transform.position = {
                        home.x + std::cos(phase) * 0.5f,
                        home.y + std::sin(phase * 1.7f) * 0.25f,
                        home.z + std::sin(phase) * 0.5f
                    };
                    transform.rotation = {
                        std::sin(phase * 0.7f) * 0.35f,
                        phase,
                        std::cos(phase * 0.5f) * 0.25f
                    };
                }
                else
                {
                    transform.position.x += std::sin(phase) * 0.015f;
                    transform.position.y += std::cos(phase) * 0.010f;
                }
                gts::transform::markDirty(state.world, entity);
            }

            const uint32_t movingLights =
                std::min<uint32_t>(config.movingLightCount, static_cast<uint32_t>(state.movingLights.size()));
            for (uint32_t i = 0; i < movingLights; ++i)
            {
                Entity entity = state.movingLights[i];
                if (!state.world.hasComponent<TransformComponent>(entity))
                    continue;
                TransformComponent& transform = state.world.getComponent<TransformComponent>(entity);
                const float phase = static_cast<float>(frameIndex) * 0.021f + static_cast<float>(i);
                transform.position.x += std::sin(phase) * 0.04f;
                transform.position.z += std::cos(phase) * 0.04f;
                gts::transform::markDirty(state.world, entity);
            }

            MaterialRuntime& runtime = materialRuntime(state.world);
            const uint32_t materialMutations =
                std::min<uint32_t>(config.materialMutationCountPerFrame,
                                   static_cast<uint32_t>(state.materials.size()));
            for (uint32_t i = 0; i < materialMutations; ++i)
            {
                MaterialInstanceHandle handle = state.materials[(frameIndex + i) % state.materials.size()];
                runtime.modifyInstance(
                    handle,
                    [&](MaterialInstance& instance)
                    {
                        const float t = static_cast<float>((frameIndex + i * 17u) % 255u) / 255.0f;
                        instance.baseColor.x = 0.35f + 0.55f * t;
                        instance.baseColor.y = 0.85f - 0.45f * t;
                        return true;
                    });
            }

            const uint32_t topologyMutations =
                std::min<uint32_t>(config.topologyMutationCountPerFrame,
                                   static_cast<uint32_t>(state.materials.size()));
            for (uint32_t i = 0; i < topologyMutations; ++i)
            {
                MaterialInstanceHandle handle = state.materials[(frameIndex + i * 3u) % state.materials.size()];
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
                                                      static_cast<uint32_t>(state.renderables.size())));
            for (uint32_t i = 0; i < dynamicMeshes; ++i)
            {
                Entity entity = state.renderables[i];
                if (!state.world.hasComponent<DynamicMeshComponent>(entity))
                    continue;

                DynamicMeshComponent& mesh = state.world.getComponent<DynamicMeshComponent>(entity);
                mesh.vertices = dynamicMeshVerticesForFrame(config, static_cast<uint32_t>(frameIndex + i));
                mesh.indices = dynamicMeshIndicesForFrame(config, static_cast<uint32_t>(frameIndex + i));
                markDynamicMeshChanged(state.world, entity);
            }
        }

        void addSample(std::map<std::string, std::vector<double>>& timings,
                       std::map<std::string, uint64_t>& counters,
                       const std::string& key,
                       double value)
        {
            timings[key].push_back(value);
        }

        void addCounter(std::map<std::string, uint64_t>& counters,
                        const std::string& key,
                        uint64_t value)
        {
            counters[key] += value;
        }

        void recordParticleCounters(const ECSWorld& world,
                                    std::map<std::string, uint64_t>& counters)
        {
            if (world.hasAny<ParticleFrameDataComponent>())
            {
                const ParticleFrameData& frameData =
                    world.getSingleton<ParticleFrameDataComponent>().frameData;
                addCounter(counters, "particle_frame_emitters", frameData.emitterCount);
                addCounter(counters, "particle_visible_emitters", frameData.visibleEmitterCount);
                addCounter(counters, "particle_culled_emitters", frameData.culledEmitterCount);
                addCounter(counters, "particle_simulated", frameData.simulatedParticleCount);
                addCounter(counters, "particle_rendered", frameData.renderedParticleCount);
                addCounter(counters, "particle_budget_clipped", frameData.budgetClippedParticleCount);
                addCounter(counters, "particle_billboard_instances",
                           static_cast<uint64_t>(frameData.instances.size()));
                addCounter(counters, "particle_mesh_instances",
                           static_cast<uint64_t>(frameData.meshInstances.size()));
                addCounter(counters, "particle_draw_commands",
                           static_cast<uint64_t>(frameData.drawCommands.size()));
                addCounter(counters, "particle_mesh_draw_commands",
                           static_cast<uint64_t>(frameData.meshDrawCommands.size()));
                addCounter(counters, "particle_collision_events", frameData.collisionEventCount);
                addCounter(counters, "particle_death_events", frameData.deathEventCount);
                addCounter(counters, "particle_event_spawns", frameData.eventSpawnedParticleCount);
                addCounter(counters, "particle_render_budget", frameData.renderBudget);
            }

            if (world.hasAny<ParticleBudgetComponent>())
            {
                const ParticleBudgetState& budget = world.getSingleton<ParticleBudgetComponent>().state;
                addCounter(counters, "particle_budget_requested_simulated",
                           budget.requestedSimulatedParticles);
                addCounter(counters, "particle_budget_active", budget.activeParticles);
                addCounter(counters, "particle_budget_spawned", budget.spawnedParticles);
                addCounter(counters, "particle_budget_rendered", budget.renderedParticles);
                addCounter(counters, "particle_budget_culled_emitters", budget.culledEmitters);
                addCounter(counters, "particle_budget_clipped_emitters", budget.budgetClippedEmitters);
            }
        }

        std::string controllerTimingKey(const EcsSystemTimingSample& sample)
        {
            std::string key(sample.name);
            if (sample.instanceIndex > 0)
            {
                key += "#";
                key += std::to_string(sample.instanceIndex);
            }
            return key;
        }

        void recordControllerTimingSamples(
            const std::vector<EcsSystemTimingSample>& samples,
            std::map<std::string, std::vector<double>>& controllerTimings,
            std::map<std::string, std::vector<double>>& controllerFlushTimings,
            std::map<std::string, std::string>& controllerGroups)
        {
            for (const EcsSystemTimingSample& sample : samples)
            {
                const std::string key = controllerTimingKey(sample);
                controllerTimings[key].push_back(sample.updateMs);
                controllerFlushTimings[key].push_back(sample.commandFlushMs);
                controllerGroups.emplace(key, ecsSystemGroupName(sample.group));
            }
        }

        void addSubstageSample(std::map<std::string, std::vector<double>>& substages,
                               const std::string& key,
                               double value)
        {
            substages[key].push_back(value);
        }

        void recordParticleSubstages(const ParticleEmitterSystem::Metrics& metrics,
                                     std::map<std::string, std::vector<double>>& substages)
        {
            addSubstageSample(substages,
                              "ParticleEmitterSystem.runtime_maintenance",
                              metrics.runtimeMaintenanceCpuMs);
            addSubstageSample(substages,
                              "ParticleEmitterSystem.setup",
                              metrics.setupCpuMs);
            addSubstageSample(substages,
                              "ParticleEmitterSystem.policy",
                              metrics.policyCpuMs);
            addSubstageSample(substages,
                              "ParticleEmitterSystem.simulation",
                              metrics.simulationCpuMs);
            addSubstageSample(substages,
                              "ParticleEmitterSystem.bounds_visibility",
                              metrics.boundsVisibilityCpuMs);
            addSubstageSample(substages,
                              "ParticleEmitterSystem.extraction",
                              metrics.extractionCpuMs);
            addSubstageSample(substages,
                              "ParticleEmitterSystem.frame_build",
                              metrics.frameBuildCpuMs);
            addSubstageSample(substages,
                              "ParticleEmitterSystem.total_instrumented",
                              metrics.totalCpuMs);
        }

        void recordRenderPrepSubstages(
            const gts::transform::TransformResolveMetrics& transformMetrics,
            const RenderGpuSystem::Metrics& renderGpuMetrics,
            const DynamicMeshBindingSystem::Metrics& dynamicMeshMetrics,
            const RenderExtractionSnapshotBuilder::Metrics& snapshotMetrics,
            std::map<std::string, std::vector<double>>& substages)
        {
            addSubstageSample(substages,
                              "TransformSystem.work_collection",
                              transformMetrics.workCollectionCpuMs);
            addSubstageSample(substages,
                              "TransformSystem.hierarchy_scheduling",
                              transformMetrics.hierarchySchedulingCpuMs);
            addSubstageSample(substages,
                              "TransformSystem.input_gather",
                              transformMetrics.inputGatherCpuMs);
            addSubstageSample(substages,
                              "TransformSystem.matrix_calculation",
                              transformMetrics.matrixCalculationCpuMs);
            addSubstageSample(substages,
                              "TransformSystem.changed_list_emission",
                              transformMetrics.changedListEmissionCpuMs);
            addSubstageSample(substages,
                              "TransformSystem.queue_children",
                              transformMetrics.queueChildrenCpuMs);
            addSubstageSample(substages,
                              "TransformSystem.resolve_world",
                              transformMetrics.resolveWorldCpuMs);
            addSubstageSample(substages,
                              "TransformSystem.publish_world_transform",
                              transformMetrics.publishWorldCpuMs);
            addSubstageSample(substages,
                              "RenderGpuSystem.scan_compare",
                              renderGpuMetrics.scanAndCompareCpuMs);
            addSubstageSample(substages,
                              "RenderGpuSystem.model_sync_enqueue",
                              renderGpuMetrics.modelSyncCpuMs);
            addSubstageSample(substages,
                              "RenderGpuSystem.candidate_discovery",
                              renderGpuMetrics.candidateDiscoveryCpuMs);
            addSubstageSample(substages,
                              "RenderGpuSystem.input_gather",
                              renderGpuMetrics.inputGatherCpuMs);
            addSubstageSample(substages,
                              "RenderGpuSystem.validation",
                              renderGpuMetrics.validationCpuMs);
            addSubstageSample(substages,
                              "RenderGpuSystem.version_checks",
                              renderGpuMetrics.versionCheckCpuMs);
            addSubstageSample(substages,
                              "RenderGpuSystem.batch_processing",
                              renderGpuMetrics.batchProcessingCpuMs);
            addSubstageSample(substages,
                              "RenderGpuSystem.output_merge",
                              renderGpuMetrics.outputMergeCpuMs);
            addSubstageSample(substages,
                              "RenderGpuSystem.model_matrix_copy",
                              renderGpuMetrics.matrixCopyCpuMs);
            addSubstageSample(substages,
                              "RenderGpuSystem.object_upload_enqueue",
                              renderGpuMetrics.objectUploadEnqueueCpuMs);
            addSubstageSample(substages,
                              "RenderGpuSystem.snapshot_invalidation",
                              renderGpuMetrics.snapshotInvalidationCpuMs);
            addSubstageSample(substages,
                              "RenderGpuSystem.queue_cleanup",
                              renderGpuMetrics.queueCleanupCpuMs);
            addSubstageSample(substages,
                              "DynamicMeshBindingSystem.candidate_discovery",
                              dynamicMeshMetrics.candidateDiscoveryCpuMs);
            addSubstageSample(substages,
                              "DynamicMeshBindingSystem.version_checks",
                              dynamicMeshMetrics.versionCheckCpuMs);
            addSubstageSample(substages,
                              "DynamicMeshBindingSystem.validation",
                              dynamicMeshMetrics.validationCpuMs);
            addSubstageSample(substages,
                              "DynamicMeshBindingSystem.bounds",
                              dynamicMeshMetrics.boundsCpuMs);
            addSubstageSample(substages,
                              "DynamicMeshBindingSystem.geometry_preparation",
                              dynamicMeshMetrics.geometryPreparationCpuMs);
            addSubstageSample(substages,
                              "DynamicMeshBindingSystem.temporary_copy",
                              dynamicMeshMetrics.temporaryCopyCpuMs);
            addSubstageSample(substages,
                              "DynamicMeshBindingSystem.resource_allocation",
                              dynamicMeshMetrics.resourceAllocationCpuMs);
            addSubstageSample(substages,
                              "DynamicMeshBindingSystem.vertex_upload",
                              dynamicMeshMetrics.vertexUploadCpuMs);
            addSubstageSample(substages,
                              "DynamicMeshBindingSystem.index_upload",
                              dynamicMeshMetrics.indexUploadCpuMs);
            addSubstageSample(substages,
                              "DynamicMeshBindingSystem.publication",
                              dynamicMeshMetrics.publicationCpuMs);
            addSubstageSample(substages,
                              "DynamicMeshBindingSystem.invalidation",
                              dynamicMeshMetrics.invalidationCpuMs);
            addSubstageSample(substages,
                              "DynamicMeshBindingSystem.cleanup",
                              dynamicMeshMetrics.cleanupCpuMs);
            addSubstageSample(substages,
                              "RenderExtractionSnapshotBuilder.dirty_collection",
                              snapshotMetrics.dirtyCollectionCpuMs);
            addSubstageSample(substages,
                              "RenderExtractionSnapshotBuilder.input_gather",
                              snapshotMetrics.inputGatherCpuMs);
            addSubstageSample(substages,
                              "RenderExtractionSnapshotBuilder.entry_refresh",
                              snapshotMetrics.entryRefreshCpuMs);
            addSubstageSample(substages,
                              "RenderExtractionSnapshotBuilder.aggregate_merge",
                              snapshotMetrics.aggregateMergeCpuMs);
        }

        void recordFrame(BenchmarkSceneState& state,
                         const RenderingBenchmarkConfig& config,
                         uint64_t frameIndex,
                         std::map<std::string, std::vector<double>>& timings,
                         std::map<std::string, std::vector<double>>& controllerTimings,
                         std::map<std::string, std::vector<double>>& controllerFlushTimings,
                         std::map<std::string, std::vector<double>>& controllerSubstages,
                         std::map<std::string, std::string>& controllerGroups,
                         std::map<std::string, uint64_t>& counters)
        {
            TimeContext time;
            time.deltaTime = FixedDeltaSeconds;
            time.unscaledDeltaTime = FixedDeltaSeconds;
            time.timeScale = 1.0f;
            time.frame = frameIndex;

            EcsControllerContext ctx{
                state.world,
                &state.resources,
                nullptr,
                &time
            };
            ctx.windowPixelWidth = static_cast<float>(config.renderWidth);
            ctx.windowPixelHeight = static_cast<float>(config.renderHeight);
            ctx.windowAspectRatio = config.renderHeight == 0
                ? 1.0f
                : static_cast<float>(config.renderWidth) / static_cast<float>(config.renderHeight);
            ctx.sceneViewportPixelWidth = ctx.windowPixelWidth;
            ctx.sceneViewportPixelHeight = ctx.windowPixelHeight;
            ctx.sceneViewportAspectRatio = ctx.windowAspectRatio;

            mutateFrame(state, config, frameIndex);

            const auto frameStart = std::chrono::steady_clock::now();
            const auto controllerStart = std::chrono::steady_clock::now();
            state.world.updateControllers(ctx);
            const auto controllerEnd = std::chrono::steady_clock::now();
            recordControllerTimingSamples(state.world.getLastControllerTimingSamples(),
                                          controllerTimings,
                                          controllerFlushTimings,
                                          controllerGroups);

            const auto& commands = state.pipeline.build(state.world);
            const auto frameEnd = std::chrono::steady_clock::now();

            const auto pipelineMetrics = state.pipeline.getLastPipelineMetrics();
            const auto snapshotMetrics = state.pipeline.getSnapshotBuilder().getLastMetrics();
            const auto extractorMetrics = state.pipeline.getExtractor().getLastMetrics();
            const auto renderGpuMetrics = RenderGpuSystem::getLastMetrics();
            const auto dynamicMeshMetrics = DynamicMeshBindingSystem::getLastMetrics();
            const auto transformMetrics = gts::transform::TransformSystem::getLastMetrics();
            const auto materialMetrics = MaterialBindingSystem::getLastMetrics();
            const RenderExtractionSnapshot& snapshot = state.pipeline.getLatestSnapshot();
            const gts::rendering::LightingFrameData& lighting = snapshot.lighting;

            addSample(timings,
                      counters,
                      "frame_cpu",
                      std::chrono::duration<double, std::milli>(frameEnd - frameStart).count());
            addSample(timings,
                      counters,
                      "controller_cpu",
                      std::chrono::duration<double, std::milli>(controllerEnd - controllerStart).count());
            addSample(timings, counters, "snapshot_build_cpu", pipelineMetrics.snapshotBuildCpuMs);
            addSample(timings, counters, "visibility_cpu", pipelineMetrics.visibilityCpuMs);
            addSample(timings, counters, "command_extract_cpu", extractorMetrics.extractCpuMs);
            addSample(timings, counters, "command_sort_cpu", extractorMetrics.sortCpuMs);
            addSample(timings, counters, "render_gpu_sync_cpu", renderGpuMetrics.cpuTimeMs);
            addSample(timings, counters, "transform_resolve_cpu", transformMetrics.cpuTimeMs);
            recordRenderPrepSubstages(transformMetrics,
                                      renderGpuMetrics,
                                      dynamicMeshMetrics,
                                      snapshotMetrics,
                                      controllerSubstages);
            recordParticleSubstages(ParticleEmitterSystem::getLastMetrics(), controllerSubstages);

            uint64_t transparent = 0;
            for (const RenderableSnapshot& renderable : snapshot.renderables)
            {
                if (renderable.renderQueue == RenderQueue::Transparent)
                    ++transparent;
            }

            const uint64_t meshRequestDelta =
                state.resources.meshRequests - state.previousMeshRequests;
            const uint64_t textureRequestDelta =
                state.resources.textureRequests - state.previousTextureRequests;
            const uint64_t proceduralUploadDelta =
                state.resources.proceduralMeshUploads - state.previousProceduralUploads;
            const uint64_t objectSlotDelta =
                state.resources.objectSlotRequests - state.previousObjectSlotRequests;
            const uint64_t cameraBufferDelta =
                state.resources.cameraBufferRequests - state.previousCameraBufferRequests;

            state.previousMeshRequests = state.resources.meshRequests;
            state.previousTextureRequests = state.resources.textureRequests;
            state.previousProceduralUploads = state.resources.proceduralMeshUploads;
            state.previousObjectSlotRequests = state.resources.objectSlotRequests;
            state.previousCameraBufferRequests = state.resources.cameraBufferRequests;

            addCounter(counters, "simulation_ticks", 0);
            addCounter(counters, "authored_renderables", config.renderableCount);
            addCounter(counters, "snapshot_renderables", snapshotMetrics.renderableCount);
            addCounter(counters, "visible_renderables", extractorMetrics.visibleRenderables);
            addCounter(counters,
                       "culled_renderables",
                       extractorMetrics.totalRenderables >= extractorMetrics.visibleRenderables
                           ? extractorMetrics.totalRenderables - extractorMetrics.visibleRenderables
                           : 0u);
            addCounter(counters, "transparent_renderables", transparent);
            addCounter(counters, "render_commands", static_cast<uint64_t>(commands.size()));
            addCounter(counters, "command_cache_hits", extractorMetrics.cachedCommands);
            addCounter(counters, "command_updates", extractorMetrics.updatedCommands);
            addCounter(counters, "command_sorts", extractorMetrics.sortedThisFrame ? 1u : 0u);
            addCounter(counters, "object_uploads", static_cast<uint64_t>(snapshot.objectUploads.size()));
            addCounter(counters, "object_upload_commands", static_cast<uint64_t>(snapshot.objectUploads.size()));
            addCounter(counters, "logical_object_updates", renderGpuMetrics.updatedRenderables);
            addCounter(counters, "camera_uploads", static_cast<uint64_t>(snapshot.cameraUploads.size()));
            addCounter(counters, "material_frame_entries",
                       static_cast<uint64_t>(snapshot.materialFrameData.materials.size()));
            addCounter(counters, "mesh_requests", meshRequestDelta);
            addCounter(counters, "texture_requests", textureRequestDelta);
            addCounter(counters, "mesh_uploads", proceduralUploadDelta);
            addCounter(counters, "dynamic_mesh_queued", dynamicMeshMetrics.queuedMeshes);
            addCounter(counters, "dynamic_mesh_candidates", dynamicMeshMetrics.candidateEntities);
            addCounter(counters, "dynamic_mesh_unchanged_skipped", dynamicMeshMetrics.unchangedMeshesSkipped);
            addCounter(counters, "dynamic_mesh_failed_version_skipped", dynamicMeshMetrics.failedVersionsSkipped);
            addCounter(counters, "dynamic_mesh_changed", dynamicMeshMetrics.changedMeshesProcessed);
            addCounter(counters, "dynamic_mesh_invalid", dynamicMeshMetrics.invalidMeshes);
            addCounter(counters, "dynamic_mesh_gpu_allocations", dynamicMeshMetrics.gpuMeshAllocations);
            addCounter(counters, "dynamic_mesh_gpu_reallocations", dynamicMeshMetrics.gpuMeshReallocations);
            addCounter(counters, "dynamic_mesh_in_place_updates", dynamicMeshMetrics.inPlaceGpuUpdates);
            addCounter(counters, "dynamic_mesh_resources_recreated", dynamicMeshMetrics.meshResourcesRecreated);
            addCounter(counters, "dynamic_mesh_renderable_invalidations", dynamicMeshMetrics.renderablesInvalidated);
            addCounter(counters, "dynamic_mesh_bounds_recomputed", dynamicMeshMetrics.boundsRecomputed);
            addCounter(counters, "dynamic_mesh_normals_generated", dynamicMeshMetrics.normalsGenerated);
            addCounter(counters, "dynamic_mesh_tangents_generated", dynamicMeshMetrics.tangentsGenerated);
            addCounter(counters, "dynamic_mesh_vertices_processed", dynamicMeshMetrics.verticesProcessed);
            addCounter(counters, "dynamic_mesh_indices_processed", dynamicMeshMetrics.indicesProcessed);
            addCounter(counters, "dynamic_mesh_cpu_bytes_copied", dynamicMeshMetrics.cpuBytesCopied);
            addCounter(counters, "dynamic_mesh_vertex_bytes_uploaded", dynamicMeshMetrics.vertexBytesUploaded);
            addCounter(counters, "dynamic_mesh_index_bytes_uploaded", dynamicMeshMetrics.indexBytesUploaded);
            addCounter(counters, "dynamic_mesh_vertex_bytes_allocated", dynamicMeshMetrics.vertexBytesAllocated);
            addCounter(counters, "dynamic_mesh_index_bytes_allocated", dynamicMeshMetrics.indexBytesAllocated);
            addCounter(counters, "object_slot_allocations", objectSlotDelta);
            addCounter(counters, "camera_buffer_allocations", cameraBufferDelta);
            addCounter(counters, "bytes_uploaded_object",
                       static_cast<uint64_t>(snapshot.objectUploads.size() * sizeof(ObjectUploadCommand)));
            addCounter(counters, "physical_object_buffer_writes", 0);
            addCounter(counters, "bytes_written_object_buffer", 0);
            addCounter(counters, "object_write_contiguous_runs", 0);
            addCounter(counters, "bytes_uploaded_camera",
                       static_cast<uint64_t>(snapshot.cameraUploads.size() * sizeof(CameraUploadCommand)));
            addCounter(counters, "material_queued", materialMetrics.queuedMaterials);
            addCounter(counters, "material_synchronized", materialMetrics.synchronizedMaterials);
            addCounter(counters, "material_user_invalidations", materialMetrics.userInvalidations);
            addCounter(counters, "material_fallback_substitutions", materialMetrics.fallbackSubstitutions);
            addCounter(counters, "material_reference_adds", materialMetrics.referenceAdds);
            addCounter(counters, "material_reference_removes", materialMetrics.referenceRemoves);
            addCounter(counters, "material_full_scans", materialMetrics.fullMaterialScans);
            addCounter(counters, "unique_referenced_materials", config.uniqueMaterialCount);
            addCounter(counters, "snapshot_dirty_entries", snapshotMetrics.dirtyUpdatedCount);
            addCounter(counters, "snapshot_reused_entries", snapshotMetrics.reusedCount);
            addCounter(counters, "snapshot_static_renderables", snapshotMetrics.staticRenderableCount);
            addCounter(counters, "snapshot_dynamic_renderables", snapshotMetrics.dynamicRenderableCount);
            addCounter(counters, "snapshot_update_work_items", snapshotMetrics.updateWorkItems);
            addCounter(counters, "snapshot_update_batches", snapshotMetrics.updateBatches);
            addCounter(counters, "snapshot_refreshed_entries", snapshotMetrics.refreshedEntries);
            addCounter(counters, "snapshot_full_rebuilds", snapshotMetrics.fullRebuilds);
            addCounter(counters, "snapshot_storage_reallocations", snapshotMetrics.storageReallocations);
            addCounter(counters, "snapshot_max_batch_size", snapshotMetrics.maxBatchSize);
            addCounter(counters, "transform_queued", transformMetrics.queuedTransforms);
            addCounter(counters, "transform_processed", transformMetrics.processedTransforms);
            addCounter(counters, "transform_updates", transformMetrics.updatedWorldTransforms);
            addCounter(counters, "transform_changed_roots", transformMetrics.changedRoots);
            addCounter(counters, "transform_work_items", transformMetrics.workItems);
            addCounter(counters, "transform_batches", transformMetrics.batches);
            addCounter(counters, "transform_hierarchy_levels", transformMetrics.hierarchyLevels);
            addCounter(counters, "transform_max_batch_size", transformMetrics.maxBatchSize);
            addCounter(counters, "transform_duplicate_work_removed", transformMetrics.duplicateWorkRemoved);
            addCounter(counters, "render_gpu_updated", renderGpuMetrics.updatedRenderables);
            addCounter(counters, "render_gpu_total", renderGpuMetrics.totalRenderables);
            addCounter(counters, "render_gpu_version_matches", renderGpuMetrics.readyVersionMatches);
            addCounter(counters, "render_gpu_snapshot_dirty_enqueues", renderGpuMetrics.snapshotDirtyEnqueues);
            addCounter(counters, "render_gpu_full_scan_visited", renderGpuMetrics.fullScanRenderablesVisited);
            addCounter(counters, "render_gpu_queued", renderGpuMetrics.queuedRenderables);
            addCounter(counters, "render_gpu_queue_drained", renderGpuMetrics.queuedEntriesDrained);
            addCounter(counters, "render_gpu_stale_skipped", renderGpuMetrics.staleQueueEntriesSkipped);
            addCounter(counters,
                       "render_gpu_missing_component_skipped",
                       renderGpuMetrics.missingComponentEntriesSkipped);
            addCounter(counters, "render_gpu_unchanged_skipped", renderGpuMetrics.unchangedVersionsSkipped);
            addCounter(counters, "render_gpu_changed_syncs", renderGpuMetrics.changedTransformsSynchronized);
            addCounter(counters, "render_gpu_matrix_copies", renderGpuMetrics.modelMatricesCopied);
            addCounter(counters, "render_gpu_object_upload_requests", renderGpuMetrics.objectUploadRequests);
            addCounter(counters,
                       "render_gpu_duplicate_queue_attempts",
                       renderGpuMetrics.duplicateQueueAttempts);
            addCounter(counters,
                       "render_gpu_queue_deduplications",
                       renderGpuMetrics.queueDeduplications);
            addCounter(counters,
                       "render_gpu_object_slot_lookup_failures",
                       renderGpuMetrics.objectSlotLookupFailures);
            addCounter(counters, "render_gpu_sync_work_items", renderGpuMetrics.syncWorkItems);
            addCounter(counters, "render_gpu_sync_batches", renderGpuMetrics.syncBatches);
            addCounter(counters, "render_gpu_sync_max_batch_size", renderGpuMetrics.syncMaxBatchSize);
            addCounter(counters, "render_gpu_sync_state_updates", renderGpuMetrics.syncStateUpdates);
            addCounter(counters,
                       "render_gpu_sync_snapshot_invalidations",
                       renderGpuMetrics.syncSnapshotInvalidations);
            addCounter(counters, "render_gpu_ecs_lookups_gather", renderGpuMetrics.ecsLookupsDuringGather);
            addCounter(counters, "render_gpu_ecs_lookups_publish", renderGpuMetrics.ecsLookupsDuringPublish);
            addCounter(counters, "lights_total_directional", lighting.diagnostics.totalDirectionalLights);
            addCounter(counters, "lights_total_point", lighting.diagnostics.totalPointLights);
            addCounter(counters, "lights_total_spot", lighting.diagnostics.totalSpotLights);
            addCounter(counters, "lights_selected_directional", lighting.directionalCount);
            addCounter(counters, "lights_selected_point", lighting.pointCount);
            addCounter(counters, "lights_selected_spot", lighting.spotCount);
            addCounter(counters, "lights_dropped_directional", lighting.diagnostics.droppedDirectionalLights);
            addCounter(counters, "lights_dropped_point", lighting.diagnostics.droppedPointLights);
            addCounter(counters, "lights_dropped_spot", lighting.diagnostics.droppedSpotLights);
            addCounter(counters, "particle_emitters", config.particleEmitterCount);
            recordParticleCounters(state.world, counters);
            addCounter(counters, "world_text_blocks", config.worldTextCount);
            addCounter(counters, "draw_calls", 0);
            addCounter(counters, "prepared_batches", 0);
            addCounter(counters, "pipeline_switches", 0);
            addCounter(counters, "descriptor_binds", 0);
            addCounter(counters, "screenshot_requested", 0);
            addCounter(counters, "screenshot_scheduled", 0);
            addCounter(counters, "screenshot_completed", 0);
            addCounter(counters, "screenshot_skipped", 0);
            addCounter(counters, "screenshot_pending_gpu", 0);
            addCounter(counters, "screenshot_pending_write", 0);
            addCounter(counters, "screenshot_readback_bytes", 0);
            addCounter(counters, "gpu_timing_available", 0);
        }

        RenderingBenchmarkConfig presetConfig(std::string name,
                                              std::string description,
                                              uint32_t version,
                                              RenderingBenchmarkConfig config,
                                              std::vector<BenchmarkPreset>& presets)
        {
            config.presetName = std::move(name);
            config.presetVersion = version;
            sanitizeConfig(config);
            presets.push_back({config.presetName, std::move(description), version, config});
            return config;
        }
    }

    const char* benchmarkRunModeName(BenchmarkRunMode mode)
    {
        switch (mode)
        {
            case BenchmarkRunMode::CpuSmoke: return "cpu_smoke";
            case BenchmarkRunMode::GpuRuntime: return "gpu_runtime";
        }
        return "unknown";
    }

    BenchmarkEnvironment collectBenchmarkEnvironment(const RenderingBenchmarkConfig& config)
    {
        return collectEnvironment(config);
    }

    std::vector<BenchmarkPreset> standardBenchmarkPresets()
    {
        std::vector<BenchmarkPreset> presets;

        RenderingBenchmarkConfig base;
        base.renderableCount = 1000;
        base.visibleRenderableCount = 1000;
        base.uniqueMeshCount = 8;
        base.uniqueMaterialCount = 16;
        base.directionalLightCount = 1;
        base.pointLightCount = 8;
        base.warmupFrames = 120;
        base.measuredFrames = 600;

        RenderingBenchmarkConfig staticSmall = base;
        staticSmall.renderableCount = 256;
        staticSmall.visibleRenderableCount = 256;
        staticSmall.uniqueMeshCount = 4;
        staticSmall.uniqueMaterialCount = 4;
        presetConfig("static_geometry_small",
                     "Static renderables sharing a small mesh/material set.",
                     1,
                     staticSmall,
                     presets);

        RenderingBenchmarkConfig screenshotSmoke = staticSmall;
        screenshotSmoke.mode = BenchmarkRunMode::GpuRuntime;
        screenshotSmoke.warmupFrames = 4;
        screenshotSmoke.measuredFrames = 12;
        screenshotSmoke.renderableCount = 256;
        screenshotSmoke.visibleRenderableCount = 256;
        screenshotSmoke.uniqueMeshCount = 4;
        screenshotSmoke.uniqueMaterialCount = 4;
        screenshotSmoke.requestScreenshot = true;
        screenshotSmoke.screenshotMeasuredFrame = 0;
        presetConfig("screenshot_capture_smoke",
                     "Small GPU runtime workload that requests one deferred screenshot capture.",
                     1,
                     screenshotSmoke,
                     presets);

        RenderingBenchmarkConfig submitEmpty = base;
        submitEmpty.mode = BenchmarkRunMode::GpuRuntime;
        submitEmpty.renderableCount = 0;
        submitEmpty.visibleRenderableCount = 0;
        submitEmpty.uniqueMeshCount = 1;
        submitEmpty.uniqueMaterialCount = 1;
        submitEmpty.directionalLightCount = 0;
        submitEmpty.pointLightCount = 0;
        submitEmpty.spotLightCount = 0;
        submitEmpty.enablePbr = false;
        submitEmpty.enableNormalMaps = false;
        submitEmpty.enableIbl = false;
        presetConfig("submit_empty_baseline",
                     "GPU runtime baseline for swapchain acquire, queue submit, present, and frame fences.",
                     1,
                     submitEmpty,
                     presets);

        RenderingBenchmarkConfig submitDrawCallPressure = submitEmpty;
        submitDrawCallPressure.renderableCount = 12000;
        submitDrawCallPressure.visibleRenderableCount = 12000;
        submitDrawCallPressure.uniqueMeshCount = 64;
        submitDrawCallPressure.uniqueMaterialCount = 256;
        presetConfig("submit_draw_call_pressure",
                     "Many visible material/mesh groups to isolate command recording and queue submission pressure.",
                     1,
                     submitDrawCallPressure,
                     presets);

        RenderingBenchmarkConfig submitStateChangePressure = submitEmpty;
        submitStateChangePressure.renderableCount = 12000;
        submitStateChangePressure.visibleRenderableCount = 12000;
        submitStateChangePressure.uniqueMeshCount = 1024;
        submitStateChangePressure.uniqueMaterialCount = 4096;
        presetConfig("submit_state_change_pressure",
                     "Fragmented mesh/material state to expose bind, descriptor, and texture switch pressure.",
                     1,
                     submitStateChangePressure,
                     presets);

        RenderingBenchmarkConfig submitObjectUploadPressure = submitDrawCallPressure;
        submitObjectUploadPressure.movingObjectCount = submitObjectUploadPressure.renderableCount;
        presetConfig("submit_object_upload_pressure",
                     "Many moving renderables to isolate backend object-buffer write pressure.",
                     1,
                     submitObjectUploadPressure,
                     presets);

        RenderingBenchmarkConfig submitParticleDrawPressure = submitEmpty;
        submitParticleDrawPressure.particleEmitterCount = 192;
        submitParticleDrawPressure.particleMeshEmitterCount = 64;
        submitParticleDrawPressure.particleEmissionRate = 48.0;
        submitParticleDrawPressure.particleMaxParticles = 128;
        presetConfig("submit_particle_draw_pressure",
                     "Particle-heavy runtime path for billboard, mesh particle, and particle draw submission pressure.",
                     1,
                     submitParticleDrawPressure,
                     presets);

        RenderingBenchmarkConfig staticLarge = base;
        staticLarge.renderableCount = 12000;
        staticLarge.visibleRenderableCount = 12000;
        staticLarge.uniqueMeshCount = 12;
        staticLarge.uniqueMaterialCount = 24;
        presetConfig("static_geometry_large",
                     "Large static world for extraction, sorting, and command cache pressure.",
                     1,
                     staticLarge,
                     presets);

        RenderingBenchmarkConfig materialShared = base;
        materialShared.renderableCount = 4000;
        materialShared.visibleRenderableCount = 4000;
        materialShared.uniqueMaterialCount = 2;
        presetConfig("material_shared_steady",
                     "Many renderables referencing a tiny shared material set with no mutations.",
                     1,
                     materialShared,
                     presets);

        RenderingBenchmarkConfig materialMutating = materialShared;
        materialMutating.materialMutationCountPerFrame = 1;
        presetConfig("material_shared_mutating",
                     "Shared material parameter mutations through the material change queue.",
                     1,
                     materialMutating,
                     presets);

        RenderingBenchmarkConfig materialUnique = base;
        materialUnique.renderableCount = 3000;
        materialUnique.visibleRenderableCount = 3000;
        materialUnique.uniqueMaterialCount = 3000;
        presetConfig("material_unique",
                     "Many unique materials for material-frame population pressure.",
                     1,
                     materialUnique,
                     presets);

        RenderingBenchmarkConfig transformWide = base;
        transformWide.renderableCount = 6000;
        transformWide.visibleRenderableCount = 6000;
        transformWide.movingObjectCount = 6000;
        presetConfig("transform_many_moving",
                     "Many independent moving roots for transform and object-upload stress.",
                     1,
                     transformWide,
                     presets);

        RenderingBenchmarkConfig movingStaticControl = base;
        movingStaticControl.renderableCount = 4000;
        movingStaticControl.visibleRenderableCount = 4000;
        movingStaticControl.uniqueMeshCount = 1;
        movingStaticControl.uniqueMaterialCount = 1;
        movingStaticControl.pointLightCount = 0;
        movingStaticControl.movingObjectCount = 0;
        presetConfig("moving_static_control",
                     "Static control with matching object count and no movement after warmup.",
                     1,
                     movingStaticControl,
                     presets);

        RenderingBenchmarkConfig movingIndependent = movingStaticControl;
        movingIndependent.movingObjectCount = 1600;
        presetConfig("moving_independent",
                     "Independent moving roots with shared mesh/material and minimal lighting.",
                     1,
                     movingIndependent,
                     presets);

        RenderingBenchmarkConfig movingSparse = movingStaticControl;
        movingSparse.movingObjectCount = 64;
        presetConfig("moving_sparse",
                     "Sparse independent moving roots among many static renderables.",
                     1,
                     movingSparse,
                     presets);

        RenderingBenchmarkConfig movingDense = movingStaticControl;
        movingDense.movingObjectCount = movingDense.renderableCount;
        presetConfig("moving_dense",
                     "Dense independent moving roots where every renderable moves.",
                     1,
                     movingDense,
                     presets);

        RenderingBenchmarkConfig movingDeep = movingStaticControl;
        movingDeep.renderableCount = 2048;
        movingDeep.visibleRenderableCount = 2048;
        movingDeep.movingObjectCount = 512;
        presetConfig("moving_deep_hierarchy",
                     "Moving-object attribution preset reserved for deep hierarchy propagation.",
                     1,
                     movingDeep,
                     presets);

        RenderingBenchmarkConfig movingWide = movingStaticControl;
        movingWide.renderableCount = 4096;
        movingWide.visibleRenderableCount = 4096;
        movingWide.movingObjectCount = 256;
        presetConfig("moving_wide_hierarchy",
                     "Moving-object attribution preset reserved for wide hierarchy propagation.",
                     1,
                     movingWide,
                     presets);

        RenderingBenchmarkConfig uploadPressure = movingStaticControl;
        uploadPressure.renderableCount = 6000;
        uploadPressure.visibleRenderableCount = 6000;
        uploadPressure.movingObjectCount = 6000;
        presetConfig("upload_only_pressure",
                     "Maximizes ordinary transform-driven object upload pressure.",
                     1,
                     uploadPressure,
                     presets);

        RenderingBenchmarkConfig gtsScene3Stress = movingStaticControl;
        gtsScene3Stress.renderableCount = GtsScene3CubeCount;
        gtsScene3Stress.visibleRenderableCount = GtsScene3CubeCount;
        gtsScene3Stress.uniqueMeshCount = 1;
        gtsScene3Stress.uniqueMaterialCount = 8;
        gtsScene3Stress.movingObjectCount = GtsScene3CubeCount;
        gtsScene3Stress.directionalLightCount = 0;
        gtsScene3Stress.pointLightCount = 0;
        gtsScene3Stress.spotLightCount = 0;
        gtsScene3Stress.enablePbr = false;
        gtsScene3Stress.enableNormalMaps = false;
        gtsScene3Stress.enableIbl = false;
        gtsScene3Stress.hitchThresholdMs = 8.0;
        gtsScene3Stress.hitchContextFrames = 3;
        gtsScene3Stress.maxHitchEvents = 8;
        presetConfig("gtsscene3_64k_moving_cubes",
                     "GtsScene3-style 40x40x40 moving cube stress scene with hitch capture enabled.",
                     1,
                     gtsScene3Stress,
                     presets);

        RenderingBenchmarkConfig moving64kIndependent = gtsScene3Stress;
        presetConfig("moving_64k_independent",
                     "M1 readiness preset: 64k independent moving renderables for maximum batchability.",
                     1,
                     moving64kIndependent,
                     presets);

        RenderingBenchmarkConfig static64kControl = gtsScene3Stress;
        static64kControl.movingObjectCount = 0;
        static64kControl.hitchThresholdMs = 0.0;
        presetConfig("static_64k_control",
                     "M1 readiness preset: 64k static renderables with empty steady-state transform/render queues.",
                     1,
                     static64kControl,
                     presets);

        RenderingBenchmarkConfig moving64kWide = movingStaticControl;
        moving64kWide.renderableCount = GtsScene3CubeCount;
        moving64kWide.visibleRenderableCount = GtsScene3CubeCount;
        moving64kWide.uniqueMeshCount = 1;
        moving64kWide.uniqueMaterialCount = 1;
        moving64kWide.movingObjectCount = 256;
        moving64kWide.directionalLightCount = 0;
        moving64kWide.pointLightCount = 0;
        moving64kWide.spotLightCount = 0;
        moving64kWide.enablePbr = false;
        moving64kWide.enableNormalMaps = false;
        moving64kWide.enableIbl = false;
        presetConfig("moving_64k_wide_hierarchy",
                     "M1 readiness preset: few moving roots fan out through 64k renderable descendants.",
                     1,
                     moving64kWide,
                     presets);

        RenderingBenchmarkConfig dynamicStatic = movingStaticControl;
        dynamicStatic.renderableCount = 4000;
        dynamicStatic.visibleRenderableCount = 4000;
        dynamicStatic.dynamicMeshCount = 4000;
        dynamicStatic.dynamicMeshMutationCountPerFrame = 0;
        presetConfig("dynamic_mesh_static_control",
                     "Many dynamic mesh descriptors with no geometry changes after warmup.",
                     1,
                     dynamicStatic,
                     presets);

        RenderingBenchmarkConfig dynamicSparse = dynamicStatic;
        dynamicSparse.dynamicMeshMutationCountPerFrame = 32;
        presetConfig("dynamic_mesh_sparse_mutation",
                     "Many dynamic meshes with a small deterministic mutation subset each frame.",
                     1,
                     dynamicSparse,
                     presets);

        RenderingBenchmarkConfig dynamicDense = dynamicStatic;
        dynamicDense.renderableCount = 2000;
        dynamicDense.visibleRenderableCount = 2000;
        dynamicDense.dynamicMeshCount = 2000;
        dynamicDense.dynamicMeshMutationCountPerFrame = 2000;
        presetConfig("dynamic_mesh_dense_mutation",
                     "Dense dynamic geometry mutation throughput.",
                     1,
                     dynamicDense,
                     presets);

        RenderingBenchmarkConfig dynamicCapacityStable = dynamicStatic;
        dynamicCapacityStable.dynamicMeshMutationCountPerFrame = 512;
        presetConfig("dynamic_mesh_capacity_stable",
                     "Dynamic geometry content changes without vertex/index capacity growth.",
                     1,
                     dynamicCapacityStable,
                     presets);

        RenderingBenchmarkConfig dynamicGrowth = dynamicStatic;
        dynamicGrowth.dynamicMeshCount = 512;
        dynamicGrowth.dynamicMeshMutationCountPerFrame = 512;
        presetConfig("dynamic_mesh_growth",
                     "Dynamic geometry periodically exceeds existing vertex/index capacity.",
                     1,
                     dynamicGrowth,
                     presets);

        RenderingBenchmarkConfig dynamicGeneratedAttributes = dynamicSparse;
        presetConfig("dynamic_mesh_attribute_generation",
                     "Dynamic meshes missing normals/tangents so preparation generates attributes.",
                     1,
                     dynamicGeneratedAttributes,
                     presets);

        RenderingBenchmarkConfig dynamicPrecomputedAttributes = dynamicSparse;
        presetConfig("dynamic_mesh_precomputed_attributes",
                     "Dynamic meshes with complete precomputed standard vertex attributes.",
                     1,
                     dynamicPrecomputedAttributes,
                     presets);

        RenderingBenchmarkConfig visibilitySparse = base;
        visibilitySparse.renderableCount = 15000;
        visibilitySparse.visibleRenderableCount = 1200;
        presetConfig("visibility_sparse",
                     "Large authored world with a small visible subset.",
                     1,
                     visibilitySparse,
                     presets);

        RenderingBenchmarkConfig visibilityDense = base;
        visibilityDense.renderableCount = 8000;
        visibilityDense.visibleRenderableCount = 7600;
        presetConfig("visibility_dense",
                     "Dense visible set for culling and command-cache comparison.",
                     1,
                     visibilityDense,
                     presets);

        RenderingBenchmarkConfig lightingCapacity = base;
        lightingCapacity.renderableCount = 2500;
        lightingCapacity.visibleRenderableCount = 2500;
        lightingCapacity.directionalLightCount = 2;
        lightingCapacity.pointLightCount = 32;
        lightingCapacity.spotLightCount = 16;
        presetConfig("lighting_at_capacity",
                     "Authored lights fill current forward-renderer selected-light limits.",
                     1,
                     lightingCapacity,
                     presets);

        RenderingBenchmarkConfig lightingOver = lightingCapacity;
        lightingOver.pointLightCount = 256;
        lightingOver.spotLightCount = 128;
        lightingOver.movingLightCount = 64;
        presetConfig("lighting_over_capacity",
                     "Many authored local lights for ranking, dropping, and movement stress.",
                     1,
                     lightingOver,
                     presets);

        RenderingBenchmarkConfig batchHigh = base;
        batchHigh.renderableCount = 8000;
        batchHigh.visibleRenderableCount = 8000;
        batchHigh.uniqueMeshCount = 2;
        batchHigh.uniqueMaterialCount = 2;
        presetConfig("batch_high_coherence",
                     "Highly coherent commands that should remain batch-friendly.",
                     1,
                     batchHigh,
                     presets);

        RenderingBenchmarkConfig batchLow = base;
        batchLow.renderableCount = 8000;
        batchLow.visibleRenderableCount = 8000;
        batchLow.uniqueMeshCount = 1024;
        batchLow.uniqueMaterialCount = 2048;
        batchLow.topologyMutationCountPerFrame = 0;
        presetConfig("batch_low_coherence",
                     "Fragmented mesh/material combinations for sort and bind pressure.",
                     1,
                     batchLow,
                     presets);

        RenderingBenchmarkConfig pbrHeavy = base;
        pbrHeavy.renderableCount = 3500;
        pbrHeavy.visibleRenderableCount = 3500;
        pbrHeavy.uniqueMeshCount = 8;
        pbrHeavy.uniqueMaterialCount = 64;
        pbrHeavy.directionalLightCount = 2;
        pbrHeavy.pointLightCount = 32;
        pbrHeavy.spotLightCount = 16;
        pbrHeavy.enableNormalMaps = true;
        presetConfig("pbr_fragment_heavy",
                     "PBR material and selected-light pressure; GPU timing awaits timestamp support.",
                     1,
                     pbrHeavy,
                     presets);

        RenderingBenchmarkConfig particleYune = base;
        particleYune.renderableCount = 0;
        particleYune.visibleRenderableCount = 0;
        particleYune.uniqueMeshCount = 1;
        particleYune.uniqueMaterialCount = 1;
        particleYune.directionalLightCount = 0;
        particleYune.pointLightCount = 0;
        particleYune.spotLightCount = 0;
        particleYune.particleEmitterCount = 144;
        particleYune.particleMeshEmitterCount = 48;
        particleYune.particleEmissionRate = 42.0;
        particleYune.particleMaxParticles = 128;
        particleYune.enablePbr = false;
        particleYune.enableNormalMaps = false;
        particleYune.enableIbl = false;
        presetConfig("particles_yune_blocker_field",
                     "Particle-heavy Yune-style blocker field with mixed billboard and mesh emitters.",
                     1,
                     particleYune,
                     presets);

        RenderingBenchmarkConfig combined = base;
        combined.renderableCount = 5000;
        combined.visibleRenderableCount = 3500;
        combined.uniqueMeshCount = 24;
        combined.uniqueMaterialCount = 96;
        combined.movingObjectCount = 800;
        combined.materialMutationCountPerFrame = 2;
        combined.directionalLightCount = 2;
        combined.pointLightCount = 80;
        combined.spotLightCount = 24;
        combined.movingLightCount = 24;
        combined.dynamicMeshCount = 32;
        combined.dynamicMeshMutationCountPerFrame = 32;
        combined.particleEmitterCount = 16;
        combined.worldTextCount = 24;
        combined.enableUi = true;
        presetConfig("combined_game_like",
                     "Mixed static, moving, material, light, dynamic mesh, particles, and text workload.",
                     1,
                     combined,
                     presets);

        return presets;
    }

    const BenchmarkPreset* findBenchmarkPreset(std::string_view name)
    {
        static const std::vector<BenchmarkPreset> presets = standardBenchmarkPresets();
        auto it = std::find_if(
            presets.begin(),
            presets.end(),
            [&](const BenchmarkPreset& preset)
            {
                return preset.name == name;
            });
        return it == presets.end() ? nullptr : &*it;
    }

    void sanitizeConfig(RenderingBenchmarkConfig& config)
    {
        config.warmupFrames = std::min(config.warmupFrames, 6000u);
        config.measuredFrames = std::clamp(config.measuredFrames, 1u, 12000u);
        config.renderableCount = std::min(config.renderableCount, MaxBenchmarkRenderables);
        if (config.visibleRenderableCount == 0)
            config.visibleRenderableCount = config.renderableCount;
        config.visibleRenderableCount = std::min(config.visibleRenderableCount, config.renderableCount);
        config.uniqueMeshCount = std::clamp(config.uniqueMeshCount, 1u, MaxBenchmarkMeshes);
        config.uniqueMaterialCount = std::clamp(config.uniqueMaterialCount, 1u, MaxBenchmarkMaterials);
        config.movingObjectCount = std::min(config.movingObjectCount, config.renderableCount);
        config.materialMutationCountPerFrame =
            std::min(config.materialMutationCountPerFrame, config.uniqueMaterialCount);
        config.topologyMutationCountPerFrame =
            std::min(config.topologyMutationCountPerFrame, config.uniqueMaterialCount);
        config.directionalLightCount = std::min(config.directionalLightCount, MaxBenchmarkLights);
        config.pointLightCount = std::min(config.pointLightCount, MaxBenchmarkLights);
        config.spotLightCount = std::min(config.spotLightCount, MaxBenchmarkLights);
        config.movingLightCount = std::min(
            config.movingLightCount,
            config.directionalLightCount + config.pointLightCount + config.spotLightCount);
        config.dynamicMeshCount = std::min(config.dynamicMeshCount, config.renderableCount);
        config.dynamicMeshMutationCountPerFrame =
            std::min(config.dynamicMeshMutationCountPerFrame, config.dynamicMeshCount);
        config.particleEmitterCount = std::min(config.particleEmitterCount, MaxBenchmarkParticleEmitters);
        if (!std::isfinite(config.particleEmissionRate) || config.particleEmissionRate < 0.0)
            config.particleEmissionRate = 0.0;
        config.particleEmissionRate = std::min(config.particleEmissionRate, 20000.0);
        config.particleMaxParticles = std::clamp(config.particleMaxParticles,
                                                 1u,
                                                 MaxBenchmarkParticlesPerEmitter);
        config.particleMeshEmitterCount = std::min(config.particleMeshEmitterCount,
                                                   config.particleEmitterCount);
        config.particleMaxSimulatedParticles =
            std::min(config.particleMaxSimulatedParticles,
                     MaxBenchmarkParticleEmitters * MaxBenchmarkParticlesPerEmitter);
        config.particleMaxRenderedParticles =
            std::min(config.particleMaxRenderedParticles,
                     MaxBenchmarkParticleEmitters * MaxBenchmarkParticlesPerEmitter);
        config.particleMaxSpawnedPerFrame =
            std::min(config.particleMaxSpawnedPerFrame,
                     MaxBenchmarkParticleEmitters * MaxBenchmarkParticlesPerEmitter);
        config.renderWidth = std::clamp(config.renderWidth, 16u, 16384u);
        config.renderHeight = std::clamp(config.renderHeight, 16u, 16384u);
        if (!std::isfinite(config.hitchThresholdMs) || config.hitchThresholdMs < 0.0)
            config.hitchThresholdMs = 0.0;
        config.hitchThresholdMs = std::min(config.hitchThresholdMs, 10000.0);
        config.hitchContextFrames = std::min(config.hitchContextFrames, 16u);
        config.maxHitchEvents = std::min(config.maxHitchEvents, 64u);
        if (config.measuredFrames > 0)
        {
            config.screenshotMeasuredFrame =
                std::min(config.screenshotMeasuredFrame, config.measuredFrames - 1u);
        }
        if (config.seed == 0)
            config.seed = 1;
    }

    bool applyConfigOverride(RenderingBenchmarkConfig& config,
                             std::string_view key,
                             std::string_view value,
                             std::string* error)
    {
        auto fail = [&](std::string message)
        {
            if (error != nullptr)
                *error = std::move(message);
            return false;
        };
        auto unsignedValue = [&](uint32_t& destination)
        {
            bool ok = false;
            const uint32_t parsed = parseUnsigned(value, ok);
            if (!ok)
                return false;
            destination = parsed;
            return true;
        };
        auto boolValue = [&](bool& destination)
        {
            bool ok = false;
            const bool parsed = parseBool(value, ok);
            if (!ok)
                return false;
            destination = parsed;
            return true;
        };
        auto doubleValue = [&](double& destination)
        {
            bool ok = false;
            const double parsed = parseDouble(value, ok);
            if (!ok)
                return false;
            destination = parsed;
            return true;
        };

        bool ok = true;
        if (key == "seed") ok = unsignedValue(config.seed);
        else if (key == "warmup-frames" || key == "warmup_frames") ok = unsignedValue(config.warmupFrames);
        else if (key == "measured-frames" || key == "measured_frames") ok = unsignedValue(config.measuredFrames);
        else if (key == "renderable-count" || key == "renderable_count") ok = unsignedValue(config.renderableCount);
        else if (key == "visible-renderable-count" || key == "visible_renderable_count") ok = unsignedValue(config.visibleRenderableCount);
        else if (key == "unique-mesh-count" || key == "unique_mesh_count") ok = unsignedValue(config.uniqueMeshCount);
        else if (key == "unique-material-count" || key == "unique_material_count") ok = unsignedValue(config.uniqueMaterialCount);
        else if (key == "moving-object-count" || key == "moving_object_count") ok = unsignedValue(config.movingObjectCount);
        else if (key == "material-mutations" || key == "material_mutations") ok = unsignedValue(config.materialMutationCountPerFrame);
        else if (key == "topology-mutations" || key == "topology_mutations") ok = unsignedValue(config.topologyMutationCountPerFrame);
        else if (key == "directional-lights" || key == "directional_lights") ok = unsignedValue(config.directionalLightCount);
        else if (key == "point-lights" || key == "point_lights") ok = unsignedValue(config.pointLightCount);
        else if (key == "spot-lights" || key == "spot_lights") ok = unsignedValue(config.spotLightCount);
        else if (key == "moving-lights" || key == "moving_lights") ok = unsignedValue(config.movingLightCount);
        else if (key == "dynamic-mesh-count" || key == "dynamic_mesh_count") ok = unsignedValue(config.dynamicMeshCount);
        else if (key == "dynamic-mesh-mutations" || key == "dynamic_mesh_mutations") ok = unsignedValue(config.dynamicMeshMutationCountPerFrame);
        else if (key == "particle-emitter-count" || key == "particle_emitter_count") ok = unsignedValue(config.particleEmitterCount);
        else if (key == "particle-emission-rate" || key == "particle_emission_rate") ok = doubleValue(config.particleEmissionRate);
        else if (key == "particle-max-particles" || key == "particle_max_particles") ok = unsignedValue(config.particleMaxParticles);
        else if (key == "particle-mesh-emitter-count" || key == "particle_mesh_emitter_count") ok = unsignedValue(config.particleMeshEmitterCount);
        else if (key == "particle-max-simulated" || key == "particle_max_simulated") ok = unsignedValue(config.particleMaxSimulatedParticles);
        else if (key == "particle-max-rendered" || key == "particle_max_rendered") ok = unsignedValue(config.particleMaxRenderedParticles);
        else if (key == "particle-max-spawned-per-frame" || key == "particle_max_spawned_per_frame") ok = unsignedValue(config.particleMaxSpawnedPerFrame);
        else if (key == "world-text-count" || key == "world_text_count") ok = unsignedValue(config.worldTextCount);
        else if (key == "render-width" || key == "render_width") ok = unsignedValue(config.renderWidth);
        else if (key == "render-height" || key == "render_height") ok = unsignedValue(config.renderHeight);
        else if (key == "hitch-threshold-ms" || key == "hitch_threshold_ms") ok = doubleValue(config.hitchThresholdMs);
        else if (key == "hitch-context-frames" || key == "hitch_context_frames") ok = unsignedValue(config.hitchContextFrames);
        else if (key == "max-hitch-events" || key == "max_hitch_events") ok = unsignedValue(config.maxHitchEvents);
        else if (key == "request-screenshot" || key == "request_screenshot") ok = boolValue(config.requestScreenshot);
        else if (key == "screenshot-measured-frame" || key == "screenshot_measured_frame") ok = unsignedValue(config.screenshotMeasuredFrame);
        else if (key == "enable-pbr" || key == "enable_pbr") ok = boolValue(config.enablePbr);
        else if (key == "enable-normal-maps" || key == "enable_normal_maps") ok = boolValue(config.enableNormalMaps);
        else if (key == "enable-ibl" || key == "enable_ibl") ok = boolValue(config.enableIbl);
        else if (key == "enable-ui" || key == "enable_ui") ok = boolValue(config.enableUi);
        else if (key == "enable-frustum-culling" || key == "enable_frustum_culling") ok = boolValue(config.enableFrustumCulling);
        else if (key == "mode")
        {
            if (value == "cpu_smoke" || value == "smoke")
                config.mode = BenchmarkRunMode::CpuSmoke;
            else if (value == "gpu_runtime" || value == "gpu")
                config.mode = BenchmarkRunMode::GpuRuntime;
            else
                ok = false;
        }
        else
        {
            return fail("unknown benchmark override: " + std::string(key));
        }

        if (!ok)
            return fail("invalid value for benchmark override " + std::string(key) + ": " + std::string(value));
        sanitizeConfig(config);
        return true;
    }

    StatisticSummary summarizeSamples(std::vector<double> samples)
    {
        StatisticSummary summary;
        samples.erase(
            std::remove_if(samples.begin(), samples.end(), [](double value) { return !std::isfinite(value); }),
            samples.end());
        if (samples.empty())
            return summary;

        std::sort(samples.begin(), samples.end());
        summary.sampleCount = static_cast<uint64_t>(samples.size());
        summary.minimum = samples.front();
        summary.maximum = samples.back();
        summary.median = percentile(samples, 0.50);
        summary.p90 = percentile(samples, 0.90);
        summary.p95 = percentile(samples, 0.95);
        summary.p99 = percentile(samples, 0.99);
        summary.p999 = percentile(samples, 0.999);
        summary.mean = std::accumulate(samples.begin(), samples.end(), 0.0) /
            static_cast<double>(samples.size());

        double variance = 0.0;
        for (double value : samples)
            variance += (value - summary.mean) * (value - summary.mean);
        variance /= static_cast<double>(samples.size());
        summary.standardDeviation = std::sqrt(variance);
        return summary;
    }

    BenchmarkRunResult runRenderingBenchmark(const RenderingBenchmarkConfig& inputConfig)
    {
        RenderingBenchmarkConfig config = inputConfig;
        sanitizeConfig(config);

        BenchmarkRunResult result;
        result.config = config;
        if (config.mode == BenchmarkRunMode::GpuRuntime)
        {
            result.warnings.push_back(
                "gpu_runtime requires the benchmark executable runtime path; CPU library harness ran cpu_smoke");
            result.config.mode = BenchmarkRunMode::CpuSmoke;
        }

        result.environment = collectBenchmarkEnvironment(result.config);
        result.gpuTimingSupported = false;
        result.gpuTimingAvailable = false;
        result.gpuTimingStatus =
            "CPU smoke mode does not create GPU timestamp queries";

        BenchmarkSceneState state;
        generateScene(state, result.config);
        gts::transform::TransformSystem::setDetailedMetricsEnabled(true);
        RenderGpuSystem::setDetailedMetricsEnabled(true);
        DynamicMeshBindingSystem::setDetailedMetricsEnabled(true);
        ParticleEmitterSystem::setDetailedMetricsEnabled(true);

        std::map<std::string, std::vector<double>> timingSamples;
        std::map<std::string, std::vector<double>> controllerTimingSamples;
        std::map<std::string, std::vector<double>> controllerFlushTimingSamples;
        std::map<std::string, std::vector<double>> controllerSubstageSamples;
        std::map<std::string, std::string> controllerGroups;
        std::map<std::string, uint64_t> counters;

        for (uint32_t i = 0; i < result.config.warmupFrames; ++i)
        {
            std::map<std::string, std::vector<double>> ignoredTimings;
            std::map<std::string, std::vector<double>> ignoredControllerTimings;
            std::map<std::string, std::vector<double>> ignoredControllerFlushTimings;
            std::map<std::string, std::vector<double>> ignoredControllerSubstages;
            std::map<std::string, std::string> ignoredControllerGroups;
            std::map<std::string, uint64_t> ignoredCounters;
            recordFrame(state,
                        result.config,
                        i,
                        ignoredTimings,
                        ignoredControllerTimings,
                        ignoredControllerFlushTimings,
                        ignoredControllerSubstages,
                        ignoredControllerGroups,
                        ignoredCounters);
        }

        for (uint32_t i = 0; i < result.config.measuredFrames; ++i)
        {
            recordFrame(state,
                        result.config,
                        result.config.warmupFrames + i,
                        timingSamples,
                        controllerTimingSamples,
                        controllerFlushTimingSamples,
                        controllerSubstageSamples,
                        controllerGroups,
                        counters);
        }

        for (auto& [key, samples] : timingSamples)
            result.timingsMs[key] = summarizeSamples(std::move(samples));
        for (auto& [key, samples] : controllerTimingSamples)
            result.controllerTimingsMs[key] = summarizeSamples(std::move(samples));
        for (auto& [key, samples] : controllerFlushTimingSamples)
            result.controllerFlushTimingsMs[key] = summarizeSamples(std::move(samples));
        for (auto& [key, samples] : controllerSubstageSamples)
            result.controllerSubstageTimingsMs[key] = summarizeSamples(std::move(samples));
        result.controllerTimingGroups = std::move(controllerGroups);
        result.counters = std::move(counters);
        result.invariantFailures = checkBenchmarkInvariants(result);
        gts::transform::TransformSystem::setDetailedMetricsEnabled(false);
        RenderGpuSystem::setDetailedMetricsEnabled(false);
        DynamicMeshBindingSystem::setDetailedMetricsEnabled(false);
        ParticleEmitterSystem::setDetailedMetricsEnabled(false);
        return result;
    }

    std::vector<std::string> validateBenchmarkResult(const BenchmarkRunResult& result)
    {
        std::vector<std::string> failures;
        if (result.config.presetName.empty())
            failures.push_back("preset name is empty");
        if (result.config.measuredFrames == 0)
            failures.push_back("measured frame count is zero");
        if (result.timingsMs.empty())
            failures.push_back("no timing summaries were emitted");
        if (result.gpuTimingAvailable && result.gpuTimingsMs.empty())
            failures.push_back("GPU timing is available but no GPU timing summaries were emitted");
        if (!result.gpuTimingAvailable && !result.gpuTimingsMs.empty())
            failures.push_back("GPU timing summaries were emitted while GPU timing is unavailable");
        if (result.environment.values.find("build_type") == result.environment.values.end())
            failures.push_back("environment metadata is missing build_type");
        if (result.environment.values.find("cpu") == result.environment.values.end())
            failures.push_back("environment metadata is missing cpu");

        for (const auto& [key, summary] : result.timingsMs)
        {
            if (summary.sampleCount != result.config.measuredFrames)
                failures.push_back(key + " sample count does not match measuredFrames");
            const double values[] = {
                summary.minimum,
                summary.median,
                summary.mean,
                summary.p90,
                summary.p95,
                summary.p99,
                summary.p999,
                summary.maximum,
                summary.standardDeviation
            };
            for (double value : values)
            {
                if (!std::isfinite(value))
                {
                    failures.push_back(key + " contains NaN or infinity");
                    break;
                }
            }
        }

        for (const auto& [key, summary] : result.gpuTimingsMs)
        {
            if (summary.sampleCount != result.config.measuredFrames)
                failures.push_back(key + " GPU sample count does not match measuredFrames");
            const double values[] = {
                summary.minimum,
                summary.median,
                summary.mean,
                summary.p90,
                summary.p95,
                summary.p99,
                summary.p999,
                summary.maximum,
                summary.standardDeviation
            };
            for (double value : values)
            {
                if (!std::isfinite(value))
                {
                    failures.push_back(key + " GPU timing contains NaN or infinity");
                    break;
                }
            }
        }

        auto validateTimingMap =
            [&](const std::map<std::string, StatisticSummary>& timings,
                const std::string& label)
            {
                for (const auto& [key, summary] : timings)
                {
                    if (summary.sampleCount != result.config.measuredFrames)
                        failures.push_back(label + "." + key + " sample count does not match measuredFrames");
                    const double values[] = {
                        summary.minimum,
                        summary.median,
                        summary.mean,
                        summary.p90,
                        summary.p95,
                        summary.p99,
                        summary.p999,
                        summary.maximum,
                        summary.standardDeviation
                    };
                    for (double value : values)
                    {
                        if (!std::isfinite(value))
                        {
                            failures.push_back(label + "." + key + " contains NaN or infinity");
                            break;
                        }
                    }
                }
            };
        validateTimingMap(result.controllerTimingsMs, "controller_timings_ms");
        validateTimingMap(result.controllerFlushTimingsMs, "controller_flush_timings_ms");
        validateTimingMap(result.controllerSubstageTimingsMs, "controller_substages_ms");

        return failures;
    }

    std::vector<std::string> checkBenchmarkInvariants(const BenchmarkRunResult& result)
    {
        std::vector<std::string> failures;
        auto counter = [&](const std::string& key) -> uint64_t
        {
            auto it = result.counters.find(key);
            return it == result.counters.end() ? 0u : it->second;
        };

        const uint64_t frames = result.config.measuredFrames;
        if (counter("material_full_scans") != 0)
            failures.push_back("material_full_scans must remain zero in benchmark workloads");
        if (counter("render_gpu_full_scan_visited") != 0)
            failures.push_back("RenderGpuSystem performed a full renderable scan during measured frames");

        if (result.config.materialMutationCountPerFrame == 0 &&
            result.config.topologyMutationCountPerFrame == 0 &&
            counter("material_queued") != 0)
        {
            failures.push_back("steady-state material workload queued material sync work");
        }

        if (result.config.materialMutationCountPerFrame == 0 &&
            result.config.topologyMutationCountPerFrame == 0 &&
            counter("material_synchronized") != 0)
        {
            failures.push_back("steady-state material workload synchronized materials after warmup");
        }

        if (result.config.movingObjectCount == 0 &&
            result.config.dynamicMeshCount == 0 &&
            counter("object_uploads") != 0)
        {
            failures.push_back("static workload produced object uploads after warmup");
        }

        if (result.config.presetName == "moving_static_control" ||
            result.config.presetName == "static_64k_control")
        {
            if (counter("render_gpu_queued") != 0)
                failures.push_back("static moving-control workload queued render-transform sync after warmup");
            if (counter("render_gpu_changed_syncs") != 0)
                failures.push_back("static moving-control workload synchronized render transforms after warmup");
        }

        if ((result.config.presetName == "moving_independent" ||
             result.config.presetName == "moving_sparse" ||
             result.config.presetName == "moving_dense" ||
             result.config.presetName == "upload_only_pressure" ||
             result.config.presetName == "submit_object_upload_pressure" ||
             result.config.presetName == "gtsscene3_64k_moving_cubes" ||
             result.config.presetName == "moving_64k_independent") &&
            result.config.movingObjectCount > 0)
        {
            const uint64_t expectedUpdates =
                static_cast<uint64_t>(result.config.movingObjectCount) * frames;
            if (counter("logical_object_updates") != expectedUpdates)
                failures.push_back("moving-object preset logical update count does not match movingObjectCount");
            if (counter("object_upload_commands") != expectedUpdates)
                failures.push_back("moving-object preset upload command count does not match movingObjectCount");
            if (counter("render_gpu_changed_syncs") != expectedUpdates)
                failures.push_back("moving-object preset render sync count does not match movingObjectCount");
            if (counter("render_gpu_object_upload_requests") != expectedUpdates)
                failures.push_back("moving-object preset render upload requests do not match movingObjectCount");
        }

        if ((result.config.presetName == "moving_deep_hierarchy" ||
             result.config.presetName == "moving_wide_hierarchy" ||
             result.config.presetName == "moving_64k_wide_hierarchy") &&
            result.config.movingObjectCount > 0)
        {
            const uint64_t rootUpdates =
                static_cast<uint64_t>(result.config.movingObjectCount) * frames;
            const uint64_t logicalUpdates = counter("logical_object_updates");
            const uint64_t uploadCommands = counter("object_upload_commands");
            if (logicalUpdates <= rootUpdates)
                failures.push_back("hierarchy moving-object preset did not fan out transform updates");
            if (uploadCommands != logicalUpdates)
                failures.push_back("hierarchy moving-object preset upload commands do not match logical updates");
            if (counter("render_gpu_changed_syncs") != logicalUpdates)
                failures.push_back("hierarchy moving-object preset render sync count does not match logical updates");
        }

        if (result.config.presetName == "dynamic_mesh_static_control")
        {
            if (counter("dynamic_mesh_changed") != 0)
                failures.push_back("dynamic mesh static control processed changed meshes after warmup");
            if (counter("mesh_uploads") != 0)
                failures.push_back("dynamic mesh static control uploaded meshes after warmup");
            if (counter("dynamic_mesh_vertex_bytes_uploaded") != 0 ||
                counter("dynamic_mesh_index_bytes_uploaded") != 0)
            {
                failures.push_back("dynamic mesh static control uploaded geometry bytes after warmup");
            }
        }

        if (result.config.dynamicMeshMutationCountPerFrame > 0 &&
            result.config.dynamicMeshCount > 0 &&
            result.config.presetName.rfind("dynamic_mesh_", 0) == 0)
        {
            const uint64_t expectedChanged =
                static_cast<uint64_t>(result.config.dynamicMeshMutationCountPerFrame) * frames;
            if (counter("dynamic_mesh_changed") != expectedChanged)
                failures.push_back("dynamic mesh mutation preset changed count does not match configuration");
        }

        if (result.config.presetName == "dynamic_mesh_capacity_stable")
        {
            if (counter("dynamic_mesh_gpu_reallocations") != 0)
                failures.push_back("capacity-stable dynamic mesh preset reallocated after warmup");
        }

        if (result.config.presetName == "dynamic_mesh_growth" &&
            result.config.dynamicMeshMutationCountPerFrame > 0 &&
            result.config.measuredFrames >= 60 &&
            counter("dynamic_mesh_gpu_reallocations") == 0)
        {
            failures.push_back("dynamic mesh growth preset did not reallocate after warmup");
        }

        if (result.config.presetName.rfind("submit_", 0) == 0)
        {
            if (result.config.mode != BenchmarkRunMode::GpuRuntime)
                failures.push_back("submit benchmark presets must run through gpu_runtime");
        }

        if (result.config.presetName == "submit_empty_baseline")
        {
            if (counter("visible_renderables") != 0)
                failures.push_back("submit empty baseline produced visible renderables");
            if (counter("render_commands") != 0)
                failures.push_back("submit empty baseline produced render commands");
            if (counter("draw_calls") != 0)
                failures.push_back("submit empty baseline produced draw calls");
            if (counter("particle_rendered") != 0)
                failures.push_back("submit empty baseline rendered particles");
        }

        if (result.config.presetName == "submit_draw_call_pressure")
        {
            if (counter("draw_calls") < frames)
                failures.push_back("submit draw-call pressure produced fewer than one draw call per frame");
            if (counter("render_commands") == 0)
                failures.push_back("submit draw-call pressure produced zero render commands");
        }

        if (result.config.presetName == "submit_state_change_pressure")
        {
            if (counter("descriptor_binds") == 0)
                failures.push_back("submit state-change pressure produced zero descriptor binds");
            if (counter("draw_calls") == 0)
                failures.push_back("submit state-change pressure produced zero draw calls");
        }

        if (result.config.presetName == "submit_object_upload_pressure")
        {
            if (counter("object_upload_commands") == 0)
                failures.push_back("submit object-upload pressure produced zero object upload commands");
            if (counter("physical_object_buffer_writes") == 0)
                failures.push_back("submit object-upload pressure produced zero physical object buffer writes");
            if (counter("physical_object_buffer_writes") != counter("object_upload_commands"))
                failures.push_back("submit object-upload pressure wrote more than the active frame object buffer");
        }

        if (result.config.presetName == "submit_particle_draw_pressure")
        {
            if (counter("particle_rendered") == 0)
                failures.push_back("submit particle draw pressure rendered zero particles");
            if (counter("particle_draw_commands") == 0)
                failures.push_back("submit particle draw pressure produced zero particle draw commands");
        }

        if (counter("snapshot_renderables") < counter("visible_renderables"))
            failures.push_back("visible renderable count exceeds snapshot renderable count");

        if (result.config.renderableCount > 0 && result.config.visibleRenderableCount > 0 &&
            counter("visible_renderables") == 0)
        {
            failures.push_back("visible renderable workload produced zero visible renderables");
        }

        if (result.config.renderableCount > 0 && result.config.visibleRenderableCount > 0 &&
            counter("render_commands") == 0)
        {
            failures.push_back("visible renderable workload produced zero render commands");
        }

        if (counter("authored_renderables") !=
            static_cast<uint64_t>(result.config.renderableCount) * frames)
        {
            failures.push_back("authored renderable counter does not match measured frame count");
        }

        if (!result.gpuTimingAvailable && counter("gpu_timing_available") != 0)
            failures.push_back("gpu_timing_available counter contradicts result metadata");

        if (result.config.requestScreenshot)
        {
            if (counter("screenshot_requested") != 1)
                failures.push_back("screenshot benchmark did not request exactly one capture");
            if (counter("screenshot_scheduled") != 1)
                failures.push_back("screenshot benchmark did not schedule exactly one capture");
            if (counter("screenshot_completed") != 1)
                failures.push_back("screenshot benchmark did not complete exactly one capture");
            if (counter("screenshot_skipped") != 0)
                failures.push_back("screenshot benchmark skipped a capture request");
            if (counter("screenshot_readback_bytes") == 0)
                failures.push_back("screenshot benchmark completed without readback bytes");
        }

        return failures;
    }

    std::string benchmarkResultToJson(const BenchmarkRunResult& result)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(6);
        out << "{\n";
        out << "  \"benchmark\": \"" << escapedJson(result.config.presetName) << "\",\n";
        out << "  \"preset_version\": " << result.config.presetVersion << ",\n";
        out << "  \"mode\": \"" << escapedJson(benchmarkRunModeName(result.config.mode)) << "\",\n";
        out << "  \"seed\": " << result.config.seed << ",\n";
        out << "  \"warmup_frames\": " << result.config.warmupFrames << ",\n";
        out << "  \"measured_frames\": " << result.config.measuredFrames << ",\n";
        out << "  \"render_resolution\": {\n";
        out << "    \"width\": " << result.config.renderWidth << ",\n";
        out << "    \"height\": " << result.config.renderHeight << "\n";
        out << "  },\n";
        out << "  \"gpu_supported\": " << (result.gpuTimingSupported ? "true" : "false") << ",\n";
        out << "  \"gpu_timing\": {\n";
        out << "    \"supported\": " << (result.gpuTimingSupported ? "true" : "false") << ",\n";
        out << "    \"available\": " << (result.gpuTimingAvailable ? "true" : "false") << ",\n";
        out << "    \"status\": \"" << escapedJson(result.gpuTimingStatus) << "\"\n";
        out << "  },\n";
        out << "  \"config\": {\n";
        out << "    \"renderable_count\": " << result.config.renderableCount << ",\n";
        out << "    \"visible_renderable_count\": " << result.config.visibleRenderableCount << ",\n";
        out << "    \"unique_mesh_count\": " << result.config.uniqueMeshCount << ",\n";
        out << "    \"unique_material_count\": " << result.config.uniqueMaterialCount << ",\n";
        out << "    \"moving_object_count\": " << result.config.movingObjectCount << ",\n";
        out << "    \"material_mutation_count_per_frame\": " << result.config.materialMutationCountPerFrame << ",\n";
        out << "    \"topology_mutation_count_per_frame\": " << result.config.topologyMutationCountPerFrame << ",\n";
        out << "    \"directional_light_count\": " << result.config.directionalLightCount << ",\n";
        out << "    \"point_light_count\": " << result.config.pointLightCount << ",\n";
        out << "    \"spot_light_count\": " << result.config.spotLightCount << ",\n";
        out << "    \"moving_light_count\": " << result.config.movingLightCount << ",\n";
        out << "    \"dynamic_mesh_count\": " << result.config.dynamicMeshCount << ",\n";
        out << "    \"dynamic_mesh_mutation_count_per_frame\": " << result.config.dynamicMeshMutationCountPerFrame << ",\n";
        out << "    \"particle_emitter_count\": " << result.config.particleEmitterCount << ",\n";
        out << "    \"particle_emission_rate\": " << result.config.particleEmissionRate << ",\n";
        out << "    \"particle_max_particles\": " << result.config.particleMaxParticles << ",\n";
        out << "    \"particle_mesh_emitter_count\": " << result.config.particleMeshEmitterCount << ",\n";
        out << "    \"particle_max_simulated_particles\": " << result.config.particleMaxSimulatedParticles << ",\n";
        out << "    \"particle_max_rendered_particles\": " << result.config.particleMaxRenderedParticles << ",\n";
        out << "    \"particle_max_spawned_per_frame\": " << result.config.particleMaxSpawnedPerFrame << ",\n";
        out << "    \"world_text_count\": " << result.config.worldTextCount << ",\n";
        out << "    \"enable_pbr\": " << (result.config.enablePbr ? "true" : "false") << ",\n";
        out << "    \"enable_normal_maps\": " << (result.config.enableNormalMaps ? "true" : "false") << ",\n";
        out << "    \"enable_ibl\": " << (result.config.enableIbl ? "true" : "false") << ",\n";
        out << "    \"enable_ui\": " << (result.config.enableUi ? "true" : "false") << ",\n";
        out << "    \"enable_frustum_culling\": " << (result.config.enableFrustumCulling ? "true" : "false") << ",\n";
        out << "    \"hitch_threshold_ms\": " << result.config.hitchThresholdMs << ",\n";
        out << "    \"hitch_context_frames\": " << result.config.hitchContextFrames << ",\n";
        out << "    \"max_hitch_events\": " << result.config.maxHitchEvents << ",\n";
        out << "    \"request_screenshot\": " << (result.config.requestScreenshot ? "true" : "false") << ",\n";
        out << "    \"screenshot_measured_frame\": " << result.config.screenshotMeasuredFrame << ",\n";
        out << "    \"screenshot_output_directory\": \""
            << escapedJson(result.config.screenshotOutputDirectory) << "\"\n";
        out << "  },\n";
        out << "  \"environment\": {\n";
        size_t index = 0;
        for (const auto& [key, value] : result.environment.values)
        {
            out << "    \"" << escapedJson(key) << "\": \"" << escapedJson(value) << "\"";
            out << (++index < result.environment.values.size() ? ",\n" : "\n");
        }
        out << "  },\n";
        out << "  \"timings_ms\": {\n";
        index = 0;
        for (const auto& [key, summary] : result.timingsMs)
        {
            out << "    \"" << escapedJson(key) << "\": {\n";
            out << "      \"min\": " << summary.minimum << ",\n";
            out << "      \"median\": " << summary.median << ",\n";
            out << "      \"mean\": " << summary.mean << ",\n";
            out << "      \"p90\": " << summary.p90 << ",\n";
            out << "      \"p95\": " << summary.p95 << ",\n";
            out << "      \"p99\": " << summary.p99 << ",\n";
            out << "      \"p999\": " << summary.p999 << ",\n";
            out << "      \"max\": " << summary.maximum << ",\n";
            out << "      \"stddev\": " << summary.standardDeviation << ",\n";
            out << "      \"sample_count\": " << summary.sampleCount << "\n";
            out << "    }" << (++index < result.timingsMs.size() ? ",\n" : "\n");
        }
        out << "  },\n";
        out << "  \"gpu_timings_ms\": {\n";
        index = 0;
        for (const auto& [key, summary] : result.gpuTimingsMs)
        {
            out << "    \"" << escapedJson(key) << "\": {\n";
            out << "      \"min\": " << summary.minimum << ",\n";
            out << "      \"median\": " << summary.median << ",\n";
            out << "      \"mean\": " << summary.mean << ",\n";
            out << "      \"p90\": " << summary.p90 << ",\n";
            out << "      \"p95\": " << summary.p95 << ",\n";
            out << "      \"p99\": " << summary.p99 << ",\n";
            out << "      \"p999\": " << summary.p999 << ",\n";
            out << "      \"max\": " << summary.maximum << ",\n";
            out << "      \"stddev\": " << summary.standardDeviation << ",\n";
            out << "      \"sample_count\": " << summary.sampleCount << "\n";
            out << "    }" << (++index < result.gpuTimingsMs.size() ? ",\n" : "\n");
        }
        out << "  },\n";
        out << "  \"controller_timings_ms\": {\n";
        index = 0;
        for (const auto& [key, summary] : result.controllerTimingsMs)
        {
            const auto groupIt = result.controllerTimingGroups.find(key);
            out << "    \"" << escapedJson(key) << "\": {\n";
            out << "      \"group\": \"" << escapedJson(groupIt == result.controllerTimingGroups.end()
                                                        ? "Unknown"
                                                        : groupIt->second) << "\",\n";
            out << "      \"min\": " << summary.minimum << ",\n";
            out << "      \"median\": " << summary.median << ",\n";
            out << "      \"mean\": " << summary.mean << ",\n";
            out << "      \"p90\": " << summary.p90 << ",\n";
            out << "      \"p95\": " << summary.p95 << ",\n";
            out << "      \"p99\": " << summary.p99 << ",\n";
            out << "      \"p999\": " << summary.p999 << ",\n";
            out << "      \"max\": " << summary.maximum << ",\n";
            out << "      \"stddev\": " << summary.standardDeviation << ",\n";
            out << "      \"sample_count\": " << summary.sampleCount << "\n";
            out << "    }" << (++index < result.controllerTimingsMs.size() ? ",\n" : "\n");
        }
        out << "  },\n";
        out << "  \"controller_flush_timings_ms\": {\n";
        index = 0;
        for (const auto& [key, summary] : result.controllerFlushTimingsMs)
        {
            out << "    \"" << escapedJson(key) << "\": {\n";
            out << "      \"median\": " << summary.median << ",\n";
            out << "      \"p95\": " << summary.p95 << ",\n";
            out << "      \"p99\": " << summary.p99 << ",\n";
            out << "      \"p999\": " << summary.p999 << ",\n";
            out << "      \"sample_count\": " << summary.sampleCount << "\n";
            out << "    }" << (++index < result.controllerFlushTimingsMs.size() ? ",\n" : "\n");
        }
        out << "  },\n";
        out << "  \"controller_substages_ms\": {\n";
        index = 0;
        for (const auto& [key, summary] : result.controllerSubstageTimingsMs)
        {
            out << "    \"" << escapedJson(key) << "\": {\n";
            out << "      \"median\": " << summary.median << ",\n";
            out << "      \"p95\": " << summary.p95 << ",\n";
            out << "      \"p99\": " << summary.p99 << ",\n";
            out << "      \"p999\": " << summary.p999 << ",\n";
            out << "      \"sample_count\": " << summary.sampleCount << "\n";
            out << "    }" << (++index < result.controllerSubstageTimingsMs.size() ? ",\n" : "\n");
        }
        out << "  },\n";
        out << "  \"counters\": {\n";
        index = 0;
        for (const auto& [key, value] : result.counters)
        {
            out << "    \"" << escapedJson(key) << "\": " << value;
            out << (++index < result.counters.size() ? ",\n" : "\n");
        }
        out << "  },\n";
        out << "  \"hitch_capture\": {\n";
        out << "    \"enabled\": " << (result.config.hitchThresholdMs > 0.0 ? "true" : "false") << ",\n";
        out << "    \"threshold_ms\": " << result.config.hitchThresholdMs << ",\n";
        out << "    \"context_frames\": " << result.config.hitchContextFrames << ",\n";
        out << "    \"max_events\": " << result.config.maxHitchEvents << ",\n";
        out << "    \"events\": [";
        for (size_t eventIndex = 0; eventIndex < result.hitchEvents.size(); ++eventIndex)
        {
            const BenchmarkHitchEvent& event = result.hitchEvents[eventIndex];
            out << (eventIndex == 0 ? "\n" : ",\n");
            out << "      {\n";
            out << "        \"trigger_frame_index\": " << event.triggerFrameIndex << ",\n";
            out << "        \"trigger_measured_frame_index\": " << event.triggerMeasuredFrameIndex << ",\n";
            out << "        \"trigger_frame_cpu_ms\": " << event.triggerFrameCpuMs << ",\n";
            out << "        \"frames\": [";
            for (size_t frameIndex = 0; frameIndex < event.frames.size(); ++frameIndex)
            {
                const BenchmarkHitchFrame& frame = event.frames[frameIndex];
                out << (frameIndex == 0 ? "\n" : ",\n");
                out << "          {\n";
                out << "            \"frame_index\": " << frame.frameIndex << ",\n";
                out << "            \"measured_frame_index\": " << frame.measuredFrameIndex << ",\n";
                out << "            \"relative_frame\": " << frame.relativeFrame << ",\n";
                out << "            \"trigger\": " << (frame.trigger ? "true" : "false") << ",\n";
                out << "            \"timings_ms\": {\n";
                size_t timingIndex = 0;
                for (const auto& [key, value] : frame.timingsMs)
                {
                    out << "              \"" << escapedJson(key) << "\": " << value;
                    out << (++timingIndex < frame.timingsMs.size() ? ",\n" : "\n");
                }
                out << "            },\n";
                out << "            \"counters\": {\n";
                size_t counterIndex = 0;
                for (const auto& [key, value] : frame.counters)
                {
                    out << "              \"" << escapedJson(key) << "\": " << value;
                    out << (++counterIndex < frame.counters.size() ? ",\n" : "\n");
                }
                out << "            },\n";
                out << "            \"controller_timings_ms\": [";
                for (size_t controllerIndex = 0; controllerIndex < frame.controllerTimings.size(); ++controllerIndex)
                {
                    const BenchmarkControllerFrameTiming& controller = frame.controllerTimings[controllerIndex];
                    out << (controllerIndex == 0 ? "\n" : ",\n");
                    out << "              {"
                        << "\"name\": \"" << escapedJson(controller.name) << "\", "
                        << "\"group\": \"" << escapedJson(controller.group) << "\", "
                        << "\"update\": " << controller.updateMs << ", "
                        << "\"command_flush\": " << controller.commandFlushMs << "}";
                }
                out << (frame.controllerTimings.empty() ? "" : "\n            ") << "]\n";
                out << "          }";
            }
            out << (event.frames.empty() ? "" : "\n        ") << "]\n";
            out << "      }";
        }
        out << (result.hitchEvents.empty() ? "" : "\n    ") << "]\n";
        out << "  },\n";
        out << "  \"warnings\": [";
        for (size_t i = 0; i < result.warnings.size(); ++i)
        {
            out << (i == 0 ? "\n    " : ",\n    ")
                << "\"" << escapedJson(result.warnings[i]) << "\"";
        }
        out << (result.warnings.empty() ? "" : "\n  ") << "],\n";
        out << "  \"invariant_failures\": [";
        for (size_t i = 0; i < result.invariantFailures.size(); ++i)
        {
            out << (i == 0 ? "\n    " : ",\n    ")
                << "\"" << escapedJson(result.invariantFailures[i]) << "\"";
        }
        out << (result.invariantFailures.empty() ? "" : "\n  ") << "]\n";
        out << "}\n";
        return out.str();
    }

    bool writeBenchmarkResultJson(const BenchmarkRunResult& result,
                                  const std::string& outputPath,
                                  std::string* error)
    {
        std::ofstream output(outputPath);
        if (!output)
        {
            if (error != nullptr)
                *error = "failed to open benchmark output path: " + outputPath;
            return false;
        }
        output << benchmarkResultToJson(result);
        return true;
    }

    std::string benchmarkResultSummary(const BenchmarkRunResult& result)
    {
        auto timing = [&](const std::string& key) -> const StatisticSummary*
        {
            auto it = result.timingsMs.find(key);
            return it == result.timingsMs.end() ? nullptr : &it->second;
        };
        auto gpuTiming = [&](const std::string& key) -> const StatisticSummary*
        {
            auto it = result.gpuTimingsMs.find(key);
            return it == result.gpuTimingsMs.end() ? nullptr : &it->second;
        };
        auto counter = [&](const std::string& key) -> uint64_t
        {
            auto it = result.counters.find(key);
            return it == result.counters.end() ? 0u : it->second;
        };

        std::ostringstream out;
        out << std::fixed << std::setprecision(3);
        out << result.config.presetName << " v" << result.config.presetVersion
            << " [" << benchmarkRunModeName(result.config.mode) << "]\n";
        out << "CPU\n";
        if (const StatisticSummary* frame = timing("frame_cpu"))
            out << "frame median/p95/p99/p99.9/max: "
                << frame->median << " / " << frame->p95 << " / " << frame->p99
                << " / " << frame->p999 << " / " << frame->maximum << " ms\n";
        if (const StatisticSummary* controller = timing("controller_cpu"))
            out << "controller median/p95/p99: " << controller->median << " / "
                << controller->p95 << " / " << controller->p99 << " ms\n";
        if (const StatisticSummary* snapshot = timing("snapshot_build_cpu"))
            out << "snapshot median/p95/p99: " << snapshot->median << " / "
                << snapshot->p95 << " / " << snapshot->p99 << " ms\n";
        if (const StatisticSummary* extract = timing("command_extract_cpu"))
            out << "commands median/p95/p99: " << extract->median << " / "
                << extract->p95 << " / " << extract->p99 << " ms\n";
        if (const StatisticSummary* submit = timing("render_submit_cpu"))
            out << "submit wrapper median/p95/p99/max: " << submit->median << " / "
                << submit->p95 << " / " << submit->p99 << " / " << submit->maximum << " ms\n";

        struct SubmitTimingRow
        {
            const char* key = "";
            const char* label = "";
            bool aggregate = false;
        };
        const std::vector<SubmitTimingRow> submitRows = {
            {"backend_frame_cpu", "backend frame", true},
            {"backend_fence_wait_cpu", "fence wait", false},
            {"backend_acquire_cpu", "acquire", false},
            {"backend_image_wait_cpu", "image wait", false},
            {"backend_object_write_cpu", "object writes", false},
            {"backend_fence_reset_cpu", "fence reset", false},
            {"backend_cmd_reset_cpu", "cmd reset", false},
            {"backend_cmd_record_cpu", "cmd record", false},
            {"backend_queue_submit_cpu", "queue submit", false},
            {"backend_present_cpu", "present", false}
        };
        const SubmitTimingRow* largestSubmitRow = nullptr;
        const StatisticSummary* largestSubmitTiming = nullptr;
        bool printedSubmitHeader = false;
        for (const SubmitTimingRow& row : submitRows)
        {
            const StatisticSummary* summary = timing(row.key);
            if (summary == nullptr)
                continue;
            if (!printedSubmitHeader)
            {
                out << "Submit / Backend\n";
                printedSubmitHeader = true;
            }
            out << row.label << " median/p95/p99/max: "
                << summary->median << " / " << summary->p95 << " / "
                << summary->p99 << " / " << summary->maximum << " ms\n";
            if (!row.aggregate &&
                (largestSubmitTiming == nullptr || summary->median > largestSubmitTiming->median))
            {
                largestSubmitRow = &row;
                largestSubmitTiming = summary;
            }
        }
        if (largestSubmitRow != nullptr && largestSubmitTiming != nullptr)
        {
            out << "largest backend submit contributor: " << largestSubmitRow->label
                << " median " << largestSubmitTiming->median
                << " ms, max " << largestSubmitTiming->maximum << " ms\n";
        }
        if (!result.controllerTimingsMs.empty())
        {
            std::vector<std::pair<std::string, StatisticSummary>> rows(
                result.controllerTimingsMs.begin(),
                result.controllerTimingsMs.end());
            std::sort(rows.begin(),
                      rows.end(),
                      [](const auto& lhs, const auto& rhs)
                      {
                          if (lhs.second.median != rhs.second.median)
                              return lhs.second.median > rhs.second.median;
                          return lhs.first < rhs.first;
                      });

            out << "Controller Systems\n";
            const size_t rowCount = std::min<size_t>(rows.size(), 8u);
            for (size_t i = 0; i < rowCount; ++i)
            {
                const auto& [name, summary] = rows[i];
                const auto groupIt = result.controllerTimingGroups.find(name);
                out << name;
                if (groupIt != result.controllerTimingGroups.end())
                    out << " [" << groupIt->second << "]";
                out << " median/p95: " << summary.median << " / " << summary.p95 << " ms\n";
            }
        }
        out << "GPU\n";
        if (result.gpuTimingAvailable)
        {
            if (const StatisticSummary* frame = gpuTiming("frame"))
                out << "frame median/p95/p99/p99.9: " << frame->median << " / "
                    << frame->p95 << " / " << frame->p99 << " / " << frame->p999 << " ms\n";
            if (const StatisticSummary* scene = gpuTiming("scene"))
                out << "scene median/p95/p99: " << scene->median << " / "
                    << scene->p95 << " / " << scene->p99 << " ms\n";
            if (const StatisticSummary* particles = gpuTiming("particles"))
                out << "particles median/p95: " << particles->median << " / " << particles->p95 << " ms\n";
            if (const StatisticSummary* ui = gpuTiming("ui"))
                out << "ui median/p95: " << ui->median << " / " << ui->p95 << " ms\n";
        }
        else
        {
            out << result.gpuTimingStatus << "\n";
        }
        const uint64_t frames = std::max<uint64_t>(1u, result.config.measuredFrames);
        out << "render_commands total: " << counter("render_commands") << "\n";
        out << "visible_renderables total: " << counter("visible_renderables") << "\n";
        out << "draw_calls/pipeline_switches/descriptor_binds per frame: "
            << static_cast<double>(counter("draw_calls")) / static_cast<double>(frames)
            << " / "
            << static_cast<double>(counter("pipeline_switches")) / static_cast<double>(frames)
            << " / "
            << static_cast<double>(counter("descriptor_binds")) / static_cast<double>(frames) << "\n";
        out << "texture_switches per frame: "
            << static_cast<double>(counter("texture_switches")) / static_cast<double>(frames) << "\n";
        out << "particle emitters visible/culled per frame: "
            << static_cast<double>(counter("particle_visible_emitters")) / static_cast<double>(frames)
            << " / "
            << static_cast<double>(counter("particle_culled_emitters")) / static_cast<double>(frames) << "\n";
        out << "particles simulated/rendered/clipped per frame: "
            << static_cast<double>(counter("particle_simulated")) / static_cast<double>(frames)
            << " / "
            << static_cast<double>(counter("particle_rendered")) / static_cast<double>(frames)
            << " / "
            << static_cast<double>(counter("particle_budget_clipped")) / static_cast<double>(frames) << "\n";
        out << "material_synchronized total: " << counter("material_synchronized") << "\n";
        out << "object_uploads total: " << counter("object_uploads") << "\n";
        out << "render_gpu_full_scan_visited/frame: "
            << static_cast<double>(counter("render_gpu_full_scan_visited")) / static_cast<double>(frames) << "\n";
        out << "render_gpu_queued/frame: "
            << static_cast<double>(counter("render_gpu_queued")) / static_cast<double>(frames) << "\n";
        out << "render_gpu_changed_syncs/frame: "
            << static_cast<double>(counter("render_gpu_changed_syncs")) / static_cast<double>(frames) << "\n";
        out << "render_gpu_object_upload_requests/frame: "
            << static_cast<double>(counter("render_gpu_object_upload_requests")) / static_cast<double>(frames) << "\n";
        out << "transform_work_items/batches per frame: "
            << static_cast<double>(counter("transform_work_items")) / static_cast<double>(frames)
            << " / "
            << static_cast<double>(counter("transform_batches")) / static_cast<double>(frames) << "\n";
        out << "render_sync_work_items/batches per frame: "
            << static_cast<double>(counter("render_gpu_sync_work_items")) / static_cast<double>(frames)
            << " / "
            << static_cast<double>(counter("render_gpu_sync_batches")) / static_cast<double>(frames) << "\n";
        out << "snapshot_work_items/batches per frame: "
            << static_cast<double>(counter("snapshot_update_work_items")) / static_cast<double>(frames)
            << " / "
            << static_cast<double>(counter("snapshot_update_batches")) / static_cast<double>(frames) << "\n";
        out << "logical_object_updates/frame: "
            << static_cast<double>(counter("logical_object_updates")) / static_cast<double>(frames) << "\n";
        out << "object_upload_commands/frame: "
            << static_cast<double>(counter("object_upload_commands")) / static_cast<double>(frames) << "\n";
        out << "physical_object_buffer_writes/frame: "
            << static_cast<double>(counter("physical_object_buffer_writes")) / static_cast<double>(frames) << "\n";
        out << "bytes_written_object_buffer/frame: "
            << static_cast<double>(counter("bytes_written_object_buffer")) / static_cast<double>(frames) << "\n";
        if (result.config.requestScreenshot ||
            counter("screenshot_requested") != 0 ||
            counter("screenshot_scheduled") != 0 ||
            counter("screenshot_completed") != 0 ||
            counter("screenshot_skipped") != 0)
        {
            out << "Screenshot\n";
            out << "requested/scheduled/completed/skipped: "
                << counter("screenshot_requested") << " / "
                << counter("screenshot_scheduled") << " / "
                << counter("screenshot_completed") << " / "
                << counter("screenshot_skipped") << "\n";
            out << "pending_gpu/pending_write observations: "
                << counter("screenshot_pending_gpu") << " / "
                << counter("screenshot_pending_write") << "\n";
            out << "readback_bytes total: " << counter("screenshot_readback_bytes") << "\n";
        }
        if (result.config.dynamicMeshCount != 0 ||
            counter("dynamic_mesh_changed") != 0 ||
            counter("dynamic_mesh_candidates") != 0)
        {
            out << "Dynamic Mesh\n";
            out << "changed/frame: "
                << static_cast<double>(counter("dynamic_mesh_changed")) / static_cast<double>(frames) << "\n";
            out << "unchanged_skipped/frame: "
                << static_cast<double>(counter("dynamic_mesh_unchanged_skipped")) / static_cast<double>(frames) << "\n";
            out << "failed_versions_skipped/frame: "
                << static_cast<double>(counter("dynamic_mesh_failed_version_skipped")) / static_cast<double>(frames) << "\n";
            out << "gpu_reallocations total: " << counter("dynamic_mesh_gpu_reallocations") << "\n";
            out << "in_place_updates/frame: "
                << static_cast<double>(counter("dynamic_mesh_in_place_updates")) / static_cast<double>(frames) << "\n";
            out << "vertices_processed/frame: "
                << static_cast<double>(counter("dynamic_mesh_vertices_processed")) / static_cast<double>(frames) << "\n";
            out << "indices_processed/frame: "
                << static_cast<double>(counter("dynamic_mesh_indices_processed")) / static_cast<double>(frames) << "\n";
            out << "bounds_recomputed/frame: "
                << static_cast<double>(counter("dynamic_mesh_bounds_recomputed")) / static_cast<double>(frames) << "\n";
            out << "cpu_bytes_copied/frame: "
                << static_cast<double>(counter("dynamic_mesh_cpu_bytes_copied")) / static_cast<double>(frames) << "\n";
            out << "vertex_bytes_uploaded/frame: "
                << static_cast<double>(counter("dynamic_mesh_vertex_bytes_uploaded")) / static_cast<double>(frames) << "\n";
            out << "index_bytes_uploaded/frame: "
                << static_cast<double>(counter("dynamic_mesh_index_bytes_uploaded")) / static_cast<double>(frames) << "\n";

            const std::vector<std::string> dynamicSubstages = {
                "DynamicMeshBindingSystem.candidate_discovery",
                "DynamicMeshBindingSystem.version_checks",
                "DynamicMeshBindingSystem.validation",
                "DynamicMeshBindingSystem.bounds",
                "DynamicMeshBindingSystem.geometry_preparation",
                "DynamicMeshBindingSystem.temporary_copy",
                "DynamicMeshBindingSystem.resource_allocation",
                "DynamicMeshBindingSystem.vertex_upload",
                "DynamicMeshBindingSystem.index_upload",
                "DynamicMeshBindingSystem.publication",
                "DynamicMeshBindingSystem.invalidation",
                "DynamicMeshBindingSystem.cleanup"
            };
            bool printedSubstageHeader = false;
            for (const std::string& key : dynamicSubstages)
            {
                auto it = result.controllerSubstageTimingsMs.find(key);
                if (it == result.controllerSubstageTimingsMs.end())
                    continue;
                if (it->second.median == 0.0 && it->second.p95 == 0.0)
                    continue;
                if (!printedSubstageHeader)
                {
                    out << "Dynamic Mesh Substages\n";
                    printedSubstageHeader = true;
                }
                out << key.substr(std::string("DynamicMeshBindingSystem.").size())
                    << " median/p95: " << it->second.median << " / " << it->second.p95 << " ms\n";
            }
        }
        if (!result.invariantFailures.empty())
            out << "invariant_failures: " << result.invariantFailures.size() << "\n";
        if (!result.hitchEvents.empty())
        {
            out << "Hitches\n";
            out << "threshold_ms: " << result.config.hitchThresholdMs
                << " events: " << result.hitchEvents.size() << "\n";
            const size_t eventCount = std::min<size_t>(result.hitchEvents.size(), 4u);
            for (size_t i = 0; i < eventCount; ++i)
            {
                const BenchmarkHitchEvent& event = result.hitchEvents[i];
                out << "frame " << event.triggerFrameIndex
                    << " measured#" << event.triggerMeasuredFrameIndex
                    << " cpu: " << event.triggerFrameCpuMs
                    << " ms context_frames: " << event.frames.size() << "\n";
                const BenchmarkHitchFrame* triggerFrame = nullptr;
                for (const BenchmarkHitchFrame& frame : event.frames)
                {
                    if (frame.trigger)
                    {
                        triggerFrame = &frame;
                        break;
                    }
                }
                if (triggerFrame != nullptr)
                {
                    auto timingValue = [&](const std::string& key) -> double
                    {
                        auto it = triggerFrame->timingsMs.find(key);
                        return it == triggerFrame->timingsMs.end() ? 0.0 : it->second;
                    };
                    auto counterValue = [&](const std::string& key) -> uint64_t
                    {
                        auto it = triggerFrame->counters.find(key);
                        return it == triggerFrame->counters.end() ? 0u : it->second;
                    };
                    out << "  trigger cpu sim/controller/snapshot/submit: "
                        << timingValue("simulation_cpu") << " / "
                        << timingValue("controller_cpu") << " / "
                        << timingValue("snapshot_build_cpu") << " / "
                        << timingValue("render_submit_cpu") << " ms\n";
                    out << "  ticks/transforms/uploads/physical_writes: "
                        << counterValue("simulation_ticks") << " / "
                        << counterValue("transform_updates") << " / "
                        << counterValue("object_uploads") << " / "
                        << counterValue("physical_object_buffer_writes") << "\n";
                }
            }
        }
        return out.str();
    }
}
