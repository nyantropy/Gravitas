# Tooling Presets And Screenshot Workflow

Tooling launch presets configure the engine into a deterministic startup state.
They are intended for development, screenshot capture, visual verification, and
future regression tests.

A preset is not:

- an editor workspace save file
- user preferences
- docking/layout persistence
- arbitrary widget state serialization
- runtime persistence

## Command Line

Primary usage:

```sh
build/src/DungeonCrawler --tooling-preset=engine/docs/tooling/tooling_presets/particles.json
```

Supported overrides:

```sh
--screenshot-directory=screenshots/tooling/run_001
--no-auto-exit
```

Avoid adding many individual launch flags. The JSON preset is the primary
configuration mechanism.

## JSON Schema

Current schema:

```json
{
  "tools": {
    "visible": true,
    "workspace": "particles",
    "visualEvaluation": true,
    "debugDraw": false,
    "gizmos": false,
    "scene": "editor_visual_eval",
    "particleEffect": "resources/particles/ambient_brazier.json",
    "selectedEmitter": 0,
    "selectedModule": 0
  },
  "screenshots": {
    "enabled": true,
    "afterSeconds": 2.0,
    "intervalSeconds": 1.0,
    "count": 3,
    "directory": "screenshots/tooling/particles",
    "exitAfterCapture": true
  }
}
```

Fields:

- `tools.visible`: seeds `EngineToolStateComponent::visible`.
- `tools.workspace`: `world`, `particles`, or `assets`.
- `tools.visualEvaluation`: enables a clean screenshot-oriented startup state.
  It clears tool selection and disables debug draw/gizmos unless explicit
  overrides are also supplied.
- `tools.debugDraw`: enables or disables tool debug draw primitives at startup.
- `tools.gizmos`: enables or disables transform gizmos at startup.
- `tools.scene`: scene id registered by the application.
- `tools.particleEffect`: particle asset path opened by `ParticleEditorSession`.
- `tools.assetManifest`: asset manifest path selected by `AssetBrowserSession`.
- `tools.selectedEmitter`: selected emitter index, clamped by the session.
- `tools.selectedModule`: selected module index, clamped by the session.
- `screenshots.enabled`: enables automated screenshot scheduling.
- `screenshots.afterSeconds`: delay before first capture.
- `screenshots.intervalSeconds`: delay between captures.
- `screenshots.count`: number of captures to request.
- `screenshots.directory`: output directory. Relative paths resolve from the
  project root.
- `screenshots.exitAfterCapture`: exits after the final requested capture.

Example presets live in `engine/docs/tooling/tooling_presets/`:

- `empty_editor.json`
- `world_viewer.json`
- `assets.json`
- `particles.json`
- `particles_inspector.json`
- `editor_eval_world.json`
- `editor_eval_assets.json`
- `editor_eval_particles.json`

## Startup Flow

```text
Application launch
  -> parse --tooling-preset=<json>
  -> initialize engine runtime
  -> seed EngineToolRuntime
  -> seed ParticleEditorSession through EngineToolShellSystem
  -> load requested scene as the initial active scene
  -> open requested workspace
  -> open requested particle effect
  -> restore requested emitter/module selection
  -> wait for screenshot timing
  -> request screenshots through the renderer screenshot path
  -> optionally exit after the final capture
```

No simulated UI clicks are used. The preset configures runtime state directly.

## Visual Evaluation Presets

Use the `editor_eval_*` presets for editor UI work. They launch a deterministic
tooling state with gameplay HUDs, debug primitives, and gizmos disabled so
screenshots evaluate the editor rather than the loaded game scene.

Default UI pass workflow:

1. Capture a baseline with `editor_eval_particles.json`.
2. Rename or copy the captured image to
   `screenshots/editor_eval_particles_before_<pass>.png`.
3. Implement the visual change.
4. Capture the same preset again.
5. Rename or copy the captured image to
   `screenshots/editor_eval_particles_after_<pass>.png`.
6. Repeat with `editor_eval_world.json` or `editor_eval_assets.json` when the
   change affects those workspaces.

Expected launch shape:

```sh
build/src/DungeonCrawler \
  --tooling-preset=engine/docs/tooling/tooling_presets/editor_eval_particles.json
```

Use `--screenshot-directory=<path>` for one-off comparison runs. Keep
comparison screenshots under the repository root `screenshots/` folder with
clear `before_` and `after_` labels.

## Screenshot Capture Path

Preset screenshots reuse the renderer screenshot infrastructure:

```text
ToolLaunchPreset screenshots
  -> GravitasEngine::processToolScreenshotAutomation
  -> IGtsGraphicsModule::requestScreenshot
  -> ForwardRenderer::requestScreenshot
  -> ScreenshotManager::saveImage
  -> async PNG write job
```

Manual screenshot requests use the same renderer path through the
`engine.screenshot` action or `GtsCommandBuffer::requestScreenshot(...)`.

`ScreenshotManager` allocates output paths, captures the swapchain image into a
staging buffer, and writes PNG files asynchronously. Image transition, copy, and
map still happen on the render path; if screenshot hitches become unacceptable,
the next optimization should be frame-delayed readback with a small staging
buffer ring.

## Agent Inspection Workflow

The renderer prints saved paths such as:

```text
Saved screenshot: screenshots/tooling/editor_eval_particles/screenshot_0001.png
```

Agents with image-inspection tools should open those PNG files directly and
compare before/after captures. Terminal output only proves that capture
succeeded; it does not verify visual correctness.
