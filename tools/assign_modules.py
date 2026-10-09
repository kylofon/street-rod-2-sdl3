"""Assign every function of INTRO.EXE, PS.EXE and AP_flat.exe to the port module and C file that
implement it (phase 5, step 1). Writes port/modules.csv.

Inputs: port/symbols.csv (exe, name, owner = spec module), port/spec/*_symbols.csv (which sub-spec
documents a function), port/{intro,ps,ap}_functions.json (index + call graph), port/shared_matches.csv
(cross-program matches), the spec texts (section of each function, for the headers).

Columns: exe (intro/ps/ap, "shared" = one C function for several programs, "drv" = VGA driver entry),
address (shared: "intro:SSSS:OOOO ps:... ap:..."), name (the C name), module, c_file, kind, iface, spec
(md + section), orig_name (symbols.csv name).

Rules
  kind   prog    program-specific function (C name gets a program prefix intro_/ps_/ap_ where the
                 plain name would collide with another function; same name twice in one program:
                 _SSSS segment suffix, HANDOFFS.md N-b)
         shared  the same routine in 2-3 programs: a shared_matches.csv link with equal names (or a
                 strong match where one side is unnamed), or the same name in the same library module
                 once per program (code that differs per program branches on prog_current_id()).
                 PS 3d50/ts2 copies of AP 277a/ts2 join render3d (video_ts.md §2, §8).
         rtl     Borland C++ 3.0 RTL (segment 0000 from atexit on, minus volume.c/DAC) called from game
                 code: module platform, files platform/rtl_*.c, C name rtl_<name>, platform/rtl.h
         rtl_int RTL internals only reached from other RTL functions (part of the rtl function that
                 needs them, if at all)
         rtl_op  Borland long-arithmetic / huge-pointer helpers (LDIV@, LXMUL@, PADD@ ...): C operators
                 and mem.h far_* helpers, not functions
         c0      c0 start-up / exit machinery: prog.c / mem.h (coordinator)
         alias   the body at +1 of an `inc bp` far-frame entry (part of the entry's function)
         thunk   AP VROOMM stub segments 3C59-3EA0 (JMP FAR thunks; codeptr follows them)
         dropped AP overlay manager 36F6 (not ported, PORTING.md)
         drv     VMCODE.OVL VGA driver entries (not in the EXEs; video/drv_vga.c)
  module = the spec owner in symbols.csv (normalised: shell_ui_career -> shell_ui, platform_res ->
           platform ...); shared groups whose owners differ: render3d > video, shell_ui > sim_flow.
  c_file = tools/module_files.txt (segment / offset-range patterns), else the nearest function of the
           same module in the same segment.
  iface  = x: called from another module (or by prog.c) -> prototype in <module>/<module>.h;
           f: called only from another C file of the same module -> prototype in the same header,
           marked [file]; empty: file-local.
Headers: tools/gen_module_headers.py.

usage: python tools/assign_modules.py          (from the repository root)
"""
import csv, glob, json, os, re, sys
from collections import Counter, defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXES = ('intro', 'ps', 'ap')
MODULES = ('platform', 'video', 'sound', 'shell_ui', 'shell_data', 'sim_flow', 'sim_world', 'render3d')
P = lambda *a: os.path.join(ROOT, *a)


def norm_owner(o):
    o = (o or '').strip()
    for m in sorted(MODULES, key=len, reverse=True):
        if o == m or o.startswith(m + '_'):
            return m
    if o.startswith('platform'):
        return 'platform'
    return None


def seg(a):
    return int(a[:4], 16)


def off(a):
    return int(a[5:], 16)


# ---------------------------------------------------------------------------------------------- input
SYM = {}                                   # (exe, addr) -> row
for r in csv.DictReader(open(P('port', 'symbols.csv'), encoding='utf-8')):
    if r['kind'] == 'func':
        SYM[(r['exe'], r['address'].lower())] = r

ORIGIN = defaultdict(list)                 # (exe, addr) -> [(subspec, module, name)]
for fn in sorted(glob.glob(P('port', 'spec', '*_symbols.csv'))):
    sub = os.path.basename(fn)[:-len('_symbols.csv')]
    for r in csv.DictReader(open(fn, encoding='utf-8')):
        if r.get('kind') == 'func':
            ORIGIN[(r['exe'], r['address'].lower())].append((sub, norm_owner(r['owner']), r['name']))

FUNCS = {}                                 # (exe, addr) -> index record (or a stub for symbol-only rows)
for e in EXES:
    j = json.load(open(P('port', e + '_functions.json')))
    for f in j['functions']:
        FUNCS[(e, f['start'])] = f
