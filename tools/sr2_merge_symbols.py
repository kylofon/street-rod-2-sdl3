"""S3.7: merge port/spec/*_symbols.csv into port/symbols.csv (the source for gen_symbols.py and Ghidra).

usage: python tools/sr2_merge_symbols.py
Columns: kind,address,name,type,owner,other_names,notes. Functions: one row each (the coverage gate
guarantees a single owner). Globals: first owner wins, other specs' names go to other_names; name
clashes between different addresses are reported in port/symbol_conflicts.txt.
"""
import csv, glob, os
from collections import defaultdict

rows = {}
names = defaultdict(set)
for path in sorted(glob.glob('port/spec/*_symbols.csv')):
    spec = os.path.basename(path)[:-len('_symbols.csv')]
    for r in csv.DictReader(open(path, encoding='utf-8')):
        kind = (r.get('kind') or '').strip()
        if kind not in ('func', 'global'):
            continue
        addr = r['address'].strip().upper().replace('0X', '')
        key = (kind, addr)
        name = (r.get('name') or '').strip()
        if key in rows:
            if name and name != rows[key]['name'] and name not in rows[key]['other_names']:
                rows[key]['other_names'].append(name)
            continue
        rows[key] = dict(kind=kind, address=addr, name=name, type=(r.get('type') or '').strip(), owner=spec,
                         other_names=[], notes=(r.get('notes') or '').strip())
        names[name].add(key)

with open('port/symbols.csv', 'w', encoding='utf-8', newline='') as o:
    w = csv.writer(o, lineterminator='\n')
    w.writerow(['kind', 'address', 'name', 'type', 'owner', 'other_names', 'notes'])
    for key in sorted(rows, key=lambda k: (k[0] != 'func', k[1])):
        r = rows[key]
        w.writerow([r['kind'], r['address'].lower() if r['kind'] == 'func' else r['address'], r['name'],
                    r['type'], r['owner'], ' '.join(r['other_names']), r['notes']])
clash = {n: k for n, k in names.items() if n and len(k) > 1}
with open('port/symbol_conflicts.txt', 'w', encoding='utf-8') as o:
    for n, ks in sorted(clash.items()):
        o.write('%s: %s\n' % (n, ', '.join('%s %s (%s)' % (k[0], k[1], rows[k]['owner']) for k in sorted(ks))))
nf = sum(1 for k in rows if k[0] == 'func')
print('port/symbols.csv: %d functions, %d globals; %d name clashes -> port/symbol_conflicts.txt'
      % (nf, len(rows) - nf, len(clash)))
