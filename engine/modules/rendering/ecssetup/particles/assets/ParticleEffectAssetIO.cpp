#include "ParticleEffectAssetIO.h"
#include "GtsJsonParser.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

#include "ParticleGraphAuthoring.h"
#include "ParticleProgramCompiler.h"

namespace
{
    const GtsJsonValue* findTypedMemberDeep(const GtsJsonValue& source, const std::string& key, GtsJsonValue::Type type)
    {
        if (source.type() != GtsJsonValue::Type::Object)
            return nullptr;

        for (const auto& entry : source.asObject())
        {
            if (entry.first == key && entry.second.type() == type)
                return &entry.second;
        }
        for (const auto& entry : source.asObject())
        {
            if (entry.second.type() == GtsJsonValue::Type::Object)
            {
                if (const GtsJsonValue* found = findTypedMemberDeep(entry.second, key, type))
                    return found;
            }
        }
        return nullptr;
    }

    const GtsJsonValue* findTypedMember(const GtsJsonValue& source,
                                     const std::string& key,
                                     GtsJsonValue::Type    type,
                                     bool               deep = true)
    {
        const GtsJsonValue* direct = source.find(key);
        if (direct != nullptr && direct->type() == type)
            return direct;
        return deep ? findTypedMemberDeep(source, key, type) : nullptr;
    }

    bool readObject(const GtsJsonValue& source, const std::string& key, const GtsJsonValue*& objectValue)
    {
        objectValue = findTypedMember(source, key, GtsJsonValue::Type::Object, false);
        return objectValue != nullptr;
    }

    bool readObjectArray(const GtsJsonValue& source,
                         const std::string& key,
                         std::vector<const GtsJsonValue*>& objectValues)
    {
        const GtsJsonValue* arrayValue = findTypedMember(source, key, GtsJsonValue::Type::Array, false);
        if (arrayValue == nullptr)
            return false;

        std::vector<const GtsJsonValue*> parsed;
        parsed.reserve(arrayValue->asArray().size());
        for (const GtsJsonValue& item : arrayValue->asArray())
        {
            if (item.type() != GtsJsonValue::Type::Object)
                return false;
            parsed.push_back(&item);
        }

        objectValues = std::move(parsed);
        return true;
    }

    bool readString(const GtsJsonValue& source, const std::string& key, std::string& value, bool deep = true)
    {
        const GtsJsonValue* stringValue = findTypedMember(source, key, GtsJsonValue::Type::String, deep);
        if (stringValue == nullptr)
            return false;
        value = stringValue->asString();
        return true;
    }

    bool readFloat(const GtsJsonValue& source, const std::string& key, float& value, bool deep = true)
    {
        const GtsJsonValue* numberValue = findTypedMember(source, key, GtsJsonValue::Type::Number, deep);
        if (numberValue == nullptr)
            return false;
        value = static_cast<float>(numberValue->asNumber());
        return true;
    }

    bool readUint(const GtsJsonValue& source, const std::string& key, uint32_t& value, bool deep = true)
    {
        const auto* number = findTypedMember(source, key, GtsJsonValue::Type::Number, deep);
        if (number == nullptr)
            return false;
        const double parsed = number->asNumber();
        if (parsed < 0 || parsed > std::numeric_limits<uint32_t>::max() || std::trunc(parsed) != parsed)
            return false;
        value = static_cast<uint32_t>(parsed);
        return true;
    }

    bool readBool(const GtsJsonValue& source, const std::string& key, bool& value, bool deep = true)
    {
        const GtsJsonValue* boolValue = findTypedMember(source, key, GtsJsonValue::Type::Bool, deep);
        if (boolValue == nullptr)
            return false;
        value = boolValue->asBool();
        return true;
    }

    bool readVec3Value(const GtsJsonValue& source, glm::vec3& value)
    {
        if (source.type() != GtsJsonValue::Type::Array || source.asArray().size() != 3u)
            return false;
        for (const GtsJsonValue& item : source.asArray())
        {
            if (item.type() != GtsJsonValue::Type::Number)
                return false;
        }
        value = {static_cast<float>(source.asArray()[0].asNumber()),
                 static_cast<float>(source.asArray()[1].asNumber()),
                 static_cast<float>(source.asArray()[2].asNumber())};
        return true;
    }

    bool readVec2Value(const GtsJsonValue& source, glm::vec2& value)
    {
        if (source.type() != GtsJsonValue::Type::Array || source.asArray().size() != 2u)
            return false;
        for (const GtsJsonValue& item : source.asArray())
        {
            if (item.type() != GtsJsonValue::Type::Number)
                return false;
        }
        value = {static_cast<float>(source.asArray()[0].asNumber()), static_cast<float>(source.asArray()[1].asNumber())};
        return true;
    }

    bool readVec4Value(const GtsJsonValue& source, glm::vec4& value)
    {
        if (source.type() != GtsJsonValue::Type::Array || source.asArray().size() != 4u)
            return false;
        for (const GtsJsonValue& item : source.asArray())
        {
            if (item.type() != GtsJsonValue::Type::Number)
                return false;
        }
        value = {static_cast<float>(source.asArray()[0].asNumber()),
                 static_cast<float>(source.asArray()[1].asNumber()),
                 static_cast<float>(source.asArray()[2].asNumber()),
                 static_cast<float>(source.asArray()[3].asNumber())};
        return true;
    }

    bool readVec3(const GtsJsonValue& source, const std::string& key, glm::vec3& value, bool deep = true)
    {
        const GtsJsonValue* arrayValue = findTypedMember(source, key, GtsJsonValue::Type::Array, deep);
        return arrayValue != nullptr && readVec3Value(*arrayValue, value);
    }

    bool readVec2(const GtsJsonValue& source, const std::string& key, glm::vec2& value, bool deep = true)
    {
        const GtsJsonValue* arrayValue = findTypedMember(source, key, GtsJsonValue::Type::Array, deep);
        return arrayValue != nullptr && readVec2Value(*arrayValue, value);
    }

    bool readVec4(const GtsJsonValue& source, const std::string& key, glm::vec4& value, bool deep = true)
    {
        const GtsJsonValue* arrayValue = findTypedMember(source, key, GtsJsonValue::Type::Array, deep);
        return arrayValue != nullptr && readVec4Value(*arrayValue, value);
    }

