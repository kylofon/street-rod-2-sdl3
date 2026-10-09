"""Write the phase-5 module interface headers aotpport/src/<module>/<module>.h (+ platform/rtl.h) from
port/modules.csv (tools/assign_modules.py) — phase 5, step 1. The headers are a starting point written
before any implementation: after this step they are maintained by hand by the module owners, so the
script refuses to overwrite an existing header unless --force is given (platform.h: --force replaces only the
generated part after GENERATED_MARK).

Every function with iface x (called from another module, another program's code, or prog.c) or f
(called from another C file of the same module) gets a prototype, grouped by its C file. Signatures:

  arguments  the stack frame of the original ([bp+6..] far / [bp+4..] near, all members of a shared
             unit): a slot read with LES/LDS is a far pointer (FarPtr), a DWORD access a 32-bit value,
             anything else one word. Names and types come from the spec's type column
             (port/symbols.csv) when its word count agrees with the frame; a near data pointer is a
             u16 DS offset named *_ds. Functions without a BP frame (assembly, register interfaces) keep
             the spec's parameter list, or get (void) plus a note.
  return     the spec type when it gives one; otherwise AX (u16) / DX:AX (u32) when a caller reads the
             register right after the call, else void.
  varargs    a function that takes the address of its own arguments (va_start) gets "...": the words
             of the original stack image as C `unsigned` arguments (PORTING.md "Varargs").

usage: python tools/gen_module_headers.py [--force]
"""
import csv, os, re, runpy, struct, sys
from collections import Counter, defaultdict
from capstone import Cs, CS_ARCH_X86, CS_MODE_16
from capstone.x86 import X86_REG_AX, X86_REG_AL, X86_REG_AH, X86_REG_EAX, X86_REG_DX, X86_REG_DL, \
    X86_REG_DH, X86_REG_EDX

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
P = lambda *a: os.path.join(ROOT, *a)
FORCE = '--force' in sys.argv
A = runpy.run_path(P('tools', 'assign_modules.py'))
UNITS, UKIND, UMOD, CFILE, IFACE, CNAME, SPEC, FUNCS, SYM = (A[x] for x in (
    'UNITS', 'UKIND', 'UMOD', 'CFILE', 'IFACE', 'CNAME', 'SPEC', 'FUNCS', 'SYM'))
name_of, home, MODULES, EXES, UNIT_OF = A['name_of'], A['home'], A['MODULES'], A['EXES'], A['UNIT_OF']

EXE_FILE = {'intro': 'Game/INTRO.EXE', 'ps': 'Game/PS.EXE', 'ap': 'work/AP_flat.exe'}
IMG = {}
for e, fn in EXE_FILE.items():
    d = open(P(fn), 'rb').read()
    IMG[e] = d[struct.unpack_from('<H', d, 8)[0] * 16:]
md = Cs(CS_ARCH_X86, CS_MODE_16)
md.detail = True
AXR = {X86_REG_AX, X86_REG_AL, X86_REG_AH, X86_REG_EAX}
DXR = {X86_REG_DX, X86_REG_DL, X86_REG_DH, X86_REG_EDX}


def lin(a):
    return int(a[:4], 16) * 16 + int(a[5:], 16)


def disasm(k):
    f = FUNCS[k]
    if f.get('unindexed') or 'size' not in f:
        return []
    start = int(f['image'], 16)
    code = IMG[k[0]][start:start + f['size']]
    try:
        ins = list(md.disasm(code, start))
        if not any(x.mnemonic in ('ret', 'retf') for x in ins):
            # the index ends some functions before their epilogue: continue to the first return
            more = []
            for x in md.disasm(IMG[k[0]][start + f['size']:start + f['size'] + 1024], start + f['size']):
                more.append(x)
                if x.mnemonic in ('ret', 'retf', 'iret'):
                    break
            ins += more
        return ins
    except Exception:
        return []


BPARG = re.compile(r'(?:(byte|word|dword|qword) ptr |ptr )?(?:ss:)?\[bp \+ (0x[0-9a-f]+|\d+)\]')


def frame(k):
    """-> (has_frame, slots{word_index: 'w'|'d'|'f'}, varargs). The frame register is BP (C functions,
    most assembly) or BX (`mov bx, sp` helpers of 277a); pushes before it shift the argument base."""
    ins = disasm(k)
    if not ins:
        return False, {}, False
    rets = {x.mnemonic for x in ins if x.mnemonic in ('ret', 'retf')}
    far = ('retf' in rets) if len(rets) == 1 else bool(FUNCS[k]['far'])
    fr, pushes, start = None, 0, 0
    for i, x in enumerate(ins[:10]):
        if x.mnemonic == 'enter':
            fr, pushes, start = 'bp', pushes + 1, i + 1
            break
        if x.mnemonic == 'mov' and x.op_str in ('bp, sp', 'bx, sp'):
            fr, start = x.op_str[:2], i + 1
            break
        if x.mnemonic == 'push':
            pushes += 1
        elif x.mnemonic.startswith('j') or x.mnemonic in ('call', 'lcall', 'ret', 'retf'):
            break
    if not fr:
        return False, {}, False
    base = (4 if far else 2) + 2 * pushes
    slots, va = {}, False
    rx = BPARG if fr == 'bp' else BXARG
    for x in ins[start:]:
        for m in rx.finditer(x.op_str):
            o = int(m.group(2), 0)
            if o < base:
                continue
            w = (o - base) // 2
            size = m.group(1)
            if x.mnemonic in ('les', 'lds', 'lfs', 'lgs') and not size:
                kind = 'f'
            elif size in ('dword', 'qword'):
                kind = 'd'
            else:
                kind = 'w'
            if x.mnemonic == 'lea':
                va = fr == 'bp'
                kind = 'w'
            prev = slots.get(w)
            if prev is None or (prev == 'w' and kind in 'fd'):
                slots[w] = kind
        if x.mnemonic in ('mov', 'xchg', 'pop', 'les', 'lds', 'lea', 'add', 'sub', 'inc', 'dec', 'xor') and                 x.op_str.split(',')[0].strip() in (fr, 'e' + fr) and x.op_str != fr + ', sp':
            break                                   # frame register reused
    return True, slots, va


