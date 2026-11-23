#!/usr/bin/env python3
"""Human↔organism exchange. Never execute organism bytes."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import time
from pathlib import Path

BOOK = 7
BOOKSIZE = 65536
COLLECT = Path(__file__).resolve().parents[1] / "transmission-20261026" / "collect.py"


def sha(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest()


def atomic(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_name(path.name + ".tmp")
    with tmp.open("wb") as f:
        f.write(data)
        f.flush()
        os.fsync(f.fileno())
    os.replace(tmp, path)
    fd = os.open(path.parent, os.O_DIRECTORY)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)


def world_paths(world: Path) -> dict:
    world = world.resolve()
    bridge = world / "transmission" / "bridge"
    ex = world / "exchange"
    return {
        "world": world,
        "bridge": bridge,
        "book": bridge / f"{BOOK}.book",
        "ex": ex,
        "inbox": ex / "in",
        "outbox": ex / "out",
        "journal": ex / "journal.jsonl",
    }


def next_seq(inbox: Path) -> int:
    nums = [int(p.stem) for p in inbox.glob("*.json") if p.stem.isdigit()]
    return max(nums, default=0) + 1


def send(world: Path, payload: bytes, origin: str, note: str) -> dict:
    if len(payload) > BOOKSIZE:
        raise ValueError(f"message exceeds {BOOKSIZE} bytes")
    if origin not in ("daniel", "grok"):
        raise ValueError("from must be daniel or grok")
    p = world_paths(world)
    p["inbox"].mkdir(parents=True, exist_ok=True)
    p["bridge"].mkdir(parents=True, exist_ok=True)
    seq = next_seq(p["inbox"])
    rec = {
        "seq": seq,
        "at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "from": origin,
        "bytes": len(payload),
        "sha256": sha(payload),
        "note": note,
        "slot": BOOK,
    }
    stem = p["inbox"] / f"{seq:012d}"
    atomic(stem.with_suffix(".bin"), payload)
    atomic(stem.with_suffix(".json"), json.dumps(rec, ensure_ascii=False, indent=2).encode() + b"\n")
    atomic(p["book"], payload)
    with p["journal"].open("a") as f:
        f.write(json.dumps({"kind": "in", **rec}, ensure_ascii=False) + "\n")
        f.flush()
        os.fsync(f.fileno())
    return rec


def inbox(world: Path) -> list:
    p = world_paths(world)
    rows = []
    for meta in sorted((p["inbox"]).glob("*.json")) if p["inbox"].exists() else []:
        rows.append(json.loads(meta.read_text()))
    return rows


def outbox(world: Path, mark: bool) -> dict:
    p = world_paths(world)
    trans = p["world"] / "transmission"
    if not trans.exists():
        raise FileNotFoundError(trans)
    cmd = ["python3", str(COLLECT), str(trans)]
    if mark:
        cmd.append("--mark")
    out = subprocess.check_output(cmd, text=True)
    data = json.loads(out)
    p["outbox"].mkdir(parents=True, exist_ok=True)
    rec = {
        "kind": "out",
        "at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "picked_at": data.get("picked_at"),
        "after": data.get("after"),
        "through": data.get("through"),
        "n": len(data.get("messages") or []),
        "interpretation": data.get("interpretation"),
    }
    atomic(p["outbox"] / f"{int(time.time()):012d}.json", json.dumps({**rec, "collect": data}, ensure_ascii=False, indent=2).encode() + b"\n")
    with p["journal"].open("a") as f:
        f.write(json.dumps(rec, ensure_ascii=False) + "\n")
        f.flush()
        os.fsync(f.fileno())
    return data


def status(world: Path) -> dict:
    p = world_paths(world)
    book = p["book"].read_bytes() if p["book"].exists() else b""
    return {
        "world": str(p["world"]),
        "book7_bytes": len(book),
        "book7_sha256": sha(book) if book else None,
        "inbox": len(inbox(world)),
        "journal": p["journal"].exists(),
    }


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    sub = ap.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("send")
    s.add_argument("world", type=Path)
    s.add_argument("file", type=Path)
    s.add_argument("--from", dest="origin", default="grok", choices=("daniel", "grok"))
    s.add_argument("--note", default="")
    q = sub.add_parser("inbox")
    q.add_argument("world", type=Path)
    o = sub.add_parser("outbox")
    o.add_argument("world", type=Path)
    o.add_argument("--mark", action="store_true")
    t = sub.add_parser("status")
    t.add_argument("world", type=Path)
    a = ap.parse_args()
    if a.cmd == "send":
        rec = send(a.world, a.file.read_bytes(), a.origin, a.note)
        print(json.dumps(rec, ensure_ascii=False, indent=2))
    elif a.cmd == "inbox":
        print(json.dumps(inbox(a.world), ensure_ascii=False, indent=2))
    elif a.cmd == "outbox":
        print(json.dumps(outbox(a.world, a.mark), ensure_ascii=False, indent=2))
    else:
        print(json.dumps(status(a.world), ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
