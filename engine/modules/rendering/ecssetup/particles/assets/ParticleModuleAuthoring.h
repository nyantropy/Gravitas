#pragma once

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "GlmConfig.h"
#include "ParticleEmitterComponent.h"
#include "ParticleTypes.h"

namespace gts::particles
{
    inline constexpr uint32_t V1ParticleModuleSchemaVersion  = 1u;
    inline constexpr uint32_t CurrentParticleModuleSchemaVersion = 3u;
    inline constexpr uint32_t CurrentParticleProgramSchemaVersion = 1u;

    enum class ParticleModuleParameterType
    {
        Float,
        UInt,
        Bool,
        Enum,
        String,
        FloatCurve,
        ColorGradient,
        BurstTimeline
    };

    enum class ParticleModuleAssetPicker
    {
        None,
        Texture,
        Mesh
    };

    enum class ParticleModuleExecutionStage
    {
        Spawn,
        Initialize,
        Update,
        Render
    };

    enum class ParticleModuleExecutionCategory
    {
        Emitter,
        ParticleInitializer,
        ParticleUpdater,
        Renderer
    };

    enum class ParticleModulePortType
    {
        Flow,
        ParticleStream,
        Float,
        UInt,
        Bool,
        Enum,
        String,
        FloatCurve,
        ColorGradient,
        BurstTimeline,
        Vec3,
        Vec4
    };

    struct ParticleModulePort
    {
        std::string            id;
        std::string            label;
        ParticleModulePortType type = ParticleModulePortType::Flow;
    };

    struct ParticleModuleDependency
    {
        std::string typeId;
        std::string outputId;
        bool        required = true;
    };

    enum class ParticleModuleGraphDiagnosticSeverity
    {
        Warning,
        Error
    };

    struct ParticleModuleGraphDiagnostic
    {
        ParticleModuleGraphDiagnosticSeverity severity = ParticleModuleGraphDiagnosticSeverity::Error;
        std::string                           moduleStableId;
        std::string                           moduleTypeId;
        std::string                           message;
    };

    struct ParticleModuleEnumOption
    {
        uint32_t    value = 0;
        std::string label;
    };

    struct ParticleModuleParameterDefinition
    {
        std::string                           id;
        std::string                           label;
        ParticleModuleParameterType           type         = ParticleModuleParameterType::Float;
        float                                 minValue     = 0.0f;
        float                                 maxValue     = 1.0f;
        float                                 defaultFloat = 0.0f;
        uint32_t                              defaultUInt  = 0;
        bool                                  defaultBool  = false;
        std::string                           defaultString;
        ParticleFloatCurve                    defaultFloatCurve;
        ParticleColorCurve                    defaultColorGradient;
        std::vector<ParticleBurst>            defaultBursts;
        bool                                  wholeNumber = false;
        ParticleModuleAssetPicker             assetPicker = ParticleModuleAssetPicker::None;
        std::vector<ParticleModuleEnumOption> enumOptions;
    };

    struct ParticleModuleDefinition
    {
        std::string                                    typeId;
        std::string                                    category;
        std::string                                    displayName;
        std::string                                    description;
        uint32_t                                       version = CurrentParticleModuleSchemaVersion;
        std::vector<ParticleModuleParameterDefinition> parameters;
        ParticleModuleExecutionStage                   executionStage = ParticleModuleExecutionStage::Update;
        ParticleModuleExecutionCategory       executionCategory = ParticleModuleExecutionCategory::ParticleUpdater;
        std::vector<ParticleModulePort>       inputs;
        std::vector<ParticleModulePort>       outputs;
        std::vector<ParticleModuleDependency> dependencies;
    };

    struct ParticleModuleParameter
    {
        std::string                 id;
        ParticleModuleParameterType type       = ParticleModuleParameterType::Float;
        float                       floatValue = 0.0f;
        uint32_t                    uintValue  = 0;
        bool                        boolValue  = false;
        std::string                 stringValue;
        ParticleFloatCurve          floatCurveValue;
        ParticleColorCurve          colorGradientValue;
        std::vector<ParticleBurst>  burstTimelineValue;
    };

    struct ParticleModuleInstance
    {
        std::string                          stableId;
        std::string                          typeId;
        std::string                          displayName;
        uint32_t                             version = CurrentParticleModuleSchemaVersion;
        bool                                 enabled = true;
        std::vector<ParticleModuleParameter> parameters;
    };

    enum class ParticleProgramBackend
    {
        CpuDescriptor
    };

    struct ParticleCompiledModule
    {
        std::string                     stableId;
        std::string                     typeId;
        std::string                     displayName;
        ParticleModuleExecutionStage    executionStage    = ParticleModuleExecutionStage::Update;
        ParticleModuleExecutionCategory executionCategory = ParticleModuleExecutionCategory::ParticleUpdater;
        bool                            enabled           = true;
        uint32_t                        parameterCount    = 0;
        uint32_t                        inputCount        = 0;
        uint32_t                        outputCount       = 0;
        uint32_t                        dependencyCount   = 0;
    };

    struct ParticleCompiledParticleProgram
    {
        uint32_t                                  schemaVersion = CurrentParticleProgramSchemaVersion;
        ParticleProgramBackend                    backend       = ParticleProgramBackend::CpuDescriptor;
        bool                                      valid         = false;
        ParticleEmitterComponent                  runtimeDescriptor;
        std::vector<ParticleCompiledModule>       modules;
        std::vector<ParticleModuleGraphDiagnostic> diagnostics;
        uint32_t                                  deadNodesEliminated      = 0;
        uint32_t                                  staticParametersEvaluated = 0;
        uint32_t                                  curvesBaked              = 0;
        uint32_t                                  modulesFused             = 0;
    };

    inline ParticleModuleParameterDefinition
    floatParam(const std::string& id, const std::string& label, float minValue, float maxValue, float defaultValue)
    {
        ParticleModuleParameterDefinition param;
        param.id           = id;
        param.label        = label;
        param.type         = ParticleModuleParameterType::Float;
        param.minValue     = minValue;
        param.maxValue     = maxValue;
        param.defaultFloat = defaultValue;
        return param;
    }

    inline ParticleModuleParameterDefinition uintParam(
        const std::string& id, const std::string& label, uint32_t minValue, uint32_t maxValue, uint32_t defaultValue)
    {
        ParticleModuleParameterDefinition param;
        param.id          = id;
        param.label       = label;
        param.type        = ParticleModuleParameterType::UInt;
        param.minValue    = static_cast<float>(minValue);
        param.maxValue    = static_cast<float>(maxValue);
        param.defaultUInt = defaultValue;
        param.wholeNumber = true;
        return param;
    }

    inline ParticleModuleParameterDefinition
    boolParam(const std::string& id, const std::string& label, bool defaultValue)
    {
        ParticleModuleParameterDefinition param;
        param.id          = id;
        param.label       = label;
        param.type        = ParticleModuleParameterType::Bool;
        param.defaultBool = defaultValue;
        return param;
    }

    inline ParticleModuleParameterDefinition enumParam(const std::string&                           id,
                                                       const std::string&                           label,
                                                       uint32_t                                     defaultValue,
                                                       const std::vector<ParticleModuleEnumOption>& options)
    {
        ParticleModuleParameterDefinition param;
        param.id          = id;
        param.label       = label;
        param.type        = ParticleModuleParameterType::Enum;
        param.defaultUInt = defaultValue;
        param.enumOptions = options;
        param.wholeNumber = true;
        return param;
    }

    inline ParticleModuleParameterDefinition
    stringParam(const std::string&        id,
                const std::string&        label,
                const std::string&        defaultValue = {},
                ParticleModuleAssetPicker assetPicker  = ParticleModuleAssetPicker::None)
    {
        ParticleModuleParameterDefinition param;
        param.id            = id;
        param.label         = label;
        param.type          = ParticleModuleParameterType::String;
        param.defaultString = defaultValue;
        param.assetPicker   = assetPicker;
        return param;
    }

    inline ParticleModuleParameterDefinition floatCurveParam(const std::string&        id,
                                                             const std::string&        label,
                                                             float                     minValue,
                                                             float                     maxValue,
                                                             const ParticleFloatCurve& defaultValue)
    {
        ParticleModuleParameterDefinition param;
        param.id                = id;
        param.label             = label;
        param.type              = ParticleModuleParameterType::FloatCurve;
        param.minValue          = minValue;
        param.maxValue          = maxValue;
        param.defaultFloatCurve = defaultValue;
        return param;
    }

    inline ParticleModuleParameterDefinition
    colorGradientParam(const std::string& id, const std::string& label, const ParticleColorCurve& defaultValue)
    {
        ParticleModuleParameterDefinition param;
        param.id                   = id;
        param.label                = label;
        param.type                 = ParticleModuleParameterType::ColorGradient;
        param.minValue             = 0.0f;
        param.maxValue             = 1.0f;
        param.defaultColorGradient = defaultValue;
        return param;
    }

    inline ParticleModuleParameterDefinition
    burstTimelineParam(const std::string& id, const std::string& label, const std::vector<ParticleBurst>& defaultValue)
    {
        ParticleModuleParameterDefinition param;
        param.id            = id;
        param.label         = label;
        param.type          = ParticleModuleParameterType::BurstTimeline;
        param.minValue      = 0.0f;
        param.maxValue      = 12.0f;
        param.defaultBursts = defaultValue;
        return param;
    }

    inline ParticleModulePort port(const std::string& id, const std::string& label, ParticleModulePortType type)
    {
        ParticleModulePort result;
        result.id    = id;
        result.label = label;
        result.type  = type;
        return result;
    }

    inline ParticleModuleDependency
    dependency(const std::string& typeId, const std::string& outputId = {}, bool required = true)
    {
        ParticleModuleDependency result;
        result.typeId   = typeId;
        result.outputId = outputId;
        result.required = required;
        return result;
    }

