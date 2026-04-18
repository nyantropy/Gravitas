#pragma once

#include <unordered_map>
#include <memory>
#include <typeindex>
#include <typeinfo>
#include <array>
#include <vector>
#include <algorithm>
#include <tuple>
#include <type_traits>
#include <cassert>
#include <functional>
#include <unordered_set>
#include <cstring>
#include <cstdint>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string_view>

#include "Archetype.h"
#include "Entity.h"
#include "ComponentStorage.hpp"
#include "ECSSimulationSystem.hpp"
#include "ECSControllerSystem.hpp"
#include "SubscriptionToken.hpp"

// Integrated ECS world with component storage, systems, and an event bus.
// Simulation systems run at a fixed tick rate; controller systems run every frame.
// The event bus uses immediate dispatch: publish<E>() calls handlers synchronously.
// SubscriptionTokens RAII-unsubscribe on destruction — store them as system members.
template<typename... Components>
class ECSQuery;

class ECSWorld
{
    private:
        template<typename... Components>
        friend class ECSQuery;

        static constexpr uint32_t MAX_SIGNATURE_BITS = ComponentSignature::BitCapacity;

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
        std::vector<EntityLocation> entityLocations;
        std::vector<Archetype> archetypes;
        std::unordered_map<ComponentSignature, uint32_t, ComponentSignatureHash> archetypeIndexBySignature;

        std::vector<std::unique_ptr<ECSSimulationSystem>> simulationSystems;
        std::vector<std::unique_ptr<ECSControllerSystem>> controllerSystems;
        struct SystemProfile
        {
            float totalMs = 0.0f;
            float maxMs   = 0.0f;
            int   calls   = 0;
        };
        std::unordered_map<std::string_view, SystemProfile> controllerProfiles;
        std::vector<std::pair<std::string_view, SystemProfile>> controllerProfilePrintScratch;

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
        std::unordered_map<std::type_index, std::function<void(ECSWorld&, Entity, void*)>> addCallbacks;
        std::unordered_map<std::type_index, std::function<void(ECSWorld&, Entity, void*)>> removeCallbacks;
        std::vector<uint32_t> forEachScratch;

        template<typename Component>
        uint32_t storageSize() const
        {
            return getStorage<Component>().size();
        }

        template<typename Component>
        Component* tryGetComponentPtr(Entity entity)
        {
            if (!hasSignatureBit<Component>(entity))
                return nullptr;

            return &getStorage<Component>().getAssumePresent(entity);
        }

        template<typename Component>
        const Component* tryGetComponentPtr(Entity entity) const
        {
            if (!hasSignatureBit<Component>(entity))
                return nullptr;

            return &getStorage<Component>().getAssumePresent(entity);
        }

        template<typename Driver>
        const uint32_t* copyDenseIdsForQuery(uint32_t& count)
        {
            auto& storage = getStorage<Driver>();
            count = storage.size();

            forEachScratch.resize(count);
            if (count > 0)
                std::memcpy(forEachScratch.data(), storage.denseIds(), count * sizeof(uint32_t));

            return forEachScratch.data();
        }

        void invokeAddCallback(const std::type_index& type, Entity entity, void* componentData)
        {
            auto it = addCallbacks.find(type);
            if (it != addCallbacks.end() && componentData != nullptr)
                it->second(*this, entity, componentData);
        }

        void ensureEntityLocationCapacity(entity_id_type id)
        {
            if (entityLocations.size() <= id)
                entityLocations.resize(id + 1);
        }

        template<typename Component>
        static uint32_t componentBitIndex()
        {
            static const uint32_t bitIndex = allocateComponentBit();
            return bitIndex;
        }

        static uint32_t allocateComponentBit()
        {
            static uint32_t nextBit = 0;
            assert(nextBit < MAX_SIGNATURE_BITS && "Component signature bit budget exceeded");
            return nextBit++;
        }

