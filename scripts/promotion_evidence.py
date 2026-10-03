#!/usr/bin/env python3
"""Content-based promotion evidence; GitHub I/O and execution belong to the caller.

Git input maps contain actual ``git ls-tree`` object IDs, never hashes of the
worktree or the repository's root tree. Identities deliberately have no commit
SHA. The caller supplies a canonical, complete toolchain/environment/dependency
fingerprint (including dependencySHA), and must run from the recorded checkout.
"""
from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime, timezone
import fnmatch
import hashlib
import json
from pathlib import Path
import subprocess
from typing import Any, Iterable, Mapping


SCHEMA = 1
PLATFORMS = frozenset(("linux", "windows", "macos"))


def _platform(fingerprint: Mapping) -> str:
    platform = fingerprint.get("platform")
    if platform not in PLATFORMS:
        raise ValueError("fingerprint requires a canonical linux/windows/macos platform")
    return platform


def _digest(value: Any) -> str:
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":"),
                                     ensure_ascii=True, allow_nan=False).encode()).hexdigest()


def git_inputs(root: str | Path, paths: Iterable[str], ref: str = "HEAD") -> dict:
    """Return selected tracked path -> {mode, type, oid} at *ref*.

    Selectors are repository-relative files, directories, or shell-style globs.
    Missing selectors yield no objects (so additions/deletions change identity).
    Directory selection includes descendants. Listing the tree does not make
    unrelated tracked files inputs. Uncommitted/untracked files are not evidence.
    """
    selectors = sorted(set(paths))
    if any(not p or p.startswith("/") or ".." in p.split("/") or p in (".", "*")
           for p in selectors):
        raise ValueError("input selectors must be scoped repository-relative paths")
    output = subprocess.check_output(
        ["git", "ls-tree", "-r", "-z", "--full-tree", ref, "--"], cwd=root)
    result = {}
    for entry in output.split(b"\0"):
        if not entry:
            continue
        header, raw_path = entry.split(b"\t", 1)
        mode, kind, oid = header.decode("ascii").split()
        path = raw_path.decode("utf-8", "surrogateescape")
        if any(path == p.rstrip("/") or path.startswith(p.rstrip("/") + "/")
               or fnmatch.fnmatchcase(path, p) for p in selectors):
            result[path] = {"mode": mode, "type": kind, "oid": oid}
    return dict(sorted(result.items()))


def build_identity(inputs: Mapping, fingerprint: Mapping) -> str:
    """Identify relevant build Git objects plus canonical toolchain/env/deps.

    ``fingerprint`` must contain the actual compiler/version, configuration,
    environment and resolved dependencySHA; do not include promotion run/commit
    identifiers. The caller decides the relevant input paths conservatively.
    """
    _platform(fingerprint)
    if not inputs or not fingerprint:
        raise ValueError("build inputs and toolchain/environment/dependency fingerprint required")
    return _digest({"schema": SCHEMA, "kind": "build", "inputs": inputs,
                    "fingerprint": fingerprint})


def _test(test: Mapping) -> dict:
    name, command = test.get("name"), test.get("command")
    if not isinstance(name, str) or not name or not isinstance(command, (list, tuple)) or not command:
        raise ValueError("each configured CTest test needs a name and actual command")
    if name.endswith("_NOT_BUILT"):
        raise ValueError("required CTest executable was not built: " + name)
    if not all(isinstance(arg, str) for arg in command):
        raise ValueError("CTest command arguments must be strings")
    properties = test.get("properties", [])
    if isinstance(properties, list):
        properties = {p["name"]: p["value"] for p in properties}
    if not isinstance(properties, Mapping):
        raise ValueError("CTest properties must be a mapping or CTest JSON property list")
    properties = dict(properties)
    labels = test.get("labels", properties.pop("LABELS", []))
    if isinstance(labels, str):
        labels = labels.split(";")
    if not isinstance(labels, (list, tuple)) or not all(isinstance(x, str) for x in labels):
        raise ValueError("CTest labels must be strings")
    return {"name": name, "command": list(command), "labels": sorted(set(labels)),
            "properties": properties}


def test_identity(test: Mapping, build_id: str, inputs: Mapping,
                  fingerprint: Mapping, policy_inputs: Mapping) -> str:
    """Identify one actual CTest name+command, independent of overlapping labels.

    ``inputs`` is a git_inputs map for test fixtures/runners/configuration;
    ``policy_inputs`` records current workflow/selector policy Git objects.
    All CTest properties are included, since working directory, environment,
    timeout, fixtures and similar properties can change test semantics. Caller
    may normalize relocatable checkout/build paths consistently before use.
    """
    _platform(fingerprint)
    if not build_id or not fingerprint or not policy_inputs:
        raise ValueError("test identity requires build, fingerprint and fresh policy inputs")
    return _digest({"schema": SCHEMA, "kind": "test", "build_id": build_id,
                    "test": _test(test), "inputs": inputs,
                    "fingerprint": fingerprint, "policy_inputs": policy_inputs})


