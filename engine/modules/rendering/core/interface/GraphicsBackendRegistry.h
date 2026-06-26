#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "IGtsGraphicsModule.hpp"

namespace gts::rendering
{
    inline const char* graphicsBackendName(GraphicsBackend backend)
    {
        switch (backend)
        {
            case GraphicsBackend::Vulkan: return "Vulkan";
        }

        return "Unknown";
    }

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
        void registerProvider(const IGraphicsBackendProvider& provider)
        {
            for (const IGraphicsBackendProvider*& registeredProvider : providers)
            {
                if (registeredProvider->backend() == provider.backend())
                {
                    registeredProvider = &provider;
                    return;
                }
            }

            providers.push_back(&provider);
        }

        const IGraphicsBackendProvider* find(GraphicsBackend backend) const
        {
            for (const IGraphicsBackendProvider* provider : providers)
            {
                if (provider->backend() == backend)
                    return provider;
            }

            return nullptr;
        }

        std::unique_ptr<IGtsGraphicsModule> create(const GraphicsConfig& config) const
        {
            const IGraphicsBackendProvider* provider = find(config.backend);
            if (provider == nullptr)
            {
                throw std::runtime_error(
                    std::string("No graphics backend provider registered for ")
                    + graphicsBackendName(config.backend));
            }

            std::unique_ptr<IGtsGraphicsModule> graphics = provider->create(config);
            if (!graphics)
            {
                throw std::runtime_error(
                    std::string("Graphics backend provider failed to create ")
                    + graphicsBackendName(config.backend));
            }

            return graphics;
        }

        bool hasProvider(GraphicsBackend backend) const
        {
            return find(backend) != nullptr;
        }

    private:
        std::vector<const IGraphicsBackendProvider*> providers;
    };
}
