#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <utility>

#include "AssetBrowserSession.h"
#include "AssetPreviewWorld.hpp"
#include "ECSWorld.hpp"
#include "EcsControllerContext.hpp"
#include "EditorPreviewRenderData.h"
#include "EngineToolInputCaptureSystem.h"
#include "ParticleEditorSession.h"
#include "ParticlePreviewWorld.hpp"
#include "ToolWorkspace.h"
#include "Types.h"
#include "UiHandle.h"
#include "UiNode.h"
#include "UiSystem.h"

namespace gts::tools
{
    class EngineToolPreviewCoordinator
    {
    public:
        void destroy()
        {
            if (particlePreviewWorld)
                particlePreviewWorld->destroy();
            if (assetPreviewWorld)
                assetPreviewWorld->destroy();
        }

        void clear(ECSWorld& world)
        {
            clearPreviewRender(world);
            particlePreviewTexture = 0;
            assetPreviewTexture = 0;
        }

        void resetParticlePreview()
        {
            if (particlePreviewWorld)
                particlePreviewWorld->resetSimulation();
        }

        void resetAssetPreviewTexture()
        {
            assetPreviewTexture = 0;
        }

        texture_id_type particleTexture() const
        {
            return particlePreviewTexture;
        }

        texture_id_type assetTexture() const
        {
            return assetPreviewTexture;
        }

        void syncParticle(const EcsControllerContext& ctx, const ParticleEditorSession& particleSession)
        {
            if (!particleSession.hasAsset() || particleSession.path().empty() || !particlePreviewWorld)
                return;

            particlePreviewWorld->ensure(ctx.resources);
            particlePreviewWorld->syncAsset(particleSession.path(),
                                            particleSession.asset(),
                                            particleSession.timeScale(),
                                            particleSession.isPaused());
        }

        void publish(const EcsControllerContext& ctx,
                     ToolWorkspace activeWorkspace,
                     UiHandle particlePreviewImage,
                     UiHandle assetPreviewImage,
                     const ParticleEditorSession& particleSession,
                     const AssetBrowserSession& assetSession)
        {
            switch (activeWorkspace)
            {
                case ToolWorkspace::Particles:
                    publishParticle(ctx, particlePreviewImage, particleSession);
                    return;
                case ToolWorkspace::Assets:
                    publishAsset(ctx, assetPreviewImage, assetSession);
                    return;
                case ToolWorkspace::World:
                    clearPreviewRender(ctx.world);
                    return;
            }
            clearPreviewRender(ctx.world);
        }

    private:
        std::unique_ptr<ParticlePreviewWorld> particlePreviewWorld = std::make_unique<ParticlePreviewWorld>();
        std::unique_ptr<AssetPreviewWorld> assetPreviewWorld = std::make_unique<AssetPreviewWorld>();
        texture_id_type particlePreviewTexture = 0;
        texture_id_type assetPreviewTexture = 0;

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

        static void clearPreviewRender(ECSWorld& world)
        {
            if (world.hasAny<EditorPreviewRenderComponent>())
                world.getSingleton<EditorPreviewRenderComponent>().data.enabled = false;
        }

        bool syncAsset(const EcsControllerContext& ctx, const AssetBrowserSession& assetSession)
        {
            const AssetBrowserEntry* asset = assetSession.selected();
            if (asset == nullptr || !asset->valid || !assetPreviewWorld || ctx.resources == nullptr)
                return false;

            assetPreviewWorld->ensure(ctx.resources);
            assetPreviewWorld->syncAsset(asset->manifestPath, asset->manifest);
            return true;
        }

        void publishAsset(const EcsControllerContext& ctx,
                          UiHandle assetPreviewImage,
                          const AssetBrowserSession& assetSession)
        {
            if (!syncAsset(ctx, assetSession))
            {
                clearPreviewRender(ctx.world);
                assetPreviewTexture = 0;
                return;
            }

            const auto [width, height] = previewImageSize(ctx, assetPreviewImage);

            texture_id_type previousTexture = 0;
            if (ctx.world.hasAny<EditorPreviewRenderComponent>())
                previousTexture = ctx.world.getSingleton<EditorPreviewRenderComponent>().data.colorTextureID;

            const float dt = ctx.time == nullptr ? 1.0f / 60.0f : ctx.time->unscaledDeltaTime;
            EditorPreviewRenderData data =
                assetPreviewWorld->buildFrame(dt, width, height, EngineToolInputCaptureSystem::current(ctx.world));
            data.colorTextureID = previousTexture;

            EditorPreviewRenderComponent* component = nullptr;
            if (ctx.world.hasAny<EditorPreviewRenderComponent>())
                component = &ctx.world.getSingleton<EditorPreviewRenderComponent>();
            else
                component = &ctx.world.createSingleton<EditorPreviewRenderComponent>();

            component->data = std::move(data);
            assetPreviewTexture = component->data.enabled ? component->data.colorTextureID : 0;
        }

        void publishParticle(const EcsControllerContext& ctx,
                             UiHandle particlePreviewImage,
                             const ParticleEditorSession& particleSession)
        {
            if (!particleSession.hasAsset() || particleSession.path().empty() || !particlePreviewWorld)
            {
                clearPreviewRender(ctx.world);
                particlePreviewTexture = 0;
                return;
            }

            const auto [width, height] = previewImageSize(ctx, particlePreviewImage);

            texture_id_type previousTexture = 0;
            if (ctx.world.hasAny<EditorPreviewRenderComponent>())
                previousTexture = ctx.world.getSingleton<EditorPreviewRenderComponent>().data.colorTextureID;

            const float dt = ctx.time == nullptr ? 1.0f / 60.0f : ctx.time->unscaledDeltaTime;
            EditorPreviewRenderData data = particlePreviewWorld->buildFrame(particleSession.asset(),
                                                                            dt,
                                                                            particleSession.isPlaying(),
                                                                            particleSession.timeScale(),
                                                                            width,
                                                                            height,
                                                                            EngineToolInputCaptureSystem::current(ctx.world));
            data.colorTextureID = previousTexture;

            EditorPreviewRenderComponent* component = nullptr;
            if (ctx.world.hasAny<EditorPreviewRenderComponent>())
                component = &ctx.world.getSingleton<EditorPreviewRenderComponent>();
            else
                component = &ctx.world.createSingleton<EditorPreviewRenderComponent>();

            component->data = std::move(data);
            particlePreviewTexture = component->data.colorTextureID;
        }
    };
}
