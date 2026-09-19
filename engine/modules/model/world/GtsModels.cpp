#include "GtsModels.h"
#include "RenderingControllerContext.h"
#include "GtsModelControllerContext.h"
#include "GtsModelRegistry.h"
#include "GtsModelResource.h"
#include "GtsModelSkin.h"
#include "EcsControllerContext.hpp"
#include <stdexcept>

GtsModelRequestResult requestGtsModel(const EcsControllerContext& context, const GtsModelRequest& request)
{
    if (!gts::model::controllerContext(context).models)
        throw std::logic_error("Model requests require the engine model service");
    return gts::model::controllerContext(context).models->requestModel(request);
}
GtsModelRequestResult requestGtsModel(const EcsControllerContext& context, const std::filesystem::path& path)
{
    return requestGtsModel(context, GtsModelRequest{path});
}
GtsModelInstanceRuntime& modelInstances(ECSWorld& world, const EcsControllerContext& context)
{
    if (!gts::model::controllerContext(context).modelRealizations)
        throw std::logic_error("Model instances require the engine realization service");
    return modelInstances(world, *gts::model::controllerContext(context).modelRealizations, gts::rendering::controllerContext(context).resources);
}
std::size_t gtsModelSkeletonUseCount(const GtsModelHandle& model)
{
    return model ? model->skeletonUses().size() : 0;
}
GtsModelClipLookupResult
findGtsModelClip(const GtsModelHandle& model, std::string_view name, std::optional<uint32_t> skeletonUse)
{
    if (!model)
        return {{},
                {{GtsModelDiagnosticSeverity::Error, "model.clip.resource", "Clip lookup requires a model", "model"}}};
    return model->findClip(name, skeletonUse);
}