def select_tests(catalog: Iterable[Mapping] | Mapping, tier: str, policy: Mapping) -> list[dict]:
    """Select unique configured CTest tests; labels are selectors, not scopes.

    Accepts a list of descriptors or CTest ``--show-only=json-v1`` JSON.
    Policy: ``nightly_labels`` and/or ``nightly_names`` arrays;
    ``scheduled_only_labels`` is label -> nonempty reason. Nightly takes the
    union of matching fast/security selectors. Stable/release require every
    configured test except the explicitly reasoned scheduled-only labels.
    Disabled/skipped tests remain desired; they can never establish a pass.
    Unselected nightly placeholders may lack commands; selected entries may
    not. Conflicting selected CTest names are rejected instead of merged.
    """
    if tier not in ("nightly", "stable", "release"):
        raise ValueError("unknown promotion tier")
    excluded = policy.get("scheduled_only_labels", {})
    if not isinstance(excluded, Mapping) or any(not isinstance(reason, str) or not reason.strip()
                                              for reason in excluded.values()):
        raise ValueError("scheduled-only exclusions require explicit reasons")
    labels, names = set(policy.get("nightly_labels", [])), set(policy.get("nightly_names", []))
    if tier == "nightly" and not labels and not names:
        raise ValueError("nightly needs explicit fast/security selectors")
    tests = catalog.get("tests", []) if isinstance(catalog, Mapping) else catalog
    selected = {}
    seen = {}
    for raw in tests:
        # CTest includes command-less *_NOT_BUILT placeholders for targets that
        # fast nightly intentionally does not build. Apply selection first;
        # every selected entry still has to describe a real executable test.
        properties = raw.get("properties", [])
        if isinstance(properties, list):
            properties = {item["name"]: item["value"] for item in properties}
        raw_labels = raw.get("labels", properties.get("LABELS", []))
        if isinstance(raw_labels, str):
            raw_labels = raw_labels.split(";")
        if set(raw_labels) & set(excluded):
            continue
        if tier == "nightly" and raw.get("name") not in names and not labels.intersection(raw_labels):
            continue
        test = _test(raw)
        name = test["name"]
        if name in seen and seen[name] != test:
            raise ValueError("conflicting CTest test name: " + name)
        seen[name] = test
        selected[name] = test
    return [selected[name] for name in sorted(selected)]


def _time(value: str | datetime) -> datetime:
    result = value if isinstance(value, datetime) else datetime.fromisoformat(value.replace("Z", "+00:00"))
    if result.tzinfo is None:
        raise ValueError("timestamps must include a timezone")
    return result.astimezone(timezone.utc)


@dataclass(frozen=True)
class ValidatedSource:
    """Only validate_source creates this handle from independently queried APIs.

    Do not deserialize a manifest into this class. Embedded provenance merely
    names this externally verified source; it cannot authorize its own reuse.
    """
    run_id: int
    artifact_id: int
    check_id: int
    run_attempt: int
    head_sha: str
    branch: str
    repository: str
    workflow_path: str
    expires_at: datetime
    run_url: str
    artifact_url: str
    check_url: str

    def provenance(self) -> dict:
        return {key: getattr(self, key) for key in
                ("run_id", "artifact_id", "check_id", "run_attempt", "head_sha", "branch",
                 "repository", "workflow_path", "run_url", "artifact_url", "check_url")}


