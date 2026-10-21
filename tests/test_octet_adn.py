"""Tests Octet-ADN — lancer : python3 -m unittest -v  (ou pytest)."""
import random
import tempfile
import unittest
from pathlib import Path

from octet_adn import isa, runner
from octet_adn.genome import assemble, forager
from octet_adn.labs import LABS
from octet_adn.mutate import mutate, crossover, duplicate_block
from octet_adn.vm import Creature, run
from octet_adn.world import World, WorldConfig


class TestISA(unittest.TestCase):
    def test_legacy_opcodes_preserved(self):
        legacy = ["NOP", "INC_A", "DEC_A", "INC_B", "DEC_B", "CLR_A", "STORE", "LOAD",
                  "JMP", "JZ", "SENSE", "TURN", "STEP", "EAT", "SPLIT", "YIELD"]
        self.assertEqual([isa.OPS[i] for i in range(16)], legacy)

    def test_encode_decode_roundtrip(self):
        asm = ["EAT", ("STORE", 19), ("NEURAL", 0x80), "EMIT", ("LOADI", 0xAC), "LOOP"]
        raw = isa.encode(asm)
        self.assertEqual(raw.hex(), "0d0613138014" "1cac1e")
        dec = [(n, i) for _, n, i in isa.decode(raw)]
        self.assertEqual(dec, [("EAT", None), ("STORE", 19), ("NEURAL", 128),
                               ("EMIT", None), ("LOADI", 172), ("LOOP", None)])

    def test_introns_decoded(self):
        self.assertEqual(isa.decode(b"\xff")[0][1], "INTRON:ff")

    def test_assembler_labels_and_size(self):
        g = assemble(["SENSE", ("JZ", "x"), "STEP", ("LABEL", "x"), "EAT"], size=300)
        self.assertEqual(len(g), 300)
        self.assertEqual(g[:5], bytes([0x0A, 0x09, 0x01, 0x0C, 0x0D]))  # JZ +1 → EAT


class TestVM(unittest.TestCase):
    def _world(self, g, seed=7, n=6):
        return World([g] * n, random.Random(seed), WorldConfig())

    def test_tick_deterministic(self):
        g = forager()
        a, b = self._world(g), self._world(g)
        for _ in range(60):
            a.step()
            b.step()
        self.assertEqual(a.state_hash(), b.state_hash())
        c = self._world(g, seed=8)
        for _ in range(60):
            c.step()
        self.assertNotEqual(a.state_hash(), c.state_hash())

    def test_store_rewrites_byte_and_counts_useful(self):
        # A=0x0E ; STORE en 6 (slot NOP) ; le slot est ensuite exécuté → SPLIT
        g = bytearray(isa.encode([("LOADI", 0x0E), ("STORE", 6), ("JMP", 0), "NOP", "YIELD"]))
        g += b"\xff" * 8
        w = World([], random.Random(0), WorldConfig())
        c = Creature(5, 5, bytes(g), e=200)
        w.cs = [c]
        self.assertEqual(c.dna[6], 0x00)
        run(c, w)
        self.assertEqual(c.dna[6], 0x0E)
        self.assertEqual(c.stores, 1)
        self.assertEqual(c.useful_stores, 1)
        self.assertEqual(len(w.nb), 1)            # le SPLIT réécrit a bien été exécuté

    def test_neural_identity_on_stimulus(self):
        g = forager([("NEURAL", "syn0"), "EMIT"], synapses=[[0, 0, 0, 16, 0]])
        w = World([g], random.Random(0), WorldConfig())
        w.stimulus = 9
        run(w.cs[0], w)
        self.assertEqual(w.emissions[0][1], 9)


class TestMutate(unittest.TestCase):
    def test_mutation_bounds_and_determinism(self):
        g = forager()
        m1 = mutate(g, random.Random(3), rate=0.2, dup_rate=1.0)
        m2 = mutate(g, random.Random(3), rate=0.2, dup_rate=1.0)
        self.assertEqual(m1, m2)
        self.assertTrue(isa.MIN_LEN <= len(m1) <= isa.MAX_LEN)
        self.assertNotEqual(m1, g)

    def test_duplication_and_crossover(self):
        g = bytes(range(32))
        d = duplicate_block(g, random.Random(1))
        self.assertGreater(len(d), len(g))
        x = crossover(b"\x01" * 16, b"\x02" * 16, random.Random(2))
        self.assertIn(1, x)
        self.assertIn(2, x)


class TestLabs(unittest.TestCase):
    def test_ten_labs_registered(self):
        self.assertEqual(len(LABS), 10)

    def test_emit_echo_small_sim(self):
        lab = LABS["emit-echo"]()
        pool = [(g.hex(), sp) for g, sp in lab.seeds()]
        _, fit, new_pool, _ = lab.run_cycle(pool, random.Random(1), 60)
        self.assertGreater(lab.total, 0)
        self.assertGreater(lab.correct / lab.total, 0.9)   # la seed fait l'écho
        self.assertGreater(fit, 0.5)
        self.assertTrue(new_pool)

    def test_runner_dry_run_two_labs(self):
        rows = runner.main(["--labs", "emit-echo,max-pop", "--ticks-per-cycle", "50", "--dry-run"])
        self.assertEqual([r["lab"] for r in rows], ["emit-echo", "max-pop"])
        self.assertTrue(all(r["ticks"] == 50 for r in rows))

    def test_runner_persists_checkpoints_and_status(self):
        with tempfile.TemporaryDirectory() as d:
            runner.main(["--labs", "survival,predict-stock", "--ticks-per-cycle", "50",
                         "--cycles", "2", "--data-dir", d])
            p = Path(d)
            self.assertTrue((p / "survival" / "checkpoints" / "latest.json").exists())
            self.assertEqual(len((p / "predict-stock" / "metrics.jsonl").read_text().splitlines()), 2)
            self.assertIn("predict-stock", (p / "status.md").read_text())


if __name__ == "__main__":
    unittest.main()
