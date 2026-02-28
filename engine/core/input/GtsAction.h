#pragma once

// Engine-level semantic actions shared across applications.
// Application-specific actions belong in their own project-local enums
// that are passed as the template argument to InputActionManager<>.
enum class GtsAction
{
    // ─── camera / navigation ───────────────────────────────────────────
    ZoomIn = 0,
    ZoomOut,
    OrbitLeft,
    OrbitRight,

    // ─── engine / UI ───────────────────────────────────────────────────
    Pause,
    Resume,

    // sentinel — always keep last
    ACTION_COUNT
};
