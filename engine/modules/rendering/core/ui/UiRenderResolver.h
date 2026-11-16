#pragma once

#include <string>
#include <unordered_map>

#include "BitmapFont.h"
#include "IUiResourceProvider.h"
#include "UiCommand.h"
#include "UiDocument.h"

class UiRenderResolver
{
public:
    void buildCommandBuffer(
        UiCommandBuffer& buffer,
        const UiVisualList& visualList,
        IUiResourceProvider* resources,
        const std::unordered_map<UiHandle, BitmapFont*>& textBindings,
        int viewportWidth,
        int viewportHeight) const;
};
