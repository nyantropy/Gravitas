#pragma once

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "GlmConfig.h"

namespace gts::debugdraw
{
    enum class DebugDrawColor
    {
        White,
        Red,
        Green,
        Blue,
        Cyan,
        Yellow,
        Orange,
        Purple,
        Grey
    };

    inline constexpr size_t DebugDrawColorCount = 9;

    struct DebugDrawLine
    {
        glm::vec3 start{0.0f};
        glm::vec3 end{0.0f};
        DebugDrawColor color = DebugDrawColor::White;
        float thickness = 0.025f;
    };

    inline size_t debugDrawColorIndex(DebugDrawColor color)
    {
        return std::min(static_cast<size_t>(color), DebugDrawColorCount - 1);
    }

    inline void debugDrawHashCombine(size_t& seed, size_t value)
    {
        seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    }

    inline size_t debugDrawFloatBits(float value)
    {
        uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        return static_cast<size_t>(bits);
    }

    inline void debugDrawHashLine(size_t& seed, const DebugDrawLine& line)
    {
        debugDrawHashCombine(seed, debugDrawFloatBits(line.start.x));
        debugDrawHashCombine(seed, debugDrawFloatBits(line.start.y));
        debugDrawHashCombine(seed, debugDrawFloatBits(line.start.z));
        debugDrawHashCombine(seed, debugDrawFloatBits(line.end.x));
        debugDrawHashCombine(seed, debugDrawFloatBits(line.end.y));
        debugDrawHashCombine(seed, debugDrawFloatBits(line.end.z));
        debugDrawHashCombine(seed, debugDrawFloatBits(line.thickness));
    }

    inline size_t debugDrawHashLines(const DebugDrawLine* lines, size_t count)
    {
        size_t seed = 0;
        for (size_t i = 0; i < count; ++i)
            debugDrawHashLine(seed, lines[i]);
        return seed;
    }

    struct DebugDrawQueueComponent
    {
        std::array<std::vector<DebugDrawLine>, DebugDrawColorCount> lineBatches;
        std::array<size_t, DebugDrawColorCount> batchSignatures{};
        std::array<size_t, DebugDrawColorCount> batchLineCounts{};
        uint64_t version = 0;

        bool empty() const
        {
            for (const auto& batch : lineBatches)
            {
                if (!batch.empty())
                    return false;
            }
            return true;
        }

        void clear()
        {
            for (auto& batch : lineBatches)
                batch.clear();
            batchSignatures.fill(0);
            batchLineCounts.fill(0);
            ++version;
        }

        void reserve(DebugDrawColor color, size_t additionalLines)
        {
            std::vector<DebugDrawLine>& batch = lineBatches[debugDrawColorIndex(color)];
            batch.reserve(batch.size() + additionalLines);
        }

        void pushLine(const DebugDrawLine& line)
        {
            const size_t index = debugDrawColorIndex(line.color);
            lineBatches[index].push_back(line);
            debugDrawHashLine(batchSignatures[index], line);
            ++batchLineCounts[index];
            ++version;
        }

        void appendLines(DebugDrawColor color,
                         const DebugDrawLine* lines,
                         size_t count,
                         size_t signature)
        {
            if (lines == nullptr || count == 0)
                return;

            const size_t index = debugDrawColorIndex(color);
            std::vector<DebugDrawLine>& batch = lineBatches[index];
            batch.insert(batch.end(), lines, lines + count);
            debugDrawHashCombine(batchSignatures[index], signature);
            debugDrawHashCombine(batchSignatures[index], count);
            batchLineCounts[index] += count;
            version += count;
        }

        void appendLines(DebugDrawColor color,
                         const DebugDrawLine* lines,
                         size_t count)
        {
            appendLines(color, lines, count, debugDrawHashLines(lines, count));
        }
    };
}
