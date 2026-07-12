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
        uint32_t particleEmitterCount = 0;
        uint32_t worldTextCount = 0;

        bool enablePbr = true;
        bool enableNormalMaps = true;
        bool enableIbl = true;
        bool enableUi = false;
        bool enableFrustumCulling = true;

        uint32_t renderWidth = 1280;
        uint32_t renderHeight = 720;
        BenchmarkRunMode mode = BenchmarkRunMode::CpuSmoke;
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
        double maximum = 0.0;
        double standardDeviation = 0.0;
        uint64_t sampleCount = 0;
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
        std::map<std::string, uint64_t> counters;
        std::vector<std::string> warnings;
        std::vector<std::string> invariantFailures;
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
