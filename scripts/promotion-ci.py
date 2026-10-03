#!/usr/bin/env python3
"""Run missing promotion checks and admit the current candidate from real evidence.

No release publishing, branch writes, protection changes, or whole-tree hashing.
"""
from __future__ import annotations
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import xml.etree.ElementTree as ET
import zipfile
import stat

from promotion_evidence import (build_identity, git_inputs, plan_tests, release_admission,
                                select_tests, test_identity, validate_source)
from promotion_examples import discover_examples
from promotion_platform import probe_environment, detect_platform

ROOT = Path(__file__).resolve().parents[1]
POLICY = ROOT / "configs/promotion-ci.json"
BUILD = ROOT / "build/promotion"
OUT = ROOT / "build/promotion-evidence"
BUILD_PATHS = ["src", "cmake", "CMakeLists.txt", "FindICU.cmake", "configs/styio-nano-default.toml",
               "tests/CMakeLists.txt", "tests/*/CMakeLists.txt", "tests/*.cpp", "tests/*.hpp", "benchmark/CMakeLists.txt",
               "benchmark/*.cpp", "grammar", ".github/workflows/styio-ci-gate.yml",
               "scripts/promotion_platform.py", "scripts/gen-styio-nano-profile.py"]
TEST_PATHS = ["tests", "example", "scripts", "benchmark", "configs/promotion-ci.json"]
DISTRIBUTION_PATHS = ["LICENSE", "NOTICE", "README.md", "README_zh.md", "CHANGELOG.md",
                      "RELEASE-POLICY.md", "DEPENDENCY-USAGE.md", "LICENSE-POLICY.md", "example"]
POLICY_PATHS = ["configs/promotion-ci.json", "scripts/promotion-ci.py", "scripts/promotion_evidence.py",
                "scripts/promotion_examples.py", ".github/workflows/styio-ci-gate.yml"]


def command(args, *, capture=False, check=True, **kwargs):
    return subprocess.run([str(x) for x in args], cwd=ROOT, text=True,
                          capture_output=capture, check=check, **kwargs)


def api(path):
    return json.loads(command(["gh", "api", path], capture=True).stdout)


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def checksum(path):
    # Distribution payload integrity only; never hash repository contents.
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def extract_archive(archive, destination):
    destination.mkdir(parents=True, exist_ok=True)
    with tarfile.open(archive) as handle:
        for member in handle.getmembers():
            target = (destination / member.name).resolve()
            if not target.is_relative_to(destination.resolve()) or member.issym() or member.islnk():
                raise ValueError("artifact contains unsafe path or link")
        handle.extractall(destination, filter="data")


def pack(directory, archive):
    archive.parent.mkdir(parents=True, exist_ok=True)
    with tarfile.open(archive, "w:gz", compresslevel=1) as handle:
        for path in sorted(directory.rglob("*")):
            if path.is_symlink():
                continue
            if path.is_file():
                handle.add(path, arcname=path.relative_to(directory).as_posix(), recursive=False)


def source_metadata(repo, run, artifact):
    details = api(f"repos/{repo}/actions/runs/{run['id']}")
    jobs = api(f"repos/{repo}/actions/runs/{run['id']}/jobs?per_page=100")["jobs"]
    gate = next(job for job in jobs if job["name"] == "styio-ci-gate")
    metadata = {"run": details, "artifact": artifact, "check": api(gate["check_run_url"])}
    branch_name = run["head_branch"]
    if details["event"] == "pull_request":
        associated = details.get("pull_requests", [])
        if len(associated) != 1:
            raise ValueError("PR evidence lacks one unambiguous source pull request")
        pr = api(f"repos/{repo}/pulls/{associated[0]['number']}")
        metadata["pull_request"] = pr
        branch_name = pr["base"]["ref"]
    metadata["branch"] = api(f"repos/{repo}/branches/{branch_name}")
    return metadata


def download_artifact(repo, artifact, destination):
    """Download the exact API-validated artifact ID, never an ambiguous name."""
    destination.mkdir(parents=True, exist_ok=True)
    archive = destination / ".download.zip"
    with archive.open("wb") as stream:
        subprocess.run(["gh", "api", f"repos/{repo}/actions/artifacts/{artifact['id']}/zip"],
                       cwd=ROOT, stdout=stream, check=True)
    with zipfile.ZipFile(archive) as handle:
        for member in handle.infolist():
            target = (destination / member.filename).resolve()
            if not target.is_relative_to(destination.resolve()) or stat.S_ISLNK(member.external_attr >> 16):
                raise ValueError("evidence artifact contains unsafe paths or links")
        handle.extractall(destination)
    archive.unlink()
    return destination


def run_order(run):
    """Actual producer-attempt chronology, not the original numeric run ID."""
    stamp = run.get("updated_at") or run.get("run_started_at")
    if not stamp:
        raise ValueError("producer attempt chronology unavailable")
    moment = datetime.fromisoformat(stamp.replace("Z", "+00:00"))
    if moment.tzinfo is None:
        raise ValueError("producer chronology lacks timezone")
    return (moment.timestamp(), int(run["run_attempt"]), int(run["id"]))


