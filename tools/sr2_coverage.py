"""Spec coverage gate (S3.7): every indexed SR2 function must have exactly one `func` row in one
port/spec/*_symbols.csv.

usage: python tools/sr2_coverage.py
Reads port/sr2_functions.csv, port/sr2_modules.csv and port/spec/*_symbols.csv. Writes
work/coverage/uncovered.txt (address, size, guessed module, SR1 name) and
work/coverage/duplicates.txt; prints a per-module summary and exits 1 if anything is missing.
"""
import csv, glob, os, sys
from collections import Counter, defaultdict


def norm(a):
    a = a.strip().lower().replace('0x', '')
    if ':' not in a:
        return None
    s, o = a.split(':', 1)
    try:
        return '%04x:%04x' % (int(s, 16), int(o, 16))
    except ValueError:
        return None


funcs = {norm(r['start']): r for r in csv.DictReader(open('port/sr2_functions.csv'))}
module = {norm(r['sr2_start']): r['module'] for r in csv.DictReader(open('port/sr2_modules.csv'))}
sr1 = {norm(r['sr2_start']): r['sr1_name'] for r in csv.DictReader(open('port/sr2_sr1_matches.csv'))}
owner = defaultdict(list)
extra = []
for path in sorted(glob.glob('port/spec/*_symbols.csv')):
    spec = os.path.basename(path)[:-len('_symbols.csv')]
    for r in csv.DictReader(open(path, encoding='utf-8')):
        if r.get('kind') != 'func':
            continue
        a = norm(r['address'])
        if a in funcs:
            owner[a].append(spec)
        else:
            extra.append((spec, r['address'], r['name']))

os.makedirs('work/coverage', exist_ok=True)
missing = [a for a in funcs if not owner.get(a)]
dups = {a: s for a, s in owner.items() if len(s) > 1}
with open('work/coverage/uncovered.txt', 'w') as o:
    for a in sorted(missing):
        o.write('%s %6s %-12s %s\n' % (a, funcs[a]['size'], module.get(a, '?'), sr1.get(a, '')))
with open('work/coverage/duplicates.txt', 'w') as o:
    for a, s in sorted(dups.items()):
        o.write('%s %s\n' % (a, ' '.join(s)))
    for e in extra:
        o.write('not indexed: %s %s %s\n' % e)

tot, miss = Counter(), Counter()
for a in funcs:
    tot[module.get(a, '?')] += 1
for a in missing:
    miss[module.get(a, '?')] += 1
for m in sorted(tot):
    print('%-12s %4d functions, %4d uncovered' % (m, tot[m], miss[m]))
print('total %d, covered %d (%.1f %%), uncovered %d, in several specs %d, spec rows not in the index %d'
      % (len(funcs), len(funcs) - len(missing), 100.0 * (len(funcs) - len(missing)) / len(funcs),
         len(missing), len(dups), len(extra)))
sys.exit(1 if missing else 0)
