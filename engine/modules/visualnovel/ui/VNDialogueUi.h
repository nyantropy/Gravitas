#pragma once

#include <algorithm>
#include <string>
#include <variant>
#include <vector>

#include "BitmapFont.h"
#include "UiEvent.h"
#include "UiInteraction.h"
#include "UiNode.h"
#include "UiPanelSkinNode.h"
#include "UiSystem.h"
#include "VNPresentationProfile.h"
#include "VNRuntime.h"

namespace gts::vn
{
    struct VNDialogueUiConfig
    {
        BitmapFont* font = nullptr;
        UiColor dimColor = {0.0f, 0.0f, 0.0f, 1.0f};
        VNPresentationProfile profile = defaultVNPresentationProfile();
    };

    struct VNDialogueUiHandles
    {
        UiHandle root = UI_INVALID_HANDLE;
        UiHandle backgroundColor = UI_INVALID_HANDLE;
        UiHandle backgroundImage = UI_INVALID_HANDLE;
        UiHandle dimming = UI_INVALID_HANDLE;
        UiHandle spriteLayer = UI_INVALID_HANDLE;
        std::vector<UiHandle> spriteImages;
        UiHandle dialogueLayer = UI_INVALID_HANDLE;
        gts::ui::UiPanelSkinNodeHandles panelShadow;
        gts::ui::UiPanelSkinNodeHandles panel;
        gts::ui::UiPanelSkinNodeHandles nameplate;
        UiHandle speakerText = UI_INVALID_HANDLE;
        UiHandle bodyText = UI_INVALID_HANDLE;
        UiHandle continueIndicator = UI_INVALID_HANDLE;
        UiHandle choiceLayer = UI_INVALID_HANDLE;
        std::vector<gts::ui::UiPanelSkinNodeHandles> choiceButtons;
        std::vector<UiHandle> choiceLabels;
    };

    class VNDialogueUi
    {
    public:
        static VNDialogueUiHandles build(UiSystem& ui, const VNDialogueUiConfig& config)
        {
            return build(ui, UI_INVALID_HANDLE, config);
        }

