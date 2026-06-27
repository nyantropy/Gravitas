#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "GlmConfig.h"
#include "ParticleEmitterComponent.h"

inline constexpr uint32_t CurrentParticleEffectSchemaVersion  = 3u;
inline constexpr uint32_t CurrentParticleEmitterSchemaVersion = 2u;

struct ParticleEffectMetadata
{
    std::string name;
    std::string description;
    std::string author;
};

struct ParticleEffectPreviewSettings
{
    glm::vec4 backgroundColor = {0.02f, 0.025f, 0.03f, 1.0f};
    glm::vec3 cameraPosition  = {0.0f, 1.0f, 4.0f};
    glm::vec3 cameraTarget    = {0.0f, 0.6f, 0.0f};
    float     orbitDistance   = 4.0f;
};

struct ParticleEffectEmitter
{
    std::string              stableId = "emitter";
    std::string              name     = "Emitter";
    ParticleEmitterComponent descriptor;
};

struct ParticleEffectAsset
{
    uint32_t                           schemaVersion = CurrentParticleEffectSchemaVersion;
    ParticleEffectMetadata             metadata;
    ParticleEffectPreviewSettings      preview;
    std::vector<ParticleEffectEmitter> emitters;
};

struct ParticleEffectRegistryEntry
{
    ParticleEffectAsset             asset;
    std::filesystem::file_time_type lastWriteTime{};
    uint64_t                        version = 1;
    bool                            loaded  = false;
};

struct ParticleEffectRegistryComponent
{
    std::unordered_map<std::string, ParticleEffectRegistryEntry> effects;
};
