#!/usr/bin/env python3
"""Compare compatible CPU scaling reports without performance pass/fail gates."""
import argparse
import json
import sys


def compare(before, after, allow_build_difference=False):
    for key in ("schema_version", "workload_version", "settings"):
        if before[key] != after[key]:
            raise ValueError(f"Incompatible {key}")
    keys = ["fixture_sha256"]
    if not allow_build_difference:
        keys += ["configuration", "compiler", "compiler_path", "cxx_flags", "platform"]
    for key in keys:
        if before["build"][key] != after["build"][key]:
            raise ValueError(f"Incompatible build field {key}; use --allow-build-difference for an explicit raw comparison")

    def index(report):
        result = {}
        for run in report["runs"]:
            key = (run["workload"], run["parameters"]["count"])
            if key in result:
                raise ValueError(f"Duplicate case: {key}")
            result[key] = run
        return result

    left, right = index(before), index(after)
    if left.keys() != right.keys():
        raise ValueError("Different workload/size sets")
    rows = []
    for key, old in left.items():
        new = right[key]
        if old["status"] != new["status"] or old["parameters"] != new["parameters"]:
            raise ValueError(f"Status/parameters differ for {key}")
        if old["status"] != "ok":
            rows.append(f"{key[0]} count={key[1]}: {old['status']} (no timing comparison)")
            continue
        expected = old["parameters"]["measured_samples"]
        if len(old["samples"]) != expected or len(new["samples"]) != expected:
            raise ValueError(f"Incomplete samples for {key}")
        for a, b in zip(old["samples"], new["samples"]):
            if a["tick"] != b["tick"] or a["counters"] != b["counters"]:
                raise ValueError(f"Work counters differ for {key}")
            if a["timings_ms"].keys() != b["timings_ms"].keys() or any(
                (value is None) != (b["timings_ms"][name] is None)
                for name, value in a["timings_ms"].items()
            ):
                raise ValueError(f"Sample metric availability differs for {key}")
        if old["aggregate_timings_ms"].keys() != new["aggregate_timings_ms"].keys():
            raise ValueError(f"Different aggregate metric sets for {key}")
        for name, a in old["aggregate_timings_ms"].items():
            b = new["aggregate_timings_ms"].get(name)
            if (a is None) != (b is None):
                raise ValueError(f"Metric availability differs for {key}: {name}")
            if a is None:
                continue
            delta = "n/a (zero baseline)" if a["median"] == 0 else f"{100 * (b['median'] / a['median'] - 1):+.2f}%"
            rows.append(f"{key[0]} count={key[1]} {name}: {a['median']:.6f} -> {b['median']:.6f} ms ({delta})")
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("before")
    parser.add_argument("after")
    parser.add_argument("--allow-build-difference", action="store_true")
    args = parser.parse_args()
    try:
        with open(args.before, encoding="utf-8") as stream:
            before = json.load(stream)
        with open(args.after, encoding="utf-8") as stream:
            after = json.load(stream)
        if args.allow_build_difference:
            print("Explicit cross-build comparison: raw timings only; no Debug/Release normalization.")
        for report in (before, after):
            print("Build:", report["build"])
        print("\n".join(compare(before, after, args.allow_build_difference)))
        return 0
    except (ValueError, KeyError, OSError) as error:
        print(f"Cannot compare: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
