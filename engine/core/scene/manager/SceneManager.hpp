#pragma once

#include <memory>
#include <unordered_map>
#include <string>

#include "GtsScene.hpp"

// manage active scenes in the renderer, hold the current active scene, a simple management class
class SceneManager 
{
    private:
        // Ownership lives here for the lifetime of the engine.
        // Scenes are never moved out, so raw pointers into them remain valid.
        std::unordered_map<std::string, std::unique_ptr<GtsScene>> loadedScenes;

        // Non-owning pointer to the currently active scene.
        GtsScene* activeScene = nullptr;

    public:
        void setActiveScene(const std::string& name)
        {
            activeScene = loadedScenes.at(name).get();
        }

        void registerScene(const std::string& name, std::unique_ptr<GtsScene> scene)
        {
            loadedScenes[name] = std::move(scene);
        }

        GtsScene* getActiveScene()
        {
            return activeScene;
        }
};
