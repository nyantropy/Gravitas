#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "RenderingBenchmark.h"

namespace
{
    struct CliOptions
    {
        std::string preset = "static_geometry_small";
        std::string outputPath;
        bool listPresets = false;
        bool checkInvariants = false;
        bool quiet = false;
        std::vector<std::pair<std::string, std::string>> overrides;
    };

    void printUsage()
    {
        std::cout
            << "Usage: GtsRenderingBenchmarks [options]\n"
            << "\n"
            << "Options:\n"
            << "  --list-presets\n"
            << "  --preset <name>\n"
            << "  --output <path>\n"
            << "  --check-invariants\n"
            << "  --quiet\n"
            << "  --set <key=value>\n"
            << "  --mode <cpu_smoke|gpu_runtime>\n"
            << "  --warmup-frames <n>\n"
            << "  --measured-frames <n>\n"
            << "  --renderable-count <n>\n"
            << "  --visible-renderable-count <n>\n"
            << "  --unique-mesh-count <n>\n"
            << "  --unique-material-count <n>\n"
            << "  --seed <n>\n";
    }

    bool splitAssignment(std::string_view text, std::string& key, std::string& value)
    {
        const size_t equals = text.find('=');
        if (equals == std::string_view::npos || equals == 0 || equals + 1 >= text.size())
            return false;

        key = std::string(text.substr(0, equals));
        value = std::string(text.substr(equals + 1));
        return true;
    }

    bool parseArgs(int argc, char** argv, CliOptions& options)
    {
        for (int i = 1; i < argc; ++i)
        {
            const std::string_view arg = argv[i];
            auto requireValue = [&](const char* option) -> std::optional<std::string>
            {
                if (i + 1 >= argc)
                {
                    std::cerr << option << " requires a value\n";
                    return std::nullopt;
                }
                ++i;
                return std::string(argv[i]);
            };

            if (arg == "--help" || arg == "-h")
            {
                printUsage();
                std::exit(EXIT_SUCCESS);
            }
            if (arg == "--list-presets")
            {
                options.listPresets = true;
                continue;
            }
            if (arg == "--check-invariants")
            {
                options.checkInvariants = true;
                continue;
            }
            if (arg == "--quiet")
            {
                options.quiet = true;
                continue;
            }
            if (arg == "--preset")
            {
                auto value = requireValue("--preset");
                if (!value)
                    return false;
                options.preset = *value;
                continue;
            }
            if (arg == "--output")
            {
                auto value = requireValue("--output");
                if (!value)
                    return false;
                options.outputPath = *value;
                continue;
            }
            if (arg == "--set")
            {
                auto assignment = requireValue("--set");
                if (!assignment)
                    return false;
                std::string key;
                std::string value;
                if (!splitAssignment(*assignment, key, value))
                {
                    std::cerr << "--set requires key=value\n";
                    return false;
                }
                options.overrides.emplace_back(std::move(key), std::move(value));
                continue;
            }

            if (arg.rfind("--", 0) == 0)
            {
                auto value = requireValue(std::string(arg).c_str());
                if (!value)
                    return false;
                options.overrides.emplace_back(std::string(arg.substr(2)), *value);
                continue;
            }

            std::cerr << "unknown argument: " << arg << "\n";
            return false;
        }

        return true;
    }
}

int main(int argc, char** argv)
{
    using namespace gts::rendering::benchmarks;

    CliOptions options;
    if (!parseArgs(argc, argv, options))
    {
        printUsage();
        return EXIT_FAILURE;
    }

    if (options.listPresets)
    {
        for (const BenchmarkPreset& preset : standardBenchmarkPresets())
            std::cout << preset.name << " v" << preset.version << " - " << preset.description << "\n";
        return EXIT_SUCCESS;
    }

    const BenchmarkPreset* preset = findBenchmarkPreset(options.preset);
    if (preset == nullptr)
    {
        std::cerr << "unknown benchmark preset: " << options.preset << "\n";
        return EXIT_FAILURE;
    }

    RenderingBenchmarkConfig config = preset->config;
    for (const auto& [key, value] : options.overrides)
    {
        std::string error;
        if (!applyConfigOverride(config, key, value, &error))
        {
            std::cerr << error << "\n";
            return EXIT_FAILURE;
        }
    }

    BenchmarkRunResult result = runRenderingBenchmark(config);
    const std::vector<std::string> validationFailures = validateBenchmarkResult(result);
    if (!validationFailures.empty())
    {
        std::cerr << "benchmark result validation failed:\n";
        for (const std::string& failure : validationFailures)
            std::cerr << "  - " << failure << "\n";
        return EXIT_FAILURE;
    }

    if (!options.outputPath.empty())
    {
        std::string error;
        if (!writeBenchmarkResultJson(result, options.outputPath, &error))
        {
            std::cerr << error << "\n";
            return EXIT_FAILURE;
        }
    }

    if (!options.quiet)
        std::cout << benchmarkResultSummary(result);

    if (options.checkInvariants && !result.invariantFailures.empty())
    {
        std::cerr << "benchmark invariants failed:\n";
        for (const std::string& failure : result.invariantFailures)
            std::cerr << "  - " << failure << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
