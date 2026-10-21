from .base import Lab
from ..genome import forager


class SelfRewrite(Lab):
    name = "self-rewrite"
    title = "Auto-réécriture utile"
    fitness_doc = "STORE utiles (codon réécrit PUIS exécuté) par créature-tick"

    def seeds(self):
        return [(forager(), 0)]

    def rank_key(self, c):
        return (c.useful_stores, c.e)

    def fitness(self, w, ticks):
        self.extra.update(store_writes=w.stats["store_writes"],
                          useful_stores=w.stats["useful_stores"])
        return w.stats["useful_stores"] / self.pop_ticks()