        static VNDialogueUiHandles build(UiSystem& ui, UiHandle parent, const VNDialogueUiConfig& config)
        {
            VNDialogueUiHandles handles;
            const VNPresentationProfile& profile = config.profile;

            handles.root = ui.createNode(UiNodeType::Container, parent);
            ui.setLayout(handles.root, anchored(0.0f, 0.0f, 1.0f, 1.0f));
            setState(ui, handles.root, false, false);

            handles.backgroundColor = ui.createNode(UiNodeType::Rect, handles.root);
            ui.setLayout(handles.backgroundColor, anchored(0.0f, 0.0f, 1.0f, 1.0f));
            setState(ui, handles.backgroundColor, false, false);

            handles.backgroundImage = ui.createNode(UiNodeType::Image, handles.root);
            ui.setLayout(handles.backgroundImage, anchored(0.0f, 0.0f, 1.0f, 1.0f));
            setState(ui, handles.backgroundImage, false, false);

            handles.dimming = ui.createNode(UiNodeType::Rect, handles.root);
            ui.setLayout(handles.dimming, anchored(0.0f, 0.0f, 1.0f, 1.0f));
            setState(ui, handles.dimming, false, false);

            handles.spriteLayer = ui.createNode(UiNodeType::Container, handles.root);
            ui.setLayout(handles.spriteLayer, anchored(0.0f, 0.0f, 1.0f, 1.0f));
            setState(ui, handles.spriteLayer, true, false);

            handles.spriteImages.reserve(profile.layout.maxSprites);
            for (size_t i = 0; i < profile.layout.maxSprites; ++i)
            {
                UiHandle image = ui.createNode(UiNodeType::Image, handles.spriteLayer);
                ui.setLayout(image, absolute(0.0f, 0.0f, 0.0f, 0.0f));
                setState(ui, image, false, false);
                handles.spriteImages.push_back(image);
            }

            handles.dialogueLayer = ui.createNode(UiNodeType::Container, handles.root);
            ui.setLayout(handles.dialogueLayer, rectToAnchored(profile.layout.dialogue));
            setState(ui, handles.dialogueLayer, false, false);

            UiRect shadowRect = {0.010f, 0.030f, 1.0f, 1.0f};
            handles.panelShadow = gts::ui::UiPanelSkinNode::build(
                ui, handles.dialogueLayer, rectToAnchored(shadowRect));
            gts::ui::UiPanelSkinNode::applySkin(ui, handles.panelShadow, profile.dialogueSkin.panelShadow);

            handles.panel = gts::ui::UiPanelSkinNode::build(
                ui, handles.dialogueLayer, anchored(0.0f, 0.0f, 1.0f, 1.0f));
            gts::ui::UiPanelSkinNode::applySkin(ui, handles.panel, profile.dialogueSkin.panel);

            handles.nameplate = gts::ui::UiPanelSkinNode::build(
                ui, handles.dialogueLayer, rectToAnchored(profile.layout.nameplate));
            gts::ui::UiPanelSkinNode::applySkin(ui, handles.nameplate, profile.dialogueSkin.nameplate);

            handles.speakerText = ui.createNode(UiNodeType::Text, handles.nameplate.content);
            ui.setLayout(handles.speakerText, rectToAnchored(profile.layout.speakerText));
            setTextNode(ui,
                        handles.speakerText,
                        config.font,
                        "",
                        profile.dialogueSkin.speakerText,
                        profile.dialogueSkin.speakerTextScale);
            setTextAlignment(ui, handles.speakerText, UiHorizontalAlign::Left, UiVerticalAlign::Middle);

            handles.bodyText = ui.createNode(UiNodeType::Text, handles.dialogueLayer);
            UiLayoutSpec bodyLayout = rectToAnchored(profile.layout.bodyText);
            bodyLayout.margin = profile.dialogueSkin.textPadding;
            ui.setLayout(handles.bodyText, bodyLayout);
            setTextNode(ui,
                        handles.bodyText,
                        config.font,
                        "",
                        profile.dialogueSkin.bodyText,
                        profile.dialogueSkin.bodyTextScale);
            setTextWrap(ui, handles.bodyText, 4);

            handles.continueIndicator = ui.createNode(UiNodeType::Text, handles.dialogueLayer);
            ui.setLayout(handles.continueIndicator, rectToAnchored(profile.layout.continueIndicator));
            setTextNode(ui,
                        handles.continueIndicator,
                        config.font,
                        ">",
                        profile.dialogueSkin.continueIndicator,
                        profile.dialogueSkin.bodyTextScale);
            setTextAlignment(ui, handles.continueIndicator, UiHorizontalAlign::Right, UiVerticalAlign::Middle);

            handles.choiceLayer = ui.createNode(UiNodeType::Container, handles.root);
            UiLayoutSpec choiceLayerLayout = rectToAnchored(profile.layout.choices);
            choiceLayerLayout.layoutMode = UiLayoutMode::Stack;
            choiceLayerLayout.stackAxis = UiLayoutAxis::Vertical;
            choiceLayerLayout.crossAxisAlignment = UiLayoutAlignment::Stretch;
            const float choiceRowHeight = profile.layout.choices.height /
                static_cast<float>(std::max<size_t>(1, profile.layout.maxChoices));
            const float choiceGap = std::min(profile.layout.choiceRowGap * profile.layout.choices.height,
                                             choiceRowHeight * 0.40f);
            choiceLayerLayout.gap = choiceGap;
            ui.setLayout(handles.choiceLayer, choiceLayerLayout);
            setState(ui, handles.choiceLayer, false, false);

            handles.choiceButtons.reserve(profile.layout.maxChoices);
            handles.choiceLabels.reserve(profile.layout.maxChoices);
            for (size_t i = 0; i < profile.layout.maxChoices; ++i)
            {
                UiLayoutSpec choiceLayout;
                choiceLayout.constraints.preferredHeight =
                    {UiLayoutUnit::Normalized, std::max(0.0f, choiceRowHeight - choiceGap)};

                gts::ui::UiPanelSkinNodeHandles choice = gts::ui::UiPanelSkinNode::build(
                    ui,
                    handles.choiceLayer,
                    choiceLayout,
                    true);
                gts::ui::UiPanelSkinNode::setState(ui, choice, false, true);
                gts::ui::UiPanelSkinNode::applyStateSkin(ui, choice, profile.dialogueSkin.choice);

                UiHandle label = ui.createNode(UiNodeType::Text, choice.content);
                ui.setLayout(label, anchored(0.045f, 0.0f, 0.910f, 1.0f));
                setTextNode(ui,
                            label,
                            config.font,
                            "",
                            profile.dialogueSkin.choiceText,
                            profile.dialogueSkin.choiceTextScale);
                setTextAlignment(ui, label, UiHorizontalAlign::Left, UiVerticalAlign::Middle);

                handles.choiceButtons.push_back(choice);
                handles.choiceLabels.push_back(label);
            }

            return handles;
        }

        static void destroy(UiSystem& ui, VNDialogueUiHandles& handles)
        {
            if (handles.root != UI_INVALID_HANDLE)
                ui.removeNode(handles.root);

            handles = VNDialogueUiHandles{};
        }

