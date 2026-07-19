#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gts::rendering::benchmarks
{
    enum class BenchmarkRunMode
    {
        CpuSmoke,
        GpuRuntime
    };

    struct RenderingBenchmarkConfig
    {
        std::string presetName = "static_geometry_small";
        uint32_t presetVersion = 1;
        uint32_t seed = 1;

        uint32_t warmupFrames = 120;
        uint32_t measuredFrames = 600;

        uint32_t renderableCount = 0;
        uint32_t visibleRenderableCount = 0;
        uint32_t uniqueMeshCount = 1;
        uint32_t uniqueMaterialCount = 1;
        uint32_t movingObjectCount = 0;
        uint32_t materialMutationCountPerFrame = 0;
        uint32_t topologyMutationCountPerFrame = 0;

        uint32_t directionalLightCount = 0;
        uint32_t pointLightCount = 0;
        uint32_t spotLightCount = 0;
        uint32_t movingLightCount = 0;

        uint32_t dynamicMeshCount = 0;
        uint32_t dynamicMeshMutationCountPerFrame = 0;
        uint32_t particleEmitterCount = 0;
        double particleEmissionRate = 24.0;
        uint32_t particleMaxParticles = 96;
        uint32_t particleMeshEmitterCount = 0;
        uint32_t particleMaxSimulatedParticles = 0;
        uint32_t particleMaxRenderedParticles = 0;
        uint32_t particleMaxSpawnedPerFrame = 0;
        uint32_t worldTextCount = 0;

        bool enablePbr = true;
        bool enableNormalMaps = true;
        bool enableIbl = true;
        bool enableUi = false;
        bool enableFrustumCulling = true;

        uint32_t renderWidth = 1280;
        uint32_t renderHeight = 720;
        BenchmarkRunMode mode = BenchmarkRunMode::CpuSmoke;

        double hitchThresholdMs = 0.0;
        uint32_t hitchContextFrames = 2;
        uint32_t maxHitchEvents = 8;

        bool requestScreenshot = false;
        uint32_t screenshotMeasuredFrame = 0;
        std::string screenshotOutputDirectory;
    };

    struct BenchmarkPreset
    {
        std::string name;
        std::string description;
        uint32_t version = 1;
        RenderingBenchmarkConfig config;
    };

    struct StatisticSummary
    {
        double minimum = 0.0;
        double median = 0.0;
        double mean = 0.0;
        double p90 = 0.0;
        double p95 = 0.0;
        double p99 = 0.0;
        double p999 = 0.0;
        double maximum = 0.0;
        double standardDeviation = 0.0;
        uint64_t sampleCount = 0;
    };

    struct BenchmarkControllerFrameTiming
    {
        std::string name;
        std::string group;
        double updateMs = 0.0;
        double commandFlushMs = 0.0;
    };

    struct BenchmarkHitchFrame
    {
        uint64_t frameIndex = 0;
        uint32_t measuredFrameIndex = 0;
        int32_t relativeFrame = 0;
        bool trigger = false;
        std::map<std::string, double> timingsMs;
        std::map<std::string, uint64_t> counters;
        std::vector<BenchmarkControllerFrameTiming> controllerTimings;
    };

    struct BenchmarkHitchEvent
    {
        uint64_t triggerFrameIndex = 0;
        uint32_t triggerMeasuredFrameIndex = 0;
        double triggerFrameCpuMs = 0.0;
        std::vector<BenchmarkHitchFrame> frames;
    };

    struct BenchmarkEnvironment
    {
        std::map<std::string, std::string> values;
    };

    struct BenchmarkRunResult
    {
        RenderingBenchmarkConfig config;
        BenchmarkEnvironment environment;
        std::map<std::string, StatisticSummary> timingsMs;
        std::map<std::string, StatisticSummary> gpuTimingsMs;
        std::map<std::string, StatisticSummary> controllerTimingsMs;
        std::map<std::string, StatisticSummary> controllerFlushTimingsMs;
        std::map<std::string, StatisticSummary> controllerSubstageTimingsMs;
        std::map<std::string, std::string> controllerTimingGroups;
        std::map<std::string, uint64_t> counters;
        std::vector<std::string> warnings;
        std::vector<std::string> invariantFailures;
        std::vector<BenchmarkHitchEvent> hitchEvents;
        bool gpuTimingSupported = false;
        bool gpuTimingAvailable = false;
        std::string gpuTimingStatus = "unavailable";
    };

    std::vector<BenchmarkPreset> standardBenchmarkPresets();
    const BenchmarkPreset* findBenchmarkPreset(std::string_view name);
    void sanitizeConfig(RenderingBenchmarkConfig& config);
    bool applyConfigOverride(RenderingBenchmarkConfig& config,
                             std::string_view key,
                             std::string_view value,
                             std::string* error = nullptr);
    const char* benchmarkRunModeName(BenchmarkRunMode mode);

    BenchmarkEnvironment collectBenchmarkEnvironment(const RenderingBenchmarkConfig& config);
    StatisticSummary summarizeSamples(std::vector<double> samples);

    BenchmarkRunResult runRenderingBenchmark(const RenderingBenchmarkConfig& config);
    std::vector<std::string> validateBenchmarkResult(const BenchmarkRunResult& result);
    std::vector<std::string> checkBenchmarkInvariants(const BenchmarkRunResult& result);

    std::string benchmarkResultToJson(const BenchmarkRunResult& result);
    bool writeBenchmarkResultJson(const BenchmarkRunResult& result,
                                  const std::string& outputPath,
                                  std::string* error = nullptr);
    std::string benchmarkResultSummary(const BenchmarkRunResult& result);
}
