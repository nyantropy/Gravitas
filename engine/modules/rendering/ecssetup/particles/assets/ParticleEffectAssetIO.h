#pragma once

#include <string>

#include "ParticleEmitterComponent.h"

namespace gts::particles
{
    bool loadParticleEffect(const std::string& path, ParticleEmitterComponent& emitter);
    bool saveParticleEffect(const std::string& path, const ParticleEmitterComponent& emitter);
}

