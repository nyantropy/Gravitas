#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "IGtsGraphicsModule.hpp"

namespace gts::rendering
{
    // get the name of the backend, we only have vulkan right now
    inline const char* graphicsBackendName(GraphicsBackend backend)
    {
        switch (backend)
        {
            case GraphicsBackend::Vulkan: return "Vulkan";
        }

        return "Unknown";
    }

    // factory for one backend type; the provider is not a running graphics backend
    // instead it just provides functionality for creation
    class IGraphicsBackendProvider
    {
    public:
        virtual ~IGraphicsBackendProvider() = default;

        virtual GraphicsBackend backend() const = 0;
        virtual std::unique_ptr<IGtsGraphicsModule> create(const GraphicsConfig& config) const = 0;
    };

    class GraphicsBackendRegistry
    {
    public:
        // store the provider for later creation
        void registerProvider(const IGraphicsBackendProvider& provider)
        {
            for (const IGraphicsBackendProvider*& registeredProvider : providers)
            {
                if (registeredProvider->backend() == provider.backend())
                {
                    // Replace the factory for future create() calls, not an existing backend.
                    registeredProvider = &provider;
                    return;
                }
            }

            providers.push_back(&provider);
        }

        // look up a factory without constructing anything; nullptr means unavailable
        const IGraphicsBackendProvider* find(GraphicsBackend backend) const
        {
            for (const IGraphicsBackendProvider* provider : providers)
            {
                if (provider->backend() == backend)
                    return provider;
            }

            return nullptr;
        }

        // construct the selected backend and transfer ownership to the caller (GtsPlatform)
        std::unique_ptr<IGtsGraphicsModule> create(const GraphicsConfig& config) const
        {
            const IGraphicsBackendProvider* provider = find(config.startup.backend);
            if (provider == nullptr)
            {
                throw std::runtime_error(
                    std::string("No graphics backend provider registered for ")
                    + graphicsBackendName(config.startup.backend));
            }

            std::unique_ptr<IGtsGraphicsModule> graphics = provider->create(config);
            if (!graphics)
            {
                throw std::runtime_error(
                    std::string("Graphics backend provider failed to create ")
                    + graphicsBackendName(config.startup.backend));
            }

            return graphics;
        }

        bool hasProvider(GraphicsBackend backend) const
        {
            return find(backend) != nullptr;
        }

    private:
        // borrowed pointers: registered providers must outlive their use by this registry
        std::vector<const IGraphicsBackendProvider*> providers;
    };
}
