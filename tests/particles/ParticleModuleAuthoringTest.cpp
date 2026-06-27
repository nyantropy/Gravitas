#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "ParticleEffectAsset.h"
#include "ParticleEffectAssetIO.h"
#include "ParticleModuleAuthoring.h"

namespace
{
    void require(bool condition, const std::string& message)
    {
        if (condition)
            return;

        std::cerr << "Particle module authoring test failed: " << message << std::endl;
        std::exit(EXIT_FAILURE);
    }

    bool near(float lhs, float rhs)
    {
        return std::abs(lhs - rhs) <= 0.0001f;
    }
} // namespace

int main()
{
    using namespace gts::particles;

    require(particleModuleDefinitions().size() == 10u, "expected initial module category set");

    ParticleEmitterComponent descriptor;
    descriptor.emissionRate = 77.0f;
    descriptor.maxParticles = 333u;
    descriptor.shape = ParticleEmitterShape::Ring;
    descriptor.ringOuterRadius = 1.7f;
    descriptor.baseTint = {0.2f, 0.4f, 0.6f, 0.8f};
    descriptor.bursts = {{0.25f, 4u, 8u, 0.5f, 2u}};

    std::vector<ParticleModuleInstance> modules = buildModulesFromEmitterDescriptor(descriptor);
    require(modules.size() == particleModuleDefinitions().size(), "descriptor should generate every module");

    ParticleModuleInstance* spawn = nullptr;
    ParticleModuleInstance* shape = nullptr;
    ParticleModuleInstance* renderer = nullptr;
    ParticleModuleInstance* bursts = nullptr;
    for (ParticleModuleInstance& module : modules)
    {
        if (module.typeId == "spawn.basic")
            spawn = &module;
        else if (module.typeId == "shape.basic")
            shape = &module;
        else if (module.typeId == "renderer.basic")
            renderer = &module;
        else if (module.typeId == "bursts.basic")
            bursts = &module;
    }

    require(spawn != nullptr, "spawn module missing");
    require(shape != nullptr, "shape module missing");
    require(renderer != nullptr, "renderer module missing");
    require(bursts != nullptr, "bursts module missing");

    setFloatParameter(*spawn, "emissionRate", 12.5f);
    setUIntParameter(*spawn, "maxParticles", 64u);
    setUIntParameter(*shape, "shape", static_cast<uint32_t>(ParticleEmitterShape::Box));
    setFloatParameter(*shape, "boxX", 1.25f);
    setUIntParameter(*renderer, "blend", static_cast<uint32_t>(ParticleBlendMode::Additive));
    setBoolParameter(*bursts, "burstEnabled", true);
    setUIntParameter(*bursts, "burstCountMax", 99u);

    applyParticleModulesToEmitterDescriptor(modules, descriptor);
    require(near(descriptor.emissionRate, 12.5f), "spawn emission rate did not apply");
    require(descriptor.maxParticles == 64u, "spawn max particles did not apply");
    require(descriptor.shape == ParticleEmitterShape::Box, "shape enum did not apply");
    require(near(descriptor.boxExtents.x, 1.25f), "shape scalar did not apply");
    require(descriptor.blend == ParticleBlendMode::Additive, "renderer enum did not apply");
    require(!descriptor.bursts.empty() && descriptor.bursts.front().countMax == 99u, "burst module did not apply");

    ParticleEffectAsset asset;
    asset.metadata.name = "Module Test";
    ParticleEffectEmitter emitter;
    emitter.stableId = "emitter";
    emitter.name = "Emitter";
    emitter.descriptor = descriptor;
    emitter.modules = modules;
    asset.emitters.push_back(emitter);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "gravitas_particle_module_authoring_test.json";
    require(saveParticleEffectAsset(path.string(), asset), "failed to save module asset");

    ParticleEffectAsset loaded;
    require(loadParticleEffectAsset(path.string(), loaded), "failed to load module asset");
    require(loaded.schemaVersion == CurrentParticleEffectSchemaVersion, "schema version was not migrated");
    require(loaded.emitters.size() == 1u, "loaded emitter count mismatch");
    require(loaded.emitters.front().modules.size() == particleModuleDefinitions().size(),
            "loaded module count mismatch");
    require(near(loaded.emitters.front().descriptor.emissionRate, 12.5f),
            "loaded descriptor did not compile module emission rate");
    require(loaded.emitters.front().descriptor.blend == ParticleBlendMode::Additive,
            "loaded descriptor did not compile renderer blend");

    std::error_code ec;
    std::filesystem::remove(path, ec);
    return EXIT_SUCCESS;
}
