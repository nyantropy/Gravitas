#pragma once

class ECSWorld;
class GtsScene;

namespace gts::transform
{
    void resetTransformSceneFeature(ECSWorld& world);

    // Installs lifetime ownership, component callbacks and initial dirty tracking;
    // does not schedule resolution. Suitable for explicit/physics resolution.
    void installTransformRuntime(ECSWorld& world);

    // Appends one RenderPrep controller here, after writers and before consumers.
    // Low-level world installers are called once per installation; scene overloads
    // retain their per-feature guards. No systems are reordered or deduplicated.
    void installTransformResolver(ECSWorld& world);

    // Runtime plus one resolver at the caller's registration position.
    void installTransformFeature(ECSWorld& world);
    void installTransformRuntime(GtsScene& scene);
    void installTransformResolver(GtsScene& scene);
    void installTransformFeature(GtsScene& scene);
} // namespace gts::transform
