# Application tooling contracts

`gravitas_tool_contracts` is a compiled C++20 target depending only on
`gravitas_core`. It exists even when tools, rendering, physics and debug drawing
are disabled. Its public headers include no panels, tool runtime, GPU companions,
window handles or resource caches. Implementations are paired source files.

| Header | Supported operation | Lifetime/policy |
| --- | --- | --- |
| `ToolEntityLabelComponent.h` | Author name, category and selectable flag | World-owned optional metadata; game rules do not read selection |
| `EngineToolQueries.h` | `engineToolsVisibility(const ECSWorld&)` | Call-scoped const borrow; owned `optional<bool>`: absent, hidden or visible |
| `ToolCameraHandoff.h` | `isToolCameraActive`, `replaceToolCameraReturnTarget` | Live same-world camera identities; no cached service/context |
| `ToolLaunchPreset.h` | Owned startup/screenshot options and preset loader | Integration configures the engine; loading has no runtime effects |
| `ToolWorkspace.h` | Workspace identifiers and existing stable aliases | Owned values |
| `ToolSettings.h` | Compile-independent startup enablement | Existing configuration behavior |

Visibility observation does not install tools or retain a world reference. Callers
choose their fallback deliberately: ordinary HUD/menu callers treat absence as
not visible; standalone developer overlays may remain visible without a toolkit.

Camera handoff replaces the return target only while a tool camera is active and
its current target matches the retiring camera (or is invalid). Missing/inactive
state is a no-op. The caller supplies same-world identities and keeps a replacement
camera live until restoration. The camera system still writes descriptors and
marks authoritative transforms dirty; gizmos author parent-local transforms and
use the same resolver. Selection/gizmo/control components are engine-private.
Scene reset clears scene identities; no borrowed camera/component pointer crosses
world teardown.

`ToolStartupOptions::hasAnyToolState` is compiled out of line. Preset loading retains
workspace aliases, defaults, optional type behavior, source-root path fallback,
error/non-mutation behavior and schema. Loader parsing uses shared core JSON;
applying a preset is a separate runtime/integration operation. Screenshot timing,
render capture, asynchronous writes and quit-after-drain behavior are unchanged;
see [presets](presets.md). This API adds neither a second capture implementation
nor a telemetry framework.

Engine tests may explicitly include private directories for their own camera,
selection, gizmo and preview subjects. Such test access does not publish those
headers from `gravitas_tools`. Applications should never link that implementation
merely to obtain metadata; game build enforcement rejects direct implementation
links, private imports and private include exports. Runtime composition remains
engine-owned.