BXARG = re.compile(r'(?:(byte|word|dword|qword) ptr |ptr )?(?:ss:)?\[bx \+ (0x[0-9a-f]+|\d+)\]')


REGNAME = {}
for _n in ('ax', 'bx', 'cx', 'dx', 'si', 'di', 'es'):
    REGNAME[_n] = _n
for _n, _b in (('al', 'ax'), ('ah', 'ax'), ('bl', 'bx'), ('bh', 'bx'), ('cl', 'cx'), ('ch', 'cx'),
               ('dl', 'dx'), ('dh', 'dx'), ('eax', 'ax'), ('ebx', 'bx'), ('ecx', 'cx'), ('edx', 'dx'),
               ('esi', 'si'), ('edi', 'di')):
    REGNAME[_n] = _b


def reg_inputs(k):
    """registers read before written at the start of the function (register interface) -> [(ctype, reg)]"""
    ins = disasm(k)
    written, found = set(), []
    for x in ins[:24]:
        if x.mnemonic in ('push', 'pushf', 'pusha', 'pushad', 'pushfd'):
            continue                                       # saving a register is not using it
        if x.mnemonic == 'mov' and x.op_str.split(', ')[-1] in ('si', 'di'):
            d = REGNAME.get(x.op_str.split(', ')[0])
            if d:
                written.add(d)
            continue                                       # mov dx, si: saving a register variable
        if x.mnemonic.startswith('j') or x.mnemonic in ('call', 'lcall', 'ret', 'retf', 'iret', 'loop', 'int'):
            break
        try:
            rd, wr = x.regs_access()
        except Exception:
            break
        if x.mnemonic in ('xor', 'sub', 'sbb') and len(set(x.op_str.split(', '))) == 1:
            rd = []                                        # xor r, r / sub r, r only write
        if x.mnemonic == 'les':
            written.add('es')
        if x.mnemonic in ('shld', 'shrd', 'movzx', 'movsx'):
            d = REGNAME.get(x.op_str.split(', ')[0])
            rd = [r for r in rd if REGNAME.get(x.reg_name(r)) != d]   # shld edx, eax, 10h: builds DX:AX
        for r in rd:
            n = x.reg_name(r)
            b = REGNAME.get(n)
            if b and b not in written and b not in [f[1] for f in found]:
                found.append(('u32' if (n.startswith('e') and len(n) == 3 and n != 'es') else ('u8' if n[1] in 'lh' and n != 'dh' or n == 'dh'
                                                               else 'u16'), b if not n[1:] in ('l', 'h') else n))
        for r in wr:
            b = REGNAME.get(x.reg_name(r))
            if b:
                written.add(b)
    return found


def slot_list(slots):
    """word slots -> [(kind, nwords)] covering 0..max, far/dword consuming two words"""
    if not slots:
        return []
    top = max(w + (2 if k in 'fd' else 1) for w, k in slots.items())
    out, w = [], 0
    while w < top:
        k = slots.get(w, 'w')
        if k in 'fd':
            out.append((k, 2))
            w += 2
        else:
            out.append(('w', 1))
            w += 1
    return out


# ------------------------------------------------------------------------------ return value from callers
CALLSITES = defaultdict(list)          # callee key -> [(exe, ins list, index)]


def scan_callers():
    wanted = defaultdict(set)
    for t in UNITS:
        if IFACE[t]:
            for k in t:
                for c in FUNCS[k]['callers']:
                    wanted[(k[0], c)].add(k)
    for ck, callees in wanted.items():
        if ck not in FUNCS:
            continue
        ins = disasm(ck)
        seg = int(ck[1][:4], 16)
        tl = {lin(k[1]): k for k in callees}
        for i, x in enumerate(ins):
            if x.mnemonic not in ('call', 'lcall'):
                continue
            tgt = None
            b = bytes(x.bytes)
            if b[0] == 0x9A and len(b) == 5:                 # call far seg:off (file segment)
                o_, s_ = struct.unpack_from('<HH', b, 1)
                tgt = s_ * 16 + o_
            elif b[0] == 0xE8 and len(b) == 3:               # call near rel16 (push cs first = far)
                ip = (x.address - seg * 16 + 3) & 0xFFFF
                tgt = seg * 16 + ((ip + struct.unpack_from('<h', b, 1)[0]) & 0xFFFF)
            if tgt in tl:
                CALLSITES[tl[tgt]].append((ins, i))


scan_callers()


def reads_after(ins, i):
    """-> (reads AX, reads DX) before they are written, following the fall-through after the call"""
    ax = dx = None
    for x in ins[i + 1:i + 10]:
        if x.mnemonic in ('add', 'pop', 'inc', 'dec') and x.op_str.startswith(('sp', 'cx')):
            continue                                        # argument clean-up
        if x.mnemonic.startswith('j') or x.mnemonic in ('ret', 'retf', 'call', 'lcall', 'iret'):
            if x.mnemonic.startswith('j') and x.mnemonic != 'jmp':
                break
            if x.mnemonic in ('ret', 'retf'):
                break
            break
        try:
            rd, wr = x.regs_access()
        except Exception:
            break
        rd, wr = set(rd), set(wr)
        if x.mnemonic in ('xor', 'sub') and len(set(x.op_str.split(', '))) == 1:
            rd = set()                                      # xor r, r only writes
        if ax is None and rd & AXR:
            ax = True
        if dx is None and rd & DXR:
            dx = True
        if ax is None and wr & AXR:
            ax = False
        if dx is None and wr & DXR:
            dx = False
        if ax is not None and dx is not None:
            break
    return bool(ax), bool(dx)


