# CPU scaling benchmarks

`GtsPerformanceBenchmarks` is a separate application for explicit performance
experiments. It measures fixed work through public ECS/module APIs, independent
of DungeonCrawler, the runtime catch-up loop, and ordinary correctness tests.
It does not optimize or change engine behavior. There are no performance gates.

## Build and run

From the engine repository root:

```sh
cmake -S . -B build/performance-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DGTS_BUILD_PERFORMANCE_BENCHMARKS=ON -DGTS_BUILD_TEST_SCENES=OFF
cmake --build build/performance-debug --target GtsPerformanceBenchmarks
build/performance-debug/applications/GtsPerformanceBenchmarks/GtsPerformanceBenchmarks \
  --suite quick --output reports/debug-quick.json

cmake -S . -B build/performance-release -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DGTS_BUILD_PERFORMANCE_BENCHMARKS=ON -DGTS_BUILD_TEST_SCENES=OFF
cmake --build build/performance-release --target GtsPerformanceBenchmarks
build/performance-release/applications/GtsPerformanceBenchmarks/GtsPerformanceBenchmarks \
  --suite quick --output reports/release-quick.json
```

On Windows use the executable's `.exe` suffix and the selected configuration
subdirectory when using a multi-configuration generator. The fixture is copied
into the build directory and addressed independently of the working directory.
Keep that fixture with the build when running the executable.

The target is enabled by `GTS_BUILD_PERFORMANCE_BENCHMARKS` (default ON), separately
from validation scenes. It consumes core, transform/animation, model, and runtime
execution contracts through explicit targets. It does not require the complete
`GravitasEngine` facade or Vulkan: rendering/backend/physics/tools-disabled
capability-development builds can still build it when those CPU module targets
exist. No module depends on this application. The normal three-layer checks
remain unchanged.

```sh
GtsPerformanceBenchmarks --list
GtsPerformanceBenchmarks --suite scaling --output scaling.json
GtsPerformanceBenchmarks --workload moving-transforms --count 100000 \
  --warmup 5 --samples 30 --seed 1337 --dt 0.0166666666666667 --output moving.json
GtsPerformanceBenchmarks --suite scaling --sizes 1000,10000,25000 \
  --warmup 3 --samples 10 --output short-ladder.json
GtsPerformanceBenchmarks --self-test
```

The quick suite uses 1,000 instances, two warmup ticks and five measured samples.
Scaling uses the explicit ladder **1k, 10k, 25k, 50k, 100k, 250k, 500k, 1M**, with
three warmup ticks and ten samples. Override sizes/warmup/samples explicitly.
The same defaults, workloads, instrumentation and JSON schema apply to Debug
and Release. No optimization flags are added to Debug.

Million-entity runs are expensive, especially in Debug, and may consume substantial
memory. Invoke them deliberately; they are never registered with CTest. Model
workloads have a conservative 100k ceiling because each occurrence owns an instance
and extraction builds/releases a CPU draw frame. This is a benchmark resource
budget, not an engine/GPU capability limit. Raising it requires deliberate
revalidation of the workload's memory/time cost.

## Workloads, version 1

| Workload | Ceiling | Measured work |
|---|---:|---|
| static-transforms | 1M | Clean flat transforms, normal ECS dispatch/resolver |
| moving-transforms | 1M | App simulation system authors rotation and marks every transform dirty; normal resolution |
| transform-animation | 1M | Engine TransformAnimationSystem rotation plus normal resolution |
| static-model-instances | 100k | Independent static instances, shared geometry/base materials, CPU model extraction |
| moving-model-instances | 100k | Author transforms, resolve, extract model frame |
| shared-material-model-instances | 100k | Static instances with one shared model-wide override material |
| varied-material-model-instances | 100k | Static instances with a distinct material allocation/override per instance |

The model fixture reuses `tests/assets/fixtures/canonical-cooking/triangle.obj`
as data, without including test or private implementation headers. The public
model registry imports it; realization and instance creation use their ordinary
world-owned services with a null resource provider for CPU-only realization.
The report fingerprints the fixture. Each occurrence contributes one authored
triangle: this isolates **instance overhead**, not high-poly geometry throughput.
All instances are independently owned; only immutable definitions/geometry and
base materials are shared. Source fallback policy is respected. A cooked-only
policy or an unexpected colocated cooked fixture produces an explicit failure,
not a switch to a different asset pipeline.

`mostly-visible-model-instances`, `mostly-culled-model-instances`, and
`skinned-model-instances` are explicitly deferred. The first two need a deliberate
camera/frustum and rendering-visibility integration workload; the third needs a
versioned rig/clip fixture, playback and palette work. They are listed in the
catalog and produce `unsupported` result records. No fake culling, skeletal
work, or private-header shortcuts substitute for these missing workloads.

