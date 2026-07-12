# Rendering Benchmarks

The rendering benchmark suite provides deterministic workloads for renderer
performance work. It is measurement infrastructure: benchmark code must use
public engine descriptors, normal ECS lifecycle systems, normal extraction, and
normal renderer counters. It must not bypass renderer ownership boundaries to
produce better numbers.

## Application Structure

The native benchmark entry point is:

```text
applications/GtsRenderingBenchmarks/
```

Shared benchmark support lives in:

```text
modules/rendering/benchmarks/RenderingBenchmark.h
modules/rendering/benchmarks/RenderingBenchmark.cpp
```

The application accepts command-line configuration, runs warmup and measured
frames, writes JSON results, prints a concise summary, and terminates without
user input.

## Modes

`cpu_smoke` is the normal CI mode. It creates a deterministic ECS world,
installs transform, camera, geometry, material, particle, and extraction
systems, and runs the normal render-prep plus `RenderPipeline` path with a fake
resource provider. It does not create a Vulkan device or GPU timestamp queries.

`gpu_runtime` starts the real headless Vulkan engine path, installs normal
renderer scene features, records regular command buffers, enables backend GPU
timestamp queries, and exits after the requested measured frame window. If no
Vulkan device is available, the executable exits with code 77 so CTest can mark
the benchmark skipped. If the device cannot provide timestamp queries, the run
continues with CPU timings and counters while marking GPU timing unsupported.

## Usage

List presets:

```bash
GtsRenderingBenchmarks --list-presets
```

Run a small smoke benchmark:

```bash
GtsRenderingBenchmarks \
  --preset static_geometry_small \
  --mode cpu_smoke \
  --warmup-frames 30 \
  --measured-frames 120 \
  --output static_geometry_small.json \
  --check-invariants
```

Override fields either with named options or `--set key=value`:

```bash
GtsRenderingBenchmarks --preset combined_game_like --set renderable-count=2000
```

## Configuration Fields

`RenderingBenchmarkConfig` controls:

- seed, warmup frames, measured frames
- renderable count and visible renderable count
- unique mesh and material counts
- moving object count
- material parameter and topology mutation counts per frame
- directional, point, spot, and moving light counts
- dynamic mesh, particle emitter, and world text counts
- PBR, normal-map, IBL, UI, and frustum-culling toggles
- render resolution and run mode

Generation is seeded and deterministic. Animation, material mutation, dynamic
mesh mutation, and light movement use fixed frame indices rather than arbitrary
wall-clock time.

## Standard Presets

Current presets:

- `static_geometry_small`
- `static_geometry_large`
- `material_shared_steady`
- `material_shared_mutating`
- `material_unique`
- `transform_many_moving`
- `moving_static_control`
- `moving_independent`
- `moving_sparse`
- `moving_dense`
- `moving_deep_hierarchy`
- `moving_wide_hierarchy`
- `upload_only_pressure`
- `dynamic_mesh_static_control`
- `dynamic_mesh_sparse_mutation`
- `dynamic_mesh_dense_mutation`
- `dynamic_mesh_capacity_stable`
- `dynamic_mesh_growth`
- `dynamic_mesh_attribute_generation`
- `dynamic_mesh_precomputed_attributes`
- `visibility_sparse`
- `visibility_dense`
- `lighting_at_capacity`
- `lighting_over_capacity`
- `batch_high_coherence`
- `batch_low_coherence`
- `pbr_fragment_heavy`
- `combined_game_like`

Each preset has a version. Increment the version when changing generation,
object counts, camera paths, mutation schedules, render settings, or measured
counters.

## Timing Buckets

CPU timing buckets currently emitted:

- `frame_cpu`
- `controller_cpu`
- `simulation_cpu`
- `transform_resolve_cpu`
- `render_gpu_sync_cpu`
- `snapshot_build_cpu`
- `visibility_cpu`
- `command_extract_cpu`
- `command_sort_cpu`
- `backend_frame_cpu`
- `backend_cmd_record_cpu`
- `backend_queue_submit_cpu`
- `backend_present_cpu`
- `render_submit_cpu`

