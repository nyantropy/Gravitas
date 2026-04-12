#pragma once

#include <unordered_map>
#include <memory>
#include <typeindex>
#include <typeinfo>
#include <vector>
#include <algorithm>
#include <tuple>
#include <type_traits>
#include <cassert>
#include <functional>
#include <unordered_set>
#include <cstring>
#include <cstdint>

#include "Entity.h"
#include "ComponentStorage.hpp"
#include "ECSSimulationSystem.hpp"
#include "ECSControllerSystem.hpp"
#include "SubscriptionToken.hpp"

// Integrated ECS world with component storage, systems, and an event bus.
// Simulation systems run at a fixed tick rate; controller systems run every frame.
// The event bus uses immediate dispatch: publish<E>() calls handlers synchronously.
// SubscriptionTokens RAII-unsubscribe on destruction — store them as system members.
class ECSWorld
{
    private:
        // ── Event bus ─────────────────────────────────────────────────────────
        // m_alive and eventHandlers are declared first so they are destroyed
        // last (C++ destroys members in reverse declaration order).
        //
        // m_alive is a shared sentinel: the unsubscribe lambda in each
        // SubscriptionToken holds a weak_ptr to it. If the ECSWorld is fully
        // destroyed before a token (e.g., an external holder outlives the
        // world), the weak_ptr expires and the lambda returns without touching
        // the dead eventHandlers map.
        //
        // For tokens held inside the world (system members), the ordering
        // guarantee alone is sufficient: controllerSystems/simulationSystems
        // are destroyed before eventHandlers, so their token destructors always
        // find a live map.

