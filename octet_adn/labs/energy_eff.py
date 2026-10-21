from .base import Lab
from ..genome import forager


class EnergyEff(Lab):
    name = "energy-eff"
    title = "Efficacité métabolique"
    fitness_doc = "énergie mangée / créature-tick (>1 = surplus métabolique)"

    def seeds(self):
        return [(forager(), 0)]

    def rank_key(self, c):
        return (c.eaten / max(1, c.age), c.e)

    def fitness(self, w, ticks):
        self.extra.update(energy_eaten=w.stats["energy_eaten"])
        return w.stats["energy_eaten"] / self.pop_ticks()
