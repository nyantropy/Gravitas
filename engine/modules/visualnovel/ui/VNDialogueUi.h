#pragma once

#include <algorithm>
#include <string>
#include <variant>
#include <vector>

#include "BitmapFont.h"
#include "UiEvent.h"
#include "UiInteraction.h"
#include "UiNode.h"
#include "UiSystem.h"
#include "UiWidget.h"
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
        UiHandle stageLayer = UI_INVALID_HANDLE;
        gts::ui::UiPanelWidget backgroundColor;
        gts::ui::UiImageWidget backgroundImage;
        gts::ui::UiPanelWidget dimming;
        UiHandle spriteLayer = UI_INVALID_HANDLE;
        std::vector<gts::ui::UiImageWidget> spriteImages;

        UiHandle dialogueLayer = UI_INVALID_HANDLE;
        gts::ui::UiPanelWidget panelShadow;
        gts::ui::UiPanelWidget panel;
        gts::ui::UiPanelWidget nameplate;
        gts::ui::UiLabelWidget speakerText;
        gts::ui::UiLabelWidget bodyText;
        gts::ui::UiLabelWidget continueIndicator;

        gts::ui::UiStackWidget choiceLayer;
        std::vector<gts::ui::UiButtonWidget> choiceButtons;
    };

    class VNDialogueUi
    {
    public:
        static VNDialogueUiHandles build(UiCompositionContext& context, const VNDialogueUiConfig& config)
        {
            installTheme(context.ui, context.surface, config.profile);
            gts::ui::UiWidgetContext widgetContext(context);
            return build(widgetContext, context.root, config);
        }

        static VNDialogueUiHandles build(UiSystem& ui, const VNDialogueUiConfig& config)
        {
            return build(ui, UI_INVALID_HANDLE, config);
        }

        static VNDialogueUiHandles build(UiSystem& ui, UiHandle parent, const VNDialogueUiConfig& config)
        {
            installTheme(ui, ui.getDefaultSurface(), config.profile);
            gts::ui::UiWidgetContext widgetContext(ui);
            return build(widgetContext,
                         parent == UI_INVALID_HANDLE ? ui.getRoot() : parent,
                         config);
        }

        static void destroy(UiCompositionContext& context, VNDialogueUiHandles& handles)
        {
            destroy(context.ui, context.surface, handles);
        }

        static void destroy(UiSystem& ui, VNDialogueUiHandles& handles)
        {
            destroy(ui, ui.getDefaultSurface(), handles);
        }

        static void sync(UiCompositionContext& context,
                         VNDialogueUiHandles& handles,
                         const VNDialogueUiConfig& config,
                         const VNRuntime& runtime)
        {
            sync(context.ui, context.surface, handles, config, runtime);
        }

        static void sync(UiSystem& ui,
                         VNDialogueUiHandles& handles,
                         const VNDialogueUiConfig& config,
                         const VNRuntime& runtime)
        {
            sync(ui, ui.getDefaultSurface(), handles, config, runtime);
        }

        static int choiceIndexFromInteraction(const VNDialogueUiHandles& handles,
                                              const UiInteractionResult& interaction)
        {
            if (interaction.clicked == UI_INVALID_HANDLE)
                return -1;

            for (size_t i = 0; i < handles.choiceButtons.size(); ++i)
            {
                if (interaction.clicked == handles.choiceButtons[i].root()
                    || interaction.clicked == handles.choiceButtons[i].label())
                {
                    return static_cast<int>(i);
                }
            }

            return -1;
        }

        static int choiceIndexFromEvent(const VNDialogueUiHandles& handles, const UiEvent& event)
        {
            if (event.type != UiEventType::PointerClick && event.type != UiEventType::NavigationSubmit)
                return -1;

            for (size_t i = 0; i < handles.choiceButtons.size(); ++i)
            {
                if (eventTargetsHandle(event, handles.choiceButtons[i].root())
                    || eventTargetsHandle(event, handles.choiceButtons[i].label()))
                {
                    return static_cast<int>(i);
                }
            }

            return -1;
        }

        static int handleChoiceEvent(UiCompositionContext& context,
                                     VNDialogueUiHandles& handles,
                                     UiEvent& event)
        {
            gts::ui::UiWidgetContext widgetContext(context);
            return handleChoiceEvent(widgetContext, handles, event);
        }

        static int handleChoiceEvent(UiSystem& ui, VNDialogueUiHandles& handles, UiEvent& event)
        {
            gts::ui::UiWidgetContext widgetContext(ui);
            return handleChoiceEvent(widgetContext, handles, event);
        }

    private:
        static void installTheme(UiSystem& ui,
                                 UiSurfaceId surface,
                                 const VNPresentationProfile& profile)
        {
            const UiTheme* parentTheme = ui.theme(surface);
            ui.setTheme(surface, vnThemeFromPresentationProfile(profile, parentTheme));
        }

        static VNDialogueUiHandles build(gts::ui::UiWidgetContext& widgetContext,
                                         UiHandle parent,
                                         const VNDialogueUiConfig& config)
        {
            VNDialogueUiHandles handles;
            const VNPresentationProfile& profile = config.profile;

            handles.root = widgetContext.ui.createNode(widgetContext.surface, UiNodeType::Container, parent);
            UiLayoutSpec rootLayout = gts::ui::fillLayout();
            rootLayout.layoutMode = UiLayoutMode::Overlay;
            widgetContext.ui.setLayout(widgetContext.surface, handles.root, rootLayout);
            setState(widgetContext, handles.root, false, false);

            handles.stageLayer = widgetContext.ui.createNode(widgetContext.surface, UiNodeType::Container, handles.root);
            UiLayoutSpec stageLayout = gts::ui::fillLayout();
            stageLayout.layoutMode = UiLayoutMode::Overlay;
            widgetContext.ui.setLayout(widgetContext.surface, handles.stageLayer, stageLayout);
            setState(widgetContext, handles.stageLayer, true, false);

            gts::ui::UiPanelDesc backgroundDesc;
            backgroundDesc.layout = gts::ui::fillLayout();
            backgroundDesc.styleClass = VNThemeClass::StageBackground;
            backgroundDesc.visible = false;
            handles.backgroundColor.build(widgetContext, handles.stageLayer, backgroundDesc);

            gts::ui::UiImageDesc backgroundImageDesc;
            backgroundImageDesc.layout = gts::ui::fillLayout();
            backgroundImageDesc.visible = false;
            handles.backgroundImage.build(widgetContext, handles.stageLayer, backgroundImageDesc);

            gts::ui::UiPanelDesc dimmingDesc;
            dimmingDesc.layout = gts::ui::fillLayout();
            dimmingDesc.styleClass = VNThemeClass::StageDimming;
            dimmingDesc.visible = false;
            handles.dimming.build(widgetContext, handles.stageLayer, dimmingDesc);

            handles.spriteLayer = widgetContext.ui.createNode(widgetContext.surface, UiNodeType::Container, handles.stageLayer);
            widgetContext.ui.setLayout(widgetContext.surface, handles.spriteLayer, gts::ui::fillLayout());
            setState(widgetContext, handles.spriteLayer, true, false);

            handles.spriteImages.resize(profile.layout.maxSprites);
            for (gts::ui::UiImageWidget& sprite : handles.spriteImages)
            {
                gts::ui::UiImageDesc spriteDesc;
                spriteDesc.layout = absolute(0.0f, 0.0f, 0.0f, 0.0f);
                spriteDesc.visible = false;
                sprite.build(widgetContext, handles.spriteLayer, spriteDesc);
            }

            handles.dialogueLayer = widgetContext.ui.createNode(widgetContext.surface, UiNodeType::Container, handles.root);
            UiLayoutSpec dialogueLayout = semanticOverlayRegion(profile.layout.dialogue);
            dialogueLayout.layoutMode = UiLayoutMode::Overlay;
            widgetContext.ui.setLayout(widgetContext.surface, handles.dialogueLayer, dialogueLayout);
            setState(widgetContext, handles.dialogueLayer, false, false);

            gts::ui::UiPanelDesc shadowDesc;
            shadowDesc.layout = shadowLayout(widgetContext);
            shadowDesc.styleClass = VNThemeClass::DialoguePanelShadow;
            shadowDesc.visible = false;
            handles.panelShadow.build(widgetContext, handles.dialogueLayer, shadowDesc);

            gts::ui::UiPanelDesc panelDesc;
            panelDesc.layout = gts::ui::fillLayout();
            panelDesc.styleClass = VNThemeClass::DialoguePanel;
            panelDesc.visible = false;
            handles.panel.build(widgetContext, handles.dialogueLayer, panelDesc);
            makeOverlayContent(widgetContext, handles.panel.content());

            gts::ui::UiPanelDesc nameplateDesc;
            nameplateDesc.layout = relativeOverlayRegion(profile.layout.nameplate);
            nameplateDesc.styleClass = VNThemeClass::DialogueNameplate;
            nameplateDesc.visible = false;
            handles.nameplate.build(widgetContext, handles.dialogueLayer, nameplateDesc);

            gts::ui::UiLabelDesc speakerDesc;
            speakerDesc.layout = gts::ui::fillLayout();
            speakerDesc.font = config.font;
            speakerDesc.styleClass = VNThemeClass::SpeakerText;
            speakerDesc.horizontalAlign = UiHorizontalAlign::Left;
            speakerDesc.verticalAlign = UiVerticalAlign::Middle;
            speakerDesc.visible = false;
            handles.speakerText.build(widgetContext, handles.nameplate.content(), speakerDesc);

            gts::ui::UiLabelDesc bodyDesc;
            bodyDesc.layout = gts::ui::fillLayout();
            bodyDesc.font = config.font;
            bodyDesc.styleClass = VNThemeClass::BodyText;
            bodyDesc.wrapMode = UiTextWrapMode::Word;
            bodyDesc.maxLines = 4;
            bodyDesc.visible = false;
            handles.bodyText.build(widgetContext, handles.panel.content(), bodyDesc);

            gts::ui::UiLabelDesc continueDesc;
            continueDesc.layout = relativeOverlayRegion(profile.layout.continueIndicator);
            continueDesc.text = ">";
            continueDesc.font = config.font;
            continueDesc.styleClass = VNThemeClass::ContinueText;
            continueDesc.horizontalAlign = UiHorizontalAlign::Right;
            continueDesc.verticalAlign = UiVerticalAlign::Middle;
            continueDesc.visible = false;
            handles.continueIndicator.build(widgetContext, handles.panel.content(), continueDesc);

            gts::ui::UiStackDesc choiceStackDesc;
            choiceStackDesc.layout = choiceLayerLayout(profile.layout.choices);
            choiceStackDesc.axis = UiLayoutAxis::Vertical;
            choiceStackDesc.crossAxisAlignment = UiLayoutAlignment::Stretch;
            choiceStackDesc.gapMetric = VNThemeMetric::ChoiceGap;
            choiceStackDesc.visible = false;
            handles.choiceLayer.build(widgetContext, handles.root, choiceStackDesc);

            handles.choiceButtons.resize(profile.layout.maxChoices);
            for (size_t i = 0; i < handles.choiceButtons.size(); ++i)
            {
                gts::ui::UiButtonDesc choiceDesc;
                choiceDesc.layout = choiceButtonLayout(widgetContext);
                choiceDesc.labelLayout = choiceLabelLayout(widgetContext);
                choiceDesc.font = config.font;
                choiceDesc.styleClass = VNThemeClass::ChoiceButton;
                choiceDesc.labelStyleClass = VNThemeClass::ChoiceLabel;
                choiceDesc.horizontalAlign = UiHorizontalAlign::Left;
                choiceDesc.verticalAlign = UiVerticalAlign::Middle;
                choiceDesc.wrapMode = UiTextWrapMode::Word;
                choiceDesc.maxLines = 2;
                choiceDesc.visible = false;
                choiceDesc.navigationGroup = "vn.choices";
                choiceDesc.tabIndex = static_cast<int>(i);
                choiceDesc.wrapNavigation = true;
                choiceDesc.accessibilityHint = "Select dialogue choice";
                handles.choiceButtons[i].build(widgetContext, handles.choiceLayer.root(), choiceDesc);
            }

            return handles;
        }

        static void destroy(UiSystem& ui, UiSurfaceId surface, VNDialogueUiHandles& handles)
        {
            if (handles.root != UI_INVALID_HANDLE)
                ui.removeNode(surface, handles.root);

            handles = VNDialogueUiHandles{};
        }

        static void sync(UiSystem& ui,
                         UiSurfaceId surface,
                         VNDialogueUiHandles& handles,
                         const VNDialogueUiConfig& config,
                         const VNRuntime& runtime)
        {
            if (handles.root == UI_INVALID_HANDLE)
                return;

            UiDocument* document = ui.findDocument(surface);
            if (document == nullptr)
                return;

            UiCompositionContext compositionContext{
                ui,
                *document,
                nullptr,
                surface,
                UI_INVALID_MOUNT,
                handles.root
            };
            gts::ui::UiWidgetContext widgetContext(compositionContext);

            const bool active = runtime.isActive();
            setState(widgetContext, handles.root, active, false);
            if (!active)
                return;

            syncBackground(widgetContext, handles, runtime.getStage());
            syncDimming(widgetContext, handles, config, runtime.getStage());
            syncSprites(widgetContext, handles, runtime.getStage());
            syncDialogue(widgetContext, handles, runtime.getDialogue());
        }

        static int handleChoiceEvent(gts::ui::UiWidgetContext& widgetContext,
                                     VNDialogueUiHandles& handles,
                                     UiEvent& event)
        {
            if (event.type != UiEventType::PointerClick && event.type != UiEventType::NavigationSubmit)
                return -1;

            for (size_t i = 0; i < handles.choiceButtons.size(); ++i)
            {
                handles.choiceButtons[i].onEvent(widgetContext, event);
                if (handles.choiceButtons[i].consumePressed())
                    return static_cast<int>(i);
            }

            return -1;
        }

        static bool eventTargetsHandle(const UiEvent& event, UiHandle handle)
        {
            if (handle == UI_INVALID_HANDLE)
                return false;
            if (event.target == handle)
                return true;

            return std::find(event.targetPath.begin(), event.targetPath.end(), handle) != event.targetPath.end();
        }

        static UiLayoutSpec semanticOverlayRegion(const UiRect& rect)
        {
            UiLayoutSpec layout;
            layout.constraints.preferredWidth = {UiLayoutUnit::Normalized, std::max(0.0f, rect.width)};
            layout.constraints.preferredHeight = {UiLayoutUnit::Normalized, std::max(0.0f, rect.height)};
            layout.constraints.horizontalAlignment = UiLayoutAlignment::Center;
            layout.constraints.verticalAlignment = UiLayoutAlignment::End;
            layout.margin.bottom = std::max(0.0f, 1.0f - rect.y - rect.height);
            return layout;
        }

        static UiLayoutSpec choiceLayerLayout(const UiRect& rect)
        {
            UiLayoutSpec layout;
            layout.constraints.preferredWidth = {UiLayoutUnit::Normalized, std::max(0.0f, rect.width)};
            layout.constraints.preferredHeight = {UiLayoutUnit::Content, 0.0f};
            layout.constraints.horizontalAlignment = UiLayoutAlignment::End;
            layout.constraints.verticalAlignment = UiLayoutAlignment::Center;
            layout.margin.right = std::max(0.0f, 1.0f - rect.x - rect.width);
            return layout;
        }

        static UiLayoutSpec relativeOverlayRegion(const UiRect& rect)
        {
            UiLayoutSpec layout;
            layout.constraints.preferredWidth = {UiLayoutUnit::Percent, std::max(0.0f, rect.width)};
            layout.constraints.preferredHeight = {UiLayoutUnit::Percent, std::max(0.0f, rect.height)};
            layout.constraints.horizontalAlignment =
                rect.x + rect.width * 0.5f >= 0.5f ? UiLayoutAlignment::End : UiLayoutAlignment::Start;
            layout.constraints.verticalAlignment =
                rect.y + rect.height * 0.5f >= 0.5f ? UiLayoutAlignment::End : UiLayoutAlignment::Start;

            if (layout.constraints.horizontalAlignment == UiLayoutAlignment::End)
                layout.margin.right = 1.0f - rect.x - rect.width;
            else
                layout.margin.left = rect.x;

            if (layout.constraints.verticalAlignment == UiLayoutAlignment::End)
                layout.margin.bottom = 1.0f - rect.y - rect.height;
            else
                layout.margin.top = rect.y;
            return layout;
        }

        static UiLayoutSpec shadowLayout(gts::ui::UiWidgetContext& context)
        {
            const UiTheme* theme = context.ui.theme(context.surface);
            const float offsetX = theme == nullptr
                ? 0.010f
                : theme->metricOr(VNThemeMetric::PanelShadowOffsetX, 0.010f);
            const float offsetY = theme == nullptr
                ? 0.030f
                : theme->metricOr(VNThemeMetric::PanelShadowOffsetY, 0.030f);
            UiLayoutSpec layout = gts::ui::fillLayout();
            layout.margin = {offsetX, offsetY, -offsetX, -offsetY};
            return layout;
        }

        static UiLayoutSpec choiceButtonLayout(gts::ui::UiWidgetContext& context)
        {
            const UiTheme* theme = context.ui.theme(context.surface);
            const float height = theme == nullptr
                ? 0.044f
                : theme->metricOr(VNThemeMetric::ChoiceButtonHeight, 0.044f);

            UiLayoutSpec layout;
            layout.constraints.preferredHeight = {UiLayoutUnit::Normalized, height};
            return layout;
        }

        static UiLayoutSpec choiceLabelLayout(gts::ui::UiWidgetContext& context)
        {
            const UiTheme* theme = context.ui.theme(context.surface);
            const float inset = theme == nullptr
                ? 0.045f
                : theme->metricOr(VNThemeMetric::ChoiceLabelInset, 0.045f);
            return anchored(inset, 0.0f, std::max(0.0f, 1.0f - inset * 2.0f), 1.0f);
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

        static void makeOverlayContent(gts::ui::UiWidgetContext& context, UiHandle handle)
        {
            const UiNode* node = context.ui.findNode(context.surface, handle);
            UiLayoutSpec layout = gts::ui::fillLayout();
            layout.layoutMode = UiLayoutMode::Overlay;
            if (node != nullptr)
                layout.padding = node->layout.padding;
            context.ui.setLayout(context.surface, handle, layout);
        }

        static void setState(gts::ui::UiWidgetContext& context,
                             UiHandle handle,
                             bool visible,
                             bool interactable)
        {
            if (handle == UI_INVALID_HANDLE)
                return;

            UiStateFlags state;
            if (const UiNode* node = context.ui.findNode(context.surface, handle))
                state = node->state;

            state.visible = visible;
            state.enabled = visible;
            state.interactable = visible && interactable;
            if (!visible)
            {
                state.hovered = false;
                state.focused = false;
                state.pressed = false;
            }
            context.ui.setState(context.surface, handle, state);
        }

        static void syncBackground(gts::ui::UiWidgetContext& context,
                                   const VNDialogueUiHandles& handles,
                                   const VNStage& stage)
        {
            const VNBackground& background = stage.getBackground();
            const bool showImage = background.mode == VNBackgroundMode::FullscreenImage
                && !background.imageAsset.empty();
            const bool showColor = background.mode == VNBackgroundMode::SolidColor
                || background.mode == VNBackgroundMode::None;

            setState(context, handles.backgroundImage.root(), showImage, false);
            setState(context, handles.backgroundColor.root(), showColor, false);

            if (showImage)
            {
                UiImageData image;
                image.imageAsset = background.imageAsset;
                image.tint = {1.0f, 1.0f, 1.0f, 1.0f};
                context.ui.setPayload(context.surface, handles.backgroundImage.root(), image);
            }

            if (showColor)
            {
                const UiColor color = background.mode == VNBackgroundMode::None
                    ? UiColor{0.0f, 0.0f, 0.0f, 0.0f}
                    : UiColor{background.color.r, background.color.g, background.color.b, background.color.a};
                context.ui.setPayload(context.surface, handles.backgroundColor.root(), UiRectData{color});
            }
        }

        static void syncDimming(gts::ui::UiWidgetContext& context,
                                const VNDialogueUiHandles& handles,
                                const VNDialogueUiConfig& config,
                                const VNStage& stage)
        {
            const float alpha = std::clamp(stage.getDimming(), 0.0f, 1.0f);
            setState(context, handles.dimming.root(), alpha > 0.0f, false);
            context.ui.setPayload(context.surface, handles.dimming.root(), UiRectData{
                UiColor{config.dimColor.r, config.dimColor.g, config.dimColor.b, config.dimColor.a * alpha}
            });
        }

        static void syncSprites(gts::ui::UiWidgetContext& context,
                                const VNDialogueUiHandles& handles,
                                const VNStage& stage)
        {
            const std::vector<VNSpriteView> sprites = stage.spritesForRender();
            for (size_t i = 0; i < handles.spriteImages.size(); ++i)
            {
                const UiHandle spriteHandle = handles.spriteImages[i].root();
                if (i >= sprites.size())
                {
                    setState(context, spriteHandle, false, false);
                    continue;
                }

                const VNSprite& sprite = sprites[i].sprite;
                const float width = std::max(0.0f, sprite.size.x * sprite.scale.x);
                const float height = std::max(0.0f, sprite.size.y * sprite.scale.y);
                const float x = sprite.position.x - width * 0.5f;
                const float y = sprite.position.y - height * 0.5f;

                context.ui.setLayout(context.surface, spriteHandle, absolute(x, y, width, height));
                setState(context, spriteHandle, true, false);

                UiImageData image;
                image.imageAsset = sprite.imageAsset;
                image.tint = {1.0f, 1.0f, 1.0f, std::clamp(sprite.alpha, 0.0f, 1.0f)};
                image.rotation = sprite.rotation;
                context.ui.setPayload(context.surface, spriteHandle, image);
            }
        }

        static void syncDialogue(gts::ui::UiWidgetContext& context,
                                 VNDialogueUiHandles& handles,
                                 const VNDialogueState& dialogue)
        {
            const bool showDialogue = dialogue.visible || !dialogue.choices.empty();
            const bool showChoices = !dialogue.choices.empty();
            setState(context, handles.dialogueLayer, showDialogue, false);
            setState(context, handles.choiceLayer.root(), showChoices, false);
            handles.panelShadow.setVisible(context, showDialogue);
            handles.panel.setVisible(context, showDialogue);
            handles.nameplate.setVisible(context, showDialogue && !dialogue.speaker.empty());

            if (!showDialogue)
                return;

            handles.speakerText.setVisible(context, !dialogue.speaker.empty());
            handles.speakerText.setText(context, dialogue.speaker);

            handles.bodyText.setVisible(context, dialogue.visible);
            handles.bodyText.setText(context, dialogue.visibleText);

            handles.continueIndicator.setVisible(context, dialogue.continueIndicatorVisible);

            for (size_t i = 0; i < handles.choiceButtons.size(); ++i)
            {
                const bool visible = i < dialogue.choices.size();
                handles.choiceButtons[i].setVisible(context, visible);

                if (!visible)
                    continue;

                handles.choiceButtons[i].setText(context, dialogue.choices[i].label);
            }
        }
    };
} // namespace gts::vn