def prior_evidence(policy, tier, platform):
    """Read only protected successful runs, and bind manifests to API metadata."""
    if not os.environ.get("GH_TOKEN"):
        return [], ["No authenticated Actions reader; existing evidence unavailable."]
    repo = policy["repository"]
    branches = ["stable"] if tier == "release" else ["nightly", "stable"]
    accepted, notes, runs_found = [], [], []
    for branch in branches:
        try:
            runs = api(f"repos/{repo}/actions/workflows/styio-ci-gate.yml/runs?branch={branch}&per_page={policy['candidate_search_limit']}")["workflow_runs"]
        except (subprocess.CalledProcessError, ValueError) as exc:
            notes.append(f"{branch}: previous evidence unavailable ({type(exc).__name__})")
            continue
        runs_found.extend(runs)
    try:
        runs_found.extend(api(f"repos/{repo}/actions/workflows/styio-ci-gate.yml/runs?event=pull_request&per_page={policy['candidate_search_limit']}")["workflow_runs"])
    except (subprocess.CalledProcessError, ValueError):
        notes.append("Merged-PR source lookup unavailable; protected-branch evidence only.")
    allowed_runs = list({run["id"]: run for run in runs_found
                        if run.get("path") == policy["workflow"] and
                        (run.get("event") == "pull_request" or
                         (run.get("event") in ("push", "workflow_dispatch") and run.get("head_branch") in branches))}.values())
    failures = []
    for run in allowed_runs:
        if (run.get("status") != "completed"
                or run.get("conclusion") in ("success", "skipped", "neutral")):
            continue
        if run.get("event") == "pull_request":
            # Unmerged/fork PR failures have no authority over protected evidence.
            try:
                details = api(f"repos/{repo}/actions/runs/{run['id']}")
                associated = details.get("pull_requests", [])
                if len(associated) != 1:
                    continue
                pr = api(f"repos/{repo}/pulls/{associated[0]['number']}")
                if (pr.get("merged") is not True or pr.get("state") != "closed" or not pr.get("merged_at")
                        or pr["base"]["ref"] not in branches or pr["head"]["sha"] != details["head_sha"]
                        or any(pr[side]["repo"].get("full_name") != repo or pr[side]["repo"].get("fork") is not False
                               for side in ("head", "base"))):
                    continue
            except (ValueError, KeyError, subprocess.CalledProcessError):
                notes.append(f"run {run['id']}: failed PR source eligibility unavailable; not an admitted producer.")
                continue
        try:
            negative = failed_platform_evidence(policy, run, platform)
            if negative:
                failures.append(negative)
        except (OSError, ValueError, KeyError, StopIteration, subprocess.CalledProcessError):
            notes.append(f"run {run['id']}: failed-platform scope is unknown; older scope must be revalidated.")
            failures.append({"order": run_order(run), "run_id": run["id"], "run_attempt": run["run_attempt"],
                             "build_id": None, "phase": "unknown", "tests": {}, "scope_unknown": True})
    for run in allowed_runs:
            if run.get("conclusion") != "success":
                continue
            try:
                artifacts = api(f"repos/{repo}/actions/runs/{run['id']}/artifacts?per_page=100")["artifacts"]
                artifact = max((a for a in artifacts if a["name"] == f"promotion-evidence-{platform}" and not a["expired"]), key=lambda a: a["id"])
                metadata = source_metadata(repo, run, artifact)
                source = validate_source(metadata, repo, policy["workflow"], branches,
                                         ["push", "workflow_dispatch", "pull_request"])
                cache = ROOT / f"build/prior/{source.run_id}/{source.run_attempt}/{source.artifact_id}/{platform}"
                if not (cache / "evidence.json").is_file():
                    cache.mkdir(parents=True, exist_ok=True)
                    download_artifact(repo, artifact, cache)
                envelope = json.loads((cache / "evidence.json").read_text())
                if (envelope.get("platform") != platform or envelope.get("status") != "passed"
                        or envelope.get("tier") != source.branch):
                    raise ValueError("manifest platform/tier does not match its API-validated source")
                if metadata["run"]["event"] == "pull_request":
                    if envelope.get("source_head_sha") != source.head_sha:
                        raise ValueError("manifest PR head differs from successful source run")
                    tested = api(f"repos/{repo}/git/commits/{envelope['candidate_sha']}")
                    if source.head_sha not in {parent["sha"] for parent in tested["parents"]}:
                        raise ValueError("tested PR checkout is not bound to its source head")
                elif envelope.get("candidate_sha") != source.head_sha:
                    raise ValueError("manifest checkout differs from source run")
                envelope["source"] = source.provenance()
                for name, record in envelope.get("tests", {}).items():
                    record["origin"] = record.get("source")
                    record["source"] = source.provenance()
                    record["name"] = name
                envelope["source_attempt_order"] = run_order(metadata["run"])
                accepted.append((envelope, source, artifacts))
            except (OSError, ValueError, KeyError, StopIteration, subprocess.CalledProcessError) as exc:
                notes.append(f"run {run['id']}: evidence not reusable ({type(exc).__name__})")
    accepted.sort(key=lambda item: item[0]["source_attempt_order"])
    for envelope, source, _ in accepted:
        for negative in failures:
            if (negative["order"] <= tuple(envelope["source_attempt_order"])
                    or (not negative.get("scope_unknown") and negative["build_id"] != envelope.get("build_id"))):
                continue
            reason = {"run_id": negative["run_id"], "run_attempt": negative["run_attempt"], "platform": platform}
            if negative["phase"] in {"configure", "build", "unknown"}:
                envelope["invalidated_by"] = reason
                for record in envelope.get("tests", {}).values():
                    record["invalidated_by"] = reason
            for name, failed in negative["tests"].items():
                previous = envelope.get("tests", {}).get(name)
                if (previous and failed.get("status") != "passed"
                        and failed.get("identity") == previous.get("identity")):
                    previous["invalidated_by"] = reason
    return accepted, notes


