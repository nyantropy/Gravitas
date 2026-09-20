#pragma once

#include "BenchmarkRunner.h"
#include "ECSWorld.hpp"

namespace gts::performance
{
    void                      installTransformWorkload(ECSWorld& world, bool moving, bool animated);
    Entity                    addTransformEntity(ECSWorld& world, uint32_t index, uint32_t seed, bool animated);
    Sample                    measureWorld(ECSWorld&         world,
                                           const Parameters& parameters,
                                           uint32_t          tick,
                                           bool              animated,
                                           bool              extractModels,
                                           const Counters&   fixedCounters);
    std::unique_ptr<Workload> makeTransformWorkload(const std::string& name, const Parameters& parameters);
} // namespace gts::performance