for (e, a), r in SYM.items():
    if (e, a) not in FUNCS:
        FUNCS[(e, a)] = {'start': a, 'calls': [], 'callers': [], 'far': 'far' in r['type'], 'source': [],
                         'unindexed': True}

MATCH = list(csv.DictReader(open(P('port', 'shared_matches.csv'), encoding='utf-8')))


# Segment 0000 functions without a spec name, identified from the disassembly (x86dis) for this step.
# (name, module or None = RTL).  Module-less entries are Borland RTL; the others are small Dynamix
# helpers linked into _TEXT in front of the RTL (the DS:1A04 driver callback wrappers etc.).
MANUAL = {
    # Borland RTL (called from game code)
    ('ps', '0000:1c46'): ('labs', None), ('ap', '0000:1fff'): ('labs', None),
    ('ps', '0000:3523'): ('movedata', None), ('ap', '0000:3dc9'): ('movedata', None),
    ('ps', '0000:358e'): ('memmove', None), ('ap', '0000:3e34'): ('memmove', None),
    ('ps', '0000:3c66'): ('qsort', None), ('ap', '0000:450c'): ('qsort', None),
    ('ps', '0000:3e5c'): ('sprintf', None), ('ap', '0000:4741'): ('sprintf', None),
    ('ps', '0000:400a'): ('strrev', None),
    ('ps', '0000:474c'): ('_fmemcpy', None), ('ap', '0000:4abb'): ('_fmemcpy', None),
    ('ps', '0000:4794'): ('_fmemset', None), ('ap', '0000:4b03'): ('_fmemset', None),
    ('ps', '0000:4770'): ('__fmemset_core', None), ('ap', '0000:4adf'): ('__fmemset_core', None),
    ('ps', '0000:47b3'): ('_fstrcat', None), ('ap', '0000:4b22'): ('_fstrcat', None),
    ('ps', '0000:47f2'): ('_fstrchr', None),
    ('ps', '0000:4858'): ('_fstricmp', None), ('ap', '0000:4bba'): ('_fstricmp', None),
    ('ps', '0000:4899'): ('_fstrlen', None), ('ap', '0000:4bfb'): ('_fstrlen', None),
    ('ps', '0000:48b8'): ('_fstrncpy', None), ('ap', '0000:4c1a'): ('_fstrncpy', None),
    ('ap', '0000:4b61'): ('_fstrcmp', None),
    ('ps', '0000:199b'): ('N_LMOD_', None), ('ps', '0000:199e'): ('LMOD_', None),
    ('ps', '0000:19a3'): ('N_LUMOD_', None), ('ap', '0000:1d69'): ('LDIV_', None),
    ('ap', '0000:3c6a'): ('__ldivmod_core', None),
    ('ps', '0000:34a4'): ('memchr', None),
    ('ps', '0000:1fcd'): ('toupper', None),
    ('intro', '0000:22d9'): ('fputs', None), ('ps', '0000:2f5b'): ('fputs', None), ('ap', '0000:37e7'): ('fputs', None),
    ('intro', '0000:26a3'): ('fgetc', None), ('ps', '0000:3325'): ('fgetc', None), ('ap', '0000:3bb1'): ('fgetc', None),
    ('ap', '0000:1ca9'): ('gettime', None), ('ps', '0000:1959'): ('gettime', None),
    ('ap', '0000:1cbc'): ('getdisk', None), ('ap', '0000:1d1f'): ('getdfree', None),
    ('ap', '0000:2240'): ('unlink', None), ('ap', '0000:4602'): ('rename', None),
    ('ap', '0000:4865'): ('strnicmp', None),
    ('intro', '0000:2e40'): ('remove', None), ('ps', '0000:3d59'): ('remove', None), ('ap', '0000:45ff'): ('remove', None),
    # c0 abort paths (start-up messages): prog.c
    ('ap', '0000:02a5'): ('_ErrorDisplay', None), ('ap', '0000:02ad'): ('_abort', None),
    ('ap', '0000:1aec'): ('_stack_overflow', None), ('ap', '0000:1afe'): ('_stack_overflow_exit', None),
    ('ap', '0000:1b09'): ('_c0_nop', None),
    # Dynamix helpers in _TEXT
    ('ap', '0000:02db'): ('sim_tick_cb', 'sim_flow'), ('ap', '0000:02e7'): ('sim_ticks_read', 'sim_flow'),
    ('ap', '0000:02f1'): ('ps1_audio_off', 'sound'),
    ('intro', '0000:043b'): ('lib_hread', 'platform'), ('ps', '0000:02c8'): ('lib_hread', 'platform'),
    ('ap', '0000:1694'): ('lib_hread', 'platform'),
    ('ap', '0000:03ac'): ('lib_free', 'platform'), ('ap', '0000:03bc'): ('lib_strcat', 'platform'),
    ('ap', '0000:03f2'): ('lib_strcpy', 'platform'), ('ap', '0000:0420'): ('lib_strchr', 'platform'),
    ('ps', '0000:03c1'): ('lib_realloc', 'platform'), ('ps', '0000:0404'): ('lib_calloc', 'platform'),
    ('ps', '0000:0454'): ('lib_free', 'platform'),
    ('ps', '0000:032c'): ('snapshot', 'shell_ui'), ('ps', '0000:1664'): ('dac_set_regs', 'video'),
    ('ps', '0000:167b'): ('dac_get_regs', 'video'), ('ps', '0000:1692'): ('c2p_row', 'video'),
    ('ap', '0000:18fc'): ('c2p_row', 'video'), ('ap', '0000:18d6'): ('floppy_motor_off', 'platform'),
    ('ap', '0000:1a4e'): ('atc_get_reg', 'video'),
}
NOT_SHARED = {'main'}                      # same name, program-specific code
RTL_BOUND = {'intro': 0x049a, 'ps': 0x17e4, 'ap': 0x1b0a}     # first RTL module after c0 + helpers (atexit)
C0 = {'_c0_startup', '__cleanup', '__checknull', '__terminate', '_SaveVectors', '__restorezero',
      '_StartTable', '_ExitTable', '_ErrorDisplay', '_abort', '_stack_overflow', '_stack_overflow_exit',
      '_c0_nop', 'exit', '_exit', '__exit', 'atexit', 'getvect', 'setvect'}