        static void sync(UiSystem& ui,
                         const VNDialogueUiHandles& handles,
                         const VNDialogueUiConfig& config,
                         const VNRuntime& runtime)
        {
            if (handles.root == UI_INVALID_HANDLE)
                return;

            const bool active = runtime.isActive();
            setState(ui, handles.root, active, false);
            if (!active)
                return;

            syncBackground(ui, handles, runtime.getStage());
            syncDimming(ui, handles, config, runtime.getStage());
            syncSprites(ui, handles, runtime.getStage());
            syncDialogue(ui, handles, config, runtime.getDialogue());
        }

        static int choiceIndexFromInteraction(const VNDialogueUiHandles& handles,
                                              const UiInteractionResult& interaction)
        {
            if (interaction.clicked == UI_INVALID_HANDLE)
                return -1;

            for (size_t i = 0; i < handles.choiceButtons.size(); ++i)
            {
                if (interaction.clicked == handles.choiceButtons[i].root
                    || interaction.clicked == handles.choiceLabels[i])
                {
                    return static_cast<int>(i);
                }
            }

            return -1;
        }

        static int choiceIndexFromEvent(const VNDialogueUiHandles& handles, const UiEvent& event)
        {
            if (event.type != UiEventType::PointerClick)
                return -1;

            for (size_t i = 0; i < handles.choiceButtons.size(); ++i)
            {
                if (eventTargetsHandle(event, handles.choiceButtons[i].root)
                    || eventTargetsHandle(event, handles.choiceLabels[i]))
                {
                    return static_cast<int>(i);
                }
            }

            return -1;
        }

    private:
        static bool eventTargetsHandle(const UiEvent& event, UiHandle handle)
        {
            if (handle == UI_INVALID_HANDLE)
                return false;
            if (event.target == handle)
                return true;

            return std::find(event.targetPath.begin(), event.targetPath.end(), handle) != event.targetPath.end();
        }

        static UiLayoutSpec anchored(float x, float y, float width, float height)
        {
            UiLayoutSpec layout;
            layout.positionMode = UiPositionMode::Anchored;
            layout.widthMode = UiSizeMode::FromAnchors;
            layout.heightMode = UiSizeMode::FromAnchors;
            layout.anchorMin = {x, y};
            layout.anchorMax = {x + width, y + height};
            return layout;
        }

        static UiLayoutSpec rectToAnchored(const UiRect& rect)
        {
            return anchored(rect.x, rect.y, rect.width, rect.height);
        }

        static UiLayoutSpec absolute(float x, float y, float width, float height)
        {
            UiLayoutSpec layout;
            layout.positionMode = UiPositionMode::Absolute;
            layout.widthMode = UiSizeMode::Fixed;
            layout.heightMode = UiSizeMode::Fixed;
            layout.offsetMin = {x, y};
            layout.fixedWidth = width;
            layout.fixedHeight = height;
            return layout;
        }

        static void setState(UiSystem& ui, UiHandle handle, bool visible, bool interactable)
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

        static void setTextNode(UiSystem& ui,
                                UiHandle handle,
                                BitmapFont* font,
                                const std::string& text,
                                UiColor color,
                                float scale)
        {
            setState(ui, handle, true, false);
            ui.setPayload(handle, UiTextData{text, {}, color, scale});
            if (font != nullptr)
                ui.setTextFont(handle, font);
        }

        static void setTextWrap(UiSystem& ui, UiHandle handle, int maxLines)
        {
            const UiNode* node = ui.findNode(handle);
            if (node == nullptr || node->type != UiNodeType::Text)
                return;

            UiTextData text = std::get<UiTextData>(node->payload);
            text.wrapMode = UiTextWrapMode::Word;
            text.maxLines = maxLines;
            ui.setPayload(handle, text);
        }

        static void setTextAlignment(UiSystem& ui,
                                     UiHandle handle,
                                     UiHorizontalAlign horizontalAlign,
                                     UiVerticalAlign verticalAlign)
        {
            const UiNode* node = ui.findNode(handle);
            if (node == nullptr || node->type != UiNodeType::Text)
                return;

            UiTextData text = std::get<UiTextData>(node->payload);
            text.horizontalAlign = horizontalAlign;
            text.verticalAlign = verticalAlign;
            ui.setPayload(handle, text);
        }

        static void syncBackground(UiSystem& ui,
                                   const VNDialogueUiHandles& handles,
                                   const VNStage& stage)
        {
            const VNBackground& background = stage.getBackground();
            const bool showImage = background.mode == VNBackgroundMode::FullscreenImage
                && !background.imageAsset.empty();
            const bool showColor = background.mode == VNBackgroundMode::SolidColor
                || background.mode == VNBackgroundMode::None;

            setState(ui, handles.backgroundImage, showImage, false);
            setState(ui, handles.backgroundColor, showColor, false);

            if (showImage)
            {
                UiImageData image;
                image.imageAsset = background.imageAsset;
                image.tint = {1.0f, 1.0f, 1.0f, 1.0f};
                ui.setPayload(handles.backgroundImage, image);
            }

            if (showColor)
            {
                const UiColor color = background.mode == VNBackgroundMode::None
                    ? UiColor{0.0f, 0.0f, 0.0f, 0.0f}
                    : UiColor{background.color.r, background.color.g, background.color.b, background.color.a};
                ui.setPayload(handles.backgroundColor, UiRectData{color});
            }
        }

