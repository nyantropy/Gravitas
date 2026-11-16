#pragma once

#include <unordered_map>
#include "MaterialRuntime.h"

namespace gts::rendering
{
    std::unordered_map<const ECSWorld*, MaterialRuntime>& materialRuntimeRegistry(ECSWorld* world = nullptr);
    const MaterialRuntime*                          findMaterialRuntime(const ECSWorld& world);
    void                                            resetMaterialRuntime(ECSWorld& world);
} // namespace gts::rendering
