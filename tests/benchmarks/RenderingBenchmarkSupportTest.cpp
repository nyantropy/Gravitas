#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "RenderingBenchmark.h"

namespace
{
    bool require(bool condition, const char* message)
    {
        if (condition)
            return true;

        std::fprintf(stderr, "FAIL: %s\n", message);
        return false;
    }

    bool near(double lhs, double rhs, double epsilon = 0.00001)
    {
        return std::abs(lhs - rhs) <= epsilon;
    }
}

int main()
{
    using namespace gts::rendering::benchmarks;

    bool ok = true;

    const BenchmarkPreset* preset = findBenchmarkPreset("static_geometry_small");
    ok &= require(preset != nullptr, "static_geometry_small preset exists");
    ok &= require(findBenchmarkPreset("does_not_exist") == nullptr, "unknown preset is rejected");

    RenderingBenchmarkConfig config = preset->config;
    std::string error;
    ok &= require(applyConfigOverride(config, "warmup-frames", "2", &error), "warmup override parses");
    ok &= require(applyConfigOverride(config, "measured-frames", "4", &error), "measured override parses");
    ok &= require(applyConfigOverride(config, "renderable-count", "12", &error), "renderable override parses");
    ok &= require(applyConfigOverride(config, "visible-renderable-count", "6", &error), "visible override parses");
    ok &= require(applyConfigOverride(config, "unique-material-count", "3", &error), "material override parses");
    ok &= require(config.warmupFrames == 2, "warmup override applied");
    ok &= require(config.measuredFrames == 4, "measured override applied");
    ok &= require(config.renderableCount == 12, "renderable override applied");
    ok &= require(config.visibleRenderableCount == 6, "visible override applied");
    ok &= require(config.uniqueMaterialCount == 3, "material override applied");
    ok &= require(!applyConfigOverride(config, "unknown-key", "1", &error), "unknown override fails");

    StatisticSummary stats = summarizeSamples({4.0, 1.0, 3.0, 2.0});
    ok &= require(stats.sampleCount == 4, "sample count recorded");
    ok &= require(near(stats.minimum, 1.0), "minimum recorded");
    ok &= require(near(stats.maximum, 4.0), "maximum recorded");
    ok &= require(near(stats.median, 2.5), "median uses interpolation");
    ok &= require(near(stats.mean, 2.5), "mean recorded");

    config.seed = 7;
    config.mode = BenchmarkRunMode::CpuSmoke;
    config.movingObjectCount = 0;
    config.dynamicMeshCount = 0;
    config.materialMutationCountPerFrame = 0;
    config.topologyMutationCountPerFrame = 0;
    BenchmarkRunResult result = runRenderingBenchmark(config);

    const std::vector<std::string> validationFailures = validateBenchmarkResult(result);
    for (const std::string& failure : validationFailures)
        std::fprintf(stderr, "validation failure: %s\n", failure.c_str());
    ok &= require(validationFailures.empty(), "benchmark result validates");
    ok &= require(result.invariantFailures.empty(), "static smoke invariants pass");
    ok &= require(!result.gpuTimingAvailable, "cpu smoke reports no gpu timing");
    ok &= require(result.timingsMs.at("frame_cpu").sampleCount == config.measuredFrames,
                  "warmup frames excluded from samples");
    ok &= require(result.counters.at("material_full_scans") == 0,
                  "material full scans stay zero");

    const std::string json = benchmarkResultToJson(result);
    ok &= require(json.find("\"benchmark\": \"static_geometry_small\"") != std::string::npos,
                  "json contains benchmark name");
    ok &= require(json.find("\"timings_ms\"") != std::string::npos,
                  "json contains timing object");
    ok &= require(json.find("\"counters\"") != std::string::npos,
                  "json contains counters object");

    std::string outputError;
    ok &= require(writeBenchmarkResultJson(result, "/tmp/gts_rendering_benchmark_support_test.json", &outputError),
                  "json output file can be written");

    RenderingBenchmarkConfig mutating = config;
    mutating.materialMutationCountPerFrame = 1;
    BenchmarkRunResult mutatingResult = runRenderingBenchmark(mutating);
    ok &= require(mutatingResult.counters.at("material_synchronized") == mutating.measuredFrames,
                  "one shared material mutation synchronizes once per measured frame");

    return ok ? 0 : 1;
}
