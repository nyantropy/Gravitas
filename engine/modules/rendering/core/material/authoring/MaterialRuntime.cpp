#include "MaterialRuntime.h"

#include <algorithm>
#include <utility>
#include "IResourceProvider.hpp"

namespace gts::rendering
{
    MaterialRuntime::MaterialRuntime()
    {
        initializeBuiltIns();
    }

    std::weak_ptr<const int> MaterialRuntime::lifetimeToken() const
    {
        return lifetime;
    }

    MaterialFrameState MaterialRuntime::frameState(MaterialInstanceHandle handle) const
    {
        if (!isInstanceAlive(handle))
            return {};
        const auto* gpu   = getGpuState(handle);
        auto        state = describeInstance(handle, gpu ? gpu->textures : MaterialTextureIds{});
        if (gpu)
            state.gpuHandle = gpu->gpuHandle;
        return makeMaterialFrameState(state);
    }

    MaterialDefinitionHandle MaterialRuntime::createDefinition(const MaterialDefinition& definition)
    {
        DefinitionSlot slot;
        slot.definition = definition;
        slot.alive      = true;
        slot.generation = 1;

        definitions.push_back(slot);
        return {static_cast<uint32_t>(definitions.size()), slot.generation};
    }

    bool MaterialRuntime::destroyDefinition(MaterialDefinitionHandle handle)
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

    MaterialInstanceHandle MaterialRuntime::createInstance(const MaterialInstance& instance)
    {
        return createInstanceInternal(instance, false);
    }

    MaterialInstanceHandle MaterialRuntime::cloneInstance(MaterialInstanceHandle source)
    {
        const MaterialInstance* instance = getInstance(source);
        if (instance == nullptr)
            instance = getInstance(defaultMaterial());

        MaterialInstance clone = *instance;
        clone.version          = 1;
        return createInstance(clone);
    }

    bool MaterialRuntime::destroyInstance(MaterialInstanceHandle handle)
    {
        InstanceSlot* slot = findInstanceSlot(handle);
        if (slot == nullptr || slot->builtIn)
            return false;

        slot->alive = false;
        slot->generation += 1;
        gpuStates.erase(handle);
        queueMaterialSync(handle);
        return true;
    }

    const MaterialDefinition* MaterialRuntime::getDefinition(MaterialDefinitionHandle handle) const
    {
        const DefinitionSlot* slot = findDefinitionSlot(handle);
        return slot == nullptr ? nullptr : &slot->definition;
    }

    const MaterialInstance* MaterialRuntime::getInstance(MaterialInstanceHandle handle) const
    {
        const InstanceSlot* slot = findInstanceSlot(handle);
        return slot == nullptr ? nullptr : &slot->instance;
    }

    bool MaterialRuntime::isInstanceAlive(MaterialInstanceHandle handle) const
    {
        return findInstanceSlot(handle) != nullptr;
    }

    bool MaterialRuntime::setInstance(MaterialInstanceHandle handle, const MaterialInstance& next)
    {
        InstanceSlot* slot = findInstanceSlot(handle);
        if (slot == nullptr)
            return false;

        MaterialInstance copy = next;
        if (getDefinition(copy.definition) == nullptr)
            copy.definition = defaultDefinitionHandle;
        sanitizeMaterialInstance(copy);
        copy.version   = slot->instance.version + 1;
        slot->instance = copy;
        queueMaterialSync(handle);
        return true;
    }

    bool MaterialRuntime::modifyInstance(MaterialInstanceHandle                        handle,
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
        queueMaterialSync(handle);
        return true;
    }

    MaterialInstanceHandle MaterialRuntime::defaultMaterial() const
    {
        return defaultMaterialHandle;
    }

    MaterialInstanceHandle MaterialRuntime::vertexColorOnlyMaterial() const
    {
        return vertexColorOnlyMaterialHandle;
    }

    MaterialInstanceHandle MaterialRuntime::defaultStandardSurfaceMaterial() const
    {
        return defaultStandardSurfaceMaterialHandle;
    }

    MaterialInstanceHandle MaterialRuntime::resolveInstanceHandle(MaterialInstanceHandle handle) const
    {
        return isInstanceAlive(handle) ? handle : defaultMaterialHandle;
    }

