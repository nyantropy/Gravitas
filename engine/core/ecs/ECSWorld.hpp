#pragma once
#include <unordered_map>
#include <memory>
#include <typeindex>
#include <typeinfo>
#include <vector>
#include <algorithm>

#include "Entity.h"
#include "ComponentStorage.hpp"
#include "ECSSystem.hpp"

class ECSWorld 
{
    private:
        uint32_t nextEntityId = 0;
        std::vector<std::unique_ptr<ECSSystem>> systems;

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

        template<typename Component>
        void filterEntitiesWith(std::vector<Entity>& entities) const
        {
            auto& storage = getStorage<Component>();
            entities.erase(
                std::remove_if(
                    entities.begin(),
                    entities.end(),
                    [&](Entity e) { return !storage.has(e); }
                ),
                entities.end()
            );
        }

        mutable std::unordered_map<std::type_index, std::unique_ptr<IComponentStorage>> storages;
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

        void update(float dt) 
        {
            for (auto& system : systems)
                system->update(*this, dt);
        }

        template<typename... Components>
        std::vector<Entity> getAllEntitiesWith() 
        {
            // get all entities for the first component type
            auto& firstStorage = getStorage<typename std::tuple_element<0, std::tuple<Components...>>::type>();
            std::vector<Entity> result = firstStorage.getAllEntities();

            // for each remaining component type, filter entities that also have it
            (filterEntitiesWith<Components>(result), ...);

            return result;
        }
};
