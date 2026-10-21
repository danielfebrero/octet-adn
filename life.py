#!/usr/bin/env python3
"""Octet-ADN — STORE branché (auto-réécriture) + monde Linux."""
from __future__ import annotations
import argparse, random
from collections import Counter
from pathlib import Path

# 00 NOP  01 INC_A  02 DEC_A  03 INC_B  04 DEC_B  05 CLR_A
# 06 STORE imm8   → genome[imm] = A     ★ auto-modif
# 07 LOAD  imm8   → A = genome[imm]
# 08 JMP +imm     09 JZ A,+imm
# 0A SENSE  0B TURN  0C STEP  0D EAT  0E SPLIT  0F YIELD
OPS = {
    0x00: "NOP", 0x01: "INC_A", 0x02: "DEC_A", 0x03: "INC_B", 0x04: "DEC_B",
    0x05: "CLR_A", 0x06: "STORE", 0x07: "LOAD", 0x08: "JMP", 0x09: "JZ",
    0x0A: "SENSE", 0x0B: "TURN", 0x0C: "STEP", 0x0D: "EAT", 0x0E: "SPLIT",
    0x0F: "YIELD",
}
DIRS = [(0, -1), (1, 0), (0, 1), (-1, 0)]
W, H = 64, 24
MUT = 0.035


class C:
    __slots__ = ("x", "y", "d", "a", "b", "ip", "e", "dna", "age", "id",
                 "stores", "birth_dna")

    def __init__(self, x, y, dna, e=120, cid=0):
        self.x, self.y, self.d = x, y, 0
        self.a = self.b = self.ip = 0
        self.e, self.dna = e, bytearray(dna)
        self.age, self.id = 0, cid
        self.stores = 0
        self.birth_dna = bytes(dna)


