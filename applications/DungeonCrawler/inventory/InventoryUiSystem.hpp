#pragma once

#include <algorithm>
#include <array>
#include <cstddef>

#include "ECSControllerSystem.hpp"
#include "SceneContext.h"
#include "UiSystem.h"
#include "UiNode.h"

#include "InventoryComponent.h"
#include "PlayerComponent.h"

class InventoryUiSystem : public ECSControllerSystem
{
public:
    void update(ECSWorld& world, SceneContext& ctx) override
    {
        if (ctx.ui == nullptr)
            return;

        ensureUi(ctx);

        const InventoryComponent* inventory = findPlayerInventory(world);
        if (inventory == nullptr)
            return;

        updateLayout(ctx, *inventory);
        updateSlots(ctx, *inventory);
    }

private:
    static constexpr size_t MAX_UI_SLOTS = 8;
    static constexpr int SLOT_SIZE_PX = 56;
    static constexpr int SLOT_GAP_PX = 10;
    static constexpr int SLOT_MARGIN_PX = 20;
    static constexpr int SLOT_ICON_PADDING_PX = 6;

    UiHandle rootHandle = UI_INVALID_HANDLE;
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

    void updateLayout(SceneContext& ctx, const InventoryComponent& inventory)
    {
        const int viewportWidth = std::max(1, ctx.windowPixelWidth);
        const int viewportHeight = std::max(1, ctx.windowPixelHeight);
        const size_t slotCount = std::min(inventory.maxSlots, MAX_UI_SLOTS);

        const int totalWidthPx = static_cast<int>(slotCount) * SLOT_SIZE_PX
            + static_cast<int>(slotCount > 0 ? slotCount - 1 : 0) * SLOT_GAP_PX;

        UiLayoutSpec rootLayout;
        rootLayout.positionMode = UiPositionMode::Absolute;
        rootLayout.widthMode = UiSizeMode::Fixed;
        rootLayout.heightMode = UiSizeMode::Fixed;
        rootLayout.offsetMin = {
            1.0f - toNormalizedWidth(totalWidthPx + SLOT_MARGIN_PX, viewportWidth),
            1.0f - toNormalizedHeight(SLOT_SIZE_PX + SLOT_MARGIN_PX, viewportHeight)
        };
        rootLayout.fixedWidth = toNormalizedWidth(totalWidthPx, viewportWidth);
        rootLayout.fixedHeight = toNormalizedHeight(SLOT_SIZE_PX, viewportHeight);
        ctx.ui->setLayout(rootHandle, rootLayout);

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
                0.0f
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
                toNormalizedHeight(SLOT_ICON_PADDING_PX, viewportHeight)
            };
            iconLayout.fixedWidth = toNormalizedWidth(SLOT_SIZE_PX - SLOT_ICON_PADDING_PX * 2, viewportWidth);
            iconLayout.fixedHeight = toNormalizedHeight(SLOT_SIZE_PX - SLOT_ICON_PADDING_PX * 2, viewportHeight);
            ctx.ui->setLayout(slotIcons[i], iconLayout);
        }
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
