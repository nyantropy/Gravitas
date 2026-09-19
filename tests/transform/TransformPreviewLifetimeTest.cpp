#include <cstdlib>
#include <iostream>

#include "ECSWorld.hpp"
#include "AssetPreviewWorld.hpp"
#include "ParticlePreviewWorld.hpp"
#include "detail/TransformInvalidationState.h"
#include "../rendering/material/TestMaterialResources.h"

namespace
{
    void requireWorldCount(size_t expected)
    {
        const auto state = gts::transform::inspectTransformWorldState();
        if (state.invalidationWorlds != expected || state.publicationWorlds != expected)
        {
            std::cerr << "preview world transform state was not installed/released together" << std::endl;
            std::exit(1);
        }
    }
} // namespace

int main()
{
    Resources resources;
    requireWorldCount(0);
    {
        gts::tools::AssetPreviewWorld assets;
        assets.ensure(&resources);
        requireWorldCount(1);
        {
            gts::tools::ParticlePreviewWorld particles;
            for (int cycle = 0; cycle != 3; ++cycle)
            {
                particles.ensure(&resources);
                requireWorldCount(2);
                const auto state = gts::transform::inspectTransformWorldState(&particles.ecsWorld());
                if (state.callbacks != 1)
                    return 1;
                particles.destroy();
                requireWorldCount(1);
            }
            particles.ensure(&resources);
            requireWorldCount(2);
            // The destructor's abandon path does not call world.clear().
        }
        requireWorldCount(1);
        for (int cycle = 0; cycle != 3; ++cycle)
        {
            assets.destroy();
            requireWorldCount(0);
            assets.ensure(&resources);
            requireWorldCount(1);
        }
    }
    requireWorldCount(0);
}
