#pragma once

#include <cassert>
#include <cstdint>
#include <cstring>
#include <deque>
#include <limits>
#include <utility>
#include <vector>

#include "IComponentStorage.h"

template<typename Component>
class ComponentStorage : public IComponentStorage
{
public:
    static constexpr uint32_t INVALID   = std::numeric_limits<uint32_t>::max();
    static constexpr uint32_t PAGE_SIZE = 4096;

    void add(Entity entity, const Component& component)
    {
        const uint32_t id = entity.id;

        if (has(entity))
        {
            data[sparseGet(id)] = component;
            return;
        }

        ensurePage(id);

        const uint32_t index = static_cast<uint32_t>(dense.size());
        sparseSet(id, index);
        dense.push_back(id);
        data.push_back(component);
    }

    bool has(Entity entity) const override
    {
        const uint32_t id     = entity.id;
        const uint32_t page   = id / PAGE_SIZE;
        const uint32_t offset = id % PAGE_SIZE;

        if (page >= sparse.size() || sparse[page] == nullptr)
            return false;

        const uint32_t index = sparse[page][offset];
        return index != INVALID
            && index < static_cast<uint32_t>(dense.size())
            && dense[index] == id;
    }

    Component& get(Entity entity)
    {
        assert(has(entity) && "ComponentStorage::get - entity does not have this component");
        return data[sparseGet(entity.id)];
    }

    const Component& get(Entity entity) const
    {
        assert(has(entity) && "ComponentStorage::get - entity does not have this component");
        return data[sparseGet(entity.id)];
    }

    Component* tryGetPtr(Entity entity)
    {
        if (!has(entity))
            return nullptr;

        return &data[sparseGet(entity.id)];
    }

    const Component* tryGetPtr(Entity entity) const
    {
        if (!has(entity))
            return nullptr;

        return &data[sparseGet(entity.id)];
    }

    void remove(Entity entity) override
    {
        if (!has(entity))
            return;

        const uint32_t id        = entity.id;
        const uint32_t index     = sparseGet(id);
        const uint32_t lastIndex = static_cast<uint32_t>(dense.size()) - 1;

        if (index != lastIndex)
        {
            const uint32_t lastId = dense[lastIndex];

            dense[index] = lastId;
            data[index]  = std::move(data[lastIndex]);
            sparseSet(lastId, index);
        }

        dense.pop_back();
        data.pop_back();
        sparseSet(id, INVALID);
    }

    void* getRawPtr(Entity entity) override
    {
        if (!has(entity))
            return nullptr;
        return static_cast<void*>(&data[sparseGet(entity.id)]);
    }

    std::vector<Entity> getAllEntities() const override
    {
        std::vector<Entity> result;
        result.reserve(dense.size());
        for (uint32_t id : dense)
            result.push_back(Entity{id});
        return result;
    }

    uint32_t size() const
    {
        return static_cast<uint32_t>(dense.size());
    }

    const uint32_t* denseIds() const
    {
        return dense.data();
    }

    // Direct access to the component data array. Reserved for optimized
    // single-component iteration paths that can skip sparse lookups.
    Component* dataPtr()
    {
        return data.data();
    }

    // Direct access to the component data array. Reserved for optimized
    // single-component iteration paths that can skip sparse lookups.
    const Component* dataPtr() const
    {
        return data.data();
    }

private:
    std::vector<uint32_t*>          sparse;
    std::vector<uint32_t>           dense;
    std::vector<Component>          data;
    std::deque<std::vector<uint32_t>> pages;

    void ensurePage(uint32_t id)
    {
        const uint32_t page = id / PAGE_SIZE;
        if (page >= sparse.size())
            sparse.resize(page + 1, nullptr);

        if (sparse[page] == nullptr)
        {
            pages.emplace_back(PAGE_SIZE, INVALID);
            sparse[page] = pages.back().data();
        }
    }

    uint32_t sparseGet(uint32_t id) const
    {
        return sparse[id / PAGE_SIZE][id % PAGE_SIZE];
    }

    void sparseSet(uint32_t id, uint32_t index)
    {
        sparse[id / PAGE_SIZE][id % PAGE_SIZE] = index;
    }
};
