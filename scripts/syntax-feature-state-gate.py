#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import re
import sys
import tomllib
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_FEATURES_DIR = ROOT / "docs" / "design" / "syntax" / "features"
DEFAULT_GRAPH = ROOT / "docs" / "design" / "syntax" / "SYNTAX-FEATURE-GRAPH.json"
FEATURE_BLOCK_RE = re.compile(
    r"^```toml syntax-feature[ \t]*\n(?P<body>.*?)^```[ \t]*$",
    re.MULTILINE | re.DOTALL,
)
FEATURE_ID_RE = re.compile(r"^[a-z0-9][a-z0-9_.-]*$")

DECISION_STATE_ORDER = (
    "draft",
    "review",
    "accepted",
    "reserved",
    "deferred",
    "rejected",
    "superseded",
)
DECISION_STATES = frozenset(DECISION_STATE_ORDER)
DELIVERY_RANK = {
    "not_started": 0,
    "implementing": 1,
    "verified": 2,
    "converged": 3,
}
REQUIRED_DOCUMENT_ROLES = {
    "grammar",
    "tokens",
    "semantics",
    "diagnostics",
    "compatibility",
    "teaching",
    "implementation",
    "evidence",
}
REQUIRED_PREREQUISITES = {
    "language-owner-approval",
    "keyword-free-contract",
    "nightly-parser-authority",
    "grammar-contract",
    "semantic-contract",
    "diagnostic-boundary",
    "compatibility-decision",
    "golden-evidence",
}
@dataclass(frozen=True)
class FeatureDocument:
    path: Path
    data: dict[str, Any]

    @property
    def feature_id(self) -> str:
        return str(self.data["id"])


def rel(path: Path, root: Path = ROOT) -> str:
    try:
        return path.relative_to(root).as_posix()
    except ValueError:
        return path.as_posix()


def string_list(
    value: Any,
    context: str,
    errors: list[str],
    *,
    allow_empty: bool = False,
) -> list[str]:
    if not isinstance(value, list):
        errors.append(f"{context} must be a list")
        return []
    if not value and not allow_empty:
        errors.append(f"{context} must be non-empty")
        return []

    output: list[str] = []
    seen: set[str] = set()
    for index, item in enumerate(value):
        if not isinstance(item, str) or not item.strip():
            errors.append(f"{context}[{index}] must be a non-empty string")
            continue
        cleaned = item.strip()
        if cleaned in seen:
            errors.append(f"{context} contains duplicate entry `{cleaned}`")
            continue
        seen.add(cleaned)
        output.append(cleaned)
    return output


def require_file(
    root: Path,
    value: Any,
    context: str,
    errors: list[str],
) -> str | None:
    if not isinstance(value, str) or not value.strip():
        errors.append(f"{context} must be a non-empty repository-relative path")
        return None
    relative_path = Path(value)
    if relative_path.is_absolute() or ".." in relative_path.parts:
        errors.append(f"{context} must stay inside the repository")
        return None
    candidate = root / relative_path
    try:
        candidate.resolve().relative_to(root.resolve())
    except ValueError:
        errors.append(f"{context} must stay inside the repository")
        return None
    if not candidate.is_file():
        errors.append(f"{context} references missing file `{value}`")
        return None
    return value


def expected_oracle_paths(root: Path, case_path: Path) -> list[Path]:
    parts = case_path.relative_to(root).parts
    if len(parts) >= 4 and parts[0:2] == ("tests", "pipeline_cases") and case_path.name == "input.styio":
        expected = case_path.parent / "expected"
        return [
            expected / "tokens.txt",
            expected / "ast.txt",
            expected / "styio_ir.txt",
            expected / "llvm_ir.txt",
            expected / "stdout.txt",
        ]

    if len(parts) >= 4 and parts[0:2] in {
        ("tests", "milestones"),
        ("tests", "features"),
    } and case_path.suffix == ".styio":
        expected = case_path.parent / "expected"
        suffix = ".err" if case_path.stem.startswith("e") else ".out"
        return [expected / f"{case_path.stem}{suffix}"]

    return []


