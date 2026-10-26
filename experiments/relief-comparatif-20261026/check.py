#!/usr/bin/env python3
"""Bounded offline tests: no ecological ticks, no writes to the living world.

Run from any directory with:
  python3 experiments/relief-comparatif-20261026/check.py \
      --source experiments/relief-comparatif-20261026/data/window1/base

Large temporary checkpoints and TSV files are removed after verification. The
JSON result records the input/tool hashes and all independently checked facts.
"""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile


class Config(C.Structure):
    _fields_ = [(name, C.c_uint32) for name in
                ('width', 'height', 'capacity', 'founders', 'season', 'sample', 'checkpoint')]
    _fields_ += [('seed', C.c_uint64)]


class Cell(C.Structure):
    _fields_ = [(name, C.c_uint32) for name in
                ('mineral', 'food', 'detritus', 'structure', 'cache', 'owner')]
    _fields_ += [(name, C.c_uint16) for name in ('elevation', 'moisture', 'signal')]
    _fields_ += [(name, C.c_uint8) for name in ('rock', 'pad')]


class Agent(C.Structure):
    _fields_ = [(name, C.c_uint64) for name in ('id', 'parent', 'lineage', 'born')]
    _fields_ += [(name, C.c_uint32) for name in ('pos', 'age', 'energy', 'body', 'store')]
    _fields_ += [('genome', C.c_uint8 * 64), ('memory', C.c_uint8 * 8)]
    _fields_ += [(name, C.c_uint8) for name in ('pc', 'dir', 'alive', 'pad')]


class World(C.Structure):
    _fields_ = [('config', Config)]
    _fields_ += [(name, C.c_uint64) for name in (
        'tick', 'rng', 'next_id', 'initial_matter', 'births', 'deaths', 'moves',
        'eats', 'builds', 'breaks', 'gifts', 'attacks', 'parasites', 'mutations',
        'energy_spent', 'solar_food', 'sensor_witness', 'memory_witness', 'witness_samples')]
    _fields_ += [('population', C.c_uint32), ('pad', C.c_uint32)]
    _fields_ += [(name, C.c_void_p) for name in ('cells', 'agents', 'occupancy', 'events')]


class Header(C.Structure):
    _fields_ = [('magic', C.c_uint64)]
    _fields_ += [(name, C.c_uint32) for name in (
        'version', 'endian', 'cell_size', 'agent_size', 'world_size', 'header_size')]
    _fields_ += [('payload_size', C.c_uint64), ('checksum', C.c_uint64), ('state', World)]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(tool, *args, success=True):
    result = subprocess.run([str(tool), *map(str, args)], capture_output=True, text=True)
    assert (result.returncode == 0) == success, (
        args, result.returncode, result.stdout, result.stderr)
    return result


def export(tool, directory, prefix):
    run(tool, 'export', directory, prefix)
    result = json.loads(prefix.with_suffix('.json').read_text())
    # The JSON retains all aggregate evidence; large per-agent lists are
    # unnecessary for these focused transform/ABI tests.
    for suffix in ('.agents.tsv', '.lineages.tsv', '.owners.tsv'):
        prefix.with_suffix(suffix).unlink()
    return result


def independent_raw_check(before, after, mode):
    """Permit checksum/cache/food bytes only, then verify intended values.

    The structures above derive offsets independently from the transform code;
    the encoded ABI metadata must agree before any offsets are used.
    """
    header = Header.from_buffer_copy(before)
    assert (header.version, header.endian, header.cell_size, header.agent_size,
            header.world_size, header.header_size) == (
        1, 0x01020304, C.sizeof(Cell), C.sizeof(Agent), C.sizeof(World), C.sizeof(Header))
    assert len(before) == len(after) == C.sizeof(Header) + header.payload_size
    expected = bytearray(before)
    checksum = Header.checksum.offset
    expected[checksum:checksum + 8] = after[checksum:checksum + 8]
    n = header.state.config.width * header.state.config.height
    totals_before, totals_after = [0] * 256, [0] * 256
    changed_cache = changed_food = released = 0
    eligible = [[] for _ in range(256)]
    agents_start = C.sizeof(Header) + n * C.sizeof(Cell)
    occupied = {}
    for slot in range(header.state.config.capacity):
        a = Agent.from_buffer_copy(before, agents_start + slot * C.sizeof(Agent))
        if a.alive:
            occupied[a.pos] = a.id
    for pos in range(n):
        start = C.sizeof(Header) + pos * C.sizeof(Cell)
        food_at, cache_at = start + Cell.food.offset, start + Cell.cache.offset
        food = struct.unpack_from('=I', before, food_at)[0]
        cache = struct.unpack_from('=I', before, cache_at)[0]
        newfood = struct.unpack_from('=I', after, food_at)[0]
        newcache = struct.unpack_from('=I', after, cache_at)[0]
        totals_before[pos & 255] += cache
        totals_after[pos & 255] += newcache
        assert bool(cache) == bool(newcache), 'positive cache footprint changed'
        if mode == 'unlock':
            amount = cache // 4
            assert (newfood, newcache) == (food + amount, cache - amount)
            released += amount
        elif mode == 'redistribute':
            assert newfood == food
            if pos in occupied and cache:
                eligible[pos & 255].append((pos, cache, newcache))
            else:
                assert newcache == cache, 'unoccupied/zero cache changed'
        else:
            assert (newfood, newcache) == (food, cache)
        expected[food_at:food_at + 4] = after[food_at:food_at + 4]
        expected[cache_at:cache_at + 4] = after[cache_at:cache_at + 4]
        changed_cache += cache != newcache
        changed_food += food != newfood
    assert expected == after, 'forbidden raw checkpoint bytes changed'
    if mode == 'control':
        assert before == after, 'control is not exact'
    if mode == 'redistribute':
        assert totals_before == totals_after
        for cells in eligible:
            if not cells:
                continue
            quotient, remainder = divmod(sum(c[1] for c in cells), len(cells))
            assert all(new == quotient + (index < remainder)
                       for index, (_, _, new) in enumerate(cells))
    return {'full_forbidden_byte_comparison': 'PASS', 'positive_support': 'PASS',
            'independent_rule_check': 'PASS', 'changed_cache_fields': changed_cache,
            'changed_food_fields': changed_food, 'released_cache_to_food': released}


