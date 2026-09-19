#pragma once

#include "EcsExecutionSelection.h"

namespace gts::execution::groups
{

    inline constexpr EcsSystemGroup Always     = static_cast<EcsSystemGroup>(1ull << 0);
    inline constexpr EcsSystemGroup Gameplay   = static_cast<EcsSystemGroup>(1ull << 1);
    inline constexpr EcsSystemGroup Physics    = static_cast<EcsSystemGroup>(1ull << 2);
    inline constexpr EcsSystemGroup Camera     = static_cast<EcsSystemGroup>(1ull << 3);
    inline constexpr EcsSystemGroup RenderPrep = static_cast<EcsSystemGroup>(1ull << 4);
    inline constexpr EcsSystemGroup Particles  = static_cast<EcsSystemGroup>(1ull << 5);
    inline constexpr EcsSystemGroup Animation  = static_cast<EcsSystemGroup>(1ull << 6);
    inline constexpr EcsSystemGroup Audio      = static_cast<EcsSystemGroup>(1ull << 7);
    inline constexpr EcsSystemGroup Ui         = static_cast<EcsSystemGroup>(1ull << 8);
    inline constexpr EcsSystemGroup Dialogue   = static_cast<EcsSystemGroup>(1ull << 9);
    inline constexpr EcsSystemGroup VN         = static_cast<EcsSystemGroup>(1ull << 10);
    inline constexpr EcsSystemGroup Tools      = static_cast<EcsSystemGroup>(1ull << 11);
} // namespace gts::execution::groups

inline const char* ecsSystemGroupName(EcsSystemGroup group)
{
    switch (group)
    {
    case gts::execution::groups::Always:
        return "Always";
    case gts::execution::groups::Gameplay:
        return "Gameplay";
    case gts::execution::groups::Physics:
        return "Physics";
    case gts::execution::groups::Camera:
        return "Camera";
    case gts::execution::groups::RenderPrep:
        return "RenderPrep";
    case gts::execution::groups::Particles:
        return "Particles";
    case gts::execution::groups::Animation:
        return "Animation";
    case gts::execution::groups::Audio:
        return "Audio";
    case gts::execution::groups::Ui:
        return "Ui";
    case gts::execution::groups::Dialogue:
        return "Dialogue";
    case gts::execution::groups::VN:
        return "VN";
    case gts::execution::groups::Tools:
        return "Tools";
    }
    return "Unknown";
}