RTL_OP = re.compile(r'^(N_)?(LDIV|LUDIV|LMOD|LUMOD|LXMUL|LXLSH|LXRSH|LXURSH|PADA|PSBA|PADD|PINA|PCMP|'
                    r'SCOPY|PSUB|PSBP)_$|^__ldivmod_core$')


def name_of(k):
    if k in MANUAL:
        return MANUAL[k][0]
    r = SYM.get(k)
    return r['name'] if r else None


def owner_of(k):
    if k in MANUAL:
        return MANUAL[k][1]
    r = SYM.get(k)
    return norm_owner(r['owner']) if r else None


# ------------------------------------------------------------------------------------ classification
def base_kind(k):
    e, a = k
    s = a[:4]
    if e == 'ap' and s == '36f6':
        return 'dropped'
    if e == 'ap' and '3c59' <= s <= '3ea0':
        return 'thunk'
    if s == '0000' and owner_of(k) is None:
        n = name_of(k) or ''
        if n in C0:
            return 'c0'
        if RTL_OP.match(n):
            return 'rtl_op'
        if off(a) < RTL_BOUND[e]:
            return 'prog'          # unnamed Dynamix helper in _TEXT: module from its callers below
        return 'rtl'
    return 'prog'


KIND = {k: base_kind(k) for k in FUNCS}
# Borland near functions with a far frame start with `inc bp`; the index lists that 1-byte entry and the
# body at +1 under the same name (sim_flow_cockpit.md intro): the body is part of the entry's function.
_byname = defaultdict(list)
for k in FUNCS:
    if name_of(k):
        _byname[(k[0], name_of(k))].append(k)
for ks in _byname.values():
    for k in ks:
        if any(x[1][:4] == k[1][:4] and off(x[1]) == off(k[1]) - 1 for x in ks):
            KIND[k] = 'alias'
RTLISH = ('rtl', 'rtl_int', 'rtl_op', 'c0')

# --------------------------------------------------------------------------------------- shared groups
parent = {}


def find(x):
    while parent.get(x, x) != x:
        parent[x] = parent.get(parent[x], parent[x])
        x = parent[x]
    return x


def union(a, b):
    ra, rb = find(a), find(b)
    if ra != rb:
        parent[max(ra, rb)] = min(ra, rb)


def accept(a, b, score, exact):
    if a not in FUNCS or b not in FUNCS:
        return False
    if KIND[a] in ('thunk', 'dropped', 'alias') or KIND[b] in ('thunk', 'dropped', 'alias'):
        return False
    if (KIND[a] in RTLISH) != (KIND[b] in RTLISH):
        return False
    if KIND[a] in RTLISH:          # named RTL functions are grouped by name below
        return not (name_of(a) and name_of(b)) and (exact or score >= 0.9)
    na, nb = name_of(a), name_of(b)
    strong = exact or score >= 0.9
    if na and nb:
        return na == nb and na not in NOT_SHARED
    return strong and a[1][:4] == '0000' and b[1][:4] == '0000'


