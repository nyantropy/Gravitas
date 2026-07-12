#!/usr/bin/env python3
import argparse
import json
import math
import sys


DEFAULT_EXACT_COUNTERS = {
    "draw_calls",
    "dynamic_mesh_bounds_recomputed",
    "dynamic_mesh_changed",
    "dynamic_mesh_failed_version_skipped",
    "dynamic_mesh_gpu_reallocations",
    "dynamic_mesh_in_place_updates",
    "dynamic_mesh_invalid",
    "material_full_scans",
    "material_synchronized",
    "object_uploads",
    "prepared_batches",
    "render_commands",
}


def load_json(path):
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def timing_value(result, bucket, metric):
    return result.get("timings_ms", {}).get(bucket, {}).get(metric)


def compatible(current, baseline, allow_hardware_mismatch):
    failures = []
    for key in ("benchmark", "preset_version", "mode"):
        if current.get(key) != baseline.get(key):
            failures.append(f"{key} differs: current={current.get(key)} baseline={baseline.get(key)}")

    if current.get("render_resolution") != baseline.get("render_resolution"):
        failures.append("render_resolution differs")

    if not allow_hardware_mismatch:
        current_env = current.get("environment", {})
        baseline_env = baseline.get("environment", {})
        for key in ("cpu", "gpu", "build_type"):
            if current_env.get(key) != baseline_env.get(key):
                failures.append(f"environment.{key} differs")

    return failures


def compare_timing_section(current, baseline, section, metric, fail_relative, fail_absolute, warn_relative):
    failures = []
    warnings = []
    improvements = []
    current_timings = current.get(section, {})
    baseline_timings = baseline.get(section, {})

    missing_current = sorted(set(baseline_timings) - set(current_timings))
    missing_baseline = sorted(set(current_timings) - set(baseline_timings))
    for bucket in missing_current:
        warnings.append(f"{section}.{bucket} missing from current result")
    for bucket in missing_baseline:
        warnings.append(f"{section}.{bucket} missing from baseline result")

    for bucket in sorted(set(current_timings) & set(baseline_timings)):
        current_value = current_timings.get(bucket, {}).get(metric)
        baseline_value = baseline_timings.get(bucket, {}).get(metric)
        if current_value is None or baseline_value is None:
            continue
        if not math.isfinite(current_value) or not math.isfinite(baseline_value):
            failures.append(f"{bucket}.{metric} contains non-finite values")
            continue
        delta = current_value - baseline_value
        relative = 0.0 if baseline_value == 0 else delta / baseline_value
        if delta > fail_absolute and relative > fail_relative:
            failures.append(
                f"{section}.{bucket}.{metric} regressed by {relative * 100:.1f}% "
                f"({baseline_value:.6f} -> {current_value:.6f} ms)"
            )
        elif relative > warn_relative:
            warnings.append(
                f"{section}.{bucket}.{metric} warning regression {relative * 100:.1f}% "
                f"({baseline_value:.6f} -> {current_value:.6f} ms)"
            )
        elif relative < -warn_relative:
            improvements.append(
                f"{section}.{bucket}.{metric} improved {-relative * 100:.1f}% "
                f"({baseline_value:.6f} -> {current_value:.6f} ms)"
            )
    return failures, warnings, improvements


def compare_timings(current, baseline, metric, fail_relative, fail_absolute, warn_relative):
    failures = []
    warnings = []
    improvements = []
    for section in (
        "timings_ms",
        "gpu_timings_ms",
        "controller_timings_ms",
        "controller_flush_timings_ms",
        "controller_substages_ms",
    ):
        section_failures, section_warnings, section_improvements = compare_timing_section(
            current,
            baseline,
            section,
            metric,
            fail_relative,
            fail_absolute,
            warn_relative,
        )
        failures.extend(section_failures)
        warnings.extend(section_warnings)
        improvements.extend(section_improvements)
    return failures, warnings, improvements


def compare_counters(current, baseline, exact_counters):
    failures = []
    current_counters = current.get("counters", {})
    baseline_counters = baseline.get("counters", {})
    for counter in sorted(exact_counters):
        if counter not in current_counters or counter not in baseline_counters:
            continue
        current_value = current_counters[counter]
        baseline_value = baseline_counters[counter]
        if current_value != baseline_value:
            failures.append(
                f"{counter} differs: baseline={baseline_value} current={current_value}"
            )
    return failures


def main():
    parser = argparse.ArgumentParser(description="Compare Gravitas benchmark JSON results.")
    parser.add_argument("--current", required=True)
    parser.add_argument("--baseline", required=True)
    parser.add_argument("--metric", default="median", choices=["median", "mean", "p90", "p95", "p99"])
    parser.add_argument("--fail-relative", type=float, default=0.10)
    parser.add_argument("--fail-absolute-ms", type=float, default=0.10)
    parser.add_argument("--warn-relative", type=float, default=0.05)
    parser.add_argument("--allow-hardware-mismatch", action="store_true")
    parser.add_argument("--exact-counter", action="append", default=[])
    args = parser.parse_args()

    current = load_json(args.current)
    baseline = load_json(args.baseline)

    failures = compatible(current, baseline, args.allow_hardware_mismatch)
    warnings = []
    improvements = []

    if not failures:
        timing_failures, timing_warnings, timing_improvements = compare_timings(
            current,
            baseline,
            args.metric,
            args.fail_relative,
            args.fail_absolute_ms,
            args.warn_relative,
        )
        counter_failures = compare_counters(
            current,
            baseline,
            DEFAULT_EXACT_COUNTERS | set(args.exact_counter),
        )
        failures.extend(timing_failures)
        failures.extend(counter_failures)
        warnings.extend(timing_warnings)
        improvements.extend(timing_improvements)

    for message in warnings:
        print(f"WARN: {message}")
    for message in improvements:
        print(f"IMPROVED: {message}")
    for message in failures:
        print(f"FAIL: {message}", file=sys.stderr)

    if failures:
        return 2

    print("benchmark comparison passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
