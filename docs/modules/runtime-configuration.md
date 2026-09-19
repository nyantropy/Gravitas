# Runtime configuration contract

Core is an independently buildable/testable foundation. Modules are independently
buildable/testable wherever their actual dependencies permit. `GravitasEngine` is the
complete supported engine composition, with rendering required and Vulkan as the
primary/default backend. It is not a facade over every arbitrary subset of modules.

## Required composition

The complete facade currently requires all of:

| Option | Required capability and reason |
|---|---|
| `GTS_ENABLE_RENDERING` | Graphics/platform services, retained UI integration, frame extraction/submission and model/resource access |
| `GTS_ENABLE_VULKAN_BACKEND` | The installed default provider and supported Vulkan startup path |
| `GTS_ENABLE_TOOLS` | The facade includes, constructs and invokes `EngineToolRuntime`; the runtime `tools.enabled` setting does not erase the type dependency |
| `GTS_ENABLE_DEBUGDRAW` | Tools' existing debug drawing dependency |
| `GTS_ENABLE_PHYSICS` | Tools' existing selection dependency; scene physics contract access remains unchanged |

All default to ON. Always-defined supporting targets (core, transform, model/assets,
retained UI, dialogue/VN when rendering is present, profiling, lightweight contracts and
execution policy) retain their existing ownership and dependencies.

`runtime/CMakeLists.txt` checks the five actual capability targets before defining
`gravitas_runtime`. If any is absent, it prints the missing list and omits the target.
The parent creates `gravitas_engine` and alias `GravitasEngine` only when the runtime
exists. No separate enable switch can advertise a partial facade.

`gravitas_runtime_execution` is intentionally still defined in reduced builds: tests
and applications can choose Gravitas policy values without the complete engine facade.
It exports only the execution compartment. Modules do not link it.

## Reduced builds

Reduced configurations support focused compilation, CPU regression tests, and work on
capabilities without unrelated platform/backend requirements. Existing dependency
validation remains intact; disabling a capability does not bypass another capability's
real dependency.

Examples that intentionally omit all three complete facade targets:

- All optional capability switches off: foundational/core and available CPU leaf tests.
- Physics without rendering: physics, transform and their contracts/tests.
- Rendering without Vulkan: rendering/retained UI, materials and other CPU rendering tests.
- Vulkan without tools: backend development and module tests, without the complete facade.
- Debug drawing with or without physics, without Vulkan/tools: independent generic drawing
  and, when both features exist, the physics bridge.
- Tools with all its dependencies but no Vulkan: tools/preview CPU development.

“Core-only” is the existing matrix case name; it disables optional switches and verifies
core isolation, but the project still declares its always-available CPU feature targets.
It does not imply those features became core dependencies.

Bundled facade applications are skipped if the complete runtime is absent, even when
`GTS_BUILD_TEST_SCENES` is ON. Full runtime smoke tests likewise require that target.
Leaf tests remain available. Application CMake should require `TARGET GravitasEngine`
before linking the facade, instead of expecting any arbitrary configuration to supply it.

## Vulkan composition

`runtime/rendering/GraphicsBackendInstaller.cpp` owns the existing default-installation
operation. It calls rendering's `installVulkanGraphicsBackend`, implemented alongside the
Vulkan provider. The provider remains a function-local static with the same borrowed
registry lifetime, replacement behavior and construction path. No Vulkan implementation
headers are exposed through runtime. Runtime links the backend privately; final executables
receive its link requirements without backend header search paths.

`GraphicsStartupOptions` still defaults to Vulkan. The platform, loop, settings, startup,
GPU synchronization, rendering and scheduling behavior are unchanged. This boundary does
not add backend selection machinery or make the facade conditional at C++ call sites.

## Verification contract

Configuration assertions independently check option-to-target availability for all three
facade names. A real consuming CMake fixture succeeds for the complete configuration and
is rejected for a missing facade in each reduced configuration. The complete facade compile/
link test checks Vulkan defaults, provider installation and stable provider identity;
headless runtime smoke tests exercise construction and running with tools enabled/disabled.
The module matrix builds leaf targets, runs their relevant tests, and checks facade absence
and include visibility. The existing three-layer checker is unchanged.
