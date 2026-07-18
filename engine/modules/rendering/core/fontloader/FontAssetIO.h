#pragma once

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

#include "FontAsset.h"

namespace gts::fonts
{
    namespace detail
    {
        inline void skipWhitespace(const std::string& source, size_t& pos)
        {
            while (pos < source.size() && std::isspace(static_cast<unsigned char>(source[pos])))
                ++pos;
        }

        inline bool consume(const std::string& source, size_t& pos, char expected)
        {
            skipWhitespace(source, pos);
            if (pos >= source.size() || source[pos] != expected)
                return false;
            ++pos;
            return true;
        }

        inline bool parseStringAt(const std::string& source, size_t& pos, std::string& value)
        {
            skipWhitespace(source, pos);
            if (pos >= source.size() || source[pos] != '"')
                return false;
            ++pos;

            std::string result;
            while (pos < source.size())
            {
                const char ch = source[pos++];
                if (ch == '"')
                {
                    value = result;
                    return true;
                }
                if (ch == '\\' && pos < source.size())
                {
                    result.push_back(source[pos++]);
                    continue;
                }
                result.push_back(ch);
            }

            return false;
        }

        inline bool parseFloatAt(const std::string& source, size_t& pos, float& value)
        {
            skipWhitespace(source, pos);
            size_t end = pos;
            while (end < source.size())
            {
                const char ch = source[end];
                if (!std::isdigit(static_cast<unsigned char>(ch))
                    && ch != '-'
                    && ch != '+'
                    && ch != '.'
                    && ch != 'e'
                    && ch != 'E')
                {
                    break;
                }
                ++end;
            }

            if (end == pos)
                return false;

            value = std::stof(source.substr(pos, end - pos));
            pos = end;
            return true;
        }

        inline bool parseBoolAt(const std::string& source, size_t& pos, bool& value)
        {
            skipWhitespace(source, pos);
            if (source.compare(pos, 4, "true") == 0)
            {
                value = true;
                pos += 4;
                return true;
            }
            if (source.compare(pos, 5, "false") == 0)
            {
                value = false;
                pos += 5;
                return true;
            }
            return false;
        }

        inline size_t findValue(const std::string& source, const std::string& key)
        {
            const std::string quotedKey = "\"" + key + "\"";
            const size_t keyPos = source.find(quotedKey);
            if (keyPos == std::string::npos)
                return std::string::npos;

            const size_t colon = source.find(':', keyPos + quotedKey.size());
            if (colon == std::string::npos)
                return std::string::npos;

            return colon + 1;
        }

        inline bool readString(const std::string& source, const std::string& key, std::string& value)
        {
            size_t pos = findValue(source, key);
            return pos != std::string::npos && parseStringAt(source, pos, value);
        }

        inline bool readFloat(const std::string& source, const std::string& key, float& value)
        {
            size_t pos = findValue(source, key);
            return pos != std::string::npos && parseFloatAt(source, pos, value);
        }

        inline bool readUint(const std::string& source, const std::string& key, uint32_t& value)
        {
            float parsed = 0.0f;
            if (!readFloat(source, key, parsed))
                return false;
            value = static_cast<uint32_t>(std::max(0.0f, parsed));
            return true;
        }

        inline bool readBool(const std::string& source, const std::string& key, bool& value)
        {
            size_t pos = findValue(source, key);
            return pos != std::string::npos && parseBoolAt(source, pos, value);
        }

        inline bool parseGlyphMetricsAt(const std::string& source, size_t& pos, FontGlyphMetrics& metrics)
        {
            if (!consume(source, pos, '{'))
                return false;

            while (pos < source.size())
            {
                skipWhitespace(source, pos);
                if (pos < source.size() && source[pos] == '}')
                {
                    ++pos;
                    return true;
                }

                std::string key;
                if (!parseStringAt(source, pos, key) || !consume(source, pos, ':'))
                    return false;

                float value = 0.0f;
                if (!parseFloatAt(source, pos, value))
                    return false;

                if (key == "sizeX")
                    metrics.sizeX = value;
                else if (key == "sizeY")
                    metrics.sizeY = value;
                else if (key == "advance")
                    metrics.advance = value;

                skipWhitespace(source, pos);
                if (pos < source.size() && source[pos] == ',')
                {
                    ++pos;
                    continue;
                }
                if (pos < source.size() && source[pos] == '}')
                {
                    ++pos;
                    return true;
                }
            }

            return false;
        }

        inline bool readGlyphDefaults(const std::string& source, FontGlyphMetrics& metrics)
        {
            size_t pos = findValue(source, "glyphDefaults");
            return pos != std::string::npos && parseGlyphMetricsAt(source, pos, metrics);
        }