def caller_ret(t):
    a = d = 0
    n = 0
    for k in t:
        for ins, i in CALLSITES.get(k, []):
            ra, rd = reads_after(ins, i)
            n += 1
            a += ra
            d += rd and ra
    if not n:
        return None
    if d:
        return 'u32'
    if a:
        return 'u16'
    return 'void'


# ------------------------------------------------------------------------------------ spec type parsing
TYPEWORDS = {'int', 'char', 'long', 'unsigned', 'signed', 'short', 'void', 'u8', 'u16', 'u32', 's8', 's16',
             's32', 'i8', 'i16', 'i32', 'uint', 'word', 'byte', 'bool', 'dword', 'const', 'struct', 'far',
             'near', 'huge', 'FILE', 'VFILE', 'Item', 'item', 'Plane', 'Flight', 'Gauge', 'Ch', 'Node', 'node',
             'vec', 'lvec', 'mat', 'ctl', 'evt', 'frame', 'wp', 'pos', 'sched', 'tagnode', 'files', 'VREC',
             'Obj', 'obj', 'Rect', 'rect', 'Dlg', 'dlg', 'Slider', 'Shape', 'shape', 'View', 'view'}
CKEYWORDS = set(A['C_RESERVED']) | {'far', 'near', 'huge', 'interrupt', 'cdecl', 'pascal', 'FILE',
                                    'VFILE', 'Item', 'frame', 'item', 'n_'}


def split_params(s):
    s = s.strip()
    if not s or s in ('void', '...', '?'):
        return [] if s != '...' else ['...']
    depth, cur, out = 0, '', []
    for ch in s:
        if ch == '(':
            depth += 1
        elif ch == ')':
            depth -= 1
        if ch == ',' and depth == 0:
            out.append(cur)
            cur = ''
        else:
            cur += ch
    out.append(cur)
    out = [o.strip() for o in out if o.strip()]
    if len(out) == 1 and ' ' in out[0] and '(' not in out[0]:
        # "int code int index": split before a base type that follows a name
        toks = out[0].replace('*', ' * ').split()
        parts, cur = [], []
        for i, tk in enumerate(toks):
            if cur and tk in TYPEWORDS - {'far', 'near', 'huge', 'const', 'struct'} and \
                    cur[-1] not in ('unsigned', 'signed', 'const', 'struct', 'far', 'near', 'huge') and \
                    cur[-1] not in TYPEWORDS:
                parts.append(' '.join(cur))
                cur = []
            cur.append(tk)
        parts.append(' '.join(cur))
        out = parts
    return out


def parse_type(ty):
    """-> (ret or None, [param strings] or None)"""
    ty = (ty or '').strip()
    if not ty or '(' not in ty or 'see ' in ty:
        r = ty.replace('far', '').replace('near', '').strip()
        return (r if r in ('void', 'int', 'u16', 's16', 'long', 'char') else None), None
    i = ty.index('(')
    depth, j = 0, i
    for j in range(i, len(ty)):
        if ty[j] == '(':
            depth += 1
        elif ty[j] == ')':
            depth -= 1
            if depth == 0:
                break
    head, params = ty[:i], ty[i + 1:j]
    head = re.sub(r'\b(far|near|static|interrupt|cdecl)\b', ' ', head).strip()
    htoks = head.replace('*', ' * ').split()
    if htoks and htoks[-1] not in TYPEWORDS and htoks[-1] != '*' and len(htoks) > 1:
        htoks = htoks[:-1]                                  # "int timer_install(" -> drop the name
    elif htoks and htoks[-1] not in TYPEWORDS and htoks[-1] != '*':
        htoks = []                                          # "far best_load_mission(i)": no type
    ret = ' '.join(htoks) if htoks else None
    far_ret = bool(re.search(r'\bfar\s*\*|\*\s*far|far\*', ty[:i]))
    if ret and far_ret and '*' in ret:
        ret = 'far *'
    return ret, split_params(params)


def ctype_ret(r):
    if r is None:
        return None
    r = r.strip()
    if r == 'far *' or 'huge' in r:
        return 'FarPtr'
    if '*' in r:
        return 'u16'
    if r in ('void',):
        return 'void'
    if 'long' in r or r in ('i32', 's32'):
        return 'u32' if 'unsigned' in r else 's32'
    if r in ('u32', 'dword'):
        return 'u32'
    if r in ('char', 'i8', 's8', 'signed char'):
        return 's8'
    if r in ('u8', 'byte', 'unsigned char'):
        return 'u8'
    if r in ('unsigned', 'u16', 'word', 'uint', 'unsigned int'):
        return 'u16'
    if r in ('int', 'short', 'i16', 's16', 'bool', 'signed'):
        return 's16'
    return 'u16'


