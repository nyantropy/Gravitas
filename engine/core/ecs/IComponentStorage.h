#pragma once

// needed as a base class for all component storage
struct IComponentStorage 
{
    virtual ~IComponentStorage() = default;
    virtual void remove(Entity entity) = 0;
};