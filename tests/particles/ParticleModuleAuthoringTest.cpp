#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
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

    size_t diagnosticCount(const std::vector<gts::particles::ParticleModuleGraphDiagnostic>& diagnostics,
                           gts::particles::ParticleModuleGraphDiagnosticSeverity             severity)
    {
        size_t count = 0;
        for (const gts::particles::ParticleModuleGraphDiagnostic& diagnostic : diagnostics)
        {
            if (diagnostic.severity == severity)
                count += 1;
        }
        return count;
    }

    void removeParameter(gts::particles::ParticleModuleInstance& module, const std::string& parameterId)
    {
        module.parameters.erase(std::remove_if(module.parameters.begin(),
                                               module.parameters.end(),
                                               [&](const gts::particles::ParticleModuleParameter& parameter)
                                               {
                                                   return parameter.id == parameterId;
                                               }),
                                module.parameters.end());
    }

    void addFloatParameter(gts::particles::ParticleModuleInstance& module,
                           const std::string&                      parameterId,
                           float                                   value)
    {
        gts::particles::ParticleModuleParameter parameter;
        parameter.id         = parameterId;
        parameter.type       = gts::particles::ParticleModuleParameterType::Float;
        parameter.floatValue = value;
        module.parameters.push_back(parameter);
    }

    void addBoolParameter(gts::particles::ParticleModuleInstance& module,
                          const std::string&                      parameterId,
                          bool                                    value)
    {
        gts::particles::ParticleModuleParameter parameter;
        parameter.id        = parameterId;
        parameter.type      = gts::particles::ParticleModuleParameterType::Bool;
        parameter.boolValue = value;
        module.parameters.push_back(parameter);
    }
} // namespace

