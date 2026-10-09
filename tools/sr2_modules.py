"""Assign every SR2 function (port/sr2_functions.json) to a port module -> port/sr2_modules.csv
(sr2_start,module). Modules: platform video sound game_flow garage race race_render runtime.

1. runtime: MSC 6 C runtime segments (34b0, 371b-3726, 0000 stub) regardless of SR1 name.
2. matched to an SR1 function (port/sr2_sr1_matches.csv): the SR1 module of that function (SR1 spec
   tables in ../StreetRod/port/spec/<module>.md by address, else ../StreetRod/port/symbols.csv owner;
   SR1 'seed' = runtime; race functions listed in race_render.md = race_render).
3. new / unnamed: HAND overrides, then the majority module of the already-assigned neighbours
   (callers + callees, same segment first, then game-code segments / the library), else the
   segment default (0dcb race_render, 2925 game_flow, 2c57+2f1b sound, driver segments video).
usage: python tools/sr2_modules.py            (repo root; prints counts per module)
"""
import csv, json, os, re
from collections import Counter

R = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
S1 = os.path.join(os.path.dirname(R), 'StreetRod')
fn = json.load(open(os.path.join(R, 'port/sr2_functions.json')))['functions']
F = {f['start']: f for f in fn}
M = {r['sr2_start']: r for r in csv.DictReader(open(os.path.join(R, 'port/sr2_sr1_matches.csv')))}

sr1 = {}
for mod in ('platform', 'video', 'sound', 'game_flow', 'garage', 'race', 'race_render'):
    t = open(os.path.join(S1, 'port/spec', mod + '.md'), encoding='utf-8', errors='replace').read()
    for a in re.findall(r'^\|\s*`?([0-9a-f]{4}:[0-9a-f]{4})`?\s*\|', t, re.M | re.I):
        sr1[a.lower()] = mod
for r in csv.DictReader(open(os.path.join(S1, 'port/symbols.csv'))):
    if r['kind'] == 'func':
        o = 'runtime' if r['owner'] == 'seed' else r['owner']
        if r['address'].lower() not in sr1 or o == 'runtime':
            sr1[r['address'].lower()] = o

RUNTIME = {'0000', '34b0', '371b', '371c', '3720', '3722', '3723', '3725', '3726'}
DEFAULT = {'0001': 'game_flow', '0dcb': 'race_render', '1a21': 'platform', '2925': 'game_flow',
           '2c57': 'sound', '2f1b': 'sound', '2719': 'video', '2f32': 'video', '26b3': 'video',
           '26c0': 'video', '26e6': 'video'}
HAND = {'0001:01ea': 'sound', '2925:2e60': 'video'}


def seg_default(a):
    s = a[:4]
    if s in RUNTIME:
        return 'runtime'
    return DEFAULT.get(s, 'video')   # 320d..3488: graphics-driver assembly segments


mod = {}
for a, f in F.items():
    if a[:4] in RUNTIME:
        mod[a] = 'runtime'
    elif a in HAND:
        mod[a] = HAND[a]
    elif a[:4] in ('2c57', '2f1b'):
        mod[a] = 'sound'
    elif M[a]['sr1_start'] and M[a]['sr1_start'].lower() in sr1:
        mod[a] = sr1[M[a]['sr1_start'].lower()]
voted = 0
for _ in range(4):
    for a, f in F.items():
        if a in mod:
            continue
        nb = list(set(f['calls']) | set(f['callers']))
        same = [mod[b] for b in nb if b in mod and b[:4] == a[:4] and mod[b] != 'runtime']
        # game-code segments only look at other game-code segments (the library 1a21 and the driver
        # segments are called from everywhere and would drag everything to video / platform)
        game = ('0001', '0dcb', '2925')
        anyn = [mod[b] for b in nb if b in mod and mod[b] != 'runtime'
                and (a[:4] == '1a21' or (a[:4] in game and b[:4] in game))]
        for votes in (same, anyn):
            if votes:
                c = Counter(votes).most_common()
                if len(c) == 1 or c[0][1] > c[1][1]:
                    mod[a] = c[0][0]
                    voted += 1
                    break
for a in F:
    mod.setdefault(a, seg_default(a))
with open(os.path.join(R, 'port/sr2_modules.csv'), 'w', newline='') as o:
    w = csv.writer(o)
    w.writerow(['sr2_start', 'module'])
    for a in F:
        w.writerow([a, mod[a]])
c = Counter(mod.values())
print(len(mod), 'functions;', 'voted', voted, dict(c.most_common()))
