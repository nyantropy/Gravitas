#pragma once

#include <algorithm>
#include <string>
#include <variant>
#include <vector>

#include "BitmapFont.h"
#include "UiInteraction.h"
#include "UiNode.h"
#include "UiSystem.h"
#include "VNRuntime.h"

namespace gts::vn
{
    struct VNDialogueUiConfig
    {
        BitmapFont* font = nullptr;
        size_t maxSprites = 8;
        size_t maxChoices = 6;
        UiColor dimColor = {0.0f, 0.0f, 0.0f, 1.0f};
        UiColor dialogueBackground = {0.030f, 0.036f, 0.048f, 0.88f};
        UiColor dialogueText = {0.94f, 0.96f, 0.92f, 1.0f};
        UiColor speakerText = {1.0f, 0.90f, 0.58f, 1.0f};
        UiColor choiceBackground = {0.050f, 0.062f, 0.080f, 0.94f};
        UiColor choiceHover = {0.115f, 0.132f, 0.158f, 0.98f};
        UiColor choicePressed = {0.180f, 0.170f, 0.110f, 0.98f};
        float speakerTextScale = 0.022f;
        float bodyTextScale = 0.023f;
        float choiceTextScale = 0.020f;
    };

    struct VNDialogueUiHandles
    {
        UiHandle root = UI_INVALID_HANDLE;
        UiHandle backgroundColor = UI_INVALID_HANDLE;
        UiHandle backgroundImage = UI_INVALID_HANDLE;
        UiHandle dimming = UI_INVALID_HANDLE;
        UiHandle spriteLayer = UI_INVALID_HANDLE;
        std::vector<UiHandle> spriteImages;
        UiHandle dialogueRoot = UI_INVALID_HANDLE;
        UiHandle dialogueBackground = UI_INVALID_HANDLE;
        UiHandle speakerText = UI_INVALID_HANDLE;
        UiHandle bodyText = UI_INVALID_HANDLE;
        UiHandle continueIndicator = UI_INVALID_HANDLE;
        UiHandle choiceRoot = UI_INVALID_HANDLE;
        std::vector<UiHandle> choiceButtons;
        std::vector<UiHandle> choiceLabels;
    };

    class VNDialogueUi
    {
    public:
        static VNDialogueUiHandles build(UiSystem& ui, const VNDialogueUiConfig& config)
        {
            VNDialogueUiHandles handles;

            handles.root = ui.createNode(UiNodeType::Container);
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

            handles.spriteImages.reserve(config.maxSprites);
            for (size_t i = 0; i < config.maxSprites; ++i)
            {
                UiHandle image = ui.createNode(UiNodeType::Image, handles.spriteLayer);
                ui.setLayout(image, absolute(0.0f, 0.0f, 0.0f, 0.0f));
                setState(ui, image, false, false);
                handles.spriteImages.push_back(image);
            }

            handles.dialogueRoot = ui.createNode(UiNodeType::Container, handles.root);
            ui.setLayout(handles.dialogueRoot, anchored(0.060f, 0.720f, 0.880f, 0.220f));
            setState(ui, handles.dialogueRoot, false, false);

            handles.dialogueBackground = ui.createNode(UiNodeType::Rect, handles.dialogueRoot);
            ui.setLayout(handles.dialogueBackground, anchored(0.0f, 0.0f, 1.0f, 1.0f));
            setState(ui, handles.dialogueBackground, true, false);
            ui.setPayload(handles.dialogueBackground, UiRectData{config.dialogueBackground});

            handles.speakerText = ui.createNode(UiNodeType::Text, handles.dialogueRoot);
            ui.setLayout(handles.speakerText, anchored(0.040f, 0.080f, 0.360f, 0.180f));
            setTextNode(ui, handles.speakerText, config.font, "", config.speakerText, config.speakerTextScale);

            handles.bodyText = ui.createNode(UiNodeType::Text, handles.dialogueRoot);
            ui.setLayout(handles.bodyText, anchored(0.040f, 0.300f, 0.880f, 0.520f));
            setTextNode(ui, handles.bodyText, config.font, "", config.dialogueText, config.bodyTextScale);
            setTextWrap(ui, handles.bodyText, 4);

            handles.continueIndicator = ui.createNode(UiNodeType::Text, handles.dialogueRoot);
            ui.setLayout(handles.continueIndicator, anchored(0.920f, 0.800f, 0.050f, 0.140f));
            setTextNode(ui, handles.continueIndicator, config.font, ">", config.speakerText, config.bodyTextScale);
            setTextAlignment(ui, handles.continueIndicator, UiHorizontalAlign::Right, UiVerticalAlign::Middle);

            handles.choiceRoot = ui.createNode(UiNodeType::Container, handles.root);
            ui.setLayout(handles.choiceRoot, anchored(0.560f, 0.390f, 0.380f, 0.300f));
            setState(ui, handles.choiceRoot, false, false);

            handles.choiceButtons.reserve(config.maxChoices);
            handles.choiceLabels.reserve(config.maxChoices);
            for (size_t i = 0; i < config.maxChoices; ++i)
            {
                const float rowHeight = 1.0f / static_cast<float>(std::max<size_t>(1, config.maxChoices));
                const float y = static_cast<float>(i) * rowHeight;

                UiHandle button = ui.createNode(UiNodeType::Rect, handles.choiceRoot);
                ui.setLayout(button, anchored(0.0f, y + 0.060f * rowHeight, 1.0f, 0.800f * rowHeight));
                setState(ui, button, false, true);
                ui.setPayload(button, UiRectData{config.choiceBackground});

                UiHandle label = ui.createNode(UiNodeType::Text, button);
                ui.setLayout(label, anchored(0.045f, 0.0f, 0.910f, 1.0f));
                setTextNode(ui, label, config.font, "", config.dialogueText, config.choiceTextScale);
                setTextAlignment(ui, label, UiHorizontalAlign::Left, UiVerticalAlign::Middle);

                handles.choiceButtons.push_back(button);
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
                if (interaction.clicked == handles.choiceButtons[i]
                    || interaction.clicked == handles.choiceLabels[i])
                {
                    return static_cast<int>(i);
                }
            }

            return -1;
        }