GPU timing buckets are emitted only when real Vulkan timestamp query results are
available:

- `frame`
- `scene`
- `particles`
- `ui`

CPU queue-submit time is never used as a substitute for GPU frame time.

## Controller Attribution

Benchmarks emit per-controller timing in `controller_timings_ms`. The stable
identity is the registered ECS system name plus an instance suffix when needed:
`SystemName`, `SystemName#1`, `SystemName#2`. Default names are normalized from
the system type to the unqualified class name, and each entry includes the
`EcsSystemGroup` string.

Controller timings measure the system `update()` call only. The structural
command flush that runs after each controller is reported separately in
`controller_flush_timings_ms` so lifecycle command cost is not hidden inside the
preceding system.

GPU runtime benchmarks disable the global engine tooling runtime so editor/tool
controllers do not pollute workload attribution. Normal engine applications keep
tooling enabled by default.

Selected low-cardinality substages are emitted in `controller_substages_ms`:

- `TransformSystem.queue_children`
- `TransformSystem.resolve_world`
- `TransformSystem.publish_world_transform`
- `RenderGpuSystem.scan_compare`
- `RenderGpuSystem.model_sync_enqueue`
- `RenderGpuSystem.candidate_discovery`
- `RenderGpuSystem.validation`
- `RenderGpuSystem.version_checks`
- `RenderGpuSystem.model_matrix_copy`
- `RenderGpuSystem.object_upload_enqueue`
- `RenderGpuSystem.snapshot_invalidation`
- `RenderGpuSystem.queue_cleanup`
- `ForwardRenderer.object_buffer_writes`
- `DynamicMeshBindingSystem.candidate_discovery`
- `DynamicMeshBindingSystem.version_checks`
- `DynamicMeshBindingSystem.validation`
- `DynamicMeshBindingSystem.bounds`
- `DynamicMeshBindingSystem.geometry_preparation`
- `DynamicMeshBindingSystem.temporary_copy`
- `DynamicMeshBindingSystem.resource_allocation`
- `DynamicMeshBindingSystem.vertex_upload`
- `DynamicMeshBindingSystem.index_upload`
- `DynamicMeshBindingSystem.publication`
- `DynamicMeshBindingSystem.invalidation`
- `DynamicMeshBindingSystem.cleanup`

Detailed transform, render-GPU, and dynamic-mesh substage timers are enabled by
benchmark runs and disabled by default for normal engine execution.

## Vulkan Timestamp Architecture

GPU timestamp ownership is backend-local. `ForwardRenderer` owns a
`VulkanTimestampManager` beside the other frame resources. The manager allocates
one timestamp query pool per frame-in-flight, resets the pool while recording
the reused frame slot, writes frame begin/end timestamps, and wraps frame-graph
stage recording for scene, particle, and UI timing.

Results are collected only after the corresponding frame fence has completed,
so benchmark execution does not block the CPU just to read timestamps. The
manager converts timestamp deltas to milliseconds with the physical device
`timestampPeriod` and respects the graphics queue family's valid timestamp bit
width.

If timestamp queries are unavailable, allocation fails, or query results cannot
be collected, the backend emits a bounded warning, marks GPU timing unavailable
in `GtsFrameStats`, and continues rendering.

## Counters

Representative counters include:

- authored, snapshot, visible, culled, and transparent renderables
- render commands, command updates, command cache hits, and command sorts
- object, camera, mesh, material, and texture upload or request counts
- uploaded byte estimates for object and camera side channels
- queued and synchronized materials, user invalidations, fallback
  substitutions, material reference adds/removes, and full material scans
- authored, selected, and dropped lights by type
- transform and render-GPU synchronization counts
- render-GPU full-scan visits, queued entries, drained entries, stale skips,
  missing-component skips, unchanged-version skips, changed syncs, matrix
  copies, object-upload requests, duplicate queue attempts, queue
  deduplications, and object-slot lookup failures
- logical object updates, object-upload commands, physical object-buffer
  writes, object write bytes, and contiguous write runs
