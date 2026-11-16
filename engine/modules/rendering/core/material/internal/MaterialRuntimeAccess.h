#pragma once

#include "MaterialRuntime.h"

namespace gts::rendering
{
    // Rendering/model implementation and engine fixtures only; not an authoring API.
    class MaterialRuntimeAccess
    {
        public:
        static MaterialSyncResult      synchronizeGpuState(MaterialRuntime&       runtime,
                                                           MaterialInstanceHandle requestedHandle,
                                                           IResourceProvider*     resources);
        static const MaterialGpuState* getGpuState(const MaterialRuntime& runtime, MaterialInstanceHandle handle);
        static void                    queueMaterialSync(MaterialRuntime& runtime, MaterialInstanceHandle handle);
        static std::vector<MaterialInstanceHandle> takeQueuedMaterialSyncs(MaterialRuntime& runtime);
        static size_t                              queuedMaterialSyncCount(const MaterialRuntime& runtime);
        static bool   materialNeedsGpuSync(const MaterialRuntime& runtime, MaterialInstanceHandle requestedHandle);
        static size_t gpuStateCount(const MaterialRuntime& runtime);
    };
} // namespace gts::rendering