for m in MATCH:
    ap = ('ap', m['ap']) if m['ap'] else None
    ps = ('ps', m['ps']) if m['ps'] else None
    it = ('intro', m['intro']) if m['intro'] else None
    ex = m['exact'].split()
    if ap and ps and accept(ap, ps, float(m['ps_score'] or 0), 'ps' in ex):
        union(ap, ps)
    ref = ap or ps
    if ref and it and accept(ref, it, float(m['intro_score'] or 0), 'intro' in ex):
        union(ref, it)

# Same name, same (library) module, one function per program: the same library routine even where
# the matcher paired it differently or the code differs slightly (PORTING.md: one function, branch on
# prog_current_id()). UI-toolkit modules need a match link (done above).
LINKED = {(find(k)) for k in FUNCS}
named = defaultdict(lambda: defaultdict(list))
for k in FUNCS:
    if KIND[k] == 'prog' and name_of(k) and name_of(k) not in NOT_SHARED:
        mod = owner_of(k)
        if k[0] == 'ps' and mod == 'video' and (k[1][:4] == '3d50' or '46d0' <= k[1][:4] <= '4960'):
            mod = 'render3d'                   # PS copy of the ts2 3D library (video_ts.md §2, §8)
        named[(name_of(k), mod)][k[0]].append(k)
for (n, mod), per in named.items():
    if len(per) < 2 or any(len(v) != 1 for v in per.values()):
        continue
    if mod in ('platform', 'video', 'sound', 'render3d'):
        ks = [v[0] for v in per.values()]
        for x in ks[1:]:
            union(ks[0], x)

by_name = {}
for k in sorted(FUNCS):
    if KIND[k] in RTLISH and name_of(k):
        if name_of(k) in by_name:
            union(by_name[name_of(k)], k)
        else:
            by_name[name_of(k)] = k

groups = defaultdict(list)
for k in FUNCS:
    groups[find(k)].append(k)
split = 0
GROUP = {}                                 # key -> tuple of member keys (sorted intro, ps, ap)
for root, mem in groups.items():
    per = defaultdict(list)
    for k in mem:
        per[k[0]].append(k)
    if any(len(v) > 1 for v in per.values()):
        # the same function matched twice: keep the members that carry the majority name
        names = Counter(name_of(k) for k in mem if name_of(k))
        best = names.most_common(1)[0][0] if names else None
        keep = []
        for e, v in per.items():
            v2 = [k for k in v if name_of(k) == best] or v[:1]
            keep.append(v2[0])
        for k in mem:
            if k not in keep:
                GROUP[k] = (k,)
                split += 1
        mem = keep
    t = tuple(sorted(mem, key=lambda k: EXES.index(k[0])))
    for k in mem:
        GROUP[k] = t

# ------------------------------------------------------------------------------------ module per group
# Groups whose members are owned by different specs (checked by hand, HANDOFFS.md N-a/N-c):
#  * PS 3d50 <-> AP 277a (ts 3D core): render3d (video_ts.md §2.1: "the spec is the render3d one").
#  * otherwise the library owner wins in this order.
OWNER_PRIO = ('platform', 'render3d', 'video', 'sound', 'shell_ui', 'sim_flow', 'sim_world', 'shell_data')


def group_module(t):
    own = [owner_of(k) for k in t if owner_of(k)]
    if any(k[0] == 'ap' and k[1][:4] == '277a' for k in t):
        return 'render3d'
    if not own:
        return 'platform'
    c = Counter(own)
    if len(c) == 1:
        return own[0]
    return min(c, key=OWNER_PRIO.index)


MODULE = {}
for k, t in GROUP.items():
    MODULE[k] = 'platform' if KIND[k] in RTLISH + ('thunk', 'dropped') else group_module(t)
    if KIND[k] == 'alias':
        MODULE[k] = owner_of(k) or 'platform'

# Unnamed helpers in _TEXT without a MANUAL entry: named after the one RTL function they wrap
# (the DS:1A04 driver callback table: lib_open, lib_malloc ...), module platform.
for k in FUNCS:
    if KIND[k] == 'prog' and k[1][:4] == '0000' and not name_of(k):
        callees = [(k[0], c) for c in FUNCS[k]['calls']]
        wrapped = [name_of(c) for c in callees if KIND.get(c) in RTLISH and name_of(c)]
        MANUAL[k] = (('lib_' + wrapped[0].lstrip('_')) if len(callees) == 1 and wrapped
                     else 'fn_%s_%s' % (k[0], k[1].replace(':', '_')), 'platform')
        MODULE[k] = 'platform'

# ------------------------------------------------------------------------------ RTL used by game code
GAME_CALLS = defaultdict(set)              # callee key -> caller keys
for k, f in FUNCS.items():
    for c in f['calls']:
        GAME_CALLS[(k[0], c)].add(k)