- dynamic mesh queued candidates, unchanged skips, failed-version skips,
  changed meshes, invalid meshes, GPU allocations, GPU reallocations,
  in-place updates, processed vertices/indices, copied CPU bytes, uploaded
  vertex/index bytes, bounds recomputes, generated normal/tangent counts, and
  renderable invalidations
- particle emitter and world text counts

`object_uploads` and `object_upload_commands` are logical upload commands
generated by snapshot construction. `physical_object_buffer_writes` is the
number of backend object-buffer slot writes after frames-in-flight replication
and identical-data skips. `bytes_written_object_buffer` is based on physical
writes and the current `ObjectUBO` layout.

Backend-only counters such as descriptor binds, draw calls, pipeline switches,
physical object-buffer writes, and bytes written are present but zero in
`cpu_smoke` mode.

## JSON Result Schema

Results are JSON with these top-level fields:

- `benchmark`, `preset_version`, `mode`, `seed`
- `warmup_frames`, `measured_frames`, `render_resolution`
- `gpu_timing`
- `gpu_supported`
- `config`
- `environment`
- `timings_ms`
- `gpu_timings_ms`
- `controller_timings_ms`
- `controller_flush_timings_ms`
- `controller_substages_ms`
- `counters`
- `warnings`
- `invariant_failures`

Timing summaries include minimum, median, mean, p90, p95, p99, maximum,
standard deviation, and sample count.

`timings_ms` contains CPU wall-clock buckets. `gpu_timings_ms` contains only
real timestamp-query buckets and remains empty when GPU timing is unsupported or
unavailable.

`controller_timings_ms` entries also include a `group` field. All timing
sections use the same min/median/mean/p90/p95/p99/max/stddev/sample-count
summary shape.

## Invariant Checks

Benchmarks assert architectural expectations in addition to timings:

- material full scans must remain zero
- `RenderGpuSystem` full renderable scans must remain zero during measured
  frames
- steady-state material presets must not queue or synchronize material work
  after warmup
- static workloads must not produce object uploads after warmup
- `moving_static_control` must queue zero render-transform sync work, sync zero
  render transforms, and emit zero transform-driven object uploads after warmup
- visible renderables must not exceed snapshot renderables
- authored renderable counters must match measured frame count
- visible renderable workloads must produce visible renderables and render
  commands
- moving-object attribution presets must produce the configured logical update
  and object-upload command counts
- moving-object attribution presets must produce matching
  `render_gpu_changed_syncs` and `render_gpu_object_upload_requests`
- `dynamic_mesh_static_control` must process and upload zero geometry after
  warmup
- dynamic mesh mutation presets must process exactly
  `dynamic_mesh_mutation_count_per_frame * measured_frames`
- `dynamic_mesh_capacity_stable` must perform zero GPU reallocations after
  warmup
- `dynamic_mesh_growth` must reallocate during sufficiently long measured runs
- GPU timing metadata and counters must agree

These checks are stronger than noisy timing thresholds for many renderer
regressions.

## Baselines And Comparison

The comparison tool is:

```text
benchmarks/tools/compare_benchmark_results.py
```

Example:

```bash
python3 benchmarks/tools/compare_benchmark_results.py \
  --baseline benchmarks/baselines/<runner>/combined_game_like.json \
  --current out/combined_game_like.json \
  --metric median
```

The tool verifies benchmark compatibility, compares timing summaries from
`timings_ms`, `gpu_timings_ms`, `controller_timings_ms`,
`controller_flush_timings_ms`, and `controller_substages_ms` with relative plus
absolute thresholds, compares deterministic counters exactly for selected
counters, prints warnings and improvements, and returns nonzero on failures.

Baselines must be keyed by preset version, hardware or runner identity, build
configuration, render resolution, and graphics settings. Do not compare
developer-machine results against a universal baseline.

## CI Integration

Normal CI should run:

- `rendering_benchmark_support`
- `rendering_benchmark_static_geometry_smoke`
- `rendering_benchmark_compare_tool`

These are labeled `benchmark_smoke` and validate startup, schema, counters, and
invariants with small workloads.

