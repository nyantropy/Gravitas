# Gravitas engine agent instructions

## Scope and documentation

This repository owns the reusable C++20 Gravitas engine. Read
[GravitasAPI.md](GravitasAPI.md) for supported consumer APIs, ownership, execution
order, errors and examples. It is the single maintained human-facing technical
reference; this file holds agent operating instructions. Update the relevant API
section whenever a public surface changes. Do not create architecture documents,
per-feature documentation trees, proposal/task records or extra instruction files.

Preserved READMEs can contain historical links. Use this file and `GravitasAPI.md`
for current technical contracts; do not rewrite a human README automatically.

Keep human-owned README files, asset/fixture notes and vendor documentation intact.
Do not edit READMEs. Preserve resources unless the user requests the exact removal.
Do not delete a human document to reach a file-count target. In a DungeonCrawler
checkout this repository is `engine/`, and its production tree is `engine/engine/`
relative to the game root. Commands/paths below are relative to this repository.

## Source layers and public API

- `engine/core/`: feature-neutral ECS, scenes/resources, events/commands, input,
  math, paths, JSON, time, execution-selection mechanisms and generic module hooks.
- `engine/modules/`: capabilities such as transform, model/assets, animation,
  physics, rendering, UI, narrative, diagnostics and tools. Each owns its contracts
  and implementation, including any capability-local runtime code.
- `engine/runtime/`: `GravitasEngine`, startup/platform/loop and coordinated
  execution policy across installed capabilities.
- `applications/`: validation scenes and benchmark applications. `tools/assetc/`
  owns the offline C++ cooker; `presets/tooling/` owns maintained launch inputs.
- `tests/` and `cmake/`: regression coverage and build/dependency enforcement.
  `external/` is vendor-owned. Assets and GLSL/SPIR-V live under `resources/` and
  `shaders/`; do not change them incidentally during cleanup.

Dependencies point runtime → modules/core and modules → core or another explicit
capability contract. Core never depends on modules/runtime; modules never depend
on runtime or an application. Keep concrete feature names/execution recipes out
of core. Leaf targets own exported include directories. Do not export parent
source roots, private/test-subject paths or aggregate links to hide an upward edge.
The existing CMake layer/publication/module checks must remain active.

The public API exposes authored values, capabilities and owned results. Keep GPU
companions, backend handles, queues/cache registries, command draining, result
completion and tool control private. Applications author descriptors/transforms;
rendering owns matrix generation, extraction, upload and fenced GPU lifetime.
Use established authoritative versions/dirty queues. World clear/destruction owns
cache teardown, including preview/CPU-only worlds; do not require consumer cleanup
of internal registries. Frame services are borrowed only for the current call.
Keep Vulkan creation, binding, uploads and destruction local to backend/resource
owners. Declare frame-graph reads, writes and layout changes accurately. Avoid
per-frame allocations unless bounded and justified. Do not move those details
into application-facing descriptors to simplify a renderer change.

Keep game-specific rules/content/schemas outside the engine. Do not add a universal
facade or duplicate an API for a hypothetical alternate engine. Full
`GravitasEngine` composition currently requires rendering, Vulkan, physics, debug
draw and tools; reduced configurations expose available leaf APIs instead.

Preserve these cross-feature contracts:

- Scenes own their worlds and scene resources; host sessions outlive scenes.
  Validate scene IDs before destructive teardown. Transition payloads own values;
  destination `onLoad` borrows them only for that call. No load-failure rollback
  is promised after teardown. Direct isolated scene destruction does not call
  virtual `onUnload`; the runtime coordinates unload/reset.
- ECS events dispatch synchronously on the engine thread; retain subscription
  tokens for the intended lifetime. Entity identities stay in their live world.
- Fixed-step simulation owns authoritative mechanics; controllers own frame
  presentation/input/tooling. Preserve registration order and command-flush
  boundaries. Simulation input edges remain latched until a fixed tick consumes
  them; controller edges are frame-local. Fixed timestep alone is not universal
  deterministic physics.
- Transform writers precede resolution and consumers. Physics resolves before
  collision queries. Authoritative versions drive renderer synchronization.
- Model handles retain immutable content; instances own independent playback and
  world-scoped materials. Clip references require their originating resource.
  Static cooked-v1 rejects skeletal/animated data rather than silently degrading it.
- Headless dialogue remains independent of VN/UI. UI owns retained state and
  acquires fonts/textures through `IUiResourceProvider`; broad backend allocation
  is not a UI capability. Destroy retained borrowers before scene-owned data.
