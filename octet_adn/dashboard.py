"""Génère labs_data/status.md à partir des metrics.jsonl de chaque labo."""
import json
from pathlib import Path

BARS = "▁▂▃▄▅▆▇█"


def spark(vals: list[float]) -> str:
    if not vals:
        return ""
    lo, hi = min(vals), max(vals)
    span = (hi - lo) or 1.0
    return "".join(BARS[min(7, int((v - lo) / span * 7.999))] for v in vals)


def read_metrics(path: Path) -> list[dict]:
    if not path.exists():
        return []
    return [json.loads(l) for l in path.read_text().splitlines() if l.strip()]


def render(data_dir: Path, lab_names: list[str]) -> str:
    out = ["# Octet-ADN — état des éprouvettes", "",
           "| labo | cycle | fitness | meilleure | pop fin | génome moy. (o) | moteur | tendance (30 derniers) | détails |",
           "|---|---:|---:|---:|---:|---:|---|---|---|"]
    last_ts = ""
    for name in lab_names:
        rows = read_metrics(data_dir / name / "metrics.jsonl")
        if not rows:
            out.append(f"| {name} | – | – | – | – | – | – | – | pas encore lancé |")
            continue
        r = rows[-1]
        last_ts = max(last_ts, r.get("ts", ""))
        det = ", ".join(f"{k}={v}" for k, v in r.get("extra", {}).items())
        out.append(f"| {name} | {r['cycle']} | {r['fitness']:.4g} | {r['best_fitness']:.4g} | "
                   f"{r['pop_final']} | {r['genome_len_mean']:.0f} | `{r['engine']}` | "
                   f"{spark([x['fitness'] for x in rows[-30:]])} | {det} |")
    out += ["", f"Dernière mise à jour : {last_ts}", "",
            "`moteur` = empreinte du code du package : quand elle change, l'amélioration "
            "de la VM/ISA (la « popote ») s'applique à tous les labos dès le cycle suivant."]
    return "\n".join(out) + "\n"


def write(data_dir: Path, lab_names: list[str]) -> Path:
    p = data_dir / "status.md"
    p.write_text(render(data_dir, lab_names))
    return p
