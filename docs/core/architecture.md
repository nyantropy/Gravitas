# Foundational Core Ownership

`gravitas_core` owns mechanisms needed independently of optional capabilities:

| Compartment | Responsibility |
| --- | --- |
| `ecs/` | Entity identity, storage, systems, execution and contexts |
| `command/`, `event/` | Engine requests, neutral/platform event delivery and subscription lifetime |
| `input/` | Raw input, bindings, sources and snapshots |
| `scene/` | Scene/world lifetime, registration and transitions |
| `module/` | Generic module lifecycle and service registry mechanics |
| `json/` | Generic JSON values, parsing and serialization |
| `time/`, `threading/`, `tween/` | Time values/settings, worker utilities and generic value transitions |
| `math/` | Central GLM compile configuration |
| `paths/` | Engine/project path resolution |
| `types/` | Generic enum-name utilities |

`ecs/world/EntityTypes.h` defines the unchanged `uint32_t` entity identity.
Mesh, texture, font, view and SSBO handles are feature contracts, defined only
in `modules/rendering/contracts/ResourceTypes.h`; their names and types are unchanged.

## Feature Targets

Arrows mean **depends on**:

```text
gravitas_rendering -> gravitas_ui -> gravitas_core
                    |              (no feature dependencies)
                    +-> gravitas_rendering_contracts
gravitas_rendering -> gravitas_rendering_contracts
gravitas_rendering -> gravitas_profiling
gravitas_physics -> gravitas_physics_contracts -> gravitas_core
                -> gravitas_transform -> gravitas_core
```

Resource contracts and profiling require only the C++ standard library. Physics
contracts require core entity identity. All three and retained UI are registered
in every module configuration; no new runtime option or composition policy is
introduced. `gravitas_modules` aggregates available targets for applications,
but leaf implementations and contract tests depend on their actual owners.

Retained UI implementation is compiled once into `gravitas_ui`. Rendering retains
its existing mixed UI facade/resource/extraction integration; UI does not link
rendering implementation. See [UI architecture](../ui/architecture.md).

## Deliberately Deferred Semantic Boundaries

This is physical/build ownership, not a redesign of engine composition. The
following existing feature awareness remains intentional for this stage:

- `EcsControllerContext` forward-declared module pointers and viewport data.
- `GtsScene` physics accessors and frame-statistics hooks. `GtsFrameStats` is
  forward-declared; implementations accessing fields include its diagnostics header.
- `SceneExecutionProfile` presets and `EcsSystemGroup` vocabulary.
- Screenshot commands and their existing request semantics.
- Rendering-side `UiSystem` and resource integration.
- Service discovery and optional runtime composition.

These can only be reconsidered by discussing their API/dependency semantics.
Core does not include or link feature contracts to preserve these signatures.

## Verification Boundaries

Core JSON/input tests run in reduced builds. `core_boundary`, `physics_contracts`,
`rendering_contracts` and `ui_retained_standalone` each link only their named leaf
target and reject visibility of unrelated implementation headers. The standalone
UI test exercises retained surfaces/documents, resource values, layout and reset
without `UiSystem`. `profile_accumulator` links only `gravitas_profiling`.
The core-only module smoke case configures, builds and runs all its available tests.
