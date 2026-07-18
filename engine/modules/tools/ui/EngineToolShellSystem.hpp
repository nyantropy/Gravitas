#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "AssetBrowserSession.h"
#include "AssetPreviewWorld.hpp"
#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "EditorPreviewRenderData.h"
#include "EngineToolInputCaptureComponent.h"
#include "EngineToolShellComposition.h"
#include "EngineToolStateComponent.h"
#include "EngineToolUiHelpers.h"
#include "EngineToolWorkspaceComponent.h"
#include "GraphicsConstants.h"
#include "GtsPaths.h"
#include "InputBindingRegistry.h"
#include "ParticleEditorSession.h"
#include "ParticleEffectAsset.h"
#include "ParticleEmitterComponent.h"
#include "ParticleModuleAuthoring.h"
#include "ParticlePreviewWorld.hpp"
#include "RegisteredSceneInfo.h"
#include "RenderViewportComponent.h"
#include "TimeContext.h"
#include "ToolLaunchPreset.h"
#include "UiNode.h"
#include "UiSystem.h"

namespace gts::tools
{
    class EngineToolShellSystem : public ECSControllerSystem
    {
    public:
        void update(const EcsControllerContext& ctx) override
        {
            if (ctx.ui == nullptr)
                return;

            EngineToolStateComponent& state = ensureState(ctx.world);
            state.selectionChangedThisFrame = false;
            processVisibilityToggle(ctx, state);

            if (!state.visible)
            {
                deactivate(ctx, state);
                return;
            }

            setToolsInputContext(ctx, true);
            state.editorMode = EditorMode::Runtime;
            EngineToolWorkspaceComponent workspace = publishWorkspace(ctx, true);

            if (!ensureFont(ctx))
            {
                state.status = "TOOLS FONT NOT READY";
                return;
            }

            EngineToolShellComposition* shell = ensureComposition(*ctx.ui);
            if (shell == nullptr)
            {
                state.status = "TOOLS UI NOT READY";
                return;
            }

            const ToolWorkspace workspaceBeforeCommands = activeWorkspace;
            processCommands(ctx, state, *shell);
            if (activeWorkspace != workspaceBeforeCommands)
                workspace = publishWorkspace(ctx, true);
            if (activeWorkspace == ToolWorkspace::Particles)
                syncParticlePreviewWorld(ctx);

            ToolShellView view = buildView(ctx, state, workspace);
            shell->setView(view);
            ctx.ui->updateComposition(shellComposition);

            updateInputCapture(ctx.world, ctx);
            publishActivePreview(ctx, *shell);
            if (view.previewTexture != lastPreviewTexture ||
                view.assetPreviewTexture != lastAssetPreviewTexture)
            {
                view.previewTexture = lastPreviewTexture;
                view.assetPreviewTexture = lastAssetPreviewTexture;
                shell->setView(view);
                ctx.ui->updateComposition(shellComposition);
            }
        }

        void prepareVisibility(const EcsControllerContext& ctx)
        {
            EngineToolStateComponent& state = ensureState(ctx.world);
            state.selectionChangedThisFrame = false;
            processVisibilityToggle(ctx, state);
        }

        void shutdown()
        {
            if (previewWorld)
                previewWorld->destroy();
            if (assetPreviewWorld)
                assetPreviewWorld->destroy();
        }

        ToolWorkspace currentWorkspace() const
        {
            return activeWorkspace;
        }

        std::string applyStartupOptions(const ToolStartupOptions& options)
        {
            std::string status;
            bool previewNeedsReset = false;

            if (options.hasWorkspace)
            {
                activeWorkspace = options.workspace;
                status = workspaceStatus(activeWorkspace);
            }

            if (!options.particleEffect.empty())
            {
                if (particleSession.open(options.particleEffect, status, true, false))
                    previewNeedsReset = true;
            }

            if (options.hasSelectedEmitter)
            {
                particleSession.selectEmitter(options.selectedEmitter);
                previewNeedsReset = true;
            }

            if (options.hasSelectedModule)
            {
                particleSession.selectModule(options.selectedModule);
                previewNeedsReset = true;
            }

            if (!options.assetManifest.empty())
            {
                assetSession.select(options.assetManifest);
                activeWorkspace = ToolWorkspace::Assets;
                status = "ASSET BROWSER";
            }

            if (previewNeedsReset)
                resetParticlePreview();

            return status;
        }

    private:
        static constexpr const char* ToolsInputContext = "engine.tools";
        static constexpr const char* ToolsSelectAction = "engine.tools_select";
        static constexpr const char* ToolsOrbitAction = "engine.tool_camera_look";
        static constexpr const char* ToolsPanAction = "engine.tool_viewport_pan";

        static constexpr size_t ScenePageSize = 10;
        static constexpr size_t EffectPageSize = 6;
        static constexpr size_t AssetPageSize = 10;
        static constexpr size_t EmitterPageSize = 5;
        static constexpr size_t ModulePageSize = 4;

        static constexpr const char* PropertyEmitterEnabled = "particle.emitter.enabled";
        static constexpr const char* PropertyEmitterMaxParticles = "particle.emitter.maxParticles";
        static constexpr const char* PropertyEmitterEmissionRate = "particle.emitter.emissionRate";
        static constexpr const char* PropertyEmitterLifetimeMin = "particle.emitter.lifetimeMin";
        static constexpr const char* PropertyEmitterLifetimeMax = "particle.emitter.lifetimeMax";
        static constexpr const char* PropertyModuleEnabled = "particle.module.enabled";

        BitmapFont font;
        bool fontReady = false;
        bool toolsContextRequested = false;
        bool inputPointerReady = false;
        double previousInputMouseX = 0.0;
        double previousInputMouseY = 0.0;
        uint64_t lastVisibilityToggleFrame = std::numeric_limits<uint64_t>::max();
        UiCompositionId shellComposition = UI_INVALID_COMPOSITION;
        ToolWorkspace activeWorkspace = ToolWorkspace::Particles;

        std::vector<std::string> effectPaths;
        size_t sceneOffset = 0;
        size_t effectOffset = 0;
        size_t assetOffset = 0;
        texture_id_type lastPreviewTexture = 0;
        texture_id_type lastAssetPreviewTexture = 0;

        AssetBrowserSession assetSession;
        ParticleEditorSession particleSession;
        std::unique_ptr<ParticlePreviewWorld> previewWorld = std::make_unique<ParticlePreviewWorld>();
        std::unique_ptr<AssetPreviewWorld> assetPreviewWorld = std::make_unique<AssetPreviewWorld>();

        static EngineToolStateComponent& ensureState(ECSWorld& world)
        {
            if (!world.hasAny<EngineToolStateComponent>())
                return world.createSingleton<EngineToolStateComponent>();
            return world.getSingleton<EngineToolStateComponent>();
        }

