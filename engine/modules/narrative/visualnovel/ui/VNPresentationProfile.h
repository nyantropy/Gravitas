#pragma once

#include <algorithm>
#include <string>

#include "UiLayout.h"
#include "UiPanelSkin.h"
#include "UiTheme.h"
#include "UiTypes.h"
#include "VNTypes.h"

namespace gts::vn
{
    namespace VNThemeClass
    {
        inline constexpr const char* StageBackground = "VN.Stage.Background";
        inline constexpr const char* StageDimming = "VN.Stage.Dimming";
        inline constexpr const char* InteractionSlot = "Interaction.Slot";
        inline constexpr const char* DialoguePanelShadow = "VN.Dialogue.PanelShadow";
        inline constexpr const char* DialoguePanel = "VN.Dialogue.Panel";
        inline constexpr const char* DialogueNameplate = "VN.Dialogue.Nameplate";
        inline constexpr const char* SpeakerText = "VN.Text.Speaker";
        inline constexpr const char* BodyText = "VN.Text.Body";
        inline constexpr const char* ContinueText = "VN.Text.Continue";
        inline constexpr const char* ChoiceButton = "VN.Choice.Button";
        inline constexpr const char* ChoiceLabel = "VN.Choice.Label";
        inline constexpr const char* MerchantWindow = "Merchant.Window";
        inline constexpr const char* MerchantHeader = "Merchant.Header";
        inline constexpr const char* MerchantItemRow = "Merchant.ItemRow";
        inline constexpr const char* MerchantPrice = "Merchant.Price";
        inline constexpr const char* MerchantPlayerInventory = "Merchant.PlayerInventory";
        inline constexpr const char* MerchantVendorInventory = "Merchant.VendorInventory";
        inline constexpr const char* MerchantActionButton = "Merchant.ActionButton";
        inline constexpr const char* MerchantDisabledItem = "Merchant.DisabledItem";
        inline constexpr const char* MerchantHighlightedItem = "Merchant.HighlightedItem";
    }

    namespace VNThemeMetric
    {
        inline constexpr const char* InteractionGap = "Interaction.Gap";
        inline constexpr const char* InteractionPadding = "Interaction.Padding";
        inline constexpr const char* ChoiceGap = "VN.Choice.Gap";
        inline constexpr const char* ChoiceButtonHeight = "VN.Choice.ButtonHeight";
        inline constexpr const char* ChoiceLabelInset = "VN.Choice.LabelInset";
        inline constexpr const char* PanelShadowOffsetX = "VN.Dialogue.ShadowOffsetX";
        inline constexpr const char* PanelShadowOffsetY = "VN.Dialogue.ShadowOffsetY";
    }

    struct VNDialogueBoxSkin
    {
        UiPanelSkin panelShadow;
        UiPanelSkin panel;
        UiPanelSkin nameplate;
        UiPanelStateSkin choice;
        UiColor speakerText = {1.0f, 0.90f, 0.58f, 1.0f};
        UiColor bodyText = {0.94f, 0.96f, 0.92f, 1.0f};
        UiColor choiceText = {0.94f, 0.96f, 0.92f, 1.0f};
        UiColor continueIndicator = {1.0f, 0.90f, 0.58f, 1.0f};
        UiThickness textPadding;
        float speakerTextScale = 0.022f;
        float bodyTextScale = 0.023f;
        float choiceTextScale = 0.020f;
    };

    struct VNLayoutProfile
    {
        // Compatibility layout seeds for existing VN content. New authoring
        // should set VNPresentationProfile::layoutAuthoring to Semantic and
        // fill interactionLayout instead of adding more raw rectangles here.
        UiRect dialogue = {0.060f, 0.720f, 0.880f, 0.220f};
        UiRect nameplate = {0.040f, -0.090f, 0.320f, 0.180f};
        UiRect speakerText = {0.060f, 0.0f, 0.880f, 1.0f};
        UiRect bodyText = {0.045f, 0.260f, 0.880f, 0.560f};
        UiRect continueIndicator = {0.920f, 0.800f, 0.050f, 0.140f};
        UiRect interaction = {0.060f, 0.110f, 0.880f, 0.560f};
        UiRect choices = {0.560f, 0.390f, 0.380f, 0.300f};
        float choiceRowGap = 0.014f;
        size_t maxChoices = 6;
        size_t maxSprites = 8;
    };

