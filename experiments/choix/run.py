#!/usr/bin/env python3
"""Campagne préenregistrée ; aucune dépendance Python externe."""
import csv
import gzip
import hashlib
import json
import math
from pathlib import Path
import shutil
import statistics as st
import subprocess
import sys
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[2]
T31 = 2.039513


def sha(path):
    h = hashlib.sha256()
    with open(path, 'rb') as stream:
        for block in iter(lambda: stream.read(1048576), b''):
            h.update(block)
    return h.hexdigest()


def ci(values):
    assert len(values) == 32
    avg = st.mean(values)
    d = T31 * st.stdev(values) / math.sqrt(32)
    return {'mean': avg, 'low': avg-d, 'high': avg+d}


def analyse(directory, task):
    with open(directory / 'graines.tsv') as f:
        rows = list(csv.DictReader(f, delimiter='\t'))
    data = {}
    for row in rows:
        key = (int(row['split']), row['arm'])
        seed = int(row['seed'])
        arm = data.setdefault(key, {})
        assert seed not in arm
        arm[seed] = {k: float(row[k]) for k in
                     ('correct', 'net', 'consults', 'omissions', 'deaths', 'metabolism')}
    for arm in data.values():
        assert sorted(arm) == list(range(32))

    def vals(arm, field='correct', split=0):
        return [data[split, arm][s][field] for s in range(32)]

    def diff(a, b):
        return ci([x-y for x, y in zip(a, b)])

    stats = {f'{split}:{name}': {field: ci(vals(name, field, split))
             for field in ('correct', 'net', 'consults', 'omissions', 'deaths')}
             for split, name in data}
    if task == 1:
        oracle = ci(vals('oracle'))
        scramble = ci(vals('brouille'))
        device = {
            'oracle_accuracy': abs(oracle['mean']-.75) <= .04,
            'oracle_net': abs(ci(vals('oracle', 'net'))['mean']-14) <= .8,
            'blind_controls': all(abs(ci(vals(a))['mean']-.5) <= .04
                                  for a in ('A', 'B', 'alternance', 'aleatoire')),
            'scramble_compatible_half': scramble['low'] <= .5 <= scramble['high'],
            'scramble_ci_width': scramble['high']-scramble['low'] <= .1,
            'controls_live_and_decide': all(not any(vals(a, f)) for a in
                 ('T8', 'oracle', 'A', 'B', 'alternance', 'aleatoire')
                 for f in ('omissions', 'deaths')),
            'low_consultation': all(x == 256 for x in vals('T8', 'consults')),
            'high_informed_cost': abs(ci(vals('cout-haut-informe','net'))['mean']-7) <= .8,
            'rational_high_no_consultation': not any(vals('cout-haut-rationnel','consults')),
        }
        best_blind = [max(vals(a, 'net')[s] for a in
                     ('A', 'B', 'alternance', 'aleatoire')) for s in range(32)]
        gain = diff(vals('candidat', 'net'), best_blind)
        drop_s = diff(vals('candidat'), vals('candidat-brouille'))
        drop_a = diff(vals('candidat'), vals('candidat-ablation'))
        cand_s = ci(vals('candidat-brouille'))
        criteria = {
            'accuracy_ge_65': ci(vals('candidat'))['mean'] >= .65,
            'gain_ci_low_positive': gain['low'] > 0,
            'scramble_drop_ge_10': drop_s['mean'] >= .1 and drop_s['low'] > 0,
            'ablation_drop_ge_10': drop_a['mean'] >= .1 and drop_a['low'] > 0,
            'scramble_compatible_half': cand_s['low'] <= .5 <= cand_s['high'],
        }
        comparisons = {'gain_over_best_blind': gain, 'drop_scramble': drop_s, 'drop_ablation': drop_a}
    else:
        hold = ci(vals('candidat', split=1))
        ds = diff(vals('candidat', split=1), vals('candidat-brouille', split=1))
        dt = diff(vals('candidat', split=1), vals('candidat-brouille-second', split=1))
        device = {
            'T8_exact_seen_and_unseen': all(x == 1 for split in (0, 1)
                                          for x in vals('T8', split=split)),
            'lookup_unseen_half': abs(ci(vals('lookup',split=1))['mean']-.5) <= .04,
            'lookup_seen_exact': all(x == 1 for x in vals('lookup')),
        }
        criteria = {
            'unseen_ge_70': hold['mean'] >= .7,
            'unseen_ci_low_gt_50': hold['low'] > .5,
            'gap_le_10': abs(ci(vals('candidat'))['mean']-hold['mean']) <= .1,
            'drop_first_ge_10': ds['mean'] >= .1 and ds['low'] > 0,
            'drop_second_ge_10': dt['mean'] >= .1 and dt['low'] > 0,
        }
        comparisons = {'drop_first': ds, 'drop_second': dt}
    result = {'device_checks': device, 'device_valid': all(device.values()),
              'candidate_checks': criteria, 'candidate_pass': all(criteria.values()),
              'comparisons': comparisons, 'statistics': stats}
    (directory/'analyse.json').write_text(json.dumps(result, indent=2)+'\n')
    return result