Suites print and record every requested case, including deferred workloads and
sizes above documented ceilings. These expected `unsupported` cases do not fail
a suite; any actual setup/warmup/measurement failure does. A direct unsupported
workload/size request exits nonzero. Unknown/invalid CLI values also exit nonzero.
Completed/failed results are written incrementally between cases so a later failed
case does not erase earlier evidence. Process termination/OOM can leave a partial
report; inspect exit status and requested cases before comparing results.

## Measurement boundaries and interpretation

Each sample runs **one simulation tick → controllers in registration order →
optional CPU model extraction**. Simulation delta is explicit and passed to ECS
as a float. This is an isolated fixed-work driver, not a substitute runtime loop:
there is no catch-up, window, GPU, camera, UI, tooling, physics or rendering system
installation in these workloads. The runtime's catch-up/pause/scheduling policy
is untouched. The application supplies the normal gameplay execution selection.

Setup includes model loading/import, CPU realization, world/entity/instance/material
creation and an initial transform resolution. It is timed separately as `setup_ms`.
Warmup is excluded from measured samples. World teardown and report serialization
are also outside the frame timing. Each case uses a fresh world and services.
There is no cooking or GPU resource realization in these CPU workloads.

Available timings, in milliseconds:

- `simulation_ms`: world simulation dispatch including structural command flushing.
- `controller_ms`: world controller dispatch, including the single TransformSystem.
- `transform_ms`: authoritative `TransformSystem::getLastMetrics().cpuTimeMs`.
  Detailed per-entity transform timing instrumentation remains disabled.
- `animation_ms`: only for transform-animation; the simulation interval, including
  dispatch/flush for its sole animation system. It is not a second independent phase.
- `model_extraction_ms`: public `extractModelFrame`, including frame destruction.
- `cpu_frame_ms`: the complete measured interval. Nested timings are not additive.

`gpu_frame_ms`, `render_gpu_ms`, `render_preparation_ms` and `snapshot_ms` are null.
The corresponding renderer/backend stages are not run. CPU model extraction is
not labeled as the renderer snapshot builder or GPU submission. Candidate draw
counts come from the extracted frame, not estimates of visibility or draw calls.
Visible-instance, submitted-draw and GPU-submission counts are null with explicit
unavailability reasons. Transform-only workloads have null model extraction and
draw candidate metrics, and zero model instances/authored triangles.

Per-sample counters include actual entity count (including module service singleton
entities, so model worlds contain more entities than their requested instance count), successfully created model
instances, authored triangle count across occurrences, distinct override materials,
engine transform queue/process/publication counts, and extracted static/skinned
draw candidates. Setup counts stay fixed during these workloads. No per-frame
full-world validation scan is added solely to collect counters.

JSON schema version 1 includes workload version, CLI invocation, timestamp,
engine commit/dirty state/working-change fingerprint, compiler/configuration/flags,
platform, hardware thread count, fixture SHA-256, workload parameters, settings,
setup time, every raw sample and per-phase aggregate statistics. Missing timing
aggregates are null. Statistics include min, median, mean, nearest-rank p95, max,
population standard deviation and sample count. Hardware thread count is not a CPU
model identifier; preserve machine/OS/power-profile notes alongside long-term baselines.
Git metadata is refreshed at build time even when only the commit changes. The
active build directory is excluded from the untracked-file fingerprint.

## Comparing reports

```sh
python3 applications/GtsPerformanceBenchmarks/compare_reports.py before.json after.json
```

The tool verifies schema/workload versions, settings, fixture, workload sets,
parameters, sample counts, tick indices and **all work counters** before printing
raw median deltas per available phase. It rejects incompatible build settings by
default. Use `--allow-build-difference` for an explicit cross-build comparison;
values remain raw, with no Debug/Release normalization. There are no hard timing
thresholds. Commit/dirty fingerprints are printed so code changes are visible.

Run on the same machine without concurrent builds or other heavy workloads, use
multiple repetitions, and retain raw samples. A changed workload definition should
increment `workload_version`; a schema change should increment `schema_version`.
A zero/near-zero clean-world baseline is not useful as a percentage speedup.

`GtsRenderingBenchmarks` retains its existing specialized renderer/Vulkan,
visibility, snapshot, submission and GPU timestamp coverage. This application
adds a workload/size harness over CPU capability paths; it neither wraps rendering
benchmark modes nor changes their instrumentation. Future rendering/GPU/visibility
adapters can extend this harness with the same explicit availability rules.

CTest registers only the eight-entity C++ self-test and a small JSON/CLI/determinism
probe at 8/16 entities. Neither quick-at-1k nor scaling suites run automatically.