        template<typename Component>
        static ComponentSignature componentBitMask()
        {
            ComponentSignature signature;
            signature.set(componentBitIndex<Component>());
            return signature;
        }

        ComponentSignature getEntitySignature(Entity entity) const
        {
            if (entity.id >= entityLocations.size())
                return {};

            return entityLocations[entity.id].signature;
        }

        template<typename Component>
        bool hasSignatureBit(Entity entity) const
        {
            return getEntitySignature(entity).test(componentBitIndex<Component>());
        }

        template<typename Component>
        void setSignatureBit(Entity entity)
        {
            ensureEntityLocationCapacity(entity.id);
            entityLocations[entity.id].signature.set(componentBitIndex<Component>());
        }

        template<typename Component>
        void clearSignatureBit(Entity entity)
        {
            if (entity.id >= entityLocations.size())
                return;

            entityLocations[entity.id].signature.clear(componentBitIndex<Component>());
        }

        template<typename... Components>
        static ComponentSignature requiredSignatureMask()
        {
            ComponentSignature mask{};
            ((mask.set(componentBitIndex<Components>())), ...);
            return mask;
        }

        bool matchesSignature(Entity entity, ComponentSignature requiredMask) const
        {
            const ComponentSignature signature = getEntitySignature(entity);
            return signature.containsAll(requiredMask);
        }

        uint32_t getOrCreateArchetype(ComponentSignature signature)
        {
            auto it = archetypeIndexBySignature.find(signature);
            if (it != archetypeIndexBySignature.end())
                return it->second;

            const uint32_t archetypeIndex = static_cast<uint32_t>(archetypes.size());
            Archetype archetype;
            archetype.signature = signature;
            archetypes.push_back(std::move(archetype));
            archetypeIndexBySignature.emplace(signature, archetypeIndex);
            return archetypeIndex;
        }

        void detachEntityFromArchetype(Entity entity)
        {
            if (entity.id >= entityLocations.size())
                return;

            auto& location = entityLocations[entity.id];
            if (!location.isAssigned())
                return;

            Archetype& archetype = archetypes[location.archetypeIndex];
            const uint32_t removedRow = location.rowIndex;
            const uint32_t lastRow = static_cast<uint32_t>(archetype.entities.size() - 1);

            if (removedRow != lastRow)
            {
                const Entity movedEntity = archetype.entities[lastRow];
                archetype.entities[removedRow] = movedEntity;
                entityLocations[movedEntity.id].rowIndex = removedRow;
            }

            archetype.entities.pop_back();
            location.archetypeIndex = EntityLocation::InvalidIndex;
            location.rowIndex = EntityLocation::InvalidIndex;
        }

        void moveEntityToArchetype(Entity entity, ComponentSignature signature)
        {
            ensureEntityLocationCapacity(entity.id);
            detachEntityFromArchetype(entity);

            const uint32_t archetypeIndex = getOrCreateArchetype(signature);
            Archetype& archetype = archetypes[archetypeIndex];
            const uint32_t rowIndex = static_cast<uint32_t>(archetype.entities.size());
            archetype.entities.push_back(entity);

            auto& location = entityLocations[entity.id];
            location.signature = signature;
            location.archetypeIndex = archetypeIndex;
            location.rowIndex = rowIndex;
        }

        void refreshEntityArchetype(Entity entity)
        {
            moveEntityToArchetype(entity, getEntitySignature(entity));
        }

    public:
        Entity createEntity()
        {
            Entity entity{ nextEntityId++ };
            ensureEntityLocationCapacity(entity.id);
            entityLocations[entity.id] = EntityLocation{};
            moveEntityToArchetype(entity, {});
            return entity;
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

            detachEntityFromArchetype(e);
            if (e.id < entityLocations.size())
                entityLocations[e.id] = EntityLocation{};
        }

