#include "Entity.h"
#include "GtsScene.hpp"
#include "EnginePaths.h"
#include "GlmConfig.h"
#include "Tween.h"

#include <type_traits>

#if __has_include("UiSurface.h") || __has_include("IGtsPhysicsModule.h") || __has_include("ResourceTypes.h") || \
    __has_include("GtsFrameStats.h") || __has_include("ProfileAccumulator.h") || \
    __has_include("ScenePhysics.h") || __has_include("ISceneFrameStats.h") || \
    __has_include("BuiltinExecutionGroups.h") || __has_include("SceneExecutionProfile.h") || \
    __has_include("VNExecutionProfiles.h") || __has_include("GravitasEngine.hpp") || \
    __has_include("EngineConfig.h") || __has_include("GtsGameLoop.h") || \
    __has_include("GtsPlatform.h") || __has_include("GtsFrameEndedEvent.h")
#error "Core must not publish feature contracts or implementation"
#endif

static_assert(std::is_same_v<entity_id_type, uint32_t>);
static_assert(sizeof(Entity) == sizeof(uint32_t));

int main() { return Entity{1} == Entity{1} ? 0 : 1; }