def validate_golden_case(root: Path, value: str, context: str, errors: list[str]) -> None:
    checked = require_file(root, value, f"{context} golden case", errors)
    if checked is None:
        return
    case_path = root / checked
    for oracle in expected_oracle_paths(root, case_path):
        if not oracle.is_file():
            errors.append(
                f"{context} is missing oracle `{rel(oracle, root)}` for `{value}`"
            )


def extract_feature_contract(path: Path, errors: list[str]) -> dict[str, Any] | None:
    text = path.read_text(encoding="utf-8")
    blocks = list(FEATURE_BLOCK_RE.finditer(text))
    if len(blocks) != 1:
        errors.append(
            f"{rel(path)} must contain exactly one fenced `toml syntax-feature` contract"
        )
        return None
    try:
        data = tomllib.loads(blocks[0].group("body"))
    except tomllib.TOMLDecodeError as exc:
        errors.append(f"{rel(path)} has invalid syntax-feature TOML: {exc}")
        return None
    if not isinstance(data, dict):
        errors.append(f"{rel(path)} syntax-feature contract must be a TOML table")
        return None
    return data


def validate_documents(
    root: Path,
    feature_id: str,
    value: Any,
    errors: list[str],
) -> tuple[dict[str, list[str]], set[str]]:
    context = f"feature `{feature_id}` documents"
    if not isinstance(value, dict):
        errors.append(f"{context} must be a table")
        return {}, set(REQUIRED_DOCUMENT_ROLES)

    documents: dict[str, list[str]] = {}
    for role, paths in value.items():
        if not isinstance(role, str) or not role.strip():
            errors.append(f"{context} contains an invalid role")
            continue
        cleaned_paths = string_list(paths, f"{context}.{role}", errors)
        for index, path in enumerate(cleaned_paths):
            require_file(root, path, f"{context}.{role}[{index}]", errors)
        documents[role] = cleaned_paths
    return documents, REQUIRED_DOCUMENT_ROLES - set(documents)


def validate_prerequisites(
    root: Path,
    feature_id: str,
    value: Any,
    errors: list[str],
) -> tuple[dict[str, str], set[str]]:
    context = f"feature `{feature_id}` prerequisites"
    if not isinstance(value, dict):
        errors.append(f"{context} must be a table")
        return {}, set(REQUIRED_PREREQUISITES)

    prerequisites: dict[str, str] = {}
    for prerequisite_id, evidence in value.items():
        if not isinstance(prerequisite_id, str) or not FEATURE_ID_RE.match(prerequisite_id):
            errors.append(f"{context} contains invalid id `{prerequisite_id}`")
            continue
        checked = require_file(
            root,
            evidence,
            f"{context}.{prerequisite_id}",
            errors,
        )
        if checked is not None:
            prerequisites[prerequisite_id] = checked
    return prerequisites, REQUIRED_PREREQUISITES - set(prerequisites)


def normalize_requirement(
    value: Any,
    context: str,
    errors: list[str],
) -> dict[str, str] | None:
    if not isinstance(value, dict):
        errors.append(f"{context} must be an inline table")
        return None
    dependency_id = value.get("id")
    if not isinstance(dependency_id, str) or not FEATURE_ID_RE.match(dependency_id):
        errors.append(f"{context}.id must be a feature id")
        return None
    decision_state = value.get("decision_state", "accepted")
    delivery_state = value.get("delivery_state", "converged")
    if decision_state not in {"review", "accepted"}:
        errors.append(f"{context}.decision_state must be `review` or `accepted`")
    if delivery_state not in DELIVERY_RANK:
        errors.append(f"{context}.delivery_state is invalid")
    return {
        "id": dependency_id,
        "decision_state": str(decision_state),
        "delivery_state": str(delivery_state),
    }