def main():
    binary = Path(sys.argv[1]).resolve()
    out = Path(sys.argv[2]).resolve()
    candidate = ROOT/'seeds/general.adn'
    out.mkdir(parents=True, exist_ok=False)
    partition = out/'partition.tsv'
    with partition.open('w') as f:
        f.write('u\tv\tH\tsplit\n')
        for u in range(20):
            for v in range(20):
                f.write(f'{u}\t{v}\t{(u^v)&1}\t{int((u+v)%5==0)}\n')
    sources = [ROOT/p for p in ('experiments/choix.c', 'src/vm.c', 'src/isa.c',
               'src/rng.c', 'include/octet.h', 'experiments/choix/README.md',
               'experiments/choix/PLAN-VALIDE.md', 'experiments/choix/run.py')]
    manifest = {'created_at': datetime.now(timezone.utc).isoformat(),
                'commit': subprocess.check_output(['git', '-c', f'safe.directory={ROOT}',
                    '-C', str(ROOT), 'rev-parse', 'HEAD'],text=True).strip(),
                'candidate': str(candidate.relative_to(ROOT)),
                'seeds': list(range(32)), 'episodes': 256,
                'files': {str(p): sha(p) for p in sources+[binary,candidate,partition]},
                'claim_limit': 'mémoire utile et composition dans la tâche ; aucune émergence ni preuve de raisonnement'}
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    test = subprocess.check_output([str(binary),'selftest'],text=True)
    (out/'selftest.txt').write_text(test)
    results = {}
    for task in (1,2):
        if task == 2 and not results['M1']['device_valid']:
            break
        directory = out/f'M{task}'
        directory.mkdir()
        subprocess.run([str(binary),'run',f'M{task}',str(candidate),str(directory)],check=True)
        results[f'M{task}'] = analyse(directory, task)
        raw = directory/'episodes.tsv'
        with raw.open('rb') as src, gzip.open(str(raw)+'.gz','wb') as dest:
            shutil.copyfileobj(src,dest)
        raw.unlink()
    results['ecology_gate'] = ('eligible' if len(results)==2 and all(
        r['device_valid'] and r['candidate_pass'] for r in results.values()) else 'closed')
    results['ecology_launched'] = False
    (out/'resultats.json').write_text(json.dumps(results,indent=2)+'\n')
    hashes = {str(p.relative_to(out)): sha(p) for p in sorted(out.rglob('*')) if p.is_file()}
    (out/'SHA256.json').write_text(json.dumps(hashes,indent=2)+'\n')
    print(json.dumps({'path': str(out),'M1_valid': results['M1']['device_valid'],
          'M1_candidate': results['M1']['candidate_pass'],
          'M2_valid': results.get('M2',{}).get('device_valid'),
          'ecology_gate': results['ecology_gate']},ensure_ascii=False))


if __name__ == '__main__':
    main()