        std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);

        using SubscriptionId = uint64_t;

        struct HandlerEntry
        {
            SubscriptionId id;
            std::function<void(const void*)> fn;
        };

        std::unordered_map<std::type_index, std::vector<HandlerEntry>> eventHandlers;
        SubscriptionId nextSubscriptionId = 1;

        // ── ECS core ──────────────────────────────────────────────────────────

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

        void invokeRemoveCallback(const std::type_index& type, Entity entity, void* componentData)
        {
            auto it = removeCallbacks.find(type);
            if (it != removeCallbacks.end() && componentData != nullptr)
                it->second(*this, entity, componentData);
        }

        mutable std::unordered_map<std::type_index, std::unique_ptr<IComponentStorage>> storages;
        std::unordered_map<std::type_index, std::function<void(ECSWorld&, Entity, void*)>> removeCallbacks;
        std::vector<uint32_t> forEachScratch;

    public:
        Entity createEntity()
        {
            return Entity{ nextEntityId++ };
        }

        void destroyEntity(Entity e)
        {
            for (auto& [type, storage] : storages)
            {
                if (!storage->has(e))
                    continue;

                invokeRemoveCallback(type, e, storage->getRawPtr(e));
                storage->remove(e);
            }
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
            auto& storage = getStorage<Component>();
            if (!storage.has(entity))
                return;

            invokeRemoveCallback(std::type_index(typeid(Component)), entity, storage.getRawPtr(entity));
            storage.remove(entity);
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
            const uint32_t count = firstStorage.size();

            forEachScratch.resize(count);
            if (count > 0)
                std::memcpy(forEachScratch.data(), firstStorage.denseIds(), count * sizeof(uint32_t));

            for (uint32_t i = 0; i < count; ++i)
            {
                Entity e{forEachScratch[i]};

                if (!(hasComponent<Components>(e) && ...))
                    continue;

                fn(e, getComponent<Components>(e)...);
            }
        }

        void updateSimulation(const EcsSimulationContext& ctx)
        {
            for (auto& system : simulationSystems)
                system->update(ctx);
        }

        void updateControllers(const EcsControllerContext& ctx)
        {
            for (auto& system : controllerSystems)
                system->update(ctx);
        }

        template<typename... Components>
        std::vector<Entity> getAllEntitiesWith()
        {
            static_assert(sizeof...(Components) > 0);
            using First = typename std::tuple_element<0, std::tuple<Components...>>::type;

            auto& firstStorage = getStorage<First>();

            std::vector<Entity> result;
            result.reserve(firstStorage.size());

            const uint32_t count = firstStorage.size();
            const uint32_t* ids = firstStorage.denseIds();

            for (uint32_t i = 0; i < count; ++i)
            {
                Entity e{ids[i]};
                if ((hasComponent<Components>(e) && ...))
                    result.push_back(e);
            }

            return result;
        }
        
        // Destroy all entities, components, systems, and event subscriptions.
        // Call at the start of onLoad when a scene may be reloaded.
        void clear()
        {
            for (auto& [type, storage] : storages)
            {
                const std::vector<Entity> entities = storage->getAllEntities();
                for (Entity entity : entities)
                    invokeRemoveCallback(type, entity, storage->getRawPtr(entity));
            }

            storages.clear();
            // Systems are destroyed here; their SubscriptionToken members call
            // unsubscribe. The eventHandlers map is still valid at this point.
            simulationSystems.clear();
            controllerSystems.clear();
            forEachScratch.clear();
            forEachScratch.shrink_to_fit();
            nextEntityId = 0;
            // Clear any remaining subscriptions after systems are gone.
            eventHandlers.clear();
            nextSubscriptionId = 1;
        }

        // ------------------------------------------------------------
        // singleton helpers
        // a singleton is just a component type that is guaranteed
        // to exist on exactly one entity
        // this whole feature is a bit experimental, but the bottom line is that not all components
        // need to exist on multiple entities - singletons should be used to share data between different systems
        // as an ecs friendly approach

        template<typename Component>
        bool hasAny() const
        {
            auto& storage = getStorage<Component>();
            return storage.size() > 0;
        }

        template<typename Component>
        Entity getSingletonEntity() const
        {
            auto& storage = getStorage<Component>();
            const uint32_t count = storage.size();

            // must exist
            assert(count > 0 && "Singleton component does not exist");

            // must be unique
            assert(count == 1 && "More than one singleton component exists");

            return Entity{storage.denseIds()[0]};
        }

        template<typename Component>
        Component& getSingleton()
        {
            Entity e = getSingletonEntity<Component>();
            return getComponent<Component>(e);
        }

        template<typename Component>
        const Component& getSingleton() const
        {
            Entity e = getSingletonEntity<Component>();
            return getStorage<Component>().get(e);
        }

        // convenience helper for scene/world setup
        // creates the singleton component on a new entity
        // and enforces that only one exists
        template<typename Component, typename... Args>
        Component& createSingleton(Args&&... args)
        {
            // make sure we do not accidentally create more than one
            assert(!hasAny<Component>() && "Singleton component already exists");

            Entity e = createEntity();
            getStorage<Component>().add(e, Component{ std::forward<Args>(args)... });
            return getComponent<Component>(e);
        }

        // ------------------------------------------------------------

        size_t getEntityCount() const
        {
            std::unordered_set<entity_id_type> uniqueEntities;
            for (const auto& [type, storage] : storages)
            {
                for (Entity e : storage->getAllEntities())
                    uniqueEntities.insert(e.id);
            }
            return uniqueEntities.size();
        }

        size_t getSimulationSystemCount() const
        {
            return simulationSystems.size();
        }

        size_t getControllerSystemCount() const
        {
            return controllerSystems.size();
        }

        template<typename Component, typename Callback>
        void registerRemoveCallback(Callback&& callback)
        {
            removeCallbacks[std::type_index(typeid(Component))] =
                [cb = std::forward<Callback>(callback)](ECSWorld& world, Entity entity, void* componentData)
                {
                    cb(world, entity, *static_cast<Component*>(componentData));
                };
        }

        // ── Event bus ─────────────────────────────────────────────────────────

        // Publish an event immediately. All subscribed handlers are called
        // synchronously before publish() returns.
        //
        // Snapshot semantics: the handler list is copied before iteration.
        //   - Handlers added during dispatch are NOT called in the current pass.
        //   - Handlers removed during dispatch (token destroyed inside a handler)
        //     still run for the current event; the live list is updated after the
        //     snapshot was taken so iteration is unaffected.
        //   - Recursive publish() calls (a handler calling publish()) work
        //     correctly: each nested call takes its own independent snapshot.
        //   - Publishing to a type with no subscribers is a no-op (no allocation).
        template<typename E>
        void publish(const E& event)
        {
            const std::type_index key = std::type_index(typeid(E));
            auto it = eventHandlers.find(key);
            if (it == eventHandlers.end())
                return;

            // Snapshot the handler list so subscribe/unsubscribe during dispatch
            // does not invalidate the iteration.
            const auto snapshot = it->second;
            const void* payload = &event;
            for (const HandlerEntry& entry : snapshot)
                entry.fn(payload);
        }

        // Subscribe to event type E. Returns an RAII token that unsubscribes
        // on destruction. Store the token as a member of the subscribing system.
        template<typename E>
        SubscriptionToken subscribe(std::function<void(const E&)> handler)
        {
            const std::type_index key = std::type_index(typeid(E));
            const SubscriptionId id = nextSubscriptionId++;

            eventHandlers[key].push_back(HandlerEntry{
                id,
                [h = std::move(handler)](const void* payload)
                {
                    h(*static_cast<const E*>(payload));
                }
            });

            std::weak_ptr<bool> weakAlive = m_alive;
            return SubscriptionToken([this, key, id, weakAlive = std::move(weakAlive)]()
            {
                // If the ECSWorld has been destroyed, m_alive's strong count
                // has dropped to zero and expired() returns true. Skip the
                // dereference to avoid a use-after-free.
                if (weakAlive.expired())
                    return;

                auto it = eventHandlers.find(key);
                if (it == eventHandlers.end())
                    return;

                auto& vec = it->second;
                vec.erase(
                    std::remove_if(vec.begin(), vec.end(),
                        [id](const HandlerEntry& e) { return e.id == id; }),
                    vec.end());
            });
        }

        // No-op with immediate dispatch — kept for game-loop symmetry.
        void flushEvents() {}

};
