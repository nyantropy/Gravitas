#include "UiTree.h"
#include "GlyphLayoutEngine.h"

// ── constructor ───────────────────────────────────────────────────────────────

UiTree::UiTree(IResourceProvider* resources)
    : resources(resources)
{}

// ── addImage ─────────────────────────────────────────────────────────────────

UiHandle UiTree::addImage(const UiImageDesc& desc)
{
    UiHandle handle = allocHandle();

    UiImageElement elem;
    elem.handle      = handle;
    elem.type        = UiElementType::Image;
    elem.visible     = desc.visible;
    elem.texturePath = desc.texturePath;
    elem.x           = desc.x;
    elem.y           = desc.y;
    elem.width       = desc.width;
    elem.imageAspect = desc.imageAspect;
    elem.tint        = desc.tint;

    imageIndex[handle] = images.size();
    images.push_back(std::move(elem));
    return handle;
}

// ── addText ───────────────────────────────────────────────────────────────────

UiHandle UiTree::addText(const UiTextDesc& desc)
{
    UiHandle handle = allocHandle();

    UiTextElement elem;
    elem.handle  = handle;
    elem.type    = UiElementType::Text;
    elem.visible = desc.visible;
    elem.text    = desc.text;
    elem.font    = desc.font;
    elem.x       = desc.x;
    elem.y       = desc.y;
    elem.scale   = desc.scale;
    elem.color   = desc.color;

    textIndex[handle] = texts.size();
    texts.push_back(std::move(elem));
    return handle;
}

// ── update (image) ────────────────────────────────────────────────────────────

void UiTree::update(UiHandle handle, const UiImageDesc& desc)
{
    auto it = imageIndex.find(handle);
    if (it == imageIndex.end()) return;

    auto& img = images[it->second];

    // If texture path changed, clear bound path so buildCommandBuffer re-resolves
    if (desc.texturePath != img.texturePath)
        img.boundTexturePath.clear();

    img.visible     = desc.visible;
    img.texturePath = desc.texturePath;
    img.x           = desc.x;
    img.y           = desc.y;
    img.width       = desc.width;
    img.imageAspect = desc.imageAspect;
    img.tint        = desc.tint;
}

// ── update (text) ─────────────────────────────────────────────────────────────

void UiTree::update(UiHandle handle, const UiTextDesc& desc)
{
    auto it = textIndex.find(handle);
    if (it == textIndex.end()) return;

    auto& txt  = texts[it->second];
    txt.visible = desc.visible;
    txt.text    = desc.text;
    txt.font    = desc.font;
    txt.x       = desc.x;
    txt.y       = desc.y;
    txt.scale   = desc.scale;
    txt.color   = desc.color;
}

// ── remove ────────────────────────────────────────────────────────────────────

void UiTree::remove(UiHandle handle)
{
    if (auto it = imageIndex.find(handle); it != imageIndex.end())
    {
        size_t idx = it->second;
        if (idx != images.size() - 1)
        {
            std::swap(images[idx], images.back());
            imageIndex[images[idx].handle] = idx;
        }
        images.pop_back();
        imageIndex.erase(it);
        return;
    }

    if (auto it = textIndex.find(handle); it != textIndex.end())
    {
        size_t idx = it->second;
        if (idx != texts.size() - 1)
        {
            std::swap(texts[idx], texts.back());
            textIndex[texts[idx].handle] = idx;
        }
        texts.pop_back();
        textIndex.erase(it);
    }
}

// ── clear ─────────────────────────────────────────────────────────────────────

void UiTree::clear()
{
    images.clear();
    texts.clear();
    imageIndex.clear();
    textIndex.clear();
    nextHandle = 1;
}

// ── buildCommandBuffer ────────────────────────────────────────────────────────

UiCommandBuffer UiTree::buildCommandBuffer(float viewportAspect)
{
    UiCommandBuffer buffer;

    // Images
    for (auto& img : images)
    {
        if (!img.visible || img.texturePath.empty()) continue;

        // Resolve texture if path changed
        if (img.texturePath != img.boundTexturePath)
        {
            img.textureID        = resources->requestTexture(img.texturePath);
            img.boundTexturePath = img.texturePath;
        }
        if (img.textureID == 0) continue;

        float h = (img.width * viewportAspect) / img.imageAspect;
        buffer.addTexturedQuad(img.x, img.y, img.width, h, img.textureID, img.tint);
    }

    // Text
    for (auto& txt : texts)
    {
        if (!txt.visible || !txt.font || txt.text.empty()) continue;
        GlyphLayoutEngine::appendUiText(buffer, *txt.font,
                                        txt.text, txt.x, txt.y,
                                        txt.scale, txt.color);
    }

    return buffer;
}
