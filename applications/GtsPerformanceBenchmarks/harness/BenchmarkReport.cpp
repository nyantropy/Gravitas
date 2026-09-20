#include "BenchmarkReport.h"

#include "BuildMetadata.h"
#include "GtsJsonParser.h"
#include "RuntimeAssetPolicy.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace gts::performance
{
    namespace
    {
        using Json = GtsJsonValue;
        template <typename T> Json optionalMap(const std::map<std::string, std::optional<T>>& values)
        {
            Json::Object object;
            for (const auto& [name, value] : values)
                object.emplace_back(name, value ? Json(*value) : Json{});
            return object;
        }

        Json summary(const Statistics& stats)
        {
            return Json::Object{{"count", stats.count},
                                {"min", stats.minimum},
                                {"median", stats.median},
                                {"mean", stats.mean},
                                {"p95", stats.p95},
                                {"max", stats.maximum},
                                {"standard_deviation", stats.standardDeviation}};
        }
    } // namespace

    GtsJsonValue makeReport(const std::string& invocation, const std::vector<Result>& results)
    {
        Json        build;
        std::string error;
        if (!GtsJsonParser::parse(BuildMetadataJson, build, &error))
            throw std::runtime_error("Invalid embedded build metadata: " + error);
        Json::Array descriptions;
        for (const auto& workload : catalog())
            descriptions.emplace_back(Json::Object{{"name", workload.name},
                                                   {"ceiling", workload.ceiling},
                                                   {"description", workload.description},
                                                   {"unavailable_reason", workload.unavailableReason}});
        Json::Array runs;
        for (const auto& result : results)
        {
            Json::Array  raw;
            Json::Object aggregates;
            for (const auto& [name, ignored] : emptyTimings())
            {
                std::vector<double> values;
                for (const auto& sample : result.samples)
                    if (const auto value = sample.timings.at(name))
                        values.push_back(*value);
                aggregates.emplace_back(name, values.empty() ? Json{} : summary(summarize(std::move(values))));
            }
            for (const auto& sample : result.samples)
                raw.emplace_back(Json::Object{{"tick", sample.tick},
                                              {"timings_ms", optionalMap(sample.timings)},
                                              {"counters", optionalMap(sample.counters)}});
            runs.emplace_back(Json::Object{{"workload", result.workload},
                                           {"status", result.status},
                                           {"message", result.message},
                                           {"parameters",
                                            Json::Object{{"count", result.parameters.count},
                                                         {"seed", result.parameters.seed},
                                                         {"warmup_ticks", result.parameters.warmup},
                                                         {"measured_samples", result.parameters.samples},
                                                         {"dt_seconds", result.parameters.dt}}},
                                           {"setup_ms", result.setupMs ? Json(*result.setupMs) : Json{}},
                                           {"aggregate_timings_ms", std::move(aggregates)},
                                           {"samples", std::move(raw)}});
        }
        const auto unixMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count();
        return Json::Object{
            {"schema_version", 1},
            {"workload_version", 1},
            {"invocation", invocation},
            {"unix_time_ms", unixMs},
            {"build", std::move(build)},
            {"hardware_threads", std::thread::hardware_concurrency()},
            {"settings",
             Json::Object{
                 {"execution", "one fixed simulation tick then controllers then optional CPU model extraction"},
                 {"selection", "gameplay"},
                 {"backend", "none"},
                 {"resource_provider", "none_cpu_realization"},
                 {"culling", false},
                 {"ui", false},
                 {"tools", false},
                 {"gpu_submission", false},
                 {"detailed_transform_metrics", false},
                 {"asset_policy",
                  gts::assets::runtimeSourceAssetFallbackAllowed() ? "development_fallback" : "cooked_only"}}},
            {"unavailable_measurements",
             Json::Object{
                 {"gpu_frame_ms", "No graphics backend or GPU timestamps in these CPU workloads"},
                 {"render_preparation_ms", "Renderer preparation is not installed"},
                 {"render_gpu_ms", "RenderGpuSystem is not installed"},
                 {"snapshot_ms", "Renderer snapshot builder is not installed; model extraction is measured separately"},
                 {"visible_instances", "No visibility/frustum pass is run"},
                 {"submitted_draws", "Draw candidates are CPU model output, not GPU submissions"},
                 {"gpu_submissions", "No backend submission is run"},
                 {"animation_ms",
                  "Null except transform-animation; then includes its sole simulation system dispatch/flush"},
                 {"model_extraction_ms", "Null for transform-only workloads"}}},
            {"catalog", std::move(descriptions)},
            {"runs", std::move(runs)}};
    }

    void writeReport(const GtsJsonValue& report, const std::string& path)
    {
        const auto parent = std::filesystem::path(path).parent_path();
        if (!parent.empty())
            std::filesystem::create_directories(parent);
        std::ofstream stream(path);
        if (!stream || !(stream << GtsJsonParser::serialize(report, 2) << '\n'))
            throw std::runtime_error("Cannot write benchmark report: " + path);
        stream.close();
        if (!stream)
            throw std::runtime_error("Cannot finish benchmark report: " + path);
    }

    void printSummary(const Result& result)
    {
        std::cout << result.workload << " count=" << result.parameters.count << " " << result.status;
        if (result.status == "ok")
        {
            std::cout << std::fixed << std::setprecision(3) << " setup=" << *result.setupMs << "ms";
            for (const char* name : {"cpu_frame_ms", "simulation_ms", "controller_ms", "model_extraction_ms"})
            {
                std::vector<double> values;
                for (const auto& sample : result.samples)
                    if (auto value = sample.timings.at(name))
                        values.push_back(*value);
                if (!values.empty())
                {
                    auto stats = summarize(std::move(values));
                    std::cout << " " << name << "(median/p95)=" << stats.median << "/" << stats.p95;
                }
            }
            if (!result.samples.empty())
                for (const char* name : {"entities",
                                         "model_instances",
                                         "authored_triangles",
                                         "world_transforms_updated",
                                         "static_draw_candidates"})
                    if (const auto value = result.samples.front().counters.at(name))
                        std::cout << " " << name << "=" << *value;
        }
        else
            std::cout << ": " << result.message;
        std::cout << '\n';
    }
} // namespace gts::performance
