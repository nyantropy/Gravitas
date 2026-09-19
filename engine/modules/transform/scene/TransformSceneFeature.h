#pragma once

#include "EcsExecutionSelection.h"

class ECSWorld;
class GtsScene;

namespace gts::transform
{
    void resetTransformSceneFeature(ECSWorld& world);

    // Installs lifetime ownership, component callbacks and initial dirty tracking;
    // does not schedule resolution. Suitable for explicit/physics resolution.
    void installTransformRuntime(ECSWorld& world, const EcsExecutionSelection& defaultSelection);

    // Appends one resolver controller in the supplied group here, after writers and before consumers.
    // Low-level world installers are called once per installation; scene overloads
    // retain their per-feature guards. No systems are reordered or deduplicated.
    void installTransformResolver(ECSWorld&                    world,
                                  const EcsExecutionSelection& defaultSelection,
                                  EcsSystemGroup               resolverGroup);

    // Runtime plus one resolver at the caller's registration position.
    void installTransformFeature(ECSWorld&                    world,
                                 const EcsExecutionSelection& defaultSelection,
                                 EcsSystemGroup               resolverGroup);
    void installTransformRuntime(GtsScene& scene, const EcsExecutionSelection& defaultSelection);
    void installTransformResolver(GtsScene&                    scene,
                                  const EcsExecutionSelection& defaultSelection,
                                  EcsSystemGroup               resolverGroup);
    void installTransformFeature(GtsScene&                    scene,
                                 const EcsExecutionSelection& defaultSelection,
                                 EcsSystemGroup               resolverGroup);
} // namespace gts::transform
