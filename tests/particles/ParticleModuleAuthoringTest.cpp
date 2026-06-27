#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "ParticleEffectAsset.h"
#include "ParticleEffectAssetIO.h"
#include "ParticleGraphAuthoring.h"
#include "ParticleModuleAuthoring.h"
#include "ParticleProgramCompiler.h"

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

    require(CurrentParticleEffectSchemaVersion == 7u, "effect schema should identify runtime-improved assets");
    require(CurrentParticleEmitterSchemaVersion == 3u, "emitter schema should identify runtime policy fields");
    require(CurrentParticleModuleSchemaVersion == 3u, "module schema should identify runtime policy modules");
    require(CurrentParticleProgramSchemaVersion == 1u, "compiled program schema should identify runtime output");
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
    descriptor.materialPath    = "resources/materials/particle_debug.material";
    descriptor.runtime.effectScale = 1.25f;
    descriptor.runtime.importance = 2.5f;
    descriptor.runtime.budgetWeight = 3u;
    descriptor.runtime.maxSpawnPerFrame = 48u;
    descriptor.runtime.distanceCulling = true;
    descriptor.runtime.maxDrawDistance = 85.0f;
    descriptor.runtime.lodNearDistance = 6.0f;
    descriptor.runtime.lodFarDistance = 28.0f;
    descriptor.runtime.lodMinSpawnScale = 0.25f;
    descriptor.runtime.lodMinRenderScale = 0.40f;
    descriptor.runtime.velocityStretch = 0.04f;
    descriptor.runtime.velocityStretchMax = 2.5f;
    descriptor.runtime.meshSoftness = 12.0f;
    descriptor.runtime.lightingInfluence = 0.35f;
    descriptor.collision.mode = ParticleCollisionMode::GroundPlane;
    descriptor.collision.groundY = -0.2f;
    descriptor.collision.bounce = 0.55f;
    descriptor.collision.damping = 0.65f;
    descriptor.collision.spawnOnCollisionCount = 2u;
    descriptor.collision.spawnOnDeathCount = 1u;
    descriptor.collision.maxEventSpawnsPerFrame = 16u;

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
    ParticleEffectEmitter disabledEmitter;
    disabledEmitter.stableId   = "disabled_graph_emitter";
    disabledEmitter.name       = "Disabled Graph Emitter";
    disabledEmitter.descriptor = descriptor;
    disabledEmitter.modules    = disabledDependencyModules;
    syncParticleGraphWithModules(disabledEmitter);
    const ParticleCompiledParticleProgram disabledProgram = compileParticleEffectEmitter(disabledEmitter);
    require(disabledProgram.valid, "disabled graph modules should still compile through default outputs");
    require(disabledProgram.deadNodesEliminated > 0u, "disabled graph nodes should be optimized away");
    require(diagnosticCount(disabledProgram.diagnostics, ParticleModuleGraphDiagnosticSeverity::Warning) > 0u,
            "disabled graph module compilation should retain warnings");

    ParticleEffectEmitter graphEmitter;
    graphEmitter.stableId   = "graph_emitter";
    graphEmitter.name       = "Graph Emitter";
    graphEmitter.descriptor = descriptor;
    graphEmitter.modules    = modules;
    syncParticleGraphWithModules(graphEmitter);
    require(graphEmitter.graph.schemaVersion == CurrentParticleGraphSchemaVersion,
            "graph schema should be current after sync");
    require(graphEmitter.graph.nodes.size() == graphEmitter.modules.size(), "graph should have one node per module");
    require(!graphEmitter.graph.links.empty(), "graph should synthesize dependency links");
    std::vector<ParticleModuleGraphDiagnostic> graphDiagnostics;
    require(validateParticleEffectGraph(graphEmitter, &graphDiagnostics), "default graph should validate");
    const ParticleCompiledParticleProgram graphProgram = compileParticleEffectEmitter(graphEmitter);
    require(graphProgram.valid, "valid graph should compile");
    require(graphProgram.backend == ParticleProgramBackend::CpuDescriptor,
            "initial compiled backend should target CPU descriptors");
    require(graphProgram.modules.size() == graphEmitter.modules.size(), "compiled program should include every module");
    require(graphProgram.modules.front().executionStage == ParticleModuleExecutionStage::Spawn,
            "compiled program should begin with spawn stage");
    require(graphProgram.modules.back().executionStage == ParticleModuleExecutionStage::Render,
            "compiled program should end with render stage");
    require(graphProgram.staticParametersEvaluated > 0u, "compiled program should evaluate static parameters");
    require(graphProgram.curvesBaked > 0u, "compiled program should bake curves");
    require(graphProgram.modulesFused > 0u, "compiled program should fuse module work into runtime output");
    ParticleEmitterComponent graphRuntimeDescriptor = graphEmitter.descriptor;
    applyParticleModulesToEmitterDescriptor(graphEmitter.modules, graphRuntimeDescriptor);
    require(near(graphProgram.runtimeDescriptor.emissionRate, graphRuntimeDescriptor.emissionRate),
            "compiled program did not preserve descriptor emission rate");
    ParticleEffectEmitter duplicateBackingNodeEmitter = graphEmitter;
    duplicateBackingNodeEmitter.graph.nodes.push_back(duplicateBackingNodeEmitter.graph.nodes.front());
    duplicateBackingNodeEmitter.graph.nodes.back().id = "duplicate_backing_node";
    std::vector<ParticleModuleGraphDiagnostic> duplicateBackingNodeDiagnostics;
    require(!validateParticleEffectGraph(duplicateBackingNodeEmitter, &duplicateBackingNodeDiagnostics),
            "duplicated backing graph module should fail validation");
    require(!compileParticleEffectEmitter(duplicateBackingNodeEmitter).valid,
            "duplicated backing graph module should fail compilation");

    const ParticleGraphNode* colorGraphNode = findParticleGraphNodeForType(graphEmitter.graph, "color.basic");
    require(colorGraphNode != nullptr, "color graph node missing");
    const ParticleGraphLink removedLink = graphEmitter.graph.links.front();
    graphEmitter.graph.links.erase(graphEmitter.graph.links.begin());
    std::vector<ParticleModuleGraphDiagnostic> brokenGraphDiagnostics;
    require(!validateParticleEffectGraph(graphEmitter, &brokenGraphDiagnostics),
            "missing required graph link should fail validation");
    const ParticleCompiledParticleProgram brokenGraphProgram = compileParticleEffectEmitter(graphEmitter);
    require(!brokenGraphProgram.valid, "broken graph should not produce a valid compiled program");
    require(diagnosticCount(brokenGraphProgram.diagnostics, ParticleModuleGraphDiagnosticSeverity::Error) > 0u,
            "broken graph compilation should report errors");
    graphEmitter.graph.links.push_back(removedLink);
    require(addParticleGraphFrameForNode(graphEmitter, colorGraphNode->id), "failed to add graph frame");
    require(addParticleGraphCommentForNode(graphEmitter, colorGraphNode->id), "failed to add graph comment");
    require(graphEmitter.graph.frames.size() == 1u, "graph frame was not stored");
    require(graphEmitter.graph.comments.size() == 1u, "graph comment was not stored");

    const ParticleGraphNode* forcesGraphNode = findParticleGraphNodeForType(graphEmitter.graph, "forces.basic");
    require(forcesGraphNode != nullptr, "forces graph node missing");
    const std::string removedForcesNodeId = forcesGraphNode->id;
    require(removeParticleGraphNode(graphEmitter, removedForcesNodeId), "failed to remove graph node");
    require(findParticleGraphNodeForType(graphEmitter.graph, "forces.basic") == nullptr,
            "graph node removal did not remove forces");
    const ParticleModuleDefinition* forcesDefinition = findParticleModuleDefinition("forces.basic");
    require(forcesDefinition != nullptr, "forces definition missing");
    require(addParticleGraphNode(graphEmitter, *forcesDefinition), "failed to add graph node from registry");
    syncParticleGraphWithModules(graphEmitter);
    require(findParticleGraphNodeForType(graphEmitter.graph, "forces.basic") != nullptr,
            "graph node creation did not restore forces");
    require(validateParticleEffectGraph(graphEmitter, &graphDiagnostics), "restored graph should validate");
    require(compileParticleEffectEmitter(graphEmitter).valid, "restored graph should compile");

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
    setFloatParameter(*spawn, "effectScale", 1.75f);
    setFloatParameter(*spawn, "importance", 4.0f);
    setUIntParameter(*spawn, "budgetWeight", 5u);
    setUIntParameter(*spawn, "maxSpawnPerFrame", 12u);
    setUIntParameter(*shape, "shape", static_cast<uint32_t>(ParticleEmitterShape::Box));
    setFloatParameter(*shape, "boxX", 1.25f);
    ParticleModuleInstance* forces = nullptr;
    for (ParticleModuleInstance& module : modules)
    {
        if (module.typeId == "forces.basic")
        {
            forces = &module;
            break;
        }
    }
    require(forces != nullptr, "forces module missing after migration");
    setUIntParameter(*forces, "collisionMode", static_cast<uint32_t>(ParticleCollisionMode::GroundPlane));
    setFloatParameter(*forces, "collisionGroundY", -0.5f);
    setFloatParameter(*forces, "collisionBounce", 0.8f);
    setFloatParameter(*forces, "collisionDamping", 0.6f);
    setBoolParameter(*forces, "killOnCollision", true);
    setUIntParameter(*forces, "spawnOnDeathCount", 3u);
    setUIntParameter(*forces, "spawnOnCollisionCount", 4u);
    setUIntParameter(*forces, "maxEventSpawnsPerFrame", 9u);
    setUIntParameter(*renderer, "blend", static_cast<uint32_t>(ParticleBlendMode::Additive));
    setStringParameter(*renderer, "texturePath", "resources/textures/engine_particle_fallback.png");
    setStringParameter(*renderer, "meshPath", "resources/models/cube.obj");
    setStringParameter(*renderer, "materialPath", "resources/materials/runtime_particle.material");
    setFloatParameter(*renderer, "meshSoftness", 18.0f);
    setFloatParameter(*renderer, "lightingInfluence", 0.25f);
    setBoolParameter(*renderer, "frustumCulling", false);
    setBoolParameter(*renderer, "distanceCulling", true);
    setBoolParameter(*renderer, "simulateWhenCulled", false);
    setFloatParameter(*renderer, "cullPadding", 1.5f);
    setFloatParameter(*renderer, "maxDrawDistance", 120.0f);
    setFloatParameter(*renderer, "lodNearDistance", 8.0f);
    setFloatParameter(*renderer, "lodFarDistance", 48.0f);
    setFloatParameter(*renderer, "lodMinSpawnScale", 0.20f);
    setFloatParameter(*renderer, "lodMinRenderScale", 0.30f);
    setFloatParameter(*renderer, "velocityStretch", 0.06f);
    setFloatParameter(*renderer, "velocityStretchMax", 3.5f);
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
    require(descriptor.materialPath == "resources/materials/runtime_particle.material",
            "material path did not apply");
    require(near(descriptor.runtime.effectScale, 1.75f), "effect scale did not apply");
    require(near(descriptor.runtime.importance, 4.0f), "importance did not apply");
    require(descriptor.runtime.budgetWeight == 5u, "budget weight did not apply");
    require(descriptor.runtime.maxSpawnPerFrame == 12u, "spawn budget did not apply");
    require(!descriptor.runtime.frustumCulling, "frustum culling flag did not apply");
    require(descriptor.runtime.distanceCulling, "distance culling flag did not apply");
    require(!descriptor.runtime.simulateWhenCulled, "culled simulation flag did not apply");
    require(near(descriptor.runtime.cullPadding, 1.5f), "cull padding did not apply");
    require(near(descriptor.runtime.maxDrawDistance, 120.0f), "draw distance did not apply");
    require(near(descriptor.runtime.lodNearDistance, 8.0f), "LOD near distance did not apply");
    require(near(descriptor.runtime.lodFarDistance, 48.0f), "LOD far distance did not apply");
    require(near(descriptor.runtime.lodMinSpawnScale, 0.20f), "LOD spawn scale did not apply");
    require(near(descriptor.runtime.lodMinRenderScale, 0.30f), "LOD render scale did not apply");
    require(near(descriptor.runtime.velocityStretch, 0.06f), "velocity stretch did not apply");
    require(near(descriptor.runtime.velocityStretchMax, 3.5f), "velocity stretch max did not apply");
    require(near(descriptor.runtime.meshSoftness, 18.0f), "mesh softness did not apply");
    require(near(descriptor.runtime.lightingInfluence, 0.25f), "lighting influence did not apply");
    require(descriptor.collision.mode == ParticleCollisionMode::GroundPlane, "collision mode did not apply");
    require(near(descriptor.collision.groundY, -0.5f), "collision ground did not apply");
    require(near(descriptor.collision.bounce, 0.8f), "collision bounce did not apply");
    require(near(descriptor.collision.damping, 0.6f), "collision damping did not apply");
    require(descriptor.collision.killOnCollision, "collision kill flag did not apply");
    require(descriptor.collision.spawnOnDeathCount == 3u, "death event spawn count did not apply");
    require(descriptor.collision.spawnOnCollisionCount == 4u, "collision event spawn count did not apply");
    require(descriptor.collision.maxEventSpawnsPerFrame == 9u, "event spawn cap did not apply");
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
    syncParticleGraphWithModules(emitter);
    asset.emitters.push_back(emitter);

    ParticleEffectEmitter secondEmitter;
    secondEmitter.stableId               = "second";
    secondEmitter.name                   = "Second";
    secondEmitter.descriptor.emissionRate = 99.0f;
    secondEmitter.descriptor.maxParticles = 24u;
    secondEmitter.modules                = buildModulesFromEmitterDescriptor(secondEmitter.descriptor);
    syncParticleGraphWithModules(secondEmitter);
    addParticleGraphFrameForNode(secondEmitter, secondEmitter.graph.nodes.front().id);
    addParticleGraphCommentForNode(secondEmitter, secondEmitter.graph.nodes.front().id);
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
    require(near(loaded.emitters.front().descriptor.runtime.effectScale, 1.75f),
            "loaded descriptor did not compile effect scale");
    require(near(loaded.emitters.front().descriptor.runtime.maxDrawDistance, 120.0f),
            "loaded descriptor did not compile draw distance");
    require(near(loaded.emitters.front().descriptor.runtime.velocityStretch, 0.06f),
            "loaded descriptor did not compile velocity stretch");
    require(loaded.emitters.front().descriptor.collision.mode == ParticleCollisionMode::GroundPlane,
            "loaded descriptor did not compile collision mode");
    require(loaded.emitters.front().descriptor.collision.spawnOnCollisionCount == 4u,
            "loaded descriptor did not compile collision event spawning");
    require(loaded.emitters.front().compiledProgram.valid, "loaded emitter should have compiled runtime program");
    require(loaded.emitters.front().compiledProgram.modules.size() == particleModuleDefinitions().size(),
            "loaded compiled program module count mismatch");
    require(near(loaded.emitters.front().compiledProgram.runtimeDescriptor.emissionRate, 12.5f),
            "loaded compiled descriptor did not preserve emission rate");
    require(selectParticleEffectEmitter(loaded, "second") != nullptr, "loaded multi-emitter asset cannot select second");
    require(near(selectParticleEffectEmitter(loaded, "second")->descriptor.emissionRate, 99.0f),
            "loaded second emitter descriptor was not preserved");
    require(loaded.emitters.front().graph.nodes.size() == loaded.emitters.front().modules.size(),
            "loaded graph node count mismatch");
    require(!loaded.emitters.front().graph.links.empty(), "loaded graph links missing");
    require(loaded.emitters[1].graph.frames.size() == 1u, "loaded graph frame missing");
    require(loaded.emitters[1].graph.comments.size() == 1u, "loaded graph comment missing");
    std::vector<ParticleModuleGraphDiagnostic> loadedGraphDiagnostics;
    require(validateParticleEffectGraph(loaded.emitters.front(), &loadedGraphDiagnostics),
            "loaded graph should validate");

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
    require(flatLoaded.emitters.front().compiledProgram.valid, "flat preset did not compile after migration");

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
    require(collisionLoaded.emitters.front().compiledProgram.valid,
            "structured parser collision asset did not compile after migration");

    std::error_code ec;
    std::filesystem::remove(path, ec);
    std::filesystem::remove(flatPath, ec);
    std::filesystem::remove(collisionPath, ec);
    return EXIT_SUCCESS;
}