    ParticleEmitterShape shapeFromString(const std::string& value)
    {
        if (value == "box")
            return ParticleEmitterShape::Box;
        if (value == "disc")
            return ParticleEmitterShape::Disc;
        if (value == "cylinder")
            return ParticleEmitterShape::Cylinder;
        if (value == "ring")
            return ParticleEmitterShape::Ring;
        return ParticleEmitterShape::Sphere;
    }

    std::string shapeToString(ParticleEmitterShape shape)
    {
        switch (shape)
        {
        case ParticleEmitterShape::Sphere:
            return "sphere";
        case ParticleEmitterShape::Box:
            return "box";
        case ParticleEmitterShape::Disc:
            return "disc";
        case ParticleEmitterShape::Cylinder:
            return "cylinder";
        case ParticleEmitterShape::Ring:
            return "ring";
        }
        return "sphere";
    }

    ParticleBlendMode blendFromString(const std::string& value)
    {
        if (value == "additive")
            return ParticleBlendMode::Additive;
        return ParticleBlendMode::Alpha;
    }

    std::string blendToString(ParticleBlendMode blend)
    {
        return blend == ParticleBlendMode::Additive ? "additive" : "alpha";
    }

    ParticlePrimitive primitiveFromString(const std::string& value)
    {
        if (value == "mesh")
            return ParticlePrimitive::Mesh;
        return ParticlePrimitive::Billboard;
    }

    std::string primitiveToString(ParticlePrimitive primitive)
    {
        return primitive == ParticlePrimitive::Mesh ? "mesh" : "billboard";
    }

    ParticleSpriteShape spriteShapeFromString(const std::string& value)
    {
        if (value == "square")
            return ParticleSpriteShape::Square;
        if (value == "diamond")
            return ParticleSpriteShape::Diamond;
        if (value == "petal")
            return ParticleSpriteShape::Petal;
        if (value == "streak")
            return ParticleSpriteShape::Streak;
        return ParticleSpriteShape::SoftCircle;
    }

    std::string spriteShapeToString(ParticleSpriteShape shape)
    {
        switch (shape)
        {
        case ParticleSpriteShape::SoftCircle:
            return "softCircle";
        case ParticleSpriteShape::Square:
            return "square";
        case ParticleSpriteShape::Diamond:
            return "diamond";
        case ParticleSpriteShape::Petal:
            return "petal";
        case ParticleSpriteShape::Streak:
            return "streak";
        }
        return "softCircle";
    }

    ParticleCollisionMode collisionModeFromString(const std::string& value)
    {
        if (value == "groundPlane")
            return ParticleCollisionMode::GroundPlane;
        return ParticleCollisionMode::None;
    }

    std::string collisionModeToString(ParticleCollisionMode mode)
    {
        return mode == ParticleCollisionMode::GroundPlane ? "groundPlane" : "none";
    }

    std::string moduleParameterTypeToString(gts::particles::ParticleModuleParameterType type)
    {
        switch (type)
        {
        case gts::particles::ParticleModuleParameterType::Float:
            return "float";
        case gts::particles::ParticleModuleParameterType::UInt:
            return "uint";
        case gts::particles::ParticleModuleParameterType::Bool:
            return "bool";
        case gts::particles::ParticleModuleParameterType::Enum:
            return "enum";
        case gts::particles::ParticleModuleParameterType::String:
            return "string";
        case gts::particles::ParticleModuleParameterType::FloatCurve:
            return "floatCurve";
        case gts::particles::ParticleModuleParameterType::ColorGradient:
            return "colorGradient";
        case gts::particles::ParticleModuleParameterType::BurstTimeline:
            return "burstTimeline";
        }
        return "float";
    }

    bool moduleParameterTypeFromString(const std::string& value, gts::particles::ParticleModuleParameterType& type)
    {
        if (value == "float")
        {
            type = gts::particles::ParticleModuleParameterType::Float;
            return true;
        }
        if (value == "uint")
        {
            type = gts::particles::ParticleModuleParameterType::UInt;
            return true;
        }
        if (value == "bool")
        {
            type = gts::particles::ParticleModuleParameterType::Bool;
            return true;
        }
        if (value == "enum")
        {
            type = gts::particles::ParticleModuleParameterType::Enum;
            return true;
        }
        if (value == "string")
        {
            type = gts::particles::ParticleModuleParameterType::String;
            return true;
        }
        if (value == "floatCurve")
        {
            type = gts::particles::ParticleModuleParameterType::FloatCurve;
            return true;
        }
        if (value == "colorGradient")
        {
            type = gts::particles::ParticleModuleParameterType::ColorGradient;
            return true;
        }
        if (value == "burstTimeline")
        {
            type = gts::particles::ParticleModuleParameterType::BurstTimeline;
            return true;
        }
        return false;
    }

    bool readColorCurveValue(const GtsJsonValue& source, ParticleColorCurve& curve)
    {
        if (source.type() != GtsJsonValue::Type::Array)
            return false;

        ParticleColorCurve parsed;
        for (const GtsJsonValue& itemValue : source.asArray())
        {
            if (itemValue.type() != GtsJsonValue::Type::Array || itemValue.asArray().size() != 2u ||
                itemValue.asArray()[0].type() != GtsJsonValue::Type::Number)
                return false;
            ParticleColorKey item;
            item.t = static_cast<float>(itemValue.asArray()[0].asNumber());
            if (!readVec4Value(itemValue.asArray()[1], item.color))
                return false;
            parsed.push_back(item);
        }

        curve = std::move(parsed);
        return true;
    }

    bool readColorCurve(const GtsJsonValue& source, const std::string& key, ParticleColorCurve& curve, bool deep = true)
    {
        const GtsJsonValue* arrayValue = findTypedMember(source, key, GtsJsonValue::Type::Array, deep);
        return arrayValue != nullptr && readColorCurveValue(*arrayValue, curve);
    }

