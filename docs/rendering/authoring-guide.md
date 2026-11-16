# Rendering Authoring Guide

Use this guide when authoring scene renderables, materials, lights, particles,
or rendering integrations. Architecture details live in
[architecture.md](architecture.md).

## Renderable Geometry

For complete file-backed models, use `requestGtsModel`, create an instance through
`modelInstances`, and attach `ModelInstanceComponent` plus an ECS transform. The
model owns hierarchy/material associations and static/skinned profile selection;
see the [model authoring boundary](../model/architecture.md#encapsulated-model-subsystem).

For direct/generated geometry, author exactly one geometry descriptor per entity:

- `StaticMeshComponent` for mesh assets.
- `QuadMeshComponent` for shared generated quads.
- `DynamicMeshComponent` for runtime-authored geometry.
- `WorldTextComponent` for world-space text.

Add `TransformComponent` and `BoundsComponent` where the renderable needs world
placement and culling. Use the transform module's dirty helpers when changing
transforms.

Do not write GPU companion components from application code. `MeshGpuComponent`
and `RenderGpuComponent` are engine-managed.

## Cameras

Author `CameraDescriptionComponent` plus a transform. A game-controlled camera
uses `CameraControlOverrideComponent` to suppress default input, while the engine
still computes/synchronizes matrices. Do not add `CameraOverrideComponent` or
write GPU companions to implement ordinary free-camera movement. Keep aspect
current from call-scoped viewport metrics. See the [camera API](camera-api.md)
and [read-only diagnostics](diagnostics-api.md).

## Materials

Create material instances through the compiled CPU `MaterialRuntime` API and attach
`MaterialReferenceComponent` to renderable entities. For simple unlit props,
tools, and tests, use `UnlitMaterialDescriptor` with
`createUnlitMaterial(...)` when the entity needs a unique mutable material, or
`sharedUnlitMaterialReference(...)` when the material should be shared.

Use `Unlit` for:

- world text
- debug draw
- particles
- tool visuals
- UI-like world geometry
- intentionally unlit scene props

Use `StandardSurface` for lit scene geometry. Provide normal-capable geometry
for lit output and tangent/UV0 data when enabling normal maps.

Do not put texture IDs, colors, or blend flags into render commands or object
uploads. They belong to material instances and material frame state.

See the [material API](material-api.md) for mutation, borrowed views, versioning
and world lifetime. GPU synchronization and cache registries are engine-private.

## Texture Roles

Use explicit material texture roles:

- Base color and emissive maps are sRGB color textures.
- Metallic-roughness, normal, and AO maps are linear/data textures.
- Metallic-roughness maps use glTF-compatible channel packing:
  `R = AO`, `G = roughness`, `B = metallic`.

The texture cache keys by color-space intent, so the same file may be realized
as sRGB or linear data depending on role.

## Lights

Author lights as scene entities with light descriptors and transforms:

- Directional lights use local `-Z` as the ray direction.
- Point lights use world position.
- Spot lights use world position plus local `-Z` cone direction.
- Environment lights are frame-level IBL descriptors and do not need spatial
  transform semantics.

Set priorities intentionally. Excess lights are dropped deterministically after
the renderer's bounded capacities.

## Texture Animation

Use `TextureAnimationComponent` for visual UV motion:

- `UvScroll` for water, lava, fog sheets, shimmer, or looping surface motion.
- `FlipbookAtlas` for discrete animated atlas frames.

Do not swap material texture paths or rewrite mesh vertices every frame for
ordinary texture animation.

## Particles

Application code authors `ParticleEmitterComponent` plus `TransformComponent`.
Set `effectPath` to a particle effect asset where possible.

New particle authoring features should extend:

- `ParticleEffectAsset`
- graph/module authoring data
- the module registry
- the compiler

Runtime simulation should continue consuming compiled runtime data rather than
editor UI structures.

## Screenshots

Manual screenshot requests use the `engine.screenshot` action or
`gts::rendering::requestScreenshot(commands, directory)` from `ScreenshotCommand.h`.

Automated tooling screenshots should use presets documented in
[../tooling/presets.md](../tooling/presets.md). The renderer prints saved PNG
paths; inspect those files for visual validation.

## Common Mistakes

- Do not expose Vulkan handles through gameplay-facing APIs.
- Do not make physics, gameplay, or tools include Vulkan headers.
- Do not mutate GPU companion components from application code.
- Do not recompute transform-owned world matrices in rendering consumers; read
  `WorldTransformComponent`.
- Do not treat material parameters as object state unless the object truly
  needs a unique material instance.
- Do not make particle editor data the runtime simulation source.
