#pragma once

#include <optional>

class ECSWorld;

namespace gts::tools
{
    std::optional<bool> engineToolsVisibility(const ECSWorld& world);
}