def validate_dependencies(
    feature_id: str,
    value: Any,
    errors: list[str],
) -> dict[str, Any]:
    context = f"feature `{feature_id}` dependencies"
    if not isinstance(value, dict):
        errors.append(f"{context} must be a table")
        value = {}

    normalized: dict[str, Any] = {}
    requirements: list[dict[str, str]] = []
    raw_requires = value.get("requires", [])
    if not isinstance(raw_requires, list):
        errors.append(f"{context}.requires must be a list")
        raw_requires = []
    for index, item in enumerate(raw_requires):
        requirement = normalize_requirement(
            item,
            f"{context}.requires[{index}]",
            errors,
        )
        if requirement is not None:
            requirements.append(requirement)
    normalized["requires"] = requirements

    groups: list[list[dict[str, str]]] = []
    raw_groups = value.get("requires_any", [])
    if not isinstance(raw_groups, list):
        errors.append(f"{context}.requires_any must be a list of requirement groups")
        raw_groups = []
    for group_index, raw_group in enumerate(raw_groups):
        if not isinstance(raw_group, list) or not raw_group:
            errors.append(
                f"{context}.requires_any[{group_index}] must be a non-empty list"
            )
            continue
        group: list[dict[str, str]] = []
        for item_index, item in enumerate(raw_group):
            requirement = normalize_requirement(
                item,
                f"{context}.requires_any[{group_index}][{item_index}]",
                errors,
            )
            if requirement is not None:
                group.append(requirement)
        if group:
            groups.append(group)
    normalized["requires_any"] = groups

    for kind in ("extends", "conflicts", "supersedes", "after"):
        normalized[kind] = string_list(
            value.get(kind, []),
            f"{context}.{kind}",
            errors,
            allow_empty=True,
        )

    for kind in ("requires",):
        ids = [item["id"] for item in normalized[kind]]
        if len(ids) != len(set(ids)):
            errors.append(f"{context}.{kind} contains duplicate feature ids")
    for group_index, group in enumerate(normalized["requires_any"]):
        ids = [item["id"] for item in group]
        if len(ids) != len(set(ids)):
            errors.append(
                f"{context}.requires_any[{group_index}] contains duplicate feature ids"
            )

    referenced = dependency_ids(normalized)
    if feature_id in referenced:
        errors.append(f"{context} cannot reference itself")
    return normalized


def dependency_ids(dependencies: dict[str, Any]) -> set[str]:
    output = {
        item["id"]
        for item in dependencies.get("requires", [])
        if isinstance(item, dict) and isinstance(item.get("id"), str)
    }
    for group in dependencies.get("requires_any", []):
        output.update(
            item["id"]
            for item in group
            if isinstance(item, dict) and isinstance(item.get("id"), str)
        )
    for kind in ("extends", "conflicts", "supersedes", "after"):
        output.update(
            item
            for item in dependencies.get(kind, [])
            if isinstance(item, str)
        )
    return output


