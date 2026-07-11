#pragma once

#include "MaterialComponent.h"
#include "MaterialTypes.h"

// Engine-owned compatibility bridge for legacy per-entity MaterialComponent.
// It owns the unique material instance created for that descriptor. The
// MaterialInstance is the runtime authority after conversion.
struct LegacyMaterialRuntimeComponent
{
    MaterialInstanceHandle material;
    MaterialComponent lastDescriptor;
    bool initialized = false;
};