        static EngineToolInputCaptureComponent& ensureInputCapture(ECSWorld& world)
        {
            if (!world.hasAny<EngineToolInputCaptureComponent>())
                return world.createSingleton<EngineToolInputCaptureComponent>();
            return world.getSingleton<EngineToolInputCaptureComponent>();
        }

        static std::string workspaceStatus(ToolWorkspace workspace)
        {
            switch (workspace)
            {
                case ToolWorkspace::World:
                    return "WORLD VIEWER";
                case ToolWorkspace::Particles:
                    return "PARTICLE EDITOR";
                case ToolWorkspace::Assets:
                    return "ASSET BROWSER";
            }
            return "TOOLS";
        }

        bool processVisibilityToggle(const EcsControllerContext& ctx, EngineToolStateComponent& state)
        {
            if (ctx.input == nullptr || !ctx.input->isPressed("engine.tools_toggle"))
                return false;

            const uint64_t frame = ctx.time == nullptr ? 0u : ctx.time->frame;
            if (lastVisibilityToggleFrame == frame)
                return false;

            lastVisibilityToggleFrame = frame;
            state.visible = !state.visible;
            state.status = state.visible ? "TOOLS VISIBLE" : "TOOLS HIDDEN";
            setToolsInputContext(ctx, state.visible);
            if (!state.visible)
                deactivate(ctx, state);
            return true;
        }

        void deactivate(const EcsControllerContext& ctx, EngineToolStateComponent& state)
        {
            state.editorMode = EditorMode::Runtime;
            setToolsInputContext(ctx, false);
            clearInputCapture(ctx.world);
            inputPointerReady = false;
            clearPreviewRender(ctx.world);
            if (previewWorld)
                previewWorld->destroy();
            if (assetPreviewWorld)
                assetPreviewWorld->destroy();
            if (ctx.ui != nullptr)
                destroyComposition(*ctx.ui);
            publishWorkspace(ctx, false);
        }

        void setToolsInputContext(const EcsControllerContext& ctx, bool enabled)
        {
            if (ctx.input == nullptr)
                return;

            if (enabled)
            {
                if (!toolsContextRequested && !ctx.input->isContextActive(ToolsInputContext))
                    ctx.input->pushContext(ToolsInputContext);
                toolsContextRequested = true;
                return;
            }

            if (toolsContextRequested || ctx.input->isContextActive(ToolsInputContext))
                ctx.input->popContext(ToolsInputContext);
            toolsContextRequested = false;
        }

        static void clearInputCapture(ECSWorld& world)
        {
            EngineToolInputCaptureComponent& capture = ensureInputCapture(world);
            capture = {};
        }

        EngineToolWorkspaceComponent& publishWorkspace(const EcsControllerContext& ctx, bool active)
        {
            const int width = std::max(1, static_cast<int>(std::round(ctx.windowPixelWidth)));
            const int height = std::max(1, static_cast<int>(std::round(ctx.windowPixelHeight)));
            return publishEngineToolWorkspace(ctx.world, width, height, active, activeWorkspace);
        }

        bool ensureFont(const EcsControllerContext& ctx)
        {
            if (fontReady)
                return true;
            if (ctx.resources == nullptr)
                return false;

            if (tryLoadFont(ctx, DefaultEditorTheme.typography.fontAsset))
                return true;

            return tryLoadFont(ctx, GraphicsConstants::ENGINE_RESOURCES + "/fonts/editor_sans.font.json");
        }

        bool tryLoadFont(const EcsControllerContext& ctx, const std::string& path)
        {
            if (ctx.resources == nullptr || path.empty())
                return false;

            const font_id_type fontID = ctx.resources->requestFont(path);
            const BitmapFont* loadedFont = ctx.resources->getFont(fontID);
            if (loadedFont == nullptr)
                return false;

            font = *loadedFont;
            fontReady = true;
            return true;
        }

        EngineToolShellComposition* ensureComposition(UiSystem& ui)
        {
            if (shellComposition != UI_INVALID_COMPOSITION)
            {
                if (auto* shell = dynamic_cast<EngineToolShellComposition*>(ui.findComposition(shellComposition)))
                    return shell;
                shellComposition = UI_INVALID_COMPOSITION;
            }

            UiMountDesc desc;
            desc.name = "engine-tool-shell";
            shellComposition = ui.mountComposition(std::make_unique<EngineToolShellComposition>(&font), desc);
            return dynamic_cast<EngineToolShellComposition*>(ui.findComposition(shellComposition));
        }

        void destroyComposition(UiSystem& ui)
        {
            if (shellComposition == UI_INVALID_COMPOSITION)
                return;
            ui.destroyComposition(shellComposition);
            shellComposition = UI_INVALID_COMPOSITION;
        }

        void processCommands(const EcsControllerContext& ctx,
                             EngineToolStateComponent& state,
                             EngineToolShellComposition& shell)
        {
            for (const ToolCommand& command : shell.consumeCommands())
            {
                switch (command.type)
                {
                    case ToolCommandType::SetWorkspace:
                        activeWorkspace = command.workspace;
                        state.status = workspaceStatus(activeWorkspace);
                        break;

                    case ToolCommandType::LoadScene:
                        requestSceneChange(ctx, state, command.value);
                        break;

                    case ToolCommandType::ScenePagePrevious:
                        sceneOffset = sceneOffset < ScenePageSize ? 0 : sceneOffset - ScenePageSize;
                        break;

                    case ToolCommandType::ScenePageNext:
                        sceneOffset += ScenePageSize;
                        break;

                    case ToolCommandType::OpenParticleEffect:
                        if (particleSession.open(command.value, state.status, true, false))
                        {
                            activeWorkspace = ToolWorkspace::Particles;
                            resetParticlePreview();
                        }
                        break;

                    case ToolCommandType::EffectPagePrevious:
                        effectOffset = effectOffset < EffectPageSize ? 0 : effectOffset - EffectPageSize;
                        break;

                    case ToolCommandType::EffectPageNext:
                        effectOffset += EffectPageSize;
                        break;

                    case ToolCommandType::SelectAsset:
                        assetSession.select(command.value);
                        activeWorkspace = ToolWorkspace::Assets;
                        lastAssetPreviewTexture = 0;
                        if (const AssetBrowserEntry* asset = assetSession.selected())
                        {
                            state.status = asset->valid ? "SELECTED " + assetDisplayName(*asset)
                                                          : "ASSET MANIFEST INVALID";
                        }
                        break;

                    case ToolCommandType::AssetPagePrevious:
                        assetOffset = assetOffset < AssetPageSize ? 0 : assetOffset - AssetPageSize;
                        break;

                    case ToolCommandType::AssetPageNext:
                        assetOffset += AssetPageSize;
                        break;

                    case ToolCommandType::SaveParticleEffect:
                        particleSession.save(state.status);
                        break;

                    case ToolCommandType::ReloadParticleEffect:
                        if (particleSession.reload(state.status))
                            resetParticlePreview();
                        break;

                    case ToolCommandType::ToggleParticlePlayback:
                        particleSession.togglePlayback(state.status);
                        break;

                    case ToolCommandType::RestartParticlePreview:
                        resetParticlePreview();
                        state.status = "PARTICLE PREVIEW RESTARTED";
                        break;

                    case ToolCommandType::SelectEmitter:
                        particleSession.selectEmitter(command.index);
                        break;

                    case ToolCommandType::SelectModule:
                        particleSession.selectModule(command.index);
                        break;

                    case ToolCommandType::EditProperty:
                        if (applyPropertyEdit(command, state.status))
                            resetParticlePreview();
                        break;
                }
            }
        }

