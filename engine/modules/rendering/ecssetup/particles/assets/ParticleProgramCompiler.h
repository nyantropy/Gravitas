#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "ParticleGraphAuthoring.h"

namespace gts::particles
{
    inline uint32_t particleProgramStaticParameterCount(const ParticleModuleInstance& module)
    {
        uint32_t count = 0;
        for (const ParticleModuleParameter& parameter : module.parameters)
        {
            switch (parameter.type)
            {
            case ParticleModuleParameterType::Float:
            case ParticleModuleParameterType::UInt:
            case ParticleModuleParameterType::Bool:
            case ParticleModuleParameterType::Enum:
            case ParticleModuleParameterType::String:
                count += 1;
                break;
            case ParticleModuleParameterType::FloatCurve:
            case ParticleModuleParameterType::ColorGradient:
            case ParticleModuleParameterType::BurstTimeline:
                break;
            }
        }
        return count;
    }

    inline uint32_t particleProgramCurveParameterCount(const ParticleModuleInstance& module)
    {
        uint32_t count = 0;
        for (const ParticleModuleParameter& parameter : module.parameters)
        {
            if (parameter.type == ParticleModuleParameterType::FloatCurve && !parameter.floatCurveValue.empty())
                count += 1;
            else if (parameter.type == ParticleModuleParameterType::ColorGradient &&
                     !parameter.colorGradientValue.empty())
                count += 1;
            else if (parameter.type == ParticleModuleParameterType::BurstTimeline &&
                     !parameter.burstTimelineValue.empty())
                count += 1;
        }
        return count;
    }

    inline bool particleProgramHasModuleStableId(const std::vector<ParticleModuleInstance>& modules,
                                                 const std::string&                         stableId)
    {
        return std::find_if(modules.begin(),
                            modules.end(),
                            [&](const ParticleModuleInstance& module)
                            {
                                return module.stableId == stableId;
                            }) != modules.end();
    }

    inline ParticleCompiledModule compileParticleModuleRecord(const ParticleModuleInstance& module)
    {
        ParticleCompiledModule record;
        record.stableId       = module.stableId;
        record.typeId         = module.typeId;
        record.displayName    = module.displayName;
        record.enabled        = module.enabled;
        record.parameterCount = static_cast<uint32_t>(module.parameters.size());

        if (const ParticleModuleDefinition* definition = findParticleModuleDefinition(module.typeId))
        {
            record.displayName       = record.displayName.empty() ? definition->displayName : record.displayName;
            record.executionStage    = definition->executionStage;
            record.executionCategory = definition->executionCategory;
            record.inputCount        = static_cast<uint32_t>(definition->inputs.size());
            record.outputCount       = static_cast<uint32_t>(definition->outputs.size());
            record.dependencyCount   = static_cast<uint32_t>(definition->dependencies.size());
        }

        return record;
    }

    inline std::vector<ParticleModuleInstance>
    collectParticleProgramModules(const ParticleEffectEmitter& emitter, uint32_t& deadNodesEliminated)
    {
        std::vector<ParticleModuleInstance> modules;
        modules.reserve(emitter.modules.size());

        if (!emitter.graph.nodes.empty())
        {
            for (const ParticleGraphNode& node : emitter.graph.nodes)
            {
                const ParticleModuleInstance* module = findParticleGraphModule(emitter, node.moduleStableId);
                if (module == nullptr)
                {
                    deadNodesEliminated += 1;
                    continue;
                }

                ParticleModuleInstance compiledModule = *module;
                completeModuleParameters(compiledModule);
                if (!compiledModule.enabled)
                    deadNodesEliminated += 1;
                modules.push_back(std::move(compiledModule));
            }

            for (const ParticleModuleInstance& module : emitter.modules)
            {
                if (!particleProgramHasModuleStableId(modules, module.stableId))
                    deadNodesEliminated += 1;
            }
            return modules;
        }

        for (const ParticleModuleInstance& module : emitter.modules)
        {
            ParticleModuleInstance compiledModule = module;
            completeModuleParameters(compiledModule);
            if (!compiledModule.enabled)
                deadNodesEliminated += 1;
            modules.push_back(std::move(compiledModule));
        }
        return modules;
    }

    inline uint32_t fusedParticleProgramModuleCount(const std::vector<ParticleCompiledModule>& modules)
    {
        std::array<bool, 4> usedStages = {false, false, false, false};
        uint32_t           stageCount = 0;
        for (const ParticleCompiledModule& module : modules)
        {
            const uint32_t stage = std::min<uint32_t>(executionStageOrder(module.executionStage),
                                                      static_cast<uint32_t>(usedStages.size() - 1));
            if (!usedStages[stage])
            {
                usedStages[stage] = true;
                stageCount += 1;
            }
        }

        if (modules.size() <= stageCount)
            return 0;
        return static_cast<uint32_t>(modules.size() - stageCount);
    }

    inline ParticleCompiledParticleProgram compileParticleEffectEmitter(const ParticleEffectEmitter& emitter)
    {
        ParticleCompiledParticleProgram program;
        program.schemaVersion    = CurrentParticleProgramSchemaVersion;
        program.backend          = ParticleProgramBackend::CpuDescriptor;
        program.runtimeDescriptor = emitter.descriptor;
        program.runtimeDescriptor.schemaVersion = CurrentParticleEmitterSchemaVersion;

        const bool graphValid = validateParticleEffectGraph(emitter, &program.diagnostics);

        uint32_t deadNodesEliminated = 0;
        std::vector<ParticleModuleInstance> modules =
            collectParticleProgramModules(emitter, deadNodesEliminated);

        if (modules.empty())
        {
            addGraphDiagnostic(&program.diagnostics,
                               ParticleModuleGraphDiagnosticSeverity::Error,
                               "compiled particle program has no modules");
        }

        const std::vector<const ParticleModuleInstance*> executionPlan = buildParticleModuleExecutionPlan(modules);
        std::vector<ParticleModuleInstance>              orderedModules;
        orderedModules.reserve(executionPlan.size());
        program.modules.reserve(executionPlan.size());
        for (const ParticleModuleInstance* module : executionPlan)
        {
            if (module == nullptr)
                continue;
            orderedModules.push_back(*module);
            program.modules.push_back(compileParticleModuleRecord(*module));
            program.staticParametersEvaluated += particleProgramStaticParameterCount(*module);
            program.curvesBaked += particleProgramCurveParameterCount(*module);
        }

        if (!orderedModules.empty())
            applyParticleModulesToEmitterDescriptor(orderedModules, program.runtimeDescriptor);

        program.deadNodesEliminated = deadNodesEliminated;
        program.modulesFused        = fusedParticleProgramModuleCount(program.modules);
        program.valid               = graphValid && !graphDiagnosticsHaveErrors(program.diagnostics);
        return program;
    }

    inline void compileParticleEffectAsset(ParticleEffectAsset& asset)
    {
        for (ParticleEffectEmitter& emitter : asset.emitters)
        {
            syncParticleGraphWithModules(emitter);
            emitter.compiledProgram = compileParticleEffectEmitter(emitter);
            if (emitter.compiledProgram.valid)
                emitter.descriptor = emitter.compiledProgram.runtimeDescriptor;
        }
    }

    inline const ParticleEmitterComponent& compiledParticleRuntimeDescriptor(const ParticleEffectEmitter& emitter)
    {
        return emitter.compiledProgram.valid ? emitter.compiledProgram.runtimeDescriptor : emitter.descriptor;
    }
} // namespace gts::particles
