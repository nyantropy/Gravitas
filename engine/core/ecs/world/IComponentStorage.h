#pragma once

#include <vector>

#include "Entity.h"

// needed as a base class for all component storage
struct IComponentStorage 
{
    virtual ~IComponentStorage() = default;
    virtual bool has(Entity entity) const = 0;
    virtual void* getRawPtr(Entity entity) = 0;
    virtual void remove(Entity entity) = 0;
    virtual std::vector<Entity> getAllEntities() const = 0;
};
