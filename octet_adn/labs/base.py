"""Socle commun des éprouvettes : cycle d'évolution, sélection du pool, métriques."""
from __future__ import annotations
import random
import statistics
from ..world import World, WorldConfig

POOL_SIZE = 16
ACCEPT_RATIO = 0.9          # tolère une petite régression (dérive neutre)


class Lab:
    name = "base"
    title = ""
    fitness_doc = ""

    def __init__(self):
        self.pops: list[int] = []
        self.extra: dict = {}

    # --- à surcharger ---
    def config(self) -> WorldConfig:
        return WorldConfig()

    def seeds(self) -> list[tuple[bytes, int]]:
        raise NotImplementedError

    def pre_tick(self, w: World):
        pass

    def post_tick(self, w: World):
        pass

    def fitness(self, w: World, ticks: int) -> float:
        raise NotImplementedError

    def rank_key(self, c):
        return (c.e, c.age)

    # --- cycle ---
    def run_cycle(self, pool, rng: random.Random, ticks: int):
        genomes = [bytes.fromhex(h) for h, _ in pool]
        species = [sp for _, sp in pool]
        while len(genomes) < POOL_SIZE:
            k = len(genomes) % len(pool)
            genomes.append(bytes.fromhex(pool[k][0]))
            species.append(pool[k][1])
        w = World(genomes, rng, self.config(), lab=self, species=species)
        survived = 0
        for _ in range(ticks):
            w.step()
            self.pops.append(len(w.cs))
            if not w.cs:
                break
            survived += 1
        fit = float(self.fitness(w, ticks))
        new_pool = self.select(w)
        return w, fit, new_pool, survived

    def select(self, w: World):
        if not w.cs:
            return []
        ranked = sorted(w.cs, key=self.rank_key, reverse=True)
        return [(bytes(c.dna).hex(), c.species) for c in ranked[:POOL_SIZE]]

    # --- utilitaires de métriques ---
    def pop_ticks(self) -> int:
        return max(1, sum(self.pops))

    def mean_pop(self) -> float:
        return statistics.fmean(self.pops) if self.pops else 0.0


class EmitLab(Lab):
    """Labo de communication : récompense en énergie les EMIT corrects."""
    reward, penalty = 6, 2

    def __init__(self):
        super().__init__()
        self.correct = self.total = 0

    def expected(self, w: World) -> int | None:
        raise NotImplementedError

    def match(self, v: int, target: int) -> bool:
        return v == target

    def post_tick(self, w: World):
        target = self.expected(w)
        if target is None:
            return
        for c, v in w.emissions:
            self.total += 1
            if self.match(v, target):
                self.correct += 1
                c.e = min(220, c.e + self.reward)
            else:
                c.e -= self.penalty

    def fitness(self, w, ticks):
        acc = self.correct / max(1, self.total)
        self.extra.update(correct=self.correct, emits=self.total, accuracy=round(acc, 4))
        return self.correct / self.pop_ticks()      # précision × couverture


class PredictLab(Lab):
    """Prédiction de série : EMIT au tick t = prédiction de la valeur en t+1."""
    reward, penalty = 4, 2

    def __init__(self):
        super().__init__()
        self.series_rng = None
        self.pending: list = []
        self.correct = self.total = 0
        self.base_correct = self.base_total = 0
        self.prev = None

    def next_value(self, w) -> int:
        raise NotImplementedError

    def to_stimulus(self, v: int) -> int:
        return v

    def score(self, pred: int, actual: int) -> bool:
        return pred == actual

    def baseline(self, prev_stim: int, actual: int) -> bool:
        return self.score(prev_stim, actual)

    def pre_tick(self, w):
        if self.series_rng is None:
            self.series_rng = random.Random(w.rng.random())
        v = self.next_value(w)
        for c, pred in self.pending:
            self.total += 1
            if self.score(pred, v):
                self.correct += 1
                c.e = min(220, c.e + self.reward)
            else:
                c.e -= self.penalty
        if self.prev is not None:
            self.base_total += 1
            self.base_correct += self.baseline(self.prev, v)
        w.stimulus = self.to_stimulus(v)
        self.prev = w.stimulus
        self.pending = []

    def post_tick(self, w):
        self.pending = list(w.emissions)

    def fitness(self, w, ticks):
        acc = self.correct / max(1, self.total)
        base = self.base_correct / max(1, self.base_total)
        self.extra.update(correct=self.correct, emits=self.total,
                          accuracy=round(acc, 4), baseline=round(base, 4))
        return acc if self.total >= 30 else 0.0