    bool readFloatCurveValue(const GtsJsonValue& source, ParticleFloatCurve& curve)
    {
        if (source.type() != GtsJsonValue::Type::Array)
            return false;

        ParticleFloatCurve parsed;
        for (const GtsJsonValue& itemValue : source.asArray())
        {
            if (itemValue.type() != GtsJsonValue::Type::Array || itemValue.asArray().size() != 2u ||
                itemValue.asArray()[0].type() != GtsJsonValue::Type::Number ||
                itemValue.asArray()[1].type() != GtsJsonValue::Type::Number)
                return false;
            ParticleFloatKey item;
            item.t     = static_cast<float>(itemValue.asArray()[0].asNumber());
            item.value = static_cast<float>(itemValue.asArray()[1].asNumber());
            parsed.push_back(item);
        }

        curve = std::move(parsed);
        return true;
    }

    bool readFloatCurve(const GtsJsonValue& source, const std::string& key, ParticleFloatCurve& curve, bool deep = true)
    {
        const GtsJsonValue* arrayValue = findTypedMember(source, key, GtsJsonValue::Type::Array, deep);
        return arrayValue != nullptr && readFloatCurveValue(*arrayValue, curve);
    }

    bool readBurstsValue(const GtsJsonValue& source, std::vector<ParticleBurst>& bursts)
    {
        if (source.type() != GtsJsonValue::Type::Array)
            return false;

        std::vector<ParticleBurst> parsed;
        for (const GtsJsonValue& itemValue : source.asArray())
        {
            if (itemValue.type() != GtsJsonValue::Type::Array || itemValue.asArray().size() != 5u)
                return false;
            ParticleBurst burst;
            for (const GtsJsonValue& numberValue : itemValue.asArray())
            {
                if (numberValue.type() != GtsJsonValue::Type::Number)
                    return false;
            }
            const float countMin    = static_cast<float>(itemValue.asArray()[1].asNumber());
            const float countMax    = static_cast<float>(itemValue.asArray()[2].asNumber());
            const float repeatCount = static_cast<float>(itemValue.asArray()[4].asNumber());
            burst.time             = static_cast<float>(itemValue.asArray()[0].asNumber());
            burst.repeatInterval   = static_cast<float>(itemValue.asArray()[3].asNumber());
            burst.countMin    = static_cast<uint32_t>(std::max(0.0f, countMin));
            burst.countMax    = static_cast<uint32_t>(std::max(countMin, countMax));
            burst.repeatCount = static_cast<uint32_t>(std::max(0.0f, repeatCount));
            parsed.push_back(burst);
        }

        bursts = std::move(parsed);
        return true;
    }

    bool readBursts(const GtsJsonValue& source, ParticleEmitterComponent& emitter)
    {
        const GtsJsonValue* arrayValue = findTypedMember(source, "bursts", GtsJsonValue::Type::Array, true);
        return arrayValue != nullptr && readBurstsValue(*arrayValue, emitter.bursts);
    }

    void readEmitter(const GtsJsonValue& source, ParticleEmitterComponent& emitter)
    {
        std::string text;
        readUint(source, "schemaVersion", emitter.schemaVersion);
        if (readString(source, "shape", text))
            emitter.shape = shapeFromString(text);
        if (readString(source, "blend", text))
            emitter.blend = blendFromString(text);
        if (readString(source, "primitive", text))
            emitter.primitive = primitiveFromString(text);
        if (readString(source, "spriteShape", text))
            emitter.spriteShape = spriteShapeFromString(text);
        if (readString(source, "collisionMode", text))
            emitter.collision.mode = collisionModeFromString(text);
        readString(source, "texturePath", emitter.texturePath);
        readString(source, "effectEmitterId", emitter.effectEmitterId);
        readString(source, "meshPath", emitter.meshPath);
        readString(source, "materialPath", emitter.materialPath);
        readBool(source, "enabled", emitter.enabled);
        readBool(source, "localSpace", emitter.localSpace);
        readBool(source, "looping", emitter.looping);
        readFloat(source, "emissionRate", emitter.emissionRate);
        readUint(source, "maxParticles", emitter.maxParticles);
        readFloat(source, "lifetimeMin", emitter.lifetimeMin);
        readFloat(source, "lifetimeMax", emitter.lifetimeMax);
        readFloat(source, "duration", emitter.duration);
        readFloat(source, "startDelay", emitter.startDelay);
        readFloat(source, "intensity", emitter.intensity);
        readFloat(source, "effectScale", emitter.runtime.effectScale);
        readFloat(source, "importance", emitter.runtime.importance);
        readUint(source, "budgetWeight", emitter.runtime.budgetWeight);
        readUint(source, "maxSpawnPerFrame", emitter.runtime.maxSpawnPerFrame);
        readVec3(source, "initialVelocity", emitter.initialVelocity);
        readFloat(source, "velocitySpread", emitter.velocitySpread);
        readFloat(source, "radialVelocityMin", emitter.radialVelocityMin);
        readFloat(source, "radialVelocityMax", emitter.radialVelocityMax);
        readFloat(source, "tangentVelocity", emitter.tangentVelocity);
        readFloat(source, "drag", emitter.drag);
        readFloat(source, "spinMin", emitter.spinMin);
        readFloat(source, "spinMax", emitter.spinMax);
        readFloat(source, "sizeRandomness", emitter.sizeRandomness);
        readFloat(source, "aspectRatioMin", emitter.aspectRatioMin);
        readFloat(source, "aspectRatioMax", emitter.aspectRatioMax);
        readFloat(source, "spriteEdgeSoftness", emitter.spriteEdgeSoftness);
        readFloat(source, "softness", emitter.softness);
        readFloat(source, "meshSoftness", emitter.runtime.meshSoftness);
        readFloat(source, "lightingInfluence", emitter.runtime.lightingInfluence);
        readBool(source, "frustumCulling", emitter.runtime.frustumCulling);
        readBool(source, "distanceCulling", emitter.runtime.distanceCulling);
        readBool(source, "simulateWhenCulled", emitter.runtime.simulateWhenCulled);
        readFloat(source, "cullPadding", emitter.runtime.cullPadding);
        readFloat(source, "maxDrawDistance", emitter.runtime.maxDrawDistance);
        readFloat(source, "lodNearDistance", emitter.runtime.lodNearDistance);
        readFloat(source, "lodFarDistance", emitter.runtime.lodFarDistance);
        readFloat(source, "lodMinSpawnScale", emitter.runtime.lodMinSpawnScale);
        readFloat(source, "lodMinRenderScale", emitter.runtime.lodMinRenderScale);
        readFloat(source, "velocityStretch", emitter.runtime.velocityStretch);
        readFloat(source, "velocityStretchMax", emitter.runtime.velocityStretchMax);
        readFloat(source, "hueVariation", emitter.hueVariation);
        readFloat(source, "valueVariation", emitter.valueVariation);
        readVec3(source, "meshScale", emitter.meshScale);
        readVec3(source, "meshAngularVelocityMin", emitter.meshAngularVelocityMin);
        readVec3(source, "meshAngularVelocityMax", emitter.meshAngularVelocityMax);
        readBool(source, "randomMeshRotation", emitter.randomMeshRotation);
        readVec4(source, "baseTint", emitter.baseTint);
        readFloat(source, "sphereRadius", emitter.sphereRadius);
        readVec3(source, "boxExtents", emitter.boxExtents);
        readFloat(source, "discRadius", emitter.discRadius);
        readFloat(source, "ringInnerRadius", emitter.ringInnerRadius);
        readFloat(source, "ringOuterRadius", emitter.ringOuterRadius);
        readFloat(source, "cylinderRadius", emitter.cylinderRadius);
        readFloat(source, "cylinderHeight", emitter.cylinderHeight);
        readColorCurve(source, "colorOverLifetime", emitter.colorOverLifetime);
        readFloatCurve(source, "alphaOverLifetime", emitter.alphaOverLifetime);
        readFloatCurve(source, "sizeOverLifetime", emitter.sizeOverLifetime);
        readBursts(source, emitter);
        readUint(source, "flipbookColumns", emitter.flipbook.columns);
        readUint(source, "flipbookRows", emitter.flipbook.rows);
        readUint(source, "flipbookFrameCount", emitter.flipbook.frameCount);
        readFloat(source, "flipbookFrameRate", emitter.flipbook.frameRate);
        readBool(source, "flipbookLifetimeDriven", emitter.flipbook.lifetimeDriven);
        readBool(source, "flipbookRandomStart", emitter.flipbook.randomStart);
        readVec3(source, "forceAcceleration", emitter.forces.acceleration);
        readVec3(source, "forceWind", emitter.forces.wind);
        readFloat(source, "forceVortex", emitter.forces.vortex);
        readFloat(source, "forceRadial", emitter.forces.radial);
        readFloat(source, "forceNoiseStrength", emitter.forces.noiseStrength);
        readFloat(source, "forceNoiseScale", emitter.forces.noiseScale);
        readFloat(source, "collisionGroundY", emitter.collision.groundY);
        readFloat(source, "collisionBounce", emitter.collision.bounce);
        readFloat(source, "collisionDamping", emitter.collision.damping);
        readBool(source, "killOnCollision", emitter.collision.killOnCollision);
        readUint(source, "spawnOnDeathCount", emitter.collision.spawnOnDeathCount);
        readUint(source, "spawnOnCollisionCount", emitter.collision.spawnOnCollisionCount);
        readUint(source, "maxEventSpawnsPerFrame", emitter.collision.maxEventSpawnsPerFrame);
    }

