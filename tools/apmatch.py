"""Cross-match functions between the F-117A EXEs (shared MicroProse library + compiler RTL).

Every indexed function (tools/f117index.py) is disassembled with all immediates, displacements and
branch/call targets masked, so relocated segment words, DGROUP offsets and far-call offsets do not
matter. Two functions match exactly when their masked instruction sequences are equal; otherwise the
best partner above THRESHOLD by the share of shared 6-instruction windows (as tools/td2match.py of
TD3) is taken. AP is the reference: each AP function gets its best PS and INTRO partner, then PS
functions not reached that way get their best INTRO partner.

Names: port/symbols.csv rows (exe,kind,address,name,...) of any exe are carried over to the matched
functions of the other exes in the `name` column (first found: ap > ps > intro), and written back to
port/symbols.csv as rows with owner `match:<exe>` for partners that match exactly or score >= 0.9
(earlier `match:` rows are regenerated; hand-written rows always win).

usage: apmatch.py                 (reads port/<exe>_functions.json, writes port/shared_matches.csv)
"""
import csv, hashlib, json, os, re, struct
from collections import defaultdict
from capstone import Cs, CS_ARCH_X86, CS_MODE_16

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
N = 6
THRESHOLD = 0.6
md = Cs(CS_ARCH_X86, CS_MODE_16)
NUM = re.compile(r'0x[0-9a-f]+|\b\d+\b')
EXES = ['vgame', 'start', 'end', 'setup', 'player', 'dswap', 'mps_logo']


def image(path):
    d = open(path, 'rb').read()
    return d[struct.unpack_from('<H', d, 8)[0] * 16:]


progs = {}
for e in EXES:
    j = json.load(open(os.path.join(ROOT, 'port', e + '_functions.json')))
    img = image(os.path.join(ROOT, j['exe']))
    fl = []
    for f in j['functions']:
        s = int(f['image'], 16)
        ops = [mn + ' ' + NUM.sub('#', op) for _, _, mn, op in md.disasm_lite(img[s:s + f['size']], 0)]
        g = {tuple(ops[k:k + N]) for k in range(len(ops) - N + 1)}
        h = hashlib.sha1('\n'.join(ops).encode()).hexdigest()
        fl.append(dict(start=f['start'], n=len(ops), grams=g, hash=h, f=f))
    progs[e] = fl

names = {}
sym = os.path.join(ROOT, 'port', 'symbols.csv')
sym_rows = []
if os.path.exists(sym):
    for r in csv.DictReader(open(sym, encoding='utf-8')):
        if r['owner'].startswith('match:'):
            continue
        sym_rows.append(r)
        if r['kind'] == 'func':
            names[(r['exe'], r['address'].lower())] = r


def best(fn, other):
    """best partner of fn in program `other`: (entry, score, exact)"""
    ex = [x for x in progs[other] if x['hash'] == fn['hash']]
    if ex:
        return ex[0], 1.0, True
    if fn['n'] < N + 2 or not fn['grams']:
        return None, 0, False
    votes = defaultdict(int)
    for g in fn['grams']:
        for x in index[other].get(g, ()):
            if progs[other][x]['f']['ints'] == fn['f']['ints']:
                votes[x] += 1
    if not votes:
        return None, 0, False
    xi, v = max(votes.items(), key=lambda kv: kv[1] / max(len(fn['grams']), len(progs[other][kv[0]]['grams'])))
    x = progs[other][xi]
    sc = v / max(len(fn['grams']), len(x['grams']))
    return (x, sc, False) if sc >= THRESHOLD else (None, 0, False)


index = {}
for e in EXES:
    ix = defaultdict(set)
    for i, x in enumerate(progs[e]):
        for g in x['grams']:
            ix[g].add(i)
    index[e] = ix

rows = []
used = set()
for e in EXES:                                   # reference order: biggest program first
    for fn in progs[e]:
        if fn['n'] < 4 or (e, fn['start']) in used:
            continue
        grp = {e: (fn, 1.0, True)}
        for o in EXES:
            if o == e:
                continue
            x, sc, ex = best(fn, o)
            if x is not None and (o, x['start']) not in used:
                grp[o] = (x, sc, ex)
        if len(grp) < 2:
            continue
        for o, (x, _, _) in grp.items():
            used.add((o, x['start']))
        rows.append(grp)


def hint(*fs):
    for f in fs:
        if f is None:
            continue
        bits = []
        if f.get('source'):
            bits.append('src=' + '+'.join(f['source']))
        if f['ints']:
            bits.append('int ' + ' '.join(f['ints']))
        if f['ports']:
            bits.append('ports ' + ' '.join(f['ports']))
        st = [v for v in f['strings'].values()][:2]
        if st:
            bits.append('"' + '" "'.join(s.replace('\n', ' ').replace('\r', '')[:30] for s in st) + '"')
        if bits:
            return '; '.join(bits)
    return ''


seeds = dict(names)
used = {(k[0], v['name']) for k, v in names.items()}
with open(os.path.join(ROOT, 'port', 'shared_matches.csv'), 'w', newline='') as fh:
    w = csv.writer(fh)
    w.writerow(EXES + [e + '_score' for e in EXES] + ['exact', 'insns', 'name', 'hint'])
    for g in rows:
        src = next(((e, seeds[(e, g[e][0]['start'])]) for e in EXES if e in g and (e, g[e][0]['start']) in seeds), None)
        nm = src[1]['name'] if src else ''
        w.writerow([g[e][0]['start'] if e in g else '' for e in EXES]
                   + [('%.2f' % g[e][1]) if e in g else '' for e in EXES]
                   + [' '.join(e for e in EXES if e in g and g[e][2] and len(g) > 1), max(x[0]['n'] for x in g.values()),
                      nm, hint(*[g[e][0]['f'] for e in EXES if e in g])])
# carry every hand-written name to its best partner in each other exe (pairwise, not through AP)
by_start = {e: {x['start']: x for x in progs[e]} for e in EXES}
for (e, addr), sr in sorted(seeds.items()):
    if sr['kind'] != 'func' or addr not in by_start[e]:
        continue
    for o in EXES:
        if o == e:
            continue
        x, sc, ex = best(by_start[e][addr], o)
        if x is None or sc < 0.9 or (o, x['start']) in names or (o, sr['name']) in used:
            continue
        back, _, _ = best(x, e)                 # must be mutual
        if back is None or back['start'] != addr:
            continue
        rr = dict(sr)
        rr.update(exe=o, address=x['start'], owner='match:' + e, other_names='',
                  notes='= %s %s%s' % (e, addr, '' if ex else ' (score %.2f)' % sc))
        names[(o, x['start'])] = rr
        used.add((o, rr['name']))
        sym_rows.append(rr)
order = {e: i for i, e in enumerate(EXES)}
with open(sym, 'w', newline='', encoding='utf-8') as fh:
    w = csv.DictWriter(fh, ['exe', 'kind', 'address', 'name', 'type', 'owner', 'other_names', 'notes'])
    w.writeheader()
    for rr in sorted(sym_rows, key=lambda x: (order[x['exe']], x['kind'], x['address'])):
        w.writerow(rr)
print('symbols.csv: %d rows, %d carried over by matching'
      % (len(sym_rows), sum(1 for x in sym_rows if x['owner'].startswith('match:'))))
from collections import Counter
print('%d groups; sizes %s' % (len(rows), dict(sorted(Counter(len(g) for g in rows).items()))))
print('functions: ' + ', '.join('%s %d' % (e, len(progs[e])) for e in EXES))