        static void syncDimming(UiSystem& ui,
                                const VNDialogueUiHandles& handles,
                                const VNDialogueUiConfig& config,
                                const VNStage& stage)
        {
            const float alpha = std::clamp(stage.getDimming(), 0.0f, 1.0f);
            setState(ui, handles.dimming, alpha > 0.0f, false);
            ui.setPayload(handles.dimming, UiRectData{
                UiColor{config.dimColor.r, config.dimColor.g, config.dimColor.b, config.dimColor.a * alpha}
            });
        }

        static void syncSprites(UiSystem& ui,
                                const VNDialogueUiHandles& handles,
                                const VNStage& stage)
        {
            const std::vector<VNSpriteView> sprites = stage.spritesForRender();
            for (size_t i = 0; i < handles.spriteImages.size(); ++i)
            {
                if (i >= sprites.size())
                {
                    setState(ui, handles.spriteImages[i], false, false);
                    continue;
                }

                const VNSprite& sprite = sprites[i].sprite;
                const float width = std::max(0.0f, sprite.size.x * sprite.scale.x);
                const float height = std::max(0.0f, sprite.size.y * sprite.scale.y);
                const float x = sprite.position.x - width * 0.5f;
                const float y = sprite.position.y - height * 0.5f;

                ui.setLayout(handles.spriteImages[i], absolute(x, y, width, height));
                setState(ui, handles.spriteImages[i], true, false);

                UiImageData image;
                image.imageAsset = sprite.imageAsset;
                image.tint = {1.0f, 1.0f, 1.0f, std::clamp(sprite.alpha, 0.0f, 1.0f)};
                image.rotation = sprite.rotation;
                ui.setPayload(handles.spriteImages[i], image);
            }
        }

        static void syncDialogue(UiSystem& ui,
                                 const VNDialogueUiHandles& handles,
                                 const VNDialogueUiConfig& config,
                                 const VNDialogueState& dialogue)
        {
            const VNPresentationProfile& profile = config.profile;
            const bool showDialogue = dialogue.visible || !dialogue.choices.empty();
            setState(ui, handles.dialogueLayer, showDialogue, false);
            setState(ui, handles.choiceLayer, !dialogue.choices.empty(), false);
            gts::ui::UiPanelSkinNode::setState(ui, handles.panelShadow, showDialogue, false);
            gts::ui::UiPanelSkinNode::setState(ui, handles.panel, showDialogue, false);
            gts::ui::UiPanelSkinNode::setState(ui, handles.nameplate, showDialogue && !dialogue.speaker.empty(), false);

            if (!showDialogue)
                return;

            setState(ui, handles.speakerText, !dialogue.speaker.empty(), false);
            ui.setPayload(handles.speakerText, UiTextData{
                dialogue.speaker,
                {},
                profile.dialogueSkin.speakerText,
                profile.dialogueSkin.speakerTextScale
            });

            UiTextData bodyText;
            bodyText.text = dialogue.visibleText;
            bodyText.color = profile.dialogueSkin.bodyText;
            bodyText.scale = profile.dialogueSkin.bodyTextScale;
            bodyText.wrapMode = UiTextWrapMode::Word;
            bodyText.maxLines = 4;
            ui.setPayload(handles.bodyText, bodyText);

            setState(ui, handles.continueIndicator, dialogue.continueIndicatorVisible, false);

            for (size_t i = 0; i < handles.choiceButtons.size(); ++i)
            {
                const bool visible = i < dialogue.choices.size();
                gts::ui::UiPanelSkinNode::setState(ui, handles.choiceButtons[i], visible, true);
                setState(ui, handles.choiceLabels[i], visible, false);

                if (!visible)
                    continue;

                gts::ui::UiPanelSkinNode::applyStateSkin(ui, handles.choiceButtons[i], profile.dialogueSkin.choice);

                UiTextData label;
                label.text = dialogue.choices[i].label;
                label.color = profile.dialogueSkin.choiceText;
                label.scale = profile.dialogueSkin.choiceTextScale;
                label.horizontalAlign = UiHorizontalAlign::Left;
                label.verticalAlign = UiVerticalAlign::Middle;
                label.wrapMode = UiTextWrapMode::Word;
                label.maxLines = 2;
                ui.setPayload(handles.choiceLabels[i], label);
            }
        }
    };
} // namespace gts::vn