        void resetParticlePreview()
        {
            if (previewWorld)
                previewWorld->resetSimulation();
        }

        void requestSceneChange(const EcsControllerContext& ctx,
                                EngineToolStateComponent& state,
                                const std::string& sceneId)
        {
            const std::string activeScene = ctx.activeSceneName == nullptr ? "" : *ctx.activeSceneName;
            if (sceneId.empty() || sceneId == activeScene)
            {
                state.status = "SCENE ACTIVE";
                return;
            }
            if (ctx.engineCommands == nullptr)
            {
                state.status = "NO SCENE COMMANDS";
                return;
            }

            ctx.engineCommands->requestChangeScene(sceneId);
            state.status = "LOADING " + sceneId;
        }

        ToolShellView buildView(const EcsControllerContext& ctx,
                                EngineToolStateComponent& state,
                                const EngineToolWorkspaceComponent& workspace)
        {
            collectEffectPaths(ctx.world);
            collectAssetManifests();
            clampPaging(ctx);
            particleSession.clampSelection();

            ToolShellView view;
            view.activeWorkspace = activeWorkspace;
            view.topBarHeight = workspace.topBarHeight;
            view.viewportToolbarHeight = workspace.viewportToolbarHeight;
            view.leftPaneWidth = workspace.leftRailWidth;
            view.rightPaneWidth = workspace.rightPaneWidth;
            view.bottomDockHeight = workspace.bottomDockHeight;
            view.bottomBarHeight = workspace.bottomBarHeight;
            view.viewportX = workspace.viewportX;
            view.viewportY = workspace.viewportY;
            view.viewportWidth = workspace.viewportWidth;
            view.viewportHeight = workspace.viewportHeight;
            view.activeSceneName = ctx.activeSceneName == nullptr ? "" : *ctx.activeSceneName;
            view.status = state.status;
            view.sceneOffset = sceneOffset;
            view.effectOffset = effectOffset;
            view.assetOffset = assetOffset;
            view.previewTexture = lastPreviewTexture;
            view.assetPreviewTexture = lastAssetPreviewTexture;
            view.particleLoaded = particleSession.hasAsset();
            view.particleDirty = particleSession.isDirty();
            view.particlePlaying = particleSession.isPlaying();
            view.particlePath = particleSession.path();
            view.particleTitle = particleSession.title();
            view.previewTimeScale = particleSession.timeScale();

            if (const ParticleEffectEmitter* emitter = particleSession.selectedEmitter())
            {
                view.hasSelectedEmitter = true;
                view.selectedEmitterEnabled = emitter->descriptor.enabled;
                view.previewEmitterName = emitter->name.empty() ? emitter->stableId : emitter->name;
                if (particleSession.selectedModule() != nullptr)
                {
                    view.hasSelectedModule = true;
                    view.selectedModuleEnabled = particleSession.selectedModule()->enabled;
                    view.previewModuleName = particleSession.selectedModule()->displayName.empty()
                        ? particleSession.selectedModule()->typeId
                        : particleSession.selectedModule()->displayName;
                }
            }

            fillSceneRows(ctx, view);
            fillEffectRows(view);
            fillAssetRows(view);
            fillSelectedAsset(view);
            fillParticleRows(view);
            fillPreviewSummary(ctx, view);
            fillPropertySections(view);
            fillDiagnostics(view);
            return view;
        }

        void fillSceneRows(const EcsControllerContext& ctx, ToolShellView& view)
        {
            const std::vector<RegisteredSceneInfo>* scenes = ctx.registeredScenes;
            const std::string activeScene = ctx.activeSceneName == nullptr ? "" : *ctx.activeSceneName;
            view.sceneTotal = scenes == nullptr ? 0 : scenes->size();
            if (scenes == nullptr)
                return;

            const size_t end = std::min(scenes->size(), sceneOffset + ScenePageSize);
            for (size_t i = sceneOffset; i < end; ++i)
            {
                const RegisteredSceneInfo& scene = (*scenes)[i];
                const std::string display = scene.displayName.empty() ? scene.id : scene.displayName;
                ToolSceneRow row;
                row.id = scene.id;
                row.active = scene.id == activeScene;
                row.label = toolui::compact(display, 30);
                view.scenes.push_back(std::move(row));
            }
        }

        void fillEffectRows(ToolShellView& view)
        {
            view.effectTotal = effectPaths.size();
            const size_t end = std::min(effectPaths.size(), effectOffset + EffectPageSize);
            for (size_t i = effectOffset; i < end; ++i)
            {
                ToolEffectRow row;
                row.path = effectPaths[i];
                row.active = row.path == particleSession.path();
                row.label = toolui::compact(toolui::stemName(row.path), 30);
                view.effects.push_back(std::move(row));
            }
        }

        void fillAssetRows(ToolShellView& view) const
        {
            const std::vector<AssetBrowserEntry>& assets = assetSession.assets();
            view.assetTotal = assets.size();
            const size_t end = std::min(assets.size(), assetOffset + AssetPageSize);
            for (size_t i = assetOffset; i < end; ++i)
            {
                const AssetBrowserEntry& asset = assets[i];
                ToolAssetRow row;
                row.manifestPath = asset.manifestPath;
                row.active = i == assetSession.selectedIndex();
                row.valid = asset.valid;
                row.label = toolui::compact(assetDisplayName(asset), 30);
                row.summary = asset.valid ? assetMaterialModeSummary(asset.manifest.materialMode) : "invalid";
                view.assets.push_back(std::move(row));
            }
        }

