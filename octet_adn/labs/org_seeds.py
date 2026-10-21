from .base import Lab
from ..world import WorldConfig
from ..genome import forager, predator


def hamming(a: bytes, b: bytes, n: int = 128) -> int:
    return sum(x != y for x, y in zip(a[:n], b[:n])) + abs(min(n, len(a)) - min(n, len(b)))


def clusters(genomes: list[bytes], thr: int = 6) -> int:
    reps: list[bytes] = []
    for g in genomes:
        if all(hamming(g, r) > thr for r in reps):
            reps.append(g)
    return len(reps)


class OrgSeeds(Lab):
    """Méta-labo : reçoit les champions des autres labos, les croise, les organise."""
    name = "org-seeds"
    title = "Organiser d'autres seeds"
    fitness_doc = "nb de lignées distinctes viables (clusters Hamming) × diversité (génomes uniques / pop)"
    foreign: list = []          # injecté par le runner : champions des autres labos

    def config(self):
        return WorldConfig(crossover=True, mut_rate=0.025)

    def seeds(self):
        base = [forager(), forager([("NEURAL", "syn0"), "EMIT"], synapses=[[0, 0, 0, 16, 0]]),
                forager(["SENSE_STIM", "EMIT"]), forager(["SENSE_NB", "TURN", "STEP"])]
        return [(g, 0) for g in base] + [(predator(), 0)]

    def run_cycle(self, pool, rng, ticks):
        pool = list(pool) + [(h, 0) for h in self.foreign[:8]]
        return super().run_cycle(pool, rng, ticks)

    def fitness(self, w, ticks):
        if not w.cs:
            return 0.0
        gs = [bytes(c.dna) for c in w.cs]
        k = clusters(gs)
        div = len(set(gs)) / len(gs)
        self.extra.update(clusters=k, unique=len(set(gs)), crossovers=w.stats["crossovers"])
        return k * div

    def select(self, w):
        # garde un représentant par cluster d'abord (préserve la diversité)
        reps, out = [], []
        for c in sorted(w.cs, key=self.rank_key, reverse=True):
            g = bytes(c.dna)
            if all(hamming(g, r) > 6 for r in reps):
                reps.append(g)
                out.append((g.hex(), 0))
        for c in sorted(w.cs, key=self.rank_key, reverse=True):
            if len(out) >= 16:
                break
            if (bytes(c.dna).hex(), 0) not in out:
                out.append((bytes(c.dna).hex(), 0))
        return out[:16]