def load_feature_documents(
    root: Path,
    features_dir: Path,
) -> tuple[dict[str, FeatureDocument], list[str], dict[str, dict[str, Any]]]:
    errors: list[str] = []
    features: dict[str, FeatureDocument] = {}
    derived: dict[str, dict[str, Any]] = {}

    if not features_dir.is_dir():
        return {}, [f"missing feature SSOT directory: {rel(features_dir, root)}"], {}

    feature_paths = sorted(
        path
        for path in features_dir.glob("*.md")
        if path.name not in {"README.md", "INDEX.md"}
    )
    if not feature_paths:
        return {}, [f"no feature SSOT documents found in {rel(features_dir, root)}"], {}

    for path in feature_paths:
        data = extract_feature_contract(path, errors)
        if data is None:
            continue
        feature_id = data.get("id")
        context = f"{rel(path, root)} syntax-feature contract"
        if data.get("schema_version") != 1:
            errors.append(f"{context}.schema_version must be 1")
        if not isinstance(feature_id, str) or not FEATURE_ID_RE.match(feature_id):
            errors.append(f"{context}.id must be a lowercase feature id")
            continue
        expected_stem = feature_id.replace(".", "-")
        if path.stem != expected_stem:
            errors.append(
                f"{context}.id `{feature_id}` requires filename `{expected_stem}.md`"
            )
        if feature_id in features:
            errors.append(
                f"duplicate feature id `{feature_id}` in "
                f"{rel(features[feature_id].path, root)} and {rel(path, root)}"
            )
            continue

        for field in ("title", "kind", "owner", "syntax", "resolution"):
            value = data.get(field)
            if not isinstance(value, str) or not value.strip():
                errors.append(f"feature `{feature_id}` field `{field}` must be non-empty")

        decision_state = data.get("decision_state")
        delivery_state = data.get("delivery_state")
        if decision_state not in DECISION_STATES:
            errors.append(f"feature `{feature_id}` has invalid decision_state")
        if delivery_state not in DELIVERY_RANK:
            errors.append(f"feature `{feature_id}` has invalid delivery_state")
        if decision_state != "accepted" and delivery_state != "not_started":
            errors.append(
                f"feature `{feature_id}` may advance delivery only while decision_state is `accepted`"
            )
        if decision_state == "deferred":
            reopen_when = data.get("reopen_when")
            if not isinstance(reopen_when, str) or not reopen_when.strip():
                errors.append(f"deferred feature `{feature_id}` must define reopen_when")

        documents, missing_roles = validate_documents(
            root,
            feature_id,
            data.get("documents"),
            errors,
        )
        prerequisites, missing_prerequisites = validate_prerequisites(
            root,
            feature_id,
            data.get("prerequisites"),
            errors,
        )
        dependencies = validate_dependencies(
            feature_id,
            data.get("dependencies"),
            errors,
        )

        missing_artifacts: list[str] = []
        implementation = data.get("implementation", {})
        if not isinstance(implementation, dict):
            errors.append(f"feature `{feature_id}` implementation must be a table")
            implementation = {}
        implementation_path_value = implementation.get("path")
        if isinstance(implementation_path_value, str) and implementation_path_value.strip():
            implementation_path = require_file(
                root,
                implementation_path_value,
                f"feature `{feature_id}` implementation.path",
                errors,
            )
        else:
            implementation_path = None
            missing_artifacts.append("implementation.path")
        symbol = implementation.get("symbol")
        if not isinstance(symbol, str) or not symbol.strip():
            missing_artifacts.append("implementation.symbol")
        elif implementation_path is not None:
            implementation_text = (root / implementation_path).read_text(
                encoding="utf-8",
                errors="replace",
            )
            if symbol.strip() not in implementation_text:
                errors.append(
                    f"feature `{feature_id}` implementation symbol `{symbol}` "
                    f"is absent from `{implementation_path}`"
                )

        golden_cases = string_list(
            data.get("golden_cases", []),
            f"feature `{feature_id}` golden_cases",
            errors,
            allow_empty=True,
        )
        if not golden_cases:
            missing_artifacts.append("golden_cases")
        for golden_case in golden_cases:
            validate_golden_case(
                root,
                golden_case,
                f"feature `{feature_id}`",
                errors,
            )

        normalized_data = dict(data)
        normalized_data["documents"] = documents
        normalized_data["prerequisites"] = prerequisites
        normalized_data["dependencies"] = dependencies
        normalized_data["implementation"] = {
            "path": implementation_path or "",
            "symbol": str(symbol or ""),
            "owner": str(implementation.get("owner", data.get("owner", ""))),
        }
        normalized_data["golden_cases"] = golden_cases
        features[feature_id] = FeatureDocument(path=path, data=normalized_data)
        derived[feature_id] = {
            "missing_document_roles": sorted(missing_roles),
            "missing_prerequisites": sorted(missing_prerequisites),
            "missing_artifacts": sorted(missing_artifacts),
            "dependency_blockers": [],
        }

    return features, errors, derived


def decision_meets(actual: str, required: str) -> bool:
    if required == "accepted":
        return actual == "accepted"
    if required == "review":
        return actual in {"review", "accepted"}
    return False