        void fillSelectedAsset(ToolShellView& view) const
        {
            const AssetBrowserEntry* asset = assetSession.selected();
            if (asset == nullptr)
                return;

            view.assetSelected = true;
            view.assetSelectedValid = asset->valid;
            view.assetManifestPath = asset->manifestPath;
            if (!asset->valid)
            {
                view.assetTitle = toolui::stemName(std::filesystem::path(asset->manifestPath).parent_path().string());
                view.assetError = asset->error;
                return;
            }

            view.assetTitle = assetDisplayName(*asset);
            view.assetModelPath = asset->manifest.modelPath;
            view.assetTexturePath = asset->manifest.fallbackTexturePath;
            view.assetSourcePath = asset->manifest.sourceModelPath;
            view.assetMaterialMode = assetMaterialModeSummary(asset->manifest.materialMode);
            view.assetBounds = boundsText(asset->manifest.bounds);
        }

        void fillParticleRows(ToolShellView& view) const
        {
            if (!particleSession.hasAsset())
                return;

            const ParticleEffectAsset& asset = particleSession.asset();
            const size_t emitterEnd = std::min(asset.emitters.size(), EmitterPageSize);
            for (size_t i = 0; i < emitterEnd; ++i)
            {
                const ParticleEffectEmitter& emitter = asset.emitters[i];
                ToolEmitterRow row;
                row.index = i;
                row.active = i == particleSession.selectedEmitterIndex();
                row.enabled = emitter.descriptor.enabled;
                row.label = toolui::compact(emitter.name.empty() ? emitter.stableId : emitter.name, 22);
                row.summary = emitter.descriptor.enabled ? "on" : "off";
                if (!emitter.compiledProgram.valid)
                    row.summary += " invalid";
                else
                    row.summary += " " + std::to_string(emitter.compiledProgram.modules.size()) + " modules";
                view.emitters.push_back(std::move(row));
            }

            const ParticleEffectEmitter* emitter = particleSession.selectedEmitter();
            if (emitter == nullptr)
                return;

            const size_t moduleEnd = std::min(emitter->modules.size(), ModulePageSize);
            for (size_t i = 0; i < moduleEnd; ++i)
            {
                const gts::particles::ParticleModuleInstance& module = emitter->modules[i];
                const gts::particles::ParticleModuleDefinition* definition =
                    gts::particles::findParticleModuleDefinition(module.typeId);

                ToolModuleRow row;
                row.index = i;
                row.active = i == particleSession.selectedModuleIndex();
                row.enabled = module.enabled;
                row.label = toolui::compact(module.displayName.empty() ? module.typeId : module.displayName, 22);
                row.summary = definition == nullptr
                    ? std::string("custom")
                    : std::string(gts::particles::executionStageShortLabel(definition->executionStage));
                row.summary += " " + std::to_string(module.parameters.size()) + " params";
                view.modules.push_back(std::move(row));
            }
        }

        void fillPreviewSummary(const EcsControllerContext& ctx, ToolShellView& view) const
        {
            if (!particleSession.hasAsset())
                return;

            const ParticleEffectAsset& asset = particleSession.asset();
            view.previewEmitterCount = static_cast<uint32_t>(asset.emitters.size());
            for (const ParticleEffectEmitter& emitter : asset.emitters)
            {
                if (emitter.descriptor.enabled)
                {
                    view.previewMaxParticles += emitter.descriptor.maxParticles;
                    view.previewLooping = view.previewLooping || emitter.descriptor.looping;
                }
            }

            if (const ParticleEffectEmitter* emitter = particleSession.selectedEmitter())
                view.previewModuleCount = static_cast<uint32_t>(emitter->modules.size());

            if (!ctx.world.hasAny<EditorPreviewRenderComponent>())
                return;

            const ParticleFrameData& particleData = ctx.world.getSingleton<EditorPreviewRenderComponent>().data.particleData;
            view.previewLiveParticles = particleData.simulatedParticleCount;
            view.previewRenderedParticles = particleData.renderedParticleCount;
            if (view.previewRenderedParticles == 0)
            {
                view.previewRenderedParticles = static_cast<uint32_t>(particleData.instances.size() +
                                                                      particleData.meshInstances.size());
            }
        }

        void fillPropertySections(ToolShellView& view) const
        {
            if (activeWorkspace == ToolWorkspace::World)
            {
                ToolPropertySection scene;
                scene.title = "Scene Inspector";
                scene.properties.push_back(readOnlyProperty("world.scene.active",
                                                            "Active scene",
                                                            view.activeSceneName.empty() ? "none" : view.activeSceneName));
                scene.properties.push_back(readOnlyProperty("world.scene.count",
                                                            "Registered scenes",
                                                            std::to_string(view.sceneTotal)));
                scene.properties.push_back(readOnlyProperty("world.scene.commands",
                                                            "Scene changes",
                                                            "engine commands"));
                scene.properties.push_back(readOnlyProperty("world.viewport",
                                                            "Viewport",
                                                            "runtime scene"));
                view.propertySections.push_back(std::move(scene));
                return;
            }

            if (activeWorkspace == ToolWorkspace::Assets)
            {
                fillAssetPropertySections(view);
                return;
            }

            if (!particleSession.hasAsset())
            {
                ToolPropertySection empty;
                empty.title = "Particle Inspector";
                empty.properties.push_back(readOnlyProperty("particle.empty.asset",
                                                            "Asset",
                                                            "Select a particle asset"));
                empty.properties.push_back(readOnlyProperty("particle.empty.preview",
                                                            "Preview",
                                                            "separate viewport"));
                view.propertySections.push_back(std::move(empty));
                return;
            }

            const ParticleEffectAsset& asset = particleSession.asset();
            ToolPropertySection effect;
            effect.title = "Effect";
            effect.properties.push_back(readOnlyProperty("particle.effect.name", "Name", particleSession.title()));
            effect.properties.push_back(readOnlyProperty("particle.effect.emitters",
                                                        "Emitters",
                                                        std::to_string(asset.emitters.size())));
            view.propertySections.push_back(std::move(effect));

            const ParticleEffectEmitter* emitter = particleSession.selectedEmitter();
            if (emitter == nullptr)
                return;

            ToolPropertySection emitterSection;
            emitterSection.title = "Emitter";
            emitterSection.properties.push_back(readOnlyProperty("particle.emitter.name",
                                                                 "Name",
                                                                 emitter->name.empty() ? emitter->stableId : emitter->name));
            emitterSection.properties.push_back(boolProperty(PropertyEmitterEnabled,
                                                             "Enabled",
                                                             emitter->descriptor.enabled));
            emitterSection.properties.push_back(uintProperty(PropertyEmitterMaxParticles,
                                                             "Max particles",
                                                             emitter->descriptor.maxParticles,
                                                             1u,
                                                             4096u,
                                                             16u));
            emitterSection.properties.push_back(floatProperty(PropertyEmitterEmissionRate,
                                                              "Emission rate",
                                                              emitter->descriptor.emissionRate,
                                                              0.0f,
                                                              512.0f,
                                                              1.0f));
            emitterSection.properties.push_back(floatProperty(PropertyEmitterLifetimeMin,
                                                              "Lifetime min",
                                                              emitter->descriptor.lifetimeMin,
                                                              0.01f,
                                                              std::max(0.01f, emitter->descriptor.lifetimeMax),
                                                              0.10f));
            emitterSection.properties.push_back(floatProperty(PropertyEmitterLifetimeMax,
                                                              "Lifetime max",
                                                              emitter->descriptor.lifetimeMax,
                                                              emitter->descriptor.lifetimeMin,
                                                              60.0f,
                                                              0.10f));
            view.propertySections.push_back(std::move(emitterSection));

            if (const gts::particles::ParticleModuleInstance* module = particleSession.selectedModule())
            {
                ToolPropertySection moduleSection;
                moduleSection.title = "Module";
                moduleSection.properties.push_back(readOnlyProperty("particle.module.name",
                                                                    "Name",
                                                                    module->displayName.empty()
                                                                        ? module->typeId
                                                                        : module->displayName));
                moduleSection.properties.push_back(boolProperty(PropertyModuleEnabled, "Enabled", module->enabled));
                view.propertySections.push_back(std::move(moduleSection));

                ToolPropertySection parameters;
                parameters.title = "Module Parameters";
                constexpr size_t MaxDisplayedModuleParameterRows = 18;
                const bool hasHiddenParameters = module->parameters.size() > MaxDisplayedModuleParameterRows;
                const size_t parameterCount = hasHiddenParameters
                    ? MaxDisplayedModuleParameterRows - 1
                    : module->parameters.size();
                for (size_t i = 0; i < parameterCount; ++i)
                {
                    const gts::particles::ParticleModuleParameter& parameter = module->parameters[i];
                    parameters.properties.push_back(readOnlyProperty("particle.module.parameter." + parameter.id,
                                                                     parameterLabel(*module, parameter),
                                                                     parameterValueText(*module, parameter)));
                }
                if (hasHiddenParameters)
                {
                    parameters.properties.push_back(readOnlyProperty("particle.module.parameter.more",
                                                                     "More",
                                                                     std::to_string(module->parameters.size() -
                                                                                    parameterCount) +
                                                                         " hidden"));
                }
                if (!parameters.properties.empty())
                view.propertySections.push_back(std::move(parameters));
            }
        }

