"""Spec coverage: which indexed functions have no row in any port/spec/*_symbols.csv.

usage: python tools/spec_coverage.py [exe ...]        (exe = intro, ps, ap; default all three)

Reads port/<exe>_functions.csv (tools/apindex.py) and every port/spec/*_symbols.csv (columns
exe,kind,address,...). Skips what the port does not reimplement: the Borland RTL (segment 0000;
PS/AP 0000 also hold volume.c and AP 0000 the DAC code, which platform/video list themselves), the
VROOMM overlay manager (AP 36f6) and the VROOMM stub thunks (AP 3c59-3ea0).

Writes work/coverage/<exe>_uncovered.txt (one line per missing function: address, size, source
file, first strings) and prints a per-segment summary of missing functions.
"""
import csv, glob, os, sys
from collections import Counter, defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def norm(a):
    a = a.strip().lower().replace('0x', '')
    if ':' not in a:
        return None
    s, o = a.split(':', 1)
    try:
        return '%04x:%04x' % (int(s, 16), int(o, 16))
    except ValueError:
        return None


def skipped(exe, seg):
    if seg == 0:  # Borland RTL (AP 0000 also has volume.c and DAC code: platform lists those)
        return True
    if exe == 'ap' and (seg == 0x36F6 or 0x3C59 <= seg <= 0x3EA0):
        return True
    return False


def covered_set():
    cov = defaultdict(dict)  # exe -> addr -> owner
    for path in glob.glob(os.path.join(ROOT, 'port', 'spec', '*_symbols.csv')):
        spec = os.path.basename(path)[:-len('_symbols.csv')]
        with open(path, newline='', encoding='utf-8', errors='replace') as fh:
            for r in csv.DictReader(fh):
                if (r.get('kind') or '').strip().lower() not in ('func', 'function'):
                    continue
                a = norm(r.get('address') or '')
                exe = (r.get('exe') or '').strip().lower()
                if a and exe:
                    cov[exe].setdefault(a, spec)
    return cov


def main():
    exes = sys.argv[1:] or ['intro', 'ps', 'ap']
    cov = covered_set()
    os.makedirs(os.path.join(ROOT, 'work', 'coverage'), exist_ok=True)
    for exe in exes:
        rows = list(csv.DictReader(open(os.path.join(ROOT, 'port', exe + '_functions.csv'), newline='')))
        missing, total, owners = [], 0, Counter()
        for r in rows:
            a = norm(r['start'])
            seg = int(a[:4], 16)
            if skipped(exe, seg):
                continue
            total += 1
            if a in cov[exe]:
                owners[cov[exe][a]] += 1
            else:
                missing.append((a, r))
        out = os.path.join(ROOT, 'work', 'coverage', exe + '_uncovered.txt')
        with open(out, 'w') as fh:
            for a, r in missing:
                fh.write('%s %6s %-28s %s\n' % (a, r['size'], r.get('source', '')[:28], (r.get('strings') or '')[:80]))
        print('%s: %d/%d covered, %d missing -> %s' % (exe, total - len(missing), total, len(missing), out))
        print('   by spec: ' + ', '.join('%s %d' % kv for kv in sorted(owners.items())))
        segs = Counter(a[:4] for a, _ in missing)
        print('   missing by segment: ' + ' '.join('%s:%d' % kv for kv in sorted(segs.items())))


if __name__ == '__main__':
    main()