    inline const char* executionStageLabel(ParticleModuleExecutionStage stage)
    {
        switch (stage)
        {
        case ParticleModuleExecutionStage::Spawn:
            return "Spawn";
        case ParticleModuleExecutionStage::Initialize:
            return "Initialize";
        case ParticleModuleExecutionStage::Update:
            return "Update";
        case ParticleModuleExecutionStage::Render:
            return "Render";
        }
        return "Update";
    }

    inline const char* executionStageShortLabel(ParticleModuleExecutionStage stage)
    {
        switch (stage)
        {
        case ParticleModuleExecutionStage::Spawn:
            return "SP";
        case ParticleModuleExecutionStage::Initialize:
            return "IN";
        case ParticleModuleExecutionStage::Update:
            return "UP";
        case ParticleModuleExecutionStage::Render:
            return "RD";
        }
        return "UP";
    }

    inline const char* executionCategoryLabel(ParticleModuleExecutionCategory category)
    {
        switch (category)
        {
        case ParticleModuleExecutionCategory::Emitter:
            return "Emitter";
        case ParticleModuleExecutionCategory::ParticleInitializer:
            return "Particle Init";
        case ParticleModuleExecutionCategory::ParticleUpdater:
            return "Particle Update";
        case ParticleModuleExecutionCategory::Renderer:
            return "Renderer";
        }
        return "Particle Update";
    }

    inline uint32_t executionStageOrder(ParticleModuleExecutionStage stage)
    {
        return static_cast<uint32_t>(stage);
    }

    inline const std::vector<ParticleModuleEnumOption>& shapeOptions()
    {
        static const std::vector<ParticleModuleEnumOption> options = {
            {static_cast<uint32_t>(ParticleEmitterShape::Sphere), "SPHERE"},
            {static_cast<uint32_t>(ParticleEmitterShape::Box), "BOX"},
            {static_cast<uint32_t>(ParticleEmitterShape::Disc), "DISC"},
            {static_cast<uint32_t>(ParticleEmitterShape::Cylinder), "CYL"},
            {static_cast<uint32_t>(ParticleEmitterShape::Ring), "RING"},
        };
        return options;
    }

    inline const std::vector<ParticleModuleEnumOption>& primitiveOptions()
    {
        static const std::vector<ParticleModuleEnumOption> options = {
            {static_cast<uint32_t>(ParticlePrimitive::Billboard), "BILLBOARD"},
            {static_cast<uint32_t>(ParticlePrimitive::Mesh), "MESH"},
        };
        return options;
    }

    inline const std::vector<ParticleModuleEnumOption>& blendOptions()
    {
        static const std::vector<ParticleModuleEnumOption> options = {
            {static_cast<uint32_t>(ParticleBlendMode::Alpha), "ALPHA"},
            {static_cast<uint32_t>(ParticleBlendMode::Additive), "ADD"},
        };
        return options;
    }

    inline const std::vector<ParticleModuleEnumOption>& spriteShapeOptions()
    {
        static const std::vector<ParticleModuleEnumOption> options = {
            {static_cast<uint32_t>(ParticleSpriteShape::SoftCircle), "SOFT"},
            {static_cast<uint32_t>(ParticleSpriteShape::Square), "SQUARE"},
            {static_cast<uint32_t>(ParticleSpriteShape::Diamond), "DIAMOND"},
            {static_cast<uint32_t>(ParticleSpriteShape::Petal), "PETAL"},
            {static_cast<uint32_t>(ParticleSpriteShape::Streak), "STREAK"},
        };
        return options;
    }

    inline const std::vector<ParticleModuleEnumOption>& collisionModeOptions()
    {
        static const std::vector<ParticleModuleEnumOption> options = {
            {static_cast<uint32_t>(ParticleCollisionMode::None), "NONE"},
            {static_cast<uint32_t>(ParticleCollisionMode::GroundPlane), "GROUND"},
        };
        return options;
    }