class World:
    def __init__(self, seed, rng):
        self.rng = rng
        self.food = [
            [rng.randint(4, 14) if rng.random() < 0.35 else 0 for _ in range(W)]
            for _ in range(H)
        ]
        self.nid = 1
        self.cs = [C(W // 2, H // 2, seed, e=160, cid=0)]
        self.t = self.dead = 0
        self.born = 1
        self.store_events = 0

    def step(self):
        self.t += 1
        for _ in range(3):
            x, y = self.rng.randrange(W), self.rng.randrange(H)
            self.food[y][x] = min(14, self.food[y][x] + self.rng.randint(1, 4))
        nb, live = [], []
        for c in self.cs:
            if self._run(c, nb):
                live.append(c)
            else:
                self.dead += 1
                self.food[c.y][c.x] = min(14, self.food[c.y][c.x] + 4)
        self.cs = live + nb
        self.born += len(nb)

    def _imm(self, c):
        L = len(c.dna)
        v = c.dna[c.ip % L]
        c.ip = (c.ip + 1) % L
        return v

    def _run(self, c: C, nb: list) -> bool:
        if not c.dna or c.e <= 0:
            return False
        c.age += 1
        for _ in range(16):
            if c.e <= 0:
                break
            L = len(c.dna)
            op = c.dna[c.ip % L]
            c.ip = (c.ip + 1) % L
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
            elif op == 0x06:  # STORE imm: genome[imm] = A
                idx = self._imm(c) % L
                if c.dna[idx] != c.a:
                    c.dna[idx] = c.a
                    c.stores += 1
                    self.store_events += 1
            elif op == 0x07:  # LOAD imm
                idx = self._imm(c) % L
                c.a = c.dna[idx]
            elif op == 0x08:
                c.ip = (c.ip + self._imm(c)) % L
            elif op == 0x09:
                imm = self._imm(c)
                if c.a == 0:
                    c.ip = (c.ip + imm) % L
            elif op == 0x0A:
                dx, dy = DIRS[c.d % 4]
                c.a = self.food[(c.y + dy) % H][(c.x + dx) % W]
            elif op == 0x0B:
                c.d = c.a % 4
            elif op == 0x0C:
                dx, dy = DIRS[c.d % 4]
                c.x, c.y = (c.x + dx) % W, (c.y + dy) % H
                c.e -= 1
            elif op == 0x0D:
                take = min(self.food[c.y][c.x], 6)
                if take:
                    self.food[c.y][c.x] -= take
                    c.e = min(220, c.e + take * 5)
            elif op == 0x0E:
                if c.e >= 70 and len(self.cs) + len(nb) < 150:
                    child = mutate(c.dna, self.rng)
                    dx, dy = DIRS[(c.d + 1) % 4]
                    give = c.e // 3
                    nb.append(C((c.x + dx) % W, (c.y + dy) % H, child, e=give, cid=self.nid))
                    self.nid += 1
                    c.e -= give + 8
            elif op == 0x0F:
                break
        c.e -= 1
        return c.e > 0

    def render(self) -> str:
        cells = [
            [" " if self.food[y][x] == 0 else ("·" if self.food[y][x] < 5 else "*")
             for x in range(W)]
            for y in range(H)
        ]
        g = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ@"
        for c in self.cs:
            cells[c.y][c.x] = g[c.id % len(g)]
        h = (f"tick={self.t} pop={len(self.cs)} born={self.born} "
             f"dead={self.dead} STORE_writes={self.store_events}")
        if self.cs:
            b = max(self.cs, key=lambda c: c.stores)
            diff = sum(x != y for x, y in zip(b.dna, b.birth_dna)) + abs(len(b.dna) - len(b.birth_dna))
            h += f"  rewriter#{b.id} stores={b.stores} Δbirth={diff}"
        return h + "\n" + "\n".join(map("".join, cells))


def mutate(dna, rng):
    out = bytearray(dna)
    i = 0
    while i < len(out):
        if rng.random() < MUT:
            k = rng.randrange(5)
            if k <= 1:
                out[i] = rng.choice(list(OPS))
            elif k == 2:
                out[i] = rng.randrange(256)
            elif k == 3 and len(out) < 48:
                out.insert(i, rng.choice(list(OPS)))
            elif k == 4 and len(out) > 14:
                del out[i]
                continue
        i += 1
    return bytes(out)


def decode(dna: bytes) -> str:
    o, i = [], 0
    while i < len(dna):
        op = dna[i]
        n = OPS.get(op, f"INTRON:{op:02x}")
        if op in (0x06, 0x07, 0x08, 0x09) and i + 1 < len(dna):
            o.append(f"{op:02x} {dna[i+1]:02x}  {n} [{dna[i+1]}]")
            i += 2
        else:
            o.append(f"{op:02x}     {n}")
            i += 1
    return "\n".join(o)


def invent_seed_store() -> bytes:
    """Auto-régulation: STORE écrit SPLIT ou NOP dans le slot LEV selon SENSE."""
    # Layout final (indices absolus):
    #  0 LIT_SPLIT = 0e
    #  1 LIT_NOP   = 00
    #  2 LEV       = 00   ← slot vivant réécrit
    #  3 EAT
    #  4 SENSE
    #  5-6 JZ → famine
    #  7 STEP
    #  8-9 LOAD [0]      A=0e
    # 10-11 STORE [2]    LEV=SPLIT
    # 12-13 JMP → join
    # famine:
    # 14 INC_A; 15 TURN; 16 STEP
    # 17-18 LOAD [1]     A=00
    # 19-20 STORE [2]    LEV=NOP
    # join:
    # 21 EAT
    # 22 — IP tombera sur LEV seulement si on JMP vers 2... 
    #     Plus simple: après join, exécuter via LOAD+... 
    #     En fait LEV est à l'index 2, au début. On JMP vers 2 pour l'exécuter?
    #     Non — ça relancerait LIT_SPLIT comme opcode.
    #
    # Nouveau layout — LEV au milieu du flux:
    #  0 EAT
    #  1 SENSE
    #  2-3 JZ famine
    #  4 STEP
    #  5-6 LOAD [LIT_SPLIT]
    #  7-8 STORE [LEV]
    #  9-10 JMP join
    #  famine: 11 INC_A 12 TURN 13 STEP
    #  14-15 LOAD [LIT_NOP]
    #  16-17 STORE [LEV]
    #  join: 18 EAT
    #  19 LEV   ← opcode exécuté (0x00 ou 0x0e)
    #  20 YIELD
    #  21-22 JMP +imm → 0
    #  23 LIT_SPLIT = 0e
    #  24 LIT_NOP = 00

    lit_split = 23
    lit_nop = 24
    lev = 19
    g = bytearray(25)
    g[23], g[24] = 0x0E, 0x00
    g[19] = 0x00  # LEV initial = NOP (prudent)
    g[0] = 0x0D
    g[1] = 0x0A
    g[2], g[3] = 0x09, 0x08  # JZ +8 → index 11 (famine): from after imm (4), +8 = 12? 
    # After JZ at 2, ip becomes 4 before add. if A==0: ip = 4+imm.
    # Want ip=11 → imm=7
    g[3] = 7
    g[4] = 0x0C  # STEP
    g[5], g[6] = 0x07, lit_split  # LOAD
    g[7], g[8] = 0x06, lev       # STORE
    # JMP join (18): after JMP at 9, ip=11, want 11+imm=18 → imm=7
    g[9], g[10] = 0x08, 7
    # famine at 11
    g[11], g[12], g[13] = 0x01, 0x0B, 0x0C
    g[14], g[15] = 0x07, lit_nop
    g[16], g[17] = 0x06, lev
    # join 18
    g[18] = 0x0D  # EAT
    # 19 LEV already
    g[20] = 0x0F  # YIELD
    # JMP to 0: at 21, after imm ip=23, want (23+imm)%25=0 → imm=2
    g[21], g[22] = 0x08, 2
    return bytes(g)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ticks", type=int, default=700)
    ap.add_argument("--out", type=Path, default=Path("/workspace/octet-adn"))
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    dna = invent_seed_store()
    (args.out / "seed.adn").write_bytes(dna)
    print("=== OCTET-ADN (STORE branché) ===")
    print(f"seed ({len(dna)} o): {dna.hex(' ')}")
    print("--- décodage ---\n" + decode(dna))

    best = None
    for s in range(20):
        w = World(dna, random.Random(s))
        for _ in range(args.ticks):
            w.step()
            if not w.cs:
                break
        if best is None or (len(w.cs), w.store_events, w.t) > (len(best.cs), best.store_events, best.t):
            best = w
            best_s = s
        if len(w.cs) >= 10 and w.store_events > 50 and w.t == args.ticks:
            break

    w, s = best, best_s
    print(f"\n--- monde rng={s} ---")
    print(w.render())
    rewriters = [c for c in w.cs if c.stores > 0]
    print(f"\ncréatures ayant STORE-écrit: {len(rewriters)}/{len(w.cs)}  events={w.store_events}")
    if w.cs:
        champ = max(w.cs, key=lambda c: c.stores)
        print(f"champion rewrite #{champ.id}: {champ.stores} écritures")
        print(f"  naissance: {champ.birth_dna.hex(' ')}")
        print(f"  maintenant: {bytes(champ.dna).hex(' ')}")
        print(f"  LEV (idx 19): birth={champ.birth_dna[19]:02x} now={champ.dna[19]:02x}")
        print("--- decode actuel ---\n" + decode(bytes(champ.dna)))
        (args.out / "best.adn").write_bytes(bytes(champ.dna))
        dom, n = Counter(bytes(c.dna) for c in w.cs).most_common(1)[0]
        print(f"lignée dominante {n}/{len(w.cs)}: {dom.hex(' ')}")
    print(f"born={w.born} dead={w.dead} pop={len(w.cs)}")


if __name__ == "__main__":
    main()
