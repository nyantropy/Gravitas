#include "FontAssetIO.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

#include "GtsJsonParser.h"

namespace gts::fonts
{
    namespace
    {

        bool readMetrics(const GtsJsonValue& value, FontGlyphMetrics& output)
        {
            if (!value.isObject())
                return false;
            for (const auto& [key, member] : value.asObject())
                if (!member.isNumber())
                    return false;
            output.sizeX = value.findFloat("sizeX").value_or(output.sizeX);
            output.sizeY = value.findFloat("sizeY").value_or(output.sizeY);
            output.advance = value.findFloat("advance").value_or(output.advance);
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
        const auto atlas = root.findString("atlas");
        const auto order = root.findString("charOrder");
        const auto atlasWidth = root.findUInt32("atlasWidth");
        const auto atlasHeight = root.findUInt32("atlasHeight");
        const auto cellWidth = root.findUInt32("cellWidth");
        const auto cellHeight = root.findUInt32("cellHeight");
        const auto columns = root.findUInt32("columns");
        const auto lineHeight = root.findFloat("lineHeight");
        if (!atlas || !order || !atlasWidth || !atlasHeight || !cellWidth || !cellHeight || !columns || !lineHeight)
            return false;
        parsed.atlasPath = *atlas;
        parsed.charOrder = *order;
        parsed.atlasWidth = *atlasWidth;
        parsed.atlasHeight = *atlasHeight;
        parsed.cellWidth = *cellWidth;
        parsed.cellHeight = *cellHeight;
        parsed.columns = *columns;
        parsed.lineHeight = *lineHeight;
        parsed.pixelSampling = root.findBool("pixelSampling").value_or(parsed.pixelSampling);
        if (const auto* defaults = root.find("glyphDefaults"))
            readMetrics(*defaults, parsed.glyphDefaults);
        if (const auto* overrides = root.findObject("glyphOverrides"))
        {
            for (const auto& [characters, value] : *overrides)
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
