#pragma once

#include <memory>
#include <unordered_map>
#include <string>

#include "GtsScene.hpp"

// manage active scenes in the renderer, hold the current active scene, a simple management class
class SceneManager 
{
    private:
        std::unique_ptr<GtsScene> activeScene;
        std::unordered_map<std::string, std::unique_ptr<GtsScene>> loadedScenes;

    public:
        void setActiveScene(const std::string& name) 
        {
            activeScene = std::move(loadedScenes[name]);
        }

        void registerScene(const std::string& name, std::unique_ptr<GtsScene> scene) 
        {
            loadedScenes[name] = std::move(scene);
        }

        GtsScene* getActiveScene() 
        { 
            return activeScene.get(); 
        }
};
