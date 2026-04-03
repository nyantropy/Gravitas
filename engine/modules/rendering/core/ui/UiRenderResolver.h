#pragma once

#include <string>
#include <unordered_map>

#include "BitmapFont.h"
#include "IResourceProvider.hpp"
#include "UiCommand.h"
#include "UiDocument.h"

class UiRenderResolver
{
public:
    UiCommandBuffer buildCommandBuffer(
        const UiVisualList& visualList,
        IResourceProvider* resources,
        const std::unordered_map<UiHandle, BitmapFont*>& textBindings,
        int viewportWidth,
        int viewportHeight) const;
};
