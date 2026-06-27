#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "GlmConfig.h"
#include "ParticleEmitterComponent.h"
#include "ParticleModuleAuthoring.h"

inline constexpr uint32_t CurrentParticleEffectSchemaVersion  = 6u;
inline constexpr uint32_t CurrentParticleEmitterSchemaVersion = 2u;
inline constexpr uint32_t CurrentParticleGraphSchemaVersion   = 1u;

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

struct ParticleGraphNode
{
    std::string id;
    std::string moduleStableId;
    std::string typeId;
    std::string displayName;
    std::string frameId;
    glm::vec2   position{0.0f, 0.0f};
};

struct ParticleGraphLink
{
    std::string id;
    std::string fromNodeId;
    std::string fromPortId;
    std::string toNodeId;
    std::string toPortId;
};

struct ParticleGraphFrame
{
    std::string id;
    std::string title;
    glm::vec2   position{0.0f, 0.0f};
    glm::vec2   size{1.0f, 1.0f};
};

struct ParticleGraphComment
{
    std::string id;
    std::string text;
    glm::vec2   position{0.0f, 0.0f};
};

struct ParticleEffectGraph
{
    uint32_t                          schemaVersion = CurrentParticleGraphSchemaVersion;
    std::vector<ParticleGraphNode>    nodes;
    std::vector<ParticleGraphLink>    links;
    std::vector<ParticleGraphFrame>   frames;
    std::vector<ParticleGraphComment> comments;
};

struct ParticleEffectEmitter
{
    std::string              stableId = "emitter";
    std::string              name     = "Emitter";
    ParticleEmitterComponent descriptor;
    std::vector<gts::particles::ParticleModuleInstance> modules;
    ParticleEffectGraph graph;
    gts::particles::ParticleCompiledParticleProgram compiledProgram;
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
