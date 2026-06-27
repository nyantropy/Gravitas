#pragma once

#include <cstdint>
#include <vector>

#include "ParticleFrameData.h"
#include "RenderCommand.h"
#include "RenderViewport.h"
#include "Types.h"

struct EditorPreviewRenderData
{
    bool enabled = false;
    uint32_t width = 1;
    uint32_t height = 1;
    texture_id_type colorTextureID = 0;
    RenderViewportRect viewport = RenderViewportRect::full(1, 1);
    std::vector<RenderCommand> renderList;
    std::vector<ObjectUploadCommand> objectUploads;
    std::vector<CameraUploadCommand> cameraUploads;
    ParticleFrameData particleData;

    bool valid() const
    {
        return enabled && width > 0 && height > 0 && colorTextureID != 0;
    }
};

struct EditorPreviewRenderComponent
{
    EditorPreviewRenderData data;
};
