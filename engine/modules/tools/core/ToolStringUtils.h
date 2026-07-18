#pragma once

#include <cstddef>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>

namespace gts::tools::toolui
{
    inline std::string fileName(const std::string& path)
    {
        if (path.empty())
            return {};

        std::filesystem::path fsPath(path);
        const std::string name = fsPath.filename().string();
        return name.empty() ? path : name;
    }

    inline std::string stemName(const std::string& path)
    {
        if (path.empty())
            return {};

        std::filesystem::path fsPath(path);
        const std::string name = fsPath.stem().string();
        return name.empty() ? fileName(path) : name;
    }

    inline std::string compact(const std::string& text, size_t maxChars)
    {
        if (text.size() <= maxChars || maxChars < 8)
            return text;

        const size_t left = (maxChars - 3) / 2;
        const size_t right = maxChars - 3 - left;
        return text.substr(0, left) + "..." + text.substr(text.size() - right);
    }

    inline std::string fixed(float value, int precision = 2)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(precision) << value;
        return out.str();
    }
}
