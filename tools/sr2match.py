"""Match Street Rod 2 functions against the Street Rod 1 port (names, specs, code).

SR2 reuses SR1's game code and texts, but it was rebuilt with Microsoft C 6.0 (SR1: 5.1), so
compiled C functions rarely match instruction for instruction; hand-written assembly and the
graphics library do. Matching therefore works in passes, each adding pairs that later passes use
as anchors:

  1. code   - masked 6-instruction windows (immediates/displacements hidden), mutual best,
              score >= 0.75 (as tdmatch.py)
  2. string - the same distinctive strings referenced (strings used by at most 3 functions),
              mutual best by Jaccard similarity
  3. calls  - for every matched pair, the call sequences are aligned (LCS over already-matched
              callees); unmatched callees in the same gap at the same position are paired when
              their sizes are compatible; repeated until nothing changes
  4. loose  - mnemonic-only 4-instruction windows, mutual best, score >= 0.6, size ratio <= 2

class: same (pass 1 with score >= 0.99), changed (any other match), new (no SR1 counterpart).

usage: sr2match.py [SR1 repo, default ../StreetRod]
       writes port/sr2_sr1_matches.csv (every SR2 function) and prints a summary.
"""
import csv, json, os, re, struct, sys
from collections import defaultdict
from capstone import Cs, CS_ARCH_X86, CS_MODE_16

SR1 = sys.argv[1] if len(sys.argv) > 1 else '../StreetRod'
md = Cs(CS_ARCH_X86, CS_MODE_16)
NUM = re.compile(r'0x[0-9a-f]+|\b\d+\b')


def image(path):
    d = open(path, 'rb').read()
    return d[struct.unpack_from('<H', d, 8)[0] * 16:]


class Prog:
    def __init__(self, exe, index):
        self.img = image(exe)
        self.funcs = json.load(open(index))['functions']
        self.by_start = {f['start']: f for f in self.funcs}
        self.ops, self.mnem, self.callseq = {}, {}, {}
        for f in self.funcs:
            sg = int(f['start'][:4], 16)
            s = int(f['image'], 16)
            ops, mn, calls = [], [], []
            for addr, size, m, op in md.disasm_lite(self.img[s:s + f['size']], s - sg * 16):
                ops.append(m + ' ' + NUM.sub('#', op))
                mn.append(m)
                if m == 'call':
                    if op.startswith('0x'):                         # near: offset in this segment
                        calls.append('%04x:%04x' % (sg, int(op, 16) & 0xFFFF))
                    elif re.match(r'0x[0-9a-f]+:0x[0-9a-f]+$', op):  # far: seg:off as in the file
                        a, b = op.split(':')
                        calls.append('%04x:%04x' % (int(a, 16), int(b, 16)))
            self.ops[f['start']], self.mnem[f['start']] = ops, mn
            self.callseq[f['start']] = [c for c in calls if c in self.by_start]
        self.strings = {f['start']: set(f.get('strings', {}).values()) for f in self.funcs}


def grams(seq, n):
    return {tuple(seq[k:k + n]) for k in range(len(seq) - n + 1)}


def mutual_best(feat_a, feat_b, scorer, threshold, taken_a, taken_b):
    """feat_*: start -> set of features. Returns {a: (b, score)} for mutual best pairs."""
    inv = defaultdict(set)
    for b, fs in feat_b.items():
        if b not in taken_b:
            for x in fs:
                inv[x].add(b)
    best_a, best_b = {}, {}
    for a, fs in feat_a.items():
        if a in taken_a or not fs:
            continue
        votes = defaultdict(int)
        for x in fs:
            for b in inv.get(x, ()):
                votes[b] += 1
        for b, v in votes.items():
            s = scorer(a, b, v)
            if s >= threshold:
                if s > best_a.get(a, (None, 0))[1]:
                    best_a[a] = (b, s)
                if s > best_b.get(b, (None, 0))[1]:
                    best_b[b] = (a, s)
    return {a: (b, s) for a, (b, s) in best_a.items() if best_b[b][0] == a}