def failed_platform_evidence(policy, run, platform):
    """A failed producer can invalidate matching scope, never authorize reuse."""
    repo = policy["repository"]
    jobs = api(f"repos/{repo}/actions/runs/{run['id']}/jobs?per_page=100")["jobs"]
    platform_job = next((job for job in jobs if job.get("name") == f"promotion / {platform}"), None)
    if platform_job is None:
        return None  # This platform was not in that producer's actual matrix.
    artifacts = api(f"repos/{repo}/actions/runs/{run['id']}/artifacts?per_page=100")["artifacts"]
    artifact = max((item for item in artifacts if item["name"] == f"promotion-evidence-{platform}" and not item["expired"]),
                   key=lambda item: item["id"])
    metadata = source_metadata(repo, run, artifact)
    actual, branch, check = metadata["run"], metadata["branch"], metadata["check"]
    now = datetime.now(timezone.utc)
    when = lambda value: datetime.fromisoformat(value.replace("Z", "+00:00"))
    source_branch = metadata.get("pull_request", {}).get("base", {}).get("ref", actual["head_branch"])
    if (actual["path"] != policy["workflow"] or actual["event"] not in {"push", "workflow_dispatch", "pull_request"}
            or actual["status"] != "completed" or actual["conclusion"] == "success"
            or any(actual[key].get("full_name") != repo or actual[key].get("fork") is not False
                   for key in ("repository", "head_repository"))
            or branch["name"] != source_branch or branch["protected"] is not True
            or artifact["workflow_run"]["id"] != actual["id"]
            or artifact["workflow_run"].get("head_sha") != actual["head_sha"]
            or when(artifact["expires_at"]) <= now
            or when(artifact["created_at"]) < when(actual["run_started_at"])
            or check["head_sha"] != actual["head_sha"] or check["check_suite"]["id"] != actual["check_suite_id"]):
        raise ValueError("failed-platform source binding is incomplete")
    cache = ROOT / f"build/prior-failed/{actual['id']}/{actual['run_attempt']}/{artifact['id']}/{platform}"
    download_artifact(repo, artifact, cache)
    envelope = json.loads((cache / "evidence.json").read_text())
    if envelope.get("platform") != platform or envelope.get("tier") != branch["name"]:
        raise ValueError("failed-platform manifest binding is incomplete")
    if actual["event"] == "pull_request":
        tested = api(f"repos/{repo}/git/commits/{envelope['candidate_sha']}")
        if (envelope.get("source_head_sha") != actual["head_sha"]
                or actual["head_sha"] not in {parent["sha"] for parent in tested["parents"]}):
            raise ValueError("failed PR checkout is not bound to its source head")
    elif envelope.get("candidate_sha") != actual["head_sha"]:
        raise ValueError("failed-platform manifest checkout mismatch")
    if envelope.get("status") == "passed":
        return None  # Another matrix platform failed; this platform did not.
    if envelope.get("status") != "failed" or not envelope.get("build_id"):
        raise ValueError("failed platform has no bounded build identity")
    return {"order": run_order(actual), "run_id": actual["id"], "run_attempt": actual["run_attempt"],
            "build_id": envelope["build_id"], "phase": envelope.get("phase"), "tests": envelope.get("tests", {})}


def download_named(policy, prior, name, destination):
    envelope, source, artifacts = prior
    artifact = max((a for a in artifacts if a["name"] == name and not a["expired"]),
                   key=lambda item: item["id"], default=None)
    if not artifact:
        raise ValueError(f"required artifact {name} missing or expired")
    metadata = source_metadata(policy["repository"], {"id": source.run_id, "head_branch": source.branch}, artifact)
    verified = validate_source(metadata, policy["repository"], policy["workflow"], [source.branch],
                               ["push", "workflow_dispatch", "pull_request"])
    if (verified.run_id, verified.run_attempt, verified.head_sha, verified.check_id) != (
            source.run_id, source.run_attempt, source.head_sha, source.check_id):
        raise ValueError("auxiliary artifact source differs from admitted evidence attempt")
    destination = destination / str(source.run_attempt) / str(artifact["id"])
    return download_artifact(policy["repository"], artifact, destination)


