#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

BUILD_DIR="${BUILD_DIR:-build_release}"
OUT_DIR="${1:-/tmp/gts_rendering_benchmarks}"
BENCHMARK_EXE="$BUILD_DIR/applications/GtsRenderingBenchmarks/GtsRenderingBenchmarks"

mkdir -p "$OUT_DIR"

echo "== Building GtsRenderingBenchmarks =="
cmake --build "$BUILD_DIR" --target GtsRenderingBenchmarks

if command -v vulkaninfo >/dev/null 2>&1; then
    echo
    echo "== Vulkan summary =="
    vulkaninfo --summary || true
fi

echo
echo "== GPU sanity benchmark =="
"$BENCHMARK_EXE" \
    --preset static_geometry_small \
    --mode gpu_runtime \
    --warmup-frames 10 \
    --measured-frames 30 \
    --renderable-count 256 \
    --visible-renderable-count 256 \
    --unique-mesh-count 4 \
    --unique-material-count 4 \
    --output "$OUT_DIR/static_geometry_small_gpu.json" \
    --check-invariants

echo
echo "== GPU combined benchmark =="
"$BENCHMARK_EXE" \
    --preset combined_game_like \
    --mode gpu_runtime \
    --warmup-frames 120 \
    --measured-frames 600 \
    --output "$OUT_DIR/combined_game_like_gpu.json" \
    --check-invariants

for preset in \
    moving_static_control \
    moving_independent \
    moving_deep_hierarchy \
    moving_wide_hierarchy \
    upload_only_pressure
do
    echo
    echo "== GPU attribution benchmark: $preset =="
    "$BENCHMARK_EXE" \
        --preset "$preset" \
        --mode gpu_runtime \
        --warmup-frames 120 \
        --measured-frames 600 \
        --output "$OUT_DIR/${preset}_gpu.json" \
        --check-invariants
done

echo
echo "Benchmark JSON written to:"
echo "  $OUT_DIR/static_geometry_small_gpu.json"
echo "  $OUT_DIR/combined_game_like_gpu.json"
echo "  $OUT_DIR/moving_static_control_gpu.json"
echo "  $OUT_DIR/moving_independent_gpu.json"
echo "  $OUT_DIR/moving_deep_hierarchy_gpu.json"
echo "  $OUT_DIR/moving_wide_hierarchy_gpu.json"
echo "  $OUT_DIR/upload_only_pressure_gpu.json"
