import statistics
from .base import Lab
from ..world import WorldConfig
from ..genome import forager


class Survival(Lab):
    name = "survival"
    title = "Longévité"
    fitness_doc = "espérance de vie moyenne (morts + vivants) × fraction du cycle survécue (monde pauvre)"

    def config(self):
        return WorldConfig(food_density=0.2, food_regrow=2)

    def seeds(self):
        return [(forager(), 0)]

    def rank_key(self, c):
        return (c.age, c.e)

    def fitness(self, w, ticks):
        alive_frac = min(1.0, len(self.pops) / max(1, ticks))
        med = statistics.median(c.age for c in w.cs) if w.cs else 0
        dead = w.stats["dead"]
        self.extra.update(median_age=med,
                          mean_death_age=round(w.stats["death_age_sum"] / dead, 1) if dead else 0)
        lifespan = (w.stats["death_age_sum"] + sum(c.age for c in w.cs)) / max(1, w.stats["born"])
        self.extra["mean_lifespan"] = round(lifespan, 1)
        return lifespan * alive_frac