def normalized_checkout_times():
    # CMake cache reuse is admitted only for identical relevant Git inputs.
    # Deterministic checkout mtimes avoid rebuilding identical sources solely
    # because a promotion made a new checkout. Changed identities never restore
    # the old build tree. This reads Git paths, not file contents/hashes.
    for path in command(["git", "ls-files", "-z"], capture=True).stdout.split("\0"):
        if path and (ROOT / path).is_file() and not (ROOT / path).is_symlink():
            os.utime(ROOT / path, (1, 1))


def identities(tests, build_id, fingerprint):
    inputs = git_inputs(ROOT, TEST_PATHS)
    policy = git_inputs(ROOT, POLICY_PATHS)
    docs = git_inputs(ROOT, ["docs", "workflows", "*.md", "library"])
    result = {}
    for test in tests:
        labels = test.get("labels", [])
        relevant = dict(inputs)
        if set(labels) & {"docs", "workflow", "syntax_convergence"}:
            relevant.update(docs)
        result[test["name"]] = test_identity(test, build_id, relevant, fingerprint, policy)
    return result


def collect_junit(path, requested):
    results = {name: "unknown" for name in requested}
    seen = set()
    if path.is_file():
        for test in ET.parse(path).iter("testcase"):
            name = test.attrib.get("name")
            if name in results:
                if name in seen:
                    results[name] = "unknown"
                    continue
                seen.add(name)
                results[name] = ("failed" if test.find("failure") is not None or test.find("error") is not None
                                 else "skipped" if test.find("skipped") is not None else "passed")
    return results


def run_tests(tests, plan, specs):
    records = plan.get("progress", dict(plan["reuse"]))
    ctest_names = [name for name in plan["run"] if not name.startswith("auto-example:")]
    if ctest_names:
        selection = OUT / "ctest-selection.txt"
        selection.write_text("\n".join(ctest_names) + "\n")
        junit = OUT / "ctest-results.xml"
        junit.unlink(missing_ok=True)
        process = command(["ctest", "--test-dir", BUILD, "--tests-from-file", selection,
                           "--output-on-failure", "--no-tests=error", "--output-junit", junit], check=False)
        results = collect_junit(junit, ctest_names)
        if process.returncode and all(status == "passed" for status in results.values()):
            results = {name: "unknown" for name in ctest_names}
        for name, status in results.items():
            records[name] = {**records.get(name, {}), "status": status, "execution": "current"}
    for spec in specs:
        name = spec["id"]
        if name not in plan["run"]:
            continue
        try:
            stdin = (ROOT / spec["stdin_file"]).read_text() if spec.get("stdin_file") else ""
            result = command(spec["command"], capture=True, check=False, input=stdin, timeout=spec["timeout"])
            expected = (ROOT / spec["expected_output_file"]).read_text()
            status = "passed" if result.returncode == 0 and result.stdout == expected else "failed"
        except (OSError, subprocess.TimeoutExpired):
            status = "failed"
        records[name] = {**records.get(name, {}), "status": status, "execution": "current", "scope": "stdout golden"}
    return records


def package_payload(policy, fingerprint, build_id):
    destination = ROOT / "build/install-promotion"
    command(["cmake", "--install", BUILD, "--prefix", destination, "--component", "Runtime"])
    for name in ("LICENSE", "NOTICE", "DEPENDENCY-USAGE.md", "LICENSE-POLICY.md"):
        if (ROOT / name).is_file():
            shutil.copy2(ROOT / name, destination / name)
    suffix = ".exe" if fingerprint["platform"] == "windows" else ""
    products = ["styio" + suffix, "styio-nano" + suffix, "styio_lspd" + suffix]
    if any(not (destination / "bin" / name).is_file() for name in products):
        raise ValueError("stable Runtime component lacks a required product binary")
    info = json.loads(command([destination / "bin" / products[0], "--machine-info=json"], capture=True).stdout)
    if info.get("build_id") != build_id or info.get("channel") != "release":
        raise ValueError("stable installed product identity does not match its evidence")
    write_json(destination / "build-identity.json", {"build_id": build_id, "public_channel": policy["public_channel"],
        "compiler_version": info["compiler_version"], "products": products,
        "runtime_requirements": "Native LLVM 18.1 and its platform runtime dependencies; toolchain bundle is not included.",
        "fingerprint": fingerprint, "published": False})
    payload = OUT / "payload.tar.gz"
    pack(destination, payload)
    return checksum(payload)


