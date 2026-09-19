#include "GraphicsBackendInstaller.h"

#include "VulkanBackendInstaller.h"

namespace gts::rendering
{
    void installDefaultGraphicsBackends(GraphicsBackendRegistry& registry)
    {
        installVulkanGraphicsBackend(registry);
    }
}
