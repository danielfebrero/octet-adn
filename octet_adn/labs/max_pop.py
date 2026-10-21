from .base import Lab
from ..genome import forager


class MaxPop(Lab):
    name = "max-pop"
    title = "Population la plus haute"
    fitness_doc = "population moyenne sur le cycle (+10 % du pic)"

    def seeds(self):
        return [(forager(), 0)]

    def fitness(self, w, ticks):
        peak = max(self.pops) if self.pops else 0
        self.extra.update(peak=peak)
        return self.mean_pop() + 0.1 * peak
