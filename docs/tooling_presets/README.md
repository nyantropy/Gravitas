# Tooling Preset Visual QA

Use the `editor_eval_*` presets for editor UI work. They launch a deterministic
tooling state with gameplay HUDs, debug primitives, and gizmos disabled so the
screenshots evaluate the editor instead of the loaded game scene.

Default UI pass workflow:

1. Capture a baseline with `editor_eval_particles.json`.
2. Rename the captured image to `screenshots/editor_eval_particles_before_<pass>.png`.
3. Implement the visual change.
4. Capture the same preset again.
5. Rename the captured image to `screenshots/editor_eval_particles_after_<pass>.png`.
6. Repeat with `editor_eval_world.json` when the change affects the world tab.

Expected launch shape:

```sh
build/src/DungeonCrawler --tooling-preset=engine/docs/tooling_presets/editor_eval_particles.json
```

Use `--screenshot-directory=<path>` for one-off comparison runs. Keep comparison
screenshots under the repository root `screenshots/` folder with clear
`before_` and `after_` labels.

The presets are not workspace persistence. They only seed startup state for
development, screenshot generation, and future visual regression testing.
