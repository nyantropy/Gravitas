#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "RegisteredSceneInfo.h"

class GtsScene;

// Own the active scene and coordinate scene transitions and lifecycle.
class SceneManager
{
    public:
    using SceneFactory = std::function<std::unique_ptr<GtsScene>()>;

    SceneManager();
    ~SceneManager();
    void               setActiveScene(std::string name);
    void               registerScene(std::string name, SceneFactory factory, RegisteredSceneInfo info = {});
    GtsScene*          getActiveScene();
    void               clearActiveScene();
    const std::string& getActiveSceneName() const;
    const std::vector<RegisteredSceneInfo>& getRegisteredScenes() const;
    bool                                    hasScene(const std::string& name) const;

    private:
    struct RegisteredScene
    {
        RegisteredSceneInfo info;
        SceneFactory        factory;
    };
    std::unordered_map<std::string, RegisteredScene> scenes;
    std::vector<RegisteredSceneInfo>                 registeredScenes;
    std::unique_ptr<GtsScene>                        activeScene;
    std::string                                      activeSceneName;
};
