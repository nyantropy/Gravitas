# Rendering Roadmap

This roadmap lists future rendering work only. Completed phases are documented
as current architecture in [architecture.md](architecture.md).

## Current Foundation

The renderer currently has:

- descriptor/runtime split for mesh, material, render-object, camera, texture
  animation, world text, and particles
- backend-independent render command extraction
- standard vertex contract with normals, tangents, color, and UV
- `Unlit` and `StandardSurface` shader families
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
- per-controller benchmark attribution, command-flush timing, render-prep
  substages, and logical-versus-physical object upload counters
- hitch capture in benchmark JSON for threshold-crossing frames, including
  nearby context frames, backend submit breakdown, fixed simulation tick count,
  controller timings, and transform/render-sync/upload counters
- incremental dynamic mesh synchronization with version-scheduled work,
  failed-version suppression, capacity-stable procedural buffer reuse, and
  dynamic mesh benchmark attribution
- incremental render-transform synchronization through a scene-local queue so
  `RenderGpuSystem` scales with changed world transforms instead of total
  renderable count
- single-threaded batch-ready transform, render-sync, and snapshot hot paths
  with deterministic work records, contiguous ranges, isolated outputs, and
  explicit publish/merge barriers for a future job executor
- retained UI extraction and Vulkan overlay composition
- screenshot capture with async PNG writes

## Future Rendering Work

- Establish versioned fixed-hardware performance baselines for the full CPU/GPU
  benchmark matrix.
- Add the first single-thread fallback job executor interface and run the
  existing batch-ready transform, render-sync, and snapshot ranges through that
  executor before introducing real worker threads.
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
