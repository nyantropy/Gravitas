#pragma once

#include <unordered_map>

#include "BitmapFont.h"
#include "IResourceProvider.hpp"
#include "UiCommand.h"
#include "UiDocument.h"
#include "UiRenderResolver.h"

// Engine-owned retained UI model plus render-side text/resource bindings.
class UiSystem
{
public:
    explicit UiSystem(IResourceProvider* resources);

    void clear();

    UiHandle getRoot() const;

    UiHandle createNode(UiNodeType type, UiHandle parent = UI_INVALID_HANDLE);
    bool     removeNode(UiHandle handle);
    bool     reparentNode(UiHandle handle, UiHandle newParent);

    UiNode*       findNode(UiHandle handle);
    const UiNode* findNode(UiHandle handle) const;

    bool setLayout(UiHandle handle, const UiLayoutSpec& layout);
    bool setStyle(UiHandle handle, const UiStyle& style);
    bool setState(UiHandle handle, const UiStateFlags& state);
    bool setPayload(UiHandle handle, const UiNodePayload& payload);
    bool setTextFont(UiHandle handle, BitmapFont* font);

    UiDocument&       getDocument();
    const UiDocument& getDocument() const;

    UiCommandBuffer extractCommands(float viewportAspect);

private:
    void removeTextBindingsRecursive(UiHandle handle);

    IResourceProvider*                         resources = nullptr;
    UiDocument                                 document;
    UiRenderResolver                           resolver;
    std::unordered_map<UiHandle, BitmapFont*>  textBindings;
};
