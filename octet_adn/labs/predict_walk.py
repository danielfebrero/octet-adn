from .base import PredictLab
from ..genome import forager


class PredictWalk(PredictLab):
    name = "predict-walk"
    title = "Prédire une série AR(2) bruitée"
    fitness_doc = "précision sur le bin (0..7) du prochain point ; baseline = persistance"

    def seeds(self):
        # NEURAL : A = stimulus (poids 16 sur l'entrée stimulus) → persistance
        return [(forager([("NEURAL", "syn0"), "EMIT"], synapses=[[0, 0, 0, 16, 0]]), 0)]

    def next_value(self, w):
        st = self.extra.setdefault("_ar", [0.0, 0.0])
        x = 0.75 * st[0] - 0.2 * st[1] + self.series_rng.gauss(0, 1.0)
        st[1], st[0] = st[0], x
        return max(0, min(7, int(round(x + 3.5))))
