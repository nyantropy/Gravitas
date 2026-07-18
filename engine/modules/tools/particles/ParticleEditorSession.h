#pragma once

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>

#include "ParticleEffectAsset.h"
#include "ParticleEffectAssetIO.h"
#include "ParticleModuleAuthoring.h"
#include "ParticleProgramCompiler.h"
#include "ToolStringUtils.h"

namespace gts::tools
{
    class ParticleEditorSession
    {
    public:
        bool hasAsset() const { return loaded; }
        bool isDirty() const { return dirty; }
        bool isPlaying() const { return !playbackPaused; }
        bool isPaused() const { return playbackPaused; }
        float timeScale() const { return playbackTimeScale; }
        const std::string& path() const { return currentPath; }

        const ParticleEffectAsset& asset() const { return currentAsset; }
        ParticleEffectAsset& asset() { return currentAsset; }

        size_t selectedEmitterIndex() const { return emitterIndex; }
        size_t selectedModuleIndex() const { return moduleIndex; }

        bool open(const std::string& path, std::string& status, bool protectDirty, bool forceReload)
        {
            if (path.empty())
                return false;

            if (protectDirty && dirty && path != currentPath)
            {
                status = "SAVE OR RELOAD PARTICLE EFFECT FIRST";
                return false;
            }

            if (!forceReload && path == currentPath && loaded)
                return false;

            ParticleEffectAsset loadedAsset;
            if (!gts::particles::loadParticleEffectAsset(path, loadedAsset))
            {
                status = "OPEN PARTICLE EFFECT FAILED";
                return false;
            }

            gts::particles::compileParticleEffectAsset(loadedAsset);
            currentAsset = std::move(loadedAsset);
            currentPath = path;
            loaded = true;
            dirty = false;
            playbackPaused = false;
            playbackTimeScale = 1.0f;
            emitterIndex = 0;
            moduleIndex = 0;
            status = "OPENED " + toolui::fileName(path);
            return true;
        }

        bool save(std::string& status)
        {
            if (!loaded || currentPath.empty())
            {
                status = "NO PARTICLE EFFECT SELECTED";
                return false;
            }

            ParticleEffectAsset saveAsset = assetForSave();
            if (!gts::particles::saveParticleEffectAsset(currentPath, saveAsset))
            {
                status = "SAVE PARTICLE EFFECT FAILED";
                return false;
            }

            currentAsset = std::move(saveAsset);
            dirty = false;
            status = "SAVED " + toolui::fileName(currentPath);
            return true;
        }

        bool reload(std::string& status)
        {
            if (currentPath.empty())
            {
                status = "NO PARTICLE EFFECT SELECTED";
                return false;
            }

            const std::string pathToReload = currentPath;
            const bool changed = open(pathToReload, status, false, true);
            if (changed)
                status = "RELOADED " + toolui::fileName(pathToReload);
            return changed;
        }

        void togglePlayback(std::string& status)
        {
            if (!loaded)
                return;

            playbackPaused = !playbackPaused;
            status = playbackPaused ? "PARTICLE PREVIEW PAUSED" : "PARTICLE PREVIEW PLAYING";
        }

        void selectEmitter(size_t index)
        {
            emitterIndex = index;
            moduleIndex = 0;
            clampSelection();
        }

        void selectModule(size_t index)
        {
            moduleIndex = index;
            clampSelection();
        }

        bool setSelectedEmitterEnabled(bool enabled, std::string& status)
        {
            ParticleEffectEmitter* emitter = selectedEmitter();
            if (emitter == nullptr)
            {
                status = "NO EMITTER SELECTED";
                return false;
            }

            if (emitter->descriptor.enabled == enabled)
                return false;

            emitter->descriptor.enabled = enabled;
            syncSelectedEmitterModuleParameters(*emitter);
            recompileSelectedEmitter();
            dirty = true;
            status = emitter->descriptor.enabled ? "EMITTER ENABLED" : "EMITTER DISABLED";
            return true;
        }

