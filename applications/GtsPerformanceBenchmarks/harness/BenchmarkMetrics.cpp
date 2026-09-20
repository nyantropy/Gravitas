#include "BenchmarkMetrics.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace gts::performance
{
    Statistics summarize(std::vector<double> values)
    {
        Statistics result;
        if (values.empty())
            return result;
        for (double value : values)
            if (!std::isfinite(value) || value < 0)
                throw std::runtime_error("Non-finite or negative timing sample");
        std::sort(values.begin(), values.end());
        result.count    = values.size();
        result.minimum  = values.front();
        result.maximum  = values.back();
        result.median   = (values[(values.size() - 1) / 2] + values[values.size() / 2]) * 0.5;
        result.mean     = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
        result.p95      = values[static_cast<size_t>(std::ceil(values.size() * 0.95)) - 1];
        double variance = 0;
        for (double value : values)
            variance += (value - result.mean) * (value - result.mean);
        result.standardDeviation = std::sqrt(variance / values.size());
        return result;
    }

    Timings emptyTimings()
    {
        Timings result;
        for (const char* name : {"simulation_ms",
                                 "controller_ms",
                                 "transform_ms",
                                 "animation_ms",
                                 "model_extraction_ms",
                                 "render_preparation_ms",
                                 "render_gpu_ms",
                                 "snapshot_ms",
                                 "cpu_frame_ms",
                                 "gpu_frame_ms"})
            result[name] = std::nullopt;
        return result;
    }

    Counters emptyCounters()
    {
        Counters result;
        for (const char* name : {"entities",
                                 "model_instances",
                                 "authored_triangles",
                                 "distinct_override_materials",
                                 "transforms_queued",
                                 "transforms_processed",
                                 "world_transforms_updated",
                                 "static_draw_candidates",
                                 "skinned_draw_candidates",
                                 "visible_instances",
                                 "submitted_draws",
                                 "gpu_submissions"})
            result[name] = std::nullopt;
        return result;
    }
} // namespace gts::performance
