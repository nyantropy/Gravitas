#pragma once

#include <string>

#include "Types.h"

class IResourceProvider
{
    public:
        virtual mesh_id_type    requestMesh(const std::string& path) = 0;
        virtual texture_id_type requestTexture(const std::string& path) = 0;
        virtual uniform_id_type requestUniformBuffer(size_t size) = 0;

        // Object SSBO slot allocation.
        // requestObjectSlot returns a stable index valid for the lifetime of the object.
        // releaseObjectSlot returns the index to the free-list for future reuse.
        virtual uint32_t requestObjectSlot() = 0;
        virtual void     releaseObjectSlot(uint32_t slot) = 0;
};
