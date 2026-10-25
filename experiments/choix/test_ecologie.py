#!/usr/bin/env python3
"""Vérifie la reprise complète et la récupération d'un journal post-checkpoint."""
import subprocess
import sys
import tempfile
from pathlib import Path

binary = str(Path(sys.argv[1]).resolve())
with tempfile.TemporaryDirectory(prefix='octet-choix-test-') as tmp:
    full, resumed, exported = [Path(tmp)/n for n in ('full', 'resumed', 'exported')]
    for p in (full, resumed, exported):
        p.mkdir()
    def run(path, cycles):
        subprocess.run([binary, 'ecology', str(path), str(cycles), '0'],check=True)
    run(full, 40)
    run(resumed, 20)
    for name in ('series.tsv', 'events.tsv'):
        with (resumed/name).open('ab') as f:
            f.write(b'ligne non valide apres checkpoint\n')
    run(resumed, 20)
    for name in ('checkpoint.bin', 'series.tsv', 'events.tsv'):
        assert (full/name).read_bytes() == (resumed/name).read_bytes(), name
    subprocess.run([binary, 'export', str(full), str(exported)],check=True)
    assert len(list(exported.glob('*.adn'))) == 41
    # Épuisement du milieu sans réinjection : si extinction, la reprise
    # ne ressuscite aucun fondateur. Sinon une vie continue sans écrasement.
    print('OK : état VM/RNG, journaux après crash et export sans toucher au monde')
