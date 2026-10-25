#!/usr/bin/env python3
"""Paired, exhaustive Q1 evaluation, no third-party dependencies."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile


def run(binary, directory, ticks, *options):
    return subprocess.run([str(binary), 'run', str(directory), '--ticks', str(ticks),
                           *map(str, options)], check=True, capture_output=True, text=True)


def verify(binary):
    subprocess.run([str(binary), 'selftest'], check=True)
    with tempfile.TemporaryDirectory(prefix='q1-test-') as tmp:
        root = Path(tmp)
        run(binary, root/'whole', 256, '--seed', 731, '--period', 4)
        run(binary, root/'copy', 256, '--seed', 731, '--period', 4)
        run(binary, root/'split', 113, '--seed', 731, '--period', 4)
        # Simulate flushed log lines ahead of an older atomic checkpoint.
        with (root/'split'/'events.tsv').open('a') as f:
            f.write('999\tcrash\n1000\tpartial')
        with (root/'split'/'mesures.tsv').open('a') as f:
            f.write('999\tcrash\n')
        run(binary, root/'split', 143)
        for filename in ('state.ck', 'events.tsv'):
            expected = (root/'whole'/filename).read_bytes()
            assert expected == (root/'copy'/filename).read_bytes(), filename
            assert expected == (root/'split'/filename).read_bytes(), filename
        fail = subprocess.run([str(binary), 'run', str(root/'split'), '--ticks', '1',
                               '--seed', '99'], capture_output=True)
        assert fail.returncode != 0, 'reconfiguration must fail'
    print('determinisme, reprise exacte et refus de reconfiguration: OK', flush=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('binary', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    binary = args.binary.resolve()
    verify(binary)
    if args.output is None:
        return
    args.output.mkdir(parents=True, exist_ok=False)
    rows = []
    repo = Path(__file__).resolve().parents[2]
    raw = repo/'experiments/quantique-runs'/args.output.name
    if raw.exists():
        raise RuntimeError(f'directory already exists: {raw}')
    for modes, periods in ((256, (0, 4, 16, 64)), (128, (16,))):
        for seed in (101, 202, 303, 404):
            for period in periods:
                directory = raw/f'm{modes}-s{seed}-p{period}'
                run(binary, directory, 4096, '--seed', seed, '--period', period, '--modes', modes)
                with (directory/'mesures.tsv').open() as f:
                    last = list(csv.DictReader(f, delimiter='\t'))[-1]
                row = dict(seed=seed, period=period, modes=modes, **last)
                row['checkpoint_sha256'] = hashlib.sha256((directory/'state.ck').read_bytes()).hexdigest()
                rows.append(row)
                # Copy all bounded time series, not just a chosen summary.
                for filename in ('events.tsv', 'mesures.tsv'):
                    (args.output/f'{directory.name}-{filename}').write_bytes((directory/filename).read_bytes())
                print(json.dumps(row), flush=True)
    with (args.output/'resultats.tsv').open('w') as f:
        writer = csv.DictWriter(f, fieldnames=rows[0].keys(), delimiter='\t')
        writer.writeheader()
        writer.writerows(rows)
    manifest = {
        'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
        'source_sha256': hashlib.sha256((Path(__file__).parent/'monde.cpp').read_bytes()).hexdigest(),
        'protocol_sha256': hashlib.sha256((Path(__file__).parent/'PROTOCOLE.md').read_bytes()).hexdigest(),
        'commit': subprocess.check_output(['git', '-c', f'safe.directory={repo}', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip(),
        'runs': len(rows), 'ticks_per_run': 4096,
        'note': 'All paired runs included. Finite-resolution hybrid invented model.'}
    (args.output/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')


if __name__ == '__main__':
    main()
