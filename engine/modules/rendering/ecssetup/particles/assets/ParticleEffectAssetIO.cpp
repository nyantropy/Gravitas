#include "ParticleEffectAssetIO.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace
{
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

    bool parseStringAt(const std::string& source, size_t& pos, std::string& value)
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
            if (ch == '\\' && pos < source.size())
            {
                result.push_back(source[pos++]);
                continue;
            }
            result.push_back(ch);
        }

        return false;
    }

    bool parseFloatAt(const std::string& source, size_t& pos, float& value)
    {
        skipWhitespace(source, pos);
        size_t end = pos;
        while (end < source.size())
        {
            const char ch = source[end];
            if (!std::isdigit(static_cast<unsigned char>(ch)) && ch != '-' && ch != '+' && ch != '.' && ch != 'e' &&
                ch != 'E')
            {
                break;
            }
            ++end;
        }

        if (end == pos)
            return false;

        value = std::stof(source.substr(pos, end - pos));
        pos   = end;
        return true;
    }

    bool parseBoolAt(const std::string& source, size_t& pos, bool& value)
    {
        skipWhitespace(source, pos);
        if (source.compare(pos, 4, "true") == 0)
        {
            value = true;
            pos += 4;
            return true;
        }
        if (source.compare(pos, 5, "false") == 0)
        {
            value = false;
            pos += 5;
            return true;
        }
        return false;
    }

    bool parseVec3At(const std::string& source, size_t& pos, glm::vec3& value)
    {
        if (!consume(source, pos, '['))
            return false;
        if (!parseFloatAt(source, pos, value.x))
            return false;
        if (!consume(source, pos, ','))
            return false;
        if (!parseFloatAt(source, pos, value.y))
            return false;
        if (!consume(source, pos, ','))
            return false;
        if (!parseFloatAt(source, pos, value.z))
            return false;
        return consume(source, pos, ']');
    }

    bool parseVec4At(const std::string& source, size_t& pos, glm::vec4& value)
    {
        if (!consume(source, pos, '['))
            return false;
        if (!parseFloatAt(source, pos, value.x))
            return false;
        if (!consume(source, pos, ','))
            return false;
        if (!parseFloatAt(source, pos, value.y))
            return false;
        if (!consume(source, pos, ','))
            return false;
        if (!parseFloatAt(source, pos, value.z))
            return false;
        if (!consume(source, pos, ','))
            return false;
        if (!parseFloatAt(source, pos, value.w))
            return false;
        return consume(source, pos, ']');
    }

    bool parseObjectAt(const std::string& source, size_t& pos, std::string& objectSource)
    {
        skipWhitespace(source, pos);
        if (pos >= source.size() || source[pos] != '{')
            return false;

        const size_t start    = pos;
        uint32_t     depth    = 0;
        bool         inString = false;
        bool         escaped  = false;
        for (; pos < source.size(); ++pos)
        {
            const char ch = source[pos];
            if (inString)
            {
                if (escaped)
                {
                    escaped = false;
                    continue;
                }
                if (ch == '\\')
                {
                    escaped = true;
                    continue;
                }
                if (ch == '"')
                    inString = false;
                continue;
            }

            if (ch == '"')
            {
                inString = true;
                continue;
            }
            if (ch == '{')
            {
                ++depth;
                continue;
            }
            if (ch != '}')
                continue;

            if (depth == 0)
                return false;
            --depth;
            if (depth == 0)
            {
                ++pos;
                objectSource = source.substr(start, pos - start);
                return true;
            }
        }

        return false;
    }

    size_t findValue(const std::string& source, const std::string& key)
    {
        const std::string quotedKey = "\"" + key + "\"";
        const size_t      keyPos    = source.find(quotedKey);
        if (keyPos == std::string::npos)
            return std::string::npos;

        const size_t colon = source.find(':', keyPos + quotedKey.size());
        if (colon == std::string::npos)
            return std::string::npos;

        return colon + 1;
    }

    bool readObject(const std::string& source, const std::string& key, std::string& objectSource)
    {
        size_t pos = findValue(source, key);
        return pos != std::string::npos && parseObjectAt(source, pos, objectSource);
    }

    bool readObjectArray(const std::string& source, const std::string& key, std::vector<std::string>& objectSources)
    {
        size_t pos = findValue(source, key);
        if (pos == std::string::npos || !consume(source, pos, '['))
            return false;

        std::vector<std::string> parsed;
        while (pos < source.size())
        {
            skipWhitespace(source, pos);
            if (pos < source.size() && source[pos] == ']')
            {
                ++pos;
                objectSources = std::move(parsed);
                return true;
            }

            std::string objectSource;
            if (!parseObjectAt(source, pos, objectSource))
                return false;
            parsed.push_back(std::move(objectSource));

            skipWhitespace(source, pos);
            if (pos < source.size() && source[pos] == ',')
                ++pos;
        }

        return false;
    }

    bool readString(const std::string& source, const std::string& key, std::string& value)
    {
        const std::string quotedKey   = "\"" + key + "\"";
        size_t            searchStart = 0;
        while (searchStart < source.size())
        {
            const size_t keyPos = source.find(quotedKey, searchStart);
            if (keyPos == std::string::npos)
                return false;

            const size_t colon = source.find(':', keyPos + quotedKey.size());
            if (colon == std::string::npos)
                return false;

            size_t valuePos = colon + 1;
            if (parseStringAt(source, valuePos, value))
                return true;

            searchStart = keyPos + quotedKey.size();
        }

        return false;
    }

    bool readFloat(const std::string& source, const std::string& key, float& value)
    {
        size_t pos = findValue(source, key);
        return pos != std::string::npos && parseFloatAt(source, pos, value);
    }

    bool readUint(const std::string& source, const std::string& key, uint32_t& value)
    {
        float parsed = 0.0f;
        if (!readFloat(source, key, parsed))
            return false;
        value = static_cast<uint32_t>(std::max(0.0f, parsed));
        return true;
    }

    bool readBool(const std::string& source, const std::string& key, bool& value)
    {
        size_t pos = findValue(source, key);
        return pos != std::string::npos && parseBoolAt(source, pos, value);
    }

    bool readVec3(const std::string& source, const std::string& key, glm::vec3& value)
    {
        size_t pos = findValue(source, key);
        return pos != std::string::npos && parseVec3At(source, pos, value);
    }

    bool readVec4(const std::string& source, const std::string& key, glm::vec4& value)
    {
        size_t pos = findValue(source, key);
        return pos != std::string::npos && parseVec4At(source, pos, value);
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

    bool readColorCurve(const std::string& source, const std::string& key, ParticleColorCurve& curve)
    {
        size_t pos = findValue(source, key);
        if (pos == std::string::npos || !consume(source, pos, '['))
            return false;

        ParticleColorCurve parsed;
        while (pos < source.size())
        {
            skipWhitespace(source, pos);
            if (pos < source.size() && source[pos] == ']')
            {
                ++pos;
                curve = std::move(parsed);
                return true;
            }

            if (!consume(source, pos, '['))
                return false;
            ParticleColorKey item;
            if (!parseFloatAt(source, pos, item.t))
                return false;
            if (!consume(source, pos, ','))
                return false;
            if (!parseVec4At(source, pos, item.color))
                return false;
            if (!consume(source, pos, ']'))
                return false;
            parsed.push_back(item);

            skipWhitespace(source, pos);
            if (pos < source.size() && source[pos] == ',')
                ++pos;
        }

        return false;
    }

    bool readFloatCurve(const std::string& source, const std::string& key, ParticleFloatCurve& curve)
    {
        size_t pos = findValue(source, key);
        if (pos == std::string::npos || !consume(source, pos, '['))
            return false;

        ParticleFloatCurve parsed;
        while (pos < source.size())
        {
            skipWhitespace(source, pos);
            if (pos < source.size() && source[pos] == ']')
            {
                ++pos;
                curve = std::move(parsed);
                return true;
            }

            if (!consume(source, pos, '['))
                return false;
            ParticleFloatKey item;
            if (!parseFloatAt(source, pos, item.t))
                return false;
            if (!consume(source, pos, ','))
                return false;
            if (!parseFloatAt(source, pos, item.value))
                return false;
            if (!consume(source, pos, ']'))
                return false;
            parsed.push_back(item);

            skipWhitespace(source, pos);
            if (pos < source.size() && source[pos] == ',')
                ++pos;
        }

        return false;
    }

    bool readBursts(const std::string& source, ParticleEmitterComponent& emitter)
    {
        size_t pos = findValue(source, "bursts");
        if (pos == std::string::npos || !consume(source, pos, '['))
            return false;

        std::vector<ParticleBurst> parsed;
        while (pos < source.size())
        {
            skipWhitespace(source, pos);
            if (pos < source.size() && source[pos] == ']')
            {
                ++pos;
                emitter.bursts = std::move(parsed);
                return true;
            }

            if (!consume(source, pos, '['))
                return false;
            ParticleBurst burst;
            float         countMin    = 0.0f;
            float         countMax    = 0.0f;
            float         repeatCount = 0.0f;
            if (!parseFloatAt(source, pos, burst.time))
                return false;
            if (!consume(source, pos, ','))
                return false;
            if (!parseFloatAt(source, pos, countMin))
                return false;
            if (!consume(source, pos, ','))
                return false;
            if (!parseFloatAt(source, pos, countMax))
                return false;
            if (!consume(source, pos, ','))
                return false;
            if (!parseFloatAt(source, pos, burst.repeatInterval))
                return false;
            if (!consume(source, pos, ','))
                return false;
            if (!parseFloatAt(source, pos, repeatCount))
                return false;
            if (!consume(source, pos, ']'))
                return false;
            burst.countMin    = static_cast<uint32_t>(std::max(0.0f, countMin));
            burst.countMax    = static_cast<uint32_t>(std::max(countMin, countMax));
            burst.repeatCount = static_cast<uint32_t>(std::max(0.0f, repeatCount));
            parsed.push_back(burst);

            skipWhitespace(source, pos);
            if (pos < source.size() && source[pos] == ',')
                ++pos;
        }

        return false;
    }

    void readEmitter(const std::string& source, ParticleEmitterComponent& emitter)
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

    void readMetadata(const std::string& source, ParticleEffectMetadata& metadata)
    {
        std::string metadataObject;
        if (!readObject(source, "metadata", metadataObject))
            return;

        readString(metadataObject, "name", metadata.name);
        readString(metadataObject, "description", metadata.description);
        readString(metadataObject, "author", metadata.author);
    }

    void readPreview(const std::string& source, ParticleEffectPreviewSettings& preview)
    {
        std::string previewObject;
        if (!readObject(source, "preview", previewObject))
            return;

        readVec4(previewObject, "backgroundColor", preview.backgroundColor);
        readVec3(previewObject, "cameraPosition", preview.cameraPosition);
        readVec3(previewObject, "cameraTarget", preview.cameraTarget);
        readFloat(previewObject, "orbitDistance", preview.orbitDistance);
    }

    bool readEffectEmitters(const std::string& source, ParticleEffectAsset& asset)
    {
        std::vector<std::string> emitterObjects;
        if (!readObjectArray(source, "emitters", emitterObjects))
            return false;

        std::vector<ParticleEffectEmitter> emitters;
        emitters.reserve(emitterObjects.size());
        for (size_t i = 0; i < emitterObjects.size(); ++i)
        {
            ParticleEffectEmitter emitter;
            emitter.stableId = defaultEmitterId(i);
            emitter.name     = defaultEmitterName(i);
            readString(emitterObjects[i], "id", emitter.stableId);
            readString(emitterObjects[i], "name", emitter.name);
            readEmitter(emitterObjects[i], emitter.descriptor);
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

        ParticleEffectAsset loaded;
        const bool          schemaRead = readUint(source, "schemaVersion", loaded.schemaVersion);
        readMetadata(source, loaded.metadata);
        readPreview(source, loaded.preview);
        if (!readEffectEmitters(source, loaded))
        {
            ParticleEffectEmitter emitter;
            emitter.stableId = "emitter";
            emitter.name     = defaultEffectNameFromPath(path);
            readEmitter(source, emitter.descriptor);
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
