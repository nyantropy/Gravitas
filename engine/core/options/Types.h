#pragma once
#include <cstdint>

// global type aliases
using entity_id_type   = uint32_t;
using mesh_id_type     = uint32_t;
using texture_id_type  = uint32_t;
using uniform_id_type  = uint32_t; // kept for backwards compatibility
using view_id_type     = uint32_t; // handle for a camera / render-view buffer
using ssbo_id_type     = uint32_t; // handle for an object slot in the shared SSBO
