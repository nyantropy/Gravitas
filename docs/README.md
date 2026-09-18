# Gravitas Engine Documentation

This folder contains first-party engine documentation. Game-specific
DungeonCrawler documentation belongs under the repository root `docs/game/`.
Third-party dependency documentation stays with the dependency under
`engine/external/`.

Start with:

- [assets/architecture.md](assets/architecture.md) for the canonical asset/model
  pipeline, CPU ownership, cooking and runtime boundaries.
- [assets/canonical-cooking.md](assets/canonical-cooking.md) and
  [cooked-asset-pipeline.md](cooked-asset-pipeline.md) for static cooking and storage.
- [model/runtime-instances.md](model/runtime-instances.md) for model occurrence state.
- [rendering/model-material-realization.md](rendering/model-material-realization.md)
  and [rendering/model-extraction.md](rendering/model-extraction.md) for world materials
  and generic static/skinned rendering.
- [settings/architecture.md](settings/architecture.md) for startup configuration,
  runtime preferences, resolution policies, and effective state.
- [../ARCHITECTURE.md](../ARCHITECTURE.md) for the engine architecture index.
- [ui/architecture.md](ui/architecture.md) and
  [ui/authoring-guide.md](ui/authoring-guide.md) for retained UI.
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
