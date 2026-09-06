#include <limits>
#include <stdexcept>

#include "GraphicsConfig.h"

void require(bool condition)
{
    if (!condition)
        throw std::runtime_error("Graphics settings invariant failed");
}

int main()
{
    GraphicsSettings settings;
    require(validateGraphicsSettings(settings).empty());
    auto& resolution = settings.rendering.resolution;
    require(resolveRenderExtent(resolution, {2560, 1440}) == Extent2D{2560, 1440});
    resolution.mode = RenderResolutionMode::Fixed;
    resolution.fixedExtent = {1920, 1080};
    for (auto mode : {WindowMode::Windowed, WindowMode::BorderlessFullscreen, WindowMode::Fullscreen})
    {
        settings.window.windowMode = mode;
        require(resolveRenderExtent(resolution, {2560, 1440}) == Extent2D{1920, 1080});
        require(validateGraphicsSettings(settings).empty());
    }
    resolution.mode = RenderResolutionMode::ScaleOutput;
    resolution.scale = 0.5f;
    require(resolveRenderExtent(resolution, {2560, 1440}) == Extent2D{1280, 720});
    require(resolveRenderExtent(resolution, {1, 1}) == Extent2D{1, 1});
    for (float scale : {0.0f, -1.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        resolution.scale = scale;
        require(!validateGraphicsSettings(settings).empty());
    }
    resolution.scale = std::numeric_limits<float>::max();
    require(!resolveRenderExtent(resolution, {2560, 1440}));
    resolution.mode = static_cast<RenderResolutionMode>(99);
    require(!validateGraphicsSettings(settings).empty());
    settings = {};
    settings.window.width = 0;
    require(!validateGraphicsSettings(settings).empty());
    settings = {};
    settings.presentation.mode = static_cast<PresentModePreference>(99);
    require(!validateGraphicsSettings(settings).empty());
    settings = {};
    settings.framePacing.maxFrameRate = -1;
    require(!validateGraphicsSettings(settings).empty());
}