        bool setSelectedEmitterMaxParticles(uint32_t maxParticles, std::string& status)
        {
            ParticleEffectEmitter* emitter = selectedEmitter();
            if (emitter == nullptr)
            {
                status = "NO EMITTER SELECTED";
                return false;
            }

            const uint32_t clamped = std::max(1u, maxParticles);
            if (emitter->descriptor.maxParticles == clamped)
                return false;

            emitter->descriptor.maxParticles = clamped;
            syncSelectedEmitterModuleParameters(*emitter);
            recompileSelectedEmitter();
            dirty = true;
            status = "MAX PARTICLES UPDATED";
            return true;
        }

        bool setSelectedEmitterEmissionRate(float emissionRate, std::string& status)
        {
            ParticleEffectEmitter* emitter = selectedEmitter();
            if (emitter == nullptr)
            {
                status = "NO EMITTER SELECTED";
                return false;
            }

            const float clamped = std::max(0.0f, emissionRate);
            if (emitter->descriptor.emissionRate == clamped)
                return false;

            emitter->descriptor.emissionRate = clamped;
            syncSelectedEmitterModuleParameters(*emitter);
            recompileSelectedEmitter();
            dirty = true;
            status = "EMISSION RATE UPDATED";
            return true;
        }

        bool setSelectedEmitterLifetimeMin(float lifetimeMin, std::string& status)
        {
            ParticleEffectEmitter* emitter = selectedEmitter();
            if (emitter == nullptr)
            {
                status = "NO EMITTER SELECTED";
                return false;
            }

            const float clamped = std::clamp(lifetimeMin, 0.01f, std::max(0.01f, emitter->descriptor.lifetimeMax));
            if (emitter->descriptor.lifetimeMin == clamped)
                return false;

            emitter->descriptor.lifetimeMin = clamped;
            syncSelectedEmitterModuleParameters(*emitter);
            recompileSelectedEmitter();
            dirty = true;
            status = "LIFETIME MIN UPDATED";
            return true;
        }

        bool setSelectedEmitterLifetimeMax(float lifetimeMax, std::string& status)
        {
            ParticleEffectEmitter* emitter = selectedEmitter();
            if (emitter == nullptr)
            {
                status = "NO EMITTER SELECTED";
                return false;
            }

            const float clamped = std::max(emitter->descriptor.lifetimeMin, lifetimeMax);
            if (emitter->descriptor.lifetimeMax == clamped)
                return false;

            emitter->descriptor.lifetimeMax = clamped;
            syncSelectedEmitterModuleParameters(*emitter);
            recompileSelectedEmitter();
            dirty = true;
            status = "LIFETIME MAX UPDATED";
            return true;
        }

        bool setSelectedModuleEnabled(bool enabled, std::string& status)
        {
            ParticleEffectEmitter* emitter = selectedEmitter();
            if (emitter == nullptr || moduleIndex >= emitter->modules.size())
            {
                status = "NO MODULE SELECTED";
                return false;
            }

            gts::particles::ParticleModuleInstance& module = emitter->modules[moduleIndex];
            if (module.enabled == enabled)
                return false;

            module.enabled = enabled;
            recompileSelectedEmitter();
            dirty = true;
            status = module.enabled ? "MODULE ENABLED" : "MODULE DISABLED";
            return true;
        }

        ParticleEffectEmitter* selectedEmitter()
        {
            if (!loaded || emitterIndex >= currentAsset.emitters.size())
                return nullptr;
            return &currentAsset.emitters[emitterIndex];
        }

        const ParticleEffectEmitter* selectedEmitter() const
        {
            if (!loaded || emitterIndex >= currentAsset.emitters.size())
                return nullptr;
            return &currentAsset.emitters[emitterIndex];
        }

