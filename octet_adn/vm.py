"""Machine virtuelle Octet-ADN. Déterministe : tout l'aléa passe par world.rng."""
from .isa import OPS_PER_TICK, N_SYN, OPS

DIRS = [(0, -1), (1, 0), (0, 1), (-1, 0)]


def s8(b: int) -> int:
    return b - 256 if b > 127 else b


class Creature:
    __slots__ = ("x", "y", "d", "a", "b", "ip", "e", "dna", "age", "id",
                 "stores", "useful_stores", "dirty", "birth_dna", "species",
                 "parent", "eaten", "steps", "emitted")

    def __init__(self, x, y, dna, e=120, cid=0, species=0, parent=-1):
        self.x, self.y, self.d = x, y, 0
        self.a = self.b = self.ip = 0
        self.e, self.dna = e, bytearray(dna)
        self.age, self.id, self.species, self.parent = 0, cid, species, parent
        self.stores = self.useful_stores = 0
        self.dirty = set()            # index réécrits par STORE, pas encore exécutés
        self.birth_dna = bytes(dna)
        self.eaten = self.steps = 0
        self.emitted = None


def neural(c: Creature, world, addr: int) -> int:
    """Un neurone : 4 entrées capteurs (0..15) × 4 poids int8 + biais → A (0..255)."""
    L = len(c.dna)
    x = (min(15, world.food_ahead(c)), min(15, 2 * world.neighbors(c)),
         min(15, c.e >> 4), world.stimulus & 15)
    w = [s8(c.dna[(addr + j) % L]) for j in range(N_SYN)]
    v = ((sum(wi * xi for wi, xi in zip(w, x)) + 8) >> 4) + w[4]   # arrondi
    return max(0, min(255, v))


def run(c: Creature, world) -> bool:
    """Exécute un tick de la créature. Retourne False si elle meurt."""
    if not c.dna or c.e <= 0:
        return False
    c.age += 1
    c.emitted = None
    budget, skips = OPS_PER_TICK, 0
    dna = c.dna
    while budget > 0 and c.e > 0:
        L = len(dna)
        i = c.ip % L
        op = dna[i]
        c.ip = (i + 1) % L
        if i in c.dirty:                 # on exécute un codon réécrit : STORE « utile »
            c.dirty.discard(i)
            c.useful_stores += 1
            world.stats["useful_stores"] += 1
        if op == 0x00 or op not in OPS:  # intron / NOP : gratuit, borné
            skips += 1
            if skips > L:
                break
            continue
        budget -= 1
        if op == 0x01:
            c.a = (c.a + 1) & 255
        elif op == 0x02:
            c.a = (c.a - 1) & 255
        elif op == 0x03:
            c.b = (c.b + 1) & 255
        elif op == 0x04:
            c.b = (c.b - 1) & 255
        elif op == 0x05:
            c.a = 0
        elif op == 0x06:                 # STORE imm : genome[imm] = A
            idx = _imm(c) % L
            if dna[idx] != c.a:
                dna[idx] = c.a
                c.stores += 1
                c.dirty.add(idx)
                world.stats["store_writes"] += 1
        elif op == 0x07:
            c.a = dna[_imm(c) % L]
        elif op == 0x08:
            imm = _imm(c)
            c.ip = (c.ip + imm) % L
        elif op == 0x09:
            imm = _imm(c)
            if c.a == 0:
                c.ip = (c.ip + imm) % L
        elif op == 0x0A:
            c.a = world.food_ahead(c)
        elif op == 0x0B:
            c.d = c.a % 4
        elif op == 0x0C:
            world.move(c)
        elif op == 0x0D:
            world.eat(c)
        elif op == 0x0E:
            world.split(c)
        elif op == 0x0F:
            break
        elif op == 0x10:
            c.a = world.neighbors(c)
        elif op == 0x11:
            c.a = min(255, c.e >> 3)
        elif op == 0x12:
            c.a = world.stimulus & 255
        elif op == 0x13:
            c.a = neural(c, world, _imm(c))
        elif op == 0x14:
            if c.emitted is None:        # une émission comptée par tick
                c.emitted = c.a
                world.emit(c, c.a)
        elif op == 0x15:
            c.b = c.a
        elif op == 0x16:
            c.a = c.b
        elif op == 0x17:
            c.a = (c.a + c.b) & 255
        elif op == 0x18:
            c.a = (c.a - c.b) & 255
        elif op == 0x19:
            c.a ^= c.b
        elif op == 0x1A:
            world.attack(c)
        elif op == 0x1B:
            imm = _imm(c)
            if c.a != 0:
                c.ip = (c.ip + imm) % L
        elif op == 0x1C:
            c.a = _imm(c)
        elif op == 0x1D:
            c.a = world.history_at(_imm(c))
        elif op == 0x1E:
            c.ip = 0
        elif op == 0x1F:
            c.a &= _imm(c)
    c.e -= 1                             # métabolisme de base
    return c.e > 0


def _imm(c: Creature) -> int:
    L = len(c.dna)
    v = c.dna[c.ip % L]
    c.ip = (c.ip + 1) % L
    return v