def cparam(p, idx, used):
    """spec parameter text -> (ctype, cname, nwords)"""
    p = p.strip().replace('…', '')
    fnptr = '(' in p
    toks = p.replace('*', ' * ').replace('[', ' [').split()
    name = None
    if toks and re.match(r'^[A-Za-z_]\w*$', toks[-1]) and toks[-1] not in TYPEWORDS:
        name = toks[-1]
        toks = toks[:-1]
    if fnptr:
        m = re.search(r'\(\s*(?:far\s*)?\*\s*(\w+)\s*\)', p)
        name = m.group(1) if m else name
        ct, nw = 'FarPtr', 2
    elif '*' in toks or '[' in p:
        last = toks.index('*') if '*' in toks else len(toks)
        ptr_far = any(x in ('far', 'huge') for x in toks[max(0, last - 2):last + 1]) and toks.count('*') == 1
        if ptr_far:
            ct, nw = 'FarPtr', 2
        else:
            ct, nw = 'u16', 1
            name = (name or 'p%d' % idx) + '_ds'
    elif 'far' in toks and not any(t in toks for t in ('int', 'char', 'long')):
        ct, nw = 'FarPtr', 2                                # "far fn", "far dst", "far text"
    else:
        base = ' '.join(t for t in toks if t not in ('const', 'register'))
        ct = ctype_ret(base) if base else 'u16'           # a bare name: one word of unknown sign
        if ct == 'void' or ct is None:
            ct = 'u16'
        nw = 2 if ct in ('s32', 'u32') else 1
    if not name or name in CKEYWORDS or name in TYPEWORDS:
        name = 'a%d' % idx
    name = re.sub(r'\W', '_', name)
    while name in used:
        name += '_'
    used.add(name)
    return ct, name, nw


# Parameter lists written in the specs' headings and table rows: `1142:0164 threat_score(a, b, out_f)`
SPECSIG = {}
_SIGRX = re.compile(r'([A-Za-z_]\w*)\(([^()]*(?:\([^()]*\)[^()]*)*)\)')
for _md in sorted(A['glob'].glob(P('port', 'spec', '*.md'))):
    _mdn = os.path.basename(_md)[:-3]
    for _line in open(_md, encoding='utf-8'):
        if _line.startswith('#'):
            _cells = [_line]
        elif _line.startswith('|'):
            _cells = _line.split('|')[1:3]
        else:
            continue
        for _c in _cells:
            for _m in _SIGRX.finditer(_c.replace('`', ' ')):
                SPECSIG.setdefault((_mdn, _m.group(1)), _m.group(2))
                SPECSIG.setdefault((None, _m.group(1)), _m.group(2))


def spec_params(t):
    n = name_of(home(t))
    mdn = SPEC[t][0]
    s = SPECSIG.get((mdn, n))
    if s is None:
        s = SPECSIG.get((None, n))
    if s is None or re.search(r'DS:|[+/<>=]|0x|\d{2,}| - ', s):
        return None
    return split_params(s)


def signature(t):
    k0 = home(t)
    rows = [SYM.get(k) for k in t if SYM.get(k)]
    ty = next((r['type'] for r in rows if '(' in (r['type'] or '')), rows[0]['type'] if rows else '')
    sret, sparams = parse_type(ty)
    if not sparams:
        sp2 = spec_params(t)
        if sp2:
            sparams = sp2
    notes = []
    frames = [frame(k) for k in t]
    hasf = [f for f in frames if f[0]]
    slots = {}
    va = False
    for f in hasf:
        for w, kd in f[1].items():
            if slots.get(w) in (None, 'w'):
                slots[w] = kd
        va = va or f[2]
    sl = slot_list(slots)
    nwords = sum(n for _, n in sl)
    # parameters
    used = set()
    params = None
    if sparams is not None:
        sp = [p for p in sparams if p.strip() not in ('...', 'void', '')]
        cand = [cparam(p, i, used) for i, p in enumerate(sp)]
        if hasf and sum(c[2] for c in cand) < nwords:
            # specs often write char * for a far pointer: promote near pointers where the frame agrees
            short = nwords - sum(c[2] for c in cand)
            w, promoted = 0, []
            for c in cand:
                if short and c[0] == 'u16' and c[1].endswith('_ds') and slots.get(w) == 'f':
                    c = ('FarPtr', c[1][:-3], 2)
                    short -= 1
                promoted.append(c)
                w += c[2]
            if not short:
                cand = promoted
        if hasf and cand and sum(c[2] for c in cand) > nwords:
            notes.append('last %d word(s) unused by the original' % (sum(c[2] for c in cand) - nwords))
        if not hasf or sum(c[2] for c in cand) >= nwords:
            params = cand
            # the frame knows better whether a pointer is far
            if hasf:
                w = 0
                fixed = []
                for c in cand:
                    kd = slots.get(w, 'w')
                    if c[2] == 2 and kd == 'd' and c[0] == 'FarPtr':
                        fixed.append(c)
                    else:
                        fixed.append(c)
                    w += c[2]
                params = fixed
        elif hasf and len(sp) == len(sl):
            used = set()
            params = []
            for i, ((kd, n), p) in enumerate(zip(sl, sp)):
                ct, nm, _ = cparam(p, i, used)
                if kd == 'f':
                    ct, nm = 'FarPtr', nm[:-3] if nm.endswith('_ds') else nm
                elif kd == 'd':
                    ct = ct if ct in ('s32', 'u32') else 'u32'
                elif ct == 'FarPtr':
                    ct = 'u16'
                params.append((ct, nm, n))
            notes.append('types from the frame')
    if params is None:
        if hasf:
            params = [({'f': 'FarPtr', 'd': 'u32', 'w': 'u16'}[kd], 'a%d' % i, n) for i, (kd, n) in enumerate(sl)]
            if sparams:
                notes.append('spec: (%s)' % ', '.join(sparams)[:70])
        else:
            params = []
            if sparams is None and FUNCS[k0].get('insns', 0) > 3:
                notes.append('no BP frame: register interface or no arguments, see the spec')
    # return
    cr = caller_ret(t)
    ret = ctype_ret(sret)
    if ret is None or (ret == 'void' and cr in ('u16', 'u32')):
        if ret == 'void':
            notes.append('spec says void, callers read AX')
        ret = cr if cr in ('u16', 'u32') else 'void'
    elif ret in ('s16', 'u16', 'u8', 's8') and cr == 'u32':
        notes.append('callers also read DX')
    merged, i = [], 0                  # "tbl_off, tbl_seg" -> FarPtr tbl
    while i < len(params):
        c = params[i]
        if i + 1 < len(params) and c[1].endswith('off') and params[i + 1][1].endswith('seg') \
                and c[1][:-3] == params[i + 1][1][:-3] and c[2] == 1 and params[i + 1][2] == 1:
            merged.append(('FarPtr', c[1][:-3].rstrip('_') or 'p', 2))
            i += 2
            continue
        merged.append(c)
        i += 1
    params = merged
    regs = []
    for k in t[:1]:
        regs = reg_inputs(k)
    if regs:
        names = {n for _, n, _ in params}
        params = params + [(c, r if r not in names else r + '_reg', 0) for c, r in regs]
        notes.append('register input%s %s' % ('s' if len(regs) > 1 else '', '/'.join(r.upper() for _, r in regs)))
        notes = [n for n in notes if not n.startswith('no BP frame')]
    plist = ', '.join('%s %s' % (c, n) for c, n, _ in params) or 'void'
    if va:
        plist = (plist + ', ...') if params else '...'
        notes.append('varargs: stack words as unsigned')
    return ret, plist, notes