def check_payload(payload, build_id, platform):
    # This is new installation/distribution scope, not rerunning stable tests.
    with tempfile.TemporaryDirectory(prefix="styio-install-") as temp:
        installed = Path(temp)
        extract_archive(payload, installed)
        identity = json.loads((installed / "build-identity.json").read_text())
        if identity["build_id"] != build_id or identity["public_channel"] != "release":
            raise ValueError("sealed payload identity/public channel mismatch")
        if any(not (installed / "bin" / name).is_file() for name in identity.get("products", [])):
            raise ValueError("sealed product binary missing")
        if not (installed / "LICENSE").is_file():
            raise ValueError("distribution license missing")
        executable = installed / "bin" / ("styio.exe" if platform == "windows" else "styio")
        info = command([executable, "--machine-info=json"], capture=True)
        parsed = json.loads(info.stdout)
        # Build identity is deliberately stable across internal promotion stages.
        if (parsed.get("channel") != "release" or parsed.get("build_id") != build_id
                or parsed.get("compiler_version") != identity.get("compiler_version")):
            raise ValueError("installed compiler does not match sealed public channel/build identity")
        sample = installed / "distribution-smoke.styio"
        sample.write_text('"distribution-ok" -> @stdout\n')
        result = command([executable, "--file", sample], capture=True)
        if result.stdout.strip() != "distribution-ok":
            raise ValueError("installed payload smoke failed")
    return {"status": "passed", "checks": ["payload checksum", "safe extraction", "license", "installed identity", "installed execution", "temporary uninstall"]}


def runtime_profile(fingerprint):
    """Runtime/toolchain compatibility, excluding unused release build tools."""
    keys = ("platform", "architecture", "os_release", "os_version", "runner_image",
            "llvm_root", "llvm_dir", "icu", "sdk", "build_environment", "dependency_sha")
    result = {key: fingerprint.get(key) for key in keys}
    result["tools"] = {key: fingerprint.get("tools", {}).get(key) for key in ("llvm", "cc", "cxx")}
    return result


