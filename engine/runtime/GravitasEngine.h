#pragma once

#include <functional>
#include <memory>
#include <string>

#include "EngineConfig.h"
#include "GtsSceneTransitionData.h"
#include "RegisteredSceneInfo.h"

class GtsScene;
namespace gts::tools
{
    struct ToolLaunchPreset;
}

class GravitasEngine
{
    public:
    using SceneFactory = std::function<std::unique_ptr<GtsScene>()>;

    explicit GravitasEngine(EngineConfig config = EngineConfig{});
    ~GravitasEngine();
    GravitasEngine(const GravitasEngine&)            = delete;
    GravitasEngine& operator=(const GravitasEngine&) = delete;

    void registerScene(std::string name, SceneFactory factory, RegisteredSceneInfo info = {});
    bool setActiveScene(std::string name, std::unique_ptr<GtsSceneTransitionData> data = nullptr);
    void applyToolLaunchPreset(const gts::tools::ToolLaunchPreset& preset);
    void start();

    private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
