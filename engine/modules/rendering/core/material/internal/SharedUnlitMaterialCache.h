#pragma once

#include <cstddef>
#include <unordered_map>
#include "MaterialReferenceHelpers.h"

namespace gts::rendering
{
    struct UnlitMaterialDescriptorHash
    {
        size_t      operator()(const UnlitMaterialDescriptor& descriptor) const noexcept;
        static void combine(size_t& seed, size_t value) noexcept;
    };

    using SharedUnlitMaterialCache =
        std::unordered_map<UnlitMaterialDescriptor, MaterialInstanceHandle, UnlitMaterialDescriptorHash>;

    std::unordered_map<ECSWorld*, SharedUnlitMaterialCache>&
                              sharedUnlitMaterialCacheRegistry(ECSWorld* world = nullptr);
    SharedUnlitMaterialCache& sharedUnlitMaterialCache(ECSWorld& world);
    void                      resetSharedUnlitMaterialCache(ECSWorld& world);
} // namespace gts::rendering
