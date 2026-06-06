#pragma once

#include <cassert>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "GtsScene.hpp"
#include "RegisteredSceneInfo.h"

// manage active scenes in the renderer, hold the current active scene, a simple management class
class SceneManager
{
    public:
    using SceneFactory = std::function<std::unique_ptr<GtsScene>()>;

    private:
    struct RegisteredScene
    {
        RegisteredSceneInfo info;
        SceneFactory        factory;
    };

    std::unordered_map<std::string, RegisteredScene> scenes;
    std::vector<RegisteredSceneInfo>                 registeredScenes;

    std::unique_ptr<GtsScene> activeScene;
    std::string               activeSceneName;

    public:
    void setActiveScene(std::string name)
    {
        auto it = scenes.find(name);
        if (it == scenes.end())
            throw std::runtime_error("Scene not registered: " + name);

        activeScene = it->second.factory();
        assert(activeScene != nullptr && "Scene factory returned null");
        activeSceneName = std::move(name);
    }

    void registerScene(std::string name, SceneFactory factory, RegisteredSceneInfo info = {})
    {
        assert(!name.empty() && "Scene name cannot be empty");
        assert(static_cast<bool>(factory) && "Scene factory cannot be empty");

        info.id = name;
        if (info.displayName.empty())
            info.displayName = name;

        const bool isNew = scenes.find(name) == scenes.end();
        scenes[info.id]  = RegisteredScene{info, std::move(factory)};
        if (isNew)
        {
            registeredScenes.push_back(info);
            return;
        }

        for (RegisteredSceneInfo& registeredScene : registeredScenes)
        {
            if (registeredScene.id == info.id)
            {
                registeredScene = info;
                break;
            }
        }
    }

    GtsScene* getActiveScene()
    {
        return activeScene.get();
    }

    void clearActiveScene()
    {
        activeScene.reset();
        activeSceneName.clear();
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
