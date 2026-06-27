#include "ParticleEffectAssetIO.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace
{
    struct JsonValue
    {
        enum class Type
        {
            Null,
            Bool,
            Number,
            String,
            Array,
            Object
        };

        Type                                     type   = Type::Null;
        bool                                     boolean = false;
        double                                   number  = 0.0;
        std::string                              string;
        std::vector<JsonValue>                   array;
        std::vector<std::pair<std::string, JsonValue>> object;
    };

    void skipWhitespace(const std::string& source, size_t& pos)
    {
        while (pos < source.size() && std::isspace(static_cast<unsigned char>(source[pos])))
            ++pos;
    }

    bool consume(const std::string& source, size_t& pos, char expected)
    {
        skipWhitespace(source, pos);
        if (pos >= source.size() || source[pos] != expected)
            return false;
        ++pos;
        return true;
    }

    bool parseJsonValue(const std::string& source, size_t& pos, JsonValue& value);

    bool parseJsonStringToken(const std::string& source, size_t& pos, std::string& value)
    {
        skipWhitespace(source, pos);
        if (pos >= source.size() || source[pos] != '"')
            return false;
        ++pos;

        std::string result;
        while (pos < source.size())
        {
            const char ch = source[pos++];
            if (ch == '"')
            {
                value = result;
                return true;
            }
            if (ch != '\\')
            {
                result.push_back(ch);
                continue;
            }
            if (pos >= source.size())
                return false;

            const char escaped = source[pos++];
            switch (escaped)
            {
            case '"':
            case '\\':
            case '/':
                result.push_back(escaped);
                break;
            case 'b':
                result.push_back('\b');
                break;
            case 'f':
                result.push_back('\f');
                break;
            case 'n':
                result.push_back('\n');
                break;
            case 'r':
                result.push_back('\r');
                break;
            case 't':
                result.push_back('\t');
                break;
            case 'u':
                if (pos + 4 > source.size())
                    return false;
                result.push_back('?');
                pos += 4;
                break;
            default:
                return false;
            }
        }

        return false;
    }

    bool parseJsonNumberToken(const std::string& source, size_t& pos, double& value)
    {
        skipWhitespace(source, pos);
        const char* begin = source.c_str() + pos;
        char*       end   = nullptr;
        value             = std::strtod(begin, &end);
        if (end == begin)
            return false;
        pos = static_cast<size_t>(end - source.c_str());
        return true;
    }

    bool parseJsonArrayToken(const std::string& source, size_t& pos, JsonValue& value)
    {
        if (!consume(source, pos, '['))
            return false;

        JsonValue parsed;
        parsed.type = JsonValue::Type::Array;
        while (pos < source.size())
        {
            skipWhitespace(source, pos);
            if (pos < source.size() && source[pos] == ']')
            {
                ++pos;
                value = std::move(parsed);
                return true;
            }

            JsonValue item;
            if (!parseJsonValue(source, pos, item))
                return false;
            parsed.array.push_back(std::move(item));

            skipWhitespace(source, pos);
            if (pos < source.size() && source[pos] == ',')
            {
                ++pos;
                continue;
            }
            if (pos < source.size() && source[pos] == ']')
                continue;
            return false;
        }

        return false;
    }

    bool parseJsonObjectToken(const std::string& source, size_t& pos, JsonValue& value)
    {
        if (!consume(source, pos, '{'))
            return false;

        JsonValue parsed;
        parsed.type = JsonValue::Type::Object;
        while (pos < source.size())
        {
            skipWhitespace(source, pos);
            if (pos < source.size() && source[pos] == '}')
            {
                ++pos;
                value = std::move(parsed);
                return true;
            }

            std::string key;
            if (!parseJsonStringToken(source, pos, key))
                return false;
            if (!consume(source, pos, ':'))
                return false;

            JsonValue member;
            if (!parseJsonValue(source, pos, member))
                return false;
            parsed.object.push_back({std::move(key), std::move(member)});

            skipWhitespace(source, pos);
            if (pos < source.size() && source[pos] == ',')
            {
                ++pos;
                continue;
            }
            if (pos < source.size() && source[pos] == '}')
                continue;
            return false;
        }

        return false;
    }

    bool parseJsonValue(const std::string& source, size_t& pos, JsonValue& value)
    {
        skipWhitespace(source, pos);
        if (pos >= source.size())
            return false;

        if (source[pos] == '{')
            return parseJsonObjectToken(source, pos, value);
        if (source[pos] == '[')
            return parseJsonArrayToken(source, pos, value);
        if (source[pos] == '"')
        {
            JsonValue parsed;
            parsed.type = JsonValue::Type::String;
            if (!parseJsonStringToken(source, pos, parsed.string))
                return false;
            value = std::move(parsed);
            return true;
        }
        if (source.compare(pos, 4, "true") == 0)
        {
            value.type    = JsonValue::Type::Bool;
            value.boolean = true;
            pos += 4;
            return true;
        }
        if (source.compare(pos, 5, "false") == 0)
        {
            value.type    = JsonValue::Type::Bool;
            value.boolean = false;
            pos += 5;
            return true;
        }
        if (source.compare(pos, 4, "null") == 0)
        {
            value = JsonValue{};
            pos += 4;
            return true;
        }

        JsonValue parsed;
        parsed.type = JsonValue::Type::Number;
        if (!parseJsonNumberToken(source, pos, parsed.number))
            return false;
        value = parsed;
        return true;
    }

    bool parseJson(const std::string& source, JsonValue& value)
    {
        size_t pos = 0;
        if (!parseJsonValue(source, pos, value))
            return false;
        skipWhitespace(source, pos);
        return pos == source.size();
    }

    const JsonValue* findMember(const JsonValue& source, const std::string& key)
    {
        if (source.type != JsonValue::Type::Object)
            return nullptr;
        for (const auto& entry : source.object)
        {
            if (entry.first == key)
                return &entry.second;
        }
        return nullptr;
    }

    const JsonValue* findTypedMemberDeep(const JsonValue& source, const std::string& key, JsonValue::Type type)
    {
        if (source.type != JsonValue::Type::Object)
            return nullptr;

        for (const auto& entry : source.object)
        {
            if (entry.first == key && entry.second.type == type)
                return &entry.second;
        }
        for (const auto& entry : source.object)
        {
            if (entry.second.type == JsonValue::Type::Object)
            {
                if (const JsonValue* found = findTypedMemberDeep(entry.second, key, type))
                    return found;
            }
        }
        return nullptr;
    }

    const JsonValue* findTypedMember(const JsonValue& source,
                                     const std::string& key,
                                     JsonValue::Type    type,
                                     bool               deep = true)
    {
        const JsonValue* direct = findMember(source, key);
        if (direct != nullptr && direct->type == type)
            return direct;
        return deep ? findTypedMemberDeep(source, key, type) : nullptr;
    }

    bool readObject(const JsonValue& source, const std::string& key, const JsonValue*& objectValue)
    {
        objectValue = findTypedMember(source, key, JsonValue::Type::Object, false);
        return objectValue != nullptr;
    }

    bool readObjectArray(const JsonValue& source,
                         const std::string& key,
                         std::vector<const JsonValue*>& objectValues)
    {
        const JsonValue* arrayValue = findTypedMember(source, key, JsonValue::Type::Array, false);
        if (arrayValue == nullptr)
            return false;

        std::vector<const JsonValue*> parsed;
        parsed.reserve(arrayValue->array.size());
        for (const JsonValue& item : arrayValue->array)
        {
            if (item.type != JsonValue::Type::Object)
                return false;
            parsed.push_back(&item);
        }

        objectValues = std::move(parsed);
        return true;
    }

    bool readString(const JsonValue& source, const std::string& key, std::string& value, bool deep = true)
    {
        const JsonValue* stringValue = findTypedMember(source, key, JsonValue::Type::String, deep);
        if (stringValue == nullptr)
            return false;
        value = stringValue->string;
        return true;
    }

    bool readFloat(const JsonValue& source, const std::string& key, float& value, bool deep = true)
    {
        const JsonValue* numberValue = findTypedMember(source, key, JsonValue::Type::Number, deep);
        if (numberValue == nullptr)
            return false;
        value = static_cast<float>(numberValue->number);
        return true;
    }

    bool readUint(const JsonValue& source, const std::string& key, uint32_t& value, bool deep = true)
    {
        float parsed = 0.0f;
        if (!readFloat(source, key, parsed, deep))
            return false;
        value = static_cast<uint32_t>(std::max(0.0f, parsed));
        return true;
    }

    bool readBool(const JsonValue& source, const std::string& key, bool& value, bool deep = true)
    {
        const JsonValue* boolValue = findTypedMember(source, key, JsonValue::Type::Bool, deep);
        if (boolValue == nullptr)
            return false;
        value = boolValue->boolean;
        return true;
    }

    bool readVec3Value(const JsonValue& source, glm::vec3& value)
    {
        if (source.type != JsonValue::Type::Array || source.array.size() != 3u)
            return false;
        for (const JsonValue& item : source.array)
        {
            if (item.type != JsonValue::Type::Number)
                return false;
        }
        value = {static_cast<float>(source.array[0].number),
                 static_cast<float>(source.array[1].number),
                 static_cast<float>(source.array[2].number)};
        return true;
    }

    bool readVec4Value(const JsonValue& source, glm::vec4& value)
    {
        if (source.type != JsonValue::Type::Array || source.array.size() != 4u)
            return false;
        for (const JsonValue& item : source.array)
        {
            if (item.type != JsonValue::Type::Number)
                return false;
        }
        value = {static_cast<float>(source.array[0].number),
                 static_cast<float>(source.array[1].number),
                 static_cast<float>(source.array[2].number),
                 static_cast<float>(source.array[3].number)};
        return true;
    }

    bool readVec3(const JsonValue& source, const std::string& key, glm::vec3& value, bool deep = true)
    {
        const JsonValue* arrayValue = findTypedMember(source, key, JsonValue::Type::Array, deep);
        return arrayValue != nullptr && readVec3Value(*arrayValue, value);
    }

    bool readVec4(const JsonValue& source, const std::string& key, glm::vec4& value, bool deep = true)
    {
        const JsonValue* arrayValue = findTypedMember(source, key, JsonValue::Type::Array, deep);
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

    bool readColorCurveValue(const JsonValue& source, ParticleColorCurve& curve)
    {
        if (source.type != JsonValue::Type::Array)
            return false;

        ParticleColorCurve parsed;
        for (const JsonValue& itemValue : source.array)
        {
            if (itemValue.type != JsonValue::Type::Array || itemValue.array.size() != 2u ||
                itemValue.array[0].type != JsonValue::Type::Number)
                return false;
            ParticleColorKey item;
            item.t = static_cast<float>(itemValue.array[0].number);
            if (!readVec4Value(itemValue.array[1], item.color))
                return false;
            parsed.push_back(item);
        }

        curve = std::move(parsed);
        return true;
    }

    bool readColorCurve(const JsonValue& source, const std::string& key, ParticleColorCurve& curve, bool deep = true)
    {
        const JsonValue* arrayValue = findTypedMember(source, key, JsonValue::Type::Array, deep);
        return arrayValue != nullptr && readColorCurveValue(*arrayValue, curve);
    }

    bool readFloatCurveValue(const JsonValue& source, ParticleFloatCurve& curve)
    {
        if (source.type != JsonValue::Type::Array)
            return false;

        ParticleFloatCurve parsed;
        for (const JsonValue& itemValue : source.array)
        {
            if (itemValue.type != JsonValue::Type::Array || itemValue.array.size() != 2u ||
                itemValue.array[0].type != JsonValue::Type::Number ||
                itemValue.array[1].type != JsonValue::Type::Number)
                return false;
            ParticleFloatKey item;
            item.t     = static_cast<float>(itemValue.array[0].number);
            item.value = static_cast<float>(itemValue.array[1].number);
            parsed.push_back(item);
        }

        curve = std::move(parsed);
        return true;
    }

    bool readFloatCurve(const JsonValue& source, const std::string& key, ParticleFloatCurve& curve, bool deep = true)
    {
        const JsonValue* arrayValue = findTypedMember(source, key, JsonValue::Type::Array, deep);
        return arrayValue != nullptr && readFloatCurveValue(*arrayValue, curve);
    }

    bool readBurstsValue(const JsonValue& source, std::vector<ParticleBurst>& bursts)
    {
        if (source.type != JsonValue::Type::Array)
            return false;

        std::vector<ParticleBurst> parsed;
        for (const JsonValue& itemValue : source.array)
        {
            if (itemValue.type != JsonValue::Type::Array || itemValue.array.size() != 5u)
                return false;
            ParticleBurst burst;
            for (const JsonValue& numberValue : itemValue.array)
            {
                if (numberValue.type != JsonValue::Type::Number)
                    return false;
            }
            const float countMin    = static_cast<float>(itemValue.array[1].number);
            const float countMax    = static_cast<float>(itemValue.array[2].number);
            const float repeatCount = static_cast<float>(itemValue.array[4].number);
            burst.time             = static_cast<float>(itemValue.array[0].number);
            burst.repeatInterval   = static_cast<float>(itemValue.array[3].number);
            burst.countMin    = static_cast<uint32_t>(std::max(0.0f, countMin));
            burst.countMax    = static_cast<uint32_t>(std::max(countMin, countMax));
            burst.repeatCount = static_cast<uint32_t>(std::max(0.0f, repeatCount));
            parsed.push_back(burst);
        }

        bursts = std::move(parsed);
        return true;
    }

    bool readBursts(const JsonValue& source, ParticleEmitterComponent& emitter)
    {
        const JsonValue* arrayValue = findTypedMember(source, "bursts", JsonValue::Type::Array, true);
        return arrayValue != nullptr && readBurstsValue(*arrayValue, emitter.bursts);
    }

    void readEmitter(const JsonValue& source, ParticleEmitterComponent& emitter)
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
        readString(source, "texturePath", emitter.texturePath);
        readString(source, "effectEmitterId", emitter.effectEmitterId);
        readString(source, "meshPath", emitter.meshPath);
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
    }

    std::string escaped(const std::string& value)
    {
        std::string result;
        result.reserve(value.size());
        for (char ch : value)
        {
            if (ch == '"' || ch == '\\')
                result.push_back('\\');
            result.push_back(ch);
        }
        return result;
    }

    void writeVec3(std::ostream& out, const glm::vec3& value)
    {
        out << '[' << value.x << ", " << value.y << ", " << value.z << ']';
    }

    void writeVec4(std::ostream& out, const glm::vec4& value)
    {
        out << '[' << value.x << ", " << value.y << ", " << value.z << ", " << value.w << ']';
    }

    void writeColorCurve(std::ostream& out, const ParticleColorCurve& curve)
    {
        out << '[';
        for (size_t i = 0; i < curve.size(); ++i)
        {
            if (i > 0)
                out << ", ";
            out << '[' << curve[i].t << ", ";
            writeVec4(out, curve[i].color);
            out << ']';
        }
        out << ']';
    }

    void writeFloatCurve(std::ostream& out, const ParticleFloatCurve& curve)
    {
        out << '[';
        for (size_t i = 0; i < curve.size(); ++i)
        {
            if (i > 0)
                out << ", ";
            out << '[' << curve[i].t << ", " << curve[i].value << ']';
        }
        out << ']';
    }

    void writeBursts(std::ostream& out, const std::vector<ParticleBurst>& bursts)
    {
        out << '[';
        for (size_t i = 0; i < bursts.size(); ++i)
        {
            if (i > 0)
                out << ", ";
            const ParticleBurst& burst = bursts[i];
            out << '[' << burst.time << ", " << burst.countMin << ", " << burst.countMax << ", " << burst.repeatInterval
                << ", " << burst.repeatCount << ']';
        }
        out << ']';
    }

    void writeModuleParameter(std::ostream& out, const gts::particles::ParticleModuleParameter& parameter, bool last)
    {
        out << "          {\"id\": \"" << escaped(parameter.id) << "\", \"type\": \""
            << moduleParameterTypeToString(parameter.type) << "\", \"value\": ";
        switch (parameter.type)
        {
        case gts::particles::ParticleModuleParameterType::Float:
            out << parameter.floatValue;
            break;
        case gts::particles::ParticleModuleParameterType::UInt:
        case gts::particles::ParticleModuleParameterType::Enum:
            out << parameter.uintValue;
            break;
        case gts::particles::ParticleModuleParameterType::Bool:
            out << (parameter.boolValue ? "true" : "false");
            break;
        case gts::particles::ParticleModuleParameterType::String:
            out << '"' << escaped(parameter.stringValue) << '"';
            break;
        case gts::particles::ParticleModuleParameterType::FloatCurve:
            writeFloatCurve(out, parameter.floatCurveValue);
            break;
        case gts::particles::ParticleModuleParameterType::ColorGradient:
            writeColorCurve(out, parameter.colorGradientValue);
            break;
        case gts::particles::ParticleModuleParameterType::BurstTimeline:
            writeBursts(out, parameter.burstTimelineValue);
            break;
        }
        out << "}" << (last ? "\n" : ",\n");
    }

    void writeModuleObject(std::ostream& out, const gts::particles::ParticleModuleInstance& module, bool last)
    {
        out << "        {\n";
        out << "          \"id\": \"" << escaped(module.stableId) << "\",\n";
        out << "          \"type\": \"" << escaped(module.typeId) << "\",\n";
        out << "          \"displayName\": \"" << escaped(module.displayName) << "\",\n";
        out << "          \"version\": " << module.version << ",\n";
        out << "          \"enabled\": " << (module.enabled ? "true" : "false") << ",\n";
        out << "          \"parameters\": [\n";
        for (size_t i = 0; i < module.parameters.size(); ++i)
            writeModuleParameter(out, module.parameters[i], i + 1 == module.parameters.size());
        out << "          ]\n";
        out << "        }" << (last ? "\n" : ",\n");
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

    void readMetadata(const JsonValue& source, ParticleEffectMetadata& metadata)
    {
        const JsonValue* metadataObject = nullptr;
        if (!readObject(source, "metadata", metadataObject))
            return;

        readString(*metadataObject, "name", metadata.name, false);
        readString(*metadataObject, "description", metadata.description, false);
        readString(*metadataObject, "author", metadata.author, false);
    }

    void readPreview(const JsonValue& source, ParticleEffectPreviewSettings& preview)
    {
        const JsonValue* previewObject = nullptr;
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

    bool readModuleParameter(const JsonValue&                                source,
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

        const JsonValue* value = findMember(source, "value");
        if (value == nullptr)
            return true;

        switch (parameter.type)
        {
        case gts::particles::ParticleModuleParameterType::Float:
            if (value->type != JsonValue::Type::Number)
                return false;
            parameter.floatValue = static_cast<float>(value->number);
            return true;
        case gts::particles::ParticleModuleParameterType::UInt:
        case gts::particles::ParticleModuleParameterType::Enum:
            if (value->type != JsonValue::Type::Number)
                return false;
            parameter.uintValue = static_cast<uint32_t>(std::max(0.0, value->number));
            return true;
        case gts::particles::ParticleModuleParameterType::Bool:
            if (value->type != JsonValue::Type::Bool)
                return false;
            parameter.boolValue = value->boolean;
            return true;
        case gts::particles::ParticleModuleParameterType::String:
            if (value->type != JsonValue::Type::String)
                return false;
            parameter.stringValue = value->string;
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

    bool readEmitterModules(const JsonValue& source, std::vector<gts::particles::ParticleModuleInstance>& modules)
    {
        std::vector<const JsonValue*> moduleObjects;
        if (!readObjectArray(source, "modules", moduleObjects))
            return false;

        std::vector<gts::particles::ParticleModuleInstance> parsedModules;
        parsedModules.reserve(moduleObjects.size());
        for (const JsonValue* moduleObject : moduleObjects)
        {
            gts::particles::ParticleModuleInstance module;
            readString(*moduleObject, "id", module.stableId, false);
            readString(*moduleObject, "type", module.typeId, false);
            readString(*moduleObject, "displayName", module.displayName, false);
            readUint(*moduleObject, "version", module.version, false);
            readBool(*moduleObject, "enabled", module.enabled, false);

            const gts::particles::ParticleModuleDefinition* definition =
                gts::particles::findParticleModuleDefinition(module.typeId);
            std::vector<const JsonValue*> parameterObjects;
            if (readObjectArray(*moduleObject, "parameters", parameterObjects))
            {
                module.parameters.reserve(parameterObjects.size());
                for (const JsonValue* parameterObject : parameterObjects)
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

    bool readEffectEmitters(const JsonValue& source, ParticleEffectAsset& asset)
    {
        std::vector<const JsonValue*> emitterObjects;
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
            emitters.push_back(std::move(emitter));
        }

        asset.emitters = std::move(emitters);
        return true;
    }

    void writeEmitterObject(std::ostream& out, const ParticleEffectEmitter& effectEmitter, bool last)
    {
        const ParticleEmitterComponent& emitter = effectEmitter.descriptor;

        out << "    {\n";
        out << "      \"id\": \"" << escaped(effectEmitter.stableId) << "\",\n";
        out << "      \"name\": \"" << escaped(effectEmitter.name) << "\",\n";

        out << "      \"modules\": [\n";
        for (size_t i = 0; i < effectEmitter.modules.size(); ++i)
            writeModuleObject(out, effectEmitter.modules[i], i + 1 == effectEmitter.modules.size());
        out << "      ],\n";

        out << "      \"simulation\": {\n";
        out << "        \"schemaVersion\": " << emitter.schemaVersion << ",\n";
        out << "        \"enabled\": " << (emitter.enabled ? "true" : "false") << ",\n";
        out << "        \"localSpace\": " << (emitter.localSpace ? "true" : "false") << ",\n";
        out << "        \"looping\": " << (emitter.looping ? "true" : "false") << ",\n";
        out << "        \"duration\": " << emitter.duration << ",\n";
        out << "        \"startDelay\": " << emitter.startDelay << ",\n";
        out << "        \"intensity\": " << emitter.intensity << "\n";
        out << "      },\n";

        out << "      \"renderer\": {\n";
        out << "        \"primitive\": \"" << primitiveToString(emitter.primitive) << "\",\n";
        out << "        \"blend\": \"" << blendToString(emitter.blend) << "\",\n";
        out << "        \"spriteShape\": \"" << spriteShapeToString(emitter.spriteShape) << "\",\n";
        out << "        \"texturePath\": \"" << escaped(emitter.texturePath) << "\",\n";
        out << "        \"meshPath\": \"" << escaped(emitter.meshPath) << "\",\n";
        out << "        \"spriteEdgeSoftness\": " << emitter.spriteEdgeSoftness << ",\n";
        out << "        \"softness\": " << emitter.softness << ",\n";
        out << "        \"meshScale\": ";
        writeVec3(out, emitter.meshScale);
        out << "\n";
        out << "      },\n";

        out << "      \"spawn\": {\n";
        out << "        \"emissionRate\": " << emitter.emissionRate << ",\n";
        out << "        \"maxParticles\": " << emitter.maxParticles << ",\n";
        out << "        \"lifetimeMin\": " << emitter.lifetimeMin << ",\n";
        out << "        \"lifetimeMax\": " << emitter.lifetimeMax << "\n";
        out << "      },\n";

        out << "      \"shape\": {\n";
        out << "        \"shape\": \"" << shapeToString(emitter.shape) << "\",\n";
        out << "        \"sphereRadius\": " << emitter.sphereRadius << ",\n";
        out << "        \"boxExtents\": ";
        writeVec3(out, emitter.boxExtents);
        out << ",\n";
        out << "        \"discRadius\": " << emitter.discRadius << ",\n";
        out << "        \"ringInnerRadius\": " << emitter.ringInnerRadius << ",\n";
        out << "        \"ringOuterRadius\": " << emitter.ringOuterRadius << ",\n";
        out << "        \"cylinderRadius\": " << emitter.cylinderRadius << ",\n";
        out << "        \"cylinderHeight\": " << emitter.cylinderHeight << "\n";
        out << "      },\n";

        out << "      \"velocity\": {\n";
        out << "        \"initialVelocity\": ";
        writeVec3(out, emitter.initialVelocity);
        out << ",\n";
        out << "        \"velocitySpread\": " << emitter.velocitySpread << ",\n";
        out << "        \"radialVelocityMin\": " << emitter.radialVelocityMin << ",\n";
        out << "        \"radialVelocityMax\": " << emitter.radialVelocityMax << ",\n";
        out << "        \"tangentVelocity\": " << emitter.tangentVelocity << ",\n";
        out << "        \"drag\": " << emitter.drag << "\n";
        out << "      },\n";

        out << "      \"forces\": {\n";
        out << "        \"forceAcceleration\": ";
        writeVec3(out, emitter.forces.acceleration);
        out << ",\n";
        out << "        \"forceWind\": ";
        writeVec3(out, emitter.forces.wind);
        out << ",\n";
        out << "        \"forceVortex\": " << emitter.forces.vortex << ",\n";
        out << "        \"forceRadial\": " << emitter.forces.radial << ",\n";
        out << "        \"forceNoiseStrength\": " << emitter.forces.noiseStrength << ",\n";
        out << "        \"forceNoiseScale\": " << emitter.forces.noiseScale << "\n";
        out << "      },\n";

        out << "      \"color\": {\n";
        out << "        \"baseTint\": ";
        writeVec4(out, emitter.baseTint);
        out << ",\n";
        out << "        \"hueVariation\": " << emitter.hueVariation << ",\n";
        out << "        \"valueVariation\": " << emitter.valueVariation << ",\n";
        out << "        \"colorOverLifetime\": ";
        writeColorCurve(out, emitter.colorOverLifetime);
        out << ",\n";
        out << "        \"alphaOverLifetime\": ";
        writeFloatCurve(out, emitter.alphaOverLifetime);
        out << "\n";
        out << "      },\n";

        out << "      \"size\": {\n";
        out << "        \"sizeRandomness\": " << emitter.sizeRandomness << ",\n";
        out << "        \"aspectRatioMin\": " << emitter.aspectRatioMin << ",\n";
        out << "        \"aspectRatioMax\": " << emitter.aspectRatioMax << ",\n";
        out << "        \"sizeOverLifetime\": ";
        writeFloatCurve(out, emitter.sizeOverLifetime);
        out << "\n";
        out << "      },\n";

        out << "      \"rotation\": {\n";
        out << "        \"spinMin\": " << emitter.spinMin << ",\n";
        out << "        \"spinMax\": " << emitter.spinMax << ",\n";
        out << "        \"meshAngularVelocityMin\": ";
        writeVec3(out, emitter.meshAngularVelocityMin);
        out << ",\n";
        out << "        \"meshAngularVelocityMax\": ";
        writeVec3(out, emitter.meshAngularVelocityMax);
        out << ",\n";
        out << "        \"randomMeshRotation\": " << (emitter.randomMeshRotation ? "true" : "false") << "\n";
        out << "      },\n";

        out << "      \"bursts\": ";
        writeBursts(out, emitter.bursts);
        out << ",\n";

        out << "      \"flipbook\": {\n";
        out << "        \"flipbookColumns\": " << emitter.flipbook.columns << ",\n";
        out << "        \"flipbookRows\": " << emitter.flipbook.rows << ",\n";
        out << "        \"flipbookFrameCount\": " << emitter.flipbook.frameCount << ",\n";
        out << "        \"flipbookFrameRate\": " << emitter.flipbook.frameRate << ",\n";
        out << "        \"flipbookLifetimeDriven\": " << (emitter.flipbook.lifetimeDriven ? "true" : "false") << ",\n";
        out << "        \"flipbookRandomStart\": " << (emitter.flipbook.randomStart ? "true" : "false") << "\n";
        out << "      }\n";
        out << "    }" << (last ? "\n" : ",\n");
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

        JsonValue root;
        if (!parseJson(source, root) || root.type != JsonValue::Type::Object)
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

        const std::filesystem::path parent = std::filesystem::path(path).parent_path();
        if (!parent.empty())
            std::filesystem::create_directories(parent);
        std::ofstream out(path);
        if (!out)
            return false;

        out << std::fixed << std::setprecision(4);
        out << "{\n";
        out << "  \"schemaVersion\": " << migrated.schemaVersion << ",\n";
        out << "  \"metadata\": {\n";
        out << "    \"name\": \"" << escaped(migrated.metadata.name) << "\",\n";
        out << "    \"description\": \"" << escaped(migrated.metadata.description) << "\",\n";
        out << "    \"author\": \"" << escaped(migrated.metadata.author) << "\"\n";
        out << "  },\n";

        out << "  \"preview\": {\n";
        out << "    \"backgroundColor\": ";
        writeVec4(out, migrated.preview.backgroundColor);
        out << ",\n";
        out << "    \"cameraPosition\": ";
        writeVec3(out, migrated.preview.cameraPosition);
        out << ",\n";
        out << "    \"cameraTarget\": ";
        writeVec3(out, migrated.preview.cameraTarget);
        out << ",\n";
        out << "    \"orbitDistance\": " << migrated.preview.orbitDistance << "\n";
        out << "  },\n";

        out << "  \"emitters\": [\n";
        for (size_t i = 0; i < migrated.emitters.size(); ++i)
        {
            writeEmitterObject(out, migrated.emitters[i], i + 1 == migrated.emitters.size());
        }
        out << "  ]\n";
        out << "}\n";
        return true;
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

        ParticleEmitterComponent loaded = selected->descriptor;
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