def addr_text(t):
    return ' / '.join('%s %s' % (k[0].upper(), k[1]) for k in t)


def comment_for(t, notes):
    md_, sec = SPEC[t]
    orig = name_of(home(t)) or ''
    spec = ('%s.md %s' % (md_, sec)).strip() if md_ else 'no spec'
    n = ('%s' % orig) if orig and orig != CNAME[t] else ''
    s = '/* %s%s — %s%s */' % (addr_text(t), (' ' + n) if n else '', spec,
                               ('; ' + '; '.join(notes)) if notes else '')
    return s


# ------------------------------------------------------------------------------------------ headers
HEAD = {
    'platform': ('platform.md, platform_resources.md', 'Dynamix system library (INTRO 05da / PS 16e3 / '
                 'AP 1d32 platform parts), volume.c, loaders and unpackers, EMS/emm_pool/AP mem.c, fatal '
                 'errors, the three mains and their start-up, the DS:1A04 driver callback wrappers.'),
    'video': ('video*.md', 'VGA driver entries in C (video/drv_vga.c, rows "drv" of modules.csv), the '
              'driver thunks, graphics primitives, bitmaps, text, palettes and fades, VQT/pictures, TTM, '
              'PS page helpers and PS-only ts routines.'),
    'sound': ('sound.md, sound_extra.md', 'SCI player (INTRO 1373 / PS 430a / AP 3060), the sound API '
              'modules, AP sx_sim. The AdLib driver model (SCI ADL.DRV behaviour, sound.md §13) is '
              'sound-internal and not in the EXEs.'),
    'shell_ui': ('shell_ui*.md', 'INTRO flow, PS screens, the alert/menu/listbox toolkit.'),
    'shell_data': ('shell_data*.md', 'PS emd\\*, sys\\*, miss\\script.c + mission types, file.c.'),
    'sim_flow': ('sim_flow*.md', 'AP flight loop, init, views, cockpit/HUD, messages, radio, dialogs and '
                 'sliders, map, realism, results, autopilot, VCR.'),
    'sim_world': ('sim_world*.md', 'AP flight model, AI, weapons, damage, collisions, ships, mission set-up.'),
    'render3d': ('render3d*.md', 'AP 3Space core (277a, also serving PS 3d50 where identical), ts2 shape '
                 'library, scene, world loading, horizon/sky, palette tricks.'),
}
CONVENTIONS = r''' *
 * Generated by tools/gen_module_headers.py from port/modules.csv (phase 5 step 1); maintained by hand
 * from now on (the module owner adds what is missing, fixes signatures while porting and notes it).
 * Conventions (PORTING.md): Borland int = s16, unsigned = u16, long = s32; a near data pointer is a u16
 * DGROUP offset (name *_ds); a far data pointer is a FarPtr by value; a far code pointer is a FarPtr
 * (codeptr_lookup). Shared-library code uses G_/F_ ids (symbols_layout.h), program code its own
 * <EXE>_DS_/<EXE>_FN_ symbols. Types marked "from the frame" were derived from the original stack
 * frame (LES/LDS = far pointer, DWORD = 32 bit); u16 there means "a word", check the spec for sign.
 * Each prototype: original address(es), original name if the C name differs, spec section.
 * Sections: one per C file of the module (port/modules.csv c_file); "[file]" = only called from
 * other files of this module.'''


def header(mod, units, extra_top='', include=('mem.h',)):
    spec, desc = HEAD[mod]
    out = ['#pragma once', '/* %s module (%s): %s' % (mod, spec, desc), CONVENTIONS, ' */']
    for inc in include:
        out.append('#include "%s"' % inc)
    if extra_top:
        out.append(extra_top)
    byfile = defaultdict(list)
    for t in units:
        byfile[CFILE[t]].append(t)
    n = 0
    for f in sorted(byfile):
        out.append('')
        out.append('/* ==== %s ' % f + '=' * max(4, 92 - len(f)) + ' */')
        for t in sorted(byfile[f], key=lambda t: (IFACE[t] != 'x', t[0][0] != home(t)[0], home(t)[1])):
            ret, plist, notes = signature(t)
            if IFACE[t] == 'f':
                notes = ['[file]'] + notes
            out.append(comment_for(t, notes))
            out.append('%s %s(%s);' % (ret, CNAME[t], plist))
            n += 1
    if mod in EXTRA:
        out += EXTRA[mod].splitlines()
        n += EXTRA[mod].count(');')
    out.append('')
    return out, n


