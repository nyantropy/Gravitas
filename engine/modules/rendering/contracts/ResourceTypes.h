#pragma once

#include <cstdint>

// Rendering resource handles; independent of resource realization and GPU backends.
using mesh_id_type     = uint32_t;
using texture_id_type  = uint32_t;
using font_id_type     = uint32_t;
using view_id_type     = uint32_t; // handle for a camera / render-view buffer
using ssbo_id_type     = uint32_t; // handle for an object slot in the shared SSBO
