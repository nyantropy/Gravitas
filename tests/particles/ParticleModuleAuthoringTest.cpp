#include <algorithm>
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
    std::vector<ParticleModuleGraphDiagnostic> definitionDiagnostics;
    require(validateParticleModuleDefinitions(&definitionDiagnostics),
            "default module definitions should be graph compatible");
    for (const ParticleModuleDefinition& definition : particleModuleDefinitions())
    {
        require(!definition.parameters.empty(), "module definition should expose parameters");
        require(!definition.outputs.empty(), "module definition should expose graph outputs");
    }

    ParticleEmitterComponent descriptor;
    descriptor.emissionRate    = 77.0f;
    descriptor.maxParticles    = 333u;
    descriptor.shape           = ParticleEmitterShape::Ring;
    descriptor.ringOuterRadius = 1.7f;
    descriptor.baseTint        = {0.2f, 0.4f, 0.6f, 0.8f};
    descriptor.bursts          = {{0.25f, 4u, 8u, 0.5f, 2u}};

    std::vector<ParticleModuleInstance> modules = buildModulesFromEmitterDescriptor(descriptor);
    require(modules.size() == particleModuleDefinitions().size(), "descriptor should generate every module");
    std::vector<ParticleModuleGraphDiagnostic> stackDiagnostics;
    require(validateParticleModuleStack(modules, &stackDiagnostics), "default module stack should be graph compatible");
    const std::vector<const ParticleModuleInstance*> plan = buildParticleModuleExecutionPlan(modules);
    require(plan.size() == modules.size(), "execution plan should include every module");
    require(moduleExecutionStage(plan.front()->typeId) == ParticleModuleExecutionStage::Spawn,
            "execution plan should begin with spawn stage");
    require(moduleExecutionStage(plan.back()->typeId) == ParticleModuleExecutionStage::Render,
            "execution plan should end with render stage");

    std::vector<ParticleModuleInstance> missingDependencyModules = modules;
    missingDependencyModules.erase(std::remove_if(missingDependencyModules.begin(),
                                                  missingDependencyModules.end(),
                                                  [](const ParticleModuleInstance& module)
                                                  {
                                                      return module.typeId == "spawn.basic";
                                                  }),
                                   missingDependencyModules.end());
    std::vector<ParticleModuleGraphDiagnostic> missingDependencyDiagnostics;
    require(!validateParticleModuleStack(missingDependencyModules, &missingDependencyDiagnostics),
            "missing required graph dependency should fail validation");

    std::vector<ParticleModuleInstance> duplicateStableIdModules = modules;
    duplicateStableIdModules.back().stableId                     = duplicateStableIdModules.front().stableId;
    std::vector<ParticleModuleGraphDiagnostic> duplicateStableIdDiagnostics;
    require(!validateParticleModuleStack(duplicateStableIdModules, &duplicateStableIdDiagnostics),
            "duplicate stable graph node ids should fail validation");

    ParticleModuleInstance* spawn    = nullptr;
    ParticleModuleInstance* shape    = nullptr;
    ParticleModuleInstance* color    = nullptr;
    ParticleModuleInstance* size     = nullptr;
    ParticleModuleInstance* renderer = nullptr;
    ParticleModuleInstance* bursts   = nullptr;
    for (ParticleModuleInstance& module : modules)
    {
        if (module.typeId == "spawn.basic")
            spawn = &module;
        else if (module.typeId == "shape.basic")
            shape = &module;
        else if (module.typeId == "color.basic")
            color = &module;
        else if (module.typeId == "size.basic")
            size = &module;
        else if (module.typeId == "renderer.basic")
            renderer = &module;
        else if (module.typeId == "bursts.basic")
            bursts = &module;
    }

    require(spawn != nullptr, "spawn module missing");
    require(shape != nullptr, "shape module missing");
    require(color != nullptr, "color module missing");
    require(size != nullptr, "size module missing");
    require(renderer != nullptr, "renderer module missing");
    require(bursts != nullptr, "bursts module missing");

    setFloatParameter(*spawn, "emissionRate", 12.5f);
    setUIntParameter(*spawn, "maxParticles", 64u);
    setUIntParameter(*shape, "shape", static_cast<uint32_t>(ParticleEmitterShape::Box));
    setFloatParameter(*shape, "boxX", 1.25f);
    setUIntParameter(*renderer, "blend", static_cast<uint32_t>(ParticleBlendMode::Additive));
    setStringParameter(*renderer, "texturePath", "resources/textures/engine_particle_fallback.png");
    setStringParameter(*renderer, "meshPath", "resources/models/cube.obj");
    setColorGradientParameter(
        *color, "colorOverLifetime", {{0.0f, {1.0f, 0.2f, 0.1f, 1.0f}}, {1.0f, {0.1f, 0.3f, 1.0f, 0.5f}}});
    setFloatCurveParameter(*color, "alphaOverLifetime", {{0.0f, 0.0f}, {0.5f, 0.65f}, {1.0f, 0.0f}});
    setFloatCurveParameter(*size, "sizeOverLifetime", {{0.0f, 0.2f}, {0.4f, 1.2f}, {1.0f, 0.45f}});
    setBurstTimelineParameter(*bursts, "bursts", {{0.10f, 2u, 4u, 0.0f, 0u}, {0.75f, 6u, 9u, 0.25f, 2u}});

    applyParticleModulesToEmitterDescriptor(modules, descriptor);
    require(near(descriptor.emissionRate, 12.5f), "spawn emission rate did not apply");
    require(descriptor.maxParticles == 64u, "spawn max particles did not apply");
    require(descriptor.shape == ParticleEmitterShape::Box, "shape enum did not apply");
    require(near(descriptor.boxExtents.x, 1.25f), "shape scalar did not apply");
    require(descriptor.blend == ParticleBlendMode::Additive, "renderer enum did not apply");
    require(descriptor.texturePath == "resources/textures/engine_particle_fallback.png",
            "texture picker path did not apply");
    require(descriptor.meshPath == "resources/models/cube.obj", "mesh picker path did not apply");
    require(descriptor.colorOverLifetime.size() == 2u && near(descriptor.colorOverLifetime.back().color.b, 1.0f),
            "color gradient did not apply");
    require(descriptor.alphaOverLifetime.size() == 3u && near(descriptor.alphaOverLifetime[1].value, 0.65f),
            "alpha curve did not apply");
    require(descriptor.sizeOverLifetime.size() == 3u && near(descriptor.sizeOverLifetime[1].value, 1.2f),
            "size curve did not apply");
    require(descriptor.bursts.size() == 2u && descriptor.bursts.back().countMax == 9u, "burst timeline did not apply");

    ParticleEffectAsset asset;
    asset.metadata.name = "Module Test";
    ParticleEffectEmitter emitter;
    emitter.stableId   = "emitter";
    emitter.name       = "Emitter";
    emitter.descriptor = descriptor;
    emitter.modules    = modules;
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
    require(loaded.emitters.front().descriptor.colorOverLifetime.size() == 2u,
            "loaded descriptor did not compile color gradient");
    require(loaded.emitters.front().descriptor.sizeOverLifetime.size() == 3u,
            "loaded descriptor did not compile size curve");
    require(loaded.emitters.front().descriptor.bursts.size() == 2u, "loaded descriptor did not compile burst timeline");

    std::error_code ec;
    std::filesystem::remove(path, ec);
    return EXIT_SUCCESS;
}
