#include "ResourceTypes.h"

#include <type_traits>

#if __has_include("IResourceProvider.hpp") || __has_include("UiSurface.h") || __has_include("ECSWorld.hpp")
#error "Resource identities must be usable independently of runtime implementations"
#endif

static_assert(std::is_same_v<mesh_id_type, uint32_t>);
static_assert(std::is_same_v<texture_id_type, uint32_t>);
static_assert(std::is_same_v<font_id_type, uint32_t>);
static_assert(std::is_same_v<view_id_type, uint32_t>);
static_assert(std::is_same_v<ssbo_id_type, uint32_t>);

int main() { return 0; }
