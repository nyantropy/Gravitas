#pragma once

#include <algorithm>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ECSWorld.hpp"
#include "IResourceProvider.hpp"
#include "MaterialTypes.h"

namespace gts::rendering
{
    class MaterialRuntime
    {
    public:
        MaterialRuntime()
        {
            initializeBuiltIns();
        }

        MaterialDefinitionHandle createDefinition(const MaterialDefinition& definition)
        {
            DefinitionSlot slot;
            slot.definition = definition;
            slot.alive = true;
            slot.generation = 1;

            definitions.push_back(slot);
            return {static_cast<uint32_t>(definitions.size()), slot.generation};
        }

        bool destroyDefinition(MaterialDefinitionHandle handle)
        {
            DefinitionSlot* slot = findDefinitionSlot(handle);
            if (slot == nullptr || slot->builtIn)
                return false;

            for (const InstanceSlot& instance : instances)
            {
                if (instance.alive && instance.instance.definition == handle)
                    return false;
            }

            slot->alive = false;
            slot->generation += 1;
            return true;
        }

        MaterialInstanceHandle createInstance(const MaterialInstance& instance)
        {
            return createInstanceInternal(instance, false);
        }

        MaterialInstanceHandle cloneInstance(MaterialInstanceHandle source)
        {
            const MaterialInstance* instance = getInstance(source);
            if (instance == nullptr)
                instance = getInstance(defaultMaterial());

            MaterialInstance clone = *instance;
            clone.version = 1;
            return createInstance(clone);
        }

        bool destroyInstance(MaterialInstanceHandle handle)
        {
            InstanceSlot* slot = findInstanceSlot(handle);
            if (slot == nullptr || slot->builtIn)
                return false;

            slot->alive = false;
            slot->generation += 1;
            gpuStates.erase(handle);
            return true;
        }

        const MaterialDefinition* getDefinition(MaterialDefinitionHandle handle) const
        {
            const DefinitionSlot* slot = findDefinitionSlot(handle);
            return slot == nullptr ? nullptr : &slot->definition;
        }

        const MaterialInstance* getInstance(MaterialInstanceHandle handle) const
        {
            const InstanceSlot* slot = findInstanceSlot(handle);
            return slot == nullptr ? nullptr : &slot->instance;
        }

        bool isInstanceAlive(MaterialInstanceHandle handle) const
        {
            return findInstanceSlot(handle) != nullptr;
        }

        bool setInstance(MaterialInstanceHandle handle, const MaterialInstance& next)
        {
            InstanceSlot* slot = findInstanceSlot(handle);
            if (slot == nullptr)
                return false;

            MaterialInstance copy = next;
            if (getDefinition(copy.definition) == nullptr)
                copy.definition = defaultDefinitionHandle;
            sanitizeMaterialInstance(copy);
            copy.version = slot->instance.version + 1;
            slot->instance = copy;
            return true;
        }

        bool modifyInstance(MaterialInstanceHandle handle,
                            const std::function<bool(MaterialInstance&)>& modify)
        {
            InstanceSlot* slot = findInstanceSlot(handle);
            if (slot == nullptr)
                return false;

            const bool changed = modify(slot->instance);
            if (!changed)
                return false;

            if (getDefinition(slot->instance.definition) == nullptr)
                slot->instance.definition = defaultDefinitionHandle;
            sanitizeMaterialInstance(slot->instance);
            slot->instance.version += 1;
            if (slot->instance.version == 0)
                slot->instance.version = 1;
            return true;
        }

        MaterialInstanceHandle defaultMaterial() const
        {
            return defaultMaterialHandle;
        }

        MaterialInstanceHandle vertexColorOnlyMaterial() const
        {
            return vertexColorOnlyMaterialHandle;
        }

        MaterialInstanceHandle defaultStandardSurfaceMaterial() const
        {
            return defaultStandardSurfaceMaterialHandle;
        }

        MaterialInstanceHandle resolveInstanceHandle(MaterialInstanceHandle handle) const
        {
            return isInstanceAlive(handle) ? handle : defaultMaterialHandle;
        }

