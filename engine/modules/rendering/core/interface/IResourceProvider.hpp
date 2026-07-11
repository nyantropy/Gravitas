#pragma once

#include <string>
#include <vector>

#include "GlmConfig.h"

#include "Types.h"
#include "TextureColorSpace.h"
#include "Vertex.h"

struct BitmapFont;

struct TextureDimensions
{
    int width = 0;
    int height = 0;

    bool isValid() const
    {
        return width > 0 && height > 0;
    }
};

class IResourceProvider
{
    public:
        // request a mesh id from the resource provider
        virtual mesh_id_type requestMesh(const std::string& path) = 0;

        // Returns a shared mesh ID for an axis-aligned quad of the given dimensions.
        // Identical (width, height) → same mesh ID, enabling batched instanced drawing.
        // The returned ID must NOT be passed to releaseProceduralMesh.
        virtual mesh_id_type getSharedQuadMesh(float w, float h) = 0;

        // Runtime mesh upload for CPU-generated geometry (e.g., text quads).
        // If existingId == 0 a new mesh is allocated and its ID is returned.
        // If existingId != 0 the existing GPU buffers are replaced with the new
        // geometry; the same ID is returned.  The caller (a ControllerSystem) is
        // responsible for storing the returned ID and passing it on subsequent
        // calls to update the same mesh.
        virtual mesh_id_type uploadProceduralMesh(mesh_id_type                  existingId,
                                                  const std::vector<Vertex>&    vertices,
                                                  const std::vector<uint32_t>&  indices,
                                                  VertexAttributeFlags sourceAttributes =
                                                      LegacyUnlitVertexAttributes) = 0;
        virtual void releaseProceduralMesh(mesh_id_type id) = 0;

        // request a texture from the resource provider, no nearest neighbor
        virtual texture_id_type requestTexture(const std::string& path) = 0;
        virtual texture_id_type requestTexture(const std::string& path, TextureColorSpace)
        {
            return requestTexture(path);
        }
        // Like requestTexture but clamps UVs to the texture edge while preserving linear filtering.
        virtual texture_id_type requestClampedTexture(const std::string& path) = 0;
        // Like requestTexture but forces NEAREST-neighbor sampling and no anisotropy.
        // Use for pixel-art or bitmap-font atlases that must render crisp.
        virtual texture_id_type requestPixelTexture(const std::string& path) = 0;
        virtual TextureDimensions getTextureDimensions(texture_id_type id) const = 0;

        // request a bitmap font asset; the font manager owns metadata and uses
        // the texture manager for the atlas texture
        virtual font_id_type requestFont(const std::string& path) = 0;
        virtual const BitmapFont* getFont(font_id_type id) const = 0;

        // Camera / render-view buffer allocation.
        // Each call creates framesInFlight UBOs + descriptor sets for one render view.
        virtual view_id_type requestCameraBuffer() = 0;
        virtual void         releaseCameraBuffer(view_id_type id) = 0;

        // Legacy all-frame camera upload. Frame-loop rendering should prefer
        // renderer-owned per-frame uploads after the matching frame fence.
        virtual void uploadCameraView(view_id_type id, const glm::mat4& view, const glm::mat4& proj) = 0;

        // Object SSBO slot allocation.
        // requestObjectSlot returns a stable index valid for the lifetime of the object.
        // releaseObjectSlot returns the index to the free-list for future reuse.
        virtual ssbo_id_type requestObjectSlot() = 0;
        virtual void         releaseObjectSlot(ssbo_id_type slot) = 0;
};
