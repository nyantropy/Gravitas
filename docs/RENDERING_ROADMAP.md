# Rendering Roadmap

## Goal

Complete the transition from a working renderer into a scalable
rendering architecture before expanding into full PBR and future
rendering features.

### Phase 2A --- Complete Transform Ownership (Complete)

Finish the ownership transition so **WorldTransformComponent** becomes
the authoritative source of world-space data.

-   Fix hierarchy edge cases:
    -   Reject ancestor cycles.
    -   Define parent-destruction behavior.
    -   Add hierarchy regression tests.
-   Convert remaining world-space consumers (camera, particles, debug
    rendering where appropriate) to use `WorldTransformComponent`.
-   Ensure every direct transform writer correctly invalidates
    transforms (`markDirty()` or equivalent).
-   Remove remaining renderer assumptions about local transforms.

### Phase 2B --- Split Rendering Lifecycle

Separate mesh, material, and render-object lifecycle responsibilities
into dedicated systems.

Status: complete.

### Phase 2C --- Material Asset Architecture (Complete)

Introduce:

-   MaterialDefinition
-   MaterialInstance
-   MaterialGpuState
-   MaterialReferenceComponent
-   MaterialRuntime

Entities should reference material instances rather than embedding
material state.

Status: complete for material identity, sharing, fallback, and versioned GPU
cache ownership. Full material asset files and PBR parameter sets remain future
work on top of this ownership model.

### Phase 2D --- Modernize Render Extraction and Commands (Complete)

Render extraction should emit stable render-facing objects and commands
using mesh/material handles rather than texture-specific data.

Status: complete. `RenderCommand` now carries mesh identity, material instance
identity, material GPU identity, variant key, render queue, object slot, camera
view, and sort key. Texture IDs, material tint/base color, legacy blend flags,
and vertex-color booleans are no longer generic command fields. Material frame
data travels beside the command list so the Vulkan backend can resolve
`MaterialGpuHandle` into the current compatibility texture/color/render-state
payload. Object uploads now carry object-owned model and UV data only.

### Phase 2E --- Modernize Geometry (Complete)

Add normals, tangents, improved vertex layouts, and importer support
required for modern PBR.

Status: complete. Generic scene geometry now uses a standard
position/normal/tangent/color/UV vertex contract. OBJ loading preserves
independent position/normal/UV indices, generated and procedural geometry have
explicit attribute/default behavior, missing normals and tangents are generated
deterministically outside extraction/draw hot paths, and Vulkan vertex input
plus compatibility shaders consume the expanded layout. Phase 3 can add PBR
lighting on top of this surface-frame data without reopening mesh lifecycle,
material identity, or render command ownership.

### Phase 3A --- First Lit Renderer (Complete)

Establish the first production-quality lit path before implementing the final
PBR BRDF.

Status: complete. The renderer now supports explicit `LegacyUnlit` and
`StandardSurface` shader families, one active scene directional light extracted
from ECS through `WorldTransformComponent`, frame-level camera position and
directional-light upload, world-space normal transformation in the scene vertex
shader, and ambient + diffuse + temporary Blinn-Phong-style specular lighting.
Unlit, world-text, debug, particles, editor-preview, and vertex-color-only paths
remain explicitly unlit. `StandardSurface` compatibility requires normals and
falls back to unlit shading with a diagnostic when mesh metadata is
incompatible. Phase 3B replaces the temporary lit response with the
metallic-roughness model without reopening light, material, or command
ownership.

### Phase 3B --- PBR Foundation (Complete)

Implement a metallic-roughness PBR pipeline on top of the completed
ownership model.

Status: complete. `StandardSurface` now uses shared material `baseColor`,
`metallic`, and perceptual `roughness` parameters. Direct directional lighting
uses a Cook-Torrance BRDF with GGX distribution, Smith geometry, and Schlick
Fresnel. Roughness is clamped to a documented minimum of `0.04`, metallic is
clamped to `[0, 1]`, and parameter changes update material GPU/frame data
without changing `MaterialVariantKey`. `LegacyUnlit` and `StandardSurface` now
resolve to distinct Vulkan fragment shader modules through shader-family
pipeline variants. Texture bindings carry explicit sRGB versus linear/data
intent so base-color textures are sampled through sRGB formats while future
metallic, roughness, normal, AO, and mask textures can remain linear.
`GtsPbrValidation` is a deterministic visual fixture for this phase, with a
metallic/roughness grid, a non-uniformly scaled lit object, an unlit comparison
object, a transparent object, and one directional light.

Normal maps, image-based lighting, shadows, and metallic/roughness texture maps
remain future phases.

### Phase 3C --- Generic Light Extraction and Multi-Light Forward Rendering (Complete)

Extend the completed directional-light and PBR surface-response path with
additional scene-authored light types and generic light extraction.

