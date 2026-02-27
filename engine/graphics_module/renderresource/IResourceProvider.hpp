#pragma once

#include <string>
#include <vector>

#include <glm.hpp>

#include "Types.h"
#include "Vertex.h"

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

        // Procedural mesh upload for CPU-generated geometry (e.g., text quads).
        // If existingId == 0 a new mesh is allocated and its ID is returned.
        // If existingId != 0 the existing GPU buffers are replaced with the new
        // geometry; the same ID is returned.  The caller (a ControllerSystem) is
        // responsible for storing the returned ID and passing it on subsequent
        // calls to update the same mesh.
        virtual mesh_id_type uploadProceduralMesh(mesh_id_type                  existingId,
                                                  const std::vector<Vertex>&    vertices,
                                                  const std::vector<uint32_t>&  indices) = 0;
};
