#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "GlmConfig.h"
#include "ParticleEmitterComponent.h"
#include "ParticleTypes.h"

namespace gts::particles
{
    inline constexpr uint32_t CurrentParticleModuleSchemaVersion = 1u;

    enum class ParticleModuleParameterType
    {
        Float,
        UInt,
        Bool,
        Enum,
        String
    };

    struct ParticleModuleEnumOption
    {
        uint32_t    value = 0;
        std::string label;
    };

    struct ParticleModuleParameterDefinition
    {
        std::string                            id;
        std::string                            label;
        ParticleModuleParameterType            type = ParticleModuleParameterType::Float;
        float                                  minValue = 0.0f;
        float                                  maxValue = 1.0f;
        float                                  defaultFloat = 0.0f;
        uint32_t                               defaultUInt = 0;
        bool                                   defaultBool = false;
        std::string                            defaultString;
        bool                                   wholeNumber = false;
        std::vector<ParticleModuleEnumOption>  enumOptions;
    };

    struct ParticleModuleDefinition
    {
        std::string                                      typeId;
        std::string                                      category;
        std::string                                      displayName;
        std::string                                      description;
        uint32_t                                         version = CurrentParticleModuleSchemaVersion;
        std::vector<ParticleModuleParameterDefinition>   parameters;
    };

    struct ParticleModuleParameter
    {
        std::string                 id;
        ParticleModuleParameterType type = ParticleModuleParameterType::Float;
        float                       floatValue = 0.0f;
        uint32_t                    uintValue = 0;
        bool                        boolValue = false;
        std::string                 stringValue;
    };

    struct ParticleModuleInstance
    {
        std::string                           stableId;
        std::string                           typeId;
        std::string                           displayName;
        uint32_t                              version = CurrentParticleModuleSchemaVersion;
        bool                                  enabled = true;
        std::vector<ParticleModuleParameter>  parameters;
    };

    inline ParticleModuleParameterDefinition floatParam(const std::string& id,
                                                        const std::string& label,
                                                        float              minValue,
                                                        float              maxValue,
                                                        float              defaultValue)
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

    inline ParticleModuleParameterDefinition uintParam(const std::string& id,
                                                       const std::string& label,
                                                       uint32_t           minValue,
                                                       uint32_t           maxValue,
                                                       uint32_t           defaultValue)
    {
        ParticleModuleParameterDefinition param;
        param.id           = id;
        param.label        = label;
        param.type         = ParticleModuleParameterType::UInt;
        param.minValue     = static_cast<float>(minValue);
        param.maxValue     = static_cast<float>(maxValue);
        param.defaultUInt  = defaultValue;
        param.wholeNumber  = true;
        return param;
    }

    inline ParticleModuleParameterDefinition boolParam(const std::string& id,
                                                       const std::string& label,
                                                       bool               defaultValue)
    {
        ParticleModuleParameterDefinition param;
        param.id          = id;
        param.label       = label;
        param.type        = ParticleModuleParameterType::Bool;
        param.defaultBool = defaultValue;
        return param;
    }

