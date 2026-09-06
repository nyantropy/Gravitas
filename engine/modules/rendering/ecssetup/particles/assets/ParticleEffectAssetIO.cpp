#include "ParticleEffectAssetIO.h"
#include "GtsJsonParser.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "ParticleGraphAuthoring.h"
#include "ParticleProgramCompiler.h"

namespace
{
    bool readObjectArray(const GtsJsonValue& source,
                         const std::string& key,
                         std::vector<const GtsJsonValue*>& objectValues)
    {
        const auto* arrayValue = source.findArray(key);
        if (arrayValue == nullptr)
            return false;

        std::vector<const GtsJsonValue*> parsed;
        parsed.reserve(arrayValue->size());
        for (const GtsJsonValue& item : *arrayValue)
        {
            if (item.type() != GtsJsonValue::Type::Object)
                return false;
            parsed.push_back(&item);
        }

        objectValues = std::move(parsed);
        return true;
    }

    bool readVec3Value(const GtsJsonValue& source, glm::vec3& value)
    {
        const auto* array = source.tryArray();
        if (array == nullptr || array->size() != 3u)
            return false;
        const auto x = (*array)[0].tryFloat();
        const auto y = (*array)[1].tryFloat();
        const auto z = (*array)[2].tryFloat();
        if (!x || !y || !z)
            return false;
        value = {*x, *y, *z};
        return true;
    }

    bool readVec2Value(const GtsJsonValue& source, glm::vec2& value)
    {
        const auto* array = source.tryArray();
        if (array == nullptr || array->size() != 2u)
            return false;
        const auto x = (*array)[0].tryFloat();
        const auto y = (*array)[1].tryFloat();
        if (!x || !y)
            return false;
        value = {*x, *y};
        return true;
    }

    bool readVec4Value(const GtsJsonValue& source, glm::vec4& value)
    {
        const auto* array = source.tryArray();
        if (array == nullptr || array->size() != 4u)
            return false;
        const auto x = (*array)[0].tryFloat();
        const auto y = (*array)[1].tryFloat();
        const auto z = (*array)[2].tryFloat();
        const auto w = (*array)[3].tryFloat();
        if (!x || !y || !z || !w)
            return false;
        value = {*x, *y, *z, *w};
        return true;
    }

    bool readVec3(const GtsJsonValue& source, const std::string& key, glm::vec3& value)
    {
        const GtsJsonValue* arrayValue = source.find(key);
        return arrayValue != nullptr && readVec3Value(*arrayValue, value);
    }

    bool readVec2(const GtsJsonValue& source, const std::string& key, glm::vec2& value)
    {
        const GtsJsonValue* arrayValue = source.find(key);
        return arrayValue != nullptr && readVec2Value(*arrayValue, value);
    }

    bool readVec4(const GtsJsonValue& source, const std::string& key, glm::vec4& value)
    {
        const GtsJsonValue* arrayValue = source.find(key);
        return arrayValue != nullptr && readVec4Value(*arrayValue, value);
    }

    bool readColorCurveValue(const GtsJsonValue& source, ParticleColorCurve& curve)
    {
        if (source.type() != GtsJsonValue::Type::Array)
            return false;

        ParticleColorCurve parsed;
        for (const GtsJsonValue& itemValue : source.asArray())
        {
            if (itemValue.type() != GtsJsonValue::Type::Array || itemValue.asArray().size() != 2u ||
                !itemValue.asArray()[0].tryFloat())
                return false;
            ParticleColorKey item;
            item.t = *itemValue.asArray()[0].tryFloat();
            if (!readVec4Value(itemValue.asArray()[1], item.color))
                return false;
            parsed.push_back(item);
        }

        curve = std::move(parsed);
        return true;
    }