    GtsJsonValue vec2Json(const glm::vec2& value)
    {
        return GtsJsonValue::Array{value.x, value.y};
    }

    GtsJsonValue vec3Json(const glm::vec3& value)
    {
        return GtsJsonValue::Array{value.x, value.y, value.z};
    }

    GtsJsonValue vec4Json(const glm::vec4& value)
    {
        return GtsJsonValue::Array{value.x, value.y, value.z, value.w};
    }

    GtsJsonValue colorCurveJson(const ParticleColorCurve& curve)
    {
        GtsJsonValue::Array array;
        for (const auto& point : curve)
            array.emplace_back(GtsJsonValue::Array{point.t, vec4Json(point.color)});
        return array;
    }

    GtsJsonValue floatCurveJson(const ParticleFloatCurve& curve)
    {
        GtsJsonValue::Array array;
        for (const auto& point : curve)
            array.emplace_back(GtsJsonValue::Array{point.t, point.value});
        return array;
    }

    GtsJsonValue burstsJson(const std::vector<ParticleBurst>& bursts)
    {
        GtsJsonValue::Array array;
        for (const auto& burst : bursts)
            array.emplace_back(GtsJsonValue::Array{
                burst.time, burst.countMin, burst.countMax, burst.repeatInterval, burst.repeatCount});
        return array;
    }

    GtsJsonValue moduleParameterJson(const gts::particles::ParticleModuleParameter& parameter)
    {
        GtsJsonValue value;
        switch (parameter.type)
        {
        case gts::particles::ParticleModuleParameterType::Float:
            value = parameter.floatValue;
            break;
        case gts::particles::ParticleModuleParameterType::UInt:
        case gts::particles::ParticleModuleParameterType::Enum:
            value = parameter.uintValue;
            break;
        case gts::particles::ParticleModuleParameterType::Bool:
            value = parameter.boolValue;
            break;
        case gts::particles::ParticleModuleParameterType::String:
            value = parameter.stringValue;
            break;
        case gts::particles::ParticleModuleParameterType::FloatCurve:
            value = floatCurveJson(parameter.floatCurveValue);
            break;
        case gts::particles::ParticleModuleParameterType::ColorGradient:
            value = colorCurveJson(parameter.colorGradientValue);
            break;
        case gts::particles::ParticleModuleParameterType::BurstTimeline:
            value = burstsJson(parameter.burstTimelineValue);
            break;
        }
        return GtsJsonValue::Object{
            {"id", parameter.id}, {"type", moduleParameterTypeToString(parameter.type)}, {"value", std::move(value)}};
    }

    GtsJsonValue moduleJson(const gts::particles::ParticleModuleInstance& module)
    {
        GtsJsonValue::Array parameters;
        for (const auto& parameter : module.parameters)
            parameters.push_back(moduleParameterJson(parameter));
        return GtsJsonValue::Object{
            {"id", module.stableId}, {"type", module.typeId}, {"displayName", module.displayName},
            {"version", module.version}, {"enabled", module.enabled}, {"parameters", std::move(parameters)}};
    }

