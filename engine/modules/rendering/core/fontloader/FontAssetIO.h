#pragma once

#include <string>

#include "FontAsset.h"

namespace gts::fonts
{
    bool loadFontAsset(const std::string& path, FontAsset& asset);
    bool saveFontAsset(const std::string& path, const FontAsset& asset);
} // namespace gts::fonts
