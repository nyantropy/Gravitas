#include "BenchmarkRunner.h"
#include "BenchmarkReport.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace
{
    uint32_t number(const std::string& text, uint32_t minimum, uint32_t maximum)
    {
        uint32_t   value  = 0;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || value < minimum || value > maximum)
            throw std::runtime_error("Invalid integer: " + text);
        return value;
    }

    std::vector<uint32_t> sizes(const std::string& text)
    {
        std::vector<uint32_t> result;
        size_t                start = 0;
        do
        {
            const auto end   = text.find(',', start);
            const auto value = number(text.substr(start, end == std::string::npos ? end : end - start), 1, 1000000);
            if (std::find(result.begin(), result.end(), value) != result.end())
                throw std::runtime_error("Duplicate workload size");
            result.push_back(value);
            if (end == std::string::npos)
                break;
            start = end + 1;
        } while (true);
        return result;
    }
} // namespace

int main(int argc, char** argv)
{
    using namespace gts::performance;
    try
    {
        Parameters            parameters;
        std::string           suite, workload, output = "gts-performance.json", invocation;
        std::vector<uint32_t> counts;
        bool                  hasCount = false, hasSizes = false, hasWarmup = false, hasSamples = false;
        for (int i = 1; i < argc; ++i)
        {
            const std::string argument = argv[i];
            invocation += (invocation.empty() ? "" : " ") + argument;
            if (argument == "--help")
            {
                std::cout << "GtsPerformanceBenchmarks --suite quick|scaling\n"
                             "  --workload NAME --count N | --sizes 1000,10000,...\n"
                             "  --warmup N --samples N --seed N --dt SECONDS --output FILE\n"
                             "  --list | --self-test\nCPU only; one fixed tick per sample. No automatic catch-up or "
                             "GPU measurement.\n";
                return 0;
            }
            if (argument == "--list")
            {
                for (const auto& entry : catalog())
                    std::cout << entry.name << " ceiling=" << entry.ceiling << " " << entry.description << " "
                              << entry.unavailableReason << '\n';
                return 0;
            }
            if (argument == "--self-test")
            {
                selfTest();
                std::cout << "Performance harness self-test passed (8 entities, 2 samples, repeated counters)\n";
                return 0;
            }
            if (i + 1 >= argc)
                throw std::runtime_error("Missing value for " + argument);
            const std::string value = argv[++i];
            invocation += " " + value;
            if (argument == "--suite")
                suite = value;
            else if (argument == "--workload")
                workload = value;
            else if (argument == "--output")
                output = value;
            else if (argument == "--count")
            {
                counts   = {number(value, 1, 1000000)};
                hasCount = true;
            }
            else if (argument == "--sizes")
            {
                counts   = sizes(value);
                hasSizes = true;
            }
            else if (argument == "--warmup")
            {
                parameters.warmup = number(value, 0, 100000);
                hasWarmup         = true;
            }
            else if (argument == "--samples")
            {
                parameters.samples = number(value, 1, 100000);
                hasSamples         = true;
            }
            else if (argument == "--seed")
                parameters.seed = number(value, 0, std::numeric_limits<uint32_t>::max());
            else if (argument == "--dt")
            {
                size_t consumed = 0;
                parameters.dt   = std::stod(value, &consumed);
                if (consumed != value.size() || !std::isfinite(parameters.dt) || parameters.dt < 0.000001 ||
                    parameters.dt > 1)
                    throw std::runtime_error("dt must be finite and between 0.000001 and 1 second");
            }
            else
                throw std::runtime_error("Unknown option: " + argument);
        }
        if ((!suite.empty() && !workload.empty()) || (hasCount && hasSizes) || (hasCount && workload.empty()))
            throw std::runtime_error("Choose suite OR workload; --count requires workload and excludes --sizes");
        if (suite.empty() && workload.empty())
            suite = "quick";
        if (!suite.empty() && suite != "quick" && suite != "scaling")
            throw std::runtime_error("Unknown suite: " + suite);
        if (suite == "scaling")
        {
            if (!hasWarmup)
                parameters.warmup = 3;
            if (!hasSamples)
                parameters.samples = 10;
        }
        if (counts.empty())
            counts = suite == "scaling"
                         ? std::vector<uint32_t>{1000, 10000, 25000, 50000, 100000, 250000, 500000, 1000000}
                         : std::vector<uint32_t>{1000};
        std::vector<Result> results;
        bool                failed = false, selected = false;
        for (const auto& entry : catalog())
        {
            if (!workload.empty() && entry.name != workload)
                continue;
            selected = true;
            for (uint32_t count : counts)
            {
                parameters.count = count;
                auto result      = run(entry, parameters);
                printSummary(result);
                failed = failed || result.status == "failed" || (!workload.empty() && result.status == "unsupported");
                results.push_back(std::move(result));
                // Persist completed and failed runs incrementally, outside measured work.
                writeReport(makeReport(invocation, results), output);
            }
        }
        if (!selected)
            throw std::runtime_error("Unknown workload: " + workload);
        std::cout << "JSON: " << output << " (unsupported/deferred cases are recorded explicitly)\n";
        return failed ? 1 : 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "GtsPerformanceBenchmarks: " << error.what() << '\n';
        return 2;
    }
}
