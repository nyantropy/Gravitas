#pragma once
#include <unordered_map>

// each component can have its own storage
template<typename Component>
class ComponentStorage 
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

    private:
        std::unordered_map<uint32_t, Component> components;
};
