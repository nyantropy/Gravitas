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
#include <cstring>
#include <cstdint>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string_view>

#include "Archetype.h"
#include "Entity.h"
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
    public:
        class EntityCommandBuffer
        {
        public:
            explicit EntityCommandBuffer(ECSWorld& world)
                : world(world)
            {
            }

            Entity createEntity()
            {
                return world.reserveDeferredEntity();
            }

            void destroyEntity(Entity entity)
            {
                world.enqueueStructuralCommand(
                    [entity](ECSWorld& w)
                    {
                        w.destroyEntity(entity);
                    });
            }

            template<typename Component>
            void addComponent(Entity entity, const Component& component)
            {
                world.enqueueStructuralCommand(
                    [entity, component](ECSWorld& w)
                    {
                        w.addComponent<Component>(entity, component);
                    });
            }

            template<typename Component>
            void removeComponent(Entity entity)
            {
                world.enqueueStructuralCommand(
                    [entity](ECSWorld& w)
                    {
                        w.removeComponent<Component>(entity);
                    });
            }

            template<typename Component, typename... Args>
            Entity createSingleton(Args&&... args)
            {
                Entity entity = createEntity();
                addComponent<Component>(entity, Component{std::forward<Args>(args)...});
                return entity;
            }

            bool empty() const
            {
                return world.deferredStructuralCommands.empty();
            }

            void flush()
            {
                world.flushDeferredStructuralCommands();
            }

        private:
            ECSWorld& world;
        };

    private:
        template<typename... Components>
        friend class ECSQuery;

        static constexpr uint32_t MAX_SIGNATURE_BITS = ComponentSignature::BitCapacity;

        struct ComponentTypeOps
        {
            std::type_index type = typeid(void);
            std::function<std::unique_ptr<IArchetypeColumn>()> createColumn;
            std::function<void(Archetype&, const void*)> appendValue;
            std::function<void(Archetype&, uint32_t, Archetype&, uint32_t)> moveValue;
            std::function<void*(Archetype&, uint32_t)> getPtr;
            std::function<const void*(const Archetype&, uint32_t)> getConstPtr;
        };

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
        std::vector<ComponentTypeOps> componentTypeOps;

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

        void invokeRemoveCallback(const std::type_index& type, Entity entity, void* componentData)
        {
            auto it = removeCallbacks.find(type);
            if (it != removeCallbacks.end() && componentData != nullptr)
                it->second(*this, entity, componentData);
        }

        std::unordered_map<std::type_index, std::function<void(ECSWorld&, Entity, void*)>> addCallbacks;
        std::unordered_map<std::type_index, std::function<void(ECSWorld&, Entity, void*)>> removeCallbacks;
        std::vector<uint32_t> forEachScratch;
        std::vector<std::function<void(ECSWorld&)>> deferredStructuralCommands;
        bool flushingDeferredStructuralCommands = false;
        EntityCommandBuffer commandBuffer;

        Entity reserveDeferredEntity()
        {
            Entity entity{ nextEntityId++ };
            ensureEntityLocationCapacity(entity.id);
            entityLocations[entity.id] = EntityLocation{};
            enqueueStructuralCommand(
                [entity](ECSWorld& w)
                {
                    w.moveEntityToArchetype(entity, {});
                });
            return entity;
        }

        void enqueueStructuralCommand(std::function<void(ECSWorld&)> command)
        {
            deferredStructuralCommands.push_back(std::move(command));
        }

        void flushDeferredStructuralCommands()
        {
            if (flushingDeferredStructuralCommands || deferredStructuralCommands.empty())
                return;

            flushingDeferredStructuralCommands = true;
            while (!deferredStructuralCommands.empty())
            {
                auto commands = std::move(deferredStructuralCommands);
                deferredStructuralCommands.clear();
                for (auto& command : commands)
                    command(*this);
            }
            flushingDeferredStructuralCommands = false;
        }

        template<typename Component>
        Component* tryGetComponentPtr(Entity entity)
        {
            if (!hasSignatureBit<Component>(entity))
                return nullptr;

            if (entity.id >= entityLocations.size())
                return nullptr;

            auto& location = entityLocations[entity.id];
            if (!location.isAssigned())
                return nullptr;

            Archetype& archetype = archetypes[location.archetypeIndex];
            auto it = archetype.columns.find(std::type_index(typeid(Component)));
            if (it == archetype.columns.end())
                return nullptr;

            return static_cast<Component*>(it->second->rawPtr(location.rowIndex));
        }

        template<typename Component>
        const Component* tryGetComponentPtr(Entity entity) const
        {
            if (!hasSignatureBit<Component>(entity))
                return nullptr;

            if (entity.id >= entityLocations.size())
                return nullptr;

            const auto& location = entityLocations[entity.id];
            if (!location.isAssigned())
                return nullptr;

            const Archetype& archetype = archetypes[location.archetypeIndex];
            auto it = archetype.columns.find(std::type_index(typeid(Component)));
            if (it == archetype.columns.end())
                return nullptr;

            return static_cast<const Component*>(it->second->rawPtr(location.rowIndex));
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

        void ensureComponentOpsCapacity(uint32_t bitIndex)
        {
            if (componentTypeOps.size() <= bitIndex)
                componentTypeOps.resize(bitIndex + 1);
        }

        template<typename Component>
        static uint32_t componentBitIndex()
        {
            static const uint32_t bitIndex = registerComponentType<Component>();
            return bitIndex;
        }

        static uint32_t allocateComponentBit()
        {
            static uint32_t nextBit = 0;
            assert(nextBit < MAX_SIGNATURE_BITS && "Component signature bit budget exceeded");
            return nextBit++;
        }

        template<typename Component>
        static uint32_t registerComponentType()
        {
            const uint32_t bitIndex = allocateComponentBit();
            componentTypeInfoInitializer<Component>(bitIndex);
            return bitIndex;
        }

        template<typename Component>
        static void componentTypeInfoInitializer(uint32_t bitIndex)
        {
            auto& ops = componentOpsRegistry();
            if (ops.size() <= bitIndex)
                ops.resize(bitIndex + 1);

            ComponentTypeOps info;
            info.type = std::type_index(typeid(Component));
            info.createColumn = []()
            {
                return std::make_unique<ArchetypeColumn<Component>>();
            };
            info.appendValue = [](Archetype& archetype, const void* componentData)
            {
                auto& column = static_cast<ArchetypeColumn<Component>&>(
                    *archetype.columns.at(std::type_index(typeid(Component))));
                column.append(*static_cast<const Component*>(componentData));
            };
            info.moveValue = [](Archetype& source, uint32_t sourceRow, Archetype& destination, uint32_t)
            {
                auto& sourceColumn = static_cast<ArchetypeColumn<Component>&>(
                    *source.columns.at(std::type_index(typeid(Component))));
                auto& destinationColumn = static_cast<ArchetypeColumn<Component>&>(
                    *destination.columns.at(std::type_index(typeid(Component))));
                destinationColumn.append(sourceColumn.get(sourceRow));
            };
            info.getPtr = [](Archetype& archetype, uint32_t rowIndex) -> void*
            {
                auto& column = static_cast<ArchetypeColumn<Component>&>(
                    *archetype.columns.at(std::type_index(typeid(Component))));
                return &column.get(rowIndex);
            };
            info.getConstPtr = [](const Archetype& archetype, uint32_t rowIndex) -> const void*
            {
                const auto& column = static_cast<const ArchetypeColumn<Component>&>(
                    *archetype.columns.at(std::type_index(typeid(Component))));
                return &column.get(rowIndex);
            };
            ops[bitIndex] = std::move(info);
        }

        static std::vector<ComponentTypeOps>& componentOpsRegistry()
        {
            static std::vector<ComponentTypeOps> ops;
            return ops;
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

        const ComponentTypeOps& componentOpsForBit(uint32_t bitIndex) const
        {
            const auto& registry = componentOpsRegistry();
            assert(bitIndex < registry.size() && "Component ops not registered");
            return registry[bitIndex];
        }

        void initialiseArchetypeColumns(Archetype& archetype)
        {
            for (uint32_t bit = 0; bit < MAX_SIGNATURE_BITS; ++bit)
            {
                if (!archetype.signature.test(bit))
                    continue;

                const auto& ops = componentOpsForBit(bit);
                archetype.columns.emplace(ops.type, ops.createColumn());
            }
        }

        uint32_t getOrCreateArchetype(ComponentSignature signature)
        {
            auto it = archetypeIndexBySignature.find(signature);
            if (it != archetypeIndexBySignature.end())
                return it->second;

            const uint32_t archetypeIndex = static_cast<uint32_t>(archetypes.size());
            Archetype archetype;
            archetype.signature = signature;
            initialiseArchetypeColumns(archetype);
            archetypes.push_back(std::move(archetype));
            archetypeIndexBySignature.emplace(signature, archetypeIndex);
            return archetypeIndex;
        }

        void detachEntityFromArchetype(Entity entity, const EntityLocation& location)
        {
            if (!location.isAssigned())
                return;

            Archetype& archetype = archetypes[location.archetypeIndex];
            const uint32_t removedRow = location.rowIndex;
            const uint32_t lastRow = static_cast<uint32_t>(archetype.entities.size() - 1);
            assert(removedRow < archetype.entities.size() && "Archetype row out of bounds");
            assert(archetype.entities[removedRow] == entity && "Entity location/archetype row mismatch");

            if (removedRow != lastRow)
            {
                const Entity movedEntity = archetype.entities[lastRow];
                archetype.entities[removedRow] = movedEntity;
                entityLocations[movedEntity.id].rowIndex = removedRow;
            }

            for (auto& [_, column] : archetype.columns)
                column->swapRemove(removedRow);
            archetype.entities.pop_back();
        }

        void detachEntityFromArchetype(Entity entity)
        {
            if (entity.id >= entityLocations.size())
                return;

            const EntityLocation previousLocation = entityLocations[entity.id];
            if (!previousLocation.isAssigned())
                return;

            detachEntityFromArchetype(entity, previousLocation);

            auto& location = entityLocations[entity.id];
            location.archetypeIndex = EntityLocation::InvalidIndex;
            location.rowIndex = EntityLocation::InvalidIndex;
        }

        void moveEntityToArchetype(Entity entity,
                                   ComponentSignature signature,
                                   std::type_index modifiedType = std::type_index(typeid(void)),
                                   const void* addedComponentData = nullptr)
        {
            ensureEntityLocationCapacity(entity.id);
            EntityLocation previousLocation{};
            uint32_t sourceArchetypeIndex = EntityLocation::InvalidIndex;
            uint32_t sourceRow = EntityLocation::InvalidIndex;
            if (entity.id < entityLocations.size())
            {
                previousLocation = entityLocations[entity.id];
                if (previousLocation.isAssigned())
                {
                    sourceArchetypeIndex = previousLocation.archetypeIndex;
                    sourceRow = previousLocation.rowIndex;
                }
            }

            if (previousLocation.isAssigned()
                && previousLocation.signature == signature
                && modifiedType == std::type_index(typeid(void)))
            {
                return;
            }

            const uint32_t archetypeIndex = getOrCreateArchetype(signature);
            Archetype& archetype = archetypes[archetypeIndex];
            const uint32_t rowIndex = static_cast<uint32_t>(archetype.entities.size());
            archetype.entities.push_back(entity);

            for (uint32_t bit = 0; bit < MAX_SIGNATURE_BITS; ++bit)
            {
                if (!signature.test(bit))
                    continue;

                const auto& ops = componentOpsForBit(bit);
                if (ops.type == modifiedType && addedComponentData != nullptr)
                {
                    ops.appendValue(archetype, addedComponentData);
                    continue;
                }

                assert(sourceArchetypeIndex != EntityLocation::InvalidIndex
                    && "Source archetype index invalid for migrated component");
                Archetype& sourceArchetype = archetypes[sourceArchetypeIndex];
                assert(sourceArchetype.signature.test(bit)
                    && "Source archetype missing migrated component");
                ops.moveValue(sourceArchetype, sourceRow, archetype, rowIndex);
            }

            if (previousLocation.isAssigned())
                detachEntityFromArchetype(entity, previousLocation);

            auto& location = entityLocations[entity.id];
            location.signature = signature;
            location.archetypeIndex = archetypeIndex;
            location.rowIndex = rowIndex;
        }

        void refreshEntityArchetype(Entity entity,
                                    std::type_index modifiedType = std::type_index(typeid(void)),
                                    const void* addedComponentData = nullptr)
        {
            moveEntityToArchetype(entity, getEntitySignature(entity), modifiedType, addedComponentData);
        }

        template<typename Component>
        Component* getArchetypeComponentPtr(Entity entity)
        {
            return tryGetComponentPtr<Component>(entity);
        }

    public:
        ECSWorld()
            : commandBuffer(*this)
        {
        }

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
            if (e.id >= entityLocations.size())
                return;

            const EntityLocation location = entityLocations[e.id];
            if (!location.isAssigned())
                return;

            Archetype& archetype = archetypes[location.archetypeIndex];
            const ComponentSignature signature = location.signature;

            for (uint32_t bit = 0; bit < MAX_SIGNATURE_BITS; ++bit)
            {
                if (!signature.test(bit))
                    continue;

                const auto& ops = componentOpsForBit(bit);
                invokeRemoveCallback(ops.type, e, ops.getPtr(archetype, location.rowIndex));
            }

            detachEntityFromArchetype(e);
            if (e.id < entityLocations.size())
                entityLocations[e.id] = EntityLocation{};
        }

        template<typename Component>
        void addComponent(Entity entity, const Component& component)
        {
            ensureEntityLocationCapacity(entity.id);
            setSignatureBit<Component>(entity);
            refreshEntityArchetype(entity, std::type_index(typeid(Component)), &component);
            invokeAddCallback(std::type_index(typeid(Component)), entity, getArchetypeComponentPtr<Component>(entity));
        }

        template<typename Component>
        bool hasComponent(Entity entity) const
        {
            return hasSignatureBit<Component>(entity);
        }

        template<typename Component>
        Component& getComponent(Entity entity)
        {
            assert(hasSignatureBit<Component>(entity) && "Component signature missing for entity");
            auto* component = getArchetypeComponentPtr<Component>(entity);
            assert(component != nullptr && "Archetype component missing for entity");
            return *component;
        }

        template<typename Component>
        const Component& getComponent(Entity entity) const
        {
            assert(hasSignatureBit<Component>(entity) && "Component signature missing for entity");
            const auto* component = tryGetComponentPtr<Component>(entity);
            assert(component != nullptr && "Archetype component missing for entity");
            return *component;
        }

        template<typename Component>
        void removeComponent(Entity entity)
        {
            if (!hasSignatureBit<Component>(entity))
                return;

            invokeRemoveCallback(std::type_index(typeid(Component)), entity, getArchetypeComponentPtr<Component>(entity));
            clearSignatureBit<Component>(entity);
            refreshEntityArchetype(entity, std::type_index(typeid(Component)), nullptr);
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

        // Snapshot iteration explicitly supports structural mutation inside the
        // callback by collecting entity IDs up front. Use this when a system
        // must create/destroy entities or add/remove components while walking a
        // component set. Hot-path systems should migrate to plain query/forEach
        // and remain non-structural.
        template<typename... Components, typename Func>
        void forEachSnapshot(Func&& fn)
        {
            auto entities = getAllEntitiesWith<Components...>();
            for (Entity entity : entities)
            {
                if (!(hasComponent<Components>(entity) && ...))
                    continue;

                fn(entity, getComponent<Components>(entity)...);
            }
        }

        template<typename... Components>
        ECSQuery<Components...> query()
        {
            return ECSQuery<Components...>(*this);
        }

        EntityCommandBuffer& commands()
        {
            return commandBuffer;
        }

        void flushCommands()
        {
            flushDeferredStructuralCommands();
        }

        void updateSimulation(const EcsSimulationContext& ctx)
        {
            for (auto& system : simulationSystems)
            {
                system->update(ctx);
                flushDeferredStructuralCommands();
            }
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

                flushDeferredStructuralCommands();
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
            std::vector<Entity> entities;
            for (const Archetype& archetype : archetypes)
            {
                entities.insert(entities.end(), archetype.entities.begin(), archetype.entities.end());
            }

            for (Entity entity : entities)
                destroyEntity(entity);

            // Systems are destroyed here; their SubscriptionToken members call
            // unsubscribe. The eventHandlers map is still valid at this point.
            simulationSystems.clear();
            controllerSystems.clear();
            addCallbacks.clear();
            controllerProfiles.clear();
            controllerProfilePrintScratch.clear();
            forEachScratch.clear();
            forEachScratch.shrink_to_fit();
            deferredStructuralCommands.clear();
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
            const auto requiredMask = requiredSignatureMask<Component>();
            for (const Archetype& archetype : archetypes)
            {
                if (archetype.signature.containsAll(requiredMask) && !archetype.entities.empty())
                    return true;
            }
            return false;
        }

        template<typename Component>
        Entity getSingletonEntity() const
        {
            const auto requiredMask = requiredSignatureMask<Component>();
            uint32_t count = 0;
            Entity result{};
            for (const Archetype& archetype : archetypes)
            {
                if (!archetype.signature.containsAll(requiredMask))
                    continue;
                count += static_cast<uint32_t>(archetype.entities.size());
                if (!archetype.entities.empty())
                    result = archetype.entities[0];
            }
            assert(count > 0 && "Singleton component does not exist");
            assert(count == 1 && "More than one singleton component exists");
            return result;
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
            return getComponent<Component>(e);
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
            addComponent<Component>(e, Component{ std::forward<Args>(args)... });
            return getComponent<Component>(e);
        }

        // ------------------------------------------------------------

        size_t getEntityCount() const
        {
            size_t count = 0;
            for (const Archetype& archetype : archetypes)
                count += archetype.entities.size();
            return count;
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

        const auto requiredMask = ECSWorld::template requiredSignatureMask<Components...>();
        world.forEachScratch.clear();
        world.forEachScratch.reserve(estimatedIterationCount());

        for (Archetype& archetype : world.archetypes)
        {
            if (!archetype.signature.containsAll(requiredMask))
                continue;

            for (Entity entity : archetype.entities)
                world.forEachScratch.push_back(entity.id);
        }

        // Preserve the legacy forEach contract: structural changes inside the
        // callback must not invalidate iteration or crash the world. The
        // snapshot only stores entity IDs; component payloads are still fetched
        // from archetype columns on demand, so the hot path stays off the legacy
        // sparse storage while remaining mutation-safe.
        for (uint32_t entityId : world.forEachScratch)
        {
            Entity entity{entityId};
            if (!world.matchesSignature(entity, requiredMask))
                continue;

            invoke(std::forward<Func>(fn), entity, std::index_sequence_for<Components...>{});
        }
    }

    uint32_t estimatedIterationCount() const
    {
        const auto requiredMask = ECSWorld::template requiredSignatureMask<Components...>();
        uint32_t total = 0;
        for (const Archetype& archetype : world.archetypes)
        {
            if (archetype.signature.containsAll(requiredMask))
                total += static_cast<uint32_t>(archetype.entities.size());
        }
        return total;
    }

private:
    using ComponentTuple = std::tuple<Components...>;

    ECSWorld& world;

    template<typename Func, size_t... I>
    void invoke(Func&& fn, Entity entity, std::index_sequence<I...>)
    {
        fn(entity, world.template getComponent<typename std::tuple_element<I, ComponentTuple>::type>(entity)...);
    }
};