    enum class VNLayoutAuthoringMode : uint8_t
    {
        CompatibilityRects = 0,
        Semantic
    };

    struct VNOverlaySlotLayout
    {
        UiLayoutLength width = {UiLayoutUnit::Percent, 1.0f};
        UiLayoutLength height = {UiLayoutUnit::Percent, 1.0f};
        UiLayoutAlignment horizontalAlignment = UiLayoutAlignment::Stretch;
        UiLayoutAlignment verticalAlignment = UiLayoutAlignment::Stretch;
        UiThickness margin;
    };

    struct VNContentSlotLayout
    {
        UiThickness inset;
    };

    struct VNChoiceStackLayout
    {
        UiLayoutLength width = {UiLayoutUnit::Percent, 1.0f};
        UiLayoutLength heightBudget = {UiLayoutUnit::Normalized, 0.0f};
        UiLayoutAlignment horizontalAlignment = UiLayoutAlignment::End;
        UiLayoutAlignment verticalAlignment = UiLayoutAlignment::Center;
        UiThickness margin;
        float gap = 0.0f;
        size_t maxChoices = 6;
    };

    struct VNInteractionLayout
    {
        VNOverlaySlotLayout dialoguePanel;
        VNOverlaySlotLayout nameplateSlot;
        VNContentSlotLayout speakerTextSlot;
        VNContentSlotLayout bodyTextSlot;
        VNOverlaySlotLayout continuePromptSlot;
        VNChoiceStackLayout choiceStack;
        VNOverlaySlotLayout interactionSlot;
        size_t maxSprites = 8;
    };

    struct VNPresentationProfile
    {
        VNDialogueBoxSkin dialogueSkin;
        VNLayoutProfile layout;
        VNLayoutAuthoringMode layoutAuthoring = VNLayoutAuthoringMode::CompatibilityRects;
        VNInteractionLayout interactionLayout;
        VNSpriteMotionProfile motionProfile;
    };

    inline UiLayoutLength vnPercent(float value)
    {
        return {UiLayoutUnit::Percent, value};
    }

    inline UiLayoutLength vnNormalized(float value)
    {
        return {UiLayoutUnit::Normalized, value};
    }

    inline VNOverlaySlotLayout vnOverlaySlot(UiLayoutLength width,
                                             UiLayoutLength height,
                                             UiLayoutAlignment horizontalAlignment,
                                             UiLayoutAlignment verticalAlignment,
                                             UiThickness margin = {})
    {
        VNOverlaySlotLayout slot;
        slot.width = width;
        slot.height = height;
        slot.horizontalAlignment = horizontalAlignment;
        slot.verticalAlignment = verticalAlignment;
        slot.margin = margin;
        return slot;
    }

    inline VNContentSlotLayout vnContentSlot(UiThickness inset)
    {
        VNContentSlotLayout slot;
        slot.inset = inset;
        return slot;
    }

    inline UiThickness vnInsetsFromCompatibilityRect(const UiRect& rect)
    {
        return UiThickness{
            std::max(0.0f, rect.x),
            std::max(0.0f, rect.y),
            std::max(0.0f, 1.0f - rect.x - rect.width),
            std::max(0.0f, 1.0f - rect.y - rect.height)
        };
    }

