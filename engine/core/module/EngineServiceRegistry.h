#pragma once

#include <typeindex>
#include <unordered_map>

class EngineServiceRegistry
{
public:
    template<typename Service>
    void registerService(Service& service)
    {
        services[std::type_index(typeid(Service))] = &service;
    }

    template<typename Service>
    Service* getService()
    {
        auto it = services.find(std::type_index(typeid(Service)));
        if (it == services.end())
            return nullptr;

        return static_cast<Service*>(it->second);
    }

    template<typename Service>
    const Service* getService() const
    {
        auto it = services.find(std::type_index(typeid(Service)));
        if (it == services.end())
            return nullptr;

        return static_cast<const Service*>(it->second);
    }

    template<typename Service>
    void unregisterService()
    {
        services.erase(std::type_index(typeid(Service)));
    }

    void clear()
    {
        services.clear();
    }

private:
    std::unordered_map<std::type_index, void*> services;
};