def run_tier(args, policy):
    OUT.mkdir(parents=True, exist_ok=True)
    candidate = command(["git", "rev-parse", "HEAD"], capture=True).stdout.strip()
    previous, notes = prior_evidence(policy, args.tier, args.platform)
    selected_source = os.environ.get("STYIO_STABLE_SOURCE_RUN")
    if selected_source:
        previous = [item for item in previous if str(item[1].run_id) == selected_source]
    report = {"schema": 1, "candidate_sha": candidate, "tier": args.tier, "platform": args.platform,
              "public_channel": policy["public_channel"], "status": "failed", "notes": notes,
              "source_head_sha": os.environ.get("GITHUB_PR_HEAD_SHA") or candidate,
              "source_base_sha": os.environ.get("GITHUB_PR_BASE_SHA", ""),
              "dirty": bool(command(["git", "status", "--porcelain"], capture=True).stdout),
              "published": False, "observed_at": datetime.now(timezone.utc).isoformat()}
    try:
        if args.tier == "release":
            actual_platform, actual_arch = detect_platform(args.platform)
            # Never rebuild at release. Recompute current source/policy identities
            # against the sealed stable environment and require its complete scope.
            for prior in reversed(previous):
                envelope, source, _ = prior
                if envelope.get("distribution_inputs") != git_inputs(ROOT, DISTRIBUTION_PATHS):
                    continue
                if (envelope["fingerprint"].get("platform") != actual_platform
                        or envelope["fingerprint"].get("architecture") != actual_arch):
                    continue
                current_environment = probe_environment(ROOT, BUILD, policy["public_channel"],
                    hashlib.sha256(json.dumps(git_inputs(ROOT, ["cmake/StyioGoogleTest.cmake", "cmake/StyioTreeSitter.cmake"]), sort_keys=True).encode()).hexdigest(),
                    expected_platform=args.platform)["fingerprint_data"]
                if runtime_profile(current_environment) != runtime_profile(envelope["fingerprint"]):
                    report["notes"].append("Stable runtime/toolchain environment changed; missing validation belongs in stable, not a release rebuild.")
                    continue
                bid = build_identity(git_inputs(ROOT, BUILD_PATHS), envelope["fingerprint"])
                wanted = identities(envelope["catalog"], bid, envelope["fingerprint"])
                if bid != envelope.get("build_id") or wanted != envelope.get("identities"):
                    continue
                payload_dir = download_named(policy, prior, f"promotion-payload-{args.platform}", ROOT / f"build/payload/{source.run_id}")
                payload = payload_dir / "payload.tar.gz"
                payload_id = checksum(payload)
                admission = release_admission(candidate, bid, wanted, envelope, source, payload_id, platform=args.platform)
                current_context = {key: report[key] for key in ("source_head_sha", "source_base_sha", "dirty", "observed_at")}
                report.update(envelope, candidate_sha=candidate, tier="release", source=source.provenance(), admission=admission, **current_context)
                report["phase"] = "distribution"
                report["distribution"] = check_payload(payload, bid, args.platform)
                shutil.copy2(payload, OUT / "payload.tar.gz")
                report["status"] = "passed"
                break
            else:
                raise ValueError("no compatible retained stable payload and complete evidence; return candidate to stable (release never rebuilds)")
        else:
            environment = probe_environment(ROOT, BUILD, policy["public_channel"], hashlib.sha256(json.dumps(git_inputs(ROOT, ["cmake/StyioGoogleTest.cmake", "cmake/StyioTreeSitter.cmake"]), sort_keys=True).encode()).hexdigest(), expected_platform=args.platform)
            fingerprint = environment["fingerprint_data"]
            fingerprint["platform"] = args.platform
            bid = build_identity(git_inputs(ROOT, BUILD_PATHS), fingerprint)
            report.update(build_id=bid, fingerprint=fingerprint)
            marker = BUILD / ".promotion-build-id"
            if BUILD.exists() and (not marker.is_file() or marker.read_text().strip() != bid):
                shutil.rmtree(BUILD)  # Only this generated cache, never source/worktrees.
            compatible = [p for p in previous if p[0].get("build_id") == bid and not p[0].get("invalidated_by")]
            if compatible:
                cached = compatible[-1]
                try:
                    cache_dir = download_named(policy, cached, f"promotion-build-{args.platform}", ROOT / f"build/cache/{cached[1].run_id}")
                    extract_archive(cache_dir / "build.tar.gz", BUILD)
                    if not marker.is_file() or marker.read_text().strip() != bid:
                        raise ValueError("restored cache does not carry the compatible build identity")
                    report["build_reused_source"] = cached[1].provenance()
                except (OSError, ValueError, subprocess.CalledProcessError, StopIteration, tarfile.TarError, zipfile.BadZipFile) as exc:
                    report["notes"].append(f"build cache unavailable; missing build work will run ({type(exc).__name__})")
                    if BUILD.exists():
                        shutil.rmtree(BUILD)
            normalized_checkout_times()
            report["phase"] = "configure"
            command(["cmake", *environment["configure_args"], f"-DSTYIO_BUILD_ID={bid}"])
            marker.write_text(bid + "\n")
            targets = policy["nightly_targets"] if args.tier == "nightly" else ["all"]
            report["phase"] = "build"
            command(["cmake", "--build", BUILD, "--parallel", "4", "--target", *targets])
            catalog = json.loads(command(["ctest", "--test-dir", BUILD, "--show-only=json-v1"], capture=True).stdout)
            selected = select_tests(catalog, args.tier, policy)
            if not selected:
                raise ValueError("empty required test scope is not a pass")
            if args.tier == "nightly":
                present = {test["name"] for test in selected}
                if not set(policy["nightly_names"]).issubset(present):
                    raise ValueError("configured nightly smoke names are missing")
                if not any("security" in test["labels"] for test in selected):
                    raise ValueError("nightly security selector matched no tests")
            examples = discover_examples(ROOT, catalog, compiler=str(BUILD / "bin" / ("styio.exe" if args.platform == "windows" else "styio")))
            specs = examples["specifications"] if args.tier == "stable" else []
            selected += [{"name": spec["id"], "command": spec["command"], "labels": ["example"],
                          "properties": {"stdin_file": spec.get("stdin_file"), "expected_output_file": spec["expected_output_file"], "timeout": spec["timeout"]}} for spec in specs]
            wanted = identities(selected, bid, fingerprint)
            evidence = [record for prior, _, _ in previous for record in prior.get("tests", {}).values()]
            plan = plan_tests(selected, wanted, evidence, [source for _, source, _ in previous])
            report["phase"] = "tests"
            # Persist the intended identities before executing: malformed results
            # or a runner exception must invalidate the unrecorded scope.
            report.update(catalog=selected, identities=wanted, examples=examples,
                          tests={**plan["reuse"], **{name: {"name": name, "identity": wanted[name], "status": "unknown"}
                                                    for name in plan["run"]}},
                          plan={"run": plan["run"], "reuse": sorted(plan["reuse"])})
            plan["progress"] = report["tests"]  # Completed sub-results survive a later runner exception.
            records = run_tests(selected, plan, specs)
            for name, record in records.items():
                record.update(name=name, identity=wanted[name])
            report.update(catalog=selected, identities=wanted, tests=records, examples=examples,
                          plan={"run": plan["run"], "reuse": sorted(plan["reuse"])})
            if any(records.get(name, {}).get("status") != "passed" for name in wanted):
                raise ValueError("required current-candidate test scope has failed/skipped/missing evidence")
            if args.tier == "stable":
                report["phase"] = "package"
                report["distribution_inputs"] = git_inputs(ROOT, DISTRIBUTION_PATHS)
                report["payload_id"] = package_payload(policy, fingerprint, bid)
            report["phase"] = "archive"
            pack(BUILD, OUT / "build.tar.gz")
            report["status"] = "passed"
    except Exception as exc:
        report["status"] = "failed"
        report["error"] = f"{type(exc).__name__}: {exc}"
        print(report["error"], file=sys.stderr)
    write_json(OUT / "evidence.json", report)
    summary = [f"## {args.tier} / {args.platform}", f"Candidate: {candidate}", f"Status: {report['status']}",
               f"Run checks: {len(report.get('plan', {}).get('run', []))}; reused: {len(report.get('plan', {}).get('reuse', []))}",
               "Unknown example applicability is advisory; required executed checks still must pass."]
    summary += report.get("notes", [])
    summary += ["### Actual check results"]
    for name, record in sorted(report.get("tests", {}).items()):
        origin = record.get("source") or {}
        reference = origin.get("run_url", "")
        summary.append(f"- {name}: {record.get('status', 'unknown')} ({record.get('execution', 'reused')}) {reference}")
    summary += ["### Example discovery (advisory)"]
    for entry in report.get("examples", {}).get("entries", []):
        summary.append(f"- {entry['path']}: {entry['status']}; {'; '.join(entry.get('reasons', []))}")
    summary += ["### Scheduled-only scope"]
    summary += [f"- {label}: {reason}" for label, reason in policy.get("scheduled_only_labels", {}).items()]
    if report.get("distribution"):
        summary.append(f"Distribution: {json.dumps(report['distribution'], sort_keys=True)}")
    if report.get("error"):
        summary.append(report["error"])
    (OUT / "report.md").write_text("\n\n".join(summary) + "\n")
    if os.environ.get("GITHUB_STEP_SUMMARY"):
        with open(os.environ["GITHUB_STEP_SUMMARY"], "a") as stream:
            stream.write((OUT / "report.md").read_text())
    return 0 if report["status"] == "passed" else 1