def requirement_meets(
    requirement: dict[str, str],
    target: FeatureDocument,
) -> bool:
    return decision_meets(
        str(target.data.get("decision_state")),
        requirement["decision_state"],
    ) and DELIVERY_RANK.get(str(target.data.get("delivery_state")), -1) >= DELIVERY_RANK.get(
        requirement["delivery_state"],
        99,
    )


def directed_edges(feature: FeatureDocument) -> Iterable[str]:
    dependencies = feature.data["dependencies"]
    for requirement in dependencies["requires"]:
        yield requirement["id"]
    for group in dependencies["requires_any"]:
        for requirement in group:
            yield requirement["id"]
    for kind in ("extends", "supersedes", "after"):
        yield from dependencies[kind]


def strongly_connected_components(
    graph: dict[str, set[str]],
) -> list[list[str]]:
    preorder: dict[str, int] = {}
    lowlink: dict[str, int] = {}
    found: set[str] = set()
    pending_component: list[str] = []
    components: list[list[str]] = []
    preorder_counter = 0
    neighbors = {
        node: iter(sorted(target for target in targets if target in graph))
        for node, targets in graph.items()
    }

    # Iterative Tarjan/Nuutila-style traversal keeps O(V + E) behavior without
    # inheriting Python's recursion-depth ceiling as the feature graph grows.
    for source in sorted(graph):
        if source in found:
            continue
        work = [source]
        while work:
            node = work[-1]
            if node not in preorder:
                preorder_counter += 1
                preorder[node] = preorder_counter

            for target in neighbors[node]:
                if target not in preorder:
                    work.append(target)
                    break
            else:
                lowlink[node] = preorder[node]
                for target in graph[node]:
                    if target not in graph or target in found:
                        continue
                    if preorder[target] > preorder[node]:
                        lowlink[node] = min(lowlink[node], lowlink[target])
                    else:
                        lowlink[node] = min(lowlink[node], preorder[target])

                work.pop()
                if lowlink[node] == preorder[node]:
                    component = [node]
                    while (
                        pending_component
                        and preorder[pending_component[-1]] > preorder[node]
                    ):
                        component.append(pending_component.pop())
                    found.update(component)
                    components.append(sorted(component))
                else:
                    pending_component.append(node)
    return components


