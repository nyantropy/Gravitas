#pragma once

#include <string>

#include "ParticleEffectAsset.h"
#include "ParticleEmitterComponent.h"

namespace gts::particles
{
    bool                         migrateParticleEffectAsset(ParticleEffectAsset& asset, std::string* error = nullptr);
    const ParticleEffectEmitter* selectParticleEffectEmitter(const ParticleEffectAsset& asset,
                                                             const std::string&         emitterId);
    bool                         loadParticleEffectAsset(const std::string& path, ParticleEffectAsset& asset);
    bool                         saveParticleEffectAsset(const std::string& path, const ParticleEffectAsset& asset);
    bool                         loadParticleEffect(const std::string& path, ParticleEmitterComponent& emitter);
    bool                         saveParticleEffect(const std::string& path, const ParticleEmitterComponent& emitter);
} // namespace gts::particles
