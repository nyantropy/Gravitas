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

### Phase 3 --- PBR Foundation

Implement a metallic-roughness PBR pipeline on top of the completed
ownership model.

## Guiding Principles

-   Scene systems own semantic data.
-   Rendering owns representation.
-   CPU assets remain authoritative.
-   GPU state is a cached runtime representation.
-   Extraction is the boundary between ECS and rendering.
-   Optimize only after ownership boundaries are stable.
