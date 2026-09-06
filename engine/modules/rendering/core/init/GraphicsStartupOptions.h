#pragma once

// we only support vulkan right now
enum class GraphicsBackend
{
    Vulkan
};

struct GraphicsStartupOptions
{
    // backend selector
    GraphicsBackend backend = GraphicsBackend::Vulkan;

    // enable validation layers in vulkan for development and debugging
    bool enableValidationLayers = true;

    // run in headless mode, meaning no window will be initialized when starting the engine with a game
    bool headless = false;

    // when enabled, the engine will allocate timestamp query pools and collect frame/pass timings
    // used for benchmark and performance tests in its current capacity
    bool enableGpuTimestamps = false;
};