    MaterialSyncResult MaterialRuntime::synchronizeGpuState(MaterialInstanceHandle requestedHandle,
                                                            IResourceProvider*     resources)
    {
        const MaterialInstanceHandle handle   = resolveInstanceHandle(requestedHandle);
        const MaterialInstance*      instance = getInstance(handle);
        if (instance == nullptr)
            return {};

        auto [it, inserted]     = gpuStates.emplace(handle, MaterialGpuState{});
        MaterialGpuState& state = it->second;
        if (inserted)
        {
            state.gpuHandle = {nextGpuHandle++, handle.generation};
            state.instance  = handle;
        }

        const MaterialTextureIds textureIDs = resolveTextures(*instance, state, resources);
        if (textureIDs.baseColor == 0 && resources == nullptr)
            return {&state, false, false, false};

        MaterialGpuState next = describeInstance(handle, textureIDs);
        next.gpuHandle        = state.gpuHandle;

        const bool textureChanged = inserted || state.textures != next.textures ||
                                    state.textureCache != next.textureCache || state.featureFlags != next.featureFlags;
        const bool parameterDataChanged =
            inserted || state.parameters.baseColor != next.parameters.baseColor ||
            state.parameters.surfaceParameters != next.parameters.surfaceParameters ||
            state.parameters.emissiveFactorStrength != next.parameters.emissiveFactorStrength;
        const bool topologyChanged =
            state.shaderFamily != next.shaderFamily || state.renderState.alphaMode != next.renderState.alphaMode ||
            state.renderState.blendMode != next.renderState.blendMode ||
            state.renderState.doubleSided != next.renderState.doubleSided ||
            state.renderState.depthWrite != next.renderState.depthWrite ||
            state.vertexColorOnly != next.vertexColorOnly || state.variantKey != next.variantKey;
        const bool parameterChanged = textureChanged || parameterDataChanged;
        const bool versionChanged   = state.uploadedVersion != next.uploadedVersion;
        const bool changed          = inserted || versionChanged || topologyChanged || parameterChanged;

        if (changed)
            state = next;

        return {&state, changed, parameterChanged, topologyChanged};
    }

    const MaterialGpuState* MaterialRuntime::getGpuState(MaterialInstanceHandle handle) const
    {
        auto it = gpuStates.find(resolveInstanceHandle(handle));
        return it == gpuStates.end() ? nullptr : &it->second;
    }

    void MaterialRuntime::queueMaterialSync(MaterialInstanceHandle handle)
    {
        if (!handle.valid())
            return;

        if (queuedMaterialSet.insert(handle).second)
            queuedMaterials.push_back(handle);
    }

    std::vector<MaterialInstanceHandle> MaterialRuntime::takeQueuedMaterialSyncs()
    {
        std::vector<MaterialInstanceHandle> pending;
        pending.swap(queuedMaterials);
        queuedMaterialSet.clear();
        return pending;
    }

    size_t MaterialRuntime::queuedMaterialSyncCount() const
    {
        return queuedMaterials.size();
    }

    bool MaterialRuntime::materialNeedsGpuSync(MaterialInstanceHandle requestedHandle) const
    {
        const MaterialInstanceHandle handle   = resolveInstanceHandle(requestedHandle);
        const MaterialInstance*      instance = getInstance(handle);
        if (instance == nullptr)
            return false;

        auto it = gpuStates.find(handle);
        return it == gpuStates.end() || it->second.uploadedVersion != instance->version;
    }

    size_t MaterialRuntime::gpuStateCount() const
    {
        return gpuStates.size();
    }

