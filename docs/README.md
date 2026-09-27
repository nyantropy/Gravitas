# Gravitas documentation

Start at [ARCHITECTURE.md](../ARCHITECTURE.md) for cross-module contracts and
[README.md](../README.md) for builds. These are engine-owned docs; game behavior
belongs to the consuming application's documentation.

## Path and command conventions

The engine repository root is the directory containing this `docs/` folder. In
DungeonCrawler it is the `engine/` submodule. Production sources live under its
nested `engine/` directory: `engine/modules/` here means `engine/engine/modules/`
from the game root. Short source fragments beginning `core/`, `modules/` or
`runtime/` are relative to that production source tree; feature-local fragments
are relative to the feature described by the page.

Build commands run from the engine repository root unless explicitly labeled as
DungeonCrawler commands. Third-party docs under `external/` remain vendor-owned.

## Foundation and runtime

- [Source ownership](modules/ownership.md): placement, target dependencies and include enforcement.
- [Runtime configuration](modules/runtime-configuration.md): complete facade and reduced builds.
- [Core](core/architecture.md): ECS contexts, scene resources, commands and input mutation.
- [Execution](execution/architecture.md): ordered scheduling, groups, selections and runtime recipes.
- [Settings](settings/architecture.md): startup snapshots, live requests and effective state.
- [JSON](json/architecture.md): checked values, syntax, serialization and schema boundaries.
- [Tween](core/tween.md): caller-owned easing and value transitions.
- [Transform](transform/architecture.md): hierarchy, invalidation, resolution and lifetime.
- [Diagnostics](diagnostics/architecture.md): profiling, debug drawing and feature bridges.

## Assets and models

- [Shared assets](assets/architecture.md): geometry, images, direct meshes and cooked containers.
- [Model architecture](model/architecture.md): application API, phase/target map and ownership.
- [Model domain](model/model-domain.md), [skeletons](model/skeleton-domain.md),
  [skin bindings](model/skin-binding-domain.md), [clips](model/animation-clip-domain.md).
- [Import bundle](model/import-bundle.md), [OBJ importer](model/obj-importer.md),
  [OBJ consumers](model/obj-consumers.md), [glTF/GLB importer](model/gltf-importer.md).
- [Static preparation](model/static-geometry.md), [skinned preparation](model/skinned-geometry.md).
- [Canonical cooking](model/canonical-cooking.md), [cooked formats and CLI](cooked-asset-pipeline.md).
- [Model loading](model/model-runtime.md), [geometry realization](model/model-realization.md),
  [world materials](model/model-material-realization.md), [instances](model/runtime-instances.md),
  [render extraction](model/model-extraction.md).
- [Transform animation](animation/transform-animation.md),
  [skeletal evaluation](animation/skeletal-evaluation.md),
  [skeletal playback](animation/skeletal-playback.md), [skin palettes](animation/skin-palette.md).

## Presentation and tools

- Rendering: [architecture](rendering/architecture.md),
  [authoring](rendering/authoring-guide.md), [Vulkan skinning](rendering/skinned-geometry.md).
- UI: [architecture](ui/architecture.md), [authoring](ui/authoring-guide.md).
- [Narrative](narrative/architecture.md): headless dialogue, VN presentation and interaction slots.
- Physics: [architecture](physics/architecture.md), [authoring](physics/authoring-guide.md).
- Tooling: [architecture](tooling/architecture.md), [authoring](tooling/authoring-guide.md),
  [presets and screenshots](tooling/presets.md).
- Measurements: [CPU scaling](performance-benchmarks.md),
  [rendering benchmarks](rendering/benchmarks.md),
  [baseline policy](../benchmarks/baselines/README.md).

Architecture pages describe current contracts and explicit limitations. Keep
feature details here, cross-module rules in the root index, and reproducible test
commands beside the behavior they verify. Historical benchmark measurements and
completed-task narratives are not project specifications. The cooking
[golden-fixture provenance](../tests/assets/fixtures/canonical-cooking/README.md)
is retained because it defines an immutable regression baseline.
