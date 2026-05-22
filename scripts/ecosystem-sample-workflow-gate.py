#!/usr/bin/env python3
"""Cross-repo ecosystem sample-workflow gate proxy.

MIGRATION-NEEDED: M-ECOSYSTEM-01 (docs/rollups/MIGRATION-LEDGER.md)

Same shape and same migration disposition as
``scripts/ecosystem-product-gate.py``: silent exit 0 when the sibling
``styio-spio`` repository is not present, with no current CI wiring.
"""
from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CANONICAL_GATE = ROOT.parent / "styio-spio" / "scripts" / "ecosystem-sample-workflow-gate.py"


def main(argv: list[str] | None = None) -> int:
    args = list(sys.argv[1:] if argv is None else argv)
    if not CANONICAL_GATE.is_file():
        payload = {
            "ok": True,
            "skipped": True,
            "reason": f"canonical gate not found at {CANONICAL_GATE}",
        }
        if "--json" in args:
            print(json.dumps(payload, sort_keys=True))
        else:
            print(f"[SKIP] {payload['reason']}")
        return 0

    proc = subprocess.run([sys.executable, str(CANONICAL_GATE), *args], cwd=ROOT.parent)
    return proc.returncode


if __name__ == "__main__":
    raise SystemExit(main())
