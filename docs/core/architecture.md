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

## Controller Execution Context

`EcsControllerContext` contains only the world, input registry, timing, engine
commands, scene catalog/current scene name and generic `ControllerFrameData`.
There are no named optional capabilities or viewport fields in core.

Module-owned controller contracts provide typed access to value-owned call data:
model registry/realization access, UI access, physics access, and rendering
resource/viewport access. `ControllerFrameData` only copies, reads and edits typed
values; it has no service registration, keys, factories or world/global lookup.
The concrete module types are absent from core. Missing payloads read as immutable
default values, preserving null optional pointers and default viewport metrics.

Frame data is attached to the call because tool and scene contexts for the same
world can have different viewport snapshots. Copying a context copies these values;
it does not share mutable data or extend the lifetime of borrowed service pointers.
Adding a payload preserves references to existing payloads. Owners populate data
before dispatch; controllers only read it. Payload references and borrowed services
must not be cached beyond the call. World clear/destruction never leaves frame data
in a registry because none is stored there.

The scene catalog and active scene name remain foundational lifecycle information.
They do not identify a feature capability. Engine commands, input and time retain
their existing nullable contracts for standalone callers/tests.

## Deliberately Deferred Semantic Boundaries

This is physical/build ownership, not a redesign of engine composition. The
following existing feature awareness remains intentional for this stage:

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
