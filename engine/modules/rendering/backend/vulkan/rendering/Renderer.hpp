#pragma once
// idea: have an abstract renderer class, so we can implement additional Renderers in the future
// for now we will focus on the simplest rendering technique: forward rendering
#include <string>
#include <vector>
#include "RendererConfig.h"
#include "RenderCommand.h"
#include "RenderViewport.h"
#include "EditorPreviewRenderData.h"
#include "ParticleFrameData.h"
#include <UiCommand.h>
#include "GtsFrameStats.h"

class Renderer
{
    protected:
        RendererConfig config;
        explicit Renderer(const RendererConfig& config): config(config) {};
        virtual void createResources() = 0;
    public:
        virtual ~Renderer() = default;
        virtual void renderFrame(float dt, const std::vector<RenderCommand>& renderList,
                                 const std::vector<ObjectUploadCommand>& objectUploads,
                                 const std::vector<CameraUploadCommand>& cameraUploads,
                                 const ParticleFrameData& particleData,
                                 const RenderViewportRect& sceneViewport,
                                 const UiCommandBuffer& uiBuffer,
                                 const EditorPreviewRenderData& editorPreview,
                                 const GtsFrameStats& stats) = 0;
        virtual texture_id_type ensureEditorPreviewTarget(uint32_t width, uint32_t height) = 0;
        virtual void releaseEditorPreviewTarget() = 0;
        virtual void toggleDebugOverlay() = 0;
        virtual void requestScreenshot(const std::string& outputDirectory = {}) = 0;
};