p1 = Prog(os.path.join(SR1, 'work/SR_unp.exe'), os.path.join(SR1, 'port/sr_functions.json'))
p2 = Prog('work/SR2_unp.exe', 'port/sr2_functions.json')
names = {r['address']: r['name'] for r in csv.DictReader(open(os.path.join(SR1, 'port/symbols.csv')))
         if r['kind'] == 'func'}
owners = {r['address']: r['owner'] for r in csv.DictReader(open(os.path.join(SR1, 'port/symbols.csv')))
          if r['kind'] == 'func'}

match = {}          # sr2 start -> (sr1 start, score, method)
rev = {}


def add(pairs, method):
    for a, (b, s) in pairs.items():
        if a not in match and b not in rev:
            match[a] = (b, s, method)
            rev[b] = a
    return len(pairs)


# 1. code
g1 = {f: grams(o, 6) for f, o in p1.ops.items() if len(o) >= 8}
g2 = {f: grams(o, 6) for f, o in p2.ops.items() if len(o) >= 8}
n = add(mutual_best(g2, g1, lambda a, b, v: v / max(len(g2[a]), len(g1[b])), 0.75, match, rev), 'code')
print('pass 1 code   :', n)

# 2. strings (distinctive ones only)
use1, use2 = defaultdict(int), defaultdict(int)
for s in p1.strings.values():
    for x in s:
        use1[x] += 1
for s in p2.strings.values():
    for x in s:
        use2[x] += 1
d1 = {f: {x for x in s if use1[x] <= 3 and len(x) >= 4} for f, s in p1.strings.items()}
d2 = {f: {x for x in s if use2[x] <= 3 and len(x) >= 4} for f, s in p2.strings.items()}
n = add(mutual_best(d2, d1, lambda a, b, v: v / len(d2[a] | d1[b]), 0.34, match, rev), 'string')
print('pass 2 string :', n)


# DGROUP offset map SR1 -> SR2, piecewise constant deltas from anchors: shared unique strings in the
# data segment, and DS operands of code-matched pairs with identical instruction shapes.
def dgroup(p, idx):
    return json.load(open(idx))['dgroup']


DS1 = int(dgroup(p1, os.path.join(SR1, 'port/sr_functions.json')), 16) * 16
DS2 = int(dgroup(p2, 'port/sr2_functions.json'), 16) * 16


def ds_strings(img, ds):
    out = defaultdict(list)
    for m in re.finditer(rb'(?<=\x00)[\x20-\x7e\r\n]{6,}\x00', img[ds:]):
        out[m.group(0)].append(m.start())
    return {k: v[0] for k, v in out.items() if len(v) == 1}


s1, s2 = ds_strings(p1.img, DS1), ds_strings(p2.img, DS2)
anchors = {s1[k]: s2[k] for k in s1.keys() & s2.keys()}


def ds_operands(p, start):
    f = p.by_start[start]
    s = int(f['image'], 16)
    out = []
    for ins in md.disasm(p.img[s:s + f['size']], 0):
        m = re.search(r'(?:^|[^:])\[(0x[0-9a-f]+)\]', ins.op_str)
        out.append((ins.mnemonic, int(m.group(1), 16) if m and 'ss:' not in ins.op_str and 'es:' not in ins.op_str
                    and 'cs:' not in ins.op_str else None))
    return out


md.detail = False
for a, (b, s, how) in list(match.items()):
    x, y = ds_operands(p2, a), ds_operands(p1, b)
    if len(x) == len(y) and all(u[0] == v[0] for u, v in zip(x, y)):
        for (_, u), (_, v) in zip(x, y):
            if u is not None and v is not None:
                anchors.setdefault(v, u)
anchor_keys = sorted(anchors)


def map_ds(off):
    """SR1 DS offset -> SR2 DS offset with the delta of the nearest anchor at or below."""
    import bisect
    i = bisect.bisect_right(anchor_keys, off) - 1
    if i < 0:
        return None
    k = anchor_keys[i]
    if off - k > 0x400:
        return None
    return (off + anchors[k] - k) & 0xFFFF