    MaterialGpuState MaterialRuntime::describeInstance(MaterialInstanceHandle    handle,
                                                       const MaterialTextureIds& textureIDs) const
    {
        const auto*                instance   = getInstance(handle);
        const MaterialDefinition*  definition = getDefinition(instance->definition);
        const MaterialShaderFamily shaderFamily =
            definition != nullptr ? definition->shaderFamily : MaterialShaderFamily::Unlit;
        const MaterialFeatureFlags featureFlags = materialFeatureFlagsForInstance(*instance);

        MaterialGpuState next;
        next.instance           = handle;
        next.uploadedVersion    = instance->version;
        next.shaderFamily       = shaderFamily;
        next.textures           = textureIDs;
        next.baseColorTextureID = textureIDs.baseColor;
        next.parameters         = makeMaterialGpuParameters(instance->baseColor,
                                                            instance->metallic,
                                                            instance->roughness,
                                                            instance->normalScale,
                                                            instance->ambientOcclusionStrength,
                                                            instance->emissiveFactor,
                                                            instance->emissiveStrength);
        next.renderState        = instance->renderState;
        next.vertexColorOnly    = instance->vertexColorOnly;
        next.featureFlags       = featureFlags;
        next.variantKey         = makeMaterialVariantKey(shaderFamily, *instance, featureFlags);
        next.textureCache       = makeTextureCacheState(*instance);

        return next;
    }

    void MaterialRuntime::initializeBuiltIns()
    {
        MaterialDefinition definition;
        definition.shaderFamily = MaterialShaderFamily::Unlit;
        defaultDefinitionHandle = createDefinition(definition);
        if (!definitions.empty())
            definitions.back().builtIn = true;

        MaterialInstance defaultInstance;
        defaultInstance.definition             = defaultDefinitionHandle;
        defaultInstance.baseColorTexture       = MaterialTextureBinding::assetPath({});
        defaultInstance.renderState.alphaMode  = MaterialAlphaMode::Opaque;
        defaultInstance.renderState.depthWrite = true;
        defaultMaterialHandle                  = createInstanceInternal(defaultInstance, true);

        MaterialInstance vertexColorInstance        = defaultInstance;
        vertexColorInstance.vertexColorOnly         = true;
        vertexColorInstance.renderState.doubleSided = true;
        vertexColorOnlyMaterialHandle               = createInstanceInternal(vertexColorInstance, true);

        MaterialDefinition standardDefinition;
        standardDefinition.shaderFamily = MaterialShaderFamily::StandardSurface;
        standardSurfaceDefinitionHandle = createDefinition(standardDefinition);
        if (!definitions.empty())
            definitions.back().builtIn = true;

        MaterialInstance standardInstance;
        standardInstance.definition             = standardSurfaceDefinitionHandle;
        standardInstance.baseColorTexture       = MaterialTextureBinding::assetPath({});
        standardInstance.baseColor              = {1.0f, 1.0f, 1.0f, 1.0f};
        standardInstance.metallic               = 0.0f;
        standardInstance.roughness              = 1.0f;
        standardInstance.renderState.alphaMode  = MaterialAlphaMode::Opaque;
        standardInstance.renderState.depthWrite = true;
        defaultStandardSurfaceMaterialHandle    = createInstanceInternal(standardInstance, true);
    }

    MaterialInstanceHandle MaterialRuntime::createInstanceInternal(MaterialInstance instance, bool builtIn)
    {
        if (getDefinition(instance.definition) == nullptr)
            instance.definition = defaultDefinitionHandle;
        sanitizeMaterialInstance(instance);
        if (instance.version == 0)
            instance.version = 1;

        InstanceSlot slot;
        slot.instance   = std::move(instance);
        slot.alive      = true;
        slot.builtIn    = builtIn;
        slot.generation = 1;

        instances.push_back(slot);
        MaterialInstanceHandle handle{static_cast<uint32_t>(instances.size()), slot.generation};
        if (!builtIn)
            queueMaterialSync(handle);
        return handle;
    }