def evaluate_dependency_graph(
    features: dict[str, FeatureDocument],
    derived: dict[str, dict[str, Any]],
    errors: list[str],
) -> None:
    graph: dict[str, set[str]] = {
        feature_id: set(directed_edges(feature))
        for feature_id, feature in features.items()
    }
    superseders: dict[str, list[str]] = {feature_id: [] for feature_id in features}
    for feature_id, feature in features.items():
        for target_id in feature.data["dependencies"]["supersedes"]:
            if target_id in superseders:
                superseders[target_id].append(feature_id)

    for feature_id, feature in features.items():
        dependencies = feature.data["dependencies"]
        for dependency_id in sorted(dependency_ids(dependencies)):
            if dependency_id not in features:
                errors.append(
                    f"feature `{feature_id}` references missing dependency `{dependency_id}`"
                )

        blockers: list[str] = derived[feature_id]["dependency_blockers"]
        for requirement in dependencies["requires"]:
            target = features.get(requirement["id"])
            if target is not None and not requirement_meets(requirement, target):
                blockers.append(
                    f"requires {requirement['id']} at "
                    f"{requirement['decision_state']}/{requirement['delivery_state']}"
                )

        for group in dependencies["requires_any"]:
            available = [
                requirement
                for requirement in group
                if (target := features.get(requirement["id"])) is not None
                and requirement_meets(requirement, target)
            ]
            if not available:
                blockers.append(
                    "requires any of "
                    + ", ".join(requirement["id"] for requirement in group)
                )

        for dependency_id in dependencies["extends"]:
            target = features.get(dependency_id)
            if target is not None and not (
                target.data.get("decision_state") == "accepted"
                and target.data.get("delivery_state") == "converged"
            ):
                blockers.append(f"extends non-converged feature {dependency_id}")

        for dependency_id in dependencies["after"]:
            target = features.get(dependency_id)
            if target is not None and target.data.get("delivery_state") != "converged":
                blockers.append(f"must follow non-converged feature {dependency_id}")

        for dependency_id in dependencies["supersedes"]:
            target = features.get(dependency_id)
            if target is not None and target.data.get("decision_state") != "superseded":
                blockers.append(f"supersedes active feature {dependency_id}")

        for conflict_id in dependencies["conflicts"]:
            target = features.get(conflict_id)
            if target is None:
                continue
            reciprocal = feature_id in target.data["dependencies"]["conflicts"]
            if not reciprocal:
                errors.append(
                    f"feature conflict must be reciprocal: `{feature_id}` conflicts "
                    f"with `{conflict_id}`, but the reverse edge is absent"
                )
            both_active = (
                feature.data.get("decision_state") == "accepted"
                and target.data.get("decision_state") == "accepted"
                and DELIVERY_RANK.get(str(feature.data.get("delivery_state")), 0) >= 1
                and DELIVERY_RANK.get(str(target.data.get("delivery_state")), 0) >= 1
            )
            if both_active:
                blockers.append(f"conflicts with active feature {conflict_id}")

    for feature_id, feature in features.items():
        if feature.data.get("decision_state") != "superseded":
            continue
        accepted_superseders = [
            superseder_id
            for superseder_id in superseders[feature_id]
            if features[superseder_id].data.get("decision_state") == "accepted"
        ]
        if not accepted_superseders:
            errors.append(
                f"superseded feature `{feature_id}` must have an accepted incoming "
                "`supersedes` edge"
            )

    for component in strongly_connected_components(graph):
        if len(component) > 1:
            errors.append(
                "syntax feature dependency cycle: " + " -> ".join(component + [component[0]])
            )
            for feature_id in component:
                derived[feature_id]["dependency_blockers"].append(
                    "participates in dependency cycle"
                )


def derive_readiness(
    features: dict[str, FeatureDocument],
    derived: dict[str, dict[str, Any]],
    errors: list[str],
) -> None:
    for feature_id, feature in features.items():
        state = derived[feature_id]
        delivery_state = str(feature.data.get("delivery_state"))
        if (
            state["missing_document_roles"]
            or state["missing_prerequisites"]
            or state["missing_artifacts"]
        ):
            readiness = "incomplete"
        elif state["dependency_blockers"]:
            readiness = "blocked"
        else:
            readiness = "ready"
        if readiness != "ready" and delivery_state == "converged":
            readiness = "stale"
        state["readiness"] = readiness

        if (
            DELIVERY_RANK.get(delivery_state, 0) >= DELIVERY_RANK["implementing"]
            and readiness != "ready"
        ):
            errors.append(
                f"feature `{feature_id}` cannot be `{delivery_state}` while readiness is `{readiness}`"
            )