print('DGROUP anchors:', len(anchors))


def globals_pass():
    gl2 = {f['start']: {int(x, 16) for x in f['ds_reads'] + f['ds_writes']} for f in p2.funcs}
    gl1 = {}
    for f in p1.funcs:
        gl1[f['start']] = {y for y in (map_ds(int(x, 16)) for x in f['ds_reads'] + f['ds_writes']) if y is not None}
    gl2 = {k: v for k, v in gl2.items() if len(v) >= 2}
    gl1 = {k: v for k, v in gl1.items() if len(v) >= 2}
    return add(mutual_best(gl2, gl1, lambda a, b, v: (v / len(gl2[a] | gl1[b])) if compatible(a, b) else 0,
                           0.4, match, rev), 'globals')


# 3. call graph
def lcs_pairs(x, y, eq):
    m, n_ = len(x), len(y)
    t = [[0] * (n_ + 1) for _ in range(m + 1)]
    for i in range(m - 1, -1, -1):
        for j in range(n_ - 1, -1, -1):
            t[i][j] = t[i + 1][j + 1] + 1 if eq(x[i], y[j]) else max(t[i + 1][j], t[i][j + 1])
    i = j = 0
    out = []
    while i < m and j < n_:
        if eq(x[i], y[j]):
            out.append((i, j)); i += 1; j += 1
        elif t[i + 1][j] >= t[i][j + 1]:
            i += 1
        else:
            j += 1
    return out


def compatible(a, b):
    sa, sb = p2.by_start[a]['insns'], p1.by_start[b]['insns']
    return max(sa, sb) <= 2.5 * max(1, min(sa, sb)) + 6


def calls_pass():
    new = {}
    for a, (b, _, _) in list(match.items()):
        x, y = p2.callseq[a], p1.callseq[b]
        if not x or not y:
            continue
        aligned = lcs_pairs(x, y, lambda u, v: match.get(u, (None,))[0] == v)
        bounds = [(-1, -1)] + aligned + [(len(x), len(y))]
        for (i0, j0), (i1, j1) in zip(bounds, bounds[1:]):
            gx, gy = x[i0 + 1:i1], y[j0 + 1:j1]
            ux = [c for c in dict.fromkeys(gx) if c not in match]
            uy = [c for c in dict.fromkeys(gy) if c not in rev]
            if ux and len(ux) == len(uy):
                for u, v in zip(ux, uy):
                    if compatible(u, v) and u not in new:
                        new[u] = (v, 0.5)
    seen_b = defaultdict(int)
    for u, (v, _) in new.items():
        seen_b[v] += 1
    new = {u: bv for u, bv in new.items() if seen_b[bv[0]] == 1}
    return add(new, 'calls')


# 4. loose mnemonic windows
m1 = {f: grams(o, 4) for f, o in p1.mnem.items() if len(o) >= 12}
m2 = {f: grams(o, 4) for f, o in p2.mnem.items() if len(o) >= 12}
t1 = {f: grams(o, 3) for f, o in p1.mnem.items()}
t2 = {f: grams(o, 3) for f, o in p2.mnem.items()}


def loose_pass():
    return add(mutual_best(m2, m1, lambda a, b, v: v / max(len(m2[a]), len(m1[b])) if compatible(a, b) else 0,
                           0.6, match, rev), 'loose')


def sim(a, b):
    if not compatible(a, b):
        return 0.0
    x, y = t2[a], t1[b]
    return len(x & y) / max(1, len(x | y))