- Commands are enqueue-only to consumers. Rendering requests drain before render;
  lifecycle/pause/quit afterward. Graphics receipts own const observations across
  scenes without retaining services; only private completion code changes them.
  Submission, saved preference and applied result remain separate.
- Tools use public labels/visibility/camera handoff. Selection/gizmos/previews
  remain private. Read-only diagnostics never determine application mechanics.

## C++ standards

- Use C++20, four spaces, Allman braces and `.clang-format`. Prefer clear early
  returns and readable lines. Do not reformat unrelated code.
- Use `PascalCase` types and `camelCase` functions/methods/variables; use precise
  names describing the operation. Components end in `Component`, systems in `System`.
  Use uppercase constants only where appropriate to existing conventions.
- Introduce methods/functions only for logic used more than once. Do not extract
  single-use helpers or speculative layers. Interfaces express actual substitution
  boundaries; prefer ordinary values/concrete types when sufficient.
- Pair declarations and implementations in neighboring `.h`/`.cpp` files under
  the owning feature. Do not expand legacy header-only `.hpp` implementations.
  Keep implementation dependencies out of public headers and include direct uses.
  Use `#pragma once`, one primary type or closely related group per file, and no
  separate include/source trees merely to split a declaration from implementation.
- Use `std::unique_ptr` for exclusive ownership, `std::shared_ptr` only for real
  shared ownership and references for required borrows. Raw pointers are explicit
  optional/non-owning borrows. Never adopt borrowed module/frame services.
- Use `const` wherever mutation is unnecessary. Keep public APIs minimal, internals
  private and each class/module responsible for one thing. Prefer standard-library
  facilities. Use private implementation only when it solves concrete publication
  or lifetime needs, with out-of-line destruction.
- Preserve every existing comment, especially user-written comments. Add a
  clarification when needed rather than removing the original. New comments
  explain non-obvious intent/lifetime/order; ordinary API explanation belongs in
  `GravitasAPI.md`.
- Check runtime failure explicitly; do not swallow IO/resource/Vulkan failures.
  Use assertions for programmer errors and early returns for optional absence.
- Do not introduce `.editorconfig` unless requested. Never fetch, sync a parent
  submodule, stage, commit or publish as an incidental cleanup action. Keep engine
  changes and application changes separately attributable.

## Build and verification

From the engine repository root, using a compatible existing build tree:

```sh
cmake -S . -B build_release -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build_release --parallel
ctest --test-dir build_release --output-on-failure
```

Do not replace an existing build tree's generator/configuration. Run focused tests
while implementing, then the required complete standalone suite for engine code
or build-contract changes. Module smoke cases cover supported reduced combinations;
do not disable checks or remove tests to make cleanup pass. GPU/windowed cases
require a real suitable environment; a skip is not proof. Inspect screenshots
for visible rendering/UI changes. For build-performance work compare equal
configuration/workload, separating clean-build and incremental measurements.

For documentation-only changes validate links, public signatures, targets,
examples and command working directories, then `git diff --check`. Build/test only
for changed executable inputs or unresolved concerns. The consuming application's
own boundary checks are additional to the engine layer checks.

## Development tools and generated output

No Python runtime/game logic is present or to be introduced. Retain existing
Python tooling until its behavior is deliberately replaced:

| File | Responsibility |
| --- | --- |
| `applications/GtsPerformanceBenchmarks/compare_reports.py` | Read compatible CPU benchmark JSON, verify schemas/build/work counters and print deltas; no timing pass/fail gate |
| `applications/GtsPerformanceBenchmarks/tests/test_reports.py` | CTest `performance_harness_reports`: run tiny C++ fixture workloads and verify reports/CLI/failures/deterministic counters |
| `benchmarks/tools/compare_benchmark_results.py` | Rendering JSON compatibility/counter/timing comparison with configurable warning/failure thresholds; CTest `rendering_benchmark_compare_tool` |

These use standard-library Python and do not implement workloads/rendering. CMake
discovers Python optionally for engine tooling tests; omitted tests are not a full
verification run. Vendor GLM's two Conan Python recipes remain vendor-owned.

Keep required CMake, test/tool sources, JSON inputs and assets tracked. Put generated
reports, logs, experiments, captures and disposable checks under ignored `build*`
directories or `screenshots/`. Bytecode caches are ignored; never blanket-ignore
`.py` or `.json` source extensions. Do not recreate retired audit scripts/history docs.