    inline const std::vector<ParticleModuleDefinition>& particleModuleDefinitions()
    {
        static const std::vector<ParticleModuleDefinition> definitions = {
            {"spawn.basic",
             "Spawn",
             "Spawn",
             "Emitter enablement and continuous particle emission.",
             CurrentParticleModuleSchemaVersion,
             {boolParam("emitterEnabled", "ENABLED", true),
              floatParam("emissionRate", "RATE", 0.0f, 180.0f, 32.0f),
              uintParam("maxParticles", "MAX", 1u, 512u, 128u),
              floatParam("intensity", "INTENSITY", 0.0f, 2.5f, 1.0f),
              floatParam("effectScale", "SCALE", 0.0f, 4.0f, 1.0f),
              floatParam("importance", "IMPORT", 0.0f, 8.0f, 1.0f),
              uintParam("budgetWeight", "BUDGET", 1u, 32u, 1u),
              uintParam("maxSpawnPerFrame", "SPAWN CAP", 0u, 2048u, 0u)},
             ParticleModuleExecutionStage::Spawn,
             ParticleModuleExecutionCategory::Emitter,
             {},
             {port("spawnRequests", "Spawn Requests", ParticleModulePortType::Flow)},
             {}},
            {"lifetime.basic",
             "Lifetime",
             "Lifetime",
             "Particle lifetime and emitter timing.",
             CurrentParticleModuleSchemaVersion,
             {boolParam("looping", "LOOP", true),
              floatParam("lifetimeMin", "LIFE MIN", 0.01f, 6.0f, 1.0f),
              floatParam("lifetimeMax", "LIFE MAX", 0.01f, 8.0f, 2.0f),
              floatParam("duration", "DURATION", 0.0f, 12.0f, 0.0f),
              floatParam("startDelay", "DELAY", 0.0f, 5.0f, 0.0f)},
             ParticleModuleExecutionStage::Initialize,
             ParticleModuleExecutionCategory::ParticleInitializer,
             {port("spawnRequests", "Spawn Requests", ParticleModulePortType::Flow)},
             {port("particleLifetime", "Lifetime", ParticleModulePortType::Float)},
             {dependency("spawn.basic", "spawnRequests")}},
            {"shape.basic",
             "Shape",
             "Shape",
             "Emitter spawn shape and dimensions.",
             CurrentParticleModuleSchemaVersion,
             {enumParam("shape", "SHAPE", static_cast<uint32_t>(ParticleEmitterShape::Sphere), shapeOptions()),
              floatParam("sphereRadius", "SPHERE R", 0.01f, 5.0f, 0.5f),
              floatParam("boxX", "BOX X", 0.01f, 5.0f, 0.5f),
              floatParam("boxY", "BOX Y", 0.01f, 5.0f, 0.5f),
              floatParam("boxZ", "BOX Z", 0.01f, 5.0f, 0.5f),
              floatParam("discRadius", "DISC R", 0.01f, 5.0f, 0.5f),
              floatParam("ringInnerRadius", "RING IN", 0.0f, 5.0f, 0.25f),
              floatParam("ringOuterRadius", "RING OUT", 0.01f, 5.0f, 0.65f),
              floatParam("cylinderRadius", "CYL R", 0.01f, 5.0f, 0.5f),
              floatParam("cylinderHeight", "CYL H", 0.01f, 5.0f, 1.0f)},
             ParticleModuleExecutionStage::Initialize,
             ParticleModuleExecutionCategory::ParticleInitializer,
             {port("spawnRequests", "Spawn Requests", ParticleModulePortType::Flow)},
             {port("spawnPosition", "Spawn Position", ParticleModulePortType::Vec3)},
             {dependency("spawn.basic", "spawnRequests")}},
            {"velocity.basic",
             "Velocity",
             "Velocity",
             "Initial particle velocity and drag.",
             CurrentParticleModuleSchemaVersion,
             {floatParam("initialVelocityX", "VEL X", -5.0f, 5.0f, 0.0f),
              floatParam("initialVelocityY", "VEL Y", -5.0f, 5.0f, 0.25f),
              floatParam("initialVelocityZ", "VEL Z", -5.0f, 5.0f, 0.0f),
              floatParam("velocitySpread", "SPREAD", 0.0f, 2.0f, 0.2f),
              floatParam("radialVelocityMin", "RAD MIN", -5.0f, 5.0f, 0.0f),
              floatParam("radialVelocityMax", "RAD MAX", -5.0f, 5.0f, 0.0f),
              floatParam("tangentVelocity", "TANGENT", -5.0f, 5.0f, 0.0f),
              floatParam("drag", "DRAG", 0.0f, 3.0f, 0.15f)},
             ParticleModuleExecutionStage::Initialize,
             ParticleModuleExecutionCategory::ParticleInitializer,
             {port("spawnPosition", "Spawn Position", ParticleModulePortType::Vec3)},
             {port("initialVelocity", "Initial Velocity", ParticleModulePortType::Vec3)},
             {dependency("shape.basic", "spawnPosition")}},
            {"forces.basic",
             "Forces",
             "Forces",
             "Per-frame acceleration, wind, vortex, radial, and noise forces.",
             CurrentParticleModuleSchemaVersion,
             {floatParam("accelerationX", "ACCEL X", -5.0f, 5.0f, 0.0f),
              floatParam("accelerationY", "ACCEL Y", -5.0f, 5.0f, 0.0f),
              floatParam("accelerationZ", "ACCEL Z", -5.0f, 5.0f, 0.0f),
              floatParam("windX", "WIND X", -5.0f, 5.0f, 0.0f),
              floatParam("windY", "WIND Y", -5.0f, 5.0f, 0.0f),
              floatParam("windZ", "WIND Z", -5.0f, 5.0f, 0.0f),
              floatParam("vortex", "VORTEX", -2.0f, 2.0f, 0.0f),
              floatParam("radial", "RADIAL", -5.0f, 5.0f, 0.0f),
              floatParam("noiseStrength", "NOISE", 0.0f, 1.0f, 0.0f),
              floatParam("noiseScale", "NOISE SC", 0.01f, 8.0f, 1.0f),
              enumParam("collisionMode",
                        "COLLISION",
                        static_cast<uint32_t>(ParticleCollisionMode::None),
                        collisionModeOptions()),
              floatParam("collisionGroundY", "GROUND Y", -50.0f, 50.0f, 0.0f),
              floatParam("collisionBounce", "BOUNCE", 0.0f, 2.0f, 0.35f),
              floatParam("collisionDamping", "DAMPING", 0.0f, 1.0f, 0.70f),
              boolParam("killOnCollision", "KILL HIT", false),
              uintParam("spawnOnDeathCount", "DEATH SPAWN", 0u, 128u, 0u),
              uintParam("spawnOnCollisionCount", "HIT SPAWN", 0u, 128u, 0u),
              uintParam("maxEventSpawnsPerFrame", "EVENT CAP", 0u, 512u, 32u)},
             ParticleModuleExecutionStage::Update,
             ParticleModuleExecutionCategory::ParticleUpdater,
             {port("initialVelocity", "Initial Velocity", ParticleModulePortType::Vec3)},
             {port("particleVelocity", "Particle Velocity", ParticleModulePortType::Vec3)},
             {dependency("velocity.basic", "initialVelocity")}},
            {"color.basic",
             "Color",
             "Color",
             "Tint, value variation, and alpha peak.",
             CurrentParticleModuleSchemaVersion,
             {floatParam("baseTintR", "TINT R", 0.0f, 1.0f, 1.0f),
              floatParam("baseTintG", "TINT G", 0.0f, 1.0f, 1.0f),
              floatParam("baseTintB", "TINT B", 0.0f, 1.0f, 1.0f),
              floatParam("baseTintA", "TINT A", 0.0f, 1.0f, 1.0f),
              colorGradientParam("colorOverLifetime",
                                 "GRADIENT",
                                 {{0.0f, {1.0f, 1.0f, 1.0f, 1.0f}}, {1.0f, {1.0f, 1.0f, 1.0f, 1.0f}}}),
              floatCurveParam("alphaOverLifetime",
                              "ALPHA CURVE",
                              0.0f,
                              1.0f,
                              {{0.0f, 0.0f}, {0.2f, 1.0f}, {0.8f, 1.0f}, {1.0f, 0.0f}}),
              floatParam("hueVariation", "HUE VAR", 0.0f, 1.0f, 0.0f),
              floatParam("valueVariation", "VAL VAR", 0.0f, 1.0f, 0.0f)},
             ParticleModuleExecutionStage::Initialize,
             ParticleModuleExecutionCategory::ParticleInitializer,
             {port("particleLifetime", "Lifetime", ParticleModulePortType::Float)},
             {port("particleColor", "Particle Color", ParticleModulePortType::Vec4)},
             {dependency("lifetime.basic", "particleLifetime")}},
            {"size.basic",
             "Size",
             "Size",
             "Size curve endpoints and billboard aspect ratio.",
             CurrentParticleModuleSchemaVersion,
             {floatCurveParam("sizeOverLifetime", "SIZE CURVE", 0.001f, 2.0f, {{0.0f, 0.35f}, {1.0f, 0.75f}}),
              floatParam("sizeRandomness", "SIZE RNG", 0.0f, 1.0f, 0.15f),
              floatParam("aspectRatioMin", "ASPECT MIN", 0.1f, 6.0f, 1.0f),
              floatParam("aspectRatioMax", "ASPECT MAX", 0.1f, 6.0f, 1.0f)},
             ParticleModuleExecutionStage::Initialize,
             ParticleModuleExecutionCategory::ParticleInitializer,
             {port("particleLifetime", "Lifetime", ParticleModulePortType::Float)},
             {port("particleSize", "Particle Size", ParticleModulePortType::Float)},
             {dependency("lifetime.basic", "particleLifetime")}},
            {"rotation.basic",
             "Rotation",
             "Rotation",
             "Billboard spin and mesh angular velocity.",
             CurrentParticleModuleSchemaVersion,
             {floatParam("spinMin", "SPIN MIN", -8.0f, 8.0f, -0.8f),
              floatParam("spinMax", "SPIN MAX", -8.0f, 8.0f, 0.8f),
              floatParam("meshSpinXMin", "MESH X-", -8.0f, 8.0f, -0.8f),
              floatParam("meshSpinYMin", "MESH Y-", -8.0f, 8.0f, -0.8f),
              floatParam("meshSpinZMin", "MESH Z-", -8.0f, 8.0f, -0.8f),
              floatParam("meshSpinXMax", "MESH X+", -8.0f, 8.0f, 0.8f),
              floatParam("meshSpinYMax", "MESH Y+", -8.0f, 8.0f, 0.8f),
              floatParam("meshSpinZMax", "MESH Z+", -8.0f, 8.0f, 0.8f),
              boolParam("randomMeshRotation", "RANDOM MESH", true)},
             ParticleModuleExecutionStage::Initialize,
             ParticleModuleExecutionCategory::ParticleInitializer,
             {port("particleLifetime", "Lifetime", ParticleModulePortType::Float)},
             {port("particleRotation", "Particle Rotation", ParticleModulePortType::Vec3)},
             {dependency("lifetime.basic", "particleLifetime")}},
            {"renderer.basic",
             "Renderer",
             "Renderer",
             "Particle primitive, blend, sprite mask, and render material paths.",
             CurrentParticleModuleSchemaVersion,
             {enumParam(
                  "primitive", "PRIMITIVE", static_cast<uint32_t>(ParticlePrimitive::Billboard), primitiveOptions()),
              enumParam("blend", "BLEND", static_cast<uint32_t>(ParticleBlendMode::Alpha), blendOptions()),
              enumParam("spriteShape",
                        "SPRITE",
                        static_cast<uint32_t>(ParticleSpriteShape::SoftCircle),
                        spriteShapeOptions()),
              stringParam("texturePath", "TEXTURE", {}, ParticleModuleAssetPicker::Texture),
              stringParam("meshPath", "MESH", {}, ParticleModuleAssetPicker::Mesh),
              stringParam("materialPath", "MATERIAL", {}, ParticleModuleAssetPicker::None),
              floatParam("spriteEdgeSoftness", "EDGE", 0.0f, 1.0f, 1.0f),
              floatParam("softness", "SOFT", 0.0f, 240.0f, 80.0f),
              floatParam("meshSoftness", "MESH SOFT", 0.0f, 240.0f, 0.0f),
              floatParam("lightingInfluence", "LIGHT", 0.0f, 1.0f, 0.0f),
              boolParam("frustumCulling", "FRUSTUM", true),
              boolParam("distanceCulling", "DIST CULL", false),
              boolParam("simulateWhenCulled", "SIM CULLED", true),
              floatParam("cullPadding", "CULL PAD", 0.0f, 8.0f, 0.5f),
              floatParam("maxDrawDistance", "MAX DIST", 0.0f, 200.0f, 0.0f),
              floatParam("lodNearDistance", "LOD NEAR", 0.0f, 200.0f, 12.0f),
              floatParam("lodFarDistance", "LOD FAR", 0.0f, 400.0f, 42.0f),
              floatParam("lodMinSpawnScale", "LOD SPAWN", 0.0f, 1.0f, 0.35f),
              floatParam("lodMinRenderScale", "LOD SIZE", 0.0f, 1.0f, 0.50f),
              floatParam("velocityStretch", "STRETCH", 0.0f, 0.25f, 0.0f),
              floatParam("velocityStretchMax", "STR MAX", 0.0f, 8.0f, 3.0f),
              floatParam("meshScaleX", "MESH X", 0.01f, 6.0f, 1.0f),
              floatParam("meshScaleY", "MESH Y", 0.01f, 6.0f, 1.0f),
              floatParam("meshScaleZ", "MESH Z", 0.01f, 6.0f, 1.0f)},
             ParticleModuleExecutionStage::Render,
             ParticleModuleExecutionCategory::Renderer,
             {port("particleColor", "Particle Color", ParticleModulePortType::Vec4),
              port("particleSize", "Particle Size", ParticleModulePortType::Float),
              port("particleRotation", "Particle Rotation", ParticleModulePortType::Vec3)},
             {port("renderParticles", "Render Particles", ParticleModulePortType::ParticleStream)},
             {dependency("color.basic", "particleColor"),
              dependency("size.basic", "particleSize"),
              dependency("rotation.basic", "particleRotation", false)}},
            {"bursts.basic",
             "Bursts",
             "Bursts",
             "First burst authoring bridge for the current runtime burst list.",
             CurrentParticleModuleSchemaVersion,
             {burstTimelineParam("bursts", "TIMELINE", {})},
             ParticleModuleExecutionStage::Spawn,
             ParticleModuleExecutionCategory::Emitter,
             {port("spawnRequests", "Spawn Requests", ParticleModulePortType::Flow)},
             {port("burstRequests", "Burst Requests", ParticleModulePortType::Flow)},
             {dependency("spawn.basic", "spawnRequests")}},
        };
        return definitions;
    }

    inline const ParticleModuleDefinition* findParticleModuleDefinition(const std::string& typeId)
    {
        const auto& definitions = particleModuleDefinitions();
        const auto  it          = std::find_if(definitions.begin(),
                                               definitions.end(),
                                               [&](const ParticleModuleDefinition& definition)
                                               {
                                         return definition.typeId == typeId;
                                               });
        return it == definitions.end() ? nullptr : &*it;
    }

    inline ParticleModuleExecutionStage moduleExecutionStage(const std::string& typeId)
    {
        const ParticleModuleDefinition* definition = findParticleModuleDefinition(typeId);
        return definition == nullptr ? ParticleModuleExecutionStage::Update : definition->executionStage;
    }

    inline const ParticleModulePort* findOutputPort(const ParticleModuleDefinition& definition, const std::string& id)
    {
        const auto it = std::find_if(definition.outputs.begin(),
                                     definition.outputs.end(),
                                     [&](const ParticleModulePort& output)
                                     {
                                         return output.id == id;
                                     });
        return it == definition.outputs.end() ? nullptr : &*it;
    }

