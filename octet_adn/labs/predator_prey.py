from .base import Lab, POOL_SIZE
from ..genome import forager, predator


class PredatorPrey(Lab):
    name = "predator-prey"
    title = "Proies (espèce 0) et prédateurs (espèce 1, ATTACK)"
    fitness_doc = "fraction des ticks où les 2 espèces coexistent × équilibre min(proies/20, préd/5)"

    def __init__(self):
        super().__init__()
        self.prey, self.pred = [], []

    def seeds(self):
        return [(forager(), 0)] * 3 + [(predator(), 1)]

    def post_tick(self, w):
        p = sum(1 for c in w.cs if c.species == 0 and c.e > 0)
        self.prey.append(p)
        self.pred.append(len(w.cs) - p)

    def fitness(self, w, ticks):
        n = max(1, ticks)
        both = sum(1 for a, b in zip(self.prey, self.pred) if a > 0 and b > 0) / n
        mp = sum(self.prey) / n
        md = sum(self.pred) / n
        self.extra.update(mean_prey=round(mp, 1), mean_pred=round(md, 1), attacks=w.stats["attacks"])
        return both * min(1.0, mp / 20) * min(1.0, md / 5)

    def select(self, w):
        prey = sorted((c for c in w.cs if c.species == 0), key=self.rank_key, reverse=True)
        pred = sorted((c for c in w.cs if c.species == 1), key=self.rank_key, reverse=True)
        k = min(len(pred), POOL_SIZE // 4)
        pick = [(bytes(c.dna).hex(), c.species) for c in pred[:k] + prey[:POOL_SIZE - k]]
        # immigration : si les prédateurs s'éteignent, on réintroduit la seed prédatrice
        while sum(1 for _, sp in pick if sp == 1) < 2:
            pick.append((predator().hex(), 1))
        if not prey:
            pick += [(forager().hex(), 0)] * 4
        return pick