    GtsJsonValue graphJson(const ParticleEffectGraph& graph)
    {
        GtsJsonValue::Array nodes, links, frames, comments;
        for (const auto& node : graph.nodes)
            nodes.emplace_back(GtsJsonValue::Object{
                {"id", node.id}, {"moduleStableId", node.moduleStableId}, {"type", node.typeId},
                {"displayName", node.displayName}, {"frameId", node.frameId}, {"position", vec2Json(node.position)}});
        for (const auto& link : graph.links)
            links.emplace_back(GtsJsonValue::Object{
                {"id", link.id}, {"from", link.fromNodeId}, {"fromPort", link.fromPortId},
                {"to", link.toNodeId}, {"toPort", link.toPortId}});
        for (const auto& frame : graph.frames)
            frames.emplace_back(GtsJsonValue::Object{
                {"id", frame.id}, {"title", frame.title}, {"position", vec2Json(frame.position)}, {"size", vec2Json(frame.size)}});
        for (const auto& comment : graph.comments)
            comments.emplace_back(GtsJsonValue::Object{
                {"id", comment.id}, {"text", comment.text}, {"position", vec2Json(comment.position)}});
        return GtsJsonValue::Object{
            {"schemaVersion", graph.schemaVersion}, {"nodes", std::move(nodes)}, {"links", std::move(links)},
            {"frames", std::move(frames)}, {"comments", std::move(comments)}};
    }

    std::string defaultEmitterId(size_t index)
    {
        return index == 0 ? "emitter" : "emitter_" + std::to_string(index + 1);
    }

    std::string defaultEmitterName(size_t index)
    {
        return index == 0 ? "Emitter" : "Emitter " + std::to_string(index + 1);
    }

    std::string defaultEffectNameFromPath(const std::string& path)
    {
        const std::filesystem::path filePath(path);
        const std::string           stem = filePath.stem().string();
        return stem.empty() ? "Particle Effect" : stem;
    }

    void readMetadata(const GtsJsonValue& source, ParticleEffectMetadata& metadata)
    {
        const GtsJsonValue* metadataObject = nullptr;
        if (!readObject(source, "metadata", metadataObject))
            return;

        readString(*metadataObject, "name", metadata.name, false);
        readString(*metadataObject, "description", metadata.description, false);
        readString(*metadataObject, "author", metadata.author, false);
    }

    void readPreview(const GtsJsonValue& source, ParticleEffectPreviewSettings& preview)
    {
        const GtsJsonValue* previewObject = nullptr;
        if (!readObject(source, "preview", previewObject))
            return;

        readVec4(*previewObject, "backgroundColor", preview.backgroundColor, false);
        readVec3(*previewObject, "cameraPosition", preview.cameraPosition, false);
        readVec3(*previewObject, "cameraTarget", preview.cameraTarget, false);
        readFloat(*previewObject, "orbitDistance", preview.orbitDistance, false);
    }

    const gts::particles::ParticleModuleParameterDefinition*
    findParameterDefinition(const gts::particles::ParticleModuleDefinition* definition, const std::string& parameterId)
    {
        if (definition == nullptr)
            return nullptr;

        const auto it = std::find_if(definition->parameters.begin(),
                                     definition->parameters.end(),
                                     [&](const gts::particles::ParticleModuleParameterDefinition& parameter)
                                     {
                                         return parameter.id == parameterId;
                                     });
        return it == definition->parameters.end() ? nullptr : &*it;
    }

    bool readModuleParameter(const GtsJsonValue&                                source,
                             const gts::particles::ParticleModuleDefinition* definition,
                             gts::particles::ParticleModuleParameter&        parameter)
    {
        if (!readString(source, "id", parameter.id, false) || parameter.id.empty())
            return false;

        std::string typeString;
        if (readString(source, "type", typeString, false))
        {
            if (!moduleParameterTypeFromString(typeString, parameter.type))
                return false;
        }
        else if (const auto* parameterDefinition = findParameterDefinition(definition, parameter.id))
        {
            parameter.type = parameterDefinition->type;
        }

        const GtsJsonValue* value = source.find("value");
        if (value == nullptr)
            return true;

        switch (parameter.type)
        {
        case gts::particles::ParticleModuleParameterType::Float:
            if (value->type() != GtsJsonValue::Type::Number)
                return false;
            parameter.floatValue = static_cast<float>(value->asNumber());
            return true;
        case gts::particles::ParticleModuleParameterType::UInt:
        case gts::particles::ParticleModuleParameterType::Enum:
            return readUint(source, "value", parameter.uintValue, false);
        case gts::particles::ParticleModuleParameterType::Bool:
            if (value->type() != GtsJsonValue::Type::Bool)
                return false;
            parameter.boolValue = value->asBool();
            return true;
        case gts::particles::ParticleModuleParameterType::String:
            if (value->type() != GtsJsonValue::Type::String)
                return false;
            parameter.stringValue = value->asString();
            return true;
        case gts::particles::ParticleModuleParameterType::FloatCurve:
            return readFloatCurveValue(*value, parameter.floatCurveValue);
        case gts::particles::ParticleModuleParameterType::ColorGradient:
            return readColorCurveValue(*value, parameter.colorGradientValue);
        case gts::particles::ParticleModuleParameterType::BurstTimeline:
            return readBurstsValue(*value, parameter.burstTimelineValue);
        }
        return false;
    }

    bool readEmitterModules(const GtsJsonValue& source, std::vector<gts::particles::ParticleModuleInstance>& modules)
    {
        std::vector<const GtsJsonValue*> moduleObjects;
        if (!readObjectArray(source, "modules", moduleObjects))
            return false;

        std::vector<gts::particles::ParticleModuleInstance> parsedModules;
        parsedModules.reserve(moduleObjects.size());
        for (const GtsJsonValue* moduleObject : moduleObjects)
        {
            gts::particles::ParticleModuleInstance module;
            readString(*moduleObject, "id", module.stableId, false);
            readString(*moduleObject, "type", module.typeId, false);
            readString(*moduleObject, "displayName", module.displayName, false);
            readUint(*moduleObject, "version", module.version, false);
            readBool(*moduleObject, "enabled", module.enabled, false);

            const gts::particles::ParticleModuleDefinition* definition =
                gts::particles::findParticleModuleDefinition(module.typeId);
            std::vector<const GtsJsonValue*> parameterObjects;
            if (readObjectArray(*moduleObject, "parameters", parameterObjects))
            {
                module.parameters.reserve(parameterObjects.size());
                for (const GtsJsonValue* parameterObject : parameterObjects)
                {
                    gts::particles::ParticleModuleParameter parameter;
                    if (readModuleParameter(*parameterObject, definition, parameter))
                        module.parameters.push_back(std::move(parameter));
                }
            }

            parsedModules.push_back(std::move(module));
        }

        modules = std::move(parsedModules);
        return true;
    }

