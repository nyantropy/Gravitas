#include "FontAssetIO.h"
#include "GtsJsonParser.h"
#include "InputBindingSerializer.h"
#include "ParticleEffectAssetIO.h"
#include "RenderingBenchmark.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    std::string readFile(const std::filesystem::path& path)
    {
        std::ifstream      file(path);
        std::ostringstream text;
        text << file.rdbuf();
        return text.str();
    }
}

int main()
{
    const auto directory = std::filesystem::temp_directory_path() / "gravitas_json_migration_test";
    try
    {
        std::filesystem::create_directories(directory);
        const std::string escaped = "quote\" slash\\ controls\b\f\n\t \xc3\xa4\xf0\x9f\x9a\x80";
        InputBinding      binding;
        binding.action            = escaped;
        binding.context           = escaped;
        binding.trigger.type      = InputTrigger::Type::MouseButton;
        binding.trigger.code      = 2;
        binding.trigger.modifiers = ModifierFlags::Shift | ModifierFlags::Ctrl;
        binding.mode              = ActivationMode::Repeated;
        binding.pausePolicy       = PausePolicy::AlwaysActive;
        binding.passthrough       = true;
        const auto inputJson      = serializeInputBindingDocument({binding});
        const auto input          = parseInputBindingDocument(inputJson);
        require(input && input->bindings.size() == 1 && input->bindings[0] == binding, "Input roundtrip");
        require(parseInputBindingDocument(R"({"version":1,"unknown":[null,1.5,{}],"bindings":[]})").has_value(),
                "Input unknown fields");
        require(!parseInputBindingDocument(R"({"version":1,"bindings":[{"action":"a","type":"mouse_button","code":"2x"}]})"),
                "Input invalid numeric code");
        require(!parseInputBindingDocument(R"({"bindings":[],"version":1.5})"), "Input fractional version");
        const auto inputPath = directory / "input.json";
        require(saveInputBindingDocumentToFile(inputPath.string(), {binding}), "Input save");
        const auto savedInput = readFile(inputPath);
        binding.action        = std::string(1, static_cast<char>(0xff));
        require(!saveInputBindingDocumentToFile(inputPath.string(), {binding}) && readFile(inputPath) == savedInput,
                "Input failed save overwrote file");

        FontAsset font;
        font.atlasPath            = escaped;
        font.atlasWidth           = 1024;
        font.atlasHeight          = 512;
        font.cellWidth            = 16;
        font.cellHeight           = 32;
        font.columns              = 64;
        font.charOrder            = escaped;
        font.lineHeight           = 1.2345678f;
        font.glyphDefaults        = {0.12345678f, 0.75f, 0.654321f};
        font.glyphOverrides['"']  = {0.25f, 0.5f, 0.75f};
        font.glyphOverrides['\\'] = {0.5f, 0.75f, 1.0f};
        const auto fontPath       = directory / "font.json";
        require(gts::fonts::saveFontAsset(fontPath.string(), font), "Font save");
        FontAsset loadedFont;
        require(gts::fonts::loadFontAsset(fontPath.string(), loadedFont), "Font load");
        require(loadedFont.atlasPath == escaped && loadedFont.charOrder == escaped &&
                    loadedFont.lineHeight == font.lineHeight &&
                    loadedFont.glyphDefaults.sizeX == font.glyphDefaults.sizeX &&
                    loadedFont.glyphOverrides.at('"').advance == 0.75f &&
                    loadedFont.glyphOverrides.at('\\').sizeX == 0.5f,
                "Font roundtrip");
        const auto savedFont = readFile(fontPath);
        font.lineHeight      = std::numeric_limits<float>::infinity();
        require(!gts::fonts::saveFontAsset(fontPath.string(), font) && readFile(fontPath) == savedFont,
                "Font failed save overwrote file");

        ParticleEffectAsset particle;
        particle.metadata.name = escaped;
        particle.emitters.emplace_back();
        const auto particlePath = directory / "particle.json";
        require(gts::particles::saveParticleEffectAsset(particlePath.string(), particle), "Particle save");
        ParticleEffectAsset loadedParticle;
        require(gts::particles::loadParticleEffectAsset(particlePath.string(), loadedParticle) &&
                    loadedParticle.metadata.name == escaped,
                "Particle Unicode roundtrip");

        gts::rendering::benchmarks::BenchmarkRunResult benchmark;
        benchmark.config.presetName = escaped;
        benchmark.counters["large"] = std::numeric_limits<uint64_t>::max();
        GtsJsonValue result;
        require(GtsJsonParser::parse(gts::rendering::benchmarks::benchmarkResultToJson(benchmark), result),
                "Benchmark JSON");
        require(result.find("benchmark")->asString() == escaped, "Benchmark escaping");
        require(result.find("counters")->find("large")->asUnsignedInteger() == std::numeric_limits<uint64_t>::max(),
                "Benchmark integer precision");
        std::filesystem::remove_all(directory);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        std::filesystem::remove_all(directory);
        return 1;
    }
}