def main():
    experiment = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=experiment / 'data/window1/base')
    parser.add_argument('--tool', type=Path, default=experiment / 'build/state_tool')
    parser.add_argument('--result', type=Path, default=experiment / 'data/tests/check-results.json')
    args = parser.parse_args()
    source, tool = args.source.resolve(strict=True), args.tool.resolve(strict=True)
    tests = experiment / 'data/tests'
    tests.mkdir(parents=True, exist_ok=True)
    result_path = args.result.absolute()
    assert result_path.parent.resolve() == tests.resolve(), 'results must stay under data/tests'
    assert not result_path.exists(), 'refusing to overwrite results'
    before = (source / 'state.ck').read_bytes()
    source_hash = hashlib.sha256(before).hexdigest()
    results = {'schema': 'relief.offline.check.v1', 'source': str(source),
               'source_sha256': source_hash, 'tool_sha256': sha(tool),
               'source_tool_c_sha256': sha(experiment / 'state_tool.c'),
               'abi': {name: C.sizeof(value) for name, value in
                       [('Header', Header), ('World', World), ('Cell', Cell), ('Agent', Agent)]},
               'tests': [], 'ecological_ticks_executed': 0}
    with tempfile.TemporaryDirectory(prefix='check-', dir=tests) as temporary:
        temp = Path(temporary)
        base = export(tool, source, temp / 'base')
        assert base['matter'] == base['initial_matter']
        for mode in ('control', 'unlock', 'redistribute'):
            first, second = temp / f'{mode}-first', temp / f'{mode}-second'
            run(tool, 'transform', source, first, mode)
            run(tool, 'transform', source, second, mode)
            for file in ('state.ck', 'transformation.json', 'transformation.cells.tsv'):
                assert (first / file).read_bytes() == (second / file).read_bytes(), (mode, file)
            transformed = (first / 'state.ck').read_bytes()
            summary = export(tool, first, temp / mode)
            assert summary['matter'] == base['matter']
            assert summary['rng_hex'] == base['rng_hex']
            for field in ('config', 'counters', 'diversity', 'population', 'tick', 'next_id'):
                assert summary[field] == base[field], (mode, field)
            test = {'mode': mode, 'deterministic_files': 'PASS',
                    'export_conservation': 'PASS', 'rng_and_initial_population': 'PASS',
                    'sha256': hashlib.sha256(transformed).hexdigest(),
                    **independent_raw_check(before, transformed, mode)}
            assert json.loads((first / 'transformation.json').read_text())['strict_contract_passed']
            results['tests'].append(test)
            # Peak disk use is bounded by two transforms at once, rather than
            # retaining six additional 24 MiB checkpoints and their audits.
            shutil.rmtree(first)
            shutil.rmtree(second)
        existing = temp / 'existing'
        existing.mkdir()
        sentinel = existing / 'sentinel'
        sentinel.write_text('unchanged\n')
        run(tool, 'transform', source, existing, 'control', success=False)
        assert list(existing.iterdir()) == [sentinel] and sentinel.read_text() == 'unchanged\n'
        results['tests'].append({'existing_directory_refused_without_mutation': 'PASS'})
        live = experiment.parent / 'vie-relief-20261025'
        if not live.is_dir():
            raise AssertionError('known live directory is required for rejection test')
        live_link = temp / 'live-symlink'
        live_link.symlink_to(live, target_is_directory=True)
        for parent in (live, live_link):
            target = parent / f'offline-check-forbidden-{temp.name}'
            assert not target.exists()
            run(tool, 'transform', source, target, 'control', success=False)
            run(tool, 'export', source, target, success=False)
            assert not target.exists()
            assert not target.with_suffix('.agents.tsv').exists()
        results['tests'].append({'live_destination_and_symlink_refused_without_mutation': 'PASS'})
        corrupt = temp / 'corrupt'
        corrupt.mkdir()
        broken = bytearray(before)
        broken[Header.checksum.offset] ^= 1
        (corrupt / 'state.ck').write_bytes(broken)
        no_output = temp / 'corrupt-destination'
        failure = run(tool, 'transform', corrupt, no_output, 'control', success=False)
        assert 'checksum mismatch' in failure.stderr and not no_output.exists()
        prefix = temp / 'corrupt-export'
        failure = run(tool, 'export', corrupt, prefix, success=False)
        assert 'checksum mismatch' in failure.stderr
        assert not list(temp.glob('corrupt-export.*'))
        results['tests'].append({'corrupt_checksum_refused_without_output': 'PASS'})
    assert sha(source / 'state.ck') == source_hash, 'source checkpoint changed'
    results['source_unchanged'] = True
    results['all_passed'] = True
    with result_path.open('x') as output:
        json.dump(results, output, indent=2)
        output.write('\n')
    print(json.dumps({'all_passed': True, 'result': str(result_path),
                      'source_sha256': source_hash, 'tests': len(results['tests'])}))


if __name__ == '__main__':
    main()