    private:
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
            ui.setState(handle, UiStateFlags{
                .visible = visible,
                .enabled = visible,
                .interactable = interactable
            });
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
            const bool showDialogue = dialogue.visible || !dialogue.choices.empty();
            setState(ui, handles.dialogueRoot, showDialogue, false);
            setState(ui, handles.choiceRoot, !dialogue.choices.empty(), false);

            if (!showDialogue)
                return;

            setState(ui, handles.speakerText, !dialogue.speaker.empty(), false);
            ui.setPayload(handles.speakerText, UiTextData{
                dialogue.speaker,
                {},
                config.speakerText,
                config.speakerTextScale
            });

            UiTextData bodyText;
            bodyText.text = dialogue.visibleText;
            bodyText.color = config.dialogueText;
            bodyText.scale = config.bodyTextScale;
            bodyText.wrapMode = UiTextWrapMode::Word;
            bodyText.maxLines = 4;
            ui.setPayload(handles.bodyText, bodyText);

            setState(ui, handles.continueIndicator, dialogue.continueIndicatorVisible, false);

            for (size_t i = 0; i < handles.choiceButtons.size(); ++i)
            {
                const bool visible = i < dialogue.choices.size();
                setState(ui, handles.choiceButtons[i], visible, true);
                setState(ui, handles.choiceLabels[i], visible, false);

                if (!visible)
                    continue;

                const UiNode* buttonNode = ui.findNode(handles.choiceButtons[i]);
                const bool hovered = buttonNode != nullptr && buttonNode->state.hovered;
                const bool pressed = buttonNode != nullptr && buttonNode->state.pressed;
                const UiColor background = pressed
                    ? config.choicePressed
                    : (hovered ? config.choiceHover : config.choiceBackground);
                ui.setPayload(handles.choiceButtons[i], UiRectData{background});

                UiTextData label;
                label.text = dialogue.choices[i].label;
                label.color = config.dialogueText;
                label.scale = config.choiceTextScale;
                label.horizontalAlign = UiHorizontalAlign::Left;
                label.verticalAlign = UiVerticalAlign::Middle;
                label.wrapMode = UiTextWrapMode::Word;
                label.maxLines = 2;
                ui.setPayload(handles.choiceLabels[i], label);
            }
        }
    };
} // namespace gts::vn