def validate_source(metadata: Mapping, expected_repo: str, expected_workflow: str,
                    allowed_branches: Iterable[str], allowed_events: Iterable[str],
                    now: str | datetime | None = None) -> ValidatedSource:
    """Validate actual GitHub API metadata, not claims inside an artifact.

    ``metadata`` has independently fetched ``run`` (Actions workflow run),
    ``artifact`` (Actions artifact), ``branch`` (repository branch), and ``check``
    (Checks check-run). The check must belong to the workflow run's check suite.
    Its identity will appear in current-candidate admission provenance.
    Artifact ``created_at`` must be at or after run ``run_started_at``, binding
    retained output to the successful current attempt instead of an older one.

    An explicitly allowed ``pull_request`` event additionally requires an
    independently fetched ``pull_request`` response: merged=true, state=closed,
    merged_at present, head/base repo.full_name equal expected_repo and fork=false,
    and current head.sha equal run.head_sha. ``branch`` then describes the PR's
    protected base.ref; the returned branch is that target. Unmerged PRs and
    forks are never sources. The caller separately verifies the tested merge
    checkout's parent binding and downloaded artifact/payload contents.
    """
    now = _time(now) if now is not None else datetime.now(timezone.utc)
    try:
        run, artifact, branch, check = (metadata[k] for k in ("run", "artifact", "branch", "check"))
        if any(run[k].get("full_name") != expected_repo or run[k].get("fork") is not False
               for k in ("repository", "head_repository")):
            raise ValueError("source repository must be the expected non-fork repository")
        if run["path"] != expected_workflow:
            raise ValueError("source workflow path mismatch")
        if run["status"] != "completed" or run["conclusion"] != "success":
            raise ValueError("source workflow did not finish successfully")
        if run["event"] not in set(allowed_events):
            raise ValueError("source event is not explicitly allowed")
        source_branch = run["head_branch"]
        if run["event"] == "pull_request":
            pull_request = metadata["pull_request"]
            if (pull_request["merged"] is not True or pull_request["state"] != "closed"
                    or not pull_request["merged_at"] or _time(pull_request["merged_at"]) > now):
                raise ValueError("pull-request evidence requires an already merged PR")
            if any(pull_request[side]["repo"].get("full_name") != expected_repo
                   or pull_request[side]["repo"].get("fork") is not False
                   for side in ("head", "base")):
                raise ValueError("merged PR head and base must belong to the expected non-fork repository")
            if pull_request["head"]["sha"] != run["head_sha"]:
                raise ValueError("merged PR head does not match the tested source run")
            source_branch = pull_request["base"]["ref"]
        if (source_branch not in set(allowed_branches) or branch["name"] != source_branch
                or branch["protected"] is not True):
            raise ValueError("source must be an allowed protected branch")
        if _time(artifact["created_at"]) < _time(run["run_started_at"]):
            raise ValueError("source artifact predates the current workflow attempt")
        expires = _time(artifact["expires_at"])
        if artifact["expired"] is not False or expires <= now:
            raise ValueError("source artifact is expired")
        if artifact["workflow_run"]["id"] != run["id"]:
            raise ValueError("source artifact belongs to a different workflow run")
        if artifact["workflow_run"].get("head_sha", run["head_sha"]) != run["head_sha"]:
            raise ValueError("source artifact commit mismatch")
        if (check["status"] != "completed" or check["conclusion"] != "success"
                or check["head_sha"] != run["head_sha"]
                or check["check_suite"]["id"] != run["check_suite_id"]):
            raise ValueError("source check must be successful and bound to the workflow run")
        for number in (run["id"], artifact["id"], check["id"], run["run_attempt"]):
            if not isinstance(number, int) or isinstance(number, bool) or number <= 0:
                raise ValueError("source IDs and attempt must be positive integers")
        if not run["head_sha"]:
            raise ValueError("source commit is missing")
        return ValidatedSource(run["id"], artifact["id"], check["id"], run["run_attempt"],
                               run["head_sha"], source_branch, expected_repo, expected_workflow,
                               expires, run["html_url"], artifact["url"], check["html_url"])
    except (KeyError, TypeError, AttributeError) as exc:
        raise ValueError("incomplete or malformed external source metadata") from exc


def _bound_source(record: Mapping, sources: Iterable[ValidatedSource], now: datetime) -> ValidatedSource | None:
    claimed = record.get("source", {})
    if not isinstance(claimed, Mapping):
        return None
    for source in sources:
        if not isinstance(source, ValidatedSource) or source.expires_at <= now:
            continue
        if all(claimed.get(key) == getattr(source, key) for key in
               ("run_id", "artifact_id", "check_id", "run_attempt")):
            # If additional provenance is present it must not contradict the API.
            if all(key not in claimed or claimed[key] == value
                   for key, value in source.provenance().items()):
                return source
    return None


