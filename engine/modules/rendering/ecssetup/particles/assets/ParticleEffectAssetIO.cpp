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
            if (!std::isdigit(static_cast<unsigned char>(ch))
                && ch != '-'
                && ch != '+'
                && ch != '.'
                && ch != 'e'
                && ch != 'E')
            {
                break;
            }
            ++end;
        }

        if (end == pos)
            return false;

        value = std::stof(source.substr(pos, end - pos));
        pos = end;
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
        if (!consume(source, pos, '[')) return false;
        if (!parseFloatAt(source, pos, value.x)) return false;
        if (!consume(source, pos, ',')) return false;
        if (!parseFloatAt(source, pos, value.y)) return false;
        if (!consume(source, pos, ',')) return false;
        if (!parseFloatAt(source, pos, value.z)) return false;
        return consume(source, pos, ']');
    }

    bool parseVec4At(const std::string& source, size_t& pos, glm::vec4& value)
    {
        if (!consume(source, pos, '[')) return false;
        if (!parseFloatAt(source, pos, value.x)) return false;
        if (!consume(source, pos, ',')) return false;
        if (!parseFloatAt(source, pos, value.y)) return false;
        if (!consume(source, pos, ',')) return false;
        if (!parseFloatAt(source, pos, value.z)) return false;
        if (!consume(source, pos, ',')) return false;
        if (!parseFloatAt(source, pos, value.w)) return false;
        return consume(source, pos, ']');
    }

    size_t findValue(const std::string& source, const std::string& key)
    {
        const std::string quotedKey = "\"" + key + "\"";
        const size_t keyPos = source.find(quotedKey);
        if (keyPos == std::string::npos)
            return std::string::npos;

        const size_t colon = source.find(':', keyPos + quotedKey.size());
        if (colon == std::string::npos)
            return std::string::npos;

        return colon + 1;
    }

    bool readString(const std::string& source, const std::string& key, std::string& value)
    {
        size_t pos = findValue(source, key);
        return pos != std::string::npos && parseStringAt(source, pos, value);
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
        if (value == "box") return ParticleEmitterShape::Box;
        if (value == "disc") return ParticleEmitterShape::Disc;
        if (value == "cylinder") return ParticleEmitterShape::Cylinder;
        if (value == "ring") return ParticleEmitterShape::Ring;
        return ParticleEmitterShape::Sphere;
    }

    std::string shapeToString(ParticleEmitterShape shape)
    {
        switch (shape)
        {
            case ParticleEmitterShape::Sphere: return "sphere";
            case ParticleEmitterShape::Box: return "box";
            case ParticleEmitterShape::Disc: return "disc";
            case ParticleEmitterShape::Cylinder: return "cylinder";
            case ParticleEmitterShape::Ring: return "ring";
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

    bool readColorCurve(const std::string& source,
                        const std::string& key,
                        ParticleColorCurve& curve)
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

            if (!consume(source, pos, '[')) return false;
            ParticleColorKey item;
            if (!parseFloatAt(source, pos, item.t)) return false;
            if (!consume(source, pos, ',')) return false;
            if (!parseVec4At(source, pos, item.color)) return false;
            if (!consume(source, pos, ']')) return false;
            parsed.push_back(item);

            skipWhitespace(source, pos);
            if (pos < source.size() && source[pos] == ',')
                ++pos;
        }

        return false;
    }

    bool readFloatCurve(const std::string& source,
                        const std::string& key,
                        ParticleFloatCurve& curve)
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

            if (!consume(source, pos, '[')) return false;
            ParticleFloatKey item;
            if (!parseFloatAt(source, pos, item.t)) return false;
            if (!consume(source, pos, ',')) return false;
            if (!parseFloatAt(source, pos, item.value)) return false;
            if (!consume(source, pos, ']')) return false;
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

            if (!consume(source, pos, '[')) return false;
            ParticleBurst burst;
            float countMin = 0.0f;
            float countMax = 0.0f;
            float repeatCount = 0.0f;
            if (!parseFloatAt(source, pos, burst.time)) return false;
            if (!consume(source, pos, ',')) return false;
            if (!parseFloatAt(source, pos, countMin)) return false;
            if (!consume(source, pos, ',')) return false;
            if (!parseFloatAt(source, pos, countMax)) return false;
            if (!consume(source, pos, ',')) return false;
            if (!parseFloatAt(source, pos, burst.repeatInterval)) return false;
            if (!consume(source, pos, ',')) return false;
            if (!parseFloatAt(source, pos, repeatCount)) return false;
            if (!consume(source, pos, ']')) return false;
            burst.countMin = static_cast<uint32_t>(std::max(0.0f, countMin));
            burst.countMax = static_cast<uint32_t>(std::max(countMin, countMax));
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
        if (readString(source, "shape", text)) emitter.shape = shapeFromString(text);
        if (readString(source, "blend", text)) emitter.blend = blendFromString(text);
        readString(source, "texturePath", emitter.texturePath);
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
        readFloat(source, "softness", emitter.softness);
        readFloat(source, "hueVariation", emitter.hueVariation);
        readFloat(source, "valueVariation", emitter.valueVariation);
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

    void writeVec3(std::ostream& out, const glm::vec3& value)
    {
        out << '[' << value.x << ", " << value.y << ", " << value.z << ']';
    }

    void writeVec4(std::ostream& out, const glm::vec4& value)
    {
        out << '[' << value.x << ", " << value.y << ", " << value.z << ", " << value.w << ']';
    }
}

