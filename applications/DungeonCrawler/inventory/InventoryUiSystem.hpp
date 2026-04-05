#pragma once

#include <algorithm>
#include <array>
#include <cstddef>

#include "BitmapFont.h"
#include "BitmapFontLoader.h"
#include "ECSControllerSystem.hpp"
#include "GraphicsConstants.h"
#include "GtsDebugOverlay.h"
#include "SceneContext.h"
#include "UiNode.h"
#include "UiSystem.h"

#include "inventory/GoldComponent.h"
#include "inventory/InventoryComponent.h"
#include "components/PlayerComponent.h"

class InventoryUiSystem : public ECSControllerSystem
{
public:
    void update(ECSWorld& world, SceneContext& ctx) override
    {
        if (ctx.ui == nullptr)
            return;

        ensureUi(ctx);

        const InventoryComponent* inventory = findPlayerInventory(world);
        const GoldComponent* gold = findPlayerGold(world);
        if (inventory == nullptr || gold == nullptr)
            return;

        updateLayout(ctx, *inventory);
        updateGoldLabel(ctx, *gold);
        updateSlots(ctx, *inventory);
    }

private:
    static constexpr size_t MAX_UI_SLOTS = 8;
    static constexpr int GOLD_LABEL_HEIGHT_PX = 26;
    static constexpr int GOLD_LABEL_GAP_PX = 8;
    static constexpr int SLOT_SIZE_PX = 56;
    static constexpr int SLOT_GAP_PX = 10;
    static constexpr int SLOT_MARGIN_PX = 20;
    static constexpr int SLOT_ICON_PADDING_PX = 6;

    BitmapFont uiFont;
    UiHandle rootHandle = UI_INVALID_HANDLE;
    UiHandle goldTextHandle = UI_INVALID_HANDLE;
    std::array<UiHandle, MAX_UI_SLOTS> slotBackgrounds{};
    std::array<UiHandle, MAX_UI_SLOTS> slotIcons{};
    bool initialized = false;

    static float toNormalizedWidth(int pixels, int viewportWidth)
    {
        if (viewportWidth <= 0) return 0.0f;
        return static_cast<float>(pixels) / static_cast<float>(viewportWidth);
    }

    static float toNormalizedHeight(int pixels, int viewportHeight)
    {
        if (viewportHeight <= 0) return 0.0f;
        return static_cast<float>(pixels) / static_cast<float>(viewportHeight);
    }

    void ensureUi(SceneContext& ctx)
    {
        if (initialized)
            return;

        rootHandle = ctx.ui->createNode(UiNodeType::Container);
        ctx.ui->setState(rootHandle, UiStateFlags{.visible = true, .enabled = false, .interactable = false});
        uiFont = BitmapFontLoader::load(
            ctx.resources,
            GtsDebugOverlay::DEBUG_FONT_PATH,
            GtsDebugOverlay::ATLAS_W,  GtsDebugOverlay::ATLAS_H,
            GtsDebugOverlay::CELL_W,   GtsDebugOverlay::CELL_H,
            GtsDebugOverlay::ATLAS_COLS,
            "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ :",
            GtsDebugOverlay::LINE_HEIGHT,
            true);

        goldTextHandle = ctx.ui->createNode(UiNodeType::Text, rootHandle);
        ctx.ui->setState(goldTextHandle, UiStateFlags{.visible = true, .enabled = false, .interactable = false});
        ctx.ui->setTextFont(goldTextHandle, &uiFont);

        for (size_t i = 0; i < MAX_UI_SLOTS; ++i)
        {
            slotBackgrounds[i] = ctx.ui->createNode(UiNodeType::Rect, rootHandle);
            slotIcons[i] = ctx.ui->createNode(UiNodeType::Image, rootHandle);

            ctx.ui->setState(slotBackgrounds[i], UiStateFlags{.visible = true, .enabled = false, .interactable = false});
            ctx.ui->setState(slotIcons[i], UiStateFlags{.visible = false, .enabled = false, .interactable = false});
            ctx.ui->setPayload(slotBackgrounds[i], UiRectData{{0.08f, 0.09f, 0.12f, 0.88f}});
        }

        initialized = true;
    }

    const InventoryComponent* findPlayerInventory(ECSWorld& world) const
    {
        const InventoryComponent* inventory = nullptr;

        world.forEach<PlayerComponent, InventoryComponent>(
            [&](Entity, PlayerComponent&, InventoryComponent& candidate)
        {
            inventory = &candidate;
        });

        return inventory;
    }

    const GoldComponent* findPlayerGold(ECSWorld& world) const
    {
        const GoldComponent* gold = nullptr;

        world.forEach<PlayerComponent, GoldComponent>(
            [&](Entity, PlayerComponent&, GoldComponent& candidate)
        {
            gold = &candidate;
        });

        return gold;
    }

