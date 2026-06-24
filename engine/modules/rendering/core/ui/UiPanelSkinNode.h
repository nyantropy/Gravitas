#pragma once

#include "UiPanelSkin.h"
#include "UiSystem.h"

namespace gts::ui
{
    struct UiPanelSkinNodeHandles
    {
        UiHandle root = UI_INVALID_HANDLE;
        UiHandle solid = UI_INVALID_HANDLE;
        UiHandle image = UI_INVALID_HANDLE;
        UiHandle nineSlice = UI_INVALID_HANDLE;
        UiHandle content = UI_INVALID_HANDLE;
    };

    class UiPanelSkinNode
    {
    public:
        static UiPanelSkinNodeHandles build(UiSystem& ui,
                                            UiHandle parent,
                                            const UiLayoutSpec& layout,
                                            bool interactable = false)
        {
            UiPanelSkinNodeHandles handles;

            handles.root = ui.createNode(UiNodeType::Container, parent);
            ui.setLayout(handles.root, layout);
            setNodeState(ui, handles.root, true, interactable);

            handles.solid = ui.createNode(UiNodeType::Rect, handles.root);
            ui.setLayout(handles.solid, fullLayout());
            setNodeState(ui, handles.solid, false, false);

            handles.image = ui.createNode(UiNodeType::Image, handles.root);
            ui.setLayout(handles.image, fullLayout());
            setNodeState(ui, handles.image, false, false);

            handles.nineSlice = ui.createNode(UiNodeType::NineSlice, handles.root);
            ui.setLayout(handles.nineSlice, fullLayout());
            setNodeState(ui, handles.nineSlice, false, false);

            handles.content = ui.createNode(UiNodeType::Container, handles.root);
            ui.setLayout(handles.content, fullLayout());
            setNodeState(ui, handles.content, true, false);

            return handles;
        }

        static void destroy(UiSystem& ui, UiPanelSkinNodeHandles& handles)
        {
            if (handles.root != UI_INVALID_HANDLE)
                ui.removeNode(handles.root);

            handles = UiPanelSkinNodeHandles{};
        }

        static void setLayout(UiSystem& ui, const UiPanelSkinNodeHandles& handles, const UiLayoutSpec& layout)
        {
            ui.setLayout(handles.root, layout);
        }

        static void setState(UiSystem& ui,
                             const UiPanelSkinNodeHandles& handles,
                             bool visible,
                             bool interactable)
        {
            setNodeState(ui, handles.root, visible, interactable);
            setNodeState(ui, handles.content, visible, false);
        }

        static void applySkin(UiSystem& ui, const UiPanelSkinNodeHandles& handles, const UiPanelSkin& skin)
        {
            const bool showSolid = skin.type == UiPanelSkinType::SolidColor;
            const bool showImage = skin.type == UiPanelSkinType::Image && !skin.imageAsset.empty();
            const bool showNineSlice = skin.type == UiPanelSkinType::NineSlice && !skin.imageAsset.empty();

            setNodeState(ui, handles.solid, showSolid, false);
            setNodeState(ui, handles.image, showImage, false);
            setNodeState(ui, handles.nineSlice, showNineSlice, false);

            if (showSolid)
                ui.setPayload(handles.solid, UiRectData{skin.color});

            if (showImage)
            {
                UiImageData image;
                image.imageAsset = skin.imageAsset;
                image.tint = skin.tint;
                ui.setPayload(handles.image, image);
            }

            if (showNineSlice)
            {
                UiNineSliceData nineSlice;
                nineSlice.imageAsset = skin.imageAsset;
                nineSlice.tint = skin.tint;
                nineSlice.slice = skin.slice;
                ui.setPayload(handles.nineSlice, nineSlice);
            }

            UiLayoutSpec contentLayout = fullLayout();
            contentLayout.padding = skin.contentPadding;
            ui.setLayout(handles.content, contentLayout);
        }

        static void applyStateSkin(UiSystem& ui,
                                   const UiPanelSkinNodeHandles& handles,
                                   const UiPanelStateSkin& skin)
        {
            const UiNode* node = ui.findNode(handles.root);
            if (node == nullptr)
                return;

            applySkin(ui, handles, resolvePanelSkin(skin, node->state));
        }

    private:
        static UiLayoutSpec fullLayout()
        {
            UiLayoutSpec layout;
            layout.positionMode = UiPositionMode::Anchored;
            layout.widthMode = UiSizeMode::FromAnchors;
            layout.heightMode = UiSizeMode::FromAnchors;
            layout.anchorMin = {0.0f, 0.0f};
            layout.anchorMax = {1.0f, 1.0f};
            return layout;
        }

        static void setNodeState(UiSystem& ui, UiHandle handle, bool visible, bool interactable)
        {
            UiStateFlags state;
            if (const UiNode* node = ui.findNode(handle))
                state = node->state;

            state.visible = visible;
            state.enabled = visible;
            state.interactable = interactable;
            if (!visible)
            {
                state.hovered = false;
                state.focused = false;
                state.pressed = false;
            }
            ui.setState(handle, state);
        }
    };
} // namespace gts::ui
