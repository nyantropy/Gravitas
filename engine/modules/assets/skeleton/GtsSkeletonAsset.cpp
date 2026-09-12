#include "GtsSkeletonAsset.h"

#include "GtsSkeletonCompatibility.h"

bool areGtsSkeletonsCompatible(const GtsSkeletonAsset& left, const GtsSkeletonAsset& right)
{
    const auto a = makeGtsSkeletonCompatibility(left);
    const auto b = makeGtsSkeletonCompatibility(right);
    return a.succeeded() && b.succeeded() && areGtsSkeletonCompatibilitiesEqual(*a.compatibility(), *b.compatibility());
}