    inline VNOverlaySlotLayout vnOverlaySlotFromCompatibilityRect(const UiRect& rect,
                                                                  UiLayoutUnit sizeUnit = UiLayoutUnit::Percent)
    {
        VNOverlaySlotLayout slot;
        slot.width = {sizeUnit, std::max(0.0f, rect.width)};
        slot.height = {sizeUnit, std::max(0.0f, rect.height)};
        slot.horizontalAlignment =
            rect.x + rect.width * 0.5f >= 0.5f ? UiLayoutAlignment::End : UiLayoutAlignment::Start;
        slot.verticalAlignment =
            rect.y + rect.height * 0.5f >= 0.5f ? UiLayoutAlignment::End : UiLayoutAlignment::Start;

        if (slot.horizontalAlignment == UiLayoutAlignment::End)
            slot.margin.right = 1.0f - rect.x - rect.width;
        else
            slot.margin.left = rect.x;

        if (slot.verticalAlignment == UiLayoutAlignment::End)
            slot.margin.bottom = 1.0f - rect.y - rect.height;
        else
            slot.margin.top = rect.y;
        return slot;
    }

    inline VNInteractionLayout vnInteractionLayoutFromCompatibility(const VNLayoutProfile& rectProfile)
    {
        VNInteractionLayout layout;
        layout.dialoguePanel = vnOverlaySlot(vnNormalized(std::max(0.0f, rectProfile.dialogue.width)),
                                             vnNormalized(std::max(0.0f, rectProfile.dialogue.height)),
                                             UiLayoutAlignment::Center,
                                             UiLayoutAlignment::End,
                                             UiThickness{0.0f,
                                                         0.0f,
                                                         0.0f,
                                                         std::max(0.0f,
                                                                  1.0f - rectProfile.dialogue.y - rectProfile.dialogue.height)});
        layout.nameplateSlot = vnOverlaySlotFromCompatibilityRect(rectProfile.nameplate);
        layout.speakerTextSlot = vnContentSlot(vnInsetsFromCompatibilityRect(rectProfile.speakerText));
        layout.bodyTextSlot = vnContentSlot(vnInsetsFromCompatibilityRect(rectProfile.bodyText));
        layout.continuePromptSlot = vnOverlaySlotFromCompatibilityRect(rectProfile.continueIndicator);
        layout.choiceStack.width = vnNormalized(std::max(0.0f, rectProfile.choices.width));
        layout.choiceStack.heightBudget = vnNormalized(std::max(0.0f, rectProfile.choices.height));
        layout.choiceStack.horizontalAlignment = UiLayoutAlignment::End;
        layout.choiceStack.verticalAlignment = UiLayoutAlignment::Center;
        layout.choiceStack.margin.right = std::max(0.0f, 1.0f - rectProfile.choices.x - rectProfile.choices.width);
        layout.choiceStack.gap = std::max(0.0f, rectProfile.choiceRowGap * rectProfile.choices.height);
        layout.choiceStack.maxChoices = rectProfile.maxChoices;
        layout.interactionSlot = vnOverlaySlotFromCompatibilityRect(rectProfile.interaction);
        layout.maxSprites = rectProfile.maxSprites;
        return layout;
    }

    inline UiRect vnCompatibilityRectFromOverlaySlot(const VNOverlaySlotLayout& slot)
    {
        const float width = std::max(0.0f, slot.width.value);
        const float height = std::max(0.0f, slot.height.value);

        float x = slot.margin.left;
        if (slot.horizontalAlignment == UiLayoutAlignment::End)
            x = 1.0f - slot.margin.right - width;
        else if (slot.horizontalAlignment == UiLayoutAlignment::Center)
            x = 0.5f - width * 0.5f + (slot.margin.left - slot.margin.right) * 0.5f;

        float y = slot.margin.top;
        if (slot.verticalAlignment == UiLayoutAlignment::End)
            y = 1.0f - slot.margin.bottom - height;
        else if (slot.verticalAlignment == UiLayoutAlignment::Center)
            y = 0.5f - height * 0.5f + (slot.margin.top - slot.margin.bottom) * 0.5f;

        return UiRect{x, y, width, height};
    }

    inline VNInteractionLayout resolveVNInteractionLayout(const VNPresentationProfile& profile)
    {
        return profile.layoutAuthoring == VNLayoutAuthoringMode::Semantic
            ? profile.interactionLayout
            : vnInteractionLayoutFromCompatibility(profile.layout);
    }

