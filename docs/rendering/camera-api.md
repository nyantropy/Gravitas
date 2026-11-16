# Camera authoring and projection API

Applications author `CameraDescriptionComponent` (active flag, target/up, FOV,
aspect, near/far clips) and `TransformComponent` (eye placement). Add
`CameraControlOverrideComponent` when application code owns control input; this
suppresses default input handling while retaining the normal matrix pipeline.
Use the [transform dirty/resolution contract](../transform/architecture.md)
after every live transform edit. Component insertion is tracked by the installed
transform feature.

```text
application camera pose + description
    -> transform resolution
    -> camera companion lifecycle
    -> engine view/projection generation
    -> camera buffer binding + active-view publication
    -> renderer uploads after the frame fence
```

Rendering owns matrix conventions, including projection Y inversion, camera GPU
companions, buffer IDs, upload timing and cleanup. Application controllers must
run before renderer preparation. Editing active state, target or lens values
requires no GPU dirty flag or manual matrix recomputation. Hierarchical eye
placement comes from the published world transform.

`RendererSceneFeature.h` declares `installRendererFeature(scene, context,
execution)`; its paired implementation owns the system includes and installation
order. It receives execution groups/defaults from integration and installs once
per scene lifetime. Applications do not need lifecycle implementation headers.
The feature's resource-provider borrow is scene scoped and must outlive its
installed consumers/removal callbacks. Descriptor churn, world clear and scene
unload retain the existing renderer-owned buffer release path.

`ActiveCameraViewStateComponent` is an engine-owned read-only snapshot. Consumers
may copy it for projection, but must not write it or assume that its same-world
entity identity survives teardown. Renderer controllers publish it during scene
preparation. Engine tool controllers run afterward and finalize a tool camera
snapshot for rendering; observations during a controller pass can precede that
finalization. Inspect a completed frame when validating the tool handoff.

`CameraOverrideComponent` remains an engine-only path for a custom matrix producer.
It is not needed for a game free camera. `CameraControlOverrideComponent` alone
retains ordinary pose/lens synchronization and input independence, including
controllers that continue with unscaled time while simulation is paused.

## Tool camera return targets

`ToolCameraHandoff.h`/`.cpp`, owned by the core-only `gravitas_tool_contracts`, exposes:

- `isToolCameraActive(const ECSWorld&)`: observe tool camera activity without
  exposing its state component.
- `replaceToolCameraReturnTarget(world, retiringCamera, replacement)`: while
  tools are active, replace a return target matching the retiring camera (or an
  unset target). Preserve unrelated targets. Entity arguments borrow identities
  from this world; the function performs no scene/resource transfer.

A game camera that yields to tools can retire its descriptor and redirect the
return target to its original player camera. Tools own their internal state and
restore that target when hidden. No application edits the tool camera singleton.

`camera_authoring_contract` covers descriptor/lens/dirty propagation, aspect
changes, activation, buffer release and non-mutating diagnostic observation.
The game hosted fixture covers debug controls, pause, actual tools, window resize
and stair presentation through the public paths.