int main()
{
    using namespace gts::particles;

    require(CurrentParticleEffectSchemaVersion == 5u, "effect schema should identify graph foundation assets");
    require(CurrentParticleModuleSchemaVersion == 2u, "module schema should identify rich graph-compatible modules");
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

    std::vector<ParticleModuleInstance> duplicateTypeModules = modules;
    ParticleModuleInstance              duplicateSpawn      = duplicateTypeModules.front();
    duplicateSpawn.stableId                                = "extra_spawn";
    duplicateTypeModules.push_back(duplicateSpawn);
    std::vector<ParticleModuleGraphDiagnostic> duplicateTypeDiagnostics;
    require(!validateParticleModuleStack(duplicateTypeModules, &duplicateTypeDiagnostics),
            "duplicate singleton module types should fail validation");

    std::vector<ParticleModuleInstance> futureVersionModules = modules;
    futureVersionModules.front().version                     = CurrentParticleModuleSchemaVersion + 1u;
    std::vector<ParticleModuleGraphDiagnostic> futureVersionDiagnostics;
    require(!validateParticleModuleStack(futureVersionModules, &futureVersionDiagnostics),
            "future module versions should fail validation");
    ParticleEffectAsset futureVersionAsset;
    ParticleEffectEmitter futureVersionEmitter;
    futureVersionEmitter.stableId   = "future";
    futureVersionEmitter.name       = "Future";
    futureVersionEmitter.descriptor = descriptor;
    futureVersionEmitter.modules    = futureVersionModules;
    futureVersionAsset.emitters.push_back(futureVersionEmitter);
    std::string futureVersionError;
    require(!migrateParticleEffectAsset(futureVersionAsset, &futureVersionError),
            "future module versions should fail asset migration");
    require(!futureVersionError.empty(), "future module version rejection should report an error");

    std::vector<ParticleModuleInstance> disabledDependencyModules = modules;
    disabledDependencyModules.front().enabled                     = false;
    std::vector<ParticleModuleGraphDiagnostic> disabledDependencyDiagnostics;
    require(validateParticleModuleStack(disabledDependencyModules, &disabledDependencyDiagnostics),
            "disabled dependencies should warn but remain graph-compatible through default outputs");
    require(diagnosticCount(disabledDependencyDiagnostics, ParticleModuleGraphDiagnosticSeverity::Warning) > 0u,
            "disabled dependencies should surface graph warnings");
    require(diagnosticCount(disabledDependencyDiagnostics, ParticleModuleGraphDiagnosticSeverity::Error) == 0u,
            "disabled dependency warnings should not be errors");

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

    ParticleModuleInstance legacyColor = *color;
    legacyColor.version                = LegacyParticleModuleSchemaVersion;
    removeParameter(legacyColor, "colorOverLifetime");
    removeParameter(legacyColor, "alphaOverLifetime");
    setFloatParameter(legacyColor, "baseTintR", 0.25f);
    setFloatParameter(legacyColor, "baseTintG", 0.50f);
    setFloatParameter(legacyColor, "baseTintB", 0.75f);
    setFloatParameter(legacyColor, "baseTintA", 0.90f);
    addFloatParameter(legacyColor, "alphaPeak", 0.42f);
    completeModuleParameters(legacyColor);
    require(legacyColor.version == CurrentParticleModuleSchemaVersion, "legacy color module version was not upgraded");
    require(findParameter(legacyColor, "alphaPeak") == nullptr, "legacy color parameter was not pruned");
    const ParticleModuleParameter* migratedGradient = findParameter(legacyColor, "colorOverLifetime");
    const ParticleModuleParameter* migratedAlpha    = findParameter(legacyColor, "alphaOverLifetime");
    require(migratedGradient != nullptr && migratedGradient->colorGradientValue.size() == 2u,
            "legacy color gradient was not synthesized");
    require(near(migratedGradient->colorGradientValue.front().color.b, 0.75f),
            "legacy color gradient did not preserve base tint");
    require(migratedAlpha != nullptr && migratedAlpha->floatCurveValue.size() == 4u,
            "legacy alpha curve was not synthesized");
    require(near(migratedAlpha->floatCurveValue[1].value, 0.42f), "legacy alpha peak was not preserved");

    ParticleModuleInstance legacySize = *size;
    legacySize.version                = LegacyParticleModuleSchemaVersion;
    removeParameter(legacySize, "sizeOverLifetime");
    addFloatParameter(legacySize, "sizeStart", 0.30f);
    addFloatParameter(legacySize, "sizeEnd", 1.10f);
    completeModuleParameters(legacySize);
    require(legacySize.version == CurrentParticleModuleSchemaVersion, "legacy size module version was not upgraded");
    require(findParameter(legacySize, "sizeStart") == nullptr && findParameter(legacySize, "sizeEnd") == nullptr,
            "legacy size parameters were not pruned");
    const ParticleModuleParameter* migratedSize = findParameter(legacySize, "sizeOverLifetime");
    require(migratedSize != nullptr && migratedSize->floatCurveValue.size() == 2u,
            "legacy size curve was not synthesized");
    require(near(migratedSize->floatCurveValue.back().value, 1.10f), "legacy size end was not preserved");

    ParticleModuleInstance legacyBursts = *bursts;
    legacyBursts.version                = LegacyParticleModuleSchemaVersion;
    removeParameter(legacyBursts, "bursts");
    addBoolParameter(legacyBursts, "burstEnabled", true);
    addFloatParameter(legacyBursts, "burstTime", 0.35f);
    gts::particles::ParticleModuleParameter burstMin;
    burstMin.id        = "burstCountMin";
    burstMin.type      = ParticleModuleParameterType::UInt;
    burstMin.uintValue = 3u;
    legacyBursts.parameters.push_back(burstMin);
    gts::particles::ParticleModuleParameter burstMax;
    burstMax.id        = "burstCountMax";
    burstMax.type      = ParticleModuleParameterType::UInt;
    burstMax.uintValue = 7u;
    legacyBursts.parameters.push_back(burstMax);
    addFloatParameter(legacyBursts, "repeatInterval", 0.25f);
    gts::particles::ParticleModuleParameter repeatCount;
    repeatCount.id        = "repeatCount";
    repeatCount.type      = ParticleModuleParameterType::UInt;
    repeatCount.uintValue = 2u;
    legacyBursts.parameters.push_back(repeatCount);
    completeModuleParameters(legacyBursts);
    require(legacyBursts.version == CurrentParticleModuleSchemaVersion, "legacy burst module version was not upgraded");
    require(findParameter(legacyBursts, "burstEnabled") == nullptr, "legacy burst parameters were not pruned");
    const ParticleModuleParameter* migratedBursts = findParameter(legacyBursts, "bursts");
    require(migratedBursts != nullptr && migratedBursts->burstTimelineValue.size() == 1u,
            "legacy burst timeline was not synthesized");
    require(migratedBursts->burstTimelineValue.front().countMax == 7u, "legacy burst count was not preserved");

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

    ParticleEmitterComponent reversedDescriptor;
    std::vector<ParticleModuleInstance> reversedModules = modules;
    std::reverse(reversedModules.begin(), reversedModules.end());
    const std::vector<const ParticleModuleInstance*> reversedPlan = buildParticleModuleExecutionPlan(reversedModules);
    require(moduleExecutionStage(reversedPlan.front()->typeId) == ParticleModuleExecutionStage::Spawn,
            "reordered execution plan should still begin with spawn");
    require(moduleExecutionStage(reversedPlan.back()->typeId) == ParticleModuleExecutionStage::Render,
            "reordered execution plan should still end with render");
    applyParticleModulesToEmitterDescriptor(reversedModules, reversedDescriptor);
    require(near(reversedDescriptor.emissionRate, 12.5f), "execution-plan descriptor application lost spawn data");
    require(reversedDescriptor.blend == ParticleBlendMode::Additive,
            "execution-plan descriptor application lost renderer data");

    ParticleEffectAsset asset;
    asset.metadata.name = "Module Test";
    ParticleEffectEmitter emitter;
    emitter.stableId   = "emitter";
    emitter.name       = "Emitter";
    emitter.descriptor = descriptor;
    emitter.modules    = modules;
    asset.emitters.push_back(emitter);

    ParticleEffectEmitter secondEmitter;
    secondEmitter.stableId               = "second";
    secondEmitter.name                   = "Second";
    secondEmitter.descriptor.emissionRate = 99.0f;
    secondEmitter.descriptor.maxParticles = 24u;
    secondEmitter.modules                = buildModulesFromEmitterDescriptor(secondEmitter.descriptor);
    asset.emitters.push_back(secondEmitter);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "gravitas_particle_module_authoring_test.json";
    require(saveParticleEffectAsset(path.string(), asset), "failed to save module asset");

    ParticleEffectAsset loaded;
    require(loadParticleEffectAsset(path.string(), loaded), "failed to load module asset");
    require(loaded.schemaVersion == CurrentParticleEffectSchemaVersion, "schema version was not migrated");
    require(loaded.emitters.size() == 2u, "loaded emitter count mismatch");
    require(loaded.emitters.front().modules.size() == particleModuleDefinitions().size(),
            "loaded module count mismatch");
    require(loaded.emitters.front().modules.front().version == CurrentParticleModuleSchemaVersion,
            "loaded module schema was not migrated");
    require(near(loaded.emitters.front().descriptor.emissionRate, 12.5f),
            "loaded descriptor did not compile module emission rate");
    require(loaded.emitters.front().descriptor.blend == ParticleBlendMode::Additive,
            "loaded descriptor did not compile renderer blend");
    require(loaded.emitters.front().descriptor.colorOverLifetime.size() == 2u,
            "loaded descriptor did not compile color gradient");
    require(loaded.emitters.front().descriptor.sizeOverLifetime.size() == 3u,
            "loaded descriptor did not compile size curve");
    require(loaded.emitters.front().descriptor.bursts.size() == 2u, "loaded descriptor did not compile burst timeline");
    require(selectParticleEffectEmitter(loaded, "second") != nullptr, "loaded multi-emitter asset cannot select second");
    require(near(selectParticleEffectEmitter(loaded, "second")->descriptor.emissionRate, 99.0f),
            "loaded second emitter descriptor was not preserved");

    const std::filesystem::path flatPath =
        std::filesystem::temp_directory_path() / "gravitas_particle_flat_migration_test.json";
    {
        std::ofstream flat(flatPath);
        flat << "{\n"
             << "  \"schemaVersion\": 2,\n"
             << "  \"emissionRate\": 44,\n"
             << "  \"maxParticles\": 22,\n"
             << "  \"shape\": \"box\",\n"
             << "  \"boxExtents\": [1, 2, 3],\n"
             << "  \"texturePath\": \"resources/textures/flat.png\",\n"
             << "  \"colorOverLifetime\": [[0, [1, 0, 0, 1]], [1, [0, 0, 1, 0.5]]],\n"
             << "  \"bursts\": [[0.2, 1, 4, 0, 0]]\n"
             << "}\n";
    }
    ParticleEffectAsset flatLoaded;
    require(loadParticleEffectAsset(flatPath.string(), flatLoaded), "failed to load old flat particle preset");
    require(flatLoaded.schemaVersion == CurrentParticleEffectSchemaVersion, "flat preset schema was not migrated");
    require(flatLoaded.emitters.size() == 1u, "flat preset did not synthesize one emitter");
    require(flatLoaded.emitters.front().modules.size() == particleModuleDefinitions().size(),
            "flat preset did not synthesize module stack");
    require(near(flatLoaded.emitters.front().descriptor.emissionRate, 44.0f),
            "flat preset emission rate was not preserved");
    require(flatLoaded.emitters.front().descriptor.texturePath == "resources/textures/flat.png",
            "flat preset texture path was not preserved");

    const std::filesystem::path collisionPath =
        std::filesystem::temp_directory_path() / "gravitas_particle_parser_collision_test.json";
    {
        std::ofstream collision(collisionPath);
        collision << "{\n"
                  << "  \"schemaVersion\": 4,\n"
                  << "  \"metadata\": {\"name\": \"Parser Collision\", \"description\": \"\", \"author\": \"\"},\n"
                  << "  \"emitters\": [\n"
                  << "    {\n"
                  << "      \"id\": \"emitter\",\n"
                  << "      \"name\": \"Emitter\",\n"
                  << "      \"modules\": [\n"
                  << "        {\"id\": \"Spawn\", \"type\": \"spawn.basic\", \"displayName\": \"Spawn\","
                  << " \"version\": 1, \"enabled\": true, \"parameters\": [\n"
                  << "          {\"id\": \"texturePath\", \"type\": \"string\", \"value\": \"wrong/module.png\"}\n"
                  << "        ]}\n"
                  << "      ],\n"
                  << "      \"simulation\": {\"schemaVersion\": 2, \"enabled\": true},\n"
                  << "      \"renderer\": {\"primitive\": \"billboard\", \"blend\": \"alpha\","
                  << " \"spriteShape\": \"softCircle\", \"texturePath\": \"descriptor_texture.png\","
                  << " \"meshPath\": \"descriptor_mesh.obj\"},\n"
                  << "      \"spawn\": {\"emissionRate\": 5, \"maxParticles\": 10, \"lifetimeMin\": 1,"
                  << " \"lifetimeMax\": 2}\n"
                  << "    }\n"
                  << "  ]\n"
                  << "}\n";
    }
    ParticleEffectAsset collisionLoaded;
    require(loadParticleEffectAsset(collisionPath.string(), collisionLoaded),
            "failed to load parser collision particle asset");
    require(collisionLoaded.emitters.front().descriptor.texturePath == "descriptor_texture.png",
            "structured parser confused module parameter id with renderer texture path");

    std::error_code ec;
    std::filesystem::remove(path, ec);
    std::filesystem::remove(flatPath, ec);
    std::filesystem::remove(collisionPath, ec);
    return EXIT_SUCCESS;
}