    bool readEmitterGraph(const GtsJsonValue& source, ParticleEffectGraph& graph)
    {
        const GtsJsonValue* graphObject = nullptr;
        if (!readObject(source, "graph", graphObject))
            return false;

        ParticleEffectGraph parsed;
        readUint(*graphObject, "schemaVersion", parsed.schemaVersion, false);

        std::vector<const GtsJsonValue*> nodeObjects;
        if (readObjectArray(*graphObject, "nodes", nodeObjects))
        {
            parsed.nodes.reserve(nodeObjects.size());
            for (const GtsJsonValue* nodeObject : nodeObjects)
            {
                ParticleGraphNode node;
                readString(*nodeObject, "id", node.id, false);
                readString(*nodeObject, "moduleStableId", node.moduleStableId, false);
                readString(*nodeObject, "type", node.typeId, false);
                readString(*nodeObject, "displayName", node.displayName, false);
                readString(*nodeObject, "frameId", node.frameId, false);
                readVec2(*nodeObject, "position", node.position, false);
                parsed.nodes.push_back(std::move(node));
            }
        }

        std::vector<const GtsJsonValue*> linkObjects;
        if (readObjectArray(*graphObject, "links", linkObjects))
        {
            parsed.links.reserve(linkObjects.size());
            for (const GtsJsonValue* linkObject : linkObjects)
            {
                ParticleGraphLink link;
                readString(*linkObject, "id", link.id, false);
                readString(*linkObject, "from", link.fromNodeId, false);
                readString(*linkObject, "fromPort", link.fromPortId, false);
                readString(*linkObject, "to", link.toNodeId, false);
                readString(*linkObject, "toPort", link.toPortId, false);
                parsed.links.push_back(std::move(link));
            }
        }

        std::vector<const GtsJsonValue*> frameObjects;
        if (readObjectArray(*graphObject, "frames", frameObjects))
        {
            parsed.frames.reserve(frameObjects.size());
            for (const GtsJsonValue* frameObject : frameObjects)
            {
                ParticleGraphFrame frame;
                readString(*frameObject, "id", frame.id, false);
                readString(*frameObject, "title", frame.title, false);
                readVec2(*frameObject, "position", frame.position, false);
                readVec2(*frameObject, "size", frame.size, false);
                parsed.frames.push_back(std::move(frame));
            }
        }

        std::vector<const GtsJsonValue*> commentObjects;
        if (readObjectArray(*graphObject, "comments", commentObjects))
        {
            parsed.comments.reserve(commentObjects.size());
            for (const GtsJsonValue* commentObject : commentObjects)
            {
                ParticleGraphComment comment;
                readString(*commentObject, "id", comment.id, false);
                readString(*commentObject, "text", comment.text, false);
                readVec2(*commentObject, "position", comment.position, false);
                parsed.comments.push_back(std::move(comment));
            }
        }

        graph = std::move(parsed);
        return true;
    }

    bool readEffectEmitters(const GtsJsonValue& source, ParticleEffectAsset& asset)
    {
        std::vector<const GtsJsonValue*> emitterObjects;
        if (!readObjectArray(source, "emitters", emitterObjects))
            return false;

        std::vector<ParticleEffectEmitter> emitters;
        emitters.reserve(emitterObjects.size());
        for (size_t i = 0; i < emitterObjects.size(); ++i)
        {
            ParticleEffectEmitter emitter;
            emitter.stableId = defaultEmitterId(i);
            emitter.name     = defaultEmitterName(i);
            readString(*emitterObjects[i], "id", emitter.stableId, false);
            readString(*emitterObjects[i], "name", emitter.name, false);
            readEmitter(*emitterObjects[i], emitter.descriptor);
            readEmitterModules(*emitterObjects[i], emitter.modules);
            readEmitterGraph(*emitterObjects[i], emitter.graph);
            emitters.push_back(std::move(emitter));
        }

        asset.emitters = std::move(emitters);
        return true;
    }