def plan_tests(tests: Iterable[Mapping], identities: Mapping[str, str],
               evidence: Iterable[Mapping] | Mapping = (),
               sources: Iterable[ValidatedSource] = (),
               now: str | datetime | None = None) -> dict:
    """Return {run: [actual names], reuse: {name: record}} for set difference.

    Evidence is name -> {name?, identity, status, source}, or records in oldest
    to newest order. Only the last record per name may pass; a later failure,
    skip, cancellation or unknown result cannot be hidden by an older success.
    ``source`` names run_id/artifact_id/check_id/run_attempt. Corresponding
    validated handles MUST be supplied separately; manifest trust flags have no
    effect. Callers must bind every record to the artifact they actually read.
    """
    now = _time(now) if now is not None else datetime.now(timezone.utc)
    sources = tuple(sources)
    desired = sorted({_test(test)["name"] for test in tests})
    if any(not isinstance(identities.get(name), str) or not identities[name] for name in desired):
        raise ValueError("every desired test needs its current input identity")
    if isinstance(evidence, Mapping):
        latest = {name: dict(record, name=name) for name, record in evidence.items()}
    else:
        latest = {record.get("name"): record for record in evidence}
    reuse = {}
    for name in desired:
        record = latest.get(name, {})
        source = _bound_source(record, sources, now)
        if (record.get("status") == "passed" and record.get("identity") == identities[name]
                and not record.get("invalidated_by") and source):
            reuse[name] = dict(record, source=source.provenance())
    return {"run": [name for name in desired if name not in reuse], "reuse": reuse}


def release_admission(candidate_sha: str, build_id: str, identities: Mapping[str, str],
                      envelope: Mapping, source: ValidatedSource, payload_id: str,
                      *, platform: str, now: str | datetime | None = None) -> dict:
    """Admit a new release SHA using one compatible successful stable envelope.

    Envelope: {tier:'stable', status:'passed', platform, build_id, payload_id,
    tests:{name:{identity,status}}, source:{run_id,artifact_id,check_id,run_attempt}}.
    ``payload_id`` is the independently checked identity of the sealed stable
    payload, not a newly rebuilt binary. The exact current stable test identity
    map must be provided; missing, extra, failed or changed tests fail closed.
    Reused nightly tests may be sealed by stable, so their original artifacts
    need not remain available after the stable envelope itself is validated.
    """
    now = _time(now) if now is not None else datetime.now(timezone.utc)
    if platform not in PLATFORMS or envelope.get("platform") != platform:
        raise ValueError("release platform does not match the stable envelope")
    if (not candidate_sha or not build_id or not identities or not payload_id
            or any(not isinstance(identity, str) or not identity for identity in identities.values())):
        raise ValueError("release admission requires candidate, build, tests and sealed payload")
    if not _bound_source(envelope, [source], now) or source.branch != "stable":
        raise ValueError("release requires externally verified stable evidence")
    if envelope.get("tier") != "stable" or envelope.get("status") != "passed" or envelope.get("invalidated_by"):
        raise ValueError("release requires a successful stable envelope")
    if envelope.get("build_id") != build_id or envelope.get("payload_id") != payload_id:
        raise ValueError("release build or sealed payload identity mismatch")
    tests = envelope.get("tests", {})
    if not isinstance(tests, Mapping) or set(tests) != set(identities):
        raise ValueError("release stable test scope mismatch")
    if any(not isinstance(tests[name], Mapping) or tests[name].get("identity") != identity
           or tests[name].get("status") != "passed" or tests[name].get("invalidated_by") for name, identity in identities.items()):
        raise ValueError("release stable test evidence is incomplete or incompatible")
    return {"schema": SCHEMA, "candidate_sha": candidate_sha, "tier": "release",
            "build_id": build_id, "payload_id": payload_id, "platform": platform,
            "reused_source": source.provenance(), "tests": sorted(identities),
            "build_required": False, "run": []}


def release_admissions(candidate_sha: str, platforms: Mapping[str, Mapping],
                       now: str | datetime | None = None) -> dict:
    """Require all three official platforms, then return aggregate admission.

    ``platforms`` must have exactly linux/windows/macos keys, each containing
    build_id, identities, envelope, source (ValidatedSource), and payload_id.
    Every platform is checked against its own sealed stable payload and inputs;
    a missing platform or tolerated failure cannot produce an aggregate pass.
    All platforms must originate from one successful stable run and attempt.
    """
    if set(platforms) != PLATFORMS:
        raise ValueError("release requires linux, windows and macos evidence together")
    admissions = {}
    for platform, item in sorted(platforms.items()):
        admissions[platform] = release_admission(
            candidate_sha, item["build_id"], item["identities"], item["envelope"],
            item["source"], item["payload_id"], platform=platform, now=now)
    origins = {tuple(admission["reused_source"][key] for key in
                     ("repository", "workflow_path", "run_id", "run_attempt", "head_sha"))
               for admission in admissions.values()}
    if len(origins) != 1:
        raise ValueError("all platforms must come from the same successful stable run and attempt")
    return {"schema": SCHEMA, "candidate_sha": candidate_sha, "tier": "release",
            "status": "passed", "platforms": admissions, "build_required": False, "run": []}
