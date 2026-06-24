#pragma once

#include <string>

#include "UiPanelSkin.h"
#include "UiTypes.h"
#include "VNTypes.h"

namespace gts::vn
{
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
        UiRect dialogue = {0.060f, 0.720f, 0.880f, 0.220f};
        UiRect nameplate = {0.040f, -0.090f, 0.320f, 0.180f};
        UiRect speakerText = {0.060f, 0.0f, 0.880f, 1.0f};
        UiRect bodyText = {0.045f, 0.260f, 0.880f, 0.560f};
        UiRect continueIndicator = {0.920f, 0.800f, 0.050f, 0.140f};
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
} // namespace gts::vn
