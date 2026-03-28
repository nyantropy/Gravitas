#pragma once

#include <vector>
#include <unordered_map>

#include "UiHandle.h"
#include "UiImageDesc.h"
#include "UiTextDesc.h"
#include "UiImageElement.h"
#include "UiTextElement.h"
#include "UiCommand.h"
#include "IResourceProvider.hpp"

// Retained UI element tree.
// Owned by GravitasEngine, accessible from scenes via SceneContext::ui.
// Does not use the ECS — UI elements are stored in their own data structure.
//
// Usage:
//   UiHandle h = ctx.ui->addImage({.texturePath="...", .x=0.1f, .y=0.1f});
//   ctx.ui->update(h, {.text="HP: 100"});  // update text element
//   ctx.ui->remove(h);
//   ctx.ui->clear();                       // called between scene loads
class UiTree
{
public:
    explicit UiTree(IResourceProvider* resources);

    // Add a new image element. Returns a handle for future updates/removal.
    UiHandle addImage(const UiImageDesc& desc);

    // Add a new text element. Returns a handle for future updates/removal.
    UiHandle addText(const UiTextDesc& desc);

    // Update an existing image element.
    void update(UiHandle handle, const UiImageDesc& desc);

    // Update an existing text element.
    void update(UiHandle handle, const UiTextDesc& desc);

    // Remove an element by handle.
    void remove(UiHandle handle);

    // Remove all elements. Call between scene loads.
    void clear();

    // Resolve textures and build a UiCommandBuffer for this frame.
    // Called by UiCommandExtractor each frame.
    UiCommandBuffer buildCommandBuffer(float viewportAspect);

private:
    IResourceProvider* resources  = nullptr;
    UiHandle           nextHandle = 1; // 0 is UI_INVALID_HANDLE

    // Store images and text in separate vectors for cache-friendly iteration
    std::vector<UiImageElement> images;
    std::vector<UiTextElement>  texts;

    // Map handle → index in the appropriate vector for O(1) lookup
    std::unordered_map<UiHandle, size_t> imageIndex;
    std::unordered_map<UiHandle, size_t> textIndex;

    UiHandle allocHandle() { return nextHandle++; }
};
