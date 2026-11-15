#include "SceneManager.h"

#include <cassert>
#include <stdexcept>
#include <utility>

#include "GtsScene.hpp"

SceneManager::SceneManager()  = default;
SceneManager::~SceneManager() = default;

void SceneManager::setActiveScene(std::string name)
{
    auto it = scenes.find(name);
    if (it == scenes.end())
        throw std::runtime_error("Scene not registered: " + name);

    activeScene = it->second.factory();
    if (!activeScene)
        throw std::runtime_error("Scene factory returned null: " + name);
    activeSceneName = std::move(name);
}

void SceneManager::registerScene(std::string name, SceneFactory factory, RegisteredSceneInfo info)
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

GtsScene* SceneManager::getActiveScene()
{
    return activeScene.get();
}

void SceneManager::clearActiveScene()
{
    activeScene.reset();
    activeSceneName.clear();
}

const std::string& SceneManager::getActiveSceneName() const
{
    return activeSceneName;
}

const std::vector<RegisteredSceneInfo>& SceneManager::getRegisteredScenes() const
{
    return registeredScenes;
}

bool SceneManager::hasScene(const std::string& name) const
{
    return scenes.contains(name);
}
