#pragma once
#include <vector>
#include "assets/loading/model/GtsModelResource.h"
#include "GlmConfig.h"

// Immutable authored hierarchy, without baking geometry or changing skeleton space.
std::vector<glm::mat4> gtsModelNodeTransforms(const GtsModelResource& model);
