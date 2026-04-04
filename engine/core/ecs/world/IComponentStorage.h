#pragma once

#include <vector>

#include "Entity.h"

// needed as a base class for all component storage
struct IComponentStorage 
{
    virtual ~IComponentStorage() = default;
    virtual void remove(Entity entity) = 0;
    virtual std::vector<Entity> getAllEntities() const = 0;
};
