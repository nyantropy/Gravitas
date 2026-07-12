# Rendering Roadmap

This roadmap lists future rendering work only. Completed phases are documented
as current architecture in [architecture.md](architecture.md).

## Current Foundation

The renderer currently has:

- descriptor/runtime split for mesh, material, render-object, camera, texture
  animation, world text, and particles
- backend-independent render command extraction
- standard vertex contract with normals, tangents, color, and UV
- `LegacyUnlit` and `StandardSurface` shader families
- metallic-roughness PBR direct lighting
- directional, point, and spot light extraction
- texture roles for base color, metallic-roughness, normal, AO, and emissive
- image-based lighting from HDR equirectangular sources preprocessed into
  cubemap resources
- production-compatible environment descriptor bindings
- object SSBO layout limited to model matrix and UV transform, with material
  base color owned by shared material GPU state instead of object tint writes
- incremental material synchronization through a scene-local material user
  index and deduplicated material change queue
- deterministic rendering benchmark smoke suite with JSON output, statistical
  summaries, invariant checks, baseline comparison tooling, and CTest labels
- Vulkan GPU timestamp-query benchmarking for total frame, scene, particles,
  and UI stages when supported by the selected device
- retained UI extraction and Vulkan overlay composition
- screenshot capture with async PNG writes

## Future Rendering Work

- Run the full benchmark matrix on fixed hardware and establish versioned CI
  baselines before starting the next renderer optimization phase.
- Add alpha-cutoff shader support for `RenderQueue::AlphaMasked`.
- Add sky/background rendering that consumes selected environment state without
  coupling sky draw commands to material draw commands.
- Add shadows.
- Add normal/AO/emissive texture authoring tools.
- Add material asset files and material editor workflows.
- Add material-frame caching so `MaterialFrameData` no longer has to be
  repopulated from live renderable snapshots every full world build.
- Add advanced material extensions after the first asset workflow is stable.
- Add smaller half-float environment formats where memory pressure justifies
  the optimization.
- Add frame-delayed screenshot readback with a small staging buffer ring if
  screenshot hitches become unacceptable.
- Add clustered or tiled lighting only after the bounded forward light path is
  demonstrably insufficient.
- Add render-target, world-space, and multi-window UI surface targets over the
  existing UI surface ownership boundary.

## Guiding Principles

- Scene systems own semantic data.
- Rendering owns representation.
- CPU assets remain authoritative.
- GPU state is a cached runtime representation.
- Extraction is the boundary between ECS and rendering.
- Optimize only after ownership boundaries are stable.