    inline ParticleModuleParameterDefinition enumParam(const std::string&                            id,
                                                       const std::string&                            label,
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

    inline ParticleModuleParameterDefinition stringParam(const std::string& id,
                                                         const std::string& label,
                                                         const std::string& defaultValue = {})
    {
        ParticleModuleParameterDefinition param;
        param.id            = id;
        param.label         = label;
        param.type          = ParticleModuleParameterType::String;
        param.defaultString = defaultValue;
        return param;
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
              floatParam("intensity", "INTENSITY", 0.0f, 2.5f, 1.0f)}},
            {"lifetime.basic",
             "Lifetime",
             "Lifetime",
             "Particle lifetime and emitter timing.",
             CurrentParticleModuleSchemaVersion,
             {boolParam("looping", "LOOP", true),
              floatParam("lifetimeMin", "LIFE MIN", 0.01f, 6.0f, 1.0f),
              floatParam("lifetimeMax", "LIFE MAX", 0.01f, 8.0f, 2.0f),
              floatParam("duration", "DURATION", 0.0f, 12.0f, 0.0f),
              floatParam("startDelay", "DELAY", 0.0f, 5.0f, 0.0f)}},
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
              floatParam("cylinderHeight", "CYL H", 0.01f, 5.0f, 1.0f)}},
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
              floatParam("drag", "DRAG", 0.0f, 3.0f, 0.15f)}},
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
              floatParam("noiseScale", "NOISE SC", 0.01f, 8.0f, 1.0f)}},
            {"color.basic",
             "Color",
             "Color",
             "Tint, value variation, and alpha peak.",
             CurrentParticleModuleSchemaVersion,
             {floatParam("baseTintR", "TINT R", 0.0f, 1.0f, 1.0f),
              floatParam("baseTintG", "TINT G", 0.0f, 1.0f, 1.0f),
              floatParam("baseTintB", "TINT B", 0.0f, 1.0f, 1.0f),
              floatParam("baseTintA", "TINT A", 0.0f, 1.0f, 1.0f),
              floatParam("hueVariation", "HUE VAR", 0.0f, 1.0f, 0.0f),
              floatParam("valueVariation", "VAL VAR", 0.0f, 1.0f, 0.0f),
              floatParam("alphaPeak", "ALPHA", 0.0f, 1.0f, 1.0f)}},
            {"size.basic",
             "Size",
             "Size",
             "Size curve endpoints and billboard aspect ratio.",
             CurrentParticleModuleSchemaVersion,
             {floatParam("sizeStart", "SIZE IN", 0.001f, 2.0f, 0.35f),
              floatParam("sizeEnd", "SIZE OUT", 0.001f, 2.0f, 0.75f),
              floatParam("sizeRandomness", "SIZE RNG", 0.0f, 1.0f, 0.15f),
              floatParam("aspectRatioMin", "ASPECT MIN", 0.1f, 6.0f, 1.0f),
              floatParam("aspectRatioMax", "ASPECT MAX", 0.1f, 6.0f, 1.0f)}},
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
              boolParam("randomMeshRotation", "RANDOM MESH", true)}},
            {"renderer.basic",
             "Renderer",
             "Renderer",
             "Particle primitive, blend, sprite mask, and render material paths.",
             CurrentParticleModuleSchemaVersion,
             {enumParam("primitive", "PRIMITIVE", static_cast<uint32_t>(ParticlePrimitive::Billboard), primitiveOptions()),
              enumParam("blend", "BLEND", static_cast<uint32_t>(ParticleBlendMode::Alpha), blendOptions()),
              enumParam("spriteShape",
                        "SPRITE",
                        static_cast<uint32_t>(ParticleSpriteShape::SoftCircle),
                        spriteShapeOptions()),
              stringParam("texturePath", "TEXTURE"),
              stringParam("meshPath", "MESH"),
              floatParam("spriteEdgeSoftness", "EDGE", 0.0f, 1.0f, 1.0f),
              floatParam("softness", "SOFT", 0.0f, 240.0f, 80.0f),
              floatParam("meshScaleX", "MESH X", 0.01f, 6.0f, 1.0f),
              floatParam("meshScaleY", "MESH Y", 0.01f, 6.0f, 1.0f),
              floatParam("meshScaleZ", "MESH Z", 0.01f, 6.0f, 1.0f)}},
            {"bursts.basic",
             "Bursts",
             "Bursts",
             "First burst authoring bridge for the current runtime burst list.",
             CurrentParticleModuleSchemaVersion,
             {boolParam("burstEnabled", "BURST ON", false),
              floatParam("burstTime", "TIME", 0.0f, 12.0f, 0.0f),
              uintParam("burstCountMin", "COUNT MIN", 0u, 512u, 0u),
              uintParam("burstCountMax", "COUNT MAX", 0u, 512u, 0u),
              floatParam("repeatInterval", "REPEAT", 0.0f, 12.0f, 0.0f),
              uintParam("repeatCount", "REPEATS", 0u, 64u, 0u)}},
        };
        return definitions;
    }

    inline const ParticleModuleDefinition* findParticleModuleDefinition(const std::string& typeId)
    {
        const auto& definitions = particleModuleDefinitions();
        const auto  it = std::find_if(definitions.begin(),
                                     definitions.end(),
                                     [&](const ParticleModuleDefinition& definition)
                                     {
                                         return definition.typeId == typeId;
                                     });
        return it == definitions.end() ? nullptr : &*it;
    }

    inline ParticleModuleParameter makeDefaultParameter(const ParticleModuleParameterDefinition& definition)
    {
        ParticleModuleParameter parameter;
        parameter.id          = definition.id;
        parameter.type        = definition.type;
        parameter.floatValue  = definition.defaultFloat;
        parameter.uintValue   = definition.defaultUInt;
        parameter.boolValue   = definition.defaultBool;
        parameter.stringValue = definition.defaultString;
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

    inline float floatParameter(const ParticleModuleInstance& module,
                                const std::string&           id,
                                float                        fallback = 0.0f)
    {
        const ParticleModuleParameter* parameter = findParameter(module, id);
        return parameter == nullptr ? fallback : parameter->floatValue;
    }

    inline uint32_t uintParameter(const ParticleModuleInstance& module,
                                  const std::string&           id,
                                  uint32_t                     fallback = 0u)
    {
        const ParticleModuleParameter* parameter = findParameter(module, id);
        return parameter == nullptr ? fallback : parameter->uintValue;
    }

    inline bool boolParameter(const ParticleModuleInstance& module, const std::string& id, bool fallback = false)
    {
        const ParticleModuleParameter* parameter = findParameter(module, id);
        return parameter == nullptr ? fallback : parameter->boolValue;
    }

    inline std::string stringParameter(const ParticleModuleInstance& module,
                                       const std::string&           id,
                                       const std::string&           fallback = {})
    {
        const ParticleModuleParameter* parameter = findParameter(module, id);
        return parameter == nullptr ? fallback : parameter->stringValue;
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
        }
        else if (definition.typeId == "color.basic")
        {
            setFloatParameter(module, "baseTintR", emitter.baseTint.r);
            setFloatParameter(module, "baseTintG", emitter.baseTint.g);
            setFloatParameter(module, "baseTintB", emitter.baseTint.b);
            setFloatParameter(module, "baseTintA", emitter.baseTint.a);
            setFloatParameter(module, "hueVariation", emitter.hueVariation);
            setFloatParameter(module, "valueVariation", emitter.valueVariation);
            float alphaPeak = 0.0f;
            for (const ParticleFloatKey& key : emitter.alphaOverLifetime)
                alphaPeak = std::max(alphaPeak, key.value);
            setFloatParameter(module, "alphaPeak", alphaPeak);
        }
        else if (definition.typeId == "size.basic")
        {
            setFloatParameter(module,
                              "sizeStart",
                              emitter.sizeOverLifetime.empty() ? 0.35f : emitter.sizeOverLifetime.front().value);
            setFloatParameter(module,
                              "sizeEnd",
                              emitter.sizeOverLifetime.empty() ? 0.75f : emitter.sizeOverLifetime.back().value);
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
            setFloatParameter(module, "spriteEdgeSoftness", emitter.spriteEdgeSoftness);
            setFloatParameter(module, "softness", emitter.softness);
            setFloatParameter(module, "meshScaleX", emitter.meshScale.x);
            setFloatParameter(module, "meshScaleY", emitter.meshScale.y);
            setFloatParameter(module, "meshScaleZ", emitter.meshScale.z);
        }
        else if (definition.typeId == "bursts.basic")
        {
            const bool hasBurst = !emitter.bursts.empty();
            setBoolParameter(module, "burstEnabled", hasBurst);
            if (hasBurst)
            {
                const ParticleBurst& burst = emitter.bursts.front();
                setFloatParameter(module, "burstTime", burst.time);
                setUIntParameter(module, "burstCountMin", burst.countMin);
                setUIntParameter(module, "burstCountMax", burst.countMax);
                setFloatParameter(module, "repeatInterval", burst.repeatInterval);
                setUIntParameter(module, "repeatCount", burst.repeatCount);
            }
        }

        return module;
    }

    inline std::vector<ParticleModuleInstance> buildModulesFromEmitterDescriptor(
        const ParticleEmitterComponent& emitter)
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

        if (module.stableId.empty())
            module.stableId = definition->category;
        if (module.displayName.empty())
            module.displayName = definition->displayName;
        if (module.version == 0)
            module.version = definition->version;

        for (const ParticleModuleParameterDefinition& parameterDefinition : definition->parameters)
        {
            ParticleModuleParameter* parameter = findParameter(module, parameterDefinition.id);
            if (parameter == nullptr)
            {
                module.parameters.push_back(makeDefaultParameter(parameterDefinition));
                continue;
            }

            parameter->type = parameterDefinition.type;
        }
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
        for (const ParticleModuleInstance& inputModule : modules)
        {
            const ParticleModuleInstance module = effectiveModule(inputModule);
            if (module.typeId == "spawn.basic")
            {
                emitter.enabled      = boolParameter(module, "emitterEnabled", emitter.enabled);
                emitter.emissionRate = std::max(0.0f, floatParameter(module, "emissionRate", emitter.emissionRate));
                emitter.maxParticles = std::max(1u, uintParameter(module, "maxParticles", emitter.maxParticles));
                emitter.intensity    = std::max(0.0f, floatParameter(module, "intensity", emitter.intensity));
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
                emitter.boxExtents = {
                    std::max(0.001f, floatParameter(module, "boxX", emitter.boxExtents.x)),
                    std::max(0.001f, floatParameter(module, "boxY", emitter.boxExtents.y)),
                    std::max(0.001f, floatParameter(module, "boxZ", emitter.boxExtents.z)),
                };
                emitter.discRadius       = std::max(0.001f, floatParameter(module, "discRadius", emitter.discRadius));
                emitter.ringInnerRadius  = std::max(0.0f, floatParameter(module, "ringInnerRadius", emitter.ringInnerRadius));
                emitter.ringOuterRadius  = std::max(emitter.ringInnerRadius,
                                                   floatParameter(module, "ringOuterRadius", emitter.ringOuterRadius));
                emitter.cylinderRadius   =
                    std::max(0.001f, floatParameter(module, "cylinderRadius", emitter.cylinderRadius));
                emitter.cylinderHeight   =
                    std::max(0.001f, floatParameter(module, "cylinderHeight", emitter.cylinderHeight));
            }
            else if (module.typeId == "velocity.basic")
            {
                emitter.initialVelocity = {floatParameter(module, "initialVelocityX", emitter.initialVelocity.x),
                                           floatParameter(module, "initialVelocityY", emitter.initialVelocity.y),
                                           floatParameter(module, "initialVelocityZ", emitter.initialVelocity.z)};
                emitter.velocitySpread    =
                    std::max(0.0f, floatParameter(module, "velocitySpread", emitter.velocitySpread));
                emitter.radialVelocityMin = floatParameter(module, "radialVelocityMin", emitter.radialVelocityMin);
                emitter.radialVelocityMax =
                    std::max(emitter.radialVelocityMin,
                             floatParameter(module, "radialVelocityMax", emitter.radialVelocityMax));
                emitter.tangentVelocity = floatParameter(module, "tangentVelocity", emitter.tangentVelocity);
                emitter.drag            = std::max(0.0f, floatParameter(module, "drag", emitter.drag));
            }
            else if (module.typeId == "forces.basic")
            {
                emitter.forces.acceleration = {floatParameter(module, "accelerationX", emitter.forces.acceleration.x),
                                               floatParameter(module, "accelerationY", emitter.forces.acceleration.y),
                                               floatParameter(module, "accelerationZ", emitter.forces.acceleration.z)};
                emitter.forces.wind = {floatParameter(module, "windX", emitter.forces.wind.x),
                                       floatParameter(module, "windY", emitter.forces.wind.y),
                                       floatParameter(module, "windZ", emitter.forces.wind.z)};
                emitter.forces.vortex        = floatParameter(module, "vortex", emitter.forces.vortex);
                emitter.forces.radial        = floatParameter(module, "radial", emitter.forces.radial);
                emitter.forces.noiseStrength =
                    std::max(0.0f, floatParameter(module, "noiseStrength", emitter.forces.noiseStrength));
                emitter.forces.noiseScale    =
                    std::max(0.001f, floatParameter(module, "noiseScale", emitter.forces.noiseScale));
            }
            else if (module.typeId == "color.basic")
            {
                emitter.baseTint = {std::clamp(floatParameter(module, "baseTintR", emitter.baseTint.r), 0.0f, 1.0f),
                                    std::clamp(floatParameter(module, "baseTintG", emitter.baseTint.g), 0.0f, 1.0f),
                                    std::clamp(floatParameter(module, "baseTintB", emitter.baseTint.b), 0.0f, 1.0f),
                                    std::clamp(floatParameter(module, "baseTintA", emitter.baseTint.a), 0.0f, 1.0f)};
                emitter.hueVariation   =
                    std::max(0.0f, floatParameter(module, "hueVariation", emitter.hueVariation));
                emitter.valueVariation =
                    std::max(0.0f, floatParameter(module, "valueVariation", emitter.valueVariation));

                ensureAlphaCurve(emitter);
                const float alphaPeak = std::clamp(floatParameter(module, "alphaPeak", 1.0f), 0.0f, 1.0f);
                for (ParticleFloatKey& key : emitter.alphaOverLifetime)
                {
                    if (key.t > 0.0f && key.t < 1.0f)
                        key.value = alphaPeak;
                }
            }
            else if (module.typeId == "size.basic")
            {
                ensureSizeCurve(emitter);
                emitter.sizeOverLifetime.front().value =
                    std::max(0.001f, floatParameter(module, "sizeStart", emitter.sizeOverLifetime.front().value));
                emitter.sizeOverLifetime.back().value =
                    std::max(0.001f, floatParameter(module, "sizeEnd", emitter.sizeOverLifetime.back().value));
                emitter.sizeRandomness =
                    std::max(0.0f, floatParameter(module, "sizeRandomness", emitter.sizeRandomness));
                emitter.aspectRatioMin =
                    std::max(0.01f, floatParameter(module, "aspectRatioMin", emitter.aspectRatioMin));
                emitter.aspectRatioMax =
                    std::max(emitter.aspectRatioMin,
                             floatParameter(module, "aspectRatioMax", emitter.aspectRatioMax));
            }
            else if (module.typeId == "rotation.basic")
            {
                emitter.spinMin = floatParameter(module, "spinMin", emitter.spinMin);
                emitter.spinMax = floatParameter(module, "spinMax", emitter.spinMax);
                emitter.meshAngularVelocityMin = {floatParameter(module, "meshSpinXMin", emitter.meshAngularVelocityMin.x),
                                                  floatParameter(module, "meshSpinYMin", emitter.meshAngularVelocityMin.y),
                                                  floatParameter(module, "meshSpinZMin", emitter.meshAngularVelocityMin.z)};
                emitter.meshAngularVelocityMax = {floatParameter(module, "meshSpinXMax", emitter.meshAngularVelocityMax.x),
                                                  floatParameter(module, "meshSpinYMax", emitter.meshAngularVelocityMax.y),
                                                  floatParameter(module, "meshSpinZMax", emitter.meshAngularVelocityMax.z)};
                emitter.randomMeshRotation =
                    boolParameter(module, "randomMeshRotation", emitter.randomMeshRotation);
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
                emitter.texturePath        = stringParameter(module, "texturePath", emitter.texturePath);
                emitter.meshPath           = stringParameter(module, "meshPath", emitter.meshPath);
                emitter.spriteEdgeSoftness =
                    std::max(0.0f, floatParameter(module, "spriteEdgeSoftness", emitter.spriteEdgeSoftness));
                emitter.softness = std::max(0.0f, floatParameter(module, "softness", emitter.softness));
                emitter.meshScale = {std::max(0.001f, floatParameter(module, "meshScaleX", emitter.meshScale.x)),
                                     std::max(0.001f, floatParameter(module, "meshScaleY", emitter.meshScale.y)),
                                     std::max(0.001f, floatParameter(module, "meshScaleZ", emitter.meshScale.z))};
            }
            else if (module.typeId == "bursts.basic")
            {
                const bool enabled = boolParameter(module, "burstEnabled", false);
                if (!enabled)
                {
                    emitter.bursts.clear();
                    continue;
                }

                if (emitter.bursts.empty())
                    emitter.bursts.push_back({});
                ParticleBurst& burst = emitter.bursts.front();
                burst.time           = std::max(0.0f, floatParameter(module, "burstTime", burst.time));
                burst.countMin       = uintParameter(module, "burstCountMin", burst.countMin);
                burst.countMax       = std::max(burst.countMin, uintParameter(module, "burstCountMax", burst.countMax));
                burst.repeatInterval = std::max(0.0f, floatParameter(module, "repeatInterval", burst.repeatInterval));
                burst.repeatCount    = uintParameter(module, "repeatCount", burst.repeatCount);
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
