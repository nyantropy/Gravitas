#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

#include "UiSystem.h"

namespace
{
    void require(bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "UI layout runtime test failed: " << message << std::endl;
            std::exit(1);
        }
    }

    bool near(float lhs, float rhs, float epsilon = 0.001f)
    {
        return std::fabs(lhs - rhs) <= epsilon;
    }

    UiLayoutLength normalized(float value)
    {
        return UiLayoutLength{UiLayoutUnit::Normalized, value};
    }

    UiLayoutLength percent(float value)
    {
        return UiLayoutLength{UiLayoutUnit::Percent, value};
    }

    UiLayoutLength content()
    {
        return UiLayoutLength{UiLayoutUnit::Content, 0.0f};
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

    UiLayoutSpec fixedLayout(float width, float height)
    {
        UiLayoutSpec layout;
        if (width > 0.0f)
            layout.constraints.preferredWidth = normalized(width);
        if (height > 0.0f)
            layout.constraints.preferredHeight = normalized(height);
        return layout;
    }

    UiHandle createRect(UiSystem& ui, UiHandle parent, const UiLayoutSpec& layout)
    {
        const UiHandle handle = ui.createNode(UiNodeType::Rect, parent);
        ui.setLayout(handle, layout);
        ui.setPayload(handle, UiRectData{UiColor{1.0f, 1.0f, 1.0f, 1.0f}});
        return handle;
    }

    const UiRect& bounds(UiSystem& ui, UiHandle handle)
    {
        const UiNode* node = ui.findNode(handle);
        require(node != nullptr, "node missing");
        return node->computedLayout.bounds;
    }

    void extract(UiSystem& ui)
    {
        ui.extractCommandsRef(800, 600);
    }

    void testStackLayout()
    {
        UiSystem ui(nullptr);
        UiLayoutSpec stack = fillLayout();
        stack.layoutMode = UiLayoutMode::Stack;
        stack.stackAxis = UiLayoutAxis::Vertical;
        stack.gap = 0.05f;
        const UiHandle root = createRect(ui, ui.getRoot(), stack);

        const UiHandle first = createRect(ui, root, fixedLayout(0.0f, 0.20f));
        const UiHandle second = createRect(ui, root, fixedLayout(0.0f, 0.30f));

        extract(ui);

        require(near(bounds(ui, first).x, 0.0f), "stack first x");
        require(near(bounds(ui, first).y, 0.0f), "stack first y");
        require(near(bounds(ui, first).width, 1.0f), "stack first stretch width");
        require(near(bounds(ui, first).height, 0.20f), "stack first height");
        require(near(bounds(ui, second).y, 0.25f), "stack second y includes gap");
        require(near(bounds(ui, second).height, 0.30f), "stack second height");
    }

    void testGridLayout()
    {
        UiSystem ui(nullptr);
        UiLayoutSpec grid = fillLayout();
        grid.layoutMode = UiLayoutMode::Grid;
        grid.gridColumns = 2;
        grid.gridRows = 2;
        grid.gridColumnGap = 0.02f;
        grid.gridRowGap = 0.04f;
        const UiHandle root = createRect(ui, ui.getRoot(), grid);

        UiLayoutSpec item;
        item.gridColumn = 1;
        item.gridRow = 1;
        const UiHandle child = createRect(ui, root, item);

        extract(ui);

        require(near(bounds(ui, child).x, 0.51f), "grid child x");
        require(near(bounds(ui, child).y, 0.52f), "grid child y");
        require(near(bounds(ui, child).width, 0.49f), "grid child width");
        require(near(bounds(ui, child).height, 0.48f), "grid child height");
    }

    void testDockLayout()
    {
        UiSystem ui(nullptr);
        UiLayoutSpec dock = fillLayout();
        dock.layoutMode = UiLayoutMode::Dock;
        dock.gap = 0.01f;
        const UiHandle root = createRect(ui, ui.getRoot(), dock);

        UiLayoutSpec top = fixedLayout(0.0f, 0.20f);
        top.dock = UiDockEdge::Top;
        const UiHandle topNode = createRect(ui, root, top);

        UiLayoutSpec left = fixedLayout(0.25f, 0.0f);
        left.dock = UiDockEdge::Left;
        const UiHandle leftNode = createRect(ui, root, left);

        UiLayoutSpec fill;
        fill.dock = UiDockEdge::Fill;
        const UiHandle fillNode = createRect(ui, root, fill);

        extract(ui);

        require(near(bounds(ui, topNode).height, 0.20f), "dock top height");
        require(near(bounds(ui, leftNode).x, 0.0f), "dock left x");
        require(near(bounds(ui, leftNode).y, 0.21f), "dock left y after top gap");
        require(near(bounds(ui, leftNode).width, 0.25f), "dock left width");
        require(near(bounds(ui, fillNode).x, 0.26f), "dock fill x after left gap");
        require(near(bounds(ui, fillNode).y, 0.21f), "dock fill y");
    }

    void testOverlayConstraintAndAspect()
    {
        UiSystem ui(nullptr);
        UiLayoutSpec overlay = fillLayout();
        overlay.layoutMode = UiLayoutMode::Overlay;
        const UiHandle root = createRect(ui, ui.getRoot(), overlay);

        UiLayoutSpec centered = fixedLayout(0.40f, 0.20f);
        centered.constraints.horizontalAlignment = UiLayoutAlignment::Center;
        centered.constraints.verticalAlignment = UiLayoutAlignment::End;
        const UiHandle panel = createRect(ui, root, centered);

        UiLayoutSpec aspect = fixedLayout(1.0f, 1.0f);
        aspect.constraints.aspectRatio = 2.0f;
        aspect.constraints.horizontalAlignment = UiLayoutAlignment::Center;
        aspect.constraints.verticalAlignment = UiLayoutAlignment::Center;
        const UiHandle aspectNode = createRect(ui, root, aspect);

        extract(ui);

        require(near(bounds(ui, panel).x, 0.30f), "overlay centered x");
        require(near(bounds(ui, panel).y, 0.80f), "overlay end y");
        require(near(bounds(ui, aspectNode).height, 0.50f), "aspect height");
        require(near(bounds(ui, aspectNode).y, 0.25f), "aspect centered y");
    }

    void testScrollContainer()
    {
        UiSystem ui(nullptr);
        UiLayoutSpec scroll;
        scroll.positionMode = UiPositionMode::Absolute;
        scroll.widthMode = UiSizeMode::Fixed;
        scroll.heightMode = UiSizeMode::Fixed;
        scroll.fixedWidth = 0.50f;
        scroll.fixedHeight = 0.50f;
        scroll.layoutMode = UiLayoutMode::Scroll;
        scroll.contentOffset = {0.0f, -0.20f};
        const UiHandle root = createRect(ui, ui.getRoot(), scroll);

        UiLayoutSpec child;
        child.positionMode = UiPositionMode::Absolute;
        child.widthMode = UiSizeMode::Fixed;
        child.heightMode = UiSizeMode::Fixed;
        child.offsetMin = {0.0f, 0.40f};
        child.fixedWidth = 0.40f;
        child.fixedHeight = 0.20f;
        const UiHandle item = createRect(ui, root, child);

        extract(ui);

        require(near(bounds(ui, item).y, 0.20f), "scroll content offset");
        const UiNode* rootNode = ui.findNode(root);
        require(rootNode != nullptr && near(rootNode->computedLayout.clipRect.width, 0.50f),
                "scroll clipped width");
        require(rootNode != nullptr && near(rootNode->computedLayout.clipRect.height, 0.50f),
                "scroll clipped height");
    }

    void testTextMeasurementAndInvalidation()
    {
        UiSystem ui(nullptr);
        UiLayoutSpec overlay = fillLayout();
        overlay.layoutMode = UiLayoutMode::Overlay;
        const UiHandle root = createRect(ui, ui.getRoot(), overlay);

        UiLayoutSpec textLayout;
        textLayout.constraints.preferredWidth = content();
        textLayout.constraints.preferredHeight = content();
        textLayout.constraints.horizontalAlignment = UiLayoutAlignment::Start;
        textLayout.constraints.verticalAlignment = UiLayoutAlignment::Start;
        const UiHandle text = ui.createNode(UiNodeType::Text, root);
        ui.setLayout(text, textLayout);
        ui.setPayload(text, UiTextData{"short", {}, UiColor{1.0f, 1.0f, 1.0f, 1.0f}, 0.02f});

        extract(ui);
        const float shortWidth = bounds(ui, text).width;

        ui.setPayload(text, UiTextData{"a much longer measured string", {}, UiColor{1.0f, 1.0f, 1.0f, 1.0f}, 0.02f});
        extract(ui);

        require(bounds(ui, text).width > shortWidth, "text payload did not invalidate measured layout");
    }

    void testNestedLayoutAndPercentConstraint()
    {
        UiSystem ui(nullptr);
        UiLayoutSpec outer = fillLayout();
        outer.layoutMode = UiLayoutMode::Stack;
        outer.stackAxis = UiLayoutAxis::Horizontal;
        const UiHandle root = createRect(ui, ui.getRoot(), outer);

        UiLayoutSpec inner = fixedLayout(0.50f, 0.0f);
        inner.layoutMode = UiLayoutMode::Stack;
        inner.stackAxis = UiLayoutAxis::Vertical;
        const UiHandle innerNode = createRect(ui, root, inner);

        UiLayoutSpec child;
        child.constraints.preferredWidth = percent(0.50f);
        child.constraints.preferredHeight = normalized(0.25f);
        child.constraints.horizontalAlignment = UiLayoutAlignment::Start;
        const UiHandle item = createRect(ui, innerNode, child);

        extract(ui);

        require(near(bounds(ui, innerNode).width, 0.50f), "nested inner width");
        require(near(bounds(ui, item).width, 0.25f), "nested percent child width");
        require(near(bounds(ui, item).height, 0.25f), "nested child height");
    }

    void testAnchoredCompatibility()
    {
        UiSystem ui(nullptr);
        UiLayoutSpec layout;
        layout.positionMode = UiPositionMode::Anchored;
        layout.widthMode = UiSizeMode::FromAnchors;
        layout.heightMode = UiSizeMode::FromAnchors;
        layout.anchorMin = {0.25f, 0.25f};
        layout.anchorMax = {0.75f, 0.50f};
        const UiHandle child = createRect(ui, ui.getRoot(), layout);

        extract(ui);

        require(near(bounds(ui, child).x, 0.25f), "anchored compatibility x");
        require(near(bounds(ui, child).y, 0.25f), "anchored compatibility y");
        require(near(bounds(ui, child).width, 0.50f), "anchored compatibility width");
        require(near(bounds(ui, child).height, 0.25f), "anchored compatibility height");
    }

    void testSurfaceLocalLayoutExtraction()
    {
        UiSystem ui(nullptr);
        UiSurfaceDesc desc;
        desc.name = "right";
        desc.rect = {0.5f, 0.0f, 0.5f, 1.0f};
        desc.order = 50;
        const UiSurfaceId surface = ui.createSurface(desc);

        UiLayoutSpec overlay = fillLayout();
        overlay.layoutMode = UiLayoutMode::Overlay;
        const UiHandle root = createRect(ui, ui.getRoot(surface), overlay);
        const UiHandle child = createRect(ui, root, fixedLayout(0.50f, 0.50f));
        extract(ui);

        require(near(bounds(ui, child).width, 0.50f), "surface-local layout width");

        const UiCommandBuffer& commands = ui.extractCommandsRef(800, 600);
        bool sawRightSurfaceVertex = false;
        for (const UiVertex& vertex : commands.vertices)
        {
            if (vertex.pos.x >= 0.75f)
                sawRightSurfaceVertex = true;
        }
        require(sawRightSurfaceVertex, "surface extraction did not apply layout through surface rect");
    }
}

int main()
{
    testStackLayout();
    testGridLayout();
    testDockLayout();
    testOverlayConstraintAndAspect();
    testScrollContainer();
    testTextMeasurementAndInvalidation();
    testNestedLayoutAndPercentConstraint();
    testAnchoredCompatibility();
    testSurfaceLocalLayoutExtraction();
    return 0;
}
