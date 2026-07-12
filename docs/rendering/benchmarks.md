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

`gpu_runtime` is reserved for the future Vulkan timestamp implementation. The
current executable reports GPU timing unavailable rather than substituting CPU
queue-submit time for GPU work.

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
- `transform_resolve_cpu`
- `render_gpu_sync_cpu`
- `snapshot_build_cpu`
- `visibility_cpu`
- `command_extract_cpu`
- `command_sort_cpu`

`gpu_frame` is emitted as a schema-stable placeholder with GPU timing metadata
marking it unavailable. Do not interpret it as GPU work until Vulkan timestamp
queries are implemented.

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
- particle emitter and world text counts

Backend-only counters such as prepared batches, descriptor binds, draw calls,
and pipeline switches are present but zero in `cpu_smoke` mode.

## JSON Result Schema

Results are JSON with these top-level fields:

- `benchmark`, `preset_version`, `mode`, `seed`
- `warmup_frames`, `measured_frames`, `render_resolution`
- `gpu_timing`
- `config`
- `environment`
- `timings_ms`
- `counters`
- `warnings`
- `invariant_failures`

Timing summaries include minimum, median, mean, p90, p95, p99, maximum,
standard deviation, and sample count.

## Invariant Checks

Benchmarks assert architectural expectations in addition to timings:

- material full scans must remain zero
- steady-state material presets must not queue or synchronize material work
  after warmup
- static workloads must not produce object uploads after warmup
- visible renderables must not exceed snapshot renderables
- authored renderable counters must match measured frame count
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

The tool verifies benchmark compatibility, compares timing summaries with
relative plus absolute thresholds, compares deterministic counters exactly for
selected counters, prints warnings and improvements, and returns nonzero on
failures.

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

## Adding A Preset

Add a `RenderingBenchmarkConfig` entry to `standardBenchmarkPresets()`, keep
the workload deterministic, assign a preset version, document what the preset
measures, and add invariant checks when the workload encodes an architectural
contract.

Prefer generated meshes, stable material definitions, fixed camera placement,
fixed mutation schedules, and engine-owned assets. Avoid external assets whose
contents or licenses can drift.

## Current Limitations

- GPU timestamp queries are not implemented.
- `cpu_smoke` does not exercise Vulkan command recording, prepared batch
  construction, descriptor binds, queue submission, present, or real GPU
  fragment cost.
- UI stress is represented in config and preset identity but retained-UI
  workload generation is not yet wired into the smoke generator.
- Instrumentation overhead has not been quantified beyond keeping benchmark
  counters inside benchmark runs and reusing existing renderer metrics.
