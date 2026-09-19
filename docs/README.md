# Gravitas Engine Documentation

This folder contains first-party engine documentation. Game-specific
DungeonCrawler documentation belongs under the repository root `docs/game/`.
Third-party dependency documentation stays with the dependency under
`engine/external/`.

Start with:

- [modules/ownership.md](modules/ownership.md) for the permanent core/modules/runtime
  placement rule, dependency enforcement and known migration blockers.

- [transform/architecture.md](transform/architecture.md) for transforms, hierarchy
  and world-transform publication.
- [core/architecture.md](core/architecture.md) for foundational ownership, feature contracts and deferred semantic boundaries.
- [core/tween.md](core/tween.md) for shared UI/VN easing and value transitions.
- [diagnostics/architecture.md](diagnostics/architecture.md) for debug drawing
  and its physics/tooling consumers.
- [assets/architecture.md](assets/architecture.md) for the canonical asset/model
  pipeline, CPU ownership, cooking and runtime boundaries.
- [model/canonical-cooking.md](model/canonical-cooking.md) and
  [cooked-asset-pipeline.md](cooked-asset-pipeline.md) for static cooking and storage.
- [model/runtime-instances.md](model/runtime-instances.md) for model occurrence state.
- [model/model-material-realization.md](model/model-material-realization.md)
  and [model/model-extraction.md](model/model-extraction.md) for world materials
  and generic static/skinned rendering.
- [settings/architecture.md](settings/architecture.md) for startup configuration,
  runtime preferences, resolution policies, and effective state.
- [../ARCHITECTURE.md](../ARCHITECTURE.md) for the engine architecture index.
- [ui/architecture.md](ui/architecture.md) and
  [ui/authoring-guide.md](ui/authoring-guide.md) for retained UI.
- [narrative/architecture.md](narrative/architecture.md) for headless dialogue
  and its visual-novel presentation consumer.
- [tooling/architecture.md](tooling/architecture.md) and
  [tooling/authoring-guide.md](tooling/authoring-guide.md) for engine tools.
- [tooling/presets.md](tooling/presets.md) for launch presets and screenshot
  automation.
- [rendering/architecture.md](rendering/architecture.md) and
  [rendering/authoring-guide.md](rendering/authoring-guide.md) for rendering.
- [physics/architecture.md](physics/architecture.md) and
  [physics/authoring-guide.md](physics/authoring-guide.md) for physics.

Add new engine feature docs under the owning feature folder rather than growing
the root architecture index.