        inline bool readGlyphOverrides(const std::string& source,
                                       std::unordered_map<char, FontGlyphMetrics>& overrides)
        {
            size_t pos = findValue(source, "glyphOverrides");
            if (pos == std::string::npos)
                return false;

            if (!consume(source, pos, '{'))
                return false;

            while (pos < source.size())
            {
                skipWhitespace(source, pos);
                if (pos < source.size() && source[pos] == '}')
                {
                    ++pos;
                    return true;
                }

                std::string chars;
                if (!parseStringAt(source, pos, chars) || !consume(source, pos, ':'))
                    return false;

                FontGlyphMetrics metrics;
                if (!parseGlyphMetricsAt(source, pos, metrics))
                    return false;

                for (char ch : chars)
                    overrides[ch] = metrics;

                skipWhitespace(source, pos);
                if (pos < source.size() && source[pos] == ',')
                {
                    ++pos;
                    continue;
                }
                if (pos < source.size() && source[pos] == '}')
                {
                    ++pos;
                    return true;
                }
            }

            return false;
        }

        inline bool hasGlyphMetrics(const FontGlyphMetrics& metrics)
        {
            return metrics.sizeX > 0.0f || metrics.sizeY > 0.0f || metrics.advance > 0.0f;
        }

        inline void writeGlyphMetrics(std::ostream& out, const FontGlyphMetrics& metrics)
        {
            out << "{\"sizeX\": " << std::fixed << std::setprecision(3) << metrics.sizeX
                << ", \"sizeY\": " << metrics.sizeY
                << ", \"advance\": " << metrics.advance << "}";
        }

        inline std::string escapeJsonString(const std::string& value)
        {
            std::string escaped;
            escaped.reserve(value.size());
            for (char ch : value)
            {
                if (ch == '"' || ch == '\\')
                    escaped.push_back('\\');
                escaped.push_back(ch);
            }
            return escaped;
        }
    }

    inline bool loadFontAsset(const std::string& path, FontAsset& asset)
    {
        std::ifstream in(path);
        if (!in.is_open())
            return false;

        std::stringstream buffer;
        buffer << in.rdbuf();
        const std::string source = buffer.str();

        FontAsset parsed;
        if (!detail::readString(source, "atlas", parsed.atlasPath)) return false;
        if (!detail::readUint(source, "atlasWidth", parsed.atlasWidth)) return false;
        if (!detail::readUint(source, "atlasHeight", parsed.atlasHeight)) return false;
        if (!detail::readUint(source, "cellWidth", parsed.cellWidth)) return false;
        if (!detail::readUint(source, "cellHeight", parsed.cellHeight)) return false;
        if (!detail::readUint(source, "columns", parsed.columns)) return false;
        if (!detail::readString(source, "charOrder", parsed.charOrder)) return false;
        if (!detail::readFloat(source, "lineHeight", parsed.lineHeight)) return false;
        detail::readBool(source, "pixelSampling", parsed.pixelSampling);
        detail::readGlyphDefaults(source, parsed.glyphDefaults);
        detail::readGlyphOverrides(source, parsed.glyphOverrides);

        if (parsed.atlasWidth == 0
            || parsed.atlasHeight == 0
            || parsed.cellWidth == 0
            || parsed.cellHeight == 0
            || parsed.columns == 0
            || parsed.charOrder.empty())
        {
            return false;
        }

        asset = std::move(parsed);
        return true;
    }

    inline bool saveFontAsset(const std::string& path, const FontAsset& asset)
    {
        std::ofstream out(path);
        if (!out.is_open())
            return false;

        out << "{\n";
        out << "  \"atlas\": \"" << detail::escapeJsonString(asset.atlasPath) << "\",\n";
        out << "  \"atlasWidth\": " << asset.atlasWidth << ",\n";
        out << "  \"atlasHeight\": " << asset.atlasHeight << ",\n";
        out << "  \"cellWidth\": " << asset.cellWidth << ",\n";
        out << "  \"cellHeight\": " << asset.cellHeight << ",\n";
        out << "  \"columns\": " << asset.columns << ",\n";
        out << "  \"charOrder\": \"" << detail::escapeJsonString(asset.charOrder) << "\",\n";
        out << "  \"lineHeight\": " << std::fixed << std::setprecision(3) << asset.lineHeight << ",\n";
        if (detail::hasGlyphMetrics(asset.glyphDefaults))
        {
            out << "  \"glyphDefaults\": ";
            detail::writeGlyphMetrics(out, asset.glyphDefaults);
            out << ",\n";
        }
        if (!asset.glyphOverrides.empty())
        {
            out << "  \"glyphOverrides\": {\n";
            size_t written = 0;
            for (const auto& [ch, metrics] : asset.glyphOverrides)
            {
                const std::string key(1, ch);
                out << "    \"" << detail::escapeJsonString(key) << "\": ";
                detail::writeGlyphMetrics(out, metrics);
                if (++written < asset.glyphOverrides.size())
                    out << ",";
                out << "\n";
            }
            out << "  },\n";
        }
        out << "  \"pixelSampling\": " << (asset.pixelSampling ? "true" : "false") << "\n";
        out << "}\n";
        return true;
    }
}
