#pragma once
#include "BenchmarkRunner.h"
#include "GtsJsonValue.h"

namespace gts::performance
{
    GtsJsonValue makeReport(const std::string& invocation, const std::vector<Result>& results);
    void         writeReport(const GtsJsonValue& report, const std::string& path);
    void         printSummary(const Result& result);
} // namespace gts::performance
