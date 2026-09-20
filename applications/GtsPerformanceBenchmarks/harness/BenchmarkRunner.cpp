#include "BenchmarkRunner.h"

#include "TransformScaling.h"
#include "ModelScaling.h"
#include <chrono>
#include <stdexcept>

namespace gts::performance
{
    const std::vector<WorkloadDescription>& catalog()
    {
        static const std::vector<WorkloadDescription> workloads = {
            {"static-transforms", 1000000, "Clean flat transforms after initial resolution", {}},
            {"moving-transforms", 1000000, "Author and resolve every flat transform each tick", {}},
            {"transform-animation", 1000000, "Engine transform rotation animation and resolution", {}},
            {"static-model-instances", 100000, "Shared triangle definition, independent instances, CPU extraction", {}},
            {"moving-model-instances", 100000, "Moving instances plus CPU model frame extraction", {}},
            {"shared-material-model-instances", 100000, "One shared model-wide material override", {}},
            {"varied-material-model-instances", 100000, "One distinct material override per instance", {}},
            {"mostly-visible-model-instances", 0, {}, "Deferred: camera/frustum and renderer visibility integration"},
            {"mostly-culled-model-instances", 0, {}, "Deferred: camera/frustum and renderer visibility integration"},
            {"skinned-model-instances", 0, {}, "Deferred: versioned rig/clip fixture and animation/palette workload"}};
        return workloads;
    }

    Result run(const WorkloadDescription& workload, const Parameters& parameters)
    {
        Result result;
        result.workload   = workload.name;
        result.parameters = parameters;
        if (!workload.unavailableReason.empty() || parameters.count > workload.ceiling)
        {
            result.status  = "unsupported";
            result.message = workload.unavailableReason.empty()
                                 ? "Documented workload ceiling is " + std::to_string(workload.ceiling)
                                 : workload.unavailableReason;
            return result;
        }
        const auto  start = std::chrono::steady_clock::now();
        std::string stage = "setup";
        try
        {
            auto instance = workload.name.find("model-instances") != std::string::npos
                                ? makeModelWorkload(workload.name, parameters)
                                : makeTransformWorkload(workload.name, parameters);
            result.setupMs =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            stage = "warmup";
            for (uint32_t tick = 0; tick < parameters.warmup; ++tick)
                instance->step(tick);
            stage = "measurement";
            result.samples.reserve(parameters.samples);
            for (uint32_t tick = 0; tick < parameters.samples; ++tick)
                result.samples.push_back(instance->step(parameters.warmup + tick));
        }
        catch (const std::exception& error)
        {
            result.status  = "failed";
            result.message = stage + ": " + error.what();
        }
        return result;
    }

    void selfTest()
    {
        auto require = [](bool condition, const char* message)
        {
            if (!condition)
                throw std::runtime_error(message);
        };
        const auto stats = summarize({4, 1, 3, 2});
        require(stats.count == 4 && stats.median == 2.5 && stats.mean == 2.5 && stats.p95 == 4,
                "Statistics contract failed");
        require(summarize({}).count == 0, "Empty statistics contract failed");
        Parameters parameters;
        parameters.count   = 8;
        parameters.warmup  = 1;
        parameters.samples = 2;
        for (const auto& workload : catalog())
        {
            if (!workload.unavailableReason.empty())
            {
                require(run(workload, parameters).status == "unsupported", "Deferred workload was silently run");
                continue;
            }
            const auto first  = run(workload, parameters);
            const auto second = run(workload, parameters);
            if (first.status != "ok" || second.status != "ok")
                throw std::runtime_error(workload.name + " self-test: " + first.message + " " + second.message);
            require(first.samples.size() == 2 && second.samples.size() == 2, "Wrong sample count");
            const bool dirty = workload.name == "moving-transforms" || workload.name == "transform-animation" ||
                               workload.name == "moving-model-instances";
            const bool model = workload.name.find("model-instances") != std::string::npos;
            for (size_t i = 0; i < first.samples.size(); ++i)
            {
                const auto& sample = first.samples[i];
                require(sample.counters == second.samples[i].counters, "Repeated workload counters differ");
                require(model ? sample.counters.at("entities") >= 8 : sample.counters.at("entities") == 8,
                        "Entity count mismatch");
                require(sample.counters.at("world_transforms_updated") == (dirty ? 8u : 0u), "Dirty work mismatch");
                require(sample.counters.at("model_instances") == (model ? 8u : 0u), "Instance count mismatch");
                require(sample.counters.at("authored_triangles") == (model ? 8u : 0u),
                        "Fixture triangle count mismatch");
                require(!sample.timings.at("gpu_frame_ms"), "CPU workload fabricated a GPU time");
                if (model)
                    require(sample.counters.at("static_draw_candidates") == 8, "Model extraction count mismatch");
                for (const auto& [name, value] : sample.timings)
                    if (value)
                        summarize({*value});
            }
            auto oversized  = parameters;
            oversized.count = workload.ceiling + 1;
            require(run(workload, oversized).status == "unsupported", "Workload ceiling not enforced");
        }
    }
} // namespace gts::performance
