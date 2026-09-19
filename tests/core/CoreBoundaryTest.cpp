#include "Entity.h"
#include "GtsScene.hpp"
#include "EnginePaths.h"
#include "GlmConfig.h"
#include "Tween.h"

#include <type_traits>

#if __has_include("UiSurface.h") || __has_include("IGtsPhysicsModule.h") || __has_include("ResourceTypes.h") || \
    __has_include("GtsFrameStats.h") || __has_include("ProfileAccumulator.h")
#error "Core must not publish feature contracts or implementation"
#endif

static_assert(std::is_same_v<entity_id_type, uint32_t>);
static_assert(sizeof(Entity) == sizeof(uint32_t));

int main() { return Entity{1} == Entity{1} ? 0 : 1; }
