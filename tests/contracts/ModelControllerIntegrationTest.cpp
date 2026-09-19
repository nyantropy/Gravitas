#include "GtsModelControllerContext.h"
#include "GtsModels.h"
#include "ECSWorld.hpp"
#include "GtsModelRegistry.h"
#include "GtsModelRealizationCache.h"
#include "../assets/runtime/ScopedRuntimeAssetPolicy.h"
#include <utility>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << message << '\n';
            std::exit(1);
        }
    }
} // namespace

int main()
{
    ScopedRuntimeAssetPolicy policy("development");
    const auto               path = std::filesystem::temp_directory_path() / "gravitas-controller-model.obj";
    std::ofstream(path) << "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
    GtsModelRegistry         registry;
    GtsModelRealizationCache realizations;
    ECSWorld                 world;
    EcsControllerContext     context{world};
    bool                     missingRegistry = false;
    try
    {
        requestGtsModel(context, path);
    }
    catch (const std::logic_error&)
    {
        missingRegistry = true;
    }
    require(missingRegistry, "Missing registry must retain the existing error behavior");

    gts::model::controllerContext(context).models = &registry;
    auto model                                    = requestGtsModel(context, path);
    require(model.succeeded(), "Model facade must use the supplied registry");
    require(requestGtsModel(context, path).handle() == model.handle(), "Registry reuse must be unchanged");
    bool missingRealizations = false;
    try
    {
        modelInstances(world, context);
    }
    catch (const std::logic_error&)
    {
        missingRealizations = true;
    }
    require(missingRealizations, "Missing realization service must retain the existing error behavior");

    gts::model::controllerContext(context).modelRealizations = &realizations;
    auto& runtime                                            = modelInstances(world, context);
    require(&runtime == &modelInstances(world, context), "Model runtime must remain world-owned");
    require(runtime.create(model.handle()).succeeded(), "Headless realization must retain null-resource support");
    EcsControllerContext other{world};
    require(gts::model::controllerContext(std::as_const(other)).models == nullptr,
            "Model access must not leak to another call for the same world");
    std::filesystem::remove(path);
}
