from .base import EmitLab
from ..genome import forager


class EmitEcho(EmitLab):
    name = "emit-echo"
    title = "Communiquer : écho d'un stimulus"
    fitness_doc = "EMIT == stimulus (0..15) envoyé par le labo ; score = corrects / créature-tick"

    def seeds(self):
        return [(forager(["SENSE_STIM", "EMIT"]), 0)]

    def pre_tick(self, w):
        if w.t % 4 == 1:                       # nouveau symbole tous les 4 ticks
            w.stimulus = w.rng.randrange(16)

    def expected(self, w):
        return w.stimulus