        void fillAssetPropertySections(ToolShellView& view) const
        {
            const AssetBrowserEntry* asset = assetSession.selected();
            if (asset == nullptr)
            {
                ToolPropertySection empty;
                empty.title = "Asset Inspector";
                empty.properties.push_back(readOnlyProperty("asset.browser.count",
                                                            "Assets",
                                                            std::to_string(view.assetTotal)));
                empty.properties.push_back(readOnlyProperty("asset.browser.selection",
                                                            "Selection",
                                                            "none"));
                view.propertySections.push_back(std::move(empty));
                return;
            }

            ToolPropertySection manifest;
            manifest.title = "Manifest";
            manifest.properties.push_back(readOnlyProperty("asset.manifest.path",
                                                           "Path",
                                                           toolui::compact(asset->manifestPath, 38)));
            manifest.properties.push_back(readOnlyProperty("asset.manifest.state",
                                                           "State",
                                                           asset->valid ? "valid" : "invalid"));
            if (!asset->valid)
            {
                manifest.properties.push_back(readOnlyProperty("asset.manifest.error",
                                                               "Error",
                                                               toolui::compact(asset->error, 40)));
                view.propertySections.push_back(std::move(manifest));
                return;
            }

            manifest.properties.push_back(readOnlyProperty("asset.id",
                                                           "Id",
                                                           asset->manifest.id));
            manifest.properties.push_back(readOnlyProperty("asset.display_name",
                                                           "Name",
                                                           asset->manifest.displayName));
            view.propertySections.push_back(std::move(manifest));

            ToolPropertySection files;
            files.title = "Files";
            files.properties.push_back(readOnlyProperty("asset.model",
                                                        "Model",
                                                        toolui::compact(asset->manifest.modelPath, 40)));
            files.properties.push_back(readOnlyProperty("asset.texture",
                                                        "Texture",
                                                        asset->manifest.fallbackTexturePath.empty()
                                                            ? "none"
                                                            : toolui::compact(asset->manifest.fallbackTexturePath, 40)));
            files.properties.push_back(readOnlyProperty("asset.source",
                                                        "Source",
                                                        asset->manifest.sourceModelPath.empty()
                                                            ? "none"
                                                            : toolui::compact(asset->manifest.sourceModelPath, 40)));
            view.propertySections.push_back(std::move(files));

            ToolPropertySection render;
            render.title = "Render";
            render.properties.push_back(readOnlyProperty("asset.material_mode",
                                                         "Material",
                                                         assetMaterialModeSummary(asset->manifest.materialMode)));
            render.properties.push_back(readOnlyProperty("asset.scale",
                                                         "Scale",
                                                         vec3Text(asset->manifest.scale)));
            render.properties.push_back(readOnlyProperty("asset.bounds",
                                                         "Bounds",
                                                         boundsText(asset->manifest.bounds)));
            render.properties.push_back(readOnlyProperty("asset.base",
                                                         "Base",
                                                         asset->manifest.baseAtGround ? "grounded" : "authored"));
            render.properties.push_back(readOnlyProperty("asset.preview.camera",
                                                         "Camera",
                                                         toolui::fixed(asset->manifest.preview.cameraDistance, 2)));
            view.propertySections.push_back(std::move(render));
        }

        bool applyPropertyEdit(const ToolCommand& command, std::string& status)
        {
            if (command.value == PropertyEmitterEnabled)
                return particleSession.setSelectedEmitterEnabled(command.boolValue, status);
            if (command.value == PropertyEmitterMaxParticles)
                return particleSession.setSelectedEmitterMaxParticles(command.uintValue, status);
            if (command.value == PropertyEmitterEmissionRate)
                return particleSession.setSelectedEmitterEmissionRate(command.floatValue, status);
            if (command.value == PropertyEmitterLifetimeMin)
                return particleSession.setSelectedEmitterLifetimeMin(command.floatValue, status);
            if (command.value == PropertyEmitterLifetimeMax)
                return particleSession.setSelectedEmitterLifetimeMax(command.floatValue, status);
            if (command.value == PropertyModuleEnabled)
                return particleSession.setSelectedModuleEnabled(command.boolValue, status);

            status = "UNKNOWN PROPERTY";
            return false;
        }

