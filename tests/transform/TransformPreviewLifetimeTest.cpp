#include "SceneExecutionPolicy.h"
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
        gts::tools::AssetPreviewWorld assets(gts::execution::rendererExecutionInputs());
        assets.ensure(&resources);
        requireWorldCount(1);
        {
            gts::tools::ParticlePreviewWorld particles(gts::execution::rendererExecutionInputs());
            for (int cycle = 0; cycle != 3; ++cycle)
            {
                particles.ensure(&resources);
                if (particles.ecsWorld().getCurrentExecutionSelection().id != "gameplay" ||
                    particles.ecsWorld().getCurrentExecutionSelection().enabledSystems != 0xfff)
                    return 1;
                requireWorldCount(2);
                const auto state = gts::transform::inspectTransformWorldState(&particles.ecsWorld());
                if (state.callbacks != 1)
                    return 1;
                particles.destroy();
                requireWorldCount(1);
            }
            particles.ensure(&resources);
                if (particles.ecsWorld().getCurrentExecutionSelection().id != "gameplay" ||
                    particles.ecsWorld().getCurrentExecutionSelection().enabledSystems != 0xfff)
                    return 1;
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
    {
        const auto                              group = static_cast<EcsSystemGroup>(1ull << 49);
        gts::rendering::RendererExecutionInputs inputs{{"preview-custom", toMask(group)}, group, group, group, group};
        gts::tools::ParticlePreviewWorld        preview(inputs);
        inputs.defaultSelection.id = "changed-after-construction";
        preview.ensure(nullptr);
        if (preview.ecsWorld().hasConfiguredDefaultExecutionSelection())
            return 1;
        for (int cycle = 0; cycle != 3; ++cycle)
        {
            preview.ensure(&resources);
            if (preview.ecsWorld().getCurrentExecutionSelection().id != "preview-custom")
                return 1;
            EcsControllerContext context{preview.ecsWorld()};
            gts::rendering::controllerContext(context).resources = &resources;
            preview.ecsWorld().updateControllers(context);
            const auto& timings = preview.ecsWorld().getLastControllerTimingSamples();
            if (timings.size() != 16)
                return 1;
            for (const auto& timing : timings)
                if (timing.group != group || timing.instanceIndex != 0)
                    return 1;
            preview.destroy();
        }
    }
    requireWorldCount(0);
}
