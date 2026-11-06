#!/usr/bin/env python3
"""Compare conservative Cargo inventories; never remove distribution notices."""

import argparse
import json
from pathlib import Path
import subprocess

from check_upstream import ROOT, check
from package_size import tree_bytes


def external_packages(metadata, all_features):
    """Retain all dependency kinds and all workspace roots, including tools/tests."""
    packages = {package["id"]: package for package in metadata["packages"]}
    nodes = {node["id"]: node for node in metadata["resolve"]["nodes"]}
    roots = metadata["workspace_members"]
    if not roots:
        raise ValueError("No workspace roots to audit")
    if all_features:
        for root in roots:
            if root not in packages or root not in nodes:
                raise ValueError("Incomplete Cargo workspace graph")
            if not set(packages[root]["features"]).issubset(nodes[root]["features"]):
                raise ValueError("Cargo did not enable all workspace features")
    visited = set()
    pending = list(roots)
    while pending:
        identifier = pending.pop()
        if identifier in visited:
            continue
        if identifier not in nodes or identifier not in packages:
            raise ValueError("Incomplete Cargo dependency graph")
        visited.add(identifier)
        # Do not drop build/dev edges or parse Cargo's opaque package IDs.
        pending.extend(nodes[identifier]["dependencies"])
    return {
        (packages[key]["name"], packages[key]["version"], packages[key]["source"]): packages[key]
        for key in visited
        if packages[key].get("source")
    }


def metadata_inventory(target, all_features):
    command = [
        "cargo",
        "metadata",
        "--offline",
        "--locked",
        "--format-version",
        "1",
        "--manifest-path",
        "vendor/ladybird/Cargo.toml",
    ]
    if target:
        command.extend(["--filter-platform", target])
    if all_features:
        command.append("--all-features")
    metadata = json.loads(subprocess.check_output(command, cwd=ROOT, text=True, encoding="utf-8"))
    return external_packages(metadata, all_features), command


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--notices", required=True, type=Path, help="Existing package licenses directory"
    )
    parser.add_argument("--target", required=True, help="Cargo target triple to analyze")
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    revision = check(ROOT)
    baseline_pin = json.loads((args.notices / "ladybird-pin.json").read_text(encoding="utf-8"))
    if baseline_pin["revision"] != revision:
        parser.error("Notice baseline uses a different Ladybird pin")
    baseline = json.loads((args.notices / "rust-packages.json").read_text(encoding="utf-8"))
    old = {(row["name"], row["version"], row["source"]): row for row in baseline}
    if len(old) != len(baseline):
        parser.error("Duplicate baseline package identities")
    if len({key[:2] for key in old}) != len(old):
        parser.error("Baseline notice directory names collide across sources")
    platform = {}
    unfiltered = {}
    commands = []
    for all_features in [False, True]:
        for target, inventory in [(None, unfiltered), (args.target, platform)]:
            packages, command = metadata_inventory(target, all_features)
            inventory.update(packages)
            commands.append(command)
    excluded = []
    for key in sorted(old.keys() - platform.keys()):
        directory = args.notices / "rust" / (key[0] + "-" + key[1])
        if not directory.is_dir():
            raise ValueError("Baseline notice directory missing: " + str(directory))
        excluded.append({**old[key], "noticeBytes": tree_bytes(directory)})
    added = [
        {name: platform[key].get(name) for name in ["name", "version", "source", "license"]}
        for key in sorted(platform.keys() - old.keys())
    ]
    report = {
        "analysisOnly": True,
        "noticesRemoved": False,
        "ladybird": revision,
        "target": args.target,
        "scope": "all workspace roots; all dependency kinds; default/all-feature union",
        "commands": commands,
        "baselinePackages": len(old),
        "unfilteredUnionPackages": len(unfiltered),
        "platformUnionPackages": len(platform),
        "excludedCandidates": excluded,
        "additionalCandidates": added,
        "excludedCandidateBytes": sum(row["noticeBytes"] for row in excluded),
        "netSavingBytes": None,
        "remainingProof": "Match actual build target/host, flags, selected roots/features and generated-code inputs before changing distribution inventory.",
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(
        f"Analysis only: {len(old)} baseline / {len(platform)} platform-union packages; "
        f"{len(excluded)} excluded and {len(added)} additional candidates. Notices unchanged."
    )


if __name__ == "__main__":
    main()