    inline const ParticleModulePort* findInputPort(const ParticleModuleDefinition& definition, const std::string& id)
    {
        const auto it = std::find_if(definition.inputs.begin(),
                                     definition.inputs.end(),
                                     [&](const ParticleModulePort& input)
                                     {
                                         return input.id == id;
                                     });
        return it == definition.inputs.end() ? nullptr : &*it;
    }

    inline bool graphDiagnosticsHaveErrors(const std::vector<ParticleModuleGraphDiagnostic>& diagnostics)
    {
        for (const ParticleModuleGraphDiagnostic& diagnostic : diagnostics)
        {
            if (diagnostic.severity == ParticleModuleGraphDiagnosticSeverity::Error)
                return true;
        }
        return false;
    }

    inline void addGraphDiagnostic(std::vector<ParticleModuleGraphDiagnostic>* diagnostics,
                                   ParticleModuleGraphDiagnosticSeverity       severity,
                                   const std::string&                          message,
                                   const std::string&                          moduleTypeId   = {},
                                   const std::string&                          moduleStableId = {})
    {
        if (diagnostics == nullptr)
            return;

        diagnostics->push_back({severity, moduleStableId, moduleTypeId, message});
    }

    inline bool hasDuplicateString(const std::vector<std::string>& values, const std::string& value)
    {
        return std::find(values.begin(), values.end(), value) != values.end();
    }

    inline bool validateParticleModuleDefinitions(std::vector<ParticleModuleGraphDiagnostic>* diagnostics = nullptr)
    {
        std::vector<ParticleModuleGraphDiagnostic> localDiagnostics;
        if (diagnostics == nullptr)
            diagnostics = &localDiagnostics;

        const std::vector<ParticleModuleDefinition>& definitions = particleModuleDefinitions();
        std::vector<std::string>                     typeIds;
        for (const ParticleModuleDefinition& definition : definitions)
        {
            if (definition.typeId.empty())
                addGraphDiagnostic(
                    diagnostics, ParticleModuleGraphDiagnosticSeverity::Error, "module definition has empty type id");
            else if (hasDuplicateString(typeIds, definition.typeId))
                addGraphDiagnostic(diagnostics,
                                   ParticleModuleGraphDiagnosticSeverity::Error,
                                   "module definition type id is duplicated",
                                   definition.typeId);
            typeIds.push_back(definition.typeId);

            std::vector<std::string> parameterIds;
            for (const ParticleModuleParameterDefinition& parameter : definition.parameters)
            {
                if (parameter.id.empty())
                    addGraphDiagnostic(diagnostics,
                                       ParticleModuleGraphDiagnosticSeverity::Error,
                                       "module parameter has empty id",
                                       definition.typeId);
                else if (hasDuplicateString(parameterIds, parameter.id))
                    addGraphDiagnostic(diagnostics,
                                       ParticleModuleGraphDiagnosticSeverity::Error,
                                       "module parameter id is duplicated: " + parameter.id,
                                       definition.typeId);
                parameterIds.push_back(parameter.id);
            }

            if (definition.outputs.empty())
                addGraphDiagnostic(diagnostics,
                                   ParticleModuleGraphDiagnosticSeverity::Error,
                                   "module definition has no outputs",
                                   definition.typeId);

            std::vector<std::string> inputIds;
            for (const ParticleModulePort& input : definition.inputs)
            {
                if (input.id.empty())
                    addGraphDiagnostic(diagnostics,
                                       ParticleModuleGraphDiagnosticSeverity::Error,
                                       "module input has empty id",
                                       definition.typeId);
                else if (hasDuplicateString(inputIds, input.id))
                    addGraphDiagnostic(diagnostics,
                                       ParticleModuleGraphDiagnosticSeverity::Error,
                                       "module input id is duplicated: " + input.id,
                                       definition.typeId);
                inputIds.push_back(input.id);
            }

            std::vector<std::string> outputIds;
            for (const ParticleModulePort& output : definition.outputs)
            {
                if (output.id.empty())
                    addGraphDiagnostic(diagnostics,
                                       ParticleModuleGraphDiagnosticSeverity::Error,
                                       "module output has empty id",
                                       definition.typeId);
                else if (hasDuplicateString(outputIds, output.id))
                    addGraphDiagnostic(diagnostics,
                                       ParticleModuleGraphDiagnosticSeverity::Error,
                                       "module output id is duplicated: " + output.id,
                                       definition.typeId);
                outputIds.push_back(output.id);
            }

            for (const ParticleModuleDependency& dependencyInfo : definition.dependencies)
            {
                const ParticleModuleDefinition* dependencyDefinition =
                    findParticleModuleDefinition(dependencyInfo.typeId);
                if (dependencyDefinition == nullptr)
                {
                    if (dependencyInfo.required)
                        addGraphDiagnostic(diagnostics,
                                           ParticleModuleGraphDiagnosticSeverity::Error,
                                           "required module dependency is unknown: " + dependencyInfo.typeId,
                                           definition.typeId);
                    continue;
                }

                if (!dependencyInfo.outputId.empty() &&
                    findOutputPort(*dependencyDefinition, dependencyInfo.outputId) == nullptr)
                {
                    addGraphDiagnostic(diagnostics,
                                       ParticleModuleGraphDiagnosticSeverity::Error,
                                       "module dependency output is unknown: " + dependencyInfo.outputId,
                                       definition.typeId);
                }
                else if (!dependencyInfo.outputId.empty())
                {
                    const ParticleModulePort* dependencyOutput = findOutputPort(*dependencyDefinition,
                                                                                dependencyInfo.outputId);
                    const ParticleModulePort* localInput       = findInputPort(definition, dependencyInfo.outputId);
                    if (localInput == nullptr)
                    {
                        addGraphDiagnostic(diagnostics,
                                           ParticleModuleGraphDiagnosticSeverity::Warning,
                                           "module dependency output has no matching stack input: " +
                                               dependencyInfo.outputId,
                                           definition.typeId);
                    }
                    else if (dependencyOutput != nullptr && dependencyOutput->type != localInput->type)
                    {
                        addGraphDiagnostic(diagnostics,
                                           ParticleModuleGraphDiagnosticSeverity::Error,
                                           "module dependency port type mismatch: " + dependencyInfo.outputId,
                                           definition.typeId);
                    }
                }

                if (executionStageOrder(dependencyDefinition->executionStage) >
                    executionStageOrder(definition.executionStage))
                {
                    addGraphDiagnostic(diagnostics,
                                       ParticleModuleGraphDiagnosticSeverity::Error,
                                       "module depends on a later execution stage: " + dependencyInfo.typeId,
                                       definition.typeId);
                }
            }
        }

        return !graphDiagnosticsHaveErrors(*diagnostics);
    }

    inline bool validateParticleModuleStack(const std::vector<ParticleModuleInstance>&  modules,
                                            std::vector<ParticleModuleGraphDiagnostic>* diagnostics = nullptr)
    {
        std::vector<ParticleModuleGraphDiagnostic> localDiagnostics;
        if (diagnostics == nullptr)
            diagnostics = &localDiagnostics;

        validateParticleModuleDefinitions(diagnostics);

        std::vector<std::string> stableIds;
        std::vector<std::string> typeIds;
        for (const ParticleModuleInstance& module : modules)
        {
            const ParticleModuleDefinition* definition = findParticleModuleDefinition(module.typeId);
            if (module.stableId.empty())
                addGraphDiagnostic(diagnostics,
                                   ParticleModuleGraphDiagnosticSeverity::Error,
                                   "module instance has empty stable id",
                                   module.typeId);
            else if (hasDuplicateString(stableIds, module.stableId))
                addGraphDiagnostic(diagnostics,
                                   ParticleModuleGraphDiagnosticSeverity::Error,
                                   "module stable id is duplicated",
                                   module.typeId,
                                   module.stableId);
            stableIds.push_back(module.stableId);

            if (definition == nullptr)
            {
                addGraphDiagnostic(diagnostics,
                                   ParticleModuleGraphDiagnosticSeverity::Error,
                                   "module type is not registered",
                                   module.typeId,
                                   module.stableId);
                continue;
            }

            if (module.version > definition->version)
            {
                addGraphDiagnostic(diagnostics,
                                   ParticleModuleGraphDiagnosticSeverity::Error,
                                   "module version is newer than this engine",
                                   module.typeId,
                                   module.stableId);
            }

            if (hasDuplicateString(typeIds, module.typeId))
            {
                addGraphDiagnostic(diagnostics,
                                   ParticleModuleGraphDiagnosticSeverity::Error,
                                   "module type is duplicated in stack: " + module.typeId,
                                   module.typeId,
                                   module.stableId);
            }
            typeIds.push_back(module.typeId);

            std::vector<std::string> parameterIds;
            for (const ParticleModuleParameter& parameter : module.parameters)
            {
                if (parameter.id.empty())
                    addGraphDiagnostic(diagnostics,
                                       ParticleModuleGraphDiagnosticSeverity::Error,
                                       "module parameter instance has empty id",
                                       module.typeId,
                                       module.stableId);
                else if (hasDuplicateString(parameterIds, parameter.id))
                    addGraphDiagnostic(diagnostics,
                                       ParticleModuleGraphDiagnosticSeverity::Error,
                                       "module parameter instance id is duplicated: " + parameter.id,
                                       module.typeId,
                                       module.stableId);
                parameterIds.push_back(parameter.id);
            }

            for (const ParticleModuleDependency& dependencyInfo : definition->dependencies)
            {
                const auto dependencyModule = std::find_if(modules.begin(),
                                                           modules.end(),
                                                           [&](const ParticleModuleInstance& candidate)
                                                           {
                                                               return candidate.typeId == dependencyInfo.typeId;
                                                           });
                if (dependencyModule == modules.end())
                {
                    if (dependencyInfo.required)
                    {
                        addGraphDiagnostic(diagnostics,
                                           ParticleModuleGraphDiagnosticSeverity::Error,
                                           "required module dependency is missing: " + dependencyInfo.typeId,
                                           module.typeId,
                                           module.stableId);
                    }
                    continue;
                }

                const ParticleModuleDefinition* dependencyDefinition =
                    findParticleModuleDefinition(dependencyModule->typeId);
                const ParticleModulePort* dependencyOutput =
                    dependencyDefinition == nullptr || dependencyInfo.outputId.empty()
                        ? nullptr
                        : findOutputPort(*dependencyDefinition, dependencyInfo.outputId);
                const ParticleModulePort* localInput =
                    dependencyInfo.outputId.empty() ? nullptr : findInputPort(*definition, dependencyInfo.outputId);

                if (dependencyInfo.required && !dependencyInfo.outputId.empty() && dependencyOutput == nullptr)
                {
                    addGraphDiagnostic(diagnostics,
                                       ParticleModuleGraphDiagnosticSeverity::Error,
                                       "required module dependency output is missing: " + dependencyInfo.outputId,
                                       module.typeId,
                                       module.stableId);
                }
                if (dependencyInfo.required && dependencyOutput != nullptr && localInput == nullptr)
                {
                    addGraphDiagnostic(diagnostics,
                                       ParticleModuleGraphDiagnosticSeverity::Error,
                                       "required module input is missing: " + dependencyInfo.outputId,
                                       module.typeId,
                                       module.stableId);
                }
                if (dependencyOutput != nullptr && localInput != nullptr && dependencyOutput->type != localInput->type)
                {
                    addGraphDiagnostic(diagnostics,
                                       ParticleModuleGraphDiagnosticSeverity::Error,
                                       "required module dependency port type mismatch: " + dependencyInfo.outputId,
                                       module.typeId,
                                       module.stableId);
                }
                if (dependencyInfo.required && !dependencyModule->enabled)
                {
                    addGraphDiagnostic(diagnostics,
                                       ParticleModuleGraphDiagnosticSeverity::Warning,
                                       "dependency is disabled; default module output will be used: " +
                                           dependencyInfo.typeId,
                                       module.typeId,
                                       module.stableId);
                }
            }
        }

        return !graphDiagnosticsHaveErrors(*diagnostics);
    }

