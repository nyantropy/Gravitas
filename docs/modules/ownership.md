# Source Layers And Module Placement

Gravitas has three primary source directories under `engine/`:

| Layer | Owns | Does not own |
| --- | --- | --- |
| `core/` | Feature-neutral mechanisms: ECS, generic selection/filtering, input, scene/resource lifetime, event transport, commands, generic module hooks, JSON, time, threading, tween, math and paths | Concrete capabilities, engine composition or named feature execution categories |
| `modules/` | Rendering, physics, models, animation, retained UI, narrative, transforms, diagnostics, tools and shared assets | Gravitas startup, engine facade or rules coordinating all installed capabilities |
| `runtime/` | Gravitas facade, startup aggregate, platform and loop orchestration, engine controls, coordinated execution/presentation policy | Foundational mechanisms or capability implementations |

The placement test is:

1. Does it remain meaningful without knowing installed features? Put it in core.
2. Does it implement a concrete engine capability? Put it in modules.
3. Does it coordinate capabilities into Gravitas behavior? Put it in runtime.

Arrows below mean **depends on**:

```text
runtime -> modules -> core
runtime ------------> core

core -X-> modules
core -X-> runtime
modules -X-> runtime
```

Modules may consume explicitly owned contracts from other modules. Rendering
resource identities and physics access contracts are examples. Do not use the
application aggregate to obtain one missing header. A capability's local runtime
implementation still belongs in that capability, such as rendering frame extraction
or the VN interpreter; the word “runtime” does not make it engine composition.

## CMake Ownership

`gravitas_core` stays independently buildable. Leaf targets own their sources and
include directories. `gravitas_runtime` is the header-only composition target;
`gravitas_engine` is the application entry target above it. The runtime target uses
the selected-module aggregate for composition, not for satisfying a leaf dependency.
`gravitas_tool_contracts` exposes startup settings even with tools disabled.

The engine-root include directory is not exported. Runtime exports its own directory,
so applications still include `GravitasEngine.hpp` and `EngineConfig.h` through their
owning target without forwarding files. Module headers are resolved through module
targets. Core and feature consumers do not inherit the facade.

`cmake/GravitasSourceLayers.cmake` runs after configuration has assembled the complete
target graph. It rejects forbidden links, including transitive links through aliases
and interface wrappers, higher-layer include/source paths, and explicit/short header
includes of higher layers. CTest fixtures prove permitted graphs pass and forbidden
graphs fail. Core/module compile tests also reject runtime header visibility.

Checks enforce the current CMake/source graph; they do not infer semantic ownership
from arbitrary C++ identifiers or prove all possible generated build expressions.
Review remains necessary for feature vocabulary and API design.

## Explicit Migration Blockers

These are existing exceptions to conceptual ownership, not permission for new
upward build dependencies:

- **Execution policy:** `modules/execution/` still owns the fixed catalog and
  gameplay/pause recipes. Standalone transform, animation, debug-draw and renderer
  installers establish gameplay defaults; physics consumes transform installation;
  VN constructs coordinated overlay/fullscreen profiles; tools/previews choose
  categories and rendering benchmarks label them. Moving the files directly to
  runtime would create modules → runtime dependencies. The follow-up must first
  give these boundaries explicit policy inputs and supply identical defaults from
  runtime and standalone entry points. Rendering should consume a rendering-owned
  presentation contract, not the runtime policy. Preserve one selection stack and
  all existing bits, IDs, modes and ordering. Do not duplicate numeric policy into
  modules or move capability implementations wholesale merely to hide the edge.
- **Screenshot command:** core's closed `GtsCommand` variant and
  `GtsCommandBuffer::requestScreenshot` expose a rendering-specific request.
  Extraction requires migrating command producers/consumers to an owned extension
  contract or changing the command API. A file move alone cannot remove it.
- **Input platform friendship:** `InputManager` names `GtsPlatform` as a friend for
  private event injection and frame advancement. A feature-neutral writer boundary
  requires an input API decision. Core includes/links no runtime header or target.

The execution-policy relocation is therefore **not complete** under the structural,
API-preserving migration. It must not be claimed complete merely because the three
primary directories exist. Other core references to feature systems are explanatory
comments, not types, storage, includes or scheduling vocabulary.

`GtsFrameEndedEvent` was a clean extraction: the rendering-owned event carries the
same `dt` and `imageIndex`, is still emitted at the same Vulkan point, and uses the
unchanged generic core event bus.