    bool readColorCurve(const GtsJsonValue& source, const std::string& key, ParticleColorCurve& curve)
    {
        const GtsJsonValue* arrayValue = source.find(key);
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
                !itemValue.asArray()[0].tryFloat() ||
                !itemValue.asArray()[1].tryFloat())
                return false;
            ParticleFloatKey item;
            item.t     = *itemValue.asArray()[0].tryFloat();
            item.value = *itemValue.asArray()[1].tryFloat();
            parsed.push_back(item);
        }

        curve = std::move(parsed);
        return true;
    }

    bool readFloatCurve(const GtsJsonValue& source, const std::string& key, ParticleFloatCurve& curve)
    {
        const GtsJsonValue* arrayValue = source.find(key);
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
            const auto& fields = itemValue.asArray();
            const auto time = fields[0].tryFloat();
            const auto countMin = fields[1].tryUInt32();
            const auto countMax = fields[2].tryUInt32();
            const auto interval = fields[3].tryFloat();
            const auto repeat = fields[4].tryUInt32();
            if (!time || !countMin || !countMax || !interval || !repeat)
                return false;
            ParticleBurst burst;
            burst.time = *time;
            burst.countMin = *countMin;
            burst.countMax = std::max(*countMin, *countMax);
            burst.repeatInterval = *interval;
            burst.repeatCount = *repeat;
            parsed.push_back(burst);
        }

        bursts = std::move(parsed);
        return true;
    }

    bool readBursts(const GtsJsonValue& source, ParticleEmitterComponent& emitter)
    {
        const GtsJsonValue* arrayValue = source.find("bursts");
        return arrayValue != nullptr && readBurstsValue(*arrayValue, emitter.bursts);
    }

    void readEmitter(const GtsJsonValue& source, ParticleEmitterComponent& emitter)
    {
        {
            const auto* section = source.find("simulation");
            const auto& simulation = section != nullptr && section->isObject() ? *section : source;
            emitter.schemaVersion = simulation.findUInt32("schemaVersion").value_or(emitter.schemaVersion);
            emitter.enabled = simulation.findBool("enabled").value_or(emitter.enabled);
            emitter.localSpace = simulation.findBool("localSpace").value_or(emitter.localSpace);
            emitter.looping = simulation.findBool("looping").value_or(emitter.looping);
            emitter.duration = simulation.findFloat("duration").value_or(emitter.duration);
            emitter.startDelay = simulation.findFloat("startDelay").value_or(emitter.startDelay);
            emitter.intensity = simulation.findFloat("intensity").value_or(emitter.intensity);
            emitter.runtime.effectScale = simulation.findFloat("effectScale").value_or(emitter.runtime.effectScale);
            emitter.runtime.importance = simulation.findFloat("importance").value_or(emitter.runtime.importance);
            emitter.runtime.budgetWeight = simulation.findUInt32("budgetWeight").value_or(emitter.runtime.budgetWeight);
            emitter.runtime.maxSpawnPerFrame = simulation.findUInt32("maxSpawnPerFrame").value_or(emitter.runtime.maxSpawnPerFrame);
        }

        {
            const auto* section = source.find("renderer");
            const auto& renderer = section != nullptr && section->isObject() ? *section : source;
            if (const auto text = renderer.findString("blend"))
                emitter.blend = gts::enumValue(particleBlendModeNames, *text).value_or(ParticleBlendMode::Alpha);
            if (const auto text = renderer.findString("primitive"))
                emitter.primitive =
                    gts::enumValue(particlePrimitiveNames, *text).value_or(ParticlePrimitive::Billboard);
            if (const auto text = renderer.findString("spriteShape"))
                emitter.spriteShape =
                    gts::enumValue(particleSpriteShapeNames, *text).value_or(ParticleSpriteShape::SoftCircle);
            emitter.texturePath = renderer.findString("texturePath").value_or(emitter.texturePath);
            emitter.meshPath = renderer.findString("meshPath").value_or(emitter.meshPath);
            emitter.materialPath = renderer.findString("materialPath").value_or(emitter.materialPath);
            emitter.spriteEdgeSoftness = renderer.findFloat("spriteEdgeSoftness").value_or(emitter.spriteEdgeSoftness);
            emitter.softness = renderer.findFloat("softness").value_or(emitter.softness);
            emitter.runtime.meshSoftness = renderer.findFloat("meshSoftness").value_or(emitter.runtime.meshSoftness);
            emitter.runtime.lightingInfluence = renderer.findFloat("lightingInfluence").value_or(emitter.runtime.lightingInfluence);
            emitter.runtime.frustumCulling = renderer.findBool("frustumCulling").value_or(emitter.runtime.frustumCulling);
            emitter.runtime.distanceCulling = renderer.findBool("distanceCulling").value_or(emitter.runtime.distanceCulling);
            emitter.runtime.simulateWhenCulled = renderer.findBool("simulateWhenCulled").value_or(emitter.runtime.simulateWhenCulled);
            emitter.runtime.cullPadding = renderer.findFloat("cullPadding").value_or(emitter.runtime.cullPadding);
            emitter.runtime.maxDrawDistance = renderer.findFloat("maxDrawDistance").value_or(emitter.runtime.maxDrawDistance);
            emitter.runtime.lodNearDistance = renderer.findFloat("lodNearDistance").value_or(emitter.runtime.lodNearDistance);
            emitter.runtime.lodFarDistance = renderer.findFloat("lodFarDistance").value_or(emitter.runtime.lodFarDistance);
            emitter.runtime.lodMinSpawnScale = renderer.findFloat("lodMinSpawnScale").value_or(emitter.runtime.lodMinSpawnScale);
            emitter.runtime.lodMinRenderScale = renderer.findFloat("lodMinRenderScale").value_or(emitter.runtime.lodMinRenderScale);
            emitter.runtime.velocityStretch = renderer.findFloat("velocityStretch").value_or(emitter.runtime.velocityStretch);
            emitter.runtime.velocityStretchMax = renderer.findFloat("velocityStretchMax").value_or(emitter.runtime.velocityStretchMax);
            readVec3(renderer, "meshScale", emitter.meshScale);
        }

        {
            const auto* section = source.find("spawn");
            const auto& spawn = section != nullptr && section->isObject() ? *section : source;
            emitter.emissionRate = spawn.findFloat("emissionRate").value_or(emitter.emissionRate);
            emitter.maxParticles = spawn.findUInt32("maxParticles").value_or(emitter.maxParticles);
            emitter.lifetimeMin = spawn.findFloat("lifetimeMin").value_or(emitter.lifetimeMin);
            emitter.lifetimeMax = spawn.findFloat("lifetimeMax").value_or(emitter.lifetimeMax);
        }

        {
            const auto* section = source.find("shape");
            const auto& shape = section != nullptr && section->isObject() ? *section : source;
            if (const auto text = shape.findString("shape"))
                emitter.shape = gts::enumValue(particleEmitterShapeNames, *text).value_or(ParticleEmitterShape::Sphere);
            emitter.sphereRadius = shape.findFloat("sphereRadius").value_or(emitter.sphereRadius);
            readVec3(shape, "boxExtents", emitter.boxExtents);
            emitter.discRadius = shape.findFloat("discRadius").value_or(emitter.discRadius);
            emitter.ringInnerRadius = shape.findFloat("ringInnerRadius").value_or(emitter.ringInnerRadius);
            emitter.ringOuterRadius = shape.findFloat("ringOuterRadius").value_or(emitter.ringOuterRadius);
            emitter.cylinderRadius = shape.findFloat("cylinderRadius").value_or(emitter.cylinderRadius);
            emitter.cylinderHeight = shape.findFloat("cylinderHeight").value_or(emitter.cylinderHeight);
        }

        {
            const auto* section = source.find("velocity");
            const auto& velocity = section != nullptr && section->isObject() ? *section : source;
            readVec3(velocity, "initialVelocity", emitter.initialVelocity);
            emitter.velocitySpread = velocity.findFloat("velocitySpread").value_or(emitter.velocitySpread);
            emitter.radialVelocityMin = velocity.findFloat("radialVelocityMin").value_or(emitter.radialVelocityMin);
            emitter.radialVelocityMax = velocity.findFloat("radialVelocityMax").value_or(emitter.radialVelocityMax);
            emitter.tangentVelocity = velocity.findFloat("tangentVelocity").value_or(emitter.tangentVelocity);
            emitter.drag = velocity.findFloat("drag").value_or(emitter.drag);
        }

        {
            const auto* section = source.find("forces");
            const auto& forces = section != nullptr && section->isObject() ? *section : source;
            if (const auto text = forces.findString("collisionMode"))
                emitter.collision.mode =
                    gts::enumValue(particleCollisionModeNames, *text).value_or(ParticleCollisionMode::None);
            readVec3(forces, "forceAcceleration", emitter.forces.acceleration);
            readVec3(forces, "forceWind", emitter.forces.wind);
            emitter.forces.vortex = forces.findFloat("forceVortex").value_or(emitter.forces.vortex);
            emitter.forces.radial = forces.findFloat("forceRadial").value_or(emitter.forces.radial);
            emitter.forces.noiseStrength = forces.findFloat("forceNoiseStrength").value_or(emitter.forces.noiseStrength);
            emitter.forces.noiseScale = forces.findFloat("forceNoiseScale").value_or(emitter.forces.noiseScale);
            emitter.collision.groundY = forces.findFloat("collisionGroundY").value_or(emitter.collision.groundY);
            emitter.collision.bounce = forces.findFloat("collisionBounce").value_or(emitter.collision.bounce);
            emitter.collision.damping = forces.findFloat("collisionDamping").value_or(emitter.collision.damping);
            emitter.collision.killOnCollision = forces.findBool("killOnCollision").value_or(emitter.collision.killOnCollision);
            emitter.collision.spawnOnDeathCount = forces.findUInt32("spawnOnDeathCount").value_or(emitter.collision.spawnOnDeathCount);
            emitter.collision.spawnOnCollisionCount = forces.findUInt32("spawnOnCollisionCount").value_or(emitter.collision.spawnOnCollisionCount);
            emitter.collision.maxEventSpawnsPerFrame = forces.findUInt32("maxEventSpawnsPerFrame").value_or(emitter.collision.maxEventSpawnsPerFrame);
        }

        {
            const auto* section = source.find("color");
            const auto& color = section != nullptr && section->isObject() ? *section : source;
            emitter.hueVariation = color.findFloat("hueVariation").value_or(emitter.hueVariation);
            emitter.valueVariation = color.findFloat("valueVariation").value_or(emitter.valueVariation);
            readVec4(color, "baseTint", emitter.baseTint);
            readColorCurve(color, "colorOverLifetime", emitter.colorOverLifetime);
            readFloatCurve(color, "alphaOverLifetime", emitter.alphaOverLifetime);
        }

        {
            const auto* section = source.find("size");
            const auto& size = section != nullptr && section->isObject() ? *section : source;
            emitter.sizeRandomness = size.findFloat("sizeRandomness").value_or(emitter.sizeRandomness);
            emitter.aspectRatioMin = size.findFloat("aspectRatioMin").value_or(emitter.aspectRatioMin);
            emitter.aspectRatioMax = size.findFloat("aspectRatioMax").value_or(emitter.aspectRatioMax);
            readFloatCurve(size, "sizeOverLifetime", emitter.sizeOverLifetime);
        }

        {
            const auto* section = source.find("rotation");
            const auto& rotation = section != nullptr && section->isObject() ? *section : source;
            emitter.spinMin = rotation.findFloat("spinMin").value_or(emitter.spinMin);
            emitter.spinMax = rotation.findFloat("spinMax").value_or(emitter.spinMax);
            readVec3(rotation, "meshAngularVelocityMin", emitter.meshAngularVelocityMin);
            readVec3(rotation, "meshAngularVelocityMax", emitter.meshAngularVelocityMax);
            emitter.randomMeshRotation = rotation.findBool("randomMeshRotation").value_or(emitter.randomMeshRotation);
        }

        {
            const auto* section = source.find("flipbook");
            const auto& flipbook = section != nullptr && section->isObject() ? *section : source;
            emitter.flipbook.columns = flipbook.findUInt32("flipbookColumns").value_or(emitter.flipbook.columns);
            emitter.flipbook.rows = flipbook.findUInt32("flipbookRows").value_or(emitter.flipbook.rows);
            emitter.flipbook.frameCount = flipbook.findUInt32("flipbookFrameCount").value_or(emitter.flipbook.frameCount);
            emitter.flipbook.frameRate = flipbook.findFloat("flipbookFrameRate").value_or(emitter.flipbook.frameRate);
            emitter.flipbook.lifetimeDriven = flipbook.findBool("flipbookLifetimeDriven").value_or(emitter.flipbook.lifetimeDriven);
            emitter.flipbook.randomStart = flipbook.findBool("flipbookRandomStart").value_or(emitter.flipbook.randomStart);
        }

        emitter.effectEmitterId = source.findString("effectEmitterId").value_or(emitter.effectEmitterId);
        readBursts(source, emitter);
    }

    GtsJsonValue colorCurveJson(const ParticleColorCurve& curve)
    {
        GtsJsonValue::Array array;
        for (const auto& point : curve)
            array.emplace_back(GtsJsonValue::Array{
                point.t, GtsJsonValue::Array{point.color.x, point.color.y, point.color.z, point.color.w}});
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
            {"id", parameter.id},
            {"type",
             std::string(
                 gts::enumName(gts::particles::particleModuleParameterTypeNames, parameter.type).value_or("float"))},
            {"value", std::move(value)}};
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
            nodes.emplace_back(
                GtsJsonValue::Object{{"id", node.id},
                                     {"moduleStableId", node.moduleStableId},
                                     {"type", node.typeId},
                                     {"displayName", node.displayName},
                                     {"frameId", node.frameId},
                                     {"position", GtsJsonValue::Array{node.position.x, node.position.y}}});
        for (const auto& link : graph.links)
            links.emplace_back(GtsJsonValue::Object{{"id", link.id},
                                                    {"from", link.fromNodeId},
                                                    {"fromPort", link.fromPortId},
                                                    {"to", link.toNodeId},
                                                    {"toPort", link.toPortId}});
        for (const auto& frame : graph.frames)
            frames.emplace_back(
                GtsJsonValue::Object{{"id", frame.id},
                                     {"title", frame.title},
                                     {"position", GtsJsonValue::Array{frame.position.x, frame.position.y}},
                                     {"size", GtsJsonValue::Array{frame.size.x, frame.size.y}}});
        for (const auto& comment : graph.comments)
            comments.emplace_back(
                GtsJsonValue::Object{{"id", comment.id},
                                     {"text", comment.text},
                                     {"position", GtsJsonValue::Array{comment.position.x, comment.position.y}}});
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
        const auto* metadataObject = source.find("metadata");
        if (metadataObject == nullptr || !metadataObject->isObject())
            return;

        metadata.name = metadataObject->findString("name").value_or(metadata.name);
        metadata.description = metadataObject->findString("description").value_or(metadata.description);
        metadata.author = metadataObject->findString("author").value_or(metadata.author);
    }

    void readPreview(const GtsJsonValue& source, ParticleEffectPreviewSettings& preview)
    {
        const auto* previewObject = source.find("preview");
        if (previewObject == nullptr || !previewObject->isObject())
            return;

        readVec4(*previewObject, "backgroundColor", preview.backgroundColor);
        readVec3(*previewObject, "cameraPosition", preview.cameraPosition);
        readVec3(*previewObject, "cameraTarget", preview.cameraTarget);
        preview.orbitDistance = previewObject->findFloat("orbitDistance").value_or(preview.orbitDistance);
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
        const auto id = source.findString("id");
        if (!id || id->empty())
            return false;

        parameter.id = *id;
        if (const auto typeString = source.findString("type"))
        {
            const auto type = gts::enumValue(gts::particles::particleModuleParameterTypeNames, *typeString);
            if (!type)
                return false;
            parameter.type = *type;
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
            if (const auto number = value->tryFloat())
            {
                parameter.floatValue = *number;
                return true;
            }
            return false;
        case gts::particles::ParticleModuleParameterType::UInt:
        case gts::particles::ParticleModuleParameterType::Enum:
            if (const auto number = value->tryUInt32())
            {
                parameter.uintValue = *number;
                return true;
            }
            return false;
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
            module.stableId = moduleObject->findString("id").value_or(module.stableId);
            module.typeId = moduleObject->findString("type").value_or(module.typeId);
            module.displayName = moduleObject->findString("displayName").value_or(module.displayName);
            module.version = moduleObject->findUInt32("version").value_or(module.version);
            module.enabled = moduleObject->findBool("enabled").value_or(module.enabled);

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
        const auto* graphObject = source.find("graph");
        if (graphObject == nullptr || !graphObject->isObject())
            return false;

        ParticleEffectGraph parsed;
        parsed.schemaVersion = graphObject->findUInt32("schemaVersion").value_or(parsed.schemaVersion);

        std::vector<const GtsJsonValue*> nodeObjects;
        if (readObjectArray(*graphObject, "nodes", nodeObjects))
        {
            parsed.nodes.reserve(nodeObjects.size());
            for (const GtsJsonValue* nodeObject : nodeObjects)
            {
                ParticleGraphNode node;
                node.id = nodeObject->findString("id").value_or(node.id);
                node.moduleStableId = nodeObject->findString("moduleStableId").value_or(node.moduleStableId);
                node.typeId = nodeObject->findString("type").value_or(node.typeId);
                node.displayName = nodeObject->findString("displayName").value_or(node.displayName);
                node.frameId = nodeObject->findString("frameId").value_or(node.frameId);
                readVec2(*nodeObject, "position", node.position);
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
                link.id = linkObject->findString("id").value_or(link.id);
                link.fromNodeId = linkObject->findString("from").value_or(link.fromNodeId);
                link.fromPortId = linkObject->findString("fromPort").value_or(link.fromPortId);
                link.toNodeId = linkObject->findString("to").value_or(link.toNodeId);
                link.toPortId = linkObject->findString("toPort").value_or(link.toPortId);
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
                frame.id = frameObject->findString("id").value_or(frame.id);
                frame.title = frameObject->findString("title").value_or(frame.title);
                readVec2(*frameObject, "position", frame.position);
                readVec2(*frameObject, "size", frame.size);
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
                comment.id = commentObject->findString("id").value_or(comment.id);
                comment.text = commentObject->findString("text").value_or(comment.text);
                readVec2(*commentObject, "position", comment.position);
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
            emitter.stableId = emitterObjects[i]->findString("id").value_or(emitter.stableId);
            emitter.name = emitterObjects[i]->findString("name").value_or(emitter.name);
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
             GtsJsonValue::Object{
                 {"primitive",
                  std::string(gts::enumName(particlePrimitiveNames, emitter.primitive).value_or("billboard"))},
                 {"blend", std::string(gts::enumName(particleBlendModeNames, emitter.blend).value_or("alpha"))},
                 {"spriteShape",
                  std::string(gts::enumName(particleSpriteShapeNames, emitter.spriteShape).value_or("softCircle"))},
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
                 {"meshScale", GtsJsonValue::Array{emitter.meshScale.x, emitter.meshScale.y, emitter.meshScale.z}}}},
            {"spawn",
             GtsJsonValue::Object{{"emissionRate", emitter.emissionRate},
                                  {"maxParticles", emitter.maxParticles},
                                  {"lifetimeMin", emitter.lifetimeMin},
                                  {"lifetimeMax", emitter.lifetimeMax}}},
            {"shape",
             GtsJsonValue::Object{
                 {"shape", std::string(gts::enumName(particleEmitterShapeNames, emitter.shape).value_or("sphere"))},
                 {"sphereRadius", emitter.sphereRadius},
                 {"boxExtents", GtsJsonValue::Array{emitter.boxExtents.x, emitter.boxExtents.y, emitter.boxExtents.z}},
                 {"discRadius", emitter.discRadius},
                 {"ringInnerRadius", emitter.ringInnerRadius},
                 {"ringOuterRadius", emitter.ringOuterRadius},
                 {"cylinderRadius", emitter.cylinderRadius},
                 {"cylinderHeight", emitter.cylinderHeight}}},
            {"velocity",
             GtsJsonValue::Object{
                 {"initialVelocity",
                  GtsJsonValue::Array{emitter.initialVelocity.x, emitter.initialVelocity.y, emitter.initialVelocity.z}},
                 {"velocitySpread", emitter.velocitySpread},
                 {"radialVelocityMin", emitter.radialVelocityMin},
                 {"radialVelocityMax", emitter.radialVelocityMax},
                 {"tangentVelocity", emitter.tangentVelocity},
                 {"drag", emitter.drag}}},
            {"forces",
             GtsJsonValue::Object{
                 {"forceAcceleration",
                  GtsJsonValue::Array{
                      emitter.forces.acceleration.x, emitter.forces.acceleration.y, emitter.forces.acceleration.z}},
                 {"forceWind",
                  GtsJsonValue::Array{emitter.forces.wind.x, emitter.forces.wind.y, emitter.forces.wind.z}},
                 {"forceVortex", emitter.forces.vortex},
                 {"forceRadial", emitter.forces.radial},
                 {"forceNoiseStrength", emitter.forces.noiseStrength},
                 {"forceNoiseScale", emitter.forces.noiseScale},
                 {"collisionMode",
                  std::string(gts::enumName(particleCollisionModeNames, emitter.collision.mode).value_or("none"))},
                 {"collisionGroundY", emitter.collision.groundY},
                 {"collisionBounce", emitter.collision.bounce},
                 {"collisionDamping", emitter.collision.damping},
                 {"killOnCollision", emitter.collision.killOnCollision},
                 {"spawnOnDeathCount", emitter.collision.spawnOnDeathCount},
                 {"spawnOnCollisionCount", emitter.collision.spawnOnCollisionCount},
                 {"maxEventSpawnsPerFrame", emitter.collision.maxEventSpawnsPerFrame}}},
            {"color",
             GtsJsonValue::Object{
                 {"baseTint",
                  GtsJsonValue::Array{emitter.baseTint.x, emitter.baseTint.y, emitter.baseTint.z, emitter.baseTint.w}},
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
                                  {"meshAngularVelocityMin",
                                   GtsJsonValue::Array{emitter.meshAngularVelocityMin.x,
                                                       emitter.meshAngularVelocityMin.y,
                                                       emitter.meshAngularVelocityMin.z}},
                                  {"meshAngularVelocityMax",
                                   GtsJsonValue::Array{emitter.meshAngularVelocityMax.x,
                                                       emitter.meshAngularVelocityMax.y,
                                                       emitter.meshAngularVelocityMax.z}},
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
        const auto schema = root.findUInt32("schemaVersion");
        const bool schemaRead = schema.has_value();
        if (schema) loaded.schemaVersion = *schema;
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
                                  GtsJsonValue::Object{{"backgroundColor",
                                                        GtsJsonValue::Array{migrated.preview.backgroundColor.x,
                                                                            migrated.preview.backgroundColor.y,
                                                                            migrated.preview.backgroundColor.z,
                                                                            migrated.preview.backgroundColor.w}},
                                                       {"cameraPosition",
                                                        GtsJsonValue::Array{migrated.preview.cameraPosition.x,
                                                                            migrated.preview.cameraPosition.y,
                                                                            migrated.preview.cameraPosition.z}},
                                                       {"cameraTarget",
                                                        GtsJsonValue::Array{migrated.preview.cameraTarget.x,
                                                                            migrated.preview.cameraTarget.y,
                                                                            migrated.preview.cameraTarget.z}},
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
