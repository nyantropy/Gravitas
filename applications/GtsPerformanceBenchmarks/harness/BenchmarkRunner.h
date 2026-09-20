#pragma once

#include "BenchmarkMetrics.h"
#include <memory>

namespace gts::performance
{
    struct Parameters
    {
        uint32_t count   = 1000;
        uint32_t seed    = 1337;
        uint32_t warmup  = 2;
        uint32_t samples = 5;
        double   dt      = 1.0 / 60.0;
    };

    struct WorkloadDescription
    {
        std::string name;
        uint32_t    ceiling;
        std::string description;
        std::string unavailableReason;
    };

    class Workload
    {
        public:
        virtual ~Workload()                = default;
        virtual Sample step(uint32_t tick) = 0;
    };

    struct Result
    {
        std::string           workload;
        Parameters            parameters;
        std::string           status = "ok";
        std::string           message;
        std::optional<double> setupMs;
        std::vector<Sample>   samples;
    };

    const std::vector<WorkloadDescription>& catalog();
    Result                                  run(const WorkloadDescription& workload, const Parameters& parameters);
    void                                    selfTest();
} // namespace gts::performance
