# Rendering Authoring Guide

Use this guide when authoring scene renderables, materials, lights, particles,
or rendering integrations. Architecture details live in
[architecture.md](architecture.md).

## Renderable Geometry

Author exactly one geometry descriptor for a renderable entity:

- `StaticMeshComponent` for mesh assets.
- `QuadMeshComponent` for shared generated quads.
- `DynamicMeshComponent` for runtime-authored geometry.
- `WorldTextComponent` for world-space text.

Add `TransformComponent` and `BoundsComponent` where the renderable needs world
placement and culling. Use the transform module's dirty helpers when changing
transforms.

Do not write GPU companion components from application code. `MeshGpuComponent`
and `RenderGpuComponent` are engine-managed.

## Materials

Prefer shared material instances through `MaterialReferenceComponent` for new
sharing-oriented code. `MaterialComponent` is a legacy convenience adapter and
is still valid for simple or migrating code.

Use `LegacyUnlit` for:

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
`GtsCommandBuffer::requestScreenshot(...)`.

Automated tooling screenshots should use presets documented in
[../tooling/presets.md](../tooling/presets.md). The renderer prints saved PNG
paths; inspect those files for visual validation.

## Common Mistakes

- Do not expose Vulkan handles through gameplay-facing APIs.
- Do not make physics, gameplay, or tools include Vulkan headers.
- Do not mutate GPU companion components from application code.
- Do not recompute renderer-owned world matrices in rendering consumers; read
  `WorldTransformComponent`.
- Do not treat material parameters as object state unless the object truly
  needs a unique material instance.
- Do not make particle editor data the runtime simulation source.
