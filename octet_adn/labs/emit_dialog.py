from .base import EmitLab
from ..genome import forager

# Protocole : on envoie un symbole s (1..14), la réponse attendue est s+1 ;
# 0x00 = « hello » attend l'ACK 0xAC ; 0x0F = « bye » attend 0x00 (silence actif).


def reply(s: int) -> int:
    if s == 0x00:
        return 0xAC
    if s == 0x0F:
        return 0x00
    return s + 1


class EmitDialog(EmitLab):
    name = "emit-dialog"
    title = "Dialogue : protocole stimulus → réponse"
    fitness_doc = "réponse correcte au protocole (hello→ACK 0xAC, s→s+1, bye→0) / créature-tick"

    def seeds(self):
        return [(forager(["SENSE_STIM", "INC_A", "EMIT"]), 0)]   # gère s→s+1 seulement

    def pre_tick(self, w):
        seq = w.cfg.extra.setdefault("seq", [])
        if not seq:
            seq.extend([0x00] + [w.rng.randrange(1, 15) for _ in range(5)] + [0x0F])
        if w.t % 3 == 1:
            w.stimulus = seq.pop(0)

    def expected(self, w):
        return reply(w.stimulus)
