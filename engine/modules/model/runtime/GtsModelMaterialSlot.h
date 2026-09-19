#pragma once

#include "assets/realization/model/GtsRealizedModel.h"

// Non-owning logical association scoped to one shared realization. Obtain through materialSlot().
class GtsModelMaterialSlot
{
    public:
    GtsModelMaterialSlot() = default;

    private:
    friend class GtsModelInstance;
    std::weak_ptr<const GtsRealizedModel> owner;
    const GtsRealizedMaterial*            association = nullptr;
};