    inline UiPanelSkin solidPanelSkin(UiColor color, UiThickness padding = {})
    {
        UiPanelSkin skin;
        skin.type = UiPanelSkinType::SolidColor;
        skin.color = color;
        skin.contentPadding = padding;
        return skin;
    }

    inline UiPanelSkin imagePanelSkin(const std::string& imageAsset,
                                      UiColor tint = {1.0f, 1.0f, 1.0f, 1.0f},
                                      UiThickness padding = {})
    {
        UiPanelSkin skin;
        skin.type = UiPanelSkinType::Image;
        skin.imageAsset = imageAsset;
        skin.tint = tint;
        skin.contentPadding = padding;
        return skin;
    }

    inline UiPanelSkin nineSlicePanelSkin(const std::string& imageAsset,
                                          UiThickness slice,
                                          UiColor tint = {1.0f, 1.0f, 1.0f, 1.0f},
                                          UiThickness padding = {})
    {
        UiPanelSkin skin;
        skin.type = UiPanelSkinType::NineSlice;
        skin.imageAsset = imageAsset;
        skin.tint = tint;
        skin.slice = slice;
        skin.contentPadding = padding;
        return skin;
    }

    inline VNPresentationProfile defaultVNPresentationProfile()
    {
        VNPresentationProfile profile;
        profile.interactionLayout = vnInteractionLayoutFromCompatibility(profile.layout);

        profile.dialogueSkin.panelShadow = solidPanelSkin({0.0f, 0.0f, 0.0f, 0.34f});
        profile.dialogueSkin.panel = solidPanelSkin({0.030f, 0.036f, 0.048f, 0.88f});
        UiThickness nameplatePadding;
        nameplatePadding.left = 0.014f;
        nameplatePadding.right = 0.014f;
        profile.dialogueSkin.nameplate = solidPanelSkin({0.075f, 0.064f, 0.046f, 0.94f}, nameplatePadding);

        UiThickness choicePadding;
        choicePadding.left = 0.010f;
        choicePadding.right = 0.010f;
        profile.dialogueSkin.choice.normal = solidPanelSkin({0.050f, 0.062f, 0.080f, 0.94f}, choicePadding);
        profile.dialogueSkin.choice.hover = solidPanelSkin({0.115f, 0.132f, 0.158f, 0.98f}, choicePadding);
        profile.dialogueSkin.choice.pressed = solidPanelSkin({0.180f, 0.170f, 0.110f, 0.98f}, choicePadding);
        profile.dialogueSkin.choice.disabled = solidPanelSkin({0.035f, 0.040f, 0.048f, 0.72f}, choicePadding);
        profile.dialogueSkin.textPadding = {0.030f, 0.020f, 0.030f, 0.020f};

        VNSpriteMotionPreset enterLeft;
        enterLeft.durationSeconds = 0.28f;
        enterLeft.ease = gts::tween::TweenEase::EaseOutQuad;
        enterLeft.positionOffset = {0.08f, 0.0f};
        enterLeft.waitForCompletion = true;
        profile.motionProfile.presets["enter_left"] = enterLeft;

        VNSpriteMotionPreset enterRight;
        enterRight.durationSeconds = 0.28f;
        enterRight.ease = gts::tween::TweenEase::EaseOutQuad;
        enterRight.positionOffset = {-0.08f, 0.0f};
        enterRight.waitForCompletion = true;
        profile.motionProfile.presets["enter_right"] = enterRight;

        VNSpriteMotionPreset emphasisPop;
        emphasisPop.durationSeconds = 0.16f;
        emphasisPop.ease = gts::tween::TweenEase::EaseOutQuad;
        emphasisPop.scaleOffset = {0.06f, 0.06f};
        profile.motionProfile.presets["emphasis_pop"] = emphasisPop;

        VNSpriteMotionPreset hurtFlash;
        hurtFlash.durationSeconds = 0.12f;
        hurtFlash.ease = gts::tween::TweenEase::Linear;
        hurtFlash.alphaOffset = -0.35f;
        profile.motionProfile.presets["hurt_flash"] = hurtFlash;

        return profile;
    }