def route_event(policy):
    event = json.loads(Path(os.environ["GITHUB_EVENT_PATH"]).read_text())
    kind = os.environ["GITHUB_EVENT_NAME"]
    repo = os.environ["GITHUB_REPOSITORY"]
    if repo != policy["repository"]:
        raise ValueError("official promotion workflow must run in the configured repository")
    if kind == "pull_request":
        pr = event["pull_request"]
        tier = pr["base"]["ref"]
        predecessor = {"stable": "nightly", "release": "stable"}.get(tier)
        if predecessor and (pr["head"]["ref"] != predecessor or pr["head"]["repo"]["full_name"] != repo):
            raise ValueError(f"{tier} accepts promotion from {predecessor} in this repository only")
    else:
        tier = os.environ["GITHUB_REF_NAME"]
        if tier in {"stable", "release"} and kind == "push":
            predecessor = {"stable": "nightly", "release": "stable"}[tier]
            sha = os.environ["GITHUB_SHA"]
            pulls = api(f"repos/{repo}/commits/{sha}/pulls")
            if not any(pr.get("merged_at") and pr.get("merge_commit_sha") == sha
                       and pr["base"]["ref"] == tier and pr["head"]["ref"] == predecessor
                       and pr["head"]["repo"]["full_name"] == repo for pr in pulls):
                raise ValueError("promotion push lacks its merged predecessor-branch PR")
    if tier not in {"nightly", "stable", "release"}:
        raise ValueError("unsupported promotion target")
    platforms = list(policy["nightly_platforms"] if tier == "nightly" else policy["acceptance_platforms"])
    # Ordinary nightly remains Linux-fast. Native adapters/toolchain recipes
    # require only the same basic scope on affected platforms, never stable's
    # full suite just because a PR targets nightly.
    if tier == "nightly":
        base = event.get("pull_request", {}).get("base", {}).get("sha") or event.get("before")
        if base and set(base) != {"0"}:
            changed = command(["git", "diff", "--name-only", base, "HEAD"], capture=True).stdout.splitlines()
            if any(path in {"CMakeLists.txt", "src/CMakeLists.txt", "tests/CMakeLists.txt",
                            "src/StyioUtil/ProcessPipe.hpp", "scripts/promotion_platform.py",
                            ".github/workflows/styio-ci-gate.yml"}
                   or path.startswith(("src/StyioNative/", "cmake/", "src/cmake/")) for path in changed):
                platforms = list(policy["acceptance_platforms"])
    stable_run = ""
    if tier == "release":
        candidates, _ = prior_evidence(policy, "release", "linux")
        required_artifacts = {f"promotion-{kind}-{platform}" for platform in policy["acceptance_platforms"] for kind in ("evidence", "payload")}
        for envelope, source, artifacts in reversed(candidates):
            available = {artifact["name"] for artifact in artifacts if not artifact["expired"]}
            bid = build_identity(git_inputs(ROOT, BUILD_PATHS), envelope["fingerprint"])
            if (required_artifacts.issubset(available) and bid == envelope.get("build_id")
                    and envelope.get("distribution_inputs") == git_inputs(ROOT, DISTRIBUTION_PATHS)
                    and identities(envelope["catalog"], bid, envelope["fingerprint"]) == envelope.get("identities")):
                stable_run = str(source.run_id)
                break
        if not stable_run:
            raise ValueError("release has no compatible retained three-platform stable producer")
    runners = {"linux": "ubuntu-24.04", "windows": "windows-2022", "macos": "macos-15"}
    output = {"tier": tier, "stable_run": stable_run, "required_platforms": json.dumps(platforms), "matrix": json.dumps({"include": [{"platform": p, "runner": runners[p]} for p in platforms]}),
              "pafio_sha": policy["dependencies"]["pafio"]["sha"], "vityo_sha": policy["dependencies"]["vityo"]["sha"]}
    with open(os.environ["GITHUB_OUTPUT"], "a") as stream:
        for key, value in output.items():
            stream.write(f"{key}={value}\n")
    return 0


