#pragma once

#include <cstddef>
#include <limits>
#include <string>

#include "Entity.h"

namespace gts::tools
{
    enum class EngineToolSelectionSource
    {
        None,
        WorldPick
    };

    enum class EditorMode
    {
        Runtime
    };

    struct EngineToolStateComponent
    {
        bool visible = false;
        EditorMode editorMode = EditorMode::Runtime;
        Entity selectedEntity{std::numeric_limits<entity_id_type>::max()};
        Entity hoveredEntity{std::numeric_limits<entity_id_type>::max()};
        EngineToolSelectionSource selectionSource = EngineToolSelectionSource::None;
        bool selectionChangedThisFrame = false;
        std::string status = "F6 TO HIDE";
    };
}
