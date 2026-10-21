"""Monde torique : nourriture, énergie, voisinage, stimulus externe, prédation."""
import hashlib
import random
from collections import defaultdict
from dataclasses import dataclass, field

from .vm import Creature, DIRS, run
from .mutate import mutate, crossover


@dataclass
class WorldConfig:
    w: int = 48
    h: int = 20
    food_density: float = 0.35
    food_regrow: int = 4          # cases réapprovisionnées par tick
    max_pop: int = 120
    split_energy: int = 70
    start_energy: int = 160
    mut_rate: float = 0.02
    dup_rate: float = 0.05
    crossover: bool = False
    pred_eat_factor: float = 0.2  # rendement de la nourriture pour l'espèce 1 (prédateur)
    attack_steal: int = 30
    extra: dict = field(default_factory=dict)


class World:
    def __init__(self, genomes, rng: random.Random, cfg: WorldConfig | None = None,
                 lab=None, species=None):
        self.cfg = cfg or WorldConfig()
        self.rng, self.lab = rng, lab
        W, H = self.cfg.w, self.cfg.h
        self.food = [[rng.randint(4, 14) if rng.random() < self.cfg.food_density else 0
                      for _ in range(W)] for _ in range(H)]
        self.t, self.nid = 0, 0
        self.stimulus, self.history = 0, []
        self.emissions = []
        self.stats = defaultdict(int)
        self.cs, self.nb = [], []
        self.occ = defaultdict(int)
        species = species or [0] * len(genomes)
        for g, sp in zip(genomes, species):
            x, y = rng.randrange(W), rng.randrange(H)
            self._add(Creature(x, y, g, e=self.cfg.start_energy, cid=self._id(), species=sp))
        self.cs, self.nb = self.nb, []
        self.stats["born"] = len(self.cs)

    # --- helpers ---
    def _id(self):
        self.nid += 1
        return self.nid - 1

    def _add(self, c):
        self.nb.append(c)
        self.occ[(c.x, c.y)] += 1

    def ahead(self, c):
        dx, dy = DIRS[c.d % 4]
        return (c.x + dx) % self.cfg.w, (c.y + dy) % self.cfg.h

    # --- capteurs / actions (appelés par la VM) ---
    def food_ahead(self, c):
        x, y = self.ahead(c)
        return self.food[y][x]

    def neighbors(self, c):
        W, H, n = self.cfg.w, self.cfg.h, 0
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                if dx or dy:
                    n += self.occ.get(((c.x + dx) % W, (c.y + dy) % H), 0)
        return n

    def history_at(self, k):
        return self.history[-1 - k] if k < len(self.history) else 0

    def move(self, c):
        self.occ[(c.x, c.y)] -= 1
        c.x, c.y = self.ahead(c)
        self.occ[(c.x, c.y)] += 1
        c.e -= 1
        c.steps += 1

    def eat(self, c):
        take = min(self.food[c.y][c.x], 6)
        if take:
            self.food[c.y][c.x] -= take
            gain = take * 5 if c.species == 0 else int(take * 5 * self.cfg.pred_eat_factor)
            c.e = min(220, c.e + gain)
            c.eaten += gain
            self.stats["energy_eaten"] += gain

    def split(self, c):
        if c.e < self.cfg.split_energy or len(self.cs) + len(self.nb) >= self.cfg.max_pop:
            return
        dna = bytes(c.dna)
        if self.cfg.crossover:
            mate = self._mate(c)
            if mate is not None:
                dna = crossover(dna, bytes(mate.dna), self.rng)
                self.stats["crossovers"] += 1
        child = mutate(dna, self.rng, self.cfg.mut_rate, self.cfg.dup_rate)
        dx, dy = DIRS[(c.d + 1) % 4]
        give = c.e // 3
        self._add(Creature((c.x + dx) % self.cfg.w, (c.y + dy) % self.cfg.h, child,
                           e=give, cid=self._id(), species=c.species, parent=c.id))
        c.e -= give + 8
        self.stats["born"] += 1

    def _mate(self, c):
        for o in self.cs:
            if o is not c and o.species == c.species and o.e > 0 \
                    and abs(o.x - c.x) <= 1 and abs(o.y - c.y) <= 1:
                return o
        return None

    def attack(self, c):
        c.e -= 1
        for o in self.cs:
            if o.species != c.species and o.e > 0 and abs(o.x - c.x) <= 1 and abs(o.y - c.y) <= 1:
                s = min(o.e, self.cfg.attack_steal)
                o.e -= s
                c.e = min(220, c.e + s)
                self.stats["attacks"] += 1
                return

    def emit(self, c, v):
        self.emissions.append((c, v))
        self.stats["emits"] += 1

    # --- boucle ---
    def step(self):
        self.t += 1
        cfg, rng = self.cfg, self.rng
        for _ in range(cfg.food_regrow):
            x, y = rng.randrange(cfg.w), rng.randrange(cfg.h)
            self.food[y][x] = min(14, self.food[y][x] + rng.randint(1, 4))
        if self.lab:
            self.lab.pre_tick(self)
        self.emissions = []
        live = []
        for c in self.cs:
            if run(c, self) and c.e > 0:
                live.append(c)
            else:
                self.stats["dead"] += 1
                self.stats["death_age_sum"] += c.age
                self.occ[(c.x, c.y)] -= 1
                self.food[c.y][c.x] = min(14, self.food[c.y][c.x] + 4)
        if self.lab:
            self.lab.post_tick(self)
        alive = []
        for c in live:                   # tués après leur tour (ATTACK, pénalités)
            if c.e > 0:
                alive.append(c)
            else:
                self.stats["dead"] += 1
                self.stats["death_age_sum"] += c.age
                self.occ[(c.x, c.y)] -= 1
        self.cs = alive + [c for c in self.nb if c.e > 0]
        self.nb = []
        self.history.append(self.stimulus)
        if len(self.history) > 256:
            self.history = self.history[-256:]

    def state_hash(self) -> str:
        h = hashlib.sha1()
        for c in self.cs:
            h.update(bytes([c.x & 255, c.y & 255, c.d & 3, c.a, c.b]))
            h.update(c.e.to_bytes(4, "little", signed=True))
            h.update(bytes(c.dna))
        for row in self.food:
            h.update(bytes(row))
        return h.hexdigest()[:16]
