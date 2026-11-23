#!/usr/bin/env python3
"""Keep 300 legacy acquis; replace Relief/Transmission with live extracts. Read-only on sources."""
from __future__ import annotations

import base64
import collections
import hashlib
import json
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
GEN = HERE.parent / "generateur-20261027"
OLD = GEN / "selection-data" / "selection.json"
EX = GEN / "selection-data" / "extractors"
SNAP = HERE / "snapshots"
sys.path.insert(0, str(GEN))
from selection import ranked_select  # noqa: E402


def sha(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest()


def extract(isa: str, directory: pathlib.Path) -> tuple[bytes, list]:
    blob = (directory / "state.ck").read_bytes()
    raw = subprocess.check_output([str(EX / isa), str(directory)], text=True)
    rows = [json.loads(line) for line in raw.splitlines() if line.strip()]
    rows = list({r["source_id"]: r for r in rows}.values())
    return blob, rows


def pack(isa: str, world: str, blob: bytes, r: dict, ck: pathlib.Path) -> dict:
    native = bytes.fromhex(r.pop("native_hex"))
    off = blob.find(native)
    if off < 0:
        raise ValueError(f"native record not found {isa} {r['source_id']}")
    genome = bytes.fromhex(r["genome_hex"])
    memory = bytes.fromhex(r.get("memory_hex") or "")
    r.update(
        id=f"{ck.parent.name}:{r['source_id']}",
        source_world=world,
        isa=isa,
        native_record_base64=base64.b64encode(native).decode(),
        native_record_sha256=sha(native),
        genome_sha256=sha(genome),
        memory_sha256=sha(memory),
        native_record_size=len(native),
        provenance={
            "checkpoint_path": str(ck),
            "snapshot_path": str(ck),
            "checkpoint_sha256": sha(blob),
            "record_offset": off,
            "source_kind": isa,
            "extracted_at": "2026-11-23T15:43Z",
        },
    )
    return r


def main() -> None:
    old = json.loads(OLD.read_text())
    legacy = [c for c in old["creatures"] if c["isa"] == "legacy"]
    if len(legacy) != 300:
        raise SystemExit(f"expected 300 legacy, got {len(legacy)}")
    out_creatures = list(legacy)
    sources = [s for s in old["sources"] if s["isa"] == "legacy"]
    for isa, world, quota in (
        ("relief", "experiments/vie-relief-20261025", 60),
        ("transmission", "experiments/vie-transmission-20261026", 60),
    ):
        directory = SNAP / isa
        ck = directory / "state.ck"
        blob, rows = extract(isa, directory)
        picked = ranked_select(rows, quota)
        packed = [pack(isa, world, blob, r, ck) for r in picked]
        lineages = collections.Counter(r.get("lineage") for r in packed)
        sources.append(
            {
                "world": world,
                "isa": isa,
                "checkpoint_sha256": sha(blob),
                "candidates": len(rows),
                "selected": len(packed),
                "tick": packed[0].get("tick"),
                "lineages": {str(k): v for k, v in lineages.items()},
            }
        )
        out_creatures.extend(packed)
        print(isa, "candidates", len(rows), "selected", len(packed), "lineages", dict(lineages), flush=True)
    if len(out_creatures) != 420:
        raise SystemExit(f"expected 420, got {len(out_creatures)}")
    doc = {
        "schema": "eg.world.selection.v1",
        "count": 420,
        "selection_policy": "300 legacy acquis of 2026-10-27 plus 60 live Relief and 60 live Transmission of 2026-11-23; rotating maxima, no invented founders.",
        "sources": sources,
        "creatures": out_creatures,
        "exclusions": old.get("exclusions", []),
    }
    dest = HERE / "selection.json"
    dest.write_text(json.dumps(doc, ensure_ascii=False) + "\n")
    print("wrote", dest, "bytes", dest.stat().st_size)


if __name__ == "__main__":
    main()
