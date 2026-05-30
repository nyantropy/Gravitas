#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

#include "ParticleEmitterComponent.h"

struct ParticleEffectAsset
{
    ParticleEmitterComponent emitter;
    std::filesystem::file_time_type lastWriteTime{};
    uint64_t version = 1;
    bool loaded = false;
};

struct ParticleEffectRegistryComponent
{
    std::unordered_map<std::string, ParticleEffectAsset> effects;
};

