#include "RenderingControllerContext.h"
#include "ECSWorld.hpp"
#include <utility>

#if __has_include("UiSurface.h") || __has_include("IResourceProvider.hpp") || __has_include("PhysicsWorld.h")
#error "Rendering call data must not require optional implementations"
#endif

class ViewportReader : public ECSControllerSystem
{
    public:
    explicit ViewportReader(float& width) : width(width) {}
    void update(const EcsControllerContext& ctx) override
    {
        width = gts::rendering::controllerContext(ctx).sceneViewportPixelWidth;
    }

    private:
    float& width;
};

int main()
{
    ECSWorld world;
    float    width = 0;
    world.addControllerSystem<ViewportReader>(static_cast<EcsSystemGroup>(1ull), width);
    EcsControllerContext tools{world};
    const auto&          defaults = gts::rendering::controllerContext(std::as_const(tools));
    if (defaults.resources || defaults.windowAspectRatio != 1 || defaults.windowPixelWidth != 1 ||
        defaults.windowPixelHeight != 1 || defaults.sceneViewportPixelX != 0 || defaults.sceneViewportPixelY != 0 ||
        defaults.sceneViewportPixelWidth != 1 || defaults.sceneViewportPixelHeight != 1 ||
        defaults.sceneViewportAspectRatio != 1)
        return 1;
    gts::rendering::controllerContext(tools).sceneViewportPixelWidth = 1280;
    EcsControllerContext scene                                       = tools;
    gts::rendering::controllerContext(scene).sceneViewportPixelWidth = 960;
    world.updateControllers(scene);
    if (width != 960)
        return 1;
    world.updateControllers(tools);
    if (width != 1280)
        return 1;
    world.updateControllers(EcsControllerContext{world});
    return width == 1 ? 0 : 1;
}
