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

#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "EditorPreviewRenderData.h"
#include "EngineToolInputCaptureComponent.h"
#include "EngineToolShellComposition.h"
#include "EngineToolStateComponent.h"
#include "EngineToolUiHelpers.h"
#include "EngineToolWorkspaceComponent.h"
#include "GraphicsConstants.h"
#include "InputBindingRegistry.h"
#include "ParticleEditorSession.h"
#include "ParticleEffectAsset.h"
#include "ParticleEmitterComponent.h"
#include "ParticleModuleAuthoring.h"
#include "ParticlePreviewWorld.hpp"
#include "RegisteredSceneInfo.h"
#include "RenderViewportComponent.h"
#include "TimeContext.h"
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
            const EngineToolWorkspaceComponent& workspace = publishWorkspace(ctx, true);

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

            processCommands(ctx, state, *shell);
            syncParticlePreviewWorld(ctx);

            ToolShellView view = buildView(ctx, state, workspace);
            shell->setView(view);
            ctx.ui->updateComposition(shellComposition);

            publishParticlePreview(ctx, *shell);
            if (view.previewTexture != lastPreviewTexture)
            {
                view.previewTexture = lastPreviewTexture;
                shell->setView(view);
                ctx.ui->updateComposition(shellComposition);
            }

            updateInputCapture(ctx.world, ctx);
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
        }

    private:
        static constexpr const char* ToolsInputContext = "engine.tools";
        static constexpr const char* ToolsSelectAction = "engine.tools_select";

        static constexpr size_t ScenePageSize = 10;
        static constexpr size_t EffectPageSize = 6;
        static constexpr size_t EmitterPageSize = 5;
        static constexpr size_t ModulePageSize = 4;

        BitmapFont font;
        bool fontReady = false;
        bool toolsContextRequested = false;
        uint64_t lastVisibilityToggleFrame = std::numeric_limits<uint64_t>::max();
        UiCompositionId shellComposition = UI_INVALID_COMPOSITION;
        ToolWorkspace activeWorkspace = ToolWorkspace::Particles;

        std::vector<std::string> effectPaths;
        size_t sceneOffset = 0;
        size_t effectOffset = 0;
        texture_id_type lastPreviewTexture = 0;

        ParticleEditorSession particleSession;
        std::unique_ptr<ParticlePreviewWorld> previewWorld = std::make_unique<ParticlePreviewWorld>();

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
            clearPreviewRender(ctx.world);
            if (previewWorld)
                previewWorld->destroy();
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

        static EngineToolWorkspaceComponent& publishWorkspace(const EcsControllerContext& ctx, bool active)
        {
            const int width = std::max(1, static_cast<int>(std::round(ctx.windowPixelWidth)));
            const int height = std::max(1, static_cast<int>(std::round(ctx.windowPixelHeight)));
            return publishEngineToolWorkspace(ctx.world, width, height, active);
        }

        bool ensureFont(const EcsControllerContext& ctx)
        {
            if (fontReady)
                return true;
            if (ctx.resources == nullptr)
                return false;

            const font_id_type fontID =
                ctx.resources->requestFont(GraphicsConstants::ENGINE_RESOURCES + "/fonts/gravitasfont.font.json");
            const BitmapFont* loadedFont = ctx.resources->getFont(fontID);
            if (loadedFont == nullptr)
                return false;

            font = *loadedFont;
            tuneEditorFont(font);
            fontReady = true;
            return true;
        }

        static void tuneEditorFont(BitmapFont& inFont)
        {
            inFont.lineHeight = 0.92f;
            for (auto& glyphEntry : inFont.glyphs)
            {
                GlyphInfo& glyph = glyphEntry.second;
                glyph.advance = 0.62f;
                glyph.size.x = 0.82f;
                glyph.size.y = 0.92f;

                const char ch = glyphEntry.first;
                if (ch == ' ')
                    glyph.advance = 0.34f;
                else if (ch == 'i' || ch == 'l' || ch == 'I' || ch == '!' || ch == '.' || ch == ',' || ch == ':' ||
                         ch == ';' || ch == '\'' || ch == '`')
                    glyph.advance = 0.38f;
                else if (ch == 'm' || ch == 'w' || ch == 'M' || ch == 'W')
                    glyph.advance = 0.76f;
                else if (ch >= 'A' && ch <= 'Z')
                    glyph.advance = 0.66f;
            }
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
                        state.activePanelIndex = activeWorkspace == ToolWorkspace::Particles ? 1u : 0u;
                        state.status = activeWorkspace == ToolWorkspace::Particles ? "PARTICLE EDITOR" : "WORLD VIEWER";
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

                    case ToolCommandType::ToggleEmitterEnabled:
                        if (particleSession.toggleSelectedEmitter(state.status))
                            resetParticlePreview();
                        break;

                    case ToolCommandType::ToggleModuleEnabled:
                        if (particleSession.toggleSelectedModule(state.status))
                            resetParticlePreview();
                        break;

                    case ToolCommandType::SelectEmitter:
                        particleSession.selectEmitter(command.index);
                        break;

                    case ToolCommandType::SelectModule:
                        particleSession.selectModule(command.index);
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
            view.previewTexture = lastPreviewTexture;
            view.particleLoaded = particleSession.hasAsset();
            view.particleDirty = particleSession.isDirty();
            view.particlePlaying = particleSession.isPlaying();
            view.particlePath = particleSession.path();
            view.particleTitle = particleSession.title();

            if (const ParticleEffectEmitter* emitter = particleSession.selectedEmitter())
            {
                view.hasSelectedEmitter = true;
                view.selectedEmitterEnabled = emitter->descriptor.enabled;
                if (particleSession.selectedModule() != nullptr)
                {
                    view.hasSelectedModule = true;
                    view.selectedModuleEnabled = particleSession.selectedModule()->enabled;
                }
            }

            fillSceneRows(ctx, view);
            fillEffectRows(view);
            fillParticleRows(view);
            fillInspectorLines(view);
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
                row.label = (row.active ? "ACTIVE  " : "LOAD    ") + toolui::compact(display, 30);
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
                row.label = (row.active ? "OPEN    " : "LOAD    ") +
                    toolui::compact(toolui::stemName(row.path), 30);
                view.effects.push_back(std::move(row));
            }
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

        void fillInspectorLines(ToolShellView& view) const
        {
            if (activeWorkspace == ToolWorkspace::World)
            {
                view.inspectorLines.push_back("Active scene: " +
                                              (view.activeSceneName.empty() ? std::string("none") : view.activeSceneName));
                view.inspectorLines.push_back("Registered scenes: " + std::to_string(view.sceneTotal));
                view.inspectorLines.push_back("Scene changes are requested through engine scene commands.");
                view.inspectorLines.push_back("The center viewport is the published runtime scene viewport.");
                return;
            }

            if (!particleSession.hasAsset())
            {
                view.inspectorLines.push_back("Select a particle asset from the effect hierarchy.");
                view.inspectorLines.push_back("The particle preview renders into its own viewport pane.");
                return;
            }

            const ParticleEffectAsset& asset = particleSession.asset();
            view.inspectorLines.push_back("Effect: " + particleSession.title());
            view.inspectorLines.push_back("Emitters: " + std::to_string(asset.emitters.size()));

            const ParticleEffectEmitter* emitter = particleSession.selectedEmitter();
            if (emitter == nullptr)
                return;

            view.inspectorLines.push_back("Emitter: " + (emitter->name.empty() ? emitter->stableId : emitter->name));
            view.inspectorLines.push_back("Max particles: " + std::to_string(emitter->descriptor.maxParticles));
            view.inspectorLines.push_back("Emission rate: " + toolui::fixed(emitter->descriptor.emissionRate));
            view.inspectorLines.push_back("Lifetime: " + toolui::fixed(emitter->descriptor.lifetimeMin) + "-" +
                                          toolui::fixed(emitter->descriptor.lifetimeMax));

            if (const gts::particles::ParticleModuleInstance* module = particleSession.selectedModule())
            {
                view.inspectorLines.push_back("Module: " +
                                              (module->displayName.empty() ? module->typeId : module->displayName));
                for (size_t i = 0; i < std::min<size_t>(module->parameters.size(), 3); ++i)
                    view.inspectorLines.push_back(parameterSummary(module->parameters[i]));
            }
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

        static std::string parameterSummary(const gts::particles::ParticleModuleParameter& parameter)
        {
            using Type = gts::particles::ParticleModuleParameterType;
            std::string value;
            switch (parameter.type)
            {
                case Type::Float:
                    value = toolui::fixed(parameter.floatValue);
                    break;
                case Type::UInt:
                case Type::Enum:
                    value = std::to_string(parameter.uintValue);
                    break;
                case Type::Bool:
                    value = parameter.boolValue ? "true" : "false";
                    break;
                case Type::String:
                    value = parameter.stringValue.empty() ? "<empty>" : toolui::compact(parameter.stringValue, 22);
                    break;
                case Type::FloatCurve:
                    value = std::to_string(parameter.floatCurveValue.size()) + " keys";
                    break;
                case Type::ColorGradient:
                    value = std::to_string(parameter.colorGradientValue.size()) + " colors";
                    break;
                case Type::BurstTimeline:
                    value = std::to_string(parameter.burstTimelineValue.size()) + " bursts";
                    break;
            }
            return toolui::compact(parameter.id, 18) + ": " + value;
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

        void publishParticlePreview(const EcsControllerContext& ctx, EngineToolShellComposition& shell)
        {
            if (!particleSession.hasAsset() || particleSession.path().empty() || !previewWorld)
            {
                clearPreviewRender(ctx.world);
                lastPreviewTexture = 0;
                return;
            }

            uint32_t width = 320;
            uint32_t height = 240;
            if (ctx.ui != nullptr)
            {
                const UiNode* node = ctx.ui->findNode(shell.particlePreviewImageHandle());
                if (node != nullptr)
                {
                    width = std::max(1u,
                                     static_cast<uint32_t>(
                                         std::round(node->computedLayout.bounds.width *
                                                    std::max(1.0f, ctx.windowPixelWidth))));
                    height = std::max(1u,
                                      static_cast<uint32_t>(
                                          std::round(node->computedLayout.bounds.height *
                                                     std::max(1.0f, ctx.windowPixelHeight))));
                }
            }

            texture_id_type previousTexture = 0;
            if (ctx.world.hasAny<EditorPreviewRenderComponent>())
                previousTexture = ctx.world.getSingleton<EditorPreviewRenderComponent>().data.colorTextureID;

            const float dt = ctx.time == nullptr ? 1.0f / 60.0f : ctx.time->unscaledDeltaTime;
            EditorPreviewRenderData data = previewWorld->buildFrame(particleSession.asset(),
                                                                     dt,
                                                                     particleSession.isPlaying(),
                                                                     particleSession.timeScale(),
                                                                     width,
                                                                     height);
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
                capture.pointerOverViewport = false;
                capture.viewportPointerX = 0.0f;
                capture.viewportPointerY = 0.0f;
                return;
            }

            RenderViewportRect viewport =
                RenderViewportRect::full(std::max(1, static_cast<int>(std::round(ctx.windowPixelWidth))),
                                         std::max(1, static_cast<int>(std::round(ctx.windowPixelHeight))));
            if (world.hasAny<RenderViewportComponent>())
                viewport = world.getSingleton<RenderViewportComponent>().sceneViewport;

            capture.pointerOverViewport = viewport.remapPointer(static_cast<float>(ctx.input->mouseX()),
                                                                static_cast<float>(ctx.input->mouseY()),
                                                                capture.viewportPointerX,
                                                                capture.viewportPointerY);

            const char* primaryAction =
                ctx.input->isContextActive(ToolsInputContext) ? ToolsSelectAction : "engine.ui_primary";
            capture.primaryDown = ctx.input->isHeld(primaryAction);
            capture.primaryPressed = ctx.input->isPressed(primaryAction);
            capture.primaryReleased = ctx.input->isReleased(primaryAction);
        }
    };
} // namespace gts::tools
