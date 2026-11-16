#pragma once

#include <array>
#include "types/EnumName.h"

namespace gts::tools
{
    enum class ToolWorkspace
    {
        World,
        Particles,
        Assets
    };

    inline constexpr std::array toolWorkspaceNames{
        gts::EnumName{ToolWorkspace::World, "world"},
        gts::EnumName{ToolWorkspace::Particles, "particles"},
        gts::EnumName{ToolWorkspace::Assets, "assets"},
        gts::EnumName{ToolWorkspace::World, "world_viewer"},
        gts::EnumName{ToolWorkspace::World, "viewer"},
        gts::EnumName{ToolWorkspace::Particles, "particle"},
        gts::EnumName{ToolWorkspace::Particles, "particle_editor"},
        gts::EnumName{ToolWorkspace::Assets, "asset"},
        gts::EnumName{ToolWorkspace::Assets, "asset_browser"}
    };
} // namespace gts::tools
