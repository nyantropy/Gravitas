# Render readiness observation API

`RenderableRuntimeStatus.h` declares the value result and
`renderableRuntimeStatus(const ECSWorld&, Entity)`. Its paired implementation is
owned by `gravitas_rendering`. The declaration includes only the entity contract;
it does not expose GPU companion layouts, material registries or resource providers.

The call borrows a live world/entity for its duration and returns four booleans:
mesh GPU readiness, material GPU readiness, render-object allocation and render
readiness. Unknown/missing components report false. Material readiness follows
the renderer's existing fallback-handle policy when a runtime already exists.

Observation never creates a world material runtime, registers its lifetime,
queues synchronization, allocates resources or changes components. It reads only
existing renderer-owned state. The result is a point-in-time diagnostic; do not
cache it as an authoritative gameplay condition or use its counters to gate rules.

`ActiveCameraViewStateComponent.h` is the separate read-only camera projection
snapshot. It exposes copied matrices/validity and a same-world camera identity,
without permitting writes to camera GPU companions. See the [camera API](camera-api.md).

Game presentation may aggregate these values into its own diagnostic record, but
that record must stay separate from authoritative gameplay state. Missing scene
inputs must clear aggregates rather than leaving stale readiness counts. Tool
visibility/selection and visual-debug overrides are likewise observation or
presentation policy, never prerequisites for mechanics.
