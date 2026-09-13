#pragma once

#include <optional>
#include <variant>

#include "GlmConfig.h"
#include <gtc/quaternion.hpp>

struct GtsAnimationTrack;

namespace gts::animation::detail
{
    using SampledValue = std::variant<glm::vec3, glm::quat>;

    // internal: caller has validated the complete clip and time domain
    // null means interpolation cannot produce a finite value/valid rotation
    std::optional<SampledValue> sampleTrack(const GtsAnimationTrack& track, float timeSeconds);
} // namespace gts::animation::detail
