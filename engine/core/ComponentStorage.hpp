#pragma once
#include <unordered_map>
#include <vector>

#include "IComponentStorage.h"

// each component can have its own storage
template<typename Component>
class ComponentStorage : public IComponentStorage
{
    public:
        void add(Entity entity, const Component& component) 
        {
            components[entity.id] = component;
        }

        bool has(Entity entity) const 
        {
            return components.find(entity.id) != components.end();
        }

        Component& get(Entity entity) 
        {
            return components.at(entity.id);
        }

        void remove(Entity entity) 
        {
            components.erase(entity.id);
        }

        std::vector<Entity> getAllEntities() const 
        {
            std::vector<Entity> result;
            for (auto& [id, comp] : components)
                result.push_back(Entity{id});
            return result;
        }

    private:
        std::unordered_map<uint32_t, Component> components;
};