for k in sorted(FUNCS):
    if KIND[k] == 'rtl':
        t = GROUP[k]
        used = any(KIND[c] not in RTLISH + ('thunk', 'dropped') for m in t for c in GAME_CALLS[m])
        if not used:
            KIND[k] = 'rtl_int'

# ----------------------------------------------------------------------------------- implementation units
# One unit = one C function: a shared group, or one program's function.
UNITS = sorted({GROUP[k] for k in FUNCS}, key=lambda t: (EXES.index(t[0][0]), t[0][1]))
UMOD = {t: MODULE[t[0]] for t in UNITS}
UKIND = {}
for t in UNITS:
    kd = KIND[t[0]]
    UKIND[t] = 'shared' if kd == 'prog' and len(t) > 1 else kd


def home(t):
    """The member whose spec/program defines the unit (library: INTRO first; 277a group: AP)."""
    m = UMOD[t]
    for k in t:
        if owner_of(k) == m:
            return k
    return t[0]


# ------------------------------------------------------------------------------------------------ names
C_RESERVED = set('''
auto break case char const continue default do double else enum extern float for goto if inline int long
register restrict return short signed sizeof static struct switch typedef union unsigned void volatile while
abs labs div ldiv rand srand malloc free calloc realloc exit atexit abort qsort bsearch atoi atol atof strtol
strtoul strtod getenv system memcpy memmove memset memcmp memchr strcpy strncpy strcat strncat strcmp strncmp
strchr strrchr strstr strlen strtok strerror strspn strcspn strpbrk strcoll strxfrm sprintf printf fprintf
snprintf sscanf scanf fscanf vsprintf vprintf vfprintf fopen fclose fread fwrite fseek ftell fgets fputs
fgetc fputc getc putc getchar putchar puts gets rewind remove rename tmpnam tmpfile time clock difftime
mktime localtime gmtime asctime ctime strftime sin cos tan asin acos atan atan2 sinh cosh tanh exp log log10
log2 pow sqrt ceil floor fabs fmod round trunc hypot y0 y1 yn j0 j1 jn gamma lgamma tgamma signal raise
setjmp longjmp open close read write lseek access chdir mkdir rmdir unlink stat index rindex bzero bcopy
random srandom sleep select kill wait alarm pipe dup fork link getpid itoa ltoa ultoa strupr strlwr stricmp
strnicmp strrev strdup toupper tolower isalpha isdigit isspace isupper islower isalnum isprint ispunct
iscntrl isxdigit main min max swab tell eof kbhit getch putch delay sound nosound inp outp inport outport
int86 beep send recv connect bind listen accept socket shutdown poll fflush setbuf setvbuf ungetc feof ferror
clearerr perror fileno environ errno timezone daylight tzname tzset bool true false ldexp frexp modf rint
lround fmin fmax fdim fma copysign nan scalbn logb remainder cbrt erf erfc expm1 log1p nearbyint assert
offsetof NULL stdin stdout stderr isnan isinf signbit fpclassify
lin mp rd8 rd16 rd32 wr8 wr16 wr32 lds mem
'''.split())
# Declared by the coordinator's own headers (host.h, mem.h, prog.h, codeptr.h, ems.h, platform/vga.h,
# platform/platform.h): a game function of the same name must not reuse it unless it IS that function.
PORT_DECLS = set()
for h in ('host.h', 'mem.h', 'prog.h', 'codeptr.h', 'ems.h', 'platform/vga.h', 'platform/platform.h',
          'platform/plat_priv.h'):
    txt = open(P('aotpport', 'src', h), encoding='utf-8').read().split('==== GENERATED')[0]
    for line in txt.split('\n'):               # declarations and function-like macros, not comments
        if line.lstrip().startswith(('/*', '*', 'return', 'if ', 'while ', 'for ')):
            continue
        m = re.match(r'^(?:static inline |extern |_Noreturn )*[A-Za-z_][\w ]*[\s*]\**([A-Za-z_]\w*)\s*\(', line) \
            or re.match(r'^#define\s+([A-Za-z_]\w*)\(', line)
        if m:
            PORT_DECLS.add(m.group(1))
# Library functions platform.h already declares (phase 4): same name, same function -> keep the name.
PHASE4 = {'timer_install', 'timer_restore', 'timer_add', 'timer_remove', 'timer_isr', 'is_tandy',
          'kbd_install', 'kbd_restore', 'kbd_isr', 'key_read', 'key_queue_read', 'kbd_dir_bits', 'key_held',
          'key_name_char'}
C0_NAMES = {'exit': 'prog_exit', 'atexit': 'prog_atexit', 'getvect': 'ivt_get', 'setvect': 'ivt_set'}


def ident(n):
    n = re.sub(r'\W', '_', n)
    return ('_' + n) if n[:1].isdigit() else n


