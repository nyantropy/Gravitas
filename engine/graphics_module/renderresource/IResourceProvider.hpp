#pragma once

#include <string>

#include <glm.hpp>

#include "Types.h"

class IResourceProvider
{
    public:
        virtual mesh_id_type    requestMesh(const std::string& path) = 0;
        virtual texture_id_type requestTexture(const std::string& path) = 0;

        // Camera / render-view buffer allocation.
        // Each call creates framesInFlight UBOs + descriptor sets for one render view.
        virtual view_id_type requestCameraBuffer() = 0;
        virtual void         releaseCameraBuffer(view_id_type id) = 0;

        // Upload view/proj matrices to all frames-in-flight slots for the given view.
        // Called by CameraBindingSystem when dirty = true; writes to persistently-mapped
        // UBO memory so the GPU reads current matrices for whichever frame it renders next.
        virtual void uploadCameraView(view_id_type id, const glm::mat4& view, const glm::mat4& proj) = 0;

        // Object SSBO slot allocation.
        // requestObjectSlot returns a stable index valid for the lifetime of the object.
        // releaseObjectSlot returns the index to the free-list for future reuse.
        virtual ssbo_id_type requestObjectSlot() = 0;
        virtual void         releaseObjectSlot(ssbo_id_type slot) = 0;
};
