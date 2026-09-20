#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace gts::performance
{
    using Timings  = std::map<std::string, std::optional<double>>;
    using Counters = std::map<std::string, std::optional<uint64_t>>;

    struct Sample
    {
        uint32_t tick = 0;
        Timings  timings;
        Counters counters;
    };

    struct Statistics
    {
        size_t count   = 0;
        double minimum = 0, median = 0, mean = 0, p95 = 0, maximum = 0, standardDeviation = 0;
    };

    Statistics summarize(std::vector<double> values);
    Timings    emptyTimings();
    Counters   emptyCounters();
} // namespace gts::performance
