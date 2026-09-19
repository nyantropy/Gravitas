#pragma once

class ECSWorld;
class GtsScene;

namespace gts::transform
{
    void resetTransformSceneFeature(ECSWorld& world);
    void installTransformRuntime(ECSWorld& world);
    void installTransformResolver(ECSWorld& world);
    void installTransformFeature(ECSWorld& world);
    void installTransformRuntime(GtsScene& scene);
    void installTransformResolver(GtsScene& scene);
    void installTransformFeature(GtsScene& scene);
} // namespace gts::transform