    GtsJsonValue emitterJson(const ParticleEffectEmitter& effectEmitter)
    {
        const auto&         emitter = effectEmitter.descriptor;
        GtsJsonValue::Array modules;
        for (const auto& module : effectEmitter.modules)
            modules.push_back(moduleJson(module));
        return GtsJsonValue::Object{
            {"id", effectEmitter.stableId},
            {"name", effectEmitter.name},
            {"modules", std::move(modules)},
            {"graph", graphJson(effectEmitter.graph)},
            {"simulation",
             GtsJsonValue::Object{{"schemaVersion", emitter.schemaVersion},
                                  {"enabled", emitter.enabled},
                                  {"localSpace", emitter.localSpace},
                                  {"looping", emitter.looping},
                                  {"duration", emitter.duration},
                                  {"startDelay", emitter.startDelay},
                                  {"intensity", emitter.intensity},
                                  {"effectScale", emitter.runtime.effectScale},
                                  {"importance", emitter.runtime.importance},
                                  {"budgetWeight", emitter.runtime.budgetWeight},
                                  {"maxSpawnPerFrame", emitter.runtime.maxSpawnPerFrame}}},
            {"renderer",
             GtsJsonValue::Object{{"primitive", primitiveToString(emitter.primitive)},
                                  {"blend", blendToString(emitter.blend)},
                                  {"spriteShape", spriteShapeToString(emitter.spriteShape)},
                                  {"texturePath", emitter.texturePath},
                                  {"meshPath", emitter.meshPath},
                                  {"materialPath", emitter.materialPath},
                                  {"spriteEdgeSoftness", emitter.spriteEdgeSoftness},
                                  {"softness", emitter.softness},
                                  {"meshSoftness", emitter.runtime.meshSoftness},
                                  {"lightingInfluence", emitter.runtime.lightingInfluence},
                                  {"frustumCulling", emitter.runtime.frustumCulling},
                                  {"distanceCulling", emitter.runtime.distanceCulling},
                                  {"simulateWhenCulled", emitter.runtime.simulateWhenCulled},
                                  {"cullPadding", emitter.runtime.cullPadding},
                                  {"maxDrawDistance", emitter.runtime.maxDrawDistance},
                                  {"lodNearDistance", emitter.runtime.lodNearDistance},
                                  {"lodFarDistance", emitter.runtime.lodFarDistance},
                                  {"lodMinSpawnScale", emitter.runtime.lodMinSpawnScale},
                                  {"lodMinRenderScale", emitter.runtime.lodMinRenderScale},
                                  {"velocityStretch", emitter.runtime.velocityStretch},
                                  {"velocityStretchMax", emitter.runtime.velocityStretchMax},
                                  {"meshScale", vec3Json(emitter.meshScale)}}},
            {"spawn",
             GtsJsonValue::Object{{"emissionRate", emitter.emissionRate},
                                  {"maxParticles", emitter.maxParticles},
                                  {"lifetimeMin", emitter.lifetimeMin},
                                  {"lifetimeMax", emitter.lifetimeMax}}},
            {"shape",
             GtsJsonValue::Object{{"shape", shapeToString(emitter.shape)},
                                  {"sphereRadius", emitter.sphereRadius},
                                  {"boxExtents", vec3Json(emitter.boxExtents)},
                                  {"discRadius", emitter.discRadius},
                                  {"ringInnerRadius", emitter.ringInnerRadius},
                                  {"ringOuterRadius", emitter.ringOuterRadius},
                                  {"cylinderRadius", emitter.cylinderRadius},
                                  {"cylinderHeight", emitter.cylinderHeight}}},
            {"velocity",
             GtsJsonValue::Object{{"initialVelocity", vec3Json(emitter.initialVelocity)},
                                  {"velocitySpread", emitter.velocitySpread},
                                  {"radialVelocityMin", emitter.radialVelocityMin},
                                  {"radialVelocityMax", emitter.radialVelocityMax},
                                  {"tangentVelocity", emitter.tangentVelocity},
                                  {"drag", emitter.drag}}},
            {"forces",
             GtsJsonValue::Object{{"forceAcceleration", vec3Json(emitter.forces.acceleration)},
                                  {"forceWind", vec3Json(emitter.forces.wind)},
                                  {"forceVortex", emitter.forces.vortex},
                                  {"forceRadial", emitter.forces.radial},
                                  {"forceNoiseStrength", emitter.forces.noiseStrength},
                                  {"forceNoiseScale", emitter.forces.noiseScale},
                                  {"collisionMode", collisionModeToString(emitter.collision.mode)},
                                  {"collisionGroundY", emitter.collision.groundY},
                                  {"collisionBounce", emitter.collision.bounce},
                                  {"collisionDamping", emitter.collision.damping},
                                  {"killOnCollision", emitter.collision.killOnCollision},
                                  {"spawnOnDeathCount", emitter.collision.spawnOnDeathCount},
                                  {"spawnOnCollisionCount", emitter.collision.spawnOnCollisionCount},
                                  {"maxEventSpawnsPerFrame", emitter.collision.maxEventSpawnsPerFrame}}},
            {"color",
             GtsJsonValue::Object{{"baseTint", vec4Json(emitter.baseTint)},
                                  {"hueVariation", emitter.hueVariation},
                                  {"valueVariation", emitter.valueVariation},
                                  {"colorOverLifetime", colorCurveJson(emitter.colorOverLifetime)},
                                  {"alphaOverLifetime", floatCurveJson(emitter.alphaOverLifetime)}}},
            {"size",
             GtsJsonValue::Object{{"sizeRandomness", emitter.sizeRandomness},
                                  {"aspectRatioMin", emitter.aspectRatioMin},
                                  {"aspectRatioMax", emitter.aspectRatioMax},
                                  {"sizeOverLifetime", floatCurveJson(emitter.sizeOverLifetime)}}},
            {"rotation",
             GtsJsonValue::Object{{"spinMin", emitter.spinMin},
                                  {"spinMax", emitter.spinMax},
                                  {"meshAngularVelocityMin", vec3Json(emitter.meshAngularVelocityMin)},
                                  {"meshAngularVelocityMax", vec3Json(emitter.meshAngularVelocityMax)},
                                  {"randomMeshRotation", emitter.randomMeshRotation}}},
            {"flipbook",
             GtsJsonValue::Object{{"flipbookColumns", emitter.flipbook.columns},
                                  {"flipbookRows", emitter.flipbook.rows},
                                  {"flipbookFrameCount", emitter.flipbook.frameCount},
                                  {"flipbookFrameRate", emitter.flipbook.frameRate},
                                  {"flipbookLifetimeDriven", emitter.flipbook.lifetimeDriven},
                                  {"flipbookRandomStart", emitter.flipbook.randomStart}}},
            {"bursts", burstsJson(emitter.bursts)}};
    }
} // namespace