    inline std::vector<const ParticleModuleInstance*>
    buildParticleModuleExecutionPlan(const std::vector<ParticleModuleInstance>& modules)
    {
        std::vector<const ParticleModuleInstance*> plan;
        plan.reserve(modules.size());
        for (const ParticleModuleInstance& module : modules)
            plan.push_back(&module);

        std::stable_sort(plan.begin(),
                         plan.end(),
                         [](const ParticleModuleInstance* lhs, const ParticleModuleInstance* rhs)
                         {
                             const ParticleModuleDefinition* lhsDefinition = findParticleModuleDefinition(lhs->typeId);
                             const ParticleModuleDefinition* rhsDefinition = findParticleModuleDefinition(rhs->typeId);
                             const uint32_t lhsStage = lhsDefinition == nullptr
                                                           ? executionStageOrder(ParticleModuleExecutionStage::Update)
                                                           : executionStageOrder(lhsDefinition->executionStage);
                             const uint32_t rhsStage = rhsDefinition == nullptr
                                                           ? executionStageOrder(ParticleModuleExecutionStage::Update)
                                                           : executionStageOrder(rhsDefinition->executionStage);
                             return lhsStage < rhsStage;
                         });
        return plan;
    }

    inline ParticleModuleParameter makeDefaultParameter(const ParticleModuleParameterDefinition& definition)
    {
        ParticleModuleParameter parameter;
        parameter.id                 = definition.id;
        parameter.type               = definition.type;
        parameter.floatValue         = definition.defaultFloat;
        parameter.uintValue          = definition.defaultUInt;
        parameter.boolValue          = definition.defaultBool;
        parameter.stringValue        = definition.defaultString;
        parameter.floatCurveValue    = definition.defaultFloatCurve;
        parameter.colorGradientValue = definition.defaultColorGradient;
        parameter.burstTimelineValue = definition.defaultBursts;
        return parameter;
    }

    inline ParticleModuleInstance makeDefaultModuleInstance(const ParticleModuleDefinition& definition)
    {
        ParticleModuleInstance module;
        module.stableId    = definition.category;
        module.typeId      = definition.typeId;
        module.displayName = definition.displayName;
        module.version     = definition.version;
        module.enabled     = true;
        module.parameters.reserve(definition.parameters.size());
        for (const ParticleModuleParameterDefinition& parameterDefinition : definition.parameters)
            module.parameters.push_back(makeDefaultParameter(parameterDefinition));
        return module;
    }

    inline ParticleModuleParameter* findParameter(ParticleModuleInstance& module, const std::string& id)
    {
        const auto it = std::find_if(module.parameters.begin(),
                                     module.parameters.end(),
                                     [&](const ParticleModuleParameter& parameter)
                                     {
                                         return parameter.id == id;
                                     });
        return it == module.parameters.end() ? nullptr : &*it;
    }

    inline const ParticleModuleParameter* findParameter(const ParticleModuleInstance& module, const std::string& id)
    {
        const auto it = std::find_if(module.parameters.begin(),
                                     module.parameters.end(),
                                     [&](const ParticleModuleParameter& parameter)
                                     {
                                         return parameter.id == id;
                                     });
        return it == module.parameters.end() ? nullptr : &*it;
    }

    inline void setFloatParameter(ParticleModuleInstance& module, const std::string& id, float value)
    {
        if (ParticleModuleParameter* parameter = findParameter(module, id))
            parameter->floatValue = value;
    }

    inline void setUIntParameter(ParticleModuleInstance& module, const std::string& id, uint32_t value)
    {
        if (ParticleModuleParameter* parameter = findParameter(module, id))
            parameter->uintValue = value;
    }

    inline void setBoolParameter(ParticleModuleInstance& module, const std::string& id, bool value)
    {
        if (ParticleModuleParameter* parameter = findParameter(module, id))
            parameter->boolValue = value;
    }

    inline void setStringParameter(ParticleModuleInstance& module, const std::string& id, const std::string& value)
    {
        if (ParticleModuleParameter* parameter = findParameter(module, id))
            parameter->stringValue = value;
    }

    inline void
    setFloatCurveParameter(ParticleModuleInstance& module, const std::string& id, const ParticleFloatCurve& value)
    {
        if (ParticleModuleParameter* parameter = findParameter(module, id))
            parameter->floatCurveValue = value;
    }

    inline void
    setColorGradientParameter(ParticleModuleInstance& module, const std::string& id, const ParticleColorCurve& value)
    {
        if (ParticleModuleParameter* parameter = findParameter(module, id))
            parameter->colorGradientValue = value;
    }

    inline void setBurstTimelineParameter(ParticleModuleInstance&           module,
                                          const std::string&                id,
                                          const std::vector<ParticleBurst>& value)
    {
        if (ParticleModuleParameter* parameter = findParameter(module, id))
            parameter->burstTimelineValue = value;
    }

    inline float floatParameter(const ParticleModuleInstance& module, const std::string& id, float fallback = 0.0f)
    {
        const ParticleModuleParameter* parameter = findParameter(module, id);
        return parameter == nullptr ? fallback : parameter->floatValue;
    }

    inline uint32_t uintParameter(const ParticleModuleInstance& module, const std::string& id, uint32_t fallback = 0u)
    {
        const ParticleModuleParameter* parameter = findParameter(module, id);
        return parameter == nullptr ? fallback : parameter->uintValue;
    }

    inline bool boolParameter(const ParticleModuleInstance& module, const std::string& id, bool fallback = false)
    {
        const ParticleModuleParameter* parameter = findParameter(module, id);
        return parameter == nullptr ? fallback : parameter->boolValue;
    }

    inline std::string
    stringParameter(const ParticleModuleInstance& module, const std::string& id, const std::string& fallback = {})
    {
        const ParticleModuleParameter* parameter = findParameter(module, id);
        return parameter == nullptr ? fallback : parameter->stringValue;
    }

    inline ParticleFloatCurve floatCurveParameter(const ParticleModuleInstance& module,
                                                  const std::string&            id,
                                                  const ParticleFloatCurve&     fallback = {})
    {
        const ParticleModuleParameter* parameter = findParameter(module, id);
        return parameter == nullptr ? fallback : parameter->floatCurveValue;
    }

    inline ParticleColorCurve colorGradientParameter(const ParticleModuleInstance& module,
                                                     const std::string&            id,
                                                     const ParticleColorCurve&     fallback = {})
    {
        const ParticleModuleParameter* parameter = findParameter(module, id);
        return parameter == nullptr ? fallback : parameter->colorGradientValue;
    }

    inline std::vector<ParticleBurst> burstTimelineParameter(const ParticleModuleInstance&     module,
                                                             const std::string&                id,
                                                             const std::vector<ParticleBurst>& fallback = {})
    {
        const ParticleModuleParameter* parameter = findParameter(module, id);
        return parameter == nullptr ? fallback : parameter->burstTimelineValue;
    }