        MaterialSyncResult synchronizeGpuState(MaterialInstanceHandle requestedHandle,
                                               IResourceProvider* resources)
        {
            const MaterialInstanceHandle handle = resolveInstanceHandle(requestedHandle);
            const MaterialInstance* instance = getInstance(handle);
            if (instance == nullptr)
                return {};

            auto [it, inserted] = gpuStates.emplace(handle, MaterialGpuState{});
            MaterialGpuState& state = it->second;
            if (inserted)
            {
                state.gpuHandle = {nextGpuHandle++, handle.generation};
                state.instance = handle;
            }

            const texture_id_type textureID = resolveTexture(*instance, state, resources);
            if (textureID == 0 && resources == nullptr)
                return {&state, false, false, false};

            const MaterialDefinition* definition = getDefinition(instance->definition);
            const MaterialShaderFamily shaderFamily = definition != nullptr
                ? definition->shaderFamily
                : MaterialShaderFamily::LegacyUnlit;

            MaterialGpuState next = state;
            next.instance = handle;
            next.uploadedVersion = instance->version;
            next.shaderFamily = shaderFamily;
            next.baseColorTextureID = textureID;
            next.parameters = makeMaterialGpuParameters(
                instance->baseColor,
                instance->metallic,
                instance->roughness);
            next.renderState = instance->renderState;
            next.vertexColorOnly = instance->vertexColorOnly;
            next.variantKey = makeMaterialVariantKey(shaderFamily, *instance);
            next.boundTexturePath = instance->baseColorTexture.source == MaterialTextureSource::AssetPath
                ? instance->baseColorTexture.path
                : std::string{};
            next.boundTextureColorSpace = instance->baseColorTexture.colorSpace;

            const bool textureChanged =
                inserted
                || state.baseColorTextureID != next.baseColorTextureID
                || state.boundTextureColorSpace != next.boundTextureColorSpace;
            const bool parameterDataChanged =
                inserted
                || state.parameters.baseColor != next.parameters.baseColor
                || state.parameters.surfaceParameters != next.parameters.surfaceParameters;
            const bool topologyChanged =
                state.shaderFamily != next.shaderFamily
                || state.renderState.alphaMode != next.renderState.alphaMode
                || state.renderState.legacyBlendMode != next.renderState.legacyBlendMode
                || state.renderState.doubleSided != next.renderState.doubleSided
                || state.renderState.depthWrite != next.renderState.depthWrite
                || state.vertexColorOnly != next.vertexColorOnly
                || state.variantKey != next.variantKey;
            const bool parameterChanged = textureChanged || parameterDataChanged;
            const bool versionChanged = state.uploadedVersion != next.uploadedVersion;
            const bool changed = inserted || versionChanged || topologyChanged || parameterChanged;

            if (changed)
                state = next;

            return {&state, changed, parameterChanged, topologyChanged};
        }

        const MaterialGpuState* getGpuState(MaterialInstanceHandle handle) const
        {
            auto it = gpuStates.find(resolveInstanceHandle(handle));
            return it == gpuStates.end() ? nullptr : &it->second;
        }

        size_t gpuStateCount() const
        {
            return gpuStates.size();
        }

    private:
        struct DefinitionSlot
        {
            MaterialDefinition definition;
            uint32_t generation = 1;
            bool alive = false;
            bool builtIn = false;
        };

        struct InstanceSlot
        {
            MaterialInstance instance;
            uint32_t generation = 1;
            bool alive = false;
            bool builtIn = false;
        };

        std::vector<DefinitionSlot> definitions;
        std::vector<InstanceSlot> instances;
        std::unordered_map<MaterialInstanceHandle, MaterialGpuState> gpuStates;
        MaterialDefinitionHandle defaultDefinitionHandle;
        MaterialDefinitionHandle standardSurfaceDefinitionHandle;
        MaterialInstanceHandle defaultMaterialHandle;
        MaterialInstanceHandle vertexColorOnlyMaterialHandle;
        MaterialInstanceHandle defaultStandardSurfaceMaterialHandle;
        uint32_t nextGpuHandle = 1;

