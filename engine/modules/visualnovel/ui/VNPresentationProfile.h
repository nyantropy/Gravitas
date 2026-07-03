#pragma once

#include <algorithm>
#include <string>

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
        // Compatibility layout seeds for existing VN content. These rectangles
        // do not all share identical semantics in the frontend: some describe
        // panel regions, some describe child slots, and some seed overlay
        // alignment. New VN layout work should prefer semantic theme/profile
        // metrics such as panel insets, content padding, choice width/gap, name
        // height, and continue alignment instead of adding more raw rects.
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

    struct VNPresentationProfile
    {
        VNDialogueBoxSkin dialogueSkin;
        VNLayoutProfile layout;
        VNSpriteMotionProfile motionProfile;
    };

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
        if (parentTheme != nullptr)
            theme.setParent(*parentTheme);

        theme.setSkin("VN.Dialogue.PanelShadow", profile.dialogueSkin.panelShadow);
        theme.setSkin("VN.Dialogue.Panel", profile.dialogueSkin.panel);
        theme.setSkin("VN.Dialogue.Nameplate", profile.dialogueSkin.nameplate);

        theme.setTypography("VN.Speaker", UiTypography{.scale = profile.dialogueSkin.speakerTextScale});
        theme.setTypography("VN.Body", UiTypography{.scale = profile.dialogueSkin.bodyTextScale});
        theme.setTypography("VN.Choice", UiTypography{.scale = profile.dialogueSkin.choiceTextScale});
        theme.setTypography("VN.Continue", UiTypography{.scale = profile.dialogueSkin.bodyTextScale});

        const float choiceGap = profile.layout.choiceRowGap * profile.layout.choices.height;
        const float choiceButtonHeight = profile.layout.maxChoices == 0
            ? 0.044f
            : std::max(0.0f,
                       (profile.layout.choices.height
                        - choiceGap * static_cast<float>(std::max<size_t>(1, profile.layout.maxChoices) - 1))
                           / static_cast<float>(std::max<size_t>(1, profile.layout.maxChoices)));
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