EXTRA = {'video': r'''
/* ==== video/drv_vga.c ============================================================================= */
/* The VMCODE.OVL / VMCODE2.OVL VGA driver (not in the EXEs; modules.csv rows "drv"), in C: video.md
 * §2.3, §3. gfx_init (video/gfx_init.c) "loads" it into the DOS block it allocates, fills the jump
 * table (INTRO DS:1938 / PS DS:3174 / AP DS:351A) with far pointers into that block and registers them
 * with codeptr_register_abs(); the library's one-instruction thunks (video/drv_thunks.c) and AP's
 * patched vectors (sim_flow, HANDOFFS C6) call through codeptr_lookup(). "C" entries take the words
 * of their far cdecl frame; "R" entries model their registers (parameter prefix = register). Pages:
 * write page drv[18], read page drv[16] (segments A000/A400/A800, cache AC00), platform/vga.h.
 * Open X5 decided: 13 writes 0 to a near int (out_ds); 9, 12, 16, 25, 26, 28, 39-48 are one no-op
 * function (retf); 19 and 24 have no known caller but are implemented as specified. */
FarPtr drv_init(u16 base, FarPtr callbacks);     /* 0/49 0000: sets drv[], row table; returns cs:00DA */
void drv_glyph1(s16 bp_y, s16 dx_x, u16 cx_rows, FarPtr es_si_glyph);    /* 1 R 0E72 */
void drv_line(s16 bx_x1, s16 cx_y1, s16 dx_x2, s16 si_y2);             /* 2 R 0753, colour drv[0E] */
void drv_sprite(u16 frame_ds, s16 x, s16 y);                             /* 3 C 0E58 = 29 with flags 0 */
void drv_copy_rect(s16 x, s16 y, s16 w, s16 h);                          /* 4 C 1103 */
void drv_get_rect(FarPtr dst, s16 x, s16 y, s16 w, s16 h);               /* 5 C 0F67 */
u32  drv_rect_size(u16 w, u16 h);                                        /* 6 C 0FCA */
void drv_put_rect(FarPtr src, s16 x, s16 y, s16 w, s16 h);               /* 7 C 0FD6 */
void drv_flip(u16 wait);                                                 /* 8 C 10AD */
void drv_nop(void);                                                      /* 9 12 16 25 26 28 39-48 */
void drv_hspan(u16 es_page, u16 di_row, s16 bx_x, s16 cx_count, u8 al_a, u8 ah_b, u16 dx_parity); /* 10 R 02B1 */
void drv_hspan_dither(u16 es_page, u16 di_row, s16 bx_x, s16 cx_count, u8 al_a, u8 ah_b, u16 dx_parity); /* 11 R 0205 */
u32  drv_bm_size(u16 list_ds, u16 out_ds);                               /* 13 C 0CD6 */
void drv_bm_layout(u16 list_ds, FarPtr buf, u32 n);                      /* 14 C 0D09 */
void drv_bm_merge_hi(FarPtr src, FarPtr dst, u32 n);                     /* 15 C 0E04 */
void drv_nib_put(FarPtr src, u16 xb, u16 y, u16 w, u16 rows);            /* 17 C 126D */
void drv_nib_or_hi(FarPtr src, u16 xb, u16 y, u16 w, u16 rows);          /* 18 C 131C */
void drv_cols_xlat(u16 es_page, u16 di_row, s16 dx_x, s16 cx_count, FarPtr ds_si_src, u16 bp_table_ds); /* 19 R 050E */
void drv_set_palette(FarPtr pal);                                        /* 20 C 0C16 */
u16  drv_get_pixel(s16 x, s16 y);                                        /* 21 C 103F */
void drv_put_pixel(s16 x, s16 y, u16 c);                                 /* 22 C 1075 */
void drv_span_copy(FarPtr es_si_spans);                                  /* 23 R 1173 */
void drv_span_copy_ofs(s16 x, s16 y);                                    /* 24 C 1252 */
void drv_poly_spans(FarPtr es_si_spans);                                 /* 27 R 0932, colours drv[0D]/drv[0F] */
void drv_sprite_flip(u16 frame_ds, s16 x, s16 y, u16 flags);             /* 29 C 1412 */
void drv_cache_blit(u16 frame_ds, s16 x, s16 y);                         /* 30 C 1FCC */
u16  drv_cache_upload(u16 list_ds);                                      /* 31 C 2067 */
void drv_cache_reset(void);                                              /* 32 C 2113 */
void drv_sprite_opaque(u16 frame_ds, s16 x, s16 y);                      /* 33 C 2128 */
void drv_fade_step(u16 first, u16 count, u16 c, u16 l);                  /* 34 C 0C58 */
void drv_dac_upload(u16 src_ds, u16 first, u16 count);                   /* 35 C 0BC2 */
void drv_span_tex(u16 base_pair_ds, u16 extra, u16 table_ds, s16 dx_x, s16 cx_count, u16 es_page, u16 di_row); /* 36 R 0334 */
void drv_span_cols(u16 es_page, u16 di_row, s16 dx_x, s16 cx_count, FarPtr ds_si_src, u16 bp_table_ds); /* 37 R 0438 */
void drv_row_copy(u16 es_page, u16 di_row, s16 bx_x, s16 cx_count, FarPtr ds_si_src, bool cf_backwards); /* 38 R 064E */
'''}
GENERATED_MARK = '/* ==== GENERATED (tools/gen_module_headers.py) below this line ==== */'
PHASE4 = A['PHASE4']
COUNTS = {}


