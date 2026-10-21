"""Proxy pédagogique : PAS un modèle de trading, PAS un conseil financier.

Série par défaut : mouvement brownien géométrique (rendements i.i.d.) → la
direction du prochain pas est imprévisible par construction ; ~50 % est le
plafond attendu. Un CSV local (colonne 'close') peut remplacer la série via
WorldConfig.extra['csv'] ; aucun appel réseau.
"""
import csv
import math
from pathlib import Path
from .base import PredictLab
from ..genome import forager


def load_closes(path) -> list[float]:
    with open(path, newline="") as f:
        return [float(r["close"]) for r in csv.DictReader(f)]


class PredictStock(PredictLab):
    name = "predict-stock"
    title = "Direction d'un prix (proxy GBM / CSV local)"
    fitness_doc = "précision directionnelle hausse/baisse du pas suivant (A&1) ; hasard = 50 %"
    reward, penalty = 3, 3                   # espérance nulle si on devine au hasard
    csv_path: str | None = None

    def seeds(self):
        return [(forager(["SENSE_STIM", ("AND", 1), "EMIT"]), 0)]   # momentum

    def score(self, pred, actual):
        return (pred & 1) == actual

    def baseline(self, prev_stim, actual):
        return (prev_stim & 1) == actual       # momentum naïf

    def next_value(self, w):
        st = self.extra
        if "_px" not in st:
            st["_px"] = 100.0
            st["_csv"] = load_closes(self.csv_path) if self.csv_path and Path(self.csv_path).exists() else None
            st["_i"] = 0
        if st["_csv"]:
            s = st["_csv"]
            i = st["_i"] % (len(s) - 1)
            st["_i"] += 1
            ret = math.log(s[i + 1] / s[i])
        else:
            ret = self.series_rng.gauss(0.0002, 0.01)
        st["_px"] *= math.exp(ret)
        return 1 if ret > 0 else 0

    def fitness(self, w, ticks):
        f = super().fitness(w, ticks)
        self.extra["source"] = "csv" if self.extra.get("_csv") else "gbm"
        return f
