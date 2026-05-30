#pragma once

#include <limits>

#include "Entity.h"
#include "GlmConfig.h"

namespace gts::tools
{
    enum class EngineGizmoSpace
    {
        World,
        Local
    };

    enum class EngineGizmoAxis
    {
        None,
        X,
        Y,
        Z
    };

    struct EngineGizmoStateComponent
    {
        bool enabled = true;
        EngineGizmoSpace space = EngineGizmoSpace::World;
        bool snapEnabled = false;
        float snapStep = 0.25f;
        float handleLength = 1.35f;
        float pickRadius = 0.12f;

        EngineGizmoAxis hoveredAxis = EngineGizmoAxis::None;
        EngineGizmoAxis activeAxis = EngineGizmoAxis::None;
        Entity activeEntity{std::numeric_limits<entity_id_type>::max()};

        glm::vec3 dragAxisWorld{1.0f, 0.0f, 0.0f};
        glm::vec3 dragPlaneNormal{0.0f, 0.0f, 1.0f};
        glm::vec3 dragStartHit{0.0f};
        glm::vec3 dragStartPosition{0.0f};
    };
}