CNAME = {}
base = {}
for t in UNITS:
    k = home(t)
    n = name_of(k) or 'fn_%s_%s' % (k[0], k[1].replace(':', '_'))
    kd = UKIND[t]
    if kd in ('rtl', 'rtl_int'):
        base[t] = 'rtl_' + ident(n)
    elif kd == 'rtl_op':
        base[t] = 'op_' + ident(n)
    elif kd == 'c0':
        base[t] = C0_NAMES.get(n, 'c0_' + ident(n.lstrip('_')))
    elif kd == 'alias':
        base[t] = 'alias_%s_%s' % (k[0], k[1].replace(':', '_'))
    elif kd in ('thunk', 'dropped'):
        base[t] = '%s_%s_%s' % (kd, k[0], k[1].replace(':', '_'))
    else:
        base[t] = ident(n)
count = Counter(base.values())


def unique(t):
    b = base[t]
    kd = UKIND[t]
    if kd not in ('prog', 'shared'):
        return b
    clash = count[b] > 1 or b in C_RESERVED or (b in PORT_DECLS and b not in PHASE4)
    if not clash:
        return b
    if kd == 'prog':
        return t[0][0] + '_' + b                               # intro_main, ps_main, ap_main ...
    if b in C_RESERVED or (b in PORT_DECLS and b not in PHASE4):
        return 'lib_' + b
    if sum(1 for u in UNITS if base[u] == b and UKIND[u] == 'shared') > 1:
        return b + '_' + '_'.join(k[0] for k in t)
    return b                                                   # the program-only namesakes get prefixed


# HANDOFFS.md N-b: two different AP functions are both called kill_credit; the object-destroyed
# notifier gets the name the hand-off proposes, the counter keeps kill_credit.
RENAME = {('ap', '088a:054f'): 'object_destroyed_notify',
          ('intro', '0000:042d'): 'hread_far'}                # Borland-style __hread helper (not RTL)
for t in UNITS:
    if home(t) in RENAME:
        count[base[t]] -= 1
for t in UNITS:
    CNAME[t] = RENAME.get(home(t)) or unique(t)
    if UKIND[t] in ('prog', 'shared') and any(k[1][:4] in ('17bd', '24e1', '24e5') for k in t)             and not CNAME[t].startswith('lib_'):
        CNAME[t] = 'lib_' + CNAME[t]       # the Dynamix EMS library next to the port's INT 67h model (ems.h)
dup = Counter(CNAME.values())
for t in UNITS:                            # still ambiguous: same name twice in one program (HANDOFFS N-b)
    if dup[CNAME[t]] > 1:
        CNAME[t] = '%s_%s' % (CNAME[t], home(t)[1][:4])
dup = Counter(CNAME.values())
for t in UNITS:
    if dup[CNAME[t]] > 1:
        CNAME[t] = '%s_%s' % (CNAME[t], home(t)[1][5:])
assert len(set(CNAME.values())) == len(UNITS), [n for n, c in Counter(CNAME.values()).items() if c > 1]

# ------------------------------------------------------------------------- spec documents and sections
ADDR = re.compile(r'\b([0-9a-f]{4}:[0-9a-f]{4})\b')
HEAD = re.compile(r'^(#+)\s+(.*)$')
SECNUM = re.compile(r'^(\d+(?:\.\d+)*)\.?\s')
DOC = defaultdict(list)                    # address -> [(md, section, is_definition)]
NAMEDOC = defaultdict(list)                # `name` -> [(md, section, heading or table row)]
for md in sorted(glob.glob(P('port', 'spec', '*.md'))):
    mdn = os.path.basename(md)[:-3]
    if mdn in ('HANDOFFS', 'README', 'AGENT_BRIEF', 'FINISH_BRIEF'):
        continue
    sec = ''
    for line in open(md, encoding='utf-8'):
        h = HEAD.match(line)
        if h:
            txt = h.group(2).strip()
            m = SECNUM.match(txt)
            sec = ('§' + m.group(1)) if m else txt[:40]
        for n in set(re.findall(r'`([A-Za-z_]\w*)`?', line)):
            NAMEDOC[n].append((mdn, sec, bool(h) or line.startswith('|')))
        found = ADDR.findall(line.lower())
        if not found:
            continue
        first_cell = line.split('|')[1] if line.startswith('|') and line.count('|') > 2 else ''
        for a in found:
            is_def = bool(h) or (a in first_cell.lower()) or line.lstrip().startswith(('* `' + a, '- `' + a))
            DOC[a].append((mdn, sec, is_def))


def md_module(mdn):
    return norm_owner(mdn)