        void fillDiagnostics(ToolShellView& view) const
        {
            if (activeWorkspace == ToolWorkspace::World)
            {
                view.diagnostics.push_back("Viewport: runtime scene");
                view.diagnostics.push_back("Input: UI panes capture; viewport passes through");
                view.diagnostics.push_back("Mode: Runtime");
                return;
            }

            if (activeWorkspace == ToolWorkspace::Assets)
            {
                view.diagnostics.push_back("Assets: " + std::to_string(assetSession.assets().size()) + " manifests");
                if (const AssetBrowserEntry* asset = assetSession.selected())
                {
                    view.diagnostics.push_back(std::string("Selected: ") + assetDisplayName(*asset));
                    view.diagnostics.push_back(std::string("Manifest: ") + (asset->valid ? "valid" : "invalid"));
                    if (!asset->valid)
                        view.diagnostics.push_back(toolui::compact(asset->error, 52));
                }
                else
                {
                    view.diagnostics.push_back("Selected: none");
                }
                return;
            }

            view.diagnostics.push_back(std::string("Preview: ") +
                                       (particleSession.isPlaying() ? "playing" : "paused"));
            view.diagnostics.push_back(std::string("Asset: ") +
                                       (particleSession.isDirty() ? "dirty" : "saved"));

            const ParticleEffectEmitter* emitter = particleSession.selectedEmitter();
            if (emitter == nullptr)
            {
                view.diagnostics.push_back("Emitter: none");
                return;
            }

            view.diagnostics.push_back(std::string("Emitter program: ") +
                                       (emitter->compiledProgram.valid ? "valid" : "invalid"));
            view.diagnostics.push_back("Compiled modules: " +
                                       std::to_string(emitter->compiledProgram.modules.size()));
        }

        static ToolCommand propertyEditCommand(const std::string& id)
        {
            ToolCommand command;
            command.type = ToolCommandType::EditProperty;
            command.value = id;
            return command;
        }

        static ToolPropertyDescriptor readOnlyProperty(const std::string& id,
                                                       const std::string& displayName,
                                                       const std::string& value)
        {
            ToolPropertyDescriptor descriptor;
            descriptor.id = id;
            descriptor.displayName = displayName;
            descriptor.kind = ToolPropertyKind::ReadOnlyText;
            descriptor.readOnly = true;
            descriptor.textValue = value;
            return descriptor;
        }

        static std::string assetDisplayName(const AssetBrowserEntry& asset)
        {
            if (asset.valid && !asset.manifest.displayName.empty())
                return asset.manifest.displayName;

            const std::filesystem::path manifestPath(asset.manifestPath);
            const std::string folder = manifestPath.parent_path().filename().string();
            return folder.empty() ? toolui::fileName(asset.manifestPath) : folder;
        }

        static std::string assetMaterialModeSummary(AssetMaterialMode mode)
        {
            switch (mode)
            {
                case AssetMaterialMode::CookedMeshMaterials:
                    return "lit cooked materials";
                case AssetMaterialMode::UnlitTextureOverride:
                default:
                    return "unlit texture override";
            }
        }

        static std::string vec3Text(const glm::vec3& value)
        {
            return toolui::fixed(value.x, 2) + ", " +
                toolui::fixed(value.y, 2) + ", " +
                toolui::fixed(value.z, 2);
        }

        static std::string boundsText(const BoundsComponent& bounds)
        {
            return "min " + vec3Text(bounds.min) + "  max " + vec3Text(bounds.max);
        }

        static ToolPropertyDescriptor boolProperty(const std::string& id,
                                                   const std::string& displayName,
                                                   bool value)
        {
            ToolPropertyDescriptor descriptor;
            descriptor.id = id;
            descriptor.displayName = displayName;
            descriptor.kind = ToolPropertyKind::Bool;
            descriptor.boolValue = value;
            descriptor.command = propertyEditCommand(id);
            return descriptor;
        }

        static ToolPropertyDescriptor floatProperty(const std::string& id,
                                                    const std::string& displayName,
                                                    float value,
                                                    float minValue,
                                                    float maxValue,
                                                    float step)
        {
            ToolPropertyDescriptor descriptor;
            descriptor.id = id;
            descriptor.displayName = displayName;
            descriptor.kind = ToolPropertyKind::Float;
            descriptor.floatValue = value;
            descriptor.minFloat = minValue;
            descriptor.maxFloat = maxValue;
            descriptor.floatStep = step;
            descriptor.command = propertyEditCommand(id);
            return descriptor;
        }

        static ToolPropertyDescriptor uintProperty(const std::string& id,
                                                   const std::string& displayName,
                                                   uint32_t value,
                                                   uint32_t minValue,
                                                   uint32_t maxValue,
                                                   uint32_t step)
        {
            ToolPropertyDescriptor descriptor;
            descriptor.id = id;
            descriptor.displayName = displayName;
            descriptor.kind = ToolPropertyKind::UInt;
            descriptor.uintValue = value;
            descriptor.minUInt = minValue;
            descriptor.maxUInt = maxValue;
            descriptor.uintStep = step;
            descriptor.command = propertyEditCommand(id);
            return descriptor;
        }

        static std::string parameterLabel(const gts::particles::ParticleModuleInstance& module,
                                          const gts::particles::ParticleModuleParameter& parameter)
        {
            const gts::particles::ParticleModuleDefinition* definition =
                gts::particles::findParticleModuleDefinition(module.typeId);
            if (definition != nullptr)
            {
                const auto it = std::find_if(definition->parameters.begin(),
                                             definition->parameters.end(),
                                             [&](const gts::particles::ParticleModuleParameterDefinition& candidate)
                                             {
                                                 return candidate.id == parameter.id;
                                             });
                if (it != definition->parameters.end() && !it->label.empty())
                    return it->label;
            }
            return parameter.id;
        }

        static std::string parameterValueText(const gts::particles::ParticleModuleInstance& module,
                                              const gts::particles::ParticleModuleParameter& parameter)
        {
            using Type = gts::particles::ParticleModuleParameterType;
            switch (parameter.type)
            {
                case Type::Float:
                    return toolui::fixed(parameter.floatValue);
                case Type::UInt:
                    return std::to_string(parameter.uintValue);
                case Type::Bool:
                    return parameter.boolValue ? "true" : "false";
                case Type::Enum:
                    return enumParameterValueText(module, parameter);
                case Type::String:
                    return parameter.stringValue.empty() ? "<empty>" : toolui::compact(parameter.stringValue, 26);
                case Type::FloatCurve:
                    return std::to_string(parameter.floatCurveValue.size()) + " keys";
                case Type::ColorGradient:
                    return std::to_string(parameter.colorGradientValue.size()) + " colors";
                case Type::BurstTimeline:
                    return std::to_string(parameter.burstTimelineValue.size()) + " bursts";
            }
            return {};
        }

