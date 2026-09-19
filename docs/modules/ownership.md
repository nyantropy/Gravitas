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

## Feature-Neutral Core Boundaries

Screenshot semantics belong to `gravitas_rendering_command_contracts`, which depends
only on core command transport. Rendering handles its typed extension payload at the
existing pre-render drain point; core neither names nor routes capture behavior.
The original stdlib-only rendering resource contract target stays independent of core.

Raw input mutation goes through core's five-operation `InputWriter`. The runtime
platform owns that borrowed writer alongside the input manager and calls it from its
existing event subscriptions. Core input grants no concrete runtime friendship.
Neither boundary requires a service registry, a second command transport or a new
layer exception. The CMake boundary checker remains unchanged.

Execution ownership is fully separated: runtime supplies opaque defaults/groups,
prepared VN selections, the rendering mode selector and benchmark label function.
Modules neither construct Gravitas defaults nor import runtime policy. Core execution
and the existing boundary checker are unchanged. See [execution policy](../execution/architecture.md).
Other core references to feature systems are explanatory comments, not types,
storage, includes or scheduling vocabulary.

`GtsFrameEndedEvent` was a clean extraction: the rendering-owned event carries the
same `dt` and `imageIndex`, is still emitted at the same Vulkan point, and uses the
unchanged generic core event bus.

## Public Include Surfaces

Targets export their owned leaf directories. The engine source root and the complete
`modules/` directory are never public include roots. Model, asset and animation
consumers include headers through the owning target's exported directory, for example
`GtsModelControllerContext.h`, `ModelFrameExtraction.h` and `GtsStaticVertex.h`.
The previous `model/...` and `assets/...` spellings depended on exporting every module;
no forwarding headers or generated header mirrors preserve that incidental access.

`gravitas_model_controller_contracts` exports `model/public/` and core only. Model
phases export their own directories and obtain other phases through declared links.
`gravitas_model_frontend` publicly depends on `gravitas_transform` because its ECS
extraction header consumes `WorldTransformComponent`. Model material realization
consumes the material-association contract owned by `gravitas_model_instances`.

`gravitas_asset_contracts` owns the existing geometry/material header directories.
Cooked assets export serialization, cooked loading and the shared loading-policy
folder; image decoding exports only its image API, with stb private. Material frontend
explicitly consumes image decoding for the image values in `IResourceProvider`.

`gravitas_rendering_window_contracts` owns the existing output-window and startup/
presentation-settings headers and depends on core. Both generic rendering and Vulkan
setup consume it. Vulkan setup exports only its own setup directories, with required
core/windowing declarations arriving through this contract dependency.

The generic rendering root is not public because it would expose `backend/` as well.
Its existing descriptor, integration and header-only system directories remain public
where application/module consumers require them. Vulkan backend implementation include
directories and dependencies are PRIVATE. Runtime/application consumers receive the
backend's link implementation without its header search paths. Runtime mesh loading is
a private backend dependency, not a generic-rendering export. The dungeon HUD uses its
own font-scale value and does not consume backend debug-overlay declarations. Backend-internal tests
explicitly request the backend's private include requirements and setup/rendering targets.

The source layout is unchanged. Directory-level exports still expose neighboring headers
inside an owned leaf: model public request/controller headers, loading policy alongside
its nested loader folders, material/provider declarations, and rendering's mixed UI and
window-manager integration. Removing that remaining intra-feature granularity would
require separate public-header/facade work; it is not an excuse to export sibling modules.

`GravitasPublicIncludes.cmake` supplements, without changing, the three-layer checker.
It rejects public source/module roots, ancestor exports and the backend-containing
rendering root after normalizing conventional build-interface paths. It does not attempt
to evaluate arbitrary generator expressions or provide a filesystem security boundary.
Standalone compile probes link one subject target, include its intended API, and reject
unrelated runtime/module/backend header visibility with `__has_include`. Fixtures cover
allowed leaves and rejected broad/normalized/wrapped roots.