# 5. link order: corresponding segments keep their function order; functions between two matched
# neighbours are paired in order (equal counts) or by a similarity alignment.
def order_pass():
    import bisect
    votes = defaultdict(lambda: defaultdict(int))
    for a, (b, _, _) in match.items():
        votes[a[:4]][b[:4]] += 1
    segpair = {}
    for s2, c in votes.items():
        s1, n_ = max(c.items(), key=lambda kv: kv[1])
        if n_ >= 3 and n_ >= 0.6 * sum(c.values()):
            segpair[s2] = s1
    new = {}
    for s2, s1 in segpair.items():
        L2 = sorted((f['start'] for f in p2.funcs if f['start'][:4] == s2), key=lambda k: int(k[5:], 16))
        L1 = sorted((f['start'] for f in p1.funcs if f['start'][:4] == s1), key=lambda k: int(k[5:], 16))
        pos1 = {k: j for j, k in enumerate(L1)}
        pts = [(i, pos1[match[a][0]]) for i, a in enumerate(L2) if a in match and match[a][0] in pos1]
        # longest increasing chain of anchors
        tails, tidx, prev = [], [], [None] * len(pts)
        for k, (_, j) in enumerate(pts):
            q = bisect.bisect_left(tails, j)
            if q == len(tails):
                tails.append(j); tidx.append(k)
            else:
                tails[q] = j; tidx[q] = k
            prev[k] = tidx[q - 1] if q else None
        chain, k = [], tidx[-1] if tidx else None
        while k is not None:
            chain.append(pts[k]); k = prev[k]
        chain = [(-1, -1)] + chain[::-1] + [(len(L2), len(L1))]
        for (i0, j0), (i1, j1) in zip(chain, chain[1:]):
            U2 = [a for a in L2[i0 + 1:i1] if a not in match]
            U1 = [b for b in L1[j0 + 1:j1] if b not in rev]
            if not U2 or not U1:
                continue
            if len(U2) == len(U1) and len(U2) <= 8:
                for a, b in zip(U2, U1):
                    if compatible(a, b):
                        new[a] = (b, 0.4)
                continue
            # Needleman-Wunsch without gap cost, keep confident pairs
            m_, n_ = len(U2), len(U1)
            t = [[0.0] * (n_ + 1) for _ in range(m_ + 1)]
            for i in range(m_ - 1, -1, -1):
                for j in range(n_ - 1, -1, -1):
                    t[i][j] = max(t[i + 1][j], t[i][j + 1], t[i + 1][j + 1] + sim(U2[i], U1[j]))
            i = j = 0
            while i < m_ and j < n_:
                s = sim(U2[i], U1[j])
                if s > 0 and abs(t[i][j] - (t[i + 1][j + 1] + s)) < 1e-9:
                    if s >= 0.35:
                        new[U2[i]] = (U1[j], s)
                    i += 1; j += 1
                elif t[i + 1][j] >= t[i][j + 1]:
                    i += 1
                else:
                    j += 1
    return add(new, 'order')


stats = defaultdict(int)
while True:
    k = 0
    for name, fn in (('globals', globals_pass), ('calls', calls_pass), ('loose', loose_pass), ('order', order_pass)):
        n = fn()
        stats[name] += n
        k += n
    if not k:
        break
print('passes 3-5    :', dict(stats))

rows = []
for f in p2.funcs:
    a = f['start']
    if a in match:
        b, s, how = match[a]
        cls = 'same' if how == 'code' and s >= 0.99 else 'changed'
        rows.append([a, f['image'], f['insns'], cls, how, '%.2f' % s, b, names.get(b, ''), owners.get(b, ''),
                     p1.by_start[b]['insns']])
    else:
        rows.append([a, f['image'], f['insns'], 'new', '', '', '', '', '', ''])
with open('port/sr2_sr1_matches.csv', 'w', newline='') as o:
    w = csv.writer(o)
    w.writerow(['sr2_start', 'sr2_image', 'sr2_insns', 'class', 'method', 'score', 'sr1_start', 'sr1_name',
                'sr1_module', 'sr1_insns'])
    w.writerows(rows)
cnt = defaultdict(int)
for r in rows:
    cnt[r[3]] += 1
named = sum(1 for r in rows if r[7])
print('SR2 functions %d: same %d, changed %d, new %d; %d with an SR1 name; SR1 functions matched %d of %d'
      % (len(rows), cnt['same'], cnt['changed'], cnt['new'], named, len(rev), len(p1.funcs)))