def aggregate(tier, directory, candidate, policy):
    required = set(json.loads(os.environ.get("STYIO_REQUIRED_PLATFORMS", "null")) or
                   (policy["nightly_platforms"] if tier == "nightly" else policy["acceptance_platforms"]))
    if tier != "nightly" and required != set(policy["acceptance_platforms"]):
        raise ValueError("stable/release cannot narrow the official platform set")
    envelopes = [json.loads(path.read_text()) for path in directory.glob("**/evidence.json")]
    by_platform = {item["platform"]: item for item in envelopes}
    if len(envelopes) != len(by_platform) or set(by_platform) != required:
        raise ValueError("current candidate lacks exactly the required platform evidence")
    for item in envelopes:
        if item.get("status") != "passed" or item.get("candidate_sha") != candidate or item.get("tier") != tier:
            raise ValueError("failed/missing/wrong-candidate platform cannot satisfy admission")
    if tier == "release":
        sources = {tuple(item["source"][key] for key in ("run_id", "run_attempt", "head_sha")) for item in envelopes}
        if len(sources) != 1:
            raise ValueError("release must consume all three payloads from the same accepted stable run")
        if any(item.get("distribution", {}).get("status") != "passed" for item in envelopes):
            raise ValueError("every platform needs its distribution-only checks")
    report = {"candidate_sha": candidate, "tier": tier, "public_channel": "release", "published": False,
              "status": "passed", "platforms": by_platform}
    write_json(directory / "admission.json", report)
    summary = [f"## Final {tier} admission", f"Candidate: {candidate}", "Status: passed; publication: not performed"]
    for platform, item in sorted(by_platform.items()):
        summary.append(f"### {platform}")
        summary.append(f"Executed checks: {len(item.get('plan', {}).get('run', []))}; reused checks: {len(item.get('plan', {}).get('reuse', []))}")
        summary += [f"- {name}: {record.get('status', 'unknown')} ({record.get('execution', 'reused')})" for name, record in sorted(item.get('tests', {}).items())]
        summary.append(f"Example discovery: {json.dumps(item.get('examples', {}).get('counts', {}), sort_keys=True)} (unknown applicability is advisory)")
        if item.get("distribution"):
            summary.append(f"Distribution: {json.dumps(item['distribution'], sort_keys=True)}")
    text = "\n\n".join(summary) + "\n"
    (directory / "admission.md").write_text(text, encoding="utf-8")
    if os.environ.get("GITHUB_STEP_SUMMARY"):
        with open(os.environ["GITHUB_STEP_SUMMARY"], "a", encoding="utf-8") as stream:
            stream.write(text)
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tier", choices=("nightly", "stable", "release"))
    parser.add_argument("--platform", choices=("linux", "windows", "macos"))
    parser.add_argument("--route", action="store_true")
    parser.add_argument("--aggregate", type=Path)
    args = parser.parse_args()
    policy = json.loads(POLICY.read_text())
    if args.route:
        return route_event(policy)
    if args.aggregate:
        try:
            return aggregate(args.tier, args.aggregate, os.environ["GITHUB_SHA"], policy)
        except Exception as exc:
            write_json(args.aggregate / "admission.json", {"candidate_sha": os.environ.get("GITHUB_SHA"),
                "tier": args.tier, "status": "failed", "error": f"{type(exc).__name__}: {exc}", "published": False})
            text = f"## Final {args.tier} admission\n\nCandidate: {os.environ.get('GITHUB_SHA')}\n\nStatus: failed\n\n{type(exc).__name__}: {exc}\n\nPer-platform evidence contains executed, reused, failed and unknown scope; no tests were rerun for this report.\n"
            (args.aggregate / "admission.md").write_text(text, encoding="utf-8")
            if os.environ.get("GITHUB_STEP_SUMMARY"):
                with open(os.environ["GITHUB_STEP_SUMMARY"], "a", encoding="utf-8") as stream:
                    stream.write(text)
            print(f"Admission denied: {exc}", file=sys.stderr)
            return 1
    if not args.tier or not args.platform:
        parser.error("--tier and --platform required for execution")
    return run_tier(args, policy)


if __name__ == "__main__":
    sys.exit(main())