def spec_ref(t):
    """(md, section) of the unit's definition: a heading / first table cell in one of its module's
    specs, else the first mention there, else anywhere."""
    m = UMOD[t]
    cands = []
    for k in [home(t)] + [x for x in t if x != home(t)]:
        cands += [(d, s, df) for d, s, df in DOC.get(k[1], [])]
    for pick in (lambda c: c[2] and md_module(c[0]) == m, lambda c: md_module(c[0]) == m, lambda c: c[2],
                 lambda c: True):
        for c in cands:
            if pick(c):
                return c[0], c[1]
    for k in t:                                # by name (tables that give offsets under a segment heading)
        for d, s, df in NAMEDOC.get(name_of(k) or '', []):
            if md_module(d) == m and df:
                return d, s
    for sub, mod, nm in ORIGIN.get(home(t), []):
        if mod == m and os.path.exists(P('port', 'spec', sub + '.md')):
            return sub, ''
    if (name_of(home(t)) or '').startswith('lib_'):
        return 'video', '§7'                   # the DS:1A04 driver callback wrappers (hand-off to platform)
    return None, ''


SPEC = {t: spec_ref(t) for t in UNITS}

# ------------------------------------------------------------------------------------------- C files
# "file: patterns". Pattern = exe:seg (a whole segment = one original source module), exe:seg:lo-hi (an
# offset range; the shared library 05da is split by the map of platform.md §1.2) or exe:seg:off.
# A unit matches through any of its members; units left over take the file of the nearest function of
# the same module in the same segment (PS/AP-only library functions sit next to their INTRO twins).
FILE_TABLE = open(P('tools', 'module_files.txt'), encoding='utf-8').read()


def parse_table(text):
    rules, cur = [], None
    for line in text.split('\n'):
        line = line.split('#')[0].rstrip()
        parts = line.split()
        if not parts:
            continue
        if not line[0].isspace():
            cur, parts = parts[0], parts[1:]
        for p in parts:
            f = p.split(':')
            e, s = f[0], f[1]
            if len(f) == 2 and '-' in s:
                lo, hi = s.split('-')
                for sv in range(int(lo, 16), int(hi, 16) + 1):
                    rules.append((cur, e, '%04x' % sv, 0, 0xFFFF, 1))
            elif len(f) == 2:
                rules.append((cur, e, s, 0, 0xFFFF, 1))
            elif '-' in f[2]:
                lo, hi = f[2].split('-')
                rules.append((cur, e, s, int(lo, 16), int(hi, 16), 2))
            else:
                rules.append((cur, e, s, int(f[2], 16), int(f[2], 16), 3))
    return rules


RULES = defaultdict(list)
for r in parse_table(FILE_TABLE):
    RULES[(r[1], r[2])].append(r)


def rule_file(k, mod):
    best = None
    for f, e, s, lo, hi, prec in RULES.get((k[0], k[1][:4]), []):
        if lo <= off(k[1]) <= hi and f.split('/')[0] == mod:
            if best is None or prec > best[1]:
                best = (f, prec)
    return best[0] if best else None


RTL_FILES = [
    ('platform/rtl_heap.c', r'alloc|^free|brk|heap'),
    ('platform/rtl_printf.c', r'printf|vprinter|scanner|scanf|longtoa|itoa|ltoa|ultoa|atoi|atol'),
    ('platform/rtl_stdio.c', r'^f|^_f(get|put)|setbuf|setvbuf|rewind|openfp|getfp|fputn|flush|puts|fgets'),
    ('platform/rtl_string.c', r'str|mem|movedata|toupper|tolower|stpcpy'),
    ('platform/rtl_stdlib.c', r'qsort|rand|labs|lrotl|getenv|time|date|tzset|unixtodos|dostounix'),
    ('platform/rtl_dos.c', r'.'),
]
CFILE = {}
for t in UNITS:
    kd, m = UKIND[t], UMOD[t]
    n = name_of(home(t)) or ''
    if kd in ('thunk', 'dropped', 'alias'):
        CFILE[t] = '-'
    elif kd == 'c0':
        CFILE[t] = 'prog.c' if n in ('exit', 'atexit', '_exit', '__exit') else \
            ('mem.h' if n in ('getvect', 'setvect') else '-')
    elif kd == 'rtl_op':
        CFILE[t] = 'mem.h'
    elif kd in ('rtl', 'rtl_int'):
        CFILE[t] = next(f for f, rx in RTL_FILES if re.search(rx, n.lstrip('_'))) if n else '-'
    elif n in PHASE4 and m == 'platform':
        CFILE[t] = 'platform/timer.c' if n.startswith(('timer', 'is_tandy')) else 'platform/kbd.c'
    else:
        f = rule_file(home(t), m)
        for k in t:
            f = f or rule_file(k, m)
        CFILE[t] = f
