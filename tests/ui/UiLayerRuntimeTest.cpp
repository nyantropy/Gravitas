#include <iostream>
#include <string>
#include <cstdlib>

#include "UiSystem.h"

namespace
{
    void require(bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "UI layer runtime test failed: " << message << std::endl;
            std::exit(1);
        }
    }

    UiLayoutSpec fillLayout()
    {
        UiLayoutSpec layout;
        layout.positionMode = UiPositionMode::Anchored;
        layout.widthMode = UiSizeMode::FromAnchors;
        layout.heightMode = UiSizeMode::FromAnchors;
        layout.anchorMin = {0.0f, 0.0f};
        layout.anchorMax = {1.0f, 1.0f};
        return layout;
    }

    UiHandle createHitRect(UiSystem& ui, UiHandle parent, UiColor color)
    {
        const UiHandle handle = ui.createNode(UiNodeType::Rect, parent);
        ui.setLayout(handle, fillLayout());
        ui.setState(handle, UiStateFlags{.visible = true, .enabled = true, .interactable = true});
        ui.setPayload(handle, UiRectData{color});
        return handle;
    }
}

int main()
{
    UiSystem ui(nullptr);

    const UiLayerId overlayLayer = ui.createLayer("overlay", 100);
    require(overlayLayer != UI_INVALID_LAYER, "overlay layer was not created");

    const UiHandle baseRect =
        createHitRect(ui, ui.getRoot(), UiColor{0.1f, 0.1f, 0.1f, 1.0f});
    const UiHandle overlayRoot = ui.getLayerRoot(overlayLayer);
    const UiHandle overlayRect =
        createHitRect(ui, overlayRoot, UiColor{0.9f, 0.9f, 0.9f, 1.0f});
    const UiHandle documentRoot = ui.getDocument().getDocumentRoot();
    require(ui.createNode(UiNodeType::Rect, documentRoot) == UI_INVALID_HANDLE,
            "document root accepted direct child creation");
    require(!ui.reparentNode(baseRect, documentRoot),
            "document root accepted reparented child");
    require(!ui.reparentNode(ui.getRoot(), overlayRoot),
            "layer root was reparented");

    UiInputFrame input;
    input.pointerX = 0.5f;
    input.pointerY = 0.5f;

    UiInteractionResult interaction = ui.updateInteraction(input);
    require(interaction.hovered == overlayRect, "higher-order layer did not win hit testing");

    require(ui.setLayerOrder(overlayLayer, -100), "layer order update failed");
    interaction = ui.updateInteraction(input);
    require(interaction.hovered == baseRect, "lower-order layer still won hit testing");

    require(ui.setLayerState(overlayLayer, UiLayerState{.visible = true, .inputEnabled = false}),
            "layer input disable failed");
    require(ui.setLayerOrder(overlayLayer, 100), "layer order restore failed");
    interaction = ui.updateInteraction(input);
    require(interaction.hovered == baseRect, "input-disabled layer still received hover");

    require(ui.removeLayer(overlayLayer), "overlay layer removal failed");
    require(ui.getLayerRoot(overlayLayer) == UI_INVALID_HANDLE, "removed layer still has a root");

    return 0;
}
