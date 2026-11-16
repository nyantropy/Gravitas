#pragma once

#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "MaterialTypes.h"

class ECSWorld;
class IResourceProvider;

namespace gts::rendering
{
    class MaterialRuntimeAccess;
    class MaterialRuntime
    {
        public:
        MaterialRuntime();

        MaterialRuntime(const MaterialRuntime&)                    = delete;
        MaterialRuntime&         operator=(const MaterialRuntime&) = delete;
        std::weak_ptr<const int> lifetimeToken() const;

        // CPU presentation snapshot using the same semantic description as GPU synchronization.
        MaterialFrameState frameState(MaterialInstanceHandle handle) const;

        MaterialDefinitionHandle createDefinition(const MaterialDefinition& definition);

        bool destroyDefinition(MaterialDefinitionHandle handle);

        MaterialInstanceHandle createInstance(const MaterialInstance& instance);

        MaterialInstanceHandle cloneInstance(MaterialInstanceHandle source);

        bool destroyInstance(MaterialInstanceHandle handle);

        const MaterialDefinition* getDefinition(MaterialDefinitionHandle handle) const;

        const MaterialInstance* getInstance(MaterialInstanceHandle handle) const;

        bool isInstanceAlive(MaterialInstanceHandle handle) const;

        bool setInstance(MaterialInstanceHandle handle, const MaterialInstance& next);

        bool modifyInstance(MaterialInstanceHandle handle, const std::function<bool(MaterialInstance&)>& modify);

        MaterialInstanceHandle defaultMaterial() const;

        MaterialInstanceHandle vertexColorOnlyMaterial() const;

        MaterialInstanceHandle defaultStandardSurfaceMaterial() const;

        MaterialInstanceHandle resolveInstanceHandle(MaterialInstanceHandle handle) const;

        private:
        friend class MaterialRuntimeAccess;

        MaterialSyncResult synchronizeGpuState(MaterialInstanceHandle requestedHandle, IResourceProvider* resources);

        const MaterialGpuState* getGpuState(MaterialInstanceHandle handle) const;

        void queueMaterialSync(MaterialInstanceHandle handle);

        std::vector<MaterialInstanceHandle> takeQueuedMaterialSyncs();

        size_t queuedMaterialSyncCount() const;

        bool materialNeedsGpuSync(MaterialInstanceHandle requestedHandle) const;

        size_t gpuStateCount() const;

        private:
        std::shared_ptr<const int> lifetime = std::make_shared<const int>(0);
        MaterialGpuState describeInstance(MaterialInstanceHandle handle, const MaterialTextureIds& textureIDs) const;

        struct DefinitionSlot
        {
            MaterialDefinition definition;
            uint32_t           generation = 1;
            bool               alive      = false;
            bool               builtIn    = false;
        };

        struct InstanceSlot
        {
            MaterialInstance instance;
            uint32_t         generation = 1;
            bool             alive      = false;
            bool             builtIn    = false;
        };

        std::vector<DefinitionSlot>                                  definitions;
        std::vector<InstanceSlot>                                    instances;
        std::unordered_map<MaterialInstanceHandle, MaterialGpuState> gpuStates;
        std::vector<MaterialInstanceHandle>                          queuedMaterials;
        std::unordered_set<MaterialInstanceHandle>                   queuedMaterialSet;
        MaterialDefinitionHandle                                     defaultDefinitionHandle;
        MaterialDefinitionHandle                                     standardSurfaceDefinitionHandle;
        MaterialInstanceHandle                                       defaultMaterialHandle;
        MaterialInstanceHandle                                       vertexColorOnlyMaterialHandle;
        MaterialInstanceHandle                                       defaultStandardSurfaceMaterialHandle;
        uint32_t                                                     nextGpuHandle = 1;

        void initializeBuiltIns();

        MaterialInstanceHandle createInstanceInternal(MaterialInstance instance, bool builtIn);

        static void sanitizeMaterialInstance(MaterialInstance& instance);

        DefinitionSlot* findDefinitionSlot(MaterialDefinitionHandle handle);

        const DefinitionSlot* findDefinitionSlot(MaterialDefinitionHandle handle) const;

        InstanceSlot* findInstanceSlot(MaterialInstanceHandle handle);

        const InstanceSlot* findInstanceSlot(MaterialInstanceHandle handle) const;

        static const MaterialTextureBinding& textureBindingForRole(const MaterialInstance& instance,
                                                                   MaterialTextureRole     role);

        static texture_id_type textureIdForRole(const MaterialTextureIds& textures, MaterialTextureRole role);

        static std::string assetPathForRole(const MaterialInstance& instance, MaterialTextureRole role);

        static MaterialTextureCacheState makeTextureCacheState(const MaterialInstance& instance);

        static bool textureCacheMatchesRole(const MaterialTextureCacheState& current,
                                            const MaterialTextureCacheState& desired,
                                            MaterialTextureRole              role);

        texture_id_type resolveTexture(MaterialTextureRole     role,
                                       const MaterialInstance& instance,
                                       const MaterialGpuState& currentState,
                                       IResourceProvider*      resources) const;

        MaterialTextureIds resolveTextures(const MaterialInstance& instance,
                                           const MaterialGpuState& currentState,
                                           IResourceProvider*      resources) const;
    };

    MaterialRuntime& materialRuntime(ECSWorld& world);
} // namespace gts::rendering
