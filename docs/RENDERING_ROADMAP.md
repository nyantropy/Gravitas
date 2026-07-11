# Rendering Roadmap

## Goal

Complete the transition from a working renderer into a scalable
rendering architecture before expanding into full PBR and future
rendering features.

### Phase 2A --- Complete Transform Ownership (Current)

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

### Phase 2C --- Assetize Materials

Introduce:

-   MaterialAsset
-   MaterialDefinition
-   MaterialInstance
-   MaterialGpuState

Entities should reference material instances rather than embedding
material state.

### Phase 2D --- Simplify Extraction

Render extraction should emit stable render-facing objects and commands
using mesh/material handles rather than texture-specific data.

### Phase 2E --- Modernize Geometry

Add normals, tangents, improved vertex layouts, and importer support
required for modern PBR.

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
