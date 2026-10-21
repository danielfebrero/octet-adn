"""Orchestrateur continu des 10 éprouvettes.

python -m octet_adn.runner --labs all --ticks-per-cycle 200
Chaque cycle : recharge le code du moteur (hot-reload), reprend le checkpoint de
chaque labo, fait vivre son pool, mesure la fitness, sélectionne, sauvegarde,
ajoute une ligne à metrics.jsonl et régénère labs_data/status.md.
"""
from __future__ import annotations
import argparse
import importlib
import json
import random
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

import octet_adn

MODULES = ["octet_adn.isa", "octet_adn.vm", "octet_adn.mutate", "octet_adn.genome",
           "octet_adn.world", "octet_adn.labs.base"]


def hot_reload():
    """Recharge la popote : un upgrade ISA/VM profite à tous les labos au cycle suivant."""
    for m in MODULES:
        if m in sys.modules:
            importlib.reload(sys.modules[m])
    for m in sorted(k for k in sys.modules if k.startswith("octet_adn.labs.") and k != "octet_adn.labs.base"):
        importlib.reload(sys.modules[m])
    importlib.reload(sys.modules["octet_adn.labs"])
    importlib.reload(sys.modules["octet_adn.dashboard"])


def labs_registry():
    return importlib.import_module("octet_adn.labs").LABS


def load_ckpt(d: Path) -> dict | None:
    p = d / "checkpoints" / "latest.json"
    return json.loads(p.read_text()) if p.exists() else None


def save_ckpt(d: Path, ck: dict):
    cdir = d / "checkpoints"
    cdir.mkdir(parents=True, exist_ok=True)
    tmp = cdir / "latest.json.tmp"
    tmp.write_text(json.dumps(ck))
    tmp.replace(cdir / "latest.json")
    if ck["best_genome"]:
        (d / "champion.adn").write_bytes(bytes.fromhex(ck["best_genome"]))


def run_lab_cycle(name: str, data_dir: Path | None, ticks: int, base_seed: int,
                  foreign: list[str], persist: bool = True) -> dict:
    LABS = labs_registry()
    lab = LABS[name]()
    d = data_dir / name if data_dir else None
    ck = load_ckpt(d) if d else None
    if ck is None or not ck.get("pool"):
        seeds = lab.seeds()
        ck = {"lab": name, "cycle": 0, "best_fitness": 0.0, "best_genome": "",
              "pool": [(g.hex(), sp) for g, sp in seeds]}
    cycle = ck["cycle"] + 1
    if name == "org-seeds":
        lab.foreign = foreign
    rng = random.Random(f"{name}:{cycle}:{base_seed}")
    t0 = time.perf_counter()
    w, fit, new_pool, survived = lab.run_cycle(ck["pool"], rng, ticks)
    accepted = bool(new_pool) and fit >= ACCEPT * ck["best_fitness"]
    if accepted:
        ck["pool"] = new_pool
    if fit > ck["best_fitness"] and new_pool:
        ck["best_fitness"], ck["best_genome"] = fit, new_pool[0][0]
    ck["cycle"] = cycle
    ck["engine"] = octet_adn.engine_version()
    lens = [len(c.dna) for c in w.cs] or [0]
    row = {
        "ts": datetime.now(timezone.utc).astimezone().isoformat(timespec="seconds"),
        "lab": name, "cycle": cycle, "ticks": ticks, "survived_ticks": survived,
        "fitness": round(fit, 5), "best_fitness": round(ck["best_fitness"], 5),
        "accepted": accepted, "pop_final": len(w.cs), "pop_mean": round(lab.mean_pop(), 2),
        "born": w.stats["born"], "dead": w.stats["dead"],
        "store_writes": w.stats["store_writes"], "useful_stores": w.stats["useful_stores"],
        "emits": w.stats["emits"], "genome_len_mean": sum(lens) / len(lens),
        "engine": ck["engine"], "state_hash": w.state_hash(),
        "elapsed_s": round(time.perf_counter() - t0, 3),
        "extra": {k: v for k, v in lab.extra.items() if not k.startswith("_")},
    }
    if persist and d:
        d.mkdir(parents=True, exist_ok=True)
        save_ckpt(d, ck)
        with open(d / "metrics.jsonl", "a") as f:
            f.write(json.dumps(row) + "\n")
    return row


ACCEPT = 0.9


def champions(data_dir: Path, names: list[str]) -> list[str]:
    out = []
    for n in names:
        ck = load_ckpt(data_dir / n)
        if ck and ck.get("best_genome") and n != "org-seeds":
            out.append(ck["best_genome"])
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description="Runner continu des éprouvettes Octet-ADN")
    ap.add_argument("--labs", default="all", help="'all' ou liste séparée par des virgules")
    ap.add_argument("--ticks-per-cycle", type=int, default=200)
    ap.add_argument("--cycles", type=int, default=0, help="0 = boucle infinie")
    ap.add_argument("--data-dir", type=Path, default=Path("labs_data"))
    ap.add_argument("--seed", type=int, default=0)
    ap.add_argument("--sleep", type=float, default=0.0, help="pause entre cycles (s)")
    ap.add_argument("--no-reload", action="store_true", help="désactive le hot-reload du moteur")
    ap.add_argument("--dry-run", action="store_true", help="1 cycle, rien n'est écrit sur disque")
    a = ap.parse_args(argv)

    all_names = list(labs_registry())
    names = all_names if a.labs == "all" else [n.strip() for n in a.labs.split(",")]
    bad = [n for n in names if n not in all_names]
    if bad:
        ap.error(f"labos inconnus : {bad} (dispo : {', '.join(all_names)})")
    cycles = 1 if a.dry_run else a.cycles
    from octet_adn import dashboard
    k, last_engine, rows = 0, None, []
    while cycles == 0 or k < cycles:
        if not a.no_reload and k > 0:
            hot_reload()
            from octet_adn import dashboard  # noqa: F811 (version rechargée)
        eng = octet_adn.engine_version()
        if last_engine and eng != last_engine:
            print(f"[popote] moteur mis à jour {last_engine} → {eng} : appliqué à tous les labos")
        last_engine = eng
        data = None if a.dry_run else a.data_dir
        foreign = champions(a.data_dir, all_names) if data else []
        for n in names:
            r = run_lab_cycle(n, data, a.ticks_per_cycle, a.seed, foreign, persist=not a.dry_run)
            rows.append(r)
            print(f"[{n:>13}] cycle {r['cycle']:>4}  fitness={r['fitness']:<9.4g} "
                  f"best={r['best_fitness']:<9.4g} pop={r['pop_final']:>3}  "
                  f"{'✓' if r['accepted'] else '·'}  {r['elapsed_s']}s", flush=True)
        if data:
            dashboard.write(a.data_dir, names)
        k += 1
        if a.sleep and (cycles == 0 or k < cycles):
            time.sleep(a.sleep)
    return rows


if __name__ == "__main__":
    main()
