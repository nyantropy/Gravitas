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
    const BenchmarkPreset* gtsScene3Preset = findBenchmarkPreset("gtsscene3_64k_moving_cubes");
    ok &= require(gtsScene3Preset != nullptr, "GtsScene3 hitch preset exists");
    ok &= require(gtsScene3Preset->config.renderableCount == 64000,
                  "GtsScene3 hitch preset keeps the 64k cube count");
    ok &= require(gtsScene3Preset->config.movingObjectCount == 64000,
                  "GtsScene3 hitch preset moves every cube");
    ok &= require(near(gtsScene3Preset->config.hitchThresholdMs, 8.0),
                  "GtsScene3 hitch preset has an 8 ms capture threshold");
    const BenchmarkPreset* moving64k = findBenchmarkPreset("moving_64k_independent");
    ok &= require(moving64k != nullptr, "M1 64k independent readiness preset exists");
    ok &= require(moving64k->config.renderableCount == 64000,
                  "M1 64k independent readiness preset keeps the 64k count");
    const BenchmarkPreset* static64k = findBenchmarkPreset("static_64k_control");
    ok &= require(static64k != nullptr, "M1 static 64k readiness preset exists");
    ok &= require(static64k->config.movingObjectCount == 0,
                  "M1 static 64k readiness preset has no measured movement");
    ok &= require(findBenchmarkPreset("moving_64k_wide_hierarchy") != nullptr,
                  "M1 wide hierarchy readiness preset exists");
    const BenchmarkPreset* screenshotPreset = findBenchmarkPreset("screenshot_capture_smoke");
    ok &= require(screenshotPreset != nullptr, "screenshot capture smoke preset exists");
    ok &= require(screenshotPreset->config.mode == BenchmarkRunMode::GpuRuntime,
                  "screenshot capture smoke uses the GPU runtime path");
    ok &= require(screenshotPreset->config.requestScreenshot,
                  "screenshot capture smoke requests a screenshot");
    ok &= require(screenshotPreset->config.screenshotMeasuredFrame == 0,
                  "screenshot capture smoke requests during the measured window");

    RenderingBenchmarkConfig config = preset->config;
    std::string error;
    ok &= require(applyConfigOverride(config, "warmup-frames", "2", &error), "warmup override parses");
    ok &= require(applyConfigOverride(config, "measured-frames", "4", &error), "measured override parses");
    ok &= require(applyConfigOverride(config, "renderable-count", "12", &error), "renderable override parses");
    ok &= require(applyConfigOverride(config, "visible-renderable-count", "6", &error), "visible override parses");
    ok &= require(applyConfigOverride(config, "unique-material-count", "3", &error), "material override parses");
    ok &= require(applyConfigOverride(config, "dynamic-mesh-mutations", "2", &error),
                  "dynamic mesh mutation override parses");
    ok &= require(applyConfigOverride(config, "request-screenshot", "true", &error),
                  "screenshot request override parses");
    ok &= require(applyConfigOverride(config, "screenshot-measured-frame", "2", &error),
                  "screenshot measured frame override parses");
    ok &= require(config.warmupFrames == 2, "warmup override applied");
    ok &= require(config.measuredFrames == 4, "measured override applied");
    ok &= require(config.renderableCount == 12, "renderable override applied");
    ok &= require(config.visibleRenderableCount == 6, "visible override applied");
    ok &= require(config.uniqueMaterialCount == 3, "material override applied");
    ok &= require(config.dynamicMeshMutationCountPerFrame == 0,
                  "dynamic mesh mutation override is clamped by dynamic mesh count");
    ok &= require(config.requestScreenshot, "screenshot request override applied");
    ok &= require(config.screenshotMeasuredFrame == 2,
                  "screenshot measured frame override applied");
    ok &= require(!applyConfigOverride(config, "unknown-key", "1", &error), "unknown override fails");

    StatisticSummary stats = summarizeSamples({4.0, 1.0, 3.0, 2.0});
    ok &= require(stats.sampleCount == 4, "sample count recorded");
    ok &= require(near(stats.minimum, 1.0), "minimum recorded");
    ok &= require(near(stats.maximum, 4.0), "maximum recorded");
    ok &= require(near(stats.median, 2.5), "median uses interpolation");
    ok &= require(near(stats.mean, 2.5), "mean recorded");
    ok &= require(near(stats.p999, 3.997), "p99.9 uses interpolation");

    config.seed = 7;
    config.mode = BenchmarkRunMode::CpuSmoke;
    config.movingObjectCount = 0;
    config.dynamicMeshCount = 0;
    config.materialMutationCountPerFrame = 0;
    config.topologyMutationCountPerFrame = 0;
    config.requestScreenshot = false;
    config.screenshotMeasuredFrame = 0;
    BenchmarkRunResult result = runRenderingBenchmark(config);

    const std::vector<std::string> validationFailures = validateBenchmarkResult(result);
    for (const std::string& failure : validationFailures)
        std::fprintf(stderr, "validation failure: %s\n", failure.c_str());
    ok &= require(validationFailures.empty(), "benchmark result validates");
    ok &= require(result.invariantFailures.empty(), "static smoke invariants pass");
    ok &= require(!result.gpuTimingSupported, "cpu smoke reports no gpu timestamp support");
    ok &= require(!result.gpuTimingAvailable, "cpu smoke reports no gpu timing");
    ok &= require(result.gpuTimingsMs.empty(), "cpu smoke emits no gpu timing summaries");
    ok &= require(result.timingsMs.at("frame_cpu").sampleCount == config.measuredFrames,
                  "warmup frames excluded from samples");
    ok &= require(!result.controllerTimingsMs.empty(), "controller timing samples are emitted");
    ok &= require(result.controllerTimingsMs.count("TransformSystem") != 0,
                  "transform system timing is emitted with stable name");
    ok &= require(result.controllerTimingGroups.at("TransformSystem") == "RenderPrep",
                  "controller timing group metadata is preserved");
    ok &= require(result.controllerSubstageTimingsMs.count("TransformSystem.resolve_world") != 0,
                  "transform substage timing is emitted");
    ok &= require(result.controllerSubstageTimingsMs.count("TransformSystem.work_collection") != 0,
                  "transform batch-readiness substage timing is emitted");
    ok &= require(result.controllerSubstageTimingsMs.count("RenderGpuSystem.batch_processing") != 0,
                  "render sync batch-processing timing is emitted");
    ok &= require(result.controllerSubstageTimingsMs.count(
                      "RenderExtractionSnapshotBuilder.entry_refresh") != 0,
                  "snapshot entry-refresh timing is emitted");
    ok &= require(result.counters.at("material_full_scans") == 0,
                  "material full scans stay zero");
    ok &= require(result.counters.count("transform_batches") != 0,
                  "transform batch counter is emitted");
    ok &= require(result.counters.count("render_gpu_sync_batches") != 0,
                  "render sync batch counter is emitted");
    ok &= require(result.counters.count("snapshot_update_batches") != 0,
                  "snapshot batch counter is emitted");

    const std::string json = benchmarkResultToJson(result);
    ok &= require(json.find("\"benchmark\": \"static_geometry_small\"") != std::string::npos,
                  "json contains benchmark name");
    ok &= require(json.find("\"timings_ms\"") != std::string::npos,
                  "json contains timing object");
    ok &= require(json.find("\"gpu_timings_ms\"") != std::string::npos,
                  "json contains gpu timing object");
    ok &= require(json.find("\"p999\"") != std::string::npos,
                  "json contains p99.9 timing values");
    ok &= require(json.find("\"controller_timings_ms\"") != std::string::npos,
                  "json contains controller timing object");
    ok &= require(json.find("\"controller_substages_ms\"") != std::string::npos,
                  "json contains controller substage object");
    ok &= require(json.find("\"dynamic_mesh_mutation_count_per_frame\"") != std::string::npos,
                  "json contains dynamic mesh mutation config");
    ok &= require(json.find("\"request_screenshot\"") != std::string::npos,
                  "json contains screenshot request config");
    ok &= require(json.find("\"screenshot_output_directory\"") != std::string::npos,
                  "json contains screenshot output directory config");
    ok &= require(json.find("\"dynamic_mesh_changed\"") != std::string::npos,
                  "json contains dynamic mesh counters");
    ok &= require(json.find("\"screenshot_requested\"") != std::string::npos,
                  "json contains screenshot counters");
    ok &= require(json.find("\"render_gpu_sync_work_items\"") != std::string::npos,
                  "json contains render sync work item counters");
    ok &= require(json.find("\"snapshot_update_work_items\"") != std::string::npos,
                  "json contains snapshot work item counters");
    ok &= require(json.find("\"gpu_supported\": false") != std::string::npos,
                  "json contains gpu support flag");
    ok &= require(json.find("\"counters\"") != std::string::npos,
                  "json contains counters object");
    ok &= require(json.find("\"hitch_capture\"") != std::string::npos,
                  "json contains hitch capture object");

    std::string outputError;
    ok &= require(writeBenchmarkResultJson(result, "/tmp/gts_rendering_benchmark_support_test.json", &outputError),
                  "json output file can be written");

    BenchmarkRunResult screenshotInvariant;
    screenshotInvariant.config = screenshotPreset->config;
    screenshotInvariant.config.measuredFrames = 4;
    screenshotInvariant.config.renderableCount = 0;
    screenshotInvariant.config.visibleRenderableCount = 0;
    screenshotInvariant.counters["screenshot_requested"] = 1;
    screenshotInvariant.counters["screenshot_scheduled"] = 1;
    screenshotInvariant.counters["screenshot_completed"] = 1;
    screenshotInvariant.counters["screenshot_skipped"] = 0;
    screenshotInvariant.counters["screenshot_readback_bytes"] = 64 * 64 * 4;
    ok &= require(checkBenchmarkInvariants(screenshotInvariant).empty(),
                  "screenshot benchmark invariant accepts one completed capture");
    screenshotInvariant.counters["screenshot_completed"] = 0;
    ok &= require(!checkBenchmarkInvariants(screenshotInvariant).empty(),
                  "screenshot benchmark invariant rejects missing completion");

    RenderingBenchmarkConfig mutating = config;
    mutating.materialMutationCountPerFrame = 1;
    BenchmarkRunResult mutatingResult = runRenderingBenchmark(mutating);
    ok &= require(mutatingResult.counters.at("material_synchronized") == mutating.measuredFrames,
                  "one shared material mutation synchronizes once per measured frame");

    RenderingBenchmarkConfig moving = findBenchmarkPreset("moving_independent")->config;
    moving.warmupFrames = 2;
    moving.measuredFrames = 4;
    moving.renderableCount = 8;
    moving.visibleRenderableCount = 8;
    moving.movingObjectCount = 3;
    BenchmarkRunResult movingResult = runRenderingBenchmark(moving);
    ok &= require(movingResult.invariantFailures.empty(),
                  "moving independent invariants pass");
    ok &= require(movingResult.counters.at("logical_object_updates") ==
                      moving.measuredFrames * moving.movingObjectCount,
                  "moving independent logical update count is deterministic");
    ok &= require(movingResult.counters.at("object_upload_commands") ==
                      moving.measuredFrames * moving.movingObjectCount,
                  "moving independent upload command count is deterministic");
    ok &= require(movingResult.counters.at("transform_work_items") ==
                      moving.measuredFrames * moving.movingObjectCount,
                  "moving independent transform work items match changed objects");
    ok &= require(movingResult.counters.at("render_gpu_sync_work_items") ==
                      moving.measuredFrames * moving.movingObjectCount,
                  "moving independent render-sync work items match changed objects");

    RenderingBenchmarkConfig gtsSmall = gtsScene3Preset->config;
    gtsSmall.mode = BenchmarkRunMode::CpuSmoke;
    gtsSmall.warmupFrames = 2;
    gtsSmall.measuredFrames = 4;
    gtsSmall.renderableCount = 16;
    gtsSmall.visibleRenderableCount = 16;
    gtsSmall.movingObjectCount = 16;
    BenchmarkRunResult gtsSmallResult = runRenderingBenchmark(gtsSmall);
    ok &= require(gtsSmallResult.invariantFailures.empty(),
                  "GtsScene3 small smoke invariants pass");
    ok &= require(gtsSmallResult.counters.at("logical_object_updates") ==
                      gtsSmall.measuredFrames * gtsSmall.movingObjectCount,
                  "GtsScene3 small smoke moves every configured cube");
    ok &= require(gtsSmallResult.config.hitchThresholdMs > 0.0,
                  "GtsScene3 small smoke preserves hitch threshold config");

    RenderingBenchmarkConfig hierarchy = findBenchmarkPreset("moving_wide_hierarchy")->config;
    hierarchy.warmupFrames = 2;
    hierarchy.measuredFrames = 4;
    hierarchy.renderableCount = 12;
    hierarchy.visibleRenderableCount = 12;
    hierarchy.movingObjectCount = 3;
    BenchmarkRunResult hierarchyResult = runRenderingBenchmark(hierarchy);
    ok &= require(hierarchyResult.invariantFailures.empty(),
                  "moving hierarchy invariants pass");
    ok &= require(hierarchyResult.counters.at("logical_object_updates") >
                      hierarchy.measuredFrames * hierarchy.movingObjectCount,
                  "moving hierarchy fans out logical transform updates");
    ok &= require(hierarchyResult.counters.at("object_upload_commands") ==
                      hierarchyResult.counters.at("logical_object_updates"),
                  "moving hierarchy uploads each changed render object once");

    RenderingBenchmarkConfig dynamicStatic = findBenchmarkPreset("dynamic_mesh_static_control")->config;
    dynamicStatic.warmupFrames = 2;
    dynamicStatic.measuredFrames = 4;
    dynamicStatic.renderableCount = 8;
    dynamicStatic.visibleRenderableCount = 8;
    dynamicStatic.dynamicMeshCount = 8;
    dynamicStatic.dynamicMeshMutationCountPerFrame = 0;
    BenchmarkRunResult dynamicStaticResult = runRenderingBenchmark(dynamicStatic);
    ok &= require(dynamicStaticResult.invariantFailures.empty(),
                  "dynamic mesh static-control invariants pass");
    ok &= require(dynamicStaticResult.counters.at("dynamic_mesh_changed") == 0,
                  "dynamic mesh static-control processes no changed meshes after warmup");
    ok &= require(dynamicStaticResult.counters.at("dynamic_mesh_vertex_bytes_uploaded") == 0,
                  "dynamic mesh static-control uploads no vertex bytes after warmup");
    ok &= require(dynamicStaticResult.counters.at("dynamic_mesh_bounds_recomputed") == 0,
                  "dynamic mesh static-control recomputes no bounds after warmup");
    ok &= require(dynamicStaticResult.controllerSubstageTimingsMs.count(
                      "DynamicMeshBindingSystem.candidate_discovery") != 0,
                  "dynamic mesh substage timing is emitted");

    RenderingBenchmarkConfig dynamicSparse = findBenchmarkPreset("dynamic_mesh_sparse_mutation")->config;
    dynamicSparse.warmupFrames = 2;
    dynamicSparse.measuredFrames = 4;
    dynamicSparse.renderableCount = 8;
    dynamicSparse.visibleRenderableCount = 8;
    dynamicSparse.dynamicMeshCount = 8;
    dynamicSparse.dynamicMeshMutationCountPerFrame = 3;
    BenchmarkRunResult dynamicSparseResult = runRenderingBenchmark(dynamicSparse);
    ok &= require(dynamicSparseResult.invariantFailures.empty(),
                  "dynamic mesh sparse-mutation invariants pass");
    ok &= require(dynamicSparseResult.counters.at("dynamic_mesh_changed") ==
                      dynamicSparse.measuredFrames * dynamicSparse.dynamicMeshMutationCountPerFrame,
                  "dynamic mesh sparse-mutation changed count is deterministic");
    ok &= require(dynamicSparseResult.counters.at("dynamic_mesh_in_place_updates") ==
                      dynamicSparseResult.counters.at("dynamic_mesh_changed"),
                  "dynamic mesh sparse-mutation uses in-place updates after warmup");
    ok &= require(dynamicSparseResult.counters.at("dynamic_mesh_bounds_recomputed") ==
                      dynamicSparseResult.counters.at("dynamic_mesh_changed"),
                  "dynamic mesh sparse-mutation recomputes bounds for changed meshes");
    ok &= require(dynamicSparseResult.counters.at("dynamic_mesh_gpu_reallocations") == 0,
                  "dynamic mesh sparse-mutation avoids reallocations after warmup");

    RenderingBenchmarkConfig dynamicCapacity = findBenchmarkPreset("dynamic_mesh_capacity_stable")->config;
    dynamicCapacity.warmupFrames = 2;
    dynamicCapacity.measuredFrames = 4;
    dynamicCapacity.renderableCount = 8;
    dynamicCapacity.visibleRenderableCount = 8;
    dynamicCapacity.dynamicMeshCount = 8;
    dynamicCapacity.dynamicMeshMutationCountPerFrame = 4;
    BenchmarkRunResult dynamicCapacityResult = runRenderingBenchmark(dynamicCapacity);
    ok &= require(dynamicCapacityResult.invariantFailures.empty(),
                  "dynamic mesh capacity-stable invariants pass");
    ok &= require(dynamicCapacityResult.counters.at("dynamic_mesh_gpu_reallocations") == 0,
                  "dynamic mesh capacity-stable preset avoids reallocations after warmup");

    return ok ? 0 : 1;
}
