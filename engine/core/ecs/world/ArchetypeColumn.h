#pragma once

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

struct IArchetypeColumn
{
    virtual ~IArchetypeColumn() = default;

    virtual void swapRemove(uint32_t rowIndex) = 0;
    virtual void* rawPtr(uint32_t rowIndex) = 0;
    virtual const void* rawPtr(uint32_t rowIndex) const = 0;
    virtual std::unique_ptr<IArchetypeColumn> cloneEmpty() const = 0;
};

template<typename Component>
class ArchetypeColumn final : public IArchetypeColumn
{
public:
    void append(const Component& value)
    {
        data.push_back(value);
    }

    void append(Component&& value)
    {
        data.push_back(std::move(value));
    }

    Component& get(uint32_t rowIndex)
    {
        return data[rowIndex];
    }

    const Component& get(uint32_t rowIndex) const
    {
        return data[rowIndex];
    }

    void swapRemove(uint32_t rowIndex) override
    {
        const uint32_t lastIndex = static_cast<uint32_t>(data.size() - 1);
        if (rowIndex != lastIndex)
            data[rowIndex] = std::move(data[lastIndex]);

        data.pop_back();
    }

    void* rawPtr(uint32_t rowIndex) override
    {
        return static_cast<void*>(&data[rowIndex]);
    }

    const void* rawPtr(uint32_t rowIndex) const override
    {
        return static_cast<const void*>(&data[rowIndex]);
    }

    std::unique_ptr<IArchetypeColumn> cloneEmpty() const override
    {
        return std::make_unique<ArchetypeColumn<Component>>();
    }

private:
    std::vector<Component> data;
};
