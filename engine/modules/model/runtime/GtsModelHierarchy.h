#pragma once
#include <vector>
#include "GtsModelResource.h"
#include "GlmConfig.h"

// Immutable authored hierarchy, without baking geometry or changing skeleton space.
std::vector<glm::mat4> gtsModelNodeTransforms(const GtsModelResource& model);