    void MaterialRuntime::sanitizeMaterialInstance(MaterialInstance& instance)
    {
        instance.metallic                 = sanitizeMaterialMetallic(instance.metallic);
        instance.roughness                = sanitizeMaterialRoughness(instance.roughness);
        instance.normalScale              = sanitizeMaterialNormalScale(instance.normalScale);
        instance.ambientOcclusionStrength = sanitizeMaterialAmbientOcclusionStrength(instance.ambientOcclusionStrength);
        instance.emissiveFactor           = sanitizeMaterialEmissiveFactor(instance.emissiveFactor);
        instance.emissiveStrength         = sanitizeMaterialEmissiveStrength(instance.emissiveStrength);

        instance.baseColorTexture.colorSpace         = textureColorSpaceForRole(MaterialTextureRole::BaseColor);
        instance.metallicRoughnessTexture.colorSpace = textureColorSpaceForRole(MaterialTextureRole::MetallicRoughness);
        instance.normalTexture.colorSpace            = textureColorSpaceForRole(MaterialTextureRole::Normal);
        instance.ambientOcclusionTexture.colorSpace  = textureColorSpaceForRole(MaterialTextureRole::AmbientOcclusion);
        instance.emissiveTexture.colorSpace          = textureColorSpaceForRole(MaterialTextureRole::Emissive);
    }

    MaterialRuntime::DefinitionSlot* MaterialRuntime::findDefinitionSlot(MaterialDefinitionHandle handle)
    {
        if (!handle.valid() || handle.id > definitions.size())
            return nullptr;

        DefinitionSlot& slot = definitions[handle.id - 1];
        if (!slot.alive || slot.generation != handle.generation)
            return nullptr;
        return &slot;
    }

    const MaterialRuntime::DefinitionSlot* MaterialRuntime::findDefinitionSlot(MaterialDefinitionHandle handle) const
    {
        if (!handle.valid() || handle.id > definitions.size())
            return nullptr;

        const DefinitionSlot& slot = definitions[handle.id - 1];
        if (!slot.alive || slot.generation != handle.generation)
            return nullptr;
        return &slot;
    }

    MaterialRuntime::InstanceSlot* MaterialRuntime::findInstanceSlot(MaterialInstanceHandle handle)
    {
        if (!handle.valid() || handle.id > instances.size())
            return nullptr;

        InstanceSlot& slot = instances[handle.id - 1];
        if (!slot.alive || slot.generation != handle.generation)
            return nullptr;
        return &slot;
    }

    const MaterialRuntime::InstanceSlot* MaterialRuntime::findInstanceSlot(MaterialInstanceHandle handle) const
    {
        if (!handle.valid() || handle.id > instances.size())
            return nullptr;

        const InstanceSlot& slot = instances[handle.id - 1];
        if (!slot.alive || slot.generation != handle.generation)
            return nullptr;
        return &slot;
    }

    const MaterialTextureBinding& MaterialRuntime::textureBindingForRole(const MaterialInstance& instance,
                                                                         MaterialTextureRole     role)
    {
        switch (role)
        {
        case MaterialTextureRole::BaseColor:
            return instance.baseColorTexture;
        case MaterialTextureRole::MetallicRoughness:
            return instance.metallicRoughnessTexture;
        case MaterialTextureRole::Normal:
            return instance.normalTexture;
        case MaterialTextureRole::AmbientOcclusion:
            return instance.ambientOcclusionTexture;
        case MaterialTextureRole::Emissive:
            return instance.emissiveTexture;
        }
        return instance.baseColorTexture;
    }

    texture_id_type MaterialRuntime::textureIdForRole(const MaterialTextureIds& textures, MaterialTextureRole role)
    {
        switch (role)
        {
        case MaterialTextureRole::BaseColor:
            return textures.baseColor;
        case MaterialTextureRole::MetallicRoughness:
            return textures.metallicRoughness;
        case MaterialTextureRole::Normal:
            return textures.normal;
        case MaterialTextureRole::AmbientOcclusion:
            return textures.ambientOcclusion;
        case MaterialTextureRole::Emissive:
            return textures.emissive;
        }
        return textures.baseColor;
    }

    std::string MaterialRuntime::assetPathForRole(const MaterialInstance& instance, MaterialTextureRole role)
    {
        const MaterialTextureBinding& texture = textureBindingForRole(instance, role);
        return texture.source == MaterialTextureSource::AssetPath ? texture.path : std::string{};
    }