namespace gts::particles
{
    bool migrateParticleEffectAsset(ParticleEffectAsset& asset, std::string* error)
    {
        if (asset.schemaVersion == 0)
            asset.schemaVersion = 1;
        if (asset.schemaVersion > CurrentParticleEffectSchemaVersion)
        {
            if (error != nullptr)
                *error = "particle effect schema is newer than this engine";
            return false;
        }
        if (asset.emitters.empty())
        {
            if (error != nullptr)
                *error = "particle effect has no emitters";
            return false;
        }

        asset.schemaVersion = CurrentParticleEffectSchemaVersion;
        for (size_t i = 0; i < asset.emitters.size(); ++i)
        {
            ParticleEffectEmitter& emitter = asset.emitters[i];
            if (emitter.stableId.empty())
                emitter.stableId = defaultEmitterId(i);
            if (emitter.name.empty())
                emitter.name = defaultEmitterName(i);

            if (emitter.descriptor.schemaVersion == 0)
                emitter.descriptor.schemaVersion = 1;
            if (emitter.descriptor.schemaVersion > CurrentParticleEmitterSchemaVersion)
            {
                if (error != nullptr)
                    *error = "particle emitter schema is newer than this engine";
                return false;
            }
            emitter.descriptor.schemaVersion = CurrentParticleEmitterSchemaVersion;
            for (const gts::particles::ParticleModuleInstance& module : emitter.modules)
            {
                const gts::particles::ParticleModuleDefinition* definition =
                    gts::particles::findParticleModuleDefinition(module.typeId);
                if (definition != nullptr && module.version > definition->version)
                {
                    if (error != nullptr)
                        *error = "particle module schema is newer than this engine";
                    return false;
                }
            }
            gts::particles::migrateParticleEmitterModules(emitter.modules, emitter.descriptor);
            gts::particles::syncParticleGraphWithModules(emitter);
            emitter.compiledProgram = gts::particles::compileParticleEffectEmitter(emitter);
            if (emitter.compiledProgram.valid)
                emitter.descriptor = emitter.compiledProgram.runtimeDescriptor;
        }

        return true;
    }

    const ParticleEffectEmitter* selectParticleEffectEmitter(const ParticleEffectAsset& asset,
                                                             const std::string&         emitterId)
    {
        if (!emitterId.empty())
        {
            for (const ParticleEffectEmitter& emitter : asset.emitters)
            {
                if (emitter.stableId == emitterId)
                    return &emitter;
            }
        }

        return asset.emitters.empty() ? nullptr : &asset.emitters.front();
    }

    bool loadParticleEffectAsset(const std::string& path, ParticleEffectAsset& asset)
    {
        std::ifstream file(path);
        if (!file)
            return false;

        std::stringstream buffer;
        buffer << file.rdbuf();
        const std::string source = buffer.str();

        GtsJsonValue root;
        if (!GtsJsonParser::parse(source, root) || root.type() != GtsJsonValue::Type::Object)
            return false;

        ParticleEffectAsset loaded;
        const bool          schemaRead = readUint(root, "schemaVersion", loaded.schemaVersion, false);
        readMetadata(root, loaded.metadata);
        readPreview(root, loaded.preview);
        if (!readEffectEmitters(root, loaded))
        {
            ParticleEffectEmitter emitter;
            emitter.stableId = "emitter";
            emitter.name     = defaultEffectNameFromPath(path);
            readEmitter(root, emitter.descriptor);
            loaded.schemaVersion = schemaRead ? loaded.schemaVersion : emitter.descriptor.schemaVersion;
            loaded.metadata.name =
                loaded.metadata.name.empty() ? defaultEffectNameFromPath(path) : loaded.metadata.name;
            loaded.emitters.push_back(std::move(emitter));
        }

        if (loaded.metadata.name.empty())
            loaded.metadata.name = defaultEffectNameFromPath(path);

        std::string migrationError;
        if (!migrateParticleEffectAsset(loaded, &migrationError))
            return false;

        asset = std::move(loaded);
        return true;
    }

    bool saveParticleEffectAsset(const std::string& path, const ParticleEffectAsset& asset)
    {
        ParticleEffectAsset migrated = asset;
        std::string         migrationError;
        if (!migrateParticleEffectAsset(migrated, &migrationError))
            return false;

        GtsJsonValue::Array emitters;
        for (const auto& emitter : migrated.emitters)
            emitters.push_back(emitterJson(emitter));
        const GtsJsonValue root =
            GtsJsonValue::Object{{"schemaVersion", migrated.schemaVersion},
                                 {"metadata",
                                  GtsJsonValue::Object{{"name", migrated.metadata.name},
                                                       {"description", migrated.metadata.description},
                                                       {"author", migrated.metadata.author}}},
                                 {"preview",
                                  GtsJsonValue::Object{{"backgroundColor", vec4Json(migrated.preview.backgroundColor)},
                                                       {"cameraPosition", vec3Json(migrated.preview.cameraPosition)},
                                                       {"cameraTarget", vec3Json(migrated.preview.cameraTarget)},
                                                       {"orbitDistance", migrated.preview.orbitDistance}}},
                                 {"emitters", std::move(emitters)}};
        std::string json;
        try
        {
            json = GtsJsonParser::serialize(root);
        }
        catch (const std::invalid_argument&)
        {
            return false;
        }
        const std::filesystem::path parent = std::filesystem::path(path).parent_path();
        if (!parent.empty())
            std::filesystem::create_directories(parent);
        std::ofstream out(path);
        if (!out)
            return false;
        out << json << '\n';
        return out.good();
    }

    bool loadParticleEffect(const std::string& path, ParticleEmitterComponent& emitter)
    {
        const uint32_t randomSeed = emitter.randomSeed;

        ParticleEffectAsset asset;
        if (!loadParticleEffectAsset(path, asset))
            return false;

        const ParticleEffectEmitter* selected = selectParticleEffectEmitter(asset, emitter.effectEmitterId);
        if (selected == nullptr)
            return false;

        ParticleEmitterComponent loaded = gts::particles::compiledParticleRuntimeDescriptor(*selected);
        loaded.effectPath               = path;
        loaded.effectEmitterId          = selected->stableId;
        loaded.randomSeed               = randomSeed;
        loaded.reloadFromEffect         = true;
        emitter                         = loaded;
        return true;
    }

    bool saveParticleEffect(const std::string& path, const ParticleEmitterComponent& emitter)
    {
        ParticleEffectAsset asset;
        asset.metadata.name = defaultEffectNameFromPath(path);

        ParticleEffectEmitter effectEmitter;
        effectEmitter.stableId   = emitter.effectEmitterId.empty() ? "emitter" : emitter.effectEmitterId;
        effectEmitter.name       = "Emitter";
        effectEmitter.descriptor = emitter;
        effectEmitter.descriptor.effectPath.clear();
        effectEmitter.descriptor.effectEmitterId.clear();
        asset.emitters.push_back(std::move(effectEmitter));

        return saveParticleEffectAsset(path, asset);
    }
} // namespace gts::particles