        template<typename Component>
        void addComponent(Entity entity, const Component& component)
        {
            ensureEntityLocationCapacity(entity.id);
            auto& storage = getStorage<Component>();
            storage.add(entity, component);
            setSignatureBit<Component>(entity);
            refreshEntityArchetype(entity);
            invokeAddCallback(std::type_index(typeid(Component)), entity, storage.getRawPtr(entity));
        }

        template<typename Component>
        bool hasComponent(Entity entity) const
        {
            if (!hasSignatureBit<Component>(entity))
                return false;

#ifndef NDEBUG
            return getStorage<Component>().has(entity);
#else
            return true;
#endif
        }

        template<typename Component>
        Component& getComponent(Entity entity)
        {
            assert(hasSignatureBit<Component>(entity) && "Component signature missing for entity");
            return getStorage<Component>().get(entity);
        }

        template<typename Component>
        const Component& getComponent(Entity entity) const
        {
            assert(hasSignatureBit<Component>(entity) && "Component signature missing for entity");
            return getStorage<Component>().get(entity);
        }

        template<typename Component>
        void removeComponent(Entity entity)
        {
            if (!hasSignatureBit<Component>(entity))
                return;

            auto& storage = getStorage<Component>();
            invokeRemoveCallback(std::type_index(typeid(Component)), entity, storage.getRawPtr(entity));
            storage.remove(entity);
            clearSignatureBit<Component>(entity);
            refreshEntityArchetype(entity);
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
            system->setDebugName(gts::detail::typeName<SystemType>());
            SystemType& ref = *system;
            controllerSystems.push_back(std::move(system));
            return ref;
        }

        template<typename... Components, typename Func>
        void forEach(Func&& fn)
        {
            query<Components...>().each(std::forward<Func>(fn));
        }

        template<typename... Components>
        ECSQuery<Components...> query()
        {
            return ECSQuery<Components...>(*this);
        }

        void updateSimulation(const EcsSimulationContext& ctx)
        {
            for (auto& system : simulationSystems)
                system->update(ctx);
        }

        void updateControllers(const EcsControllerContext& ctx)
        {
            for (auto& system : controllerSystems)
            {
                const auto start = std::chrono::steady_clock::now();
                system->update(ctx);
                const auto end = std::chrono::steady_clock::now();
                const float ms = std::chrono::duration<float, std::milli>(end - start).count();

                auto& profile = controllerProfiles[system->getName()];
                profile.totalMs += ms;
                profile.maxMs = std::max(profile.maxMs, ms);
                ++profile.calls;
            }
        }

        void printControllerProfiles() const
        {
            const_cast<ECSWorld*>(this)->controllerProfilePrintScratch.clear();
            const_cast<ECSWorld*>(this)->controllerProfilePrintScratch.reserve(controllerProfiles.size());

            for (const auto& [name, profile] : controllerProfiles)
            {
                if (profile.calls <= 0)
                    continue;

                const_cast<ECSWorld*>(this)->controllerProfilePrintScratch.emplace_back(name, profile);
            }

            auto& rows = const_cast<ECSWorld*>(this)->controllerProfilePrintScratch;
            std::sort(rows.begin(), rows.end(),
                      [](const auto& a, const auto& b)
                      {
                          const float avgA = a.second.totalMs / static_cast<float>(a.second.calls);
                          const float avgB = b.second.totalMs / static_cast<float>(b.second.calls);
                          if (avgA != avgB)
                              return avgA > avgB;
                          return a.second.maxMs > b.second.maxMs;
                      });

            if (rows.empty())
                return;

            std::cout << "\n=== CONTROLLER PROFILE (5s avg) ===\n\n";
            for (const auto& [name, profile] : rows)
            {
                const float avgMs = profile.totalMs / static_cast<float>(profile.calls);
                std::cout << std::left << std::setw(24) << name
                          << std::right << std::setw(6) << std::fixed << std::setprecision(2) << avgMs
                          << " / "
                          << std::setw(6) << profile.maxMs
                          << '\n';
            }
            std::cout.flush();
        }

