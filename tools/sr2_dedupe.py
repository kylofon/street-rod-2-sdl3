"""S3.7: one owner per function. Removes the func rows of non-owners from port/spec/*_symbols.csv
and marks their spec sections as secondary views.

usage: python tools/sr2_dedupe.py
Owner rule: a matched function goes to its SR1 module; new functions go to the spec that wrote the
fullest text (decided by hand in OWNER below). Re-runnable (idempotent).
"""
import csv, glob, io, os, re

OWNER = {
    # SR1 game_flow
    '0001:3934': 'game_flow', '0001:3e08': 'game_flow', '0001:3f28': 'game_flow', '0001:3f3e': 'game_flow',
    # SR1 garage
    '0001:423a': 'garage', '0001:4250': 'garage', '0001:51f8': 'garage', '0001:534c': 'garage',
    '0001:5400': 'garage', '0001:550e': 'garage', '0001:63e0': 'garage', '0001:6426': 'garage',
    '0001:64a2': 'garage', '0001:64b2': 'garage', '0001:650a': 'garage', '0001:66ae': 'garage',
    '0001:68e2': 'garage',
    # new
    '0001:0e94': 'game_flow', '0001:54ba': 'garage', '0001:72f2': 'garage',
    '0001:8c68': 'race',            # race.md reads car +06 (transmission) correctly; game_flow.md says engine
    '1a21:7304': 'race_render', '1a21:7446': 'race_render', '1a21:75ea': 'race_render',
}
# the Grudge Night / gas station cluster in 2925: game_flow wrote the full flow, race the race parts
for a in ('000e 00fa 01fa 193e 1970 1ab8 1ad2 1af0 1b28 1b84 1bba 1c06 1cae 1d00 1e08 1f38 20be 2104').split():
    OWNER['2925:' + a] = 'game_flow'

MARK = '> **Secondary view (S3.7):** owned by `%s.md`; on any conflict the owner\'s section wins.'

for path in sorted(glob.glob('port/spec/*_symbols.csv')):
    spec = os.path.basename(path)[:-len('_symbols.csv')]
    text = open(path, encoding='utf-8').read()
    rows = list(csv.reader(io.StringIO(text)))
    hdr, body = rows[0], rows[1:]
    ia, ik = hdr.index('address'), hdr.index('kind')
    keep, dropped = [], []
    for r in body:
        a = r[ia].strip().lower() if len(r) > ia else ''
        if r and r[ik] == 'func' and a in OWNER and OWNER[a] != spec:
            dropped.append(a)
        else:
            keep.append(r)
    if not dropped:
        continue
    out = io.StringIO()
    csv.writer(out, lineterminator='\n').writerows([hdr] + keep)
    open(path, 'w', encoding='utf-8', newline='\n').write(out.getvalue())
    md = 'port/spec/%s.md' % spec
    s = open(md, encoding='utf-8').read()
    n = 0
    for a in dropped:
        mark = MARK % OWNER[a]
        m = re.search(r'^(###[^\n]*%s[^\n]*\n)' % re.escape(a), s, re.M | re.I)
        if m and not s[m.end():m.end() + len(mark) + 2].strip().startswith(mark):
            s = s[:m.end()] + '\n' + mark + '\n' + s[m.end():]
            n += 1
    open(md, 'w', encoding='utf-8', newline='\n').write(s)
    print('%-12s dropped %2d func rows, marked %2d sections' % (spec, len(dropped), n))
