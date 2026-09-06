# Settings Architecture

Settings are typed data owned by the feature that interprets them. Startup
configuration, requested runtime preferences, and observed runtime state have
different lifetimes and must not be treated as interchangeable copies.

## Startup Composition

`engine/EngineConfig.h` is the application-facing startup aggregate:

```text
EngineConfig
  simulation: SimulationSettings
  tools: ToolSettings
  graphics: GraphicsConfig
    startup: GraphicsStartupOptions
    windowTitle
    screenshots: ScreenshotSettings
    settings: GraphicsSettings
      window: WindowSettings
      presentation: PresentationSettings
      rendering: RenderSettings
      framePacing: FramePacingSettings
```

`GravitasEngine` retains a const startup configuration. Runtime commands never
rewrite it. `GraphicsConfig` similarly remains a const initialization snapshot
inside the graphics backend; accepted preferences are stored separately.

The default backend registry is a temporary passed to the `GtsPlatform`
constructor. The platform uses it to create the selected backend and owns the
returned graphics module. Neither the engine nor the platform retains the
registry after construction.

Types live beside their owners, not inside `EngineConfig.h`:

| Type | Source ownership | Application |
| --- | --- | --- |
| `SimulationSettings` | `core/time/` | Fixed-step scheduler initialization; positive tick rate required. |
| `FramePacingSettings` | `core/time/` | Engine frame scheduling; zero means uncapped. |
| `ToolSettings` | `modules/tools/core/` | Tool runtime installation and initial debug overlay visibility. |
| `GraphicsStartupOptions` | `modules/rendering/core/init/` | Backend, validation, headless operation, GPU timestamps; initialization only. |
| `WindowSettings` | `modules/rendering/core/windowing/output/` | Window dimensions, mode, and preferred monitor. |
| `PresentationSettings` | `modules/rendering/core/init/` | Presentation policy resolved by the graphics backend. |
| `RenderSettings` | `modules/rendering/core/init/` | Scene render resolution and frustum culling. |
| `ScreenshotSettings` | `modules/rendering/core/init/` | Initial capture limits. |

`OutputWindowConfig` combines window settings with a creation title.
`WindowManagerConfig` selects the window backend and passes that descriptor on.
Neither windowing type contains Vulkan validation flags or presentation policy.
Backend-private Vulkan creation descriptors may carry validation options to the
Vulkan objects that require them; those are derived initialization inputs.

`GraphicsSettings` aggregates several owners so one coordinated request can
change window, presentation, rendering, and frame pacing together. It is not a
new universal settings manager. The graphics backend coordinates window and GPU
resource changes; `RenderingRuntime` applies culling; `GravitasEngine` applies
the accepted frame limit.

## Runtime Change Flow

```text
application draft: GraphicsSettings
  -> requestApplyGraphicsSettings(GtsCommandBuffer, draft)
  -> RenderingRuntime command dispatch
  -> GravitasEngine application callback
  -> graphics backend validation and coordinated application
  -> accepted frame pacing and culling changes
```

`validateGraphicsSettings` checks dimensions, enums, monitor index, frame limit,
and resolution policy without creating resources. Startup and live requests use
the same validation. Vulkan also checks resolved render dimensions against
device limits. Invalid requests leave accepted preferences unchanged.

The backend application API returns `GraphicsSettingsApplyResult`:

- `Applied`: requested resource changes have completed. Presentation may still
  have fallen back to a supported mode; inspect effective state.
- `Pending`: the request is accepted, but an output such as a minimized window
  is not drawable. Frame rendering retries resource application.
- `Rejected`: the request is invalid or unsupported; `message` explains why.

Window changes are applied together. Required frame-resource recreation waits
for graphics idle and resolves rendering size against the resulting swapchain.
Frame-rate and culling-only changes do not recreate graphics resources.
Window resize events schedule work; they do not overwrite the requested render
policy or the engine startup snapshot.

The existing culling-only command merges its value into the current graphics
request when an engine application callback is installed. Frustum freeze remains
a separate transient diagnostic control.

## Requested And Effective Values

`IGtsGraphicsModule::getRequestedGraphicsSettings()` returns the most recently
accepted preferences. An unavailable preferred monitor or presentation mode
does not erase the preference.

`getGraphicsRuntimeState()` returns observed state:

- optional window state, with actual window dimensions in window coordinates
- current allocated output extent in framebuffer pixels
- current allocated scene render extent
- optional actual presentation mode
- whether resource application is pending

Window and presentation state are absent in headless operation. The output
extent can differ from window dimensions due to display scaling. During pending
recreation the allocated output/render extents still describe existing resources.

## Render Resolution

`RenderResolutionSettings` explicitly selects one policy:

- `MatchOutput`: scene rendering follows the output framebuffer.
- `Fixed`: `fixedExtent` is independent of window size and window mode.
- `ScaleOutput`: multiply each output dimension by `scale`, round to the nearest
  integer, and retain at least one pixel per axis.

`resolveRenderExtent` is the CPU-only policy implementation. It rejects invalid
active policy values and overflow. `Fixed` and `ScaleOutput` use the renderer's
offscreen scene target and composition path in all windowed modes. UI remains
at output resolution. Composition preserves the source aspect ratio.

Headless startup resolves its output extent from the initial render policy and
configured dimensions. Resizing that headless output after creation is not
implemented; requests requiring a different extent are explicitly rejected
with a restart-required message.

## Presentation

There is one `PresentModePreference`: `Immediate`, `Mailbox`, or `Fifo`.
There is no competing VSync boolean. A simple VSync menu can map enabled to
`Fifo` and disabled to `Immediate`; an advanced menu may expose all policies.
The existing Vulkan selection logic chooses a supported fallback when needed.
Effective state reports the actual selected mode, not the requested preference.

## Persistence And Extension Rules

The application owns persistence, user-facing defaults, and menu drafts. The
engine owns validation and runtime application. DungeonCrawler currently layers
its C++ defaults with saved video preferences before engine construction.

When adding settings:

1. Put the typed data beside the subsystem that owns its meaning.
2. Decide whether it is startup-only, live, or requires resource recreation.
3. Define defaults and validation, including capability-dependent restrictions.
4. Extend the existing coordinated command if the setting participates in it.
5. Expose effective state separately when the request may differ from reality.
6. Update application persistence/version migration and focused tests.

Do not add empty settings types for unimplemented features, duplicate mutable
startup snapshots, or perform GPU/window work in serializers or UI controls.

There is no general engine configuration-file precedence resolver, reflection
registry, automatic video rollback, or asynchronous UI completion subscription
in this implementation. The command path logs rejected requests; the direct
backend API returns the application result. These can be extended without
changing settings ownership or conflating requested and observed state.
