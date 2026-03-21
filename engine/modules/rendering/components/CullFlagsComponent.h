#pragma once

// Optional per-entity culling behaviour overrides.
// Attach to any entity that needs non-default culling behaviour.
//
// neverCull = true  : entity always passes the cull test regardless of
//                     frustum position. Only meaningful when the global
//                     frustumCullingEnabled flag is true — if culling is
//                     globally disabled, all entities pass regardless.
// cullEnabled = false : opt this specific entity out of culling even when
//                       the global frustumCullingEnabled flag is true.
struct CullFlagsComponent
{
    bool neverCull   = false;
    bool cullEnabled = true;
};