        static std::string enumParameterValueText(const gts::particles::ParticleModuleInstance& module,
                                                  const gts::particles::ParticleModuleParameter& parameter)
        {
            const gts::particles::ParticleModuleDefinition* definition =
                gts::particles::findParticleModuleDefinition(module.typeId);
            if (definition != nullptr)
            {
                const auto paramIt = std::find_if(definition->parameters.begin(),
                                                  definition->parameters.end(),
                                                  [&](const gts::particles::ParticleModuleParameterDefinition& candidate)
                                                  {
                                                      return candidate.id == parameter.id;
                                                  });
                if (paramIt != definition->parameters.end())
                {
                    const auto optionIt = std::find_if(paramIt->enumOptions.begin(),
                                                       paramIt->enumOptions.end(),
                                                       [&](const gts::particles::ParticleModuleEnumOption& option)
                                                       {
                                                           return option.value == parameter.uintValue;
                                                       });
                    if (optionIt != paramIt->enumOptions.end())
                        return optionIt->label;
                }
            }
            return std::to_string(parameter.uintValue);
        }

        void collectEffectPaths(ECSWorld& world)
        {
            effectPaths.clear();

            if (world.hasAny<ParticleEffectRegistryComponent>())
            {
                const ParticleEffectRegistryComponent& registry = world.getSingleton<ParticleEffectRegistryComponent>();
                for (const auto& entry : registry.effects)
                    effectPaths.push_back(entry.first);
            }

            world.forEach<ParticleEmitterComponent>(
                [&](Entity, ParticleEmitterComponent& emitter)
                {
                    if (!emitter.effectPath.empty())
                        effectPaths.push_back(emitter.effectPath);
                });

            scanParticleEffectDirectory("resources/particles");
            scanParticleEffectDirectory("../resources/particles");
            scanParticleEffectDirectory("../../resources/particles");

            if (!particleSession.path().empty())
                effectPaths.push_back(particleSession.path());

            std::sort(effectPaths.begin(), effectPaths.end());
            effectPaths.erase(std::unique(effectPaths.begin(), effectPaths.end()), effectPaths.end());
        }

        void scanParticleEffectDirectory(const std::filesystem::path& rootPath)
        {
            std::error_code ec;
            if (!std::filesystem::exists(rootPath, ec) || !std::filesystem::is_directory(rootPath, ec))
                return;

            const std::filesystem::recursive_directory_iterator end;
            std::filesystem::recursive_directory_iterator it(
                rootPath,
                std::filesystem::directory_options::skip_permission_denied,
                ec);
            while (!ec && it != end)
            {
                const std::filesystem::directory_entry& entry = *it;
                if (entry.is_regular_file(ec) && entry.path().extension() == ".json")
                    effectPaths.push_back(entry.path().generic_string());
                it.increment(ec);
            }
        }

        void collectAssetManifests()
        {
            assetSession.refresh({
                GtsPaths::GetProjectRoot() / "resources/assets",
                "resources/assets",
                "../resources/assets",
                "../../resources/assets"
            });
        }

        void clampPaging(const EcsControllerContext& ctx)
        {
            const size_t sceneTotal = ctx.registeredScenes == nullptr ? 0 : ctx.registeredScenes->size();
            if (sceneTotal == 0)
                sceneOffset = 0;
            else if (sceneOffset >= sceneTotal)
                sceneOffset = ((sceneTotal - 1) / ScenePageSize) * ScenePageSize;

            if (effectPaths.empty())
                effectOffset = 0;
            else if (effectOffset >= effectPaths.size())
                effectOffset = ((effectPaths.size() - 1) / EffectPageSize) * EffectPageSize;

            const size_t assetTotal = assetSession.assets().size();
            if (assetTotal == 0)
                assetOffset = 0;
            else if (assetOffset >= assetTotal)
                assetOffset = ((assetTotal - 1) / AssetPageSize) * AssetPageSize;
        }

        void syncParticlePreviewWorld(const EcsControllerContext& ctx)
        {
            if (!particleSession.hasAsset() || particleSession.path().empty() || !previewWorld)
                return;

            previewWorld->ensure(ctx.resources);
            previewWorld->syncAsset(particleSession.path(),
                                    particleSession.asset(),
                                    particleSession.timeScale(),
                                    particleSession.isPaused());
        }

        bool syncAssetPreviewWorld(const EcsControllerContext& ctx)
        {
            const AssetBrowserEntry* asset = assetSession.selected();
            if (asset == nullptr || !asset->valid || !assetPreviewWorld || ctx.resources == nullptr)
                return false;

            assetPreviewWorld->ensure(ctx.resources);
            assetPreviewWorld->syncAsset(asset->manifestPath, asset->manifest);
            return true;
        }

        void publishActivePreview(const EcsControllerContext& ctx, EngineToolShellComposition& shell)
        {
            switch (activeWorkspace)
            {
                case ToolWorkspace::Particles:
                    publishParticlePreview(ctx, shell);
                    return;
                case ToolWorkspace::Assets:
                    publishAssetPreview(ctx, shell);
                    return;
                case ToolWorkspace::World:
                    clearPreviewRender(ctx.world);
                    return;
            }
            clearPreviewRender(ctx.world);
        }

        static std::pair<uint32_t, uint32_t> previewImageSize(const EcsControllerContext& ctx,
                                                              UiHandle imageHandle)
        {
            uint32_t width = 320;
            uint32_t height = 240;
            if (ctx.ui == nullptr)
                return {width, height};

            const UiNode* node = ctx.ui->findNode(imageHandle);
            if (node == nullptr)
                return {width, height};

            width = std::max(1u,
                             static_cast<uint32_t>(
                                 std::round(node->computedLayout.bounds.width *
                                            std::max(1.0f, ctx.windowPixelWidth))));
            height = std::max(1u,
                              static_cast<uint32_t>(
                                  std::round(node->computedLayout.bounds.height *
                                             std::max(1.0f, ctx.windowPixelHeight))));
            return {width, height};
        }

        static const EngineToolInputCaptureComponent* currentInputCapture(ECSWorld& world)
        {
            if (!world.hasAny<EngineToolInputCaptureComponent>())
                return nullptr;
            return &world.getSingleton<EngineToolInputCaptureComponent>();
        }

