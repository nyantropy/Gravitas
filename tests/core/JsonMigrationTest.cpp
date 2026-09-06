#include "UiLayout.h"
#include "UiTypes.h"
#include "UiBindingTypes.h"
#include "UiNavigationTypes.h"
#include "UiInteraction.h"
#include "UiAccessibilityTypes.h"
#include "UiSurface.h"
#include "Tween.h"
#include "WindowMode.h"
#include "PresentationSettings.h"
#include "FontAssetIO.h"
#include "GtsJsonParser.h"
#include "InputBindingSerializer.h"
#include "ParticleEffectAssetIO.h"
#include "RenderingBenchmark.h"
#include "UiSerialization.h"
#include "UiWidgetAsset.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace
{
    constexpr bool enumNamesRoundtrip(const auto& names)
    {
        for (const auto& entry : names)
        {
            if (gts::enumValue(names, entry.name) != entry.value ||
                gts::enumName(names, entry.value) != entry.name)
                return false;
        }
        return true;
    }

    static_assert(enumNamesRoundtrip(uiLayoutModeNames));
    static_assert(enumNamesRoundtrip(uiPositionModeNames));
    static_assert(enumNamesRoundtrip(uiSizeModeNames));
    static_assert(enumNamesRoundtrip(uiClipModeNames));
    static_assert(enumNamesRoundtrip(uiHorizontalAlignNames));
    static_assert(enumNamesRoundtrip(uiVerticalAlignNames));
    static_assert(enumNamesRoundtrip(uiTextWrapModeNames));
    static_assert(enumNamesRoundtrip(uiLayoutAxisNames));
    static_assert(enumNamesRoundtrip(uiLayoutAlignmentNames));
    static_assert(enumNamesRoundtrip(uiLayoutUnitNames));
    static_assert(enumNamesRoundtrip(uiDockEdgeNames));
    static_assert(enumNamesRoundtrip(uiBindablePropertyNames));
    static_assert(enumNamesRoundtrip(uiNavigationRoleNames));
    static_assert(enumNamesRoundtrip(uiNavigationDirectionNames));
    static_assert(enumNamesRoundtrip(uiSemanticRoleNames));
    static_assert(enumNamesRoundtrip(uiAccessibilityLiveRegionNames));
    static_assert(enumNamesRoundtrip(uiSurfaceKindNames));
    static_assert(enumNamesRoundtrip(gts::tween::tweenEaseNames));
    static_assert(enumNamesRoundtrip(uiWidgetAssetParameterTypeNames));
    static_assert(enumNamesRoundtrip(windowModeNames));
    static_assert(enumNamesRoundtrip(presentModePreferenceNames));
    static_assert(enumNamesRoundtrip(activationModeNames));
    static_assert(enumNamesRoundtrip(pausePolicyNames));
    static_assert(enumNamesRoundtrip(modifierFlagNames));
    static_assert(enumNamesRoundtrip(inputTriggerTypeNames));
    static_assert(!gts::enumValue(uiLayoutModeNames, "unknown"));
    static_assert(!gts::enumName(uiLayoutModeNames, static_cast<UiLayoutMode>(255)));

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
        GtsJsonValue numericParameters;
        require(GtsJsonParser::parse(R"({
            "id":"numbers", "type":"Label",
            "parameters":{"large":18446744073709551615,"precise":1.2345678901234567,
                          "yes":true,"no":false,"text":"unquoted","empty":""}
        })", numericParameters), "Numeric parameter fixture");
        UiSerializedWidget numericWidget;
        require(parseUiSerializedWidget(numericParameters, numericWidget), "Numeric widget parameters");
        require(numericWidget.parameters.at("yes") == "true" && numericWidget.parameters.at("no") == "false"
                && numericWidget.parameters.at("text") == "unquoted" && numericWidget.parameters.at("empty").empty(),
                "Widget boolean and string parameter text");
        require(numericWidget.parameters.at("large") == "18446744073709551615",
                "Widget parameter integer precision");
        GtsJsonValue preciseNumber;
        require(GtsJsonParser::parse(numericWidget.parameters.at("precise"), preciseNumber)
                && preciseNumber.tryNumber() == std::optional<double>{1.2345678901234567},
                "Widget parameter floating precision");
        UiWidgetAssetDefinition numericAsset;
        require(parseUiWidgetAssetDefinition(R"({
            "schema":1,"id":"numbers","version":1,
            "parameters":[
                {"name":"large","type":"Number","default":18446744073709551615},
                {"name":"precise","type":"Number","default":1.2345678901234567},
                {"name":"yes","type":"Bool","default":true},
                {"name":"no","type":"Bool","default":false},
                {"name":"text","type":"String","default":"unquoted"}
            ],
            "root":{"id":"label","type":"Label"}
        })", numericAsset), "Numeric asset parameters");
        require(numericAsset.parameters.at("yes").defaultValue == "true"
                && numericAsset.parameters.at("no").defaultValue == "false"
                && numericAsset.parameters.at("text").defaultValue == "unquoted",
                "Asset boolean and string parameter text");
        require(numericAsset.parameters.at("large").defaultValue == "18446744073709551615",
                "Asset parameter integer precision");
        require(GtsJsonParser::parse(numericAsset.parameters.at("precise").defaultValue, preciseNumber)
                && preciseNumber.tryNumber() == std::optional<double>{1.2345678901234567},
                "Asset parameter floating precision");
        UiSerializedWidget widget;
        widget.id = "checked";
        widget.type = "Label";
        widget.dragSource.emplace();
        widget.dragSource->payloadId = std::numeric_limits<uint64_t>::max();
        UiSerializedWidget restoredWidget;
        require(parseUiSerializedWidget(serializeUiSerializedWidget(widget), restoredWidget)
                && restoredWidget.dragSource->payloadId == widget.dragSource->payloadId,
                "UI payload lost integer precision");
        GtsJsonValue widgetJson;
        require(GtsJsonParser::parse(R"({"id":"checked","type":"Label","visible":false,"maxLines":1.5,
            "layout":{"gap":1e100,"gridRows":2147483648}})", widgetJson), "UI numeric fixture");
        require(parseUiSerializedWidget(widgetJson, restoredWidget), "UI optional numeric fields");
        require(!restoredWidget.visible && restoredWidget.maxLines == UiSerializedWidget{}.maxLines
                && restoredWidget.layout.gap == UiSerializedWidget{}.layout.gap
                && restoredWidget.layout.gridRows == UiSerializedWidget{}.layout.gridRows,
                "UI invalid numeric fields did not retain defaults");
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