        void resetControllerProfiles()
        {
            for (auto& [_, profile] : controllerProfiles)
                profile = {};
        }

        template<typename... Components>
        std::vector<Entity> getAllEntitiesWith()
        {
            std::vector<Entity> result;
            result.reserve(query<Components...>().estimatedIterationCount());
            query<Components...>().each([&](Entity entity, Components&...)
            {
                result.push_back(entity);
            });
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
            addCallbacks.clear();
            controllerProfiles.clear();
            controllerProfilePrintScratch.clear();
            forEachScratch.clear();
            forEachScratch.shrink_to_fit();
            entityLocations.clear();
            archetypes.clear();
            archetypeIndexBySignature.clear();
            nextEntityId = 0;
            // Clear any remaining subscriptions after systems are gone.
            // Do NOT reset nextSubscriptionId — it must be monotonically increasing
            // across the world's lifetime so that IDs from tokens held before clear()
            // never collide with IDs assigned after clear().
            eventHandlers.clear();
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
            setSignatureBit<Component>(e);
            refreshEntityArchetype(e);
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
        void registerAddCallback(Callback&& callback)
        {
            addCallbacks[std::type_index(typeid(Component))] =
                [cb = std::forward<Callback>(callback)](ECSWorld& world, Entity entity, void* componentData)
                {
                    cb(world, entity, *static_cast<Component*>(componentData));
                };
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

template<typename... Components>
class ECSQuery
{
public:
    explicit ECSQuery(ECSWorld& world)
        : world(world)
    {}

    template<typename Func>
    void each(Func&& fn)
    {
        static_assert(sizeof...(Components) > 0);

        constexpr size_t componentCount = sizeof...(Components);
        const std::array<uint32_t, componentCount> counts{world.template storageSize<Components>()...};
        const auto requiredMask = ECSWorld::template requiredSignatureMask<Components...>();

        size_t driverIndex = 0;
        for (size_t i = 1; i < componentCount; ++i)
        {
            if (counts[i] < counts[driverIndex])
                driverIndex = i;
        }

        dispatchEach(driverIndex, requiredMask, std::forward<Func>(fn));
    }

    uint32_t estimatedIterationCount() const
    {
        static_assert(sizeof...(Components) > 0);

        const std::array<uint32_t, sizeof...(Components)> counts{
            world.template storageSize<Components>()...
        };

        uint32_t best = counts[0];
        for (size_t i = 1; i < counts.size(); ++i)
            best = std::min(best, counts[i]);
        return best;
    }

private:
    using ComponentTuple = std::tuple<Components...>;

    ECSWorld& world;

    template<size_t I = 0, typename Func>
    void dispatchEach(size_t driverIndex, ComponentSignature requiredMask, Func&& fn)
    {
        if constexpr (I < sizeof...(Components))
        {
            if (driverIndex == I)
            {
                using Driver = typename std::tuple_element<I, ComponentTuple>::type;
                iterateWithDriver<Driver>(requiredMask, std::forward<Func>(fn));
                return;
            }

            dispatchEach<I + 1>(driverIndex, requiredMask, std::forward<Func>(fn));
        }
    }

    template<typename Driver, typename Func>
    void iterateWithDriver(ComponentSignature requiredMask, Func&& fn)
    {
        uint32_t count = 0;
        const uint32_t* ids = world.template copyDenseIdsForQuery<Driver>(count);

        for (uint32_t i = 0; i < count; ++i)
        {
            const Entity entity{ids[i]};
            if (!world.matchesSignature(entity, requiredMask))
                continue;

            invoke(std::forward<Func>(fn), entity, std::index_sequence_for<Components...>{});
        }
    }

    template<typename Func, size_t... I>
    void invoke(Func&& fn, Entity entity, std::index_sequence<I...>)
    {
        fn(entity, world.template tryGetComponentPtr<typename std::tuple_element<I, ComponentTuple>::type>(entity)[0]...);
    }
};