        void initializeBuiltIns()
        {
            MaterialDefinition definition;
            definition.shaderFamily = MaterialShaderFamily::LegacyUnlit;
            defaultDefinitionHandle = createDefinition(definition);
            if (!definitions.empty())
                definitions.back().builtIn = true;

            MaterialInstance defaultInstance;
            defaultInstance.definition = defaultDefinitionHandle;
            defaultInstance.baseColorTexture = MaterialTextureBinding::assetPath({});
            defaultInstance.renderState.alphaMode = MaterialAlphaMode::Opaque;
            defaultInstance.renderState.depthWrite = true;
            defaultMaterialHandle = createInstanceInternal(defaultInstance, true);

            MaterialInstance vertexColorInstance = defaultInstance;
            vertexColorInstance.vertexColorOnly = true;
            vertexColorInstance.renderState.doubleSided = true;
            vertexColorOnlyMaterialHandle = createInstanceInternal(vertexColorInstance, true);

            MaterialDefinition standardDefinition;
            standardDefinition.shaderFamily = MaterialShaderFamily::StandardSurface;
            standardSurfaceDefinitionHandle = createDefinition(standardDefinition);
            if (!definitions.empty())
                definitions.back().builtIn = true;

            MaterialInstance standardInstance;
            standardInstance.definition = standardSurfaceDefinitionHandle;
            standardInstance.baseColorTexture = MaterialTextureBinding::assetPath({});
            standardInstance.baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
            standardInstance.metallic = 0.0f;
            standardInstance.roughness = 1.0f;
            standardInstance.renderState.alphaMode = MaterialAlphaMode::Opaque;
            standardInstance.renderState.depthWrite = true;
            defaultStandardSurfaceMaterialHandle = createInstanceInternal(standardInstance, true);
        }

        MaterialInstanceHandle createInstanceInternal(MaterialInstance instance, bool builtIn)
        {
            if (getDefinition(instance.definition) == nullptr)
                instance.definition = defaultDefinitionHandle;
            sanitizeMaterialInstance(instance);
            if (instance.version == 0)
                instance.version = 1;

            InstanceSlot slot;
            slot.instance = std::move(instance);
            slot.alive = true;
            slot.builtIn = builtIn;
            slot.generation = 1;

            instances.push_back(slot);
            return {static_cast<uint32_t>(instances.size()), slot.generation};
        }

        static void sanitizeMaterialInstance(MaterialInstance& instance)
        {
            instance.metallic = sanitizeMaterialMetallic(instance.metallic);
            instance.roughness = sanitizeMaterialRoughness(instance.roughness);
        }

        DefinitionSlot* findDefinitionSlot(MaterialDefinitionHandle handle)
        {
            if (!handle.valid() || handle.id > definitions.size())
                return nullptr;

            DefinitionSlot& slot = definitions[handle.id - 1];
            if (!slot.alive || slot.generation != handle.generation)
                return nullptr;
            return &slot;
        }

        const DefinitionSlot* findDefinitionSlot(MaterialDefinitionHandle handle) const
        {
            if (!handle.valid() || handle.id > definitions.size())
                return nullptr;

            const DefinitionSlot& slot = definitions[handle.id - 1];
            if (!slot.alive || slot.generation != handle.generation)
                return nullptr;
            return &slot;
        }

        InstanceSlot* findInstanceSlot(MaterialInstanceHandle handle)
        {
            if (!handle.valid() || handle.id > instances.size())
                return nullptr;

            InstanceSlot& slot = instances[handle.id - 1];
            if (!slot.alive || slot.generation != handle.generation)
                return nullptr;
            return &slot;
        }

        const InstanceSlot* findInstanceSlot(MaterialInstanceHandle handle) const
        {
            if (!handle.valid() || handle.id > instances.size())
                return nullptr;

            const InstanceSlot& slot = instances[handle.id - 1];
            if (!slot.alive || slot.generation != handle.generation)
                return nullptr;
            return &slot;
        }

        texture_id_type resolveTexture(const MaterialInstance& instance,
                                       const MaterialGpuState& currentState,
                                       IResourceProvider* resources) const
        {
            const MaterialTextureBinding& texture = instance.baseColorTexture;
            if (texture.source == MaterialTextureSource::ResolvedTexture)
                return texture.resolvedTextureID;

            if (resources == nullptr)
                return currentState.baseColorTextureID;

            if (texture.source == MaterialTextureSource::None)
                return resources->requestTexture({}, TextureColorSpace::SRgb);

            if (currentState.baseColorTextureID != 0
                && currentState.boundTexturePath == texture.path
                && currentState.boundTextureColorSpace == texture.colorSpace)
            {
                return currentState.baseColorTextureID;
            }

            return resources->requestTexture(texture.path, texture.colorSpace);
        }
    };

    inline auto& materialRuntimeRegistry()
    {
        static std::unordered_map<ECSWorld*, MaterialRuntime> registry;
        return registry;
    }

    inline MaterialRuntime& materialRuntime(ECSWorld& world)
    {
        return materialRuntimeRegistry()[&world];
    }

    inline void resetMaterialRuntime(ECSWorld& world)
    {
        materialRuntimeRegistry().erase(&world);
    }
}
