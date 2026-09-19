#pragma once
#include <memory>
#include "MaterialInstanceHandle.h"
class GtsRealizedModel;

// World material association boundary. Implemented by the material frontend; no
// material snapshots or runtime ownership cross into model occurrence state.
class GtsRealizedModelMaterials
{
    public:
    virtual ~GtsRealizedModelMaterials() = default;
    virtual MaterialInstanceHandle
                 materialFor(const GtsRealizedModel&, uint32_t geometry, uint32_t primitive) const = 0;
    virtual bool valid() const                                                                     = 0;
    virtual bool isMaterialAlive(MaterialInstanceHandle material) const                            = 0;
    virtual bool belongsTo(const GtsRealizedModel&) const                                          = 0;
    virtual std::weak_ptr<const int> scopeToken() const                                            = 0;
};
