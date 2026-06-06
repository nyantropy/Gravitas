#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "GtsScene.hpp"
#include "RegisteredSceneInfo.h"

// manage active scenes in the renderer, hold the current active scene, a simple management class
class SceneManager
{
    private:
        // Ownership lives here for the lifetime of the engine.
        // Scenes are never moved out, so raw pointers into them remain valid.
        std::unordered_map<std::string, std::unique_ptr<GtsScene>> loadedScenes;
        std::vector<RegisteredSceneInfo> registeredScenes;

        // Non-owning pointer to the currently active scene.
        GtsScene* activeScene = nullptr;
        std::string activeSceneName;

    public:
        void setActiveScene(const std::string& name)
        {
            activeScene = loadedScenes.at(name).get();
            activeSceneName = name;
        }

        void registerScene(const std::string& name, std::unique_ptr<GtsScene> scene)
        {
            if (loadedScenes.find(name) == loadedScenes.end())
                registeredScenes.push_back({name, name, false});
            loadedScenes[name] = std::move(scene);
        }

        GtsScene* getActiveScene()
        {
            return activeScene;
        }

        const std::string& getActiveSceneName() const
        {
            return activeSceneName;
        }

        const std::vector<RegisteredSceneInfo>& getRegisteredScenes() const
        {
            return registeredScenes;
        }
};
