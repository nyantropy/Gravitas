#pragma once

#include <any>
#include <deque>

// Value-owned call data. Copies of a controller context keep independent snapshots;
// pointers inside a module's payload remain borrowed for the current call only.
class ControllerFrameData
{
    public:
    template <typename Data> const Data& get() const
    {
        for (const auto& value : values)
            if (const auto* data = std::any_cast<Data>(&value))
                return *data;
        static const Data empty{};
        return empty;
    }

    template <typename Data> Data& edit()
    {
        for (auto& value : values)
            if (auto* data = std::any_cast<Data>(&value))
                return *data;
        values.emplace_back(Data{});
        return *std::any_cast<Data>(&values.back());
    }

    private:
    std::deque<std::any> values;
};