    inline UiTheme vnThemeFromPresentationProfile(const VNPresentationProfile& profile,
                                                  const UiTheme* parentTheme = nullptr)
    {
        UiTheme theme;
        const VNInteractionLayout layout = resolveVNInteractionLayout(profile);
        if (parentTheme != nullptr)
            theme.setParent(*parentTheme);

        theme.setSkin("VN.Dialogue.PanelShadow", profile.dialogueSkin.panelShadow);
        theme.setSkin("VN.Dialogue.Panel", profile.dialogueSkin.panel);
        theme.setSkin("VN.Dialogue.Nameplate", profile.dialogueSkin.nameplate);

        theme.setTypography("VN.Speaker", UiTypography{.scale = profile.dialogueSkin.speakerTextScale});
        theme.setTypography("VN.Body", UiTypography{.scale = profile.dialogueSkin.bodyTextScale});
        theme.setTypography("VN.Choice", UiTypography{.scale = profile.dialogueSkin.choiceTextScale});
        theme.setTypography("VN.Continue", UiTypography{.scale = profile.dialogueSkin.bodyTextScale});

        const float choiceGap = layout.choiceStack.gap;
        const float choiceButtonHeight = layout.choiceStack.maxChoices == 0
            ? 0.044f
            : std::max(0.0f,
                       (layout.choiceStack.heightBudget.value
                        - choiceGap * static_cast<float>(std::max<size_t>(1, layout.choiceStack.maxChoices) - 1))
                           / static_cast<float>(std::max<size_t>(1, layout.choiceStack.maxChoices)));
        theme.setMetric(VNThemeMetric::ChoiceGap, choiceGap);
        theme.setMetric(VNThemeMetric::ChoiceButtonHeight, choiceButtonHeight);
        theme.setMetric(VNThemeMetric::ChoiceLabelInset,
                        std::max(0.0f, profile.dialogueSkin.choice.normal.contentPadding.left));
        theme.setMetric(VNThemeMetric::InteractionGap, 0.018f);
        theme.setMetric(VNThemeMetric::InteractionPadding, 0.020f);
        theme.setMetric(VNThemeMetric::PanelShadowOffsetX, 0.010f);
        theme.setMetric(VNThemeMetric::PanelShadowOffsetY, 0.030f);

        UiStyleClass stageBackground;
        stageBackground.base.backgroundColor = {0.0f, 0.0f, 0.0f, 0.0f};
        theme.setStyleClass(VNThemeClass::StageBackground, stageBackground);

        UiStyleClass stageDimming;
        stageDimming.base.backgroundColor = {0.0f, 0.0f, 0.0f, 0.0f};
        theme.setStyleClass(VNThemeClass::StageDimming, stageDimming);

        UiStyleClass interactionSlot;
        interactionSlot.base.backgroundColor = {0.0f, 0.0f, 0.0f, 0.0f};
        theme.setStyleClass(VNThemeClass::InteractionSlot, interactionSlot);

        UiStyleClass panelShadow;
        panelShadow.base.skinName = "VN.Dialogue.PanelShadow";
        theme.setStyleClass(VNThemeClass::DialoguePanelShadow, panelShadow);

        UiStyleClass panel;
        panel.base.skinName = "VN.Dialogue.Panel";
        panel.base.padding = profile.dialogueSkin.textPadding;
        theme.setStyleClass(VNThemeClass::DialoguePanel, panel);

        UiStyleClass nameplate;
        nameplate.base.skinName = "VN.Dialogue.Nameplate";
        theme.setStyleClass(VNThemeClass::DialogueNameplate, nameplate);

        UiStyleClass speakerText;
        speakerText.base.foregroundColor = profile.dialogueSkin.speakerText;
        speakerText.base.typographyName = "VN.Speaker";
        theme.setStyleClass(VNThemeClass::SpeakerText, speakerText);

        UiStyleClass bodyText;
        bodyText.base.foregroundColor = profile.dialogueSkin.bodyText;
        bodyText.base.typographyName = "VN.Body";
        theme.setStyleClass(VNThemeClass::BodyText, bodyText);

        UiStyleClass continueText;
        continueText.base.foregroundColor = profile.dialogueSkin.continueIndicator;
        continueText.base.typographyName = "VN.Continue";
        theme.setStyleClass(VNThemeClass::ContinueText, continueText);

        UiStyleClass choiceButton;
        choiceButton.base.skin = profile.dialogueSkin.choice.normal;
        if (profile.dialogueSkin.choice.hover)
            choiceButton.states[UiStyleState::Hover].skin = *profile.dialogueSkin.choice.hover;
        if (profile.dialogueSkin.choice.pressed)
            choiceButton.states[UiStyleState::Pressed].skin = *profile.dialogueSkin.choice.pressed;
        if (profile.dialogueSkin.choice.disabled)
            choiceButton.states[UiStyleState::Disabled].skin = *profile.dialogueSkin.choice.disabled;
        theme.setStyleClass(VNThemeClass::ChoiceButton, choiceButton);

        UiStyleClass choiceLabel;
        choiceLabel.base.foregroundColor = profile.dialogueSkin.choiceText;
        choiceLabel.base.typographyName = "VN.Choice";
        theme.setStyleClass(VNThemeClass::ChoiceLabel, choiceLabel);

        UiStyleClass merchantWindow;
        merchantWindow.base.skin = profile.dialogueSkin.panel;
        theme.setStyleClass(VNThemeClass::MerchantWindow, merchantWindow);

        UiStyleClass merchantHeader;
        merchantHeader.base.foregroundColor = {0.86f, 0.92f, 0.84f, 1.0f};
        theme.setStyleClass(VNThemeClass::MerchantHeader, merchantHeader);

        UiStyleClass merchantItemRow;
        merchantItemRow.base.backgroundColor = {0.045f, 0.058f, 0.052f, 0.94f};
        theme.setStyleClass(VNThemeClass::MerchantItemRow, merchantItemRow);

        UiStyleClass merchantPrice;
        merchantPrice.base.foregroundColor = {1.0f, 0.88f, 0.34f, 1.0f};
        theme.setStyleClass(VNThemeClass::MerchantPrice, merchantPrice);

        UiStyleClass merchantPane;
        merchantPane.base.backgroundColor = {0.045f, 0.058f, 0.052f, 0.94f};
        theme.setStyleClass(VNThemeClass::MerchantPlayerInventory, merchantPane);
        theme.setStyleClass(VNThemeClass::MerchantVendorInventory, merchantPane);

        UiStyleClass merchantActionButton;
        merchantActionButton.base.skin = profile.dialogueSkin.choice.normal;
        merchantActionButton.base.foregroundColor = {0.94f, 0.98f, 0.90f, 1.0f};
        if (profile.dialogueSkin.choice.hover)
            merchantActionButton.states[UiStyleState::Hover].skin = *profile.dialogueSkin.choice.hover;
        if (profile.dialogueSkin.choice.pressed)
            merchantActionButton.states[UiStyleState::Pressed].skin = *profile.dialogueSkin.choice.pressed;
        if (profile.dialogueSkin.choice.disabled)
            merchantActionButton.states[UiStyleState::Disabled].skin = *profile.dialogueSkin.choice.disabled;
        theme.setStyleClass(VNThemeClass::MerchantActionButton, merchantActionButton);

        UiStyleClass merchantDisabledItem;
        merchantDisabledItem.base.backgroundColor = {0.045f, 0.050f, 0.046f, 0.90f};
        merchantDisabledItem.base.foregroundColor = {0.46f, 0.50f, 0.45f, 1.0f};
        theme.setStyleClass(VNThemeClass::MerchantDisabledItem, merchantDisabledItem);

        UiStyleClass merchantHighlightedItem;
        merchantHighlightedItem.base.backgroundColor = {0.095f, 0.125f, 0.145f, 0.96f};
        theme.setStyleClass(VNThemeClass::MerchantHighlightedItem, merchantHighlightedItem);

        return theme;
    }
} // namespace gts::vn
