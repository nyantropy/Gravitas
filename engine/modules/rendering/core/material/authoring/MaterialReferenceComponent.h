#pragma once

#include "MaterialTypes.h"

// Renderer-facing material reference. Renderable entities use material
// instances; they do not own material values or GPU material cache state.
struct MaterialReferenceComponent
{
    MaterialInstanceHandle material;
};