Status: complete. Lighting extraction now produces a bounded generic
`LightingFrameData` payload containing up to 2 directional lights, 32 point
lights, and 16 spot lights. Directional, point, and spot descriptors remain
scene-authored data; spatial meaning comes from `WorldTransformComponent`.
Directional lights use local `-Z` as ray direction, point lights use world
position, and spot lights use world position plus local `-Z` cone direction.

Selection is deterministic: directionals are ranked by priority and stable
entity ID, while point and spot lights are ranked by priority, distance to the
active camera, and stable entity ID. Excess lights are dropped through the
documented capacities and counted in diagnostics. Extraction sanitizes negative
or non-finite authored values, clamps ranges and cone angles, and precomputes
spot cone cosine thresholds.

The Vulkan camera/frame UBO now carries the generic lighting frame arrays
beside view/projection and camera world position. The PBR fragment shader loops
over the selected directional, point, and spot lights, computes radiance and
attenuation per light type, and feeds every light into the same Cook-Torrance
metallic-roughness BRDF. Ambient remains a temporary frame-level fallback and
is applied once, outside the light loops. Light changes remain frame-level
state; they do not mutate materials, meshes, render commands, or object slots.

`GtsPbrValidation` now includes directional, point, spot, parented, and
over-capacity lights so attenuation, cone behavior, and deterministic dropping
can be inspected visually.

### Phase 3D --- Material Texture Maps and Surface Detail (Complete)

Complete the first practical `StandardSurface` material model by adding
texture-driven surface detail without leaking texture data into render commands
or object uploads.

Status: complete. `StandardSurface` now has explicit material texture roles for
base color, metallic-roughness, normal, ambient occlusion, and emissive maps.
Base-color and emissive textures are sRGB color textures; metallic-roughness,
normal, and AO textures are linear/data textures, and the texture cache keys by
color-space intent so the same path can be realized safely for different
usages. Metallic-roughness maps use glTF-compatible channel semantics:
`R = AO`, `G = roughness`, `B = metallic`, with a separate AO role able to
override the packed AO channel.

Material GPU parameters now carry base-color factor, metallic/roughness
factors, normal scale, AO strength, and emissive factor/strength. Vulkan binds
a stable five-role material texture descriptor set backed by neutral fallback
textures for missing maps. Render commands remain texture-agnostic and carry
only mesh/material/object identity.

Normal mapping uses geometry tangents and handedness, transforms normals and
tangents correctly under non-uniform and negative scale, and degrades
deterministically when a mesh lacks the required normal/tangent/UV surface
frame. At the end of Phase 3D, AO affected only the temporary ambient fallback.
Emissive adds visual linear emission but does not inject scene lighting.
`GtsPbrValidation` now includes texture-map, shared-material, AO/emissive,
non-uniform normal-map, and incompatible-geometry fallback samples.

Shadows, environment probes, normal/AO/emissive texture authoring tools, and
advanced material extensions remain later roadmap items.

### Phase 3E --- Image-Based Lighting and Environment Lighting (Complete)

Replace the temporary ambient approximation with scene-authored environment
lighting that remains frame-level renderer state.

Status: complete for the first ownership-safe IBL integration. Scenes can now
author `EnvironmentLightComponent` descriptors with path, intensity, rotation,
enabled state, and priority. Extraction deterministically selects one enabled
environment by priority and stable entity ID, sanitizes intensity/rotation, and
publishes backend-independent `EnvironmentFrameData` beside the generic light
arrays. Environment changes remain frame-level state; they do not mutate
materials, meshes, render commands, or object uploads.

The Vulkan renderer resolves the selected environment through render resource
management into a separate environment descriptor set containing irradiance,
prefiltered-specular, and BRDF-LUT bindings. PBR shaders now evaluate direct
lighting plus diffuse IBL, specular IBL, and emissive contribution. AO affects
indirect diffuse IBL only, while direct lights and emissive remain independent.
Environment intensity scales the IBL contribution and rotation is applied around
world up through sample-direction rotation. Valid fallback resources are always
bound for no-environment scenes.

The current backend realization intentionally uses deterministic linear
equirectangular/fallback 2D environment textures as the sampled environment
source. It does not yet generate true HDR cubemaps, irradiance convolution
cubemaps, or GGX-prefiltered cubemap mip chains. That backend preprocessing
work remains the main rendering-quality blocker before a production IBL pass,
but it can replace the current resource realization without reopening material,
command, object, or light-extraction ownership.

No sky/background pass is included in this phase. A future sky pass should
consume the selected environment state independently from material draw
commands.

## Guiding Principles

-   Scene systems own semantic data.
-   Rendering owns representation.
-   CPU assets remain authoritative.
-   GPU state is a cached runtime representation.
-   Extraction is the boundary between ECS and rendering.
-   Optimize only after ownership boundaries are stable.