    inline ParticleModuleInstance moduleFromDescriptor(const ParticleModuleDefinition& definition,
                                                       const ParticleEmitterComponent& emitter)
    {
        ParticleModuleInstance module = makeDefaultModuleInstance(definition);

        if (definition.typeId == "spawn.basic")
        {
            setBoolParameter(module, "emitterEnabled", emitter.enabled);
            setFloatParameter(module, "emissionRate", emitter.emissionRate);
            setUIntParameter(module, "maxParticles", emitter.maxParticles);
            setFloatParameter(module, "intensity", emitter.intensity);
            setFloatParameter(module, "effectScale", emitter.runtime.effectScale);
            setFloatParameter(module, "importance", emitter.runtime.importance);
            setUIntParameter(module, "budgetWeight", emitter.runtime.budgetWeight);
            setUIntParameter(module, "maxSpawnPerFrame", emitter.runtime.maxSpawnPerFrame);
        }
        else if (definition.typeId == "lifetime.basic")
        {
            setBoolParameter(module, "looping", emitter.looping);
            setFloatParameter(module, "lifetimeMin", emitter.lifetimeMin);
            setFloatParameter(module, "lifetimeMax", emitter.lifetimeMax);
            setFloatParameter(module, "duration", emitter.duration);
            setFloatParameter(module, "startDelay", emitter.startDelay);
        }
        else if (definition.typeId == "shape.basic")
        {
            setUIntParameter(module, "shape", static_cast<uint32_t>(emitter.shape));
            setFloatParameter(module, "sphereRadius", emitter.sphereRadius);
            setFloatParameter(module, "boxX", emitter.boxExtents.x);
            setFloatParameter(module, "boxY", emitter.boxExtents.y);
            setFloatParameter(module, "boxZ", emitter.boxExtents.z);
            setFloatParameter(module, "discRadius", emitter.discRadius);
            setFloatParameter(module, "ringInnerRadius", emitter.ringInnerRadius);
            setFloatParameter(module, "ringOuterRadius", emitter.ringOuterRadius);
            setFloatParameter(module, "cylinderRadius", emitter.cylinderRadius);
            setFloatParameter(module, "cylinderHeight", emitter.cylinderHeight);
        }
        else if (definition.typeId == "velocity.basic")
        {
            setFloatParameter(module, "initialVelocityX", emitter.initialVelocity.x);
            setFloatParameter(module, "initialVelocityY", emitter.initialVelocity.y);
            setFloatParameter(module, "initialVelocityZ", emitter.initialVelocity.z);
            setFloatParameter(module, "velocitySpread", emitter.velocitySpread);
            setFloatParameter(module, "radialVelocityMin", emitter.radialVelocityMin);
            setFloatParameter(module, "radialVelocityMax", emitter.radialVelocityMax);
            setFloatParameter(module, "tangentVelocity", emitter.tangentVelocity);
            setFloatParameter(module, "drag", emitter.drag);
        }
        else if (definition.typeId == "forces.basic")
        {
            setFloatParameter(module, "accelerationX", emitter.forces.acceleration.x);
            setFloatParameter(module, "accelerationY", emitter.forces.acceleration.y);
            setFloatParameter(module, "accelerationZ", emitter.forces.acceleration.z);
            setFloatParameter(module, "windX", emitter.forces.wind.x);
            setFloatParameter(module, "windY", emitter.forces.wind.y);
            setFloatParameter(module, "windZ", emitter.forces.wind.z);
            setFloatParameter(module, "vortex", emitter.forces.vortex);
            setFloatParameter(module, "radial", emitter.forces.radial);
            setFloatParameter(module, "noiseStrength", emitter.forces.noiseStrength);
            setFloatParameter(module, "noiseScale", emitter.forces.noiseScale);
            setUIntParameter(module, "collisionMode", static_cast<uint32_t>(emitter.collision.mode));
            setFloatParameter(module, "collisionGroundY", emitter.collision.groundY);
            setFloatParameter(module, "collisionBounce", emitter.collision.bounce);
            setFloatParameter(module, "collisionDamping", emitter.collision.damping);
            setBoolParameter(module, "killOnCollision", emitter.collision.killOnCollision);
            setUIntParameter(module, "spawnOnDeathCount", emitter.collision.spawnOnDeathCount);
            setUIntParameter(module, "spawnOnCollisionCount", emitter.collision.spawnOnCollisionCount);
            setUIntParameter(module, "maxEventSpawnsPerFrame", emitter.collision.maxEventSpawnsPerFrame);
        }
        else if (definition.typeId == "color.basic")
        {
            setFloatParameter(module, "baseTintR", emitter.baseTint.r);
            setFloatParameter(module, "baseTintG", emitter.baseTint.g);
            setFloatParameter(module, "baseTintB", emitter.baseTint.b);
            setFloatParameter(module, "baseTintA", emitter.baseTint.a);
            setColorGradientParameter(module, "colorOverLifetime", emitter.colorOverLifetime);
            setFloatCurveParameter(module, "alphaOverLifetime", emitter.alphaOverLifetime);
            setFloatParameter(module, "hueVariation", emitter.hueVariation);
            setFloatParameter(module, "valueVariation", emitter.valueVariation);
        }
        else if (definition.typeId == "size.basic")
        {
            setFloatCurveParameter(module, "sizeOverLifetime", emitter.sizeOverLifetime);
            setFloatParameter(module, "sizeRandomness", emitter.sizeRandomness);
            setFloatParameter(module, "aspectRatioMin", emitter.aspectRatioMin);
            setFloatParameter(module, "aspectRatioMax", emitter.aspectRatioMax);
        }
        else if (definition.typeId == "rotation.basic")
        {
            setFloatParameter(module, "spinMin", emitter.spinMin);
            setFloatParameter(module, "spinMax", emitter.spinMax);
            setFloatParameter(module, "meshSpinXMin", emitter.meshAngularVelocityMin.x);
            setFloatParameter(module, "meshSpinYMin", emitter.meshAngularVelocityMin.y);
            setFloatParameter(module, "meshSpinZMin", emitter.meshAngularVelocityMin.z);
            setFloatParameter(module, "meshSpinXMax", emitter.meshAngularVelocityMax.x);
            setFloatParameter(module, "meshSpinYMax", emitter.meshAngularVelocityMax.y);
            setFloatParameter(module, "meshSpinZMax", emitter.meshAngularVelocityMax.z);
            setBoolParameter(module, "randomMeshRotation", emitter.randomMeshRotation);
        }
        else if (definition.typeId == "renderer.basic")
        {
            setUIntParameter(module, "primitive", static_cast<uint32_t>(emitter.primitive));
            setUIntParameter(module, "blend", static_cast<uint32_t>(emitter.blend));
            setUIntParameter(module, "spriteShape", static_cast<uint32_t>(emitter.spriteShape));
            setStringParameter(module, "texturePath", emitter.texturePath);
            setStringParameter(module, "meshPath", emitter.meshPath);
            setStringParameter(module, "materialPath", emitter.materialPath);
            setFloatParameter(module, "spriteEdgeSoftness", emitter.spriteEdgeSoftness);
            setFloatParameter(module, "softness", emitter.softness);
            setFloatParameter(module, "meshSoftness", emitter.runtime.meshSoftness);
            setFloatParameter(module, "lightingInfluence", emitter.runtime.lightingInfluence);
            setBoolParameter(module, "frustumCulling", emitter.runtime.frustumCulling);
            setBoolParameter(module, "distanceCulling", emitter.runtime.distanceCulling);
            setBoolParameter(module, "simulateWhenCulled", emitter.runtime.simulateWhenCulled);
            setFloatParameter(module, "cullPadding", emitter.runtime.cullPadding);
            setFloatParameter(module, "maxDrawDistance", emitter.runtime.maxDrawDistance);
            setFloatParameter(module, "lodNearDistance", emitter.runtime.lodNearDistance);
            setFloatParameter(module, "lodFarDistance", emitter.runtime.lodFarDistance);
            setFloatParameter(module, "lodMinSpawnScale", emitter.runtime.lodMinSpawnScale);
            setFloatParameter(module, "lodMinRenderScale", emitter.runtime.lodMinRenderScale);
            setFloatParameter(module, "velocityStretch", emitter.runtime.velocityStretch);
            setFloatParameter(module, "velocityStretchMax", emitter.runtime.velocityStretchMax);
            setFloatParameter(module, "meshScaleX", emitter.meshScale.x);
            setFloatParameter(module, "meshScaleY", emitter.meshScale.y);
            setFloatParameter(module, "meshScaleZ", emitter.meshScale.z);
        }
        else if (definition.typeId == "bursts.basic")
        {
            setBurstTimelineParameter(module, "bursts", emitter.bursts);
        }

        return module;
    }

    inline std::vector<ParticleModuleInstance>
    buildModulesFromEmitterDescriptor(const ParticleEmitterComponent& emitter)
    {
        std::vector<ParticleModuleInstance> modules;
        const auto&                         definitions = particleModuleDefinitions();
        modules.reserve(definitions.size());
        for (const ParticleModuleDefinition& definition : definitions)
            modules.push_back(moduleFromDescriptor(definition, emitter));
        return modules;
    }

    inline void ensureSizeCurve(ParticleEmitterComponent& emitter)
    {
        if (emitter.sizeOverLifetime.empty())
        {
            emitter.sizeOverLifetime.push_back({0.0f, 0.35f});
            emitter.sizeOverLifetime.push_back({1.0f, 0.75f});
            return;
        }
        if (emitter.sizeOverLifetime.size() == 1)
            emitter.sizeOverLifetime.push_back({1.0f, emitter.sizeOverLifetime.front().value});
    }

    inline void ensureAlphaCurve(ParticleEmitterComponent& emitter)
    {
        if (emitter.alphaOverLifetime.empty())
            emitter.alphaOverLifetime = {{0.0f, 0.0f}, {0.2f, 1.0f}, {1.0f, 0.0f}};
    }