    MaterialTextureCacheState MaterialRuntime::makeTextureCacheState(const MaterialInstance& instance)
    {
        MaterialTextureCacheState cache;
        cache.baseColorPath               = assetPathForRole(instance, MaterialTextureRole::BaseColor);
        cache.metallicRoughnessPath       = assetPathForRole(instance, MaterialTextureRole::MetallicRoughness);
        cache.normalPath                  = assetPathForRole(instance, MaterialTextureRole::Normal);
        cache.ambientOcclusionPath        = assetPathForRole(instance, MaterialTextureRole::AmbientOcclusion);
        cache.emissivePath                = assetPathForRole(instance, MaterialTextureRole::Emissive);
        cache.baseColorColorSpace         = textureColorSpaceForRole(MaterialTextureRole::BaseColor);
        cache.metallicRoughnessColorSpace = textureColorSpaceForRole(MaterialTextureRole::MetallicRoughness);
        cache.normalColorSpace            = textureColorSpaceForRole(MaterialTextureRole::Normal);
        cache.ambientOcclusionColorSpace  = textureColorSpaceForRole(MaterialTextureRole::AmbientOcclusion);
        cache.emissiveColorSpace          = textureColorSpaceForRole(MaterialTextureRole::Emissive);
        return cache;
    }

    bool MaterialRuntime::textureCacheMatchesRole(const MaterialTextureCacheState& current,
                                                  const MaterialTextureCacheState& desired,
                                                  MaterialTextureRole              role)
    {
        switch (role)
        {
        case MaterialTextureRole::BaseColor:
            return current.baseColorPath == desired.baseColorPath &&
                   current.baseColorColorSpace == desired.baseColorColorSpace;
        case MaterialTextureRole::MetallicRoughness:
            return current.metallicRoughnessPath == desired.metallicRoughnessPath &&
                   current.metallicRoughnessColorSpace == desired.metallicRoughnessColorSpace;
        case MaterialTextureRole::Normal:
            return current.normalPath == desired.normalPath && current.normalColorSpace == desired.normalColorSpace;
        case MaterialTextureRole::AmbientOcclusion:
            return current.ambientOcclusionPath == desired.ambientOcclusionPath &&
                   current.ambientOcclusionColorSpace == desired.ambientOcclusionColorSpace;
        case MaterialTextureRole::Emissive:
            return current.emissivePath == desired.emissivePath &&
                   current.emissiveColorSpace == desired.emissiveColorSpace;
        }
        return false;
    }

    texture_id_type MaterialRuntime::resolveTexture(MaterialTextureRole     role,
                                                    const MaterialInstance& instance,
                                                    const MaterialGpuState& currentState,
                                                    IResourceProvider*      resources) const
    {
        const MaterialTextureBinding& texture = textureBindingForRole(instance, role);
        if (texture.source == MaterialTextureSource::ResolvedTexture)
        {
            if (texture.resolvedTextureID != 0)
                return texture.resolvedTextureID;

            if (resources == nullptr)
                return textureIdForRole(currentState.textures, role);
        }

        if (resources == nullptr)
            return textureIdForRole(currentState.textures, role);

        const MaterialTextureCacheState currentCache   = makeTextureCacheState(instance);
        const texture_id_type           currentTexture = textureIdForRole(currentState.textures, role);
        if (currentTexture != 0 && textureCacheMatchesRole(currentState.textureCache, currentCache, role))
        {
            return currentTexture;
        }

        if (texture.source == MaterialTextureSource::ResolvedTexture || texture.source == MaterialTextureSource::None ||
            texture.path.empty())
        {
            return resources->requestMaterialFallbackTexture(role);
        }

        return resources->requestTexture(texture.path, textureColorSpaceForRole(role));
    }

    MaterialTextureIds MaterialRuntime::resolveTextures(const MaterialInstance& instance,
                                                        const MaterialGpuState& currentState,
                                                        IResourceProvider*      resources) const
    {
        return {resolveTexture(MaterialTextureRole::BaseColor, instance, currentState, resources),
                resolveTexture(MaterialTextureRole::MetallicRoughness, instance, currentState, resources),
                resolveTexture(MaterialTextureRole::Normal, instance, currentState, resources),
                resolveTexture(MaterialTextureRole::AmbientOcclusion, instance, currentState, resources),
                resolveTexture(MaterialTextureRole::Emissive, instance, currentState, resources)};
    }
} // namespace gts::rendering