Dedicated performance CI should use a Release build, fixed hardware, fixed
graphics settings, versioned baselines, artifact upload, and larger presets.
Shared CI runners are acceptable for correctness smoke tests, not authoritative
performance gates.

GPU runtime benchmarks should run only on dedicated Vulkan runners. A missing
Vulkan device should be treated as a skip, not a failure of CPU smoke CI.

The engine-root helper script runs the current GPU attribution matrix:

```bash
./run_gpu_rendering_benchmarks.sh /tmp/gts_rendering_benchmarks
```

The script writes one JSON file per preset and a full console transcript to
`gpu_benchmark_run.log` in the output directory.

## Adding A Preset

Add a `RenderingBenchmarkConfig` entry to `standardBenchmarkPresets()`, keep
the workload deterministic, assign a preset version, document what the preset
measures, and add invariant checks when the workload encodes an architectural
contract.

Prefer generated meshes, stable material definitions, fixed camera placement,
fixed mutation schedules, and engine-owned assets. Avoid external assets whose
contents or licenses can drift.

## Diagnosis Workflow

Start with `combined_game_like`. If `controller_cpu` dominates frame time,
inspect `controller_timings_ms` sorted by median. Use the moving-object presets
to isolate specific paths:

- `moving_static_control`: confirms steady-state transform/upload work reaches
  zero after warmup.
- `moving_independent`: isolates independent moving roots.
- `moving_sparse`: verifies sparse movement scales with changed renderables,
  not total renderable count.
- `moving_dense`: measures all-renderable movement throughput.
- `moving_deep_hierarchy`: creates root-strided transform chains and moves the
  roots to isolate depth propagation.
- `moving_wide_hierarchy`: attaches most objects directly under a smaller root
  set and moves the centered roots to isolate fan-out propagation.
- `upload_only_pressure`: maximizes transform-driven object upload pressure.
- `dynamic_mesh_static_control`: confirms unchanged dynamic meshes do no
  preparation or upload work after warmup.
- `dynamic_mesh_sparse_mutation`: confirms dynamic mesh cost scales with a
  small changed subset rather than all authored dynamic meshes.
- `dynamic_mesh_dense_mutation`: measures worst-case mutation throughput.
- `dynamic_mesh_capacity_stable`: validates the in-place update path when
  vertex/index counts fit existing capacity.
- `dynamic_mesh_growth`: measures reallocation policy when geometry exceeds
  capacity.
- `dynamic_mesh_attribute_generation`: isolates preparation cost for meshes
  missing normals/tangents.
- `dynamic_mesh_precomputed_attributes`: separates upload and lifecycle cost
  from attribute generation.

Use counters to distinguish logical object changes from backend work. If
`logical_object_updates` matches `object_upload_commands` but
`physical_object_buffer_writes` is a multiple, the cost is likely in
frames-in-flight replication or mapped-buffer writes. If
`RenderGpuSystem` dominates while `render_gpu_full_scan_visited` remains zero,
inspect `render_gpu_changed_syncs`, `render_gpu_object_upload_requests`,
matrix-copy substages, and backend object-buffer write counters before choosing
between upload-command enqueueing, physical object-buffer writes, or transform
resolution as the next target.
If `DynamicMeshBindingSystem` dominates, inspect the dynamic mesh substage
timings and counters. Static-control work should be near zero after warmup;
sparse mutation should report changed meshes equal to the configured mutation
count; capacity-stable updates should report in-place updates and zero
reallocations. A high `geometry_preparation` bucket points at CPU vertex/index
processing, while high upload buckets point at backend buffer writes.

## Current Limitations

- `cpu_smoke` does not exercise Vulkan command recording, prepared batch
  construction, descriptor binds, queue submission, present, or real GPU
  fragment cost.
- UI stress is represented in config and preset identity but retained-UI
  workload generation is not yet wired into the smoke generator.
- GPU runtime workload generation uses generated quad/procedural geometry and
  engine fallback textures; asset-heavy benchmark fixtures can be added later.
- Timestamp overhead is optional and disabled outside GPU benchmark mode by
  default. This workspace could validate the disabled CPU smoke path and the
  no-Vulkan skip path, but not timestamp overhead on real GPU hardware.