        const gts::particles::ParticleModuleInstance* selectedModule() const
        {
            const ParticleEffectEmitter* emitter = selectedEmitter();
            if (emitter == nullptr || moduleIndex >= emitter->modules.size())
                return nullptr;
            return &emitter->modules[moduleIndex];
        }

        void clampSelection()
        {
            if (!loaded || currentAsset.emitters.empty())
            {
                emitterIndex = 0;
                moduleIndex = 0;
                return;
            }

            emitterIndex = std::min(emitterIndex, currentAsset.emitters.size() - 1);
            const ParticleEffectEmitter& emitter = currentAsset.emitters[emitterIndex];
            if (emitter.modules.empty())
                moduleIndex = 0;
            else
                moduleIndex = std::min(moduleIndex, emitter.modules.size() - 1);
        }

        std::string title() const
        {
            if (!loaded)
                return "No Effect";
            if (!currentAsset.metadata.name.empty())
                return toolui::compact(currentAsset.metadata.name, 36);
            return toolui::compact(toolui::stemName(currentPath), 36);
        }

    private:
        ParticleEffectAsset assetForSave() const
        {
            ParticleEffectAsset saveAsset = currentAsset;
            saveAsset.schemaVersion = CurrentParticleEffectSchemaVersion;
            if (saveAsset.metadata.name.empty())
                saveAsset.metadata.name = std::filesystem::path(currentPath).stem().string();

            for (ParticleEffectEmitter& emitter : saveAsset.emitters)
            {
                gts::particles::migrateParticleEmitterModules(emitter.modules, emitter.descriptor);
                gts::particles::syncParticleGraphWithModules(emitter);
                emitter.compiledProgram = gts::particles::compileParticleEffectEmitter(emitter);
                if (emitter.compiledProgram.valid)
                    emitter.descriptor = emitter.compiledProgram.runtimeDescriptor;
                emitter.descriptor.effectPath.clear();
                emitter.descriptor.effectEmitterId.clear();
                emitter.descriptor.schemaVersion = CurrentParticleEmitterSchemaVersion;
                emitter.compiledProgram.runtimeDescriptor.effectPath.clear();
                emitter.compiledProgram.runtimeDescriptor.effectEmitterId.clear();
            }
            return saveAsset;
        }

        void recompileSelectedEmitter()
        {
            ParticleEffectEmitter* emitter = selectedEmitter();
            if (emitter == nullptr)
                return;

            gts::particles::migrateParticleEmitterModules(emitter->modules, emitter->descriptor);
            gts::particles::syncParticleGraphWithModules(*emitter);
            emitter->compiledProgram = gts::particles::compileParticleEffectEmitter(*emitter);
            if (emitter->compiledProgram.valid)
                emitter->descriptor = emitter->compiledProgram.runtimeDescriptor;
            emitter->descriptor.schemaVersion = CurrentParticleEmitterSchemaVersion;
        }

        static void syncSelectedEmitterModuleParameters(ParticleEffectEmitter& emitter)
        {
            for (gts::particles::ParticleModuleInstance& module : emitter.modules)
            {
                if (module.typeId == "spawn.basic")
                {
                    gts::particles::setBoolParameter(module, "emitterEnabled", emitter.descriptor.enabled);
                    gts::particles::setFloatParameter(module, "emissionRate", emitter.descriptor.emissionRate);
                    gts::particles::setUIntParameter(module, "maxParticles", emitter.descriptor.maxParticles);
                }
                else if (module.typeId == "lifetime.basic")
                {
                    gts::particles::setFloatParameter(module, "lifetimeMin", emitter.descriptor.lifetimeMin);
                    gts::particles::setFloatParameter(module, "lifetimeMax", emitter.descriptor.lifetimeMax);
                }
            }
        }

        bool loaded = false;
        bool dirty = false;
        bool playbackPaused = false;
        float playbackTimeScale = 1.0f;
        size_t emitterIndex = 0;
        size_t moduleIndex = 0;
        std::string currentPath;
        ParticleEffectAsset currentAsset;
    };
} // namespace gts::tools