namespace gts::particles
{
    bool loadParticleEffect(const std::string& path, ParticleEmitterComponent& emitter)
    {
        std::ifstream file(path);
        if (!file)
            return false;

        std::stringstream buffer;
        buffer << file.rdbuf();

        ParticleEmitterComponent loaded = emitter;
        loaded.effectPath = path;
        readEmitter(buffer.str(), loaded);
        emitter = loaded;
        return true;
    }

    bool saveParticleEffect(const std::string& path, const ParticleEmitterComponent& emitter)
    {
        const std::filesystem::path parent = std::filesystem::path(path).parent_path();
        if (!parent.empty())
            std::filesystem::create_directories(parent);
        std::ofstream out(path);
        if (!out)
            return false;

        out << std::fixed << std::setprecision(4);
        out << "{\n";
        out << "  \"shape\": \"" << shapeToString(emitter.shape) << "\",\n";
        out << "  \"blend\": \"" << blendToString(emitter.blend) << "\",\n";
        out << "  \"texturePath\": \"" << emitter.texturePath << "\",\n";
        out << "  \"enabled\": " << (emitter.enabled ? "true" : "false") << ",\n";
        out << "  \"localSpace\": " << (emitter.localSpace ? "true" : "false") << ",\n";
        out << "  \"looping\": " << (emitter.looping ? "true" : "false") << ",\n";
        out << "  \"emissionRate\": " << emitter.emissionRate << ",\n";
        out << "  \"maxParticles\": " << emitter.maxParticles << ",\n";
        out << "  \"lifetimeMin\": " << emitter.lifetimeMin << ",\n";
        out << "  \"lifetimeMax\": " << emitter.lifetimeMax << ",\n";
        out << "  \"duration\": " << emitter.duration << ",\n";
        out << "  \"startDelay\": " << emitter.startDelay << ",\n";
        out << "  \"intensity\": " << emitter.intensity << ",\n";
        out << "  \"initialVelocity\": "; writeVec3(out, emitter.initialVelocity); out << ",\n";
        out << "  \"velocitySpread\": " << emitter.velocitySpread << ",\n";
        out << "  \"radialVelocityMin\": " << emitter.radialVelocityMin << ",\n";
        out << "  \"radialVelocityMax\": " << emitter.radialVelocityMax << ",\n";
        out << "  \"tangentVelocity\": " << emitter.tangentVelocity << ",\n";
        out << "  \"drag\": " << emitter.drag << ",\n";
        out << "  \"spinMin\": " << emitter.spinMin << ",\n";
        out << "  \"spinMax\": " << emitter.spinMax << ",\n";
        out << "  \"sizeRandomness\": " << emitter.sizeRandomness << ",\n";
        out << "  \"softness\": " << emitter.softness << ",\n";
        out << "  \"hueVariation\": " << emitter.hueVariation << ",\n";
        out << "  \"valueVariation\": " << emitter.valueVariation << ",\n";
        out << "  \"baseTint\": "; writeVec4(out, emitter.baseTint); out << ",\n";
        out << "  \"sphereRadius\": " << emitter.sphereRadius << ",\n";
        out << "  \"boxExtents\": "; writeVec3(out, emitter.boxExtents); out << ",\n";
        out << "  \"discRadius\": " << emitter.discRadius << ",\n";
        out << "  \"ringInnerRadius\": " << emitter.ringInnerRadius << ",\n";
        out << "  \"ringOuterRadius\": " << emitter.ringOuterRadius << ",\n";
        out << "  \"cylinderRadius\": " << emitter.cylinderRadius << ",\n";
        out << "  \"cylinderHeight\": " << emitter.cylinderHeight << ",\n";

        out << "  \"colorOverLifetime\": [";
        for (size_t i = 0; i < emitter.colorOverLifetime.size(); ++i)
        {
            if (i > 0) out << ", ";
            out << '[' << emitter.colorOverLifetime[i].t << ", ";
            writeVec4(out, emitter.colorOverLifetime[i].color);
            out << ']';
        }
        out << "],\n";

        out << "  \"alphaOverLifetime\": [";
        for (size_t i = 0; i < emitter.alphaOverLifetime.size(); ++i)
        {
            if (i > 0) out << ", ";
            out << '[' << emitter.alphaOverLifetime[i].t << ", " << emitter.alphaOverLifetime[i].value << ']';
        }
        out << "],\n";

        out << "  \"sizeOverLifetime\": [";
        for (size_t i = 0; i < emitter.sizeOverLifetime.size(); ++i)
        {
            if (i > 0) out << ", ";
            out << '[' << emitter.sizeOverLifetime[i].t << ", " << emitter.sizeOverLifetime[i].value << ']';
        }
        out << "],\n";

        out << "  \"bursts\": [";
        for (size_t i = 0; i < emitter.bursts.size(); ++i)
        {
            if (i > 0) out << ", ";
            const ParticleBurst& burst = emitter.bursts[i];
            out << '[' << burst.time << ", " << burst.countMin << ", " << burst.countMax
                << ", " << burst.repeatInterval << ", " << burst.repeatCount << ']';
        }
        out << "],\n";

        out << "  \"flipbookColumns\": " << emitter.flipbook.columns << ",\n";
        out << "  \"flipbookRows\": " << emitter.flipbook.rows << ",\n";
        out << "  \"flipbookFrameCount\": " << emitter.flipbook.frameCount << ",\n";
        out << "  \"flipbookFrameRate\": " << emitter.flipbook.frameRate << ",\n";
        out << "  \"flipbookLifetimeDriven\": " << (emitter.flipbook.lifetimeDriven ? "true" : "false") << ",\n";
        out << "  \"flipbookRandomStart\": " << (emitter.flipbook.randomStart ? "true" : "false") << ",\n";
        out << "  \"forceAcceleration\": "; writeVec3(out, emitter.forces.acceleration); out << ",\n";
        out << "  \"forceWind\": "; writeVec3(out, emitter.forces.wind); out << ",\n";
        out << "  \"forceVortex\": " << emitter.forces.vortex << ",\n";
        out << "  \"forceRadial\": " << emitter.forces.radial << ",\n";
        out << "  \"forceNoiseStrength\": " << emitter.forces.noiseStrength << ",\n";
        out << "  \"forceNoiseScale\": " << emitter.forces.noiseScale << "\n";
        out << "}\n";
        return true;
    }
}
