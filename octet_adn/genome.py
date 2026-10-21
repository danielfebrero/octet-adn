"""Construction de génomes : petit assembleur à labels + layout code/synapses/data.

Layout standard (256 octets) :
  0x00..0x7F  code
  0x80..0xBF  zone synapses (blocs de 5 octets int8 : 4 poids + biais)
  0xC0..0xCF  littéraux (lus par LOAD)
  reste       introns (0xFF), matière première pour les mutations
"""
from .isa import NAMES, IMM_OPS, N_SYN

SYN_BASE = 0x80
LIT_BASE = 0xC0
INTRON = 0xFF


def assemble(prog: list, size: int = 256, synapses: list | None = None,
             literals: dict | None = None) -> bytes:
    """prog : liste de str ("EAT"), tuples ("JZ", "label") ou ("LABEL", "x").

    Les sauts JMP/JZ/JNZ sont relatifs vers l'avant (imm = cible - ip_après_imm).
    Les imm LOAD/STORE/NEURAL acceptent un label ou un entier absolu.
    """
    literals = literals or {}
    labels, pos = {}, 0
    for it in prog:                       # passe 1 : positions
        t = it if isinstance(it, tuple) else (it,)
        if t[0] == "LABEL":
            labels[t[1]] = pos
            continue
        pos += 2 if NAMES[t[0]] in IMM_OPS else 1
    if pos > SYN_BASE:
        raise ValueError(f"code trop long ({pos} > {SYN_BASE})")
    for name, (off, _) in literals.items():
        labels[name] = off
    for k in range(len(synapses or [])):
        labels[f"syn{k}"] = SYN_BASE + k * N_SYN
    g = bytearray([INTRON] * size)
    pos = 0
    for it in prog:                       # passe 2 : émission
        t = it if isinstance(it, tuple) else (it,)
        if t[0] == "LABEL":
            continue
        op = NAMES[t[0]]
        g[pos] = op
        pos += 1
        if op in IMM_OPS:
            arg = t[1] if len(t) > 1 else 0
            val = labels[arg] if isinstance(arg, str) else int(arg)
            if t[0] in ("JMP", "JZ", "JNZ"):
                val = val - (pos + 1)
                if not 0 <= val < 256:
                    raise ValueError(f"saut hors portée vers {arg}: {val}")
            g[pos] = val & 255
            pos += 1
    for k, syn in enumerate(synapses or []):
        for j, w in enumerate(syn):
            g[SYN_BASE + k * N_SYN + j] = int(w) & 255
    for name, (off, val) in literals.items():
        g[off] = val & 255
    return bytes(g)


# Bloc commun : butinage + slot vivant LEV réécrit par STORE (auto-régulation).
LITS = {"lit_split": (LIT_BASE, 0x0E), "lit_nop": (LIT_BASE + 1, 0x00)}


def forager(block: list | None = None, size: int = 256, synapses=None) -> bytes:
    prog = [
        "EAT", "SENSE", ("JZ", "famine"), "STEP",
        ("LOAD", "lit_split"), ("STORE", "lev"), ("JMP", "join"),
        ("LABEL", "famine"), "INC_A", "TURN", "STEP",
        ("LOAD", "lit_nop"), ("STORE", "lev"),
        ("LABEL", "join"), "EAT",
        *(block or []),
        ("LABEL", "lev"), "NOP",
        "YIELD", "LOOP",
    ]
    # LEV est référencé par STORE comme adresse absolue (littéral à sa position).
    lits = dict(LITS)
    lev = _label_pos(prog, "lev")
    lits["lev"] = (lev, 0x00)
    return assemble(prog, size, synapses, lits)


def _label_pos(prog, name):
    pos = 0
    for it in prog:
        t = it if isinstance(it, tuple) else (it,)
        if t[0] == "LABEL":
            if t[1] == name:
                return pos
            continue
        pos += 2 if NAMES[t[0]] in IMM_OPS else 1
    raise KeyError(name)


def predator(size: int = 256) -> bytes:
    prog = [
        "SENSE_NB", ("JZ", "roam"), "ATTACK", "ATTACK", "SPLIT", ("JMP", "tail"),
        ("LABEL", "roam"), "SENSE", "TURN", "STEP", "STEP", "EAT",
        ("LABEL", "tail"), "YIELD", "LOOP",
    ]
    return assemble(prog, size)