# neighbours: the nearest assigned function of the same module in the same segment
pos = defaultdict(list)
for t in UNITS:
    if CFILE[t]:
        for k in t:
            pos[(k[0], k[1][:4], UMOD[t])].append((off(k[1]), CFILE[t]))
NEIGHBOUR = set()
for t in UNITS:
    if CFILE[t]:
        continue
    best = None
    for k in t:
        for o, f in pos.get((k[0], k[1][:4], UMOD[t]), []):
            d = off(k[1]) - o
            score = (0 if d > 0 else 1, abs(d))
            if best is None or score < best[0]:
                best = (score, f)
    CFILE[t] = best[1] if best else '%s/%s_misc.c' % (UMOD[t], UMOD[t])
    NEIGHBOUR.add(t)

# ---------------------------------------------------------------------------------- interface level
UNIT_OF = {k: t for t in UNITS for k in t}
CALLERS = defaultdict(set)                 # unit -> caller units
for k, f in FUNCS.items():
    for c in f['calls']:
        ck = (k[0], c)
        if ck in UNIT_OF and k in UNIT_OF:
            CALLERS[UNIT_OF[ck]].add(UNIT_OF[k])
IFACE = {}
for t in UNITS:
    if UKIND[t] not in ('prog', 'shared', 'rtl'):
        IFACE[t] = ''
        continue
    cs = CALLERS[t]
    if UKIND[t] == 'rtl' or any(UMOD[c] != UMOD[t] for c in cs) or name_of(home(t)) == 'main':
        IFACE[t] = 'x'
    elif any(CFILE[c] != CFILE[t] for c in cs):
        IFACE[t] = 'f'
    else:
        IFACE[t] = ''


DRV_NAMES = (['drv_init', 'drv_glyph1', 'drv_line', 'drv_sprite', 'drv_copy_rect', 'drv_get_rect',
              'drv_rect_size', 'drv_put_rect', 'drv_flip', 'drv_nop', 'drv_hspan', 'drv_hspan_dither',
              'drv_nop', 'drv_bm_size', 'drv_bm_layout', 'drv_bm_merge_hi', 'drv_nop', 'drv_nib_put',
              'drv_nib_or_hi', 'drv_cols_xlat', 'drv_set_palette', 'drv_get_pixel', 'drv_put_pixel',
              'drv_span_copy', 'drv_span_copy_ofs', 'drv_nop', 'drv_nop', 'drv_poly_spans', 'drv_nop',
              'drv_sprite_flip', 'drv_cache_blit', 'drv_cache_upload', 'drv_cache_reset', 'drv_sprite_opaque',
              'drv_fade_step', 'drv_dac_upload', 'drv_span_tex', 'drv_span_cols', 'drv_row_copy']
             + ['drv_nop'] * 10 + ['drv_init'])


def addr_field(t):
    if len(t) == 1:
        return t[0][1]
    return ' '.join('%s:%s' % k for k in t)


def main():
    rows = []
    for t in UNITS:
        exe = 'shared' if len(t) > 1 else t[0][0]
        md, sec = SPEC[t]
        rows.append((exe, addr_field(t), CNAME[t], UMOD[t], CFILE[t], UKIND[t], IFACE[t],
                     ('%s.md %s' % (md, sec)).strip() if md else '', name_of(home(t)) or ''))
    for i, n in enumerate(DRV_NAMES):     # VMCODE.OVL VGA driver entries (video.md §3), not in the EXEs
        rows.append(('drv', 'entry%02d' % i, n, 'video', 'video/drv_vga.c', 'drv', 'f', 'video.md §3', ''))
    with open(P('port', 'modules.csv'), 'w', newline='', encoding='utf-8') as fh:
        w = csv.writer(fh)
        w.writerow(['exe', 'address', 'name', 'module', 'c_file', 'kind', 'iface', 'spec', 'orig_name'])
        w.writerows(rows)
    c = Counter((r[3], r[0]) for r in rows if r[5] in ('prog', 'shared', 'rtl', 'drv'))
    print('%-11s %6s %6s %6s %6s %6s' % ('module', 'intro', 'ps', 'ap', 'shared', 'drv'))
    for m in MODULES:
        print('%-11s %6d %6d %6d %6d %6d' % ((m,) + tuple(c[(m, e)] for e in EXES + ('shared', 'drv'))))
    print('kinds:', dict(Counter(r[5] for r in rows)))
    print('iface x/f:', dict(Counter(r[6] for r in rows if r[6])))
    print('files: %d, by neighbour: %d, misc fallback: %d' % (
        len({r[4] for r in rows if '/' in r[4]}), len(NEIGHBOUR),
        sum(1 for t in UNITS if CFILE[t].endswith('_misc.c'))))


if __name__ == '__main__':
    main()
