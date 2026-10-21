"""Jeu d'instructions Octet-ADN. 1 octet = 1 codon ; certains prennent un imm8.

00..0F : ISA historique (compatibilité conservée avec life.py).
10..1F : extension (capteurs, neurone, émission, prédation, arithmétique).
Tout octet non listé est un intron : lu sans effet et sans coût.
"""

OPS = {
    # --- ISA historique ---
    0x00: "NOP", 0x01: "INC_A", 0x02: "DEC_A", 0x03: "INC_B", 0x04: "DEC_B",
    0x05: "CLR_A", 0x06: "STORE", 0x07: "LOAD", 0x08: "JMP", 0x09: "JZ",
    0x0A: "SENSE", 0x0B: "TURN", 0x0C: "STEP", 0x0D: "EAT", 0x0E: "SPLIT",
    0x0F: "YIELD",
    # --- extension ---
    0x10: "SENSE_NB",    # A = nb de voisins (8-voisinage)
    0x11: "SENSE_EN",    # A = énergie propre // 8
    0x12: "SENSE_STIM",  # A = stimulus externe courant (octet envoyé par le labo)
    0x13: "NEURAL",      # imm = adresse synapses : A = neurone(capteurs × poids)
    0x14: "EMIT",        # émet A comme symbole / réponse
    0x15: "MOV_BA",      # B = A
    0x16: "MOV_AB",      # A = B
    0x17: "ADD",         # A = A + B
    0x18: "SUB",         # A = A - B
    0x19: "XOR",         # A = A ^ B
    0x1A: "ATTACK",      # vole de l'énergie à un voisin d'une autre espèce
    0x1B: "JNZ",         # imm : saut relatif si A != 0
    0x1C: "LOADI",       # imm : A = imm
    0x1D: "SENSE_HIST",  # imm : A = historique du stimulus [t-1-imm]
    0x1E: "LOOP",        # ip = 0
    0x1F: "AND",         # imm : A = A & imm
}
NAMES = {v: k for k, v in OPS.items()}
IMM_OPS = {0x06, 0x07, 0x08, 0x09, 0x13, 0x1B, 0x1C, 0x1D, 0x1F}
INTRON_FREE = True          # introns / NOP ne consomment pas le budget d'ops
OPS_PER_TICK = 16
MAX_LEN = 1024
MIN_LEN = 8
N_SYN = 5                   # 4 poids (food, voisins, énergie, stimulus) + biais


def is_op(b: int) -> bool:
    return b in OPS


def encode(asm: list) -> bytes:
    """Encode une liste [("NOM", imm?) ou int brut] en octets (sans labels)."""
    out = bytearray()
    for item in asm:
        if isinstance(item, int):
            out.append(item & 255)
            continue
        name, *args = item if isinstance(item, tuple) else (item,)
        op = NAMES[name]
        out.append(op)
        if op in IMM_OPS:
            out.append((args[0] if args else 0) & 255)
    return bytes(out)


def decode(dna: bytes, limit: int | None = None) -> list[tuple[int, str, int | None]]:
    """Retourne [(offset, nom, imm|None)] ; les introns sont « INTRON:xx »."""
    res, i, n = [], 0, len(dna) if limit is None else min(limit, len(dna))
    while i < n:
        op = dna[i]
        name = OPS.get(op, f"INTRON:{op:02x}")
        if op in IMM_OPS and i + 1 < len(dna):
            res.append((i, name, dna[i + 1]))
            i += 2
        else:
            res.append((i, name, None))
            i += 1
    return res


def disasm(dna: bytes, limit: int | None = None, skip_introns: bool = True) -> str:
    lines = []
    for off, name, imm in decode(dna, limit):
        if skip_introns and (name.startswith("INTRON") or name == "NOP"):
            continue
        lines.append(f"{off:04x}  {name}" + ("" if imm is None else f" {imm}"))
    return "\n".join(lines)