def write(path, lines):
    if os.path.exists(path) and not FORCE:
        print('exists, not overwritten (use --force):', os.path.relpath(path, ROOT))
        return False
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', encoding='utf-8', newline='\n') as fh:
        fh.write('\n'.join(lines))
    return True


for mod in MODULES:
    units = [t for t in UNITS if UMOD[t] == mod and IFACE[t] in ('x', 'f') and UKIND[t] in ('prog', 'shared')
             and name_of(home(t)) != 'main']                  # the mains: game/game.h (ProgMain)
    path = P('aotpport', 'src', mod, mod + '.h')
    if mod == 'platform':
        units = [t for t in units if CNAME[t] not in PHASE4]      # declared by the phase-4 part
        old = open(path, encoding='utf-8').read()
        if GENERATED_MARK in old and not FORCE:
            print('exists, not overwritten (use --force):', os.path.relpath(path, ROOT))
            COUNTS[mod] = sum(1 for t in units)
            continue
        keep = old.split(GENERATED_MARK)[0].rstrip('\n')
        lines, n = header(mod, units)
        body = lines[lines.index(' */') + 1:]
        body = [l for l in body if not l.startswith('#include')]
        new = keep.split('\n') + ['', GENERATED_MARK, '/*' + CONVENTIONS[2:], ' */', '#include "platform/rtl.h"'] + body
        with open(path, 'w', encoding='utf-8', newline='\n') as fh:
            fh.write('\n'.join(new))
    else:
        lines, n = header(mod, units)
        write(path, lines)
    COUNTS[mod] = n

