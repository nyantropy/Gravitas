#pragma once

#include <unordered_map>
#include <memory>
#include <typeindex>
#include <typeinfo>
#include <vector>
#include <algorithm>
#include <tuple>
#include <type_traits>

#include "Entity.h"
#include "ComponentStorage.hpp"
#include "ECSSimulationSystem.hpp"
#include "ECSControllerSystem.hpp"
#include "SceneContext.h"

// now updated to suit both simulation and controller systems
// simulation systems may only depend on dt
// controller systems have more power given through the scene context and a way to send commands back to the engine

// component storages still work the same way, an entity can have multiple different components attached to it, and may be filtered by looking for
// those components
class ECSWorld
{
    private:
        uint32_t nextEntityId = 0;

        std::vector<std::unique_ptr<ECSSimulationSystem>> simulationSystems;
        std::vector<std::unique_ptr<ECSControllerSystem>> controllerSystems;

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

        void destroyEntity(Entity e)
        {
            for (auto& [type, storage] : storages)
                storage->remove(e);
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
        SystemType& addSimulationSystem(Args&&... args)
        {
            static_assert(std::is_base_of_v<ECSSimulationSystem, SystemType>,
                            "SystemType must derive from ECSSimulationSystem");

            auto system = std::make_unique<SystemType>(std::forward<Args>(args)...);
            SystemType& ref = *system;
            simulationSystems.push_back(std::move(system));
            return ref;
        }

        template<typename SystemType, typename... Args>
        SystemType& addControllerSystem(Args&&... args)
        {
            static_assert(std::is_base_of_v<ECSControllerSystem, SystemType>,
                            "SystemType must derive from ECSControllerSystem");

            auto system = std::make_unique<SystemType>(std::forward<Args>(args)...);
            SystemType& ref = *system;
            controllerSystems.push_back(std::move(system));
            return ref;
        }

        template<typename... Components, typename Func>
        void forEach(Func&& fn)
        {
            static_assert(sizeof...(Components) > 0);

            using First = typename std::tuple_element<0, std::tuple<Components...>>::type;

            auto& firstStorage = getStorage<First>();

            for (Entity e : firstStorage.getAllEntities())
            {
                if (!(hasComponent<Components>(e) && ...))
                    continue;

                fn(e, getComponent<Components>(e)...);
            }
        }

        void updateSimulation(float dt)
        {
            for (auto& system : simulationSystems)
                system->update(*this, dt);
        }

        void updateControllers(SceneContext& ctx)
        {
            for (auto& system : controllerSystems)
                system->update(*this, ctx);
        }

        template<typename... Components>
        std::vector<Entity> getAllEntitiesWith()
        {
            auto& firstStorage =
                getStorage<typename std::tuple_element<0, std::tuple<Components...>>::type>();

            std::vector<Entity> result = firstStorage.getAllEntities();

            (filterEntitiesWith<Components>(result), ...);

            return result;
        }     
};