def render_graph(
    root: Path,
    features: dict[str, FeatureDocument],
    derived: dict[str, dict[str, Any]],
) -> str:
    reverse_edges: dict[str, dict[str, list[str]]] = {
        feature_id: {
            "requires": [],
            "requires_any": [],
            "extends": [],
            "conflicts": [],
            "supersedes": [],
            "after": [],
        }
        for feature_id in features
    }
    for source_id, feature in features.items():
        dependencies = feature.data["dependencies"]
        for requirement in dependencies["requires"]:
            if requirement["id"] in reverse_edges:
                reverse_edges[requirement["id"]]["requires"].append(source_id)
        for group in dependencies["requires_any"]:
            for requirement in group:
                if requirement["id"] in reverse_edges:
                    reverse_edges[requirement["id"]]["requires_any"].append(source_id)
        for kind in ("extends", "conflicts", "supersedes", "after"):
            for target_id in dependencies[kind]:
                if target_id in reverse_edges:
                    reverse_edges[target_id][kind].append(source_id)
    for relations in reverse_edges.values():
        for kind, source_ids in relations.items():
            relations[kind] = sorted(set(source_ids))

    payload = {
        "schema_version": 1,
        "generated": True,
        "generated_from": "docs/design/syntax/features/*.md",
        "purpose": (
            "Generated projection of distributed Styio syntax-feature SSOT documents, "
            "their lifecycle states, dependencies, prerequisites, implementation owners, "
            "and golden evidence."
        ),
        "state_model": {
            "decision": list(DECISION_STATE_ORDER),
            "delivery": list(DELIVERY_RANK),
            "readiness": ["incomplete", "blocked", "ready", "stale"],
        },
        "features": [],
    }

    output_features: list[dict[str, Any]] = []
    for feature_id in sorted(features):
        feature = features[feature_id]
        data = feature.data
        output_features.append(
            {
                "id": feature_id,
                "title": data["title"],
                "kind": data["kind"],
                "syntax": data["syntax"],
                "decision_state": data["decision_state"],
                "delivery_state": data["delivery_state"],
                "readiness": derived[feature_id]["readiness"],
                "readiness_details": {
                    "missing_document_roles": derived[feature_id]["missing_document_roles"],
                    "missing_prerequisites": derived[feature_id]["missing_prerequisites"],
                    "missing_artifacts": derived[feature_id]["missing_artifacts"],
                    "dependency_blockers": derived[feature_id]["dependency_blockers"],
                },
                "document": rel(feature.path, root),
                "owner": data["owner"],
                "resolution": data["resolution"],
                "dependencies": data["dependencies"],
                "referenced_by": reverse_edges[feature_id],
                "prerequisites": data["prerequisites"],
                "implementation": data["implementation"],
                "documents": data["documents"],
                "golden_cases": data["golden_cases"],
            }
        )
    payload["features"] = output_features
    return json.dumps(payload, indent=2, ensure_ascii=False, sort_keys=False) + "\n"


def evaluate(
    root: Path,
    features_dir: Path,
) -> tuple[dict[str, FeatureDocument], dict[str, dict[str, Any]], list[str]]:
    features, errors, derived = load_feature_documents(root, features_dir)
    if features:
        evaluate_dependency_graph(features, derived, errors)
        derive_readiness(features, derived, errors)
    return features, derived, errors


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Validate distributed Styio syntax-feature SSOT documents and "
            "their document-driven lifecycle graph."
        )
    )
    parser.add_argument(
        "--features-dir",
        default=str(DEFAULT_FEATURES_DIR),
        help="Directory containing distributed syntax-feature Markdown SSOT documents.",
    )
    parser.add_argument(
        "--graph",
        default=str(DEFAULT_GRAPH),
        help="Generated syntax-feature graph projection.",
    )
    parser.add_argument(
        "--write",
        action="store_true",
        help="Write the generated graph after validation instead of checking it.",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    features_dir = Path(args.features_dir)
    if not features_dir.is_absolute():
        features_dir = ROOT / features_dir
    graph_path = Path(args.graph)
    if not graph_path.is_absolute():
        graph_path = ROOT / graph_path

    features, derived, errors = evaluate(ROOT, features_dir)
    if not errors:
        expected_graph = render_graph(ROOT, features, derived)
        if args.write:
            graph_path.write_text(expected_graph, encoding="utf-8")
        elif not graph_path.is_file():
            errors.append(f"missing generated syntax feature graph: {rel(graph_path)}")
        elif graph_path.read_text(encoding="utf-8") != expected_graph:
            errors.append(
                f"generated syntax feature graph is stale: {rel(graph_path)}; "
                "run `python3 scripts/syntax-feature-state-gate.py --write`"
            )

    if errors:
        print("syntax-feature-state-gate failed:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1

    converged = sum(
        feature.data.get("delivery_state") == "converged"
        for feature in features.values()
    )
    ready = sum(
        state.get("readiness") == "ready"
        for state in derived.values()
    )
    print(
        "syntax-feature-state-gate: ok "
        f"({len(features)} features; {converged} converged; {ready} ready)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
