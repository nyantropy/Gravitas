#include "GtsModelRealizationCache.h"

GtsModelRealizationResult GtsModelRealizationCache::realize(const GtsModelHandle& model)
{
    if (auto found = entries.find(model); found != entries.end())
        return found->second;
    auto result = realizeGtsModel(model);
    if (result.succeeded())
        entries.emplace(model, result);
    return result;
}
