#include "MaterialRuntimeAccess.h"

namespace gts::rendering
{
    MaterialSyncResult MaterialRuntimeAccess::synchronizeGpuState(MaterialRuntime&       runtime,
                                                                  MaterialInstanceHandle requestedHandle,
                                                                  IResourceProvider*     resources)
    {
        return runtime.synchronizeGpuState(requestedHandle, resources);
    }

    const MaterialGpuState* MaterialRuntimeAccess::getGpuState(const MaterialRuntime& runtime,
                                                               MaterialInstanceHandle handle)
    {
        return runtime.getGpuState(handle);
    }

    void MaterialRuntimeAccess::queueMaterialSync(MaterialRuntime& runtime, MaterialInstanceHandle handle)
    {
        runtime.queueMaterialSync(handle);
    }

    std::vector<MaterialInstanceHandle> MaterialRuntimeAccess::takeQueuedMaterialSyncs(MaterialRuntime& runtime)
    {
        return runtime.takeQueuedMaterialSyncs();
    }

    size_t MaterialRuntimeAccess::queuedMaterialSyncCount(const MaterialRuntime& runtime)
    {
        return runtime.queuedMaterialSyncCount();
    }

    bool MaterialRuntimeAccess::materialNeedsGpuSync(const MaterialRuntime& runtime,
                                                     MaterialInstanceHandle requestedHandle)
    {
        return runtime.materialNeedsGpuSync(requestedHandle);
    }

    size_t MaterialRuntimeAccess::gpuStateCount(const MaterialRuntime& runtime)
    {
        return runtime.gpuStateCount();
    }

} // namespace gts::rendering
