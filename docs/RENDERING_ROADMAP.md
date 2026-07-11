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

Point and spot lights, generic multi-light extraction, normal maps, image-based
lighting, shadows, and metallic/roughness texture maps remain future phases.

### Phase 3C --- Point and Spot Light Extraction

Extend the completed directional-light and PBR surface-response path with
additional scene-authored light types and generic light extraction.

## Guiding Principles

-   Scene systems own semantic data.
-   Rendering owns representation.
-   CPU assets remain authoritative.
-   GPU state is a cached runtime representation.
-   Extraction is the boundary between ECS and rendering.
-   Optimize only after ownership boundaries are stable.
