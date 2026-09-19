#pragma once
#include "model/public/GtsModelRequest.h"
#include "model/public/GtsModelRequestResult.h"
#include "model/public/GtsModelClipReference.h"
#include "model/world/GtsModelInstanceRuntime.h"
#include "model/runtime/ModelInstanceComponent.h"
#include <string_view>
struct EcsControllerContext;

// Application boundary. Engine services retain definitions/realizations; entities own instances.
GtsModelRequestResult    requestGtsModel(const EcsControllerContext& context, const GtsModelRequest& request);
GtsModelRequestResult    requestGtsModel(const EcsControllerContext& context, const std::filesystem::path& path);
GtsModelInstanceRuntime& modelInstances(ECSWorld& world, const EcsControllerContext& context);
std::size_t              gtsModelSkeletonUseCount(const GtsModelHandle& model);
GtsModelClipLookupResult
findGtsModelClip(const GtsModelHandle& model, std::string_view name, std::optional<uint32_t> skeletonUse = {});