    void updateLayout(SceneContext& ctx, const InventoryComponent& inventory)
    {
        const int viewportWidth = std::max(1, ctx.windowPixelWidth);
        const int viewportHeight = std::max(1, ctx.windowPixelHeight);
        const size_t slotCount = std::min(inventory.maxSlots, MAX_UI_SLOTS);

        const int totalWidthPx = static_cast<int>(slotCount) * SLOT_SIZE_PX
            + static_cast<int>(slotCount > 0 ? slotCount - 1 : 0) * SLOT_GAP_PX;
        const int totalHeightPx = GOLD_LABEL_HEIGHT_PX + GOLD_LABEL_GAP_PX + SLOT_SIZE_PX;

        UiLayoutSpec rootLayout;
        rootLayout.positionMode = UiPositionMode::Absolute;
        rootLayout.widthMode = UiSizeMode::Fixed;
        rootLayout.heightMode = UiSizeMode::Fixed;
        rootLayout.offsetMin = {
            1.0f - toNormalizedWidth(totalWidthPx + SLOT_MARGIN_PX, viewportWidth),
            1.0f - toNormalizedHeight(totalHeightPx + SLOT_MARGIN_PX, viewportHeight)
        };
        rootLayout.fixedWidth = toNormalizedWidth(totalWidthPx, viewportWidth);
        rootLayout.fixedHeight = toNormalizedHeight(totalHeightPx, viewportHeight);
        ctx.ui->setLayout(rootHandle, rootLayout);

        UiLayoutSpec goldLayout;
        goldLayout.positionMode = UiPositionMode::Absolute;
        goldLayout.widthMode = UiSizeMode::Fixed;
        goldLayout.heightMode = UiSizeMode::Fixed;
        goldLayout.offsetMin = {0.0f, 0.0f};
        goldLayout.fixedWidth = toNormalizedWidth(totalWidthPx, viewportWidth);
        goldLayout.fixedHeight = toNormalizedHeight(GOLD_LABEL_HEIGHT_PX, viewportHeight);
        ctx.ui->setLayout(goldTextHandle, goldLayout);

        for (size_t i = 0; i < MAX_UI_SLOTS; ++i)
        {
            const bool active = i < slotCount;
            ctx.ui->setState(slotBackgrounds[i], UiStateFlags{.visible = active, .enabled = false, .interactable = false});

            UiLayoutSpec slotLayout;
            slotLayout.positionMode = UiPositionMode::Absolute;
            slotLayout.widthMode = UiSizeMode::Fixed;
            slotLayout.heightMode = UiSizeMode::Fixed;
            slotLayout.offsetMin = {
                toNormalizedWidth(static_cast<int>(i) * (SLOT_SIZE_PX + SLOT_GAP_PX), viewportWidth),
                toNormalizedHeight(GOLD_LABEL_HEIGHT_PX + GOLD_LABEL_GAP_PX, viewportHeight)
            };
            slotLayout.fixedWidth = toNormalizedWidth(SLOT_SIZE_PX, viewportWidth);
            slotLayout.fixedHeight = toNormalizedHeight(SLOT_SIZE_PX, viewportHeight);
            ctx.ui->setLayout(slotBackgrounds[i], slotLayout);

            UiLayoutSpec iconLayout;
            iconLayout.positionMode = UiPositionMode::Absolute;
            iconLayout.widthMode = UiSizeMode::Fixed;
            iconLayout.heightMode = UiSizeMode::Fixed;
            iconLayout.offsetMin = {
                toNormalizedWidth(static_cast<int>(i) * (SLOT_SIZE_PX + SLOT_GAP_PX) + SLOT_ICON_PADDING_PX, viewportWidth),
                toNormalizedHeight(GOLD_LABEL_HEIGHT_PX + GOLD_LABEL_GAP_PX + SLOT_ICON_PADDING_PX, viewportHeight)
            };
            iconLayout.fixedWidth = toNormalizedWidth(SLOT_SIZE_PX - SLOT_ICON_PADDING_PX * 2, viewportWidth);
            iconLayout.fixedHeight = toNormalizedHeight(SLOT_SIZE_PX - SLOT_ICON_PADDING_PX * 2, viewportHeight);
            ctx.ui->setLayout(slotIcons[i], iconLayout);
        }
    }

    void updateGoldLabel(SceneContext& ctx, const GoldComponent& gold)
    {
        ctx.ui->setPayload(goldTextHandle, UiTextData{
            "GOLD: " + std::to_string(gold.amount),
            {},
            {1.0f, 0.88f, 0.22f, 1.0f},
            0.020f
        });
    }

    void updateSlots(SceneContext& ctx, const InventoryComponent& inventory)
    {
        const size_t slotCount = std::min(inventory.maxSlots, MAX_UI_SLOTS);

        for (size_t i = 0; i < MAX_UI_SLOTS; ++i)
        {
            const bool hasItem = i < inventory.items.size() && i < slotCount;
            ctx.ui->setState(slotIcons[i], UiStateFlags{.visible = hasItem, .enabled = false, .interactable = false});

            if (!hasItem)
                continue;

            ctx.ui->setPayload(slotIcons[i], UiImageData{
                inventory.items[i].iconPath,
                {1.0f, 1.0f, 1.0f, 1.0f},
                1.0f
            });
        }
    }
};