        void publishAssetPreview(const EcsControllerContext& ctx, EngineToolShellComposition& shell)
        {
            if (!syncAssetPreviewWorld(ctx))
            {
                clearPreviewRender(ctx.world);
                lastAssetPreviewTexture = 0;
                return;
            }

            const auto [width, height] = previewImageSize(ctx, shell.assetPreviewImageHandle());

            texture_id_type previousTexture = 0;
            if (ctx.world.hasAny<EditorPreviewRenderComponent>())
                previousTexture = ctx.world.getSingleton<EditorPreviewRenderComponent>().data.colorTextureID;

            const float dt = ctx.time == nullptr ? 1.0f / 60.0f : ctx.time->unscaledDeltaTime;
            EditorPreviewRenderData data =
                assetPreviewWorld->buildFrame(dt, width, height, currentInputCapture(ctx.world));
            data.colorTextureID = previousTexture;

            EditorPreviewRenderComponent* component = nullptr;
            if (ctx.world.hasAny<EditorPreviewRenderComponent>())
                component = &ctx.world.getSingleton<EditorPreviewRenderComponent>();
            else
                component = &ctx.world.createSingleton<EditorPreviewRenderComponent>();

            component->data = std::move(data);
            lastAssetPreviewTexture = component->data.enabled ? component->data.colorTextureID : 0;
        }

        void publishParticlePreview(const EcsControllerContext& ctx, EngineToolShellComposition& shell)
        {
            if (!particleSession.hasAsset() || particleSession.path().empty() || !previewWorld)
            {
                clearPreviewRender(ctx.world);
                lastPreviewTexture = 0;
                return;
            }

            const auto [width, height] = previewImageSize(ctx, shell.particlePreviewImageHandle());

            texture_id_type previousTexture = 0;
            if (ctx.world.hasAny<EditorPreviewRenderComponent>())
                previousTexture = ctx.world.getSingleton<EditorPreviewRenderComponent>().data.colorTextureID;

            const float dt = ctx.time == nullptr ? 1.0f / 60.0f : ctx.time->unscaledDeltaTime;
            EditorPreviewRenderData data = previewWorld->buildFrame(particleSession.asset(),
                                                                     dt,
                                                                     particleSession.isPlaying(),
                                                                     particleSession.timeScale(),
                                                                     width,
                                                                     height,
                                                                     currentInputCapture(ctx.world));
            data.colorTextureID = previousTexture;

            EditorPreviewRenderComponent* component = nullptr;
            if (ctx.world.hasAny<EditorPreviewRenderComponent>())
                component = &ctx.world.getSingleton<EditorPreviewRenderComponent>();
            else
                component = &ctx.world.createSingleton<EditorPreviewRenderComponent>();

            component->data = std::move(data);
            lastPreviewTexture = component->data.colorTextureID;
        }

        static void clearPreviewRender(ECSWorld& world)
        {
            if (world.hasAny<EditorPreviewRenderComponent>())
                world.getSingleton<EditorPreviewRenderComponent>().data.enabled = false;
        }

        void updateInputCapture(ECSWorld& world, const EcsControllerContext& ctx)
        {
            EngineToolInputCaptureComponent& capture = ensureInputCapture(world);
            const UiDispatchResult& dispatch = ctx.ui == nullptr ? UiDispatchResult{} : ctx.ui->dispatchResult();

            capture.pointerOverToolUi = dispatch.hovered != UI_INVALID_HANDLE;
            capture.toolUiPressed = dispatch.pressed != UI_INVALID_HANDLE;
            capture.keyboardCaptured = dispatch.keyboardConsumed || dispatch.navigationConsumed || dispatch.textInputConsumed;
            capture.worldConsumed = false;
            capture.pointerX = dispatch.pointerX;
            capture.pointerY = dispatch.pointerY;
            capture.hoveredUi = dispatch.hovered;
            capture.pressedUi = dispatch.pressed;

            if (ctx.input == nullptr)
            {
                capture.primaryDown = false;
                capture.primaryPressed = false;
                capture.primaryReleased = false;
                capture.orbitDown = false;
                capture.orbitPressed = false;
                capture.orbitReleased = false;
                capture.panDown = false;
                capture.panPressed = false;
                capture.panReleased = false;
                capture.pointerOverViewport = false;
                capture.pointerPixelX = 0.0f;
                capture.pointerPixelY = 0.0f;
                capture.pointerDeltaX = 0.0f;
                capture.pointerDeltaY = 0.0f;
                capture.viewportPointerX = 0.0f;
                capture.viewportPointerY = 0.0f;
                capture.viewportPointerDeltaX = 0.0f;
                capture.viewportPointerDeltaY = 0.0f;
                capture.scrollX = 0.0f;
                capture.scrollY = 0.0f;
                inputPointerReady = false;
                return;
            }

            const double mouseX = ctx.input->mouseX();
            const double mouseY = ctx.input->mouseY();
            capture.pointerPixelX = static_cast<float>(mouseX);
            capture.pointerPixelY = static_cast<float>(mouseY);
            capture.pointerDeltaX = inputPointerReady ? static_cast<float>(mouseX - previousInputMouseX) : 0.0f;
            capture.pointerDeltaY = inputPointerReady ? static_cast<float>(mouseY - previousInputMouseY) : 0.0f;
            previousInputMouseX = mouseX;
            previousInputMouseY = mouseY;
            inputPointerReady = true;
            capture.scrollX = static_cast<float>(ctx.input->scrollX());
            capture.scrollY = static_cast<float>(ctx.input->scrollY());

            RenderViewportRect viewport =
                RenderViewportRect::full(std::max(1, static_cast<int>(std::round(ctx.windowPixelWidth))),
                                         std::max(1, static_cast<int>(std::round(ctx.windowPixelHeight))));
            if (world.hasAny<RenderViewportComponent>())
                viewport = world.getSingleton<RenderViewportComponent>().sceneViewport;

            capture.pointerOverViewport = viewport.remapPointer(static_cast<float>(mouseX),
                                                                static_cast<float>(mouseY),
                                                                capture.viewportPointerX,
                                                                capture.viewportPointerY);
            capture.viewportPointerDeltaX =
                viewport.width > 0 ? capture.pointerDeltaX / static_cast<float>(viewport.width) : 0.0f;
            capture.viewportPointerDeltaY =
                viewport.height > 0 ? capture.pointerDeltaY / static_cast<float>(viewport.height) : 0.0f;

            const char* primaryAction =
                ctx.input->isContextActive(ToolsInputContext) ? ToolsSelectAction : "engine.ui_primary";
            capture.primaryDown = ctx.input->isHeld(primaryAction);
            capture.primaryPressed = ctx.input->isPressed(primaryAction);
            capture.primaryReleased = ctx.input->isReleased(primaryAction);
            capture.orbitDown = ctx.input->isHeld(ToolsOrbitAction);
            capture.orbitPressed = ctx.input->isPressed(ToolsOrbitAction);
            capture.orbitReleased = ctx.input->isReleased(ToolsOrbitAction);
            capture.panDown = ctx.input->isHeld(ToolsPanAction);
            capture.panPressed = ctx.input->isPressed(ToolsPanAction);
            capture.panReleased = ctx.input->isReleased(ToolsPanAction);
        }
    };
} // namespace gts::tools
