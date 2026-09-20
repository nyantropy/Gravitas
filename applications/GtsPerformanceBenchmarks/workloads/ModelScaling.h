#pragma once
#include "BenchmarkRunner.h"

namespace gts::performance
{
    std::unique_ptr<Workload> makeModelWorkload(const std::string& name, const Parameters& parameters);
}
