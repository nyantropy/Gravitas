#pragma once
#include <unordered_map>
#include <memory>
#include <typeindex>
#include <typeinfo>
#include <vector>

#include "Entity.h"
#include "ComponentStorage.hpp"
#include "System.hpp"

class ECSWorld {
public:
    Entity createEntity() 
    {
        return Entity{ nextEntityId++ };
    }

    template<typename Component>
    void addComponent(Entity entity, const Component& component) 
    {
        getStorage<Component>().add(entity, component);
    }

    template<typename Component>
    bool hasComponent(Entity entity) const 
    {
        return getStorage<Component>().has(entity);
    }

    template<typename Component>
    Component& getComponent(Entity entity) 
    {
        return getStorage<Component>().get(entity);
    }

    template<typename Component>
    void removeComponent(Entity entity) 
    {
        getStorage<Component>().remove(entity);
    }

    template<typename SystemType, typename... Args>
    SystemType& addSystem(Args&&... args) 
    {
        auto system = std::make_unique<SystemType>(std::forward<Args>(args)...);
        SystemType& ref = *system;
        systems.push_back(std::move(system));
        return ref;
    }

    void update(float deltaTime) 
    {
        for (auto& system : systems)
            system->update(deltaTime, *this);
    }

private:
    uint32_t nextEntityId = 0;
    std::vector<std::unique_ptr<System>> systems;

    template<typename Component>
    ComponentStorage<Component>& getStorage() const 
    {
        auto type = std::type_index(typeid(Component));
        if (storages.find(type) == storages.end()) 
        {
            storages[type] = std::make_unique<ComponentStorage<Component>>();
        }
        return *static_cast<ComponentStorage<Component>*>(storages[type].get());
    }

    mutable std::unordered_map<std::type_index, std::unique_ptr<void>> storages;
};
