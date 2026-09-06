#include "FontAssetIO.h"

#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

#include "GtsJsonParser.h"

namespace gts::fonts
{
    namespace
    {
        bool readUint(const GtsJsonValue& root, const char* key, uint32_t& output)
        {
            const auto* value = root.find(key);
            if (!value || !value->isNumber())
                return false;
            const double number = value->asNumber();
            if (number < 0 || number > std::numeric_limits<uint32_t>::max() || std::trunc(number) != number)
                return false;
            output = static_cast<uint32_t>(number);
            return true;
        }

        bool readFloat(const GtsJsonValue& root, const char* key, float& output)
        {
            const auto* value = root.find(key);
            if (!value || !value->isNumber() || std::abs(value->asNumber()) > std::numeric_limits<float>::max())
                return false;
            output = static_cast<float>(value->asNumber());
            return true;
        }

        bool readMetrics(const GtsJsonValue& value, FontGlyphMetrics& output)
        {
            if (!value.isObject())
                return false;
            for (const auto& [key, member] : value.asObject())
                if (!member.isNumber())
                    return false;
            readFloat(value, "sizeX", output.sizeX);
            readFloat(value, "sizeY", output.sizeY);
            readFloat(value, "advance", output.advance);
            return true;
        }

        GtsJsonValue metricsJson(const FontGlyphMetrics& metrics)
        {
            return GtsJsonValue::Object{
                {"sizeX", metrics.sizeX}, {"sizeY", metrics.sizeY}, {"advance", metrics.advance}};
        }
    }

    bool loadFontAsset(const std::string& path, FontAsset& asset)
    {
        std::ifstream file(path);
        if (!file)
            return false;
        std::ostringstream buffer;
        buffer << file.rdbuf();
        GtsJsonValue root;
        if (!GtsJsonParser::parse(buffer.str(), root) || !root.isObject())
            return false;
        FontAsset   parsed;
        const auto* atlas = root.find("atlas");
        const auto* order = root.find("charOrder");
        if (!atlas || !atlas->isString() || !order || !order->isString())
            return false;
        parsed.atlasPath = atlas->asString();
        parsed.charOrder = order->asString();
        if (!readUint(root, "atlasWidth", parsed.atlasWidth) || !readUint(root, "atlasHeight", parsed.atlasHeight) ||
            !readUint(root, "cellWidth", parsed.cellWidth) || !readUint(root, "cellHeight", parsed.cellHeight) ||
            !readUint(root, "columns", parsed.columns) || !readFloat(root, "lineHeight", parsed.lineHeight))
            return false;
        if (const auto* sampling = root.find("pixelSampling"); sampling && sampling->isBool())
            parsed.pixelSampling = sampling->asBool();
        if (const auto* defaults = root.find("glyphDefaults"))
            readMetrics(*defaults, parsed.glyphDefaults);
        if (const auto* overrides = root.find("glyphOverrides"); overrides && overrides->isObject())
        {
            for (const auto& [characters, value] : overrides->asObject())
            {
                FontGlyphMetrics metrics;
                if (!readMetrics(value, metrics))
                    continue;
                for (char ch : characters)
                    parsed.glyphOverrides[ch] = metrics;
            }
        }
        if (parsed.atlasWidth == 0 || parsed.atlasHeight == 0 || parsed.cellWidth == 0 || parsed.cellHeight == 0 ||
            parsed.columns == 0 || parsed.charOrder.empty())
            return false;
        asset = std::move(parsed);
        return true;
    }

    bool saveFontAsset(const std::string& path, const FontAsset& asset)
    {
        GtsJsonValue::Object object{{"atlas", asset.atlasPath},
                                    {"atlasWidth", asset.atlasWidth},
                                    {"atlasHeight", asset.atlasHeight},
                                    {"cellWidth", asset.cellWidth},
                                    {"cellHeight", asset.cellHeight},
                                    {"columns", asset.columns},
                                    {"charOrder", asset.charOrder},
                                    {"lineHeight", asset.lineHeight},
                                    {"pixelSampling", asset.pixelSampling}};
        const auto&          defaults = asset.glyphDefaults;
        if (defaults.sizeX > 0 || defaults.sizeY > 0 || defaults.advance > 0)
            object.emplace_back("glyphDefaults", metricsJson(defaults));
        if (!asset.glyphOverrides.empty())
        {
            GtsJsonValue::Object overrides;
            for (const auto& [ch, metrics] : asset.glyphOverrides)
                overrides.emplace_back(std::string(1, ch), metricsJson(metrics));
            object.emplace_back("glyphOverrides", std::move(overrides));
        }
        std::string json;
        try
        {
            json = GtsJsonParser::serialize(std::move(object));
        }
        catch (const std::invalid_argument&)
        {
            return false;
        }
        std::ofstream file(path);
        if (!file)
            return false;
        file << json << '\n';
        return file.good();
    }
}
