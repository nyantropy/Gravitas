"""Small schema/CLI/determinism checks. Never executes the scaling ladder."""
import copy
import importlib.util
import json
import math
import os
import statistics
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
exe = Path(sys.argv[1])
spec = importlib.util.spec_from_file_location("compare_reports", Path(__file__).parents[1] / "compare_reports.py")
comparison = importlib.util.module_from_spec(spec)
spec.loader.exec_module(comparison)


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


with tempfile.TemporaryDirectory(prefix="gts-performance-test-") as directory:
    output = Path(directory) / "report.json"

    def run(*args, success=True, env=None):
        process = subprocess.run([str(exe), *args, "--output", str(output)], capture_output=True, text=True, env=env)
        require((process.returncode == 0) == success, process.stdout + process.stderr)
        return process

    args = ["--suite", "quick", "--sizes", "8,16", "--warmup", "1", "--samples", "2"]
    run(*args)
    first = json.loads(output.read_text())
    run(*args)
    second = json.loads(output.read_text())
    comparison.compare(first, second)
    require(first["schema_version"] == 1 and len(first["runs"]) == 20, "Wrong report cases")
    ok = [case for case in first["runs"] if case["status"] == "ok"]
    require(len(ok) == 14, "Expected seven implemented workloads at two sizes")
    for case in ok:
        require(len(case["samples"]) == 2, "Wrong sample count")
        require(case["setup_ms"] >= 0, "Missing separate setup measurement")
        count = case["parameters"]["count"]
        for sample in case["samples"]:
            require(sample["counters"]["entities"] >= count, "Wrong entity count")
            require(sample["timings_ms"]["gpu_frame_ms"] is None, "Fabricated GPU metric")
            require(sample["counters"]["submitted_draws"] is None, "Fabricated submission count")
            for timing in sample["timings_ms"].values():
                require(timing is None or math.isfinite(timing) and timing >= 0, "Invalid timing")
        for name, aggregate in case["aggregate_timings_ms"].items():
            values = [sample["timings_ms"][name] for sample in case["samples"]
                      if sample["timings_ms"][name] is not None]
            require((aggregate is None) == (not values), "Unavailable aggregate must be null")
            if values:
                require(aggregate["count"] == len(values), "Wrong aggregate sample count")
                require(math.isclose(aggregate["median"], statistics.median(values), rel_tol=1e-8, abs_tol=1e-8),
                        "Wrong median")
                require(math.isclose(aggregate["mean"], statistics.mean(values), rel_tol=1e-8, abs_tol=1e-8),
                        "Wrong mean")
    altered = copy.deepcopy(second)
    altered["runs"][0]["samples"][0]["counters"]["entities"] += 1
    try:
        comparison.compare(first, altered)
    except ValueError:
        pass
    else:
        raise RuntimeError("Counter mismatch comparison was accepted")
    altered = copy.deepcopy(second)
    altered["build"]["configuration"] = "DifferentBuild"
    try:
        comparison.compare(first, altered)
    except ValueError:
        pass
    else:
        raise RuntimeError("Implicit cross-build comparison was accepted")
    comparison.compare(first, altered, True)
    run("--suite", "scaling", "--sizes", "8", "--warmup", "0", "--samples", "1")
    require(len(json.loads(output.read_text())["runs"]) == 10, "Scaling override lost workload cases")
    run("--workload", "skinned-model-instances", "--count", "8", success=False)
    require(json.loads(output.read_text())["runs"][0]["status"] == "unsupported", "Missing deferred status")
    run("--workload", "static-model-instances", "--count", "250000", success=False)
    require(json.loads(output.read_text())["runs"][0]["status"] == "unsupported", "Missing ceiling status")
    run("--workload", "moving-transforms", "--count", "8", "--warmup", "0", "--samples", "1")
    require(json.loads(output.read_text())["runs"][0]["samples"][0]["tick"] == 0, "Warmup-zero handling")
    for args in [["--samples", "0"], ["--count", "0"], ["--dt", "nan"], ["--sizes", "1,1"],
                 ["--samples", "-1"], ["--suite", "unknown"], ["--workload", "unknown"],
                 ["--suite", "quick", "--workload", "moving-transforms"]]:
        run(*args, success=False)
    env = dict(os.environ, GTS_RUNTIME_ASSET_POLICY="strict")
    run("--workload", "static-model-instances", "--count", "8", success=False, env=env)
    failed = json.loads(output.read_text())["runs"][0]
    require(failed["status"] == "failed" and failed["message"], "Setup failure was not recorded")
print("Performance report schema, CLI, failure reporting, and deterministic counters passed")
