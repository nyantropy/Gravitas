#pragma once

class EngineServiceRegistry;
class GtsScene;
struct EcsControllerContext;

class IEngineModule
{
public:
    virtual ~IEngineModule() = default;

    virtual const char* name() const = 0;

    virtual void registerServices(EngineServiceRegistry&) {}
    virtual void unregisterServices(EngineServiceRegistry&) {}

    virtual void beforeSceneLoad(GtsScene&, EcsControllerContext&) {}
    virtual void afterSceneLoad(GtsScene&, EcsControllerContext&) {}
    virtual void beforeSceneUnload(GtsScene&, EcsControllerContext&) {}
    virtual void afterSceneUnload(EngineServiceRegistry&) {}
};