    inline void completeModuleParameters(ParticleModuleInstance& module)
    {
        const ParticleModuleDefinition* definition = findParticleModuleDefinition(module.typeId);
        if (definition == nullptr)
            return;

        const uint32_t sourceVersion =
            module.version == 0 ? V1ParticleModuleSchemaVersion : module.version;
        if (module.stableId.empty())
            module.stableId = definition->category;
        if (module.displayName.empty())
            module.displayName = definition->displayName;

        std::vector<ParticleModuleParameter> migratedParameters;
        migratedParameters.reserve(definition->parameters.size());
        for (const ParticleModuleParameterDefinition& parameterDefinition : definition->parameters)
        {
            ParticleModuleParameter* parameter = findParameter(module, parameterDefinition.id);
            if (parameter == nullptr)
            {
                ParticleModuleParameter migrated = makeDefaultParameter(parameterDefinition);
                if (sourceVersion < CurrentParticleModuleSchemaVersion && module.typeId == "color.basic" &&
                    parameterDefinition.id == "colorOverLifetime")
                {
                    migrated.colorGradientValue = {{0.0f,
                                                    {floatParameter(module, "baseTintR", 1.0f),
                                                     floatParameter(module, "baseTintG", 1.0f),
                                                     floatParameter(module, "baseTintB", 1.0f),
                                                     floatParameter(module, "baseTintA", 1.0f)}},
                                                   {1.0f,
                                                    {floatParameter(module, "baseTintR", 1.0f),
                                                     floatParameter(module, "baseTintG", 1.0f),
                                                     floatParameter(module, "baseTintB", 1.0f),
                                                     floatParameter(module, "baseTintA", 1.0f)}}};
                }
                else if (sourceVersion < CurrentParticleModuleSchemaVersion && module.typeId == "color.basic" &&
                         parameterDefinition.id == "alphaOverLifetime")
                {
                    const float alphaPeak    = std::clamp(floatParameter(module, "alphaPeak", 1.0f), 0.0f, 1.0f);
                    migrated.floatCurveValue = {{0.0f, 0.0f}, {0.2f, alphaPeak}, {0.8f, alphaPeak}, {1.0f, 0.0f}};
                }
                else if (sourceVersion < CurrentParticleModuleSchemaVersion && module.typeId == "size.basic" &&
                         parameterDefinition.id == "sizeOverLifetime")
                {
                    migrated.floatCurveValue = {{0.0f, std::max(0.001f, floatParameter(module, "sizeStart", 0.35f))},
                                                {1.0f, std::max(0.001f, floatParameter(module, "sizeEnd", 0.75f))}};
                }
                else if (sourceVersion < CurrentParticleModuleSchemaVersion && module.typeId == "bursts.basic" &&
                         parameterDefinition.id == "bursts")
                {
                    if (boolParameter(module, "burstEnabled", false))
                    {
                        ParticleBurst burst;
                        burst.time     = std::max(0.0f, floatParameter(module, "burstTime", 0.0f));
                        burst.countMin = uintParameter(module, "burstCountMin", 0u);
                        burst.countMax =
                            std::max(burst.countMin, uintParameter(module, "burstCountMax", burst.countMin));
                        burst.repeatInterval = std::max(0.0f, floatParameter(module, "repeatInterval", 0.0f));
                        burst.repeatCount    = uintParameter(module, "repeatCount", 0u);
                        migrated.burstTimelineValue.push_back(burst);
                    }
                }
                migratedParameters.push_back(std::move(migrated));
                continue;
            }

            parameter->type = parameterDefinition.type;
            migratedParameters.push_back(*parameter);
        }

        module.parameters = std::move(migratedParameters);
        if (module.version <= definition->version)
            module.version = definition->version;
    }

    inline ParticleModuleInstance effectiveModule(const ParticleModuleInstance& module)
    {
        const ParticleModuleDefinition* definition = findParticleModuleDefinition(module.typeId);
        if (definition == nullptr || module.enabled)
            return module;
        return makeDefaultModuleInstance(*definition);
    }