# ------------------------------------------------------------------------------------------- rtl.h
# Borland C++ 3.0 RTL functions called by game code (medium model). Hand-written signatures.
RTL_SIG = {
    'malloc': ('u16', 'u16 size'), 'free': ('void', 'u16 block_ds'), 'calloc': ('u16', 'u16 n, u16 size'),
    'realloc': ('u16', 'u16 block_ds, u16 size'),
    'fopen': ('u16', 'u16 path_ds, u16 mode_ds'), 'fclose': ('s16', 'u16 fp_ds'),
    'fread': ('u16', 'u16 buf_ds, u16 size, u16 n, u16 fp_ds'),
    'fwrite': ('u16', 'u16 buf_ds, u16 size, u16 n, u16 fp_ds'),
    'fseek': ('s16', 'u16 fp_ds, s32 offset, s16 whence'), 'ftell': ('s32', 'u16 fp_ds'),
    'fflush': ('s16', 'u16 fp_ds'), 'fputc': ('s16', 's16 c, u16 fp_ds'), 'fgetc': ('s16', 'u16 fp_ds'),
    'fputs': ('s16', 'u16 s_ds, u16 fp_ds'), 'fgets': ('u16', 'u16 buf_ds, s16 n, u16 fp_ds'),
    'fprintf': ('s16', 'u16 fp_ds, u16 fmt_ds, ...'), 'printf': ('s16', 'u16 fmt_ds, ...'),
    'vprintf': ('s16', 'u16 fmt_ds, u16 va_ds'), 'sprintf': ('s16', 'u16 buf_ds, u16 fmt_ds, ...'),
    'sscanf': ('s16', 'u16 s_ds, u16 fmt_ds, ...'),
    'setbuf': ('void', 'u16 fp_ds, u16 buf_ds'), 'setvbuf': ('s16', 'u16 fp_ds, u16 buf_ds, s16 type, u16 size'),
    'rewind': ('void', 'u16 fp_ds'),
    'open': ('s16', 'u16 path_ds, u16 access, ...'), 'close': ('s16', 's16 fd'),
    'read': ('s16', 's16 fd, u16 buf_ds, u16 n'), '_read': ('s16', 's16 fd, u16 buf_ds, u16 n'),
    'write': ('s16', 's16 fd, u16 buf_ds, u16 n'), '_write': ('s16', 's16 fd, u16 buf_ds, u16 n'),
    'lseek': ('s32', 's16 fd, s32 offset, s16 whence'), 'tell': ('s32', 's16 fd'),
    'access': ('s16', 'u16 path_ds, s16 mode'), 'chdir': ('s16', 'u16 path_ds'),
    'unlink': ('s16', 'u16 path_ds'), 'remove': ('s16', 'u16 path_ds'),
    'rename': ('s16', 'u16 old_ds, u16 new_ds'),
    'findfirst': ('s16', 'u16 path_ds, u16 ffblk_ds, s16 attrib'), 'findnext': ('s16', 'u16 ffblk_ds'),
    'getdisk': ('s16', 'void'), 'getdfree': ('void', 'u8 drive, u16 dfree_ds'),
    'kbhit': ('s16', 'void'), '__int86': ('s16', 's16 intno, u16 inregs_ds, u16 outregs_ds'),
    'outport': ('void', 'u16 port, u16 value'),
    'time': ('s32', 'u16 tloc_ds'), 'gettime': ('void', 'u16 time_ds'), 'getdate': ('void', 'u16 date_ds'),
    'strcpy': ('u16', 'u16 dst_ds, u16 src_ds'), 'strcat': ('u16', 'u16 dst_ds, u16 src_ds'),
    'strchr': ('u16', 'u16 s_ds, s16 c'), 'strrchr': ('u16', 'u16 s_ds, s16 c'),
    'strcmp': ('s16', 'u16 a_ds, u16 b_ds'), 'stricmp': ('s16', 'u16 a_ds, u16 b_ds'),
    'strnicmp': ('s16', 'u16 a_ds, u16 b_ds, u16 n'), 'strncpy': ('u16', 'u16 dst_ds, u16 src_ds, u16 n'),
    'strlen': ('u16', 'u16 s_ds'), 'strstr': ('u16', 'u16 s_ds, u16 sub_ds'), 'strupr': ('u16', 'u16 s_ds'),
    'strrev': ('u16', 'u16 s_ds'),
    'memcpy': ('u16', 'u16 dst_ds, u16 src_ds, u16 n'), 'memmove': ('u16', 'u16 dst_ds, u16 src_ds, u16 n'),
    'memset': ('u16', 'u16 dst_ds, s16 c, u16 n'), 'memchr': ('u16', 'u16 s_ds, s16 c, u16 n'),
    'movedata': ('void', 'u16 srcseg, u16 srcoff, u16 dstseg, u16 dstoff, u16 n'),
    '_fmemcpy': ('FarPtr', 'FarPtr dst, FarPtr src, u16 n'), '_fmemset': ('FarPtr', 'FarPtr dst, s16 c, u16 n'),
    '_fstrcpy': ('FarPtr', 'FarPtr dst, FarPtr src'), '_fstrcat': ('FarPtr', 'FarPtr dst, FarPtr src'),
    '_fstrncpy': ('FarPtr', 'FarPtr dst, FarPtr src, u16 n'), '_fstrlen': ('u16', 'FarPtr s'),
    '_fstricmp': ('s16', 'FarPtr a, FarPtr b'), '_fstrcmp': ('s16', 'FarPtr a, FarPtr b'),
    '_fstrchr': ('FarPtr', 'FarPtr s, s16 c'),
    'toupper': ('s16', 's16 c'), 'tolower': ('s16', 's16 c'),
    'itoa': ('u16', 's16 value, u16 buf_ds, s16 radix'), 'ltoa': ('u16', 's32 value, u16 buf_ds, s16 radix'),
    'atol': ('s32', 'u16 s_ds'), 'atoi': ('s16', 'u16 s_ds'),
    'qsort': ('void', 'u16 base_ds, u16 n, u16 width, FarPtr cmp'),
    'labs': ('s32', 's32 x'), 'rand': ('s16', 'void'), 'srand': ('void', 'u16 seed'),
    '_lrotl': ('u32', 'u32 value, s16 count'), 'getenv': ('u16', 'u16 name_ds'),
}
RTL_NOTE = {
    'qsort': 'Borland\'s own algorithm, bit-exact (shell_data_emd1.md §5 relies on its tie order); cmp is '
             'a far code pointer called as int far cmp(void *a, void *b) through codeptr',
    'malloc': 'Borland near heap between BSS and the stack, block addresses as the original '
              '(PORTING.md memory model); 0 = out of memory',
    'fopen': 'returns the DS offset of a Borland FILE record in DGROUP (_streams), 0 = failure; paths '
             'go through the DOS work directory mapping (host_game_path)',
    'sprintf': 'the Borland __vprinter formatting (%p, %Fs, %ld ... as the original)',
    'rand': 'Borland LCG: seed = seed * 0x015A4E35 + 1, returns (seed >> 16) & 0x7FFF',
    'time': 'host clock; DOS local time as Borland computes it (tzset/TZ default EST5EDT)',
}
rtl_units = [t for t in UNITS if UKIND[t] == 'rtl']
lines = ['#pragma once',
         '/* Borland C++ 3.0 RTL functions the game code calls (medium model), implemented once for the',
         ' * three programs in platform/rtl_*.c (port/modules.csv kind "rtl"; internals "rtl_int" only as',
         ' * far as these need them). Names: rtl_ + the Borland name.',
         ' *',
         ' * Conventions: a near pointer (char *, FILE *, void *) is a u16 DGROUP offset (*_ds); far',
         ' * pointers are FarPtr. Varargs ("...") are the words of the original stack image passed as C',
         ' * `unsigned` arguments (a long or a far pointer = two words, low word first), so __vprinter',
         ' * reads them exactly like the original; the v* forms take the DS offset of such a word list',
         ' * (va_ds, e.g. built on the emulated stack with stk_alloc). exit/atexit are prog_exit/',
         ' * prog_atexit (prog.h), getvect/setvect ivt_get/ivt_set (mem.h), the long-arithmetic and',
         ' * huge-pointer helpers (LDIV@, LXMUL@, PADD@ ... kind "rtl_op") are C operators and far_*().',
         ' * Globals (_streams, errno, _openfd, the heap) stay in mem[] at each program\'s DS offsets (G_ ids).',
         ' * Addresses: INTRO / PS / AP where the program has the function. */',
         '#include "mem.h"', '',
         '/* A qsort comparator (a far code pointer in the original) is registered with codeptr and called',
         ' * with the DS offsets of the two elements; it returns the int the original leaves in AX. */',
         'typedef s16 (*RtlCmpFn)(u16 a_ds, u16 b_ds);', '']
cats = defaultdict(list)
for t in rtl_units:
    cats[CFILE[t]].append(t)
nrtl = 0
for f in sorted(cats):
    lines.append('/* ==== %s ' % f + '=' * max(4, 92 - len(f)) + ' */')
    for t in sorted(cats[f], key=lambda t: name_of(home(t))):
        n = name_of(home(t))
        ret, plist = RTL_SIG.get(n, (None, None))
        note = RTL_NOTE.get(n, '')
        if ret is None:
            ret, plist, notes = signature(t)
            note = '; '.join(['signature from the frame'] + notes)
        lines.append('/* %s %s%s */' % (addr_text(t), n, (' — ' + note) if note else ''))
        lines.append('%s %s(%s);' % (ret, CNAME[t], plist))
        nrtl += 1
    lines.append('')
write(P('aotpport', 'src', 'platform', 'rtl.h'), lines)
COUNTS['rtl'] = nrtl
print('prototypes:', COUNTS, 'total', sum(COUNTS.values()))
