#pragma once
#include "GtsModelFrameData.h"
#include "model/runtime/GtsModelInstance.h"
#include "MaterialRuntime.h"

struct GtsModelExtractionResult
{
    GtsModelFrameData               frame;
    std::vector<GtsModelDiagnostic> diagnostics;
    bool                            succeeded() const
    {
        return diagnostics.empty();
    }
};

// All-or-nothing per instance. Does not evaluate animation or realize assets/materials.
GtsModelExtractionResult extractModelRenderState(std::shared_ptr<const GtsModelInstance> instance,
                                                 const glm::mat4&                        worldTransform,
                                                 gts::rendering::MaterialRuntime&        materials,
                                                 IResourceProvider*                      resources = nullptr);