    inline void applyParticleModulesToEmitterDescriptor(const std::vector<ParticleModuleInstance>& modules,
                                                        ParticleEmitterComponent&                  emitter)
    {
        const std::vector<const ParticleModuleInstance*> executionPlan = buildParticleModuleExecutionPlan(modules);
        for (const ParticleModuleInstance* inputModule : executionPlan)
        {
            if (inputModule == nullptr)
                continue;

            const ParticleModuleInstance module = effectiveModule(*inputModule);
            if (module.typeId == "spawn.basic")
            {
                emitter.enabled      = boolParameter(module, "emitterEnabled", emitter.enabled);
                emitter.emissionRate = std::max(0.0f, floatParameter(module, "emissionRate", emitter.emissionRate));
                emitter.maxParticles = std::max(1u, uintParameter(module, "maxParticles", emitter.maxParticles));
                emitter.intensity    = std::max(0.0f, floatParameter(module, "intensity", emitter.intensity));
                emitter.runtime.effectScale =
                    std::max(0.0f, floatParameter(module, "effectScale", emitter.runtime.effectScale));
                emitter.runtime.importance =
                    std::max(0.0f, floatParameter(module, "importance", emitter.runtime.importance));
                emitter.runtime.budgetWeight =
                    std::max(1u, uintParameter(module, "budgetWeight", emitter.runtime.budgetWeight));
                emitter.runtime.maxSpawnPerFrame =
                    uintParameter(module, "maxSpawnPerFrame", emitter.runtime.maxSpawnPerFrame);
            }
            else if (module.typeId == "lifetime.basic")
            {
                emitter.looping     = boolParameter(module, "looping", emitter.looping);
                emitter.lifetimeMin = std::max(0.01f, floatParameter(module, "lifetimeMin", emitter.lifetimeMin));
                emitter.lifetimeMax =
                    std::max(emitter.lifetimeMin, floatParameter(module, "lifetimeMax", emitter.lifetimeMax));
                emitter.duration   = std::max(0.0f, floatParameter(module, "duration", emitter.duration));
                emitter.startDelay = std::max(0.0f, floatParameter(module, "startDelay", emitter.startDelay));
            }
            else if (module.typeId == "shape.basic")
            {
                emitter.shape = static_cast<ParticleEmitterShape>(
                    std::min<uint32_t>(uintParameter(module, "shape", static_cast<uint32_t>(emitter.shape)),
                                       static_cast<uint32_t>(ParticleEmitterShape::Ring)));
                emitter.sphereRadius = std::max(0.001f, floatParameter(module, "sphereRadius", emitter.sphereRadius));
                emitter.boxExtents   = {
                    std::max(0.001f, floatParameter(module, "boxX", emitter.boxExtents.x)),
                    std::max(0.001f, floatParameter(module, "boxY", emitter.boxExtents.y)),
                    std::max(0.001f, floatParameter(module, "boxZ", emitter.boxExtents.z)),
                };
                emitter.discRadius = std::max(0.001f, floatParameter(module, "discRadius", emitter.discRadius));
                emitter.ringInnerRadius =
                    std::max(0.0f, floatParameter(module, "ringInnerRadius", emitter.ringInnerRadius));
                emitter.ringOuterRadius = std::max(emitter.ringInnerRadius,
                                                   floatParameter(module, "ringOuterRadius", emitter.ringOuterRadius));
                emitter.cylinderRadius =
                    std::max(0.001f, floatParameter(module, "cylinderRadius", emitter.cylinderRadius));
                emitter.cylinderHeight =
                    std::max(0.001f, floatParameter(module, "cylinderHeight", emitter.cylinderHeight));
            }
            else if (module.typeId == "velocity.basic")
            {
                emitter.initialVelocity = {floatParameter(module, "initialVelocityX", emitter.initialVelocity.x),
                                           floatParameter(module, "initialVelocityY", emitter.initialVelocity.y),
                                           floatParameter(module, "initialVelocityZ", emitter.initialVelocity.z)};
                emitter.velocitySpread =
                    std::max(0.0f, floatParameter(module, "velocitySpread", emitter.velocitySpread));
                emitter.radialVelocityMin = floatParameter(module, "radialVelocityMin", emitter.radialVelocityMin);
                emitter.radialVelocityMax = std::max(
                    emitter.radialVelocityMin, floatParameter(module, "radialVelocityMax", emitter.radialVelocityMax));
                emitter.tangentVelocity = floatParameter(module, "tangentVelocity", emitter.tangentVelocity);
                emitter.drag            = std::max(0.0f, floatParameter(module, "drag", emitter.drag));
            }
            else if (module.typeId == "forces.basic")
            {
                emitter.forces.acceleration = {floatParameter(module, "accelerationX", emitter.forces.acceleration.x),
                                               floatParameter(module, "accelerationY", emitter.forces.acceleration.y),
                                               floatParameter(module, "accelerationZ", emitter.forces.acceleration.z)};
                emitter.forces.wind         = {floatParameter(module, "windX", emitter.forces.wind.x),
                                               floatParameter(module, "windY", emitter.forces.wind.y),
                                               floatParameter(module, "windZ", emitter.forces.wind.z)};
                emitter.forces.vortex       = floatParameter(module, "vortex", emitter.forces.vortex);
                emitter.forces.radial       = floatParameter(module, "radial", emitter.forces.radial);
                emitter.forces.noiseStrength =
                    std::max(0.0f, floatParameter(module, "noiseStrength", emitter.forces.noiseStrength));
                emitter.forces.noiseScale =
                    std::max(0.001f, floatParameter(module, "noiseScale", emitter.forces.noiseScale));
                emitter.collision.mode = static_cast<ParticleCollisionMode>(
                    std::min<uint32_t>(uintParameter(module,
                                                     "collisionMode",
                                                     static_cast<uint32_t>(emitter.collision.mode)),
                                       static_cast<uint32_t>(ParticleCollisionMode::GroundPlane)));
                emitter.collision.groundY = floatParameter(module, "collisionGroundY", emitter.collision.groundY);
                emitter.collision.bounce =
                    std::max(0.0f, floatParameter(module, "collisionBounce", emitter.collision.bounce));
                emitter.collision.damping =
                    glm::clamp(floatParameter(module, "collisionDamping", emitter.collision.damping), 0.0f, 1.0f);
                emitter.collision.killOnCollision =
                    boolParameter(module, "killOnCollision", emitter.collision.killOnCollision);
                emitter.collision.spawnOnDeathCount =
                    uintParameter(module, "spawnOnDeathCount", emitter.collision.spawnOnDeathCount);
                emitter.collision.spawnOnCollisionCount =
                    uintParameter(module, "spawnOnCollisionCount", emitter.collision.spawnOnCollisionCount);
                emitter.collision.maxEventSpawnsPerFrame =
                    uintParameter(module, "maxEventSpawnsPerFrame", emitter.collision.maxEventSpawnsPerFrame);
            }
            else if (module.typeId == "color.basic")
            {
                emitter.baseTint = {std::clamp(floatParameter(module, "baseTintR", emitter.baseTint.r), 0.0f, 1.0f),
                                    std::clamp(floatParameter(module, "baseTintG", emitter.baseTint.g), 0.0f, 1.0f),
                                    std::clamp(floatParameter(module, "baseTintB", emitter.baseTint.b), 0.0f, 1.0f),
                                    std::clamp(floatParameter(module, "baseTintA", emitter.baseTint.a), 0.0f, 1.0f)};
                ParticleColorCurve colorCurve =
                    colorGradientParameter(module, "colorOverLifetime", emitter.colorOverLifetime);
                if (colorCurve.empty())
                    colorCurve = {{0.0f, emitter.baseTint}, {1.0f, emitter.baseTint}};
                std::sort(colorCurve.begin(),
                          colorCurve.end(),
                          [](const ParticleColorKey& lhs, const ParticleColorKey& rhs)
                          {
                              return lhs.t < rhs.t;
                          });
                emitter.colorOverLifetime = std::move(colorCurve);

                emitter.hueVariation = std::max(0.0f, floatParameter(module, "hueVariation", emitter.hueVariation));
                emitter.valueVariation =
                    std::max(0.0f, floatParameter(module, "valueVariation", emitter.valueVariation));

                ParticleFloatCurve alphaCurve =
                    floatCurveParameter(module, "alphaOverLifetime", emitter.alphaOverLifetime);
                if (alphaCurve.empty())
                    alphaCurve = {{0.0f, 0.0f}, {0.2f, 1.0f}, {0.8f, 1.0f}, {1.0f, 0.0f}};
                for (ParticleFloatKey& key : alphaCurve)
                    key.value = std::clamp(key.value, 0.0f, 1.0f);
                std::sort(alphaCurve.begin(),
                          alphaCurve.end(),
                          [](const ParticleFloatKey& lhs, const ParticleFloatKey& rhs)
                          {
                              return lhs.t < rhs.t;
                          });
                emitter.alphaOverLifetime = std::move(alphaCurve);
            }
            else if (module.typeId == "size.basic")
            {
                ParticleFloatCurve sizeCurve =
                    floatCurveParameter(module, "sizeOverLifetime", emitter.sizeOverLifetime);
                if (sizeCurve.empty())
                    sizeCurve = {{0.0f, 0.35f}, {1.0f, 0.75f}};
                for (ParticleFloatKey& key : sizeCurve)
                    key.value = std::max(0.001f, key.value);
                std::sort(sizeCurve.begin(),
                          sizeCurve.end(),
                          [](const ParticleFloatKey& lhs, const ParticleFloatKey& rhs)
                          {
                              return lhs.t < rhs.t;
                          });
                emitter.sizeOverLifetime = std::move(sizeCurve);
                emitter.sizeRandomness =
                    std::max(0.0f, floatParameter(module, "sizeRandomness", emitter.sizeRandomness));
                emitter.aspectRatioMin =
                    std::max(0.01f, floatParameter(module, "aspectRatioMin", emitter.aspectRatioMin));
                emitter.aspectRatioMax =
                    std::max(emitter.aspectRatioMin, floatParameter(module, "aspectRatioMax", emitter.aspectRatioMax));
            }
            else if (module.typeId == "rotation.basic")
            {
                emitter.spinMin                = floatParameter(module, "spinMin", emitter.spinMin);
                emitter.spinMax                = floatParameter(module, "spinMax", emitter.spinMax);
                emitter.meshAngularVelocityMin = {
                    floatParameter(module, "meshSpinXMin", emitter.meshAngularVelocityMin.x),
                    floatParameter(module, "meshSpinYMin", emitter.meshAngularVelocityMin.y),
                    floatParameter(module, "meshSpinZMin", emitter.meshAngularVelocityMin.z)};
                emitter.meshAngularVelocityMax = {
                    floatParameter(module, "meshSpinXMax", emitter.meshAngularVelocityMax.x),
                    floatParameter(module, "meshSpinYMax", emitter.meshAngularVelocityMax.y),
                    floatParameter(module, "meshSpinZMax", emitter.meshAngularVelocityMax.z)};
                emitter.randomMeshRotation = boolParameter(module, "randomMeshRotation", emitter.randomMeshRotation);
            }
            else if (module.typeId == "renderer.basic")
            {
                emitter.primitive = static_cast<ParticlePrimitive>(
                    std::min<uint32_t>(uintParameter(module, "primitive", static_cast<uint32_t>(emitter.primitive)),
                                       static_cast<uint32_t>(ParticlePrimitive::Mesh)));
                emitter.blend = static_cast<ParticleBlendMode>(
                    std::min<uint32_t>(uintParameter(module, "blend", static_cast<uint32_t>(emitter.blend)),
                                       static_cast<uint32_t>(ParticleBlendMode::Additive)));
                emitter.spriteShape = static_cast<ParticleSpriteShape>(
                    std::min<uint32_t>(uintParameter(module, "spriteShape", static_cast<uint32_t>(emitter.spriteShape)),
                                       static_cast<uint32_t>(ParticleSpriteShape::Streak)));
                emitter.texturePath = stringParameter(module, "texturePath", emitter.texturePath);
                emitter.meshPath    = stringParameter(module, "meshPath", emitter.meshPath);
                emitter.materialPath = stringParameter(module, "materialPath", emitter.materialPath);
                emitter.spriteEdgeSoftness =
                    std::max(0.0f, floatParameter(module, "spriteEdgeSoftness", emitter.spriteEdgeSoftness));
                emitter.softness  = std::max(0.0f, floatParameter(module, "softness", emitter.softness));
                emitter.runtime.meshSoftness =
                    std::max(0.0f, floatParameter(module, "meshSoftness", emitter.runtime.meshSoftness));
                emitter.runtime.lightingInfluence =
                    glm::clamp(floatParameter(module, "lightingInfluence", emitter.runtime.lightingInfluence),
                               0.0f,
                               1.0f);
                emitter.runtime.frustumCulling =
                    boolParameter(module, "frustumCulling", emitter.runtime.frustumCulling);
                emitter.runtime.distanceCulling =
                    boolParameter(module, "distanceCulling", emitter.runtime.distanceCulling);
                emitter.runtime.simulateWhenCulled =
                    boolParameter(module, "simulateWhenCulled", emitter.runtime.simulateWhenCulled);
                emitter.runtime.cullPadding =
                    std::max(0.0f, floatParameter(module, "cullPadding", emitter.runtime.cullPadding));
                emitter.runtime.maxDrawDistance =
                    std::max(0.0f, floatParameter(module, "maxDrawDistance", emitter.runtime.maxDrawDistance));
                emitter.runtime.lodNearDistance =
                    std::max(0.0f, floatParameter(module, "lodNearDistance", emitter.runtime.lodNearDistance));
                emitter.runtime.lodFarDistance =
                    std::max(emitter.runtime.lodNearDistance,
                             floatParameter(module, "lodFarDistance", emitter.runtime.lodFarDistance));
                emitter.runtime.lodMinSpawnScale =
                    glm::clamp(floatParameter(module,
                                              "lodMinSpawnScale",
                                              emitter.runtime.lodMinSpawnScale),
                               0.0f,
                               1.0f);
                emitter.runtime.lodMinRenderScale =
                    glm::clamp(floatParameter(module,
                                              "lodMinRenderScale",
                                              emitter.runtime.lodMinRenderScale),
                               0.0f,
                               1.0f);
                emitter.runtime.velocityStretch =
                    std::max(0.0f, floatParameter(module, "velocityStretch", emitter.runtime.velocityStretch));
                emitter.runtime.velocityStretchMax =
                    std::max(0.0f, floatParameter(module, "velocityStretchMax", emitter.runtime.velocityStretchMax));
                emitter.meshScale = {std::max(0.001f, floatParameter(module, "meshScaleX", emitter.meshScale.x)),
                                     std::max(0.001f, floatParameter(module, "meshScaleY", emitter.meshScale.y)),
                                     std::max(0.001f, floatParameter(module, "meshScaleZ", emitter.meshScale.z))};
            }
            else if (module.typeId == "bursts.basic")
            {
                std::vector<ParticleBurst> bursts = burstTimelineParameter(module, "bursts", emitter.bursts);
                for (ParticleBurst& burst : bursts)
                {
                    burst.time           = std::max(0.0f, burst.time);
                    burst.countMax       = std::max(burst.countMin, burst.countMax);
                    burst.repeatInterval = std::max(0.0f, burst.repeatInterval);
                }
                std::sort(bursts.begin(),
                          bursts.end(),
                          [](const ParticleBurst& lhs, const ParticleBurst& rhs)
                          {
                              return lhs.time < rhs.time;
                          });
                emitter.bursts = std::move(bursts);
            }
        }
    }

    inline void migrateParticleEmitterModules(std::vector<ParticleModuleInstance>& modules,
                                              ParticleEmitterComponent&            descriptor)
    {
        if (modules.empty())
        {
            modules = buildModulesFromEmitterDescriptor(descriptor);
            return;
        }

        for (ParticleModuleInstance& module : modules)
            completeModuleParameters(module);

        for (const ParticleModuleDefinition& definition : particleModuleDefinitions())
        {
            const auto it = std::find_if(modules.begin(),
                                         modules.end(),
                                         [&](const ParticleModuleInstance& module)
                                         {
                                             return module.typeId == definition.typeId;
                                         });
            if (it == modules.end())
                modules.push_back(moduleFromDescriptor(definition, descriptor));
        }

        applyParticleModulesToEmitterDescriptor(modules, descriptor);
    }
} // namespace gts::particles
