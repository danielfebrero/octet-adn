#!/usr/bin/env python3
"""Pick up only checkpoint-committed mail; never interpret or execute bytes."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import time

parser = argparse.ArgumentParser()
parser.add_argument("world", type=Path)
parser.add_argument("--mark", action="store_true", help="save manifest and advance pickup cursor")
args = parser.parse_args()
root = args.world


def fnv64(data):
    value = 1469598103934665603
    for byte in data:
        value = ((value ^ byte) * 1099511628211) & 0xffffffffffffffff
    return value


# Atomic checkpoint file: fixed little-endian ABI v1, first seven World u64s.
with (root / "state.ck").open("rb") as f:
    header = f.read(32)
    magic, version, size, checksum = struct.unpack("<4Q", header)
    if magic != 0x31534E415254434F or version != 1:
        raise SystemExit("unknown checkpoint ABI")
    payload = f.read()
    if len(payload) != size or fnv64(payload) != checksum:
        raise SystemExit("checkpoint size/checksum mismatch")
    fields = struct.unpack("<7Q", payload[:56])
    tick, checkpoint_seq = fields[0], fields[6]
committed = min(checkpoint_seq, int((root / "mail.committed").read_text()))
cursor_file = root / "pickups" / "cursor"
cursor = int(cursor_file.read_text()) if cursor_file.exists() else 0
messages = []
for seq in range(cursor + 1, committed + 1):
    name = f"{seq:016d}"
    payload = root / "mail" / (name + ".bin")
    meta = root / "mail" / (name + ".tsv")
    fields = meta.read_text().splitlines()
    metadata = dict(zip(fields[0].split("\t"), fields[1].split("\t")))
    body = payload.read_bytes()
    if len(body) != int(metadata["bytes"]) or fnv64(body) != int(metadata["fnv64"], 16):
        raise SystemExit("mail size/checksum mismatch")
    messages.append({"sequence": seq, "metadata": metadata, "path": str(payload.resolve()),
                     "sha256": hashlib.sha256(body).hexdigest(), "bytes": len(body)})
manifest = {"picked_at": int(time.time()), "tick_observed": tick, "after": cursor,
            "through": committed, "messages": messages,
            "interpretation": "None. Payloads are untrusted organism bytes; founder programs are human-authored."}
if args.mark:
    # Local helper: no delivery and no removal. Originals remain in mail/.
    dest = root / "pickups" / f"{committed:016d}.json"
    dest.write_text(json.dumps(manifest, ensure_ascii=False, indent=2))
    temp = cursor_file.with_suffix(".tmp")
    temp.write_text(str(committed) + "\n")
    temp.replace(cursor_file)
print(json.dumps(manifest, ensure_ascii=False, indent=2))
