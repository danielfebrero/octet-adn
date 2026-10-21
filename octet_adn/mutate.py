"""Mutations : substitution, insertion, délétion, duplication de bloc, crossover."""
import random
from .isa import OPS, MAX_LEN, MIN_LEN

OPCODES = sorted(OPS)


def mutate(dna: bytes, rng: random.Random, rate: float = 0.02,
           dup_rate: float = 0.05, max_len: int = MAX_LEN) -> bytes:
    out = bytearray(dna)
    i = 0
    while i < len(out):
        if rng.random() < rate:
            k = rng.randrange(6)
            if k <= 1:
                out[i] = rng.choice(OPCODES)            # substitution par un codon
            elif k == 2:
                out[i] = rng.randrange(256)             # substitution brute (synapses)
            elif k == 3:
                out[i] = (out[i] + rng.choice((-3, -1, 1, 3))) & 255  # micro-dérive (poids)
            elif k == 4 and len(out) < max_len:
                out.insert(i, rng.choice(OPCODES))      # insertion
                i += 1
            elif k == 5 and len(out) > MIN_LEN:
                del out[i]                              # délétion
                continue
        i += 1
    if rng.random() < dup_rate and len(out) >= 8:
        out = duplicate_block(bytes(out), rng, max_len)
    return bytes(out)


def duplicate_block(dna: bytes, rng: random.Random, max_len: int = MAX_LEN) -> bytes:
    """Duplication de gène : copie un bloc et l'insère ailleurs (si la place le permet)."""
    n = len(dna)
    blen = rng.randint(2, max(2, min(16, n // 4)))
    if n + blen > max_len:
        return dna
    a = rng.randrange(0, n - blen + 1)
    dst = rng.randrange(0, n + 1)
    block = dna[a:a + blen]
    return dna[:dst] + block + dna[dst:]


def crossover(a: bytes, b: bytes, rng: random.Random) -> bytes:
    """Crossover à un point aligné (préserve les adresses absolues avant le point)."""
    n = min(len(a), len(b))
    if n < 2:
        return bytes(a)
    cut = rng.randrange(1, n)
    return bytes(a[:cut] + b[cut:])
