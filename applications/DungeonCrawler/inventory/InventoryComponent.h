#pragma once

#include <cstddef>
#include <vector>

#include "inventory/Item.h"

struct InventoryComponent
{
    std::vector<Item> items;
    size_t maxSlots = 8;

    bool hasCapacity() const
    {
        return items.size() < maxSlots;
    }

    bool addItem(const Item& item)
    {
        if (!hasCapacity())
            return false;

        items.push_back(item);
        return true;
    }
};
