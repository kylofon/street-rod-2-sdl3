# race_render — Street Rod 2 (SR.EXE): road renderer, scenery, SR2_SHP shapes, opponent sprite, Aqueduct renderer, pages

Delta spec on top of the SR1 spec **../StreetRod/port/spec/race_render.md** (SR1 segment `2645`; read its §1 overview
and §4 pseudocode for every "= SR1" line below). Addresses are file addresses `SSSS:OOOO` (Ghidra shows segment
+0x1000: `FUN_1dcb_4154` = **0dcb:4154**). `DS:xxxx` = DGROUP 0x3E08.

**Scope.** All of code segment **`0dcb`** (SR1 `2645`) plus `0001:d2a8`, the SR2_SHP loaders `1a21:7304/7446/75ea`
and the small helpers `1a21:9b96/9bac/a6e8`. Physics, AI, police and race logic belong to **race** (port/spec/race.md);
they are referenced here by address only.

**Counts:** 134 functions specified: **28 unchanged** (one-line "= SR1" reference with DS remap, possibly with a short
list of small deltas), **37 changed** (only the difference vs SR1 is described), **68 new** (full spec), plus one
non-function index entry (`0dcb:2350`). Matcher classes of the same rows (port/sr2_sr1_matches.csv): see the symbols file
notes; several matcher pairings were corrected (e.g. `507a` = SR1 `3a0d` town_walls_front_right, `50f4` = SR1 `3a86`
town_walls_mirror_right, `573e` is the hood overlay, not SR1 mirror_draw_background).

Sources: decompile port/decomp/sr2_ds.c, checked with `tools/x86dis.py`; call-graph summary from port/sr2_functions.json.

-----------------------------------------------------------------------------------------------

## 1. Overview

### 1.1 Road mode `DS:ADD6` (one table for the whole module)

Set by `challenge` 0001:9e3c (by the opponent's preferred type `DS:47A6` 0/1/2 → 1/0x10/8, or the menu "Drag race" /
"Mulholland drive" / "Aqueduct race" → 1/0x10/8; the stake menu "Just for kicks / $150 / Pink slips" is independent),
by `event17_run` 2925:1970 (Grudge Night: 2 for the breakout-time run, 4 for the rounds), and set to 0 by
`drive_to_event` 0001:8c68 / 2925:193e. Start / finish segments `DS:9AE2` / `DS:A2AA` are set by race 0001:bc88.

| ADD6 | Race | Start/finish seg | Renderer and special behaviour |
|---|---|---|---|
| 0 | town cruise / event trips (no race) | 200 / – | normal renderer (6a32 / 5aee), SR2_SHP list `DS:4BD6` |
| 1 | street **drag race** (msg 62 "You're on! Let's hit the drag race") | 200 / 0x17C | normal renderer, list `DS:4BEE` |
| 2, 4 | **Grudge Night** drag strip (2 = breakout-time run, 4 = rounds); test `ADD6 & 6` | 8 / 0x58 | normal renderer on the drag-strip flag arrays `L2/R2/C2` (track index +0xC0), flat (dy = 0), barrier walls 597e, start tree aab0 / starter figure a85c, list `DS:4C16`; opponent not drawn in mode 2 |
| 8 | **Aqueduct race** (msg 64 "Cool, dude! Get your pink slip out," is the accept text of this race type; prerequisite text "Aqueduct Races") | 8 / 0x156 | **separate renderer**: 18ec / a094 (front), 1b02 / 9ce8 (mirror): concrete channel of filled polygons, roll rotation, `AQ` flags from 33ac, wall bounce a1c0; graphics by 8840 (rotating opponent sprites); only SR2_SHP format-B sets 23..25 |
| 0x10 | **Mulholland Drive** (msg 63 "Let's hit the mulholland drive"); test `ADD6 & 0x10` | 0x172 / 0x3C0 | normal renderer + hillsides (bank arrays BL/BR), sky 3cca, truss bridges (C&0x80) 5404/555e, guard rails, rocks, cliff fall 7da2, shoulder 200 instead of 400, list `DS:4C30` (adds sets 20..22) |

`DS:B7AE` (u8) = 1 while racing (race_start 0001:be82), 0 when cruising.

### 1.2 Frame order (`road_step` 0dcb:1774 → `road_frame` 0dcb:11fc, once per frame from `race_run` 1a21:77f4)

Modes ≠ 8: `road_set_seg_ptrs` → `road_project_front` 07e4 → `road_draw_front` 6a32 (sky, bands, walls, scenery,
opponent) → `road_set_seg_ptrs` → `road_project_mirror` 0ce4 (overwrites rows 0..7 of the **same** row tables) →
`road_draw_mirror` 5aee → `road_fillmode_end` → hood overlay 573e → (racing && !(ADD6&6)) starter slide a746 →
cockpit overlay 56e8 (A-pillar LIB3 #2003) → `road_page_flip` 354e → `pace_ticks(8)` (1a21:1924; SR1 busy-waited 9 ticks).
Mode 8: 18ec (project front) → a094 (draw front) → 1b02 (project mirror) → 9ce8 (draw mirror) → same tail.
Load time (race_run): `ADD6 == 8` → 0dcb:8840, else `road_load_graphics` 37ce; then `shp_load_set` 1a21:75ea.

### 1.3 Screen, pages, clip, driver slots

Common SR1 → SR2 DGROUP remap used by every "= SR1" line below (porter: rename these in the SR1 C code):
clip `5E4E/5E50/5E52/5E54` (xmin/ymin/ymax/xmax) → `7E12/7E14/7E16/7E18`; draw segment `787A` → `9DC2`;
GC function word `5C1E` (high byte `5C1F`) → `7B9A` (`7B9B`); video driver id `8236` → `AD1C`; back page number
`5E3E` → `7E02`; page pointers `7678` (page 0, dashboard) → `9BC2`, `8188` (page 1) → `AD0C`, `82B4` (page 2) →
`ADA2`; driver slots (active table `DS:A0AE`, 20 far ptrs, §RE_GUIDE 3.7): blit `78A2` → `A0AE` (slot 0),
masked blit `78AA` → `A0B6` (slot 2), line `78D2` → `A0DE` (slot 12), hline `78D6` → `A0E2` (slot 13), fill rows
`78DA` → `A0E6` (slot 14); **new** slots: `A0F2` (slot 17) set_colour(c) (EGA `2f32:1932`: GC index 0 (set/reset) = c),
`A0F6` (slot 18) fill_polygon(Point *pts, int n, int fill, int edge) (`33f8:01ca` EGA), `A0FA` (slot 19) plot_points (below).
Row pointer `6E28` (dash width) → `96EE`, `8B82` (centre flags) → `96F0`.

**Polygon slot `A0F6` (resolved from 33f8:01ca):** `far void fill_polygon(Point *pts, int n, int fill, int edge)`;
`pts` = n × {s16 x, s16 y} (near, DS). The EGA/VGA routine fills the interior spans of every scan line with
**arg 3 = fill colour** (Set/Reset before each span, `33f8:01a2`) and plots the edge pixels with **arg 4 = edge colour**
(`33f8:0255/0267`). So `A0F6(o,n,5,2)` = concrete (5) with edge colour 2; `A0F6(o,n,c,c)` = solid. Callers clip first
with `clip_polygon` 0dcb:7f30. CGA/Tandy versions (3422:025d, 3458:0228) are parked.

Pages (VGA `DS:AD1C == -2`): page 0 A000 (`DS:9BC2`, dashboard / off-screen storage), page 1 A200 (`DS:AD0C` → `DS:7DDA`),
page 2 A400 (`DS:ADA2` → `DS:7DEC`); back page number `DS:7E02` (1 = A200), shown page `DS:7E04`, back-page video
segment `DS:9DC2`. Flip = swap + CRTC start (`2f32:1956`, vsync). Non-VGA: copy rect `DS:7E06` = {320,82,0,18,0,18}
from page 1 via `[A0AE]`. Clip `DS:7E12/14/16/18` (xmin, ymin, ymax, xmax): view = (0,18)-(319,101) (clip_set_view),
mirror = (240,22)-(319,42) (clip_set_mirror; SR1 242..317).

### 1.4 Track (far segment 3726), segments, sub-steps

Conventions as SR1 §4.1 (`int`=s16, 16-bit wrap, `/` = idiv truncating, `>>` = sar). `rnd(n)` = `1a21:6180(n)` (game_flow:
LCG in `DS:4A84` (u32), returns `(hi15 % n)`; `n < 1` reseeds from `time()` and returns −1). All track arrays are word
arrays in far segment **`3726`** (file segment; Ghidra shows `4726`); the code loads ES from the constant words
`DS:86EC…8710, 871E` (all `0x3726`), or uses the immediate `0x3726`. Array names used below:

| far addr | name | entries | meaning (SR1 eq) |
|---|---|---|---|
| 3726:27B6 | `L[i]` | 0x3DE | left side flags (SR1 `389b:33F0`) |
| 3726:2F72 | `R[i]` | 0x3DE | right side flags (`389b:3CB0`) |
| 3726:372E | `YAW[i]` | 0x3DE | road heading (`389b:4570`) |
| 3726:3EEA | `C[i]` | 0x3DE | centre flags (`389b:4E30`) |
| 3726:46A6 | `HGT[i]` | 0x3DE | road height (`389b:56F0`) |
| 3726:4E62 | `AQ[i]` | 0x172 | **new** Aqueduct segment flags (mode 8 only), built by `33ac` |
| 3726:5146 | `BL[i]` | 0x3DE | **new** left bank height: >0 hill/wall rising beside the road, <0 drop-off (cliff/water) |
| 3726:5902 | `BR[i]` | 0x3DE | **new** right bank height |
| 3726:60BE / 61A6 / 6572 | `L2/R2/C2[k]` | 0x74 | **new** drag-strip flags (modes 2/4): copies of `L/R/C[0xC0+k]`, `C2 |= 8` |
| 3726:628E | `AQM[i]` | 0x172 | **new** `AQ` reversed: `AQM[i] = AQ[0x171-i]` (mirror walk of mode 8) |
| 3726:665A | `LANE[i]` | 0x3DE | **new** lane index 1…16 of a road obstacle (race road, set with `C&4` / `C&0x1000`) |

Segment index: `seg` = `DS:A55E` (0…0x3DD). One segment = **64 sub-steps** (`DS:A560`, 0…63; SR1 had 8): every SR1
"sub-step" quantity is ×8 in SR2 (`A562` town speed 0x20/0x30 = SR1 4/6; projections use `A560/8`).

### 1.5 Row tables and row pointers of the normal renderer

Conventions used below (all SR2 DGROUP offsets):

- Row tables (15 × s16, index = row, written by `road_project_front` 07e4 for rows 0..14, then **overwritten**
  for rows 0..7 by `road_project_mirror` 0ce4 — the mirror reuses the same arrays):
  `XL` 9A5C, `XR` A2E2, `Y` 9EF0 (road y), `YE` A300 (y at eye height 0x76C), `Y79E` A7EA (y at world height 0x79E),
  `XOL` AB2E, `XOR` AB5E (outer edges), `YT1` A5A8 / `YT2` A7AE (y of `2000 - T1[seg]` / `2000 - T2[seg]`:
  left / right terrain height), `XFL` ADB2 / `XFR` B89E (far-left / far-right x).
- Row pointers (near, into those tables; `p` = current row, `q` = row-1):

| ptr p | ptr q | table | front start (row 14) p / q | mirror start (row 7) p / q |
|---|---|---|---|---|
| 96B6 | 96BE | XL | 9A78 / 9A76 | 9A6A / 9A68 |
| 96CE | 96D4 | XR | A2FE / A2FC | A2F0 / A2EE |
| 96D0 | 96D8 | Y | 9F0C / 9F0A | 9EFE / 9EFC |
| 96D2 | 96D6 | Y79E | A806 / A804 | A7F8 / A7F6 |
| 96DA | 96CC | YT1 | A5C4 / A5C2 | A5B6 / A5B4 |
| 96DC | 96DE | YT2 | A7CA / A7C8 | A7BC / A7BA |
| 96C6 | 96E0 | XFL | ADCE / ADCC | ADC0 / ADBE |
| 96C8 | 96CA | XFR | B8BA / B8B8 | B8AC / B8AA |
| 96B8 | 96C0 | XOL | AB4A / AB48 | AB3C / AB3A |
| 96BC | 96C4 | XOR | AB7A / AB78 | AB6C / AB6A |
| 96BA | 96C2 | YE | A31C / A31A | A30E / A30C |
| 96EE | — | dash width (s16 table 7E52, index = row) | 7E6E | 7E7A |

  All near pointers step **−2 bytes per row** (front and mirror). Far flag pointers (seg = track segment 4726):
  `96E2` = &L, `96E6` = &R, `96F0` = &C, `96EA` = &X6 (per-segment obstacle lateral position, array 4726:665A).
  Front: `far_ptr_of_seg_array + 0x1C` (= element seg+14), −2 per row. Mirror: `−0x10` (= element seg−8), **+2**
  per row (mirror row r reads flags of seg−r−1), as SR1. Below `L,R,C,X6` = `*96E2,*96E6,*96F0,*96EA`,
  `Cm1` = `96F0[-1]`, `Cp1` = `96F0[+1]`. `XL` alone means `XL[p]`.
- Callees named only: `shape(x,y,depth,side,type,flag)` = `scenery_draw_shape` 5158; `hydrant(x,y,depth,side)` = 51aa;
  `obj51e0(x,y,depth,side)` = 51e0; `sign_picture` 52e2 (SR1 3b79 args); `banner` 55ce (SR1 3ccc args);
  `tunnel_5404(10 args)`, `tunnel_555e(4 args)`; `poly_spans` = clip_polygon 7f30 `(Point *in, int n, Point *out /* buf */, int *nout) → int`;
  `line` = 7de4 `(x1,y1,x2,y2,colour)`; `traffic_draw` = af98 (no args); aab0 `(x,y,row)`; a85c `(x,y,row)`.
  Driver vectors: `A0F2(colour)` set colour, `A0E2(x1,x2,y)` hline, `A0E6(y,colour)` fill rows y..ymax,
  `A0F6(pts,n,fill,edge)` polygon fill (§1.3), `A0AE(src far, dst far, Rect*, 1)` blit.
  `BACK` = (`DS:7E02 == 1`) ? `DS:AD0C` (page 1) : `DS:ADA2` (page 2). Clip: 7E12 xmin, 7E14 ymin, 7E16 ymax, 7E18 xmax.
- `seg` = DS:A55E, `heading` = A56A, `cam_h` = A56C, `in_race` = byte B7AE, `mode` = **DS:ADD6** (road mode):
  0 cruising / ordinary race, 1 drag race (msg 62), 2 and 4 Grudge Night drag strip (event17_run; "flat" mode: `mode & 6`
  ⇒ no hills, track arrays indexed seg+0xC0, barrier walls), 8 Aqueduct race (msg 64; drawn by a094/9ce8, not
  here), 0x10 Mulholland Drive (msg 63; hillsides, tunnels, guard rails, cliff fall).
  `cpu` = DS:B7B4 (cpu_speed_calibrate; ≤ 3 = slow machine: palms, trees, bushes skipped).
  `start_seg` = DS:9AE2 (200 modes 0/1, 8 modes 2/4/8, 0x172 mode 0x10), `opp_seg` = DS:A9C6,
  `fall_side` = DS:7B9C (Mulholland: −1/+1 = car went over the left/right edge, else 0), `fallen` = DS:7F2C.

### 1.6 Aqueduct (mode 8) renderer and opponent sprite conventions

Conventions (as SR1 §4.1): `Rect {w,h,srcx,srcy,dstx,dsty}`; clip rect `DS:7E12` xmin, `7E14` ymin,
`7E16` ymax, `7E18` xmax (SR1 5E4E/5E50/5E52/5E54); `BACK` = `DS:7E02==1 ? DS:AD0C:AD0E : DS:ADA2:ADA4`
(back-page descriptor). Graphics slots (`DS:A0AE + 4·n`, EGA/VGA table `DS:7B38`):
`A0B6` = ega_blit_masked(src,dst,&r,1); `A0DE` = ega_line(x1,y1,x2,y2,c); `A0E2` = ega_hline(x1,x2,y)
(SR2: 3 args, colour from the last `A0F2`); `A0E6` = ega_fill_rows(y,c) (rows y…clip ymax);
`A0EA` = 0dcb:b8c8 shrink_pic(pic far, mask) → new pic; `A0EE` = 0dcb:bd12 rotate_pic(pic, angle, -1,
&ox8, &oy8, cx) → new pic + i8 offsets; `A0F2` = set_colour(c); `A0F6` = 33f8:01ca
poly_fill(pts,n,fill,edge) (arg 3 = interior fill colour, arg 4 = edge-pixel colour; §1.3); `A0FA` = 0dcb:0006 plot_points(dst_off,dst_seg,xoff,pts,n,planemask)
(each point (xoff+px,py), rotated by `rot()` if `DS:9E70≠0`, clipped, one pixel). Helpers owned elsewhere
in race_render: `0327 lerp_x(x1,y1,x2,y2,y)` (x on the line at y; returns x2 if y1==y2), `0350
lerp_y(x1,y1,x2,y2,x)`, `0379 rot(x,y,&rx,&ry)`, `02b4 isin(deg)` (byte table DS:1504, 91 entries, sin·256
capped 255), `7de4 line_clip(x1,y1,x2,y2,c)` (clip to clip rect, then `A0DE`), `7f30 poly_clip(in,n,out,&nout)`
(Sutherland–Hodgman against the clip rect, returns nout>0).

```c
/* 0379: roll rotation about the pivot (DS:A7A8, DS:A7CC); cos = DS:BA90, sin = DS:A540 (×256) */
rx = ((x-A7A8)*BA90 >> 8) + ((y-A7CC)*A540 >> 8) + A7A8;
ry = ((y-A7CC)*BA90 >> 8) + ((-(x-A7A8))*A540 >> 8) + A7CC;      /* 32-bit products, >>8 arithmetic */
```

**Mode-8 row arrays** (15 words each, index `i` = row, 0 = nearest; shared by front and mirror, the mirror
uses i = 0…7). Raw (unrotated) projections, written by `18ec`/`1b02`/`a2d8`:

| DS | row value |
|---|---|
| 9EF0[i] | floor y |
| 9A5C[i] / A2E2[i] | floor left / right edge x |
| A5A8[i] | channel "centre" x (see quirk in 1b02) |
| A300[i] | wall-top y |
| AB2E[i] / AB5E[i] | wall-top left / right x |

Aliases used by the code: `DS:9A5A`=9A5C[−1] (also a scalar, see 9d9c), `A2E0`=A2E2[−1], `9EEE`=9EF0[−1],
`A2FE`=A300[−1] **and** A2E2[14], `A5A6`=A5A8[−1], `9A78`=9A5C[14], `9F0C`=9EF0[14], `9A6A`=9A5C[7],
`A2F0`=A2E2[7], `9EFE`=9EF0[7]. In C use separate arrays and write `X[i-1]`.
Rotated arrays (written by `a094`/`9ce8`, **reversed**: index `k = N − i`, N = `DS:B194` = 14 front / 7 mirror,
so k = 0 is the farthest row): floor-left `9732`(x)/`9760`(y), floor-right `B170`/`B1A0`, wall-top-left
`9BC6`/`9CFC`, wall-top-right `9DCE`/`9F0E`.

**Mode-8 segment flags** `F(i)`: far word pointer `DS:96F6:96F8`; row k reads word `[96F6 − 2k]`.
Front: `F(i) = C[seg+i]`, mirror: `F(i) = C[seg−1−i]`, with `C` = far word array `3726:4E62` (track flags,
built for mode 8 by `0dcb:33ac`, which also stores the reversed copy at `3726:628E` used by the mirror
pointer). Bits (as used here): 0x0001 cross joint lines, 0x0002 no centre trough, 0x0004/0x0100 left wall
opening (upper/lower), 0x0008/0x0200 right wall opening, 0x0010 low box, 0x0020/0x0040 SR2_SHP object at
¼ / ⅞ of the floor, 0x0080 pair of pillars, 0x0400 white floor band.

### 1.7 Race graphics, SR2_DATA blocks 7 and 8

**Resolved meaning (supersedes FORMATS.md "road graphics"):** block 8 (`DS:AA9E`) is the per-opponent-class **sprite id
table** (SR1 `DS:5E14`) with near pointers to block-7 records; block 7 (`DS:8E3C`) holds the **shrink masks** (SR1
`DS:5E56/5A/5E`) for 7 viewing angles × 3 sizes per record. Both are used in **all** race modes: the straight-ahead
modes take the angle-3 (0°) masks at load time (road_load_graphics 37ce, police too); only the Aqueduct (mode 8) uses the
other 6 angles, per frame, via `ad40` (rotated opponent sprites built by 8840).

Conventions (all functions below): `pic_get(id, mode)` = 1a21:6b04 (id ≥ 2000 → LIB3 #id−2000; all pictures
loaded here are LIB3). Allocation modes (arena_alloc 1a21:99d2): 0 permanent, **1 = low mark stack**
(released by `heap_pop_lo(n)` = 1a21:9b40), **2 = high mark stack** (released by `heap_pop_hi(n)` = 1a21:9b56),
3 top, **−1 = bump allocation in the SR2_SHP arena** (`DS:9E6C:9E6E + DS:79E0`, `DS:79E0 += size`; race_run
sets `DS:79E0 = 0` just before, and `shp_load_set` appends the shape sets after these pictures).
Driver vectors (copied to DS:A0AE): slot 0 `DS:A0AE` blit(src, dst, Rect*, 1); slot 2 `DS:A0B6` masked blit;
**slot 15 `DS:A0EA` = shrink(Pic far*, u8 mask)**; **slot 16 `DS:A0EE` = rotate(Pic far*, angle, mode, s8 *ax,
s8 *ay, pivot_x)**. Rect = `{w, h, srcx, srcy, dstx, dsty}` (as SR1). Pic header (as SR1): `+0 w, +2 h,
+4 data bytes (all planes), +6/+8 data far, +10/+12 mask far (0:0 = none), +14 bytes per row`; header 0x14
bytes. Pages: `DS:AD0C:AD0E` (sky/horizon page, SR1 DS:8188), `DS:ADA2:ADA4` (off-screen page, SR1 DS:82B4),
`DS:9BC2:9BC4` (VGA third page, SR1 DS:7678). `DS:AD1C` = video mode (−2 VGA/EGA, −6 Tandy, else CGA/Herc).

Race-type globals used here: `DS:B7AE` u8 = 1 while racing (set by race_start 0001:be82, 0 in drive_to_event =
cruising); `DS:ADD6` race type: 0 cruise/event trip, 1 drag race (msg 62), 0x10 Mulholland drive (msg 63),
8 Aqueduct race (msg 64; uses rotating opponent sprites + SR2_SHP format-B sets), 2/4 the two races of
event17; `DS:9B0A` opponent car model index; `DS:9730` = demo flag (copy of `DS:B8D0`, "demo" switch).

#### SR2_DATA block 8 (`DS:AA9E`, 6 × 14 bytes) — opponent sprite sets (SR1 `DS:5E14` + `5E56/5A/5E`)

Index `cls = (s8) block2[DS:9B0A].byte0` (byte 0 of the opponent model's 10-byte overlay record at DS:BAAC,
SR1 `DS:8DF0 + 10*DS:7648`); row 5 = police, pointed to by `DS:AAFA` (= 0xAAE4, set by hot_data_load).

| off | type | field | SR1 equivalent |
|---|---|---|---|
| +0 | u16 | side view id (level 0) | `5E14[cls][0]` |
| +2 | u16 | rear 3/4 id (level 1) | `[1]` |
| +4 | u16 | rear id (level 2; scaled to levels 3..8) | `[2]` |
| +6 | near ptr | → mask record A (block 7) for levels 3,4,5 (rear ÷) | `DS:5E56` |
| +8 | near ptr | → mask record B for levels 6,7,8 (level-5 picture ÷) | `DS:5E5A` |
| +10 | u16 | front id (level 9; scaled to 10..12) | `[3]` |
| +12 | near ptr | → mask record C for levels 10,11,12 | `DS:5E5E` |

The pointers in the file are stale (0x0CF6 + 22·k, the record addresses of the authoring build); **hot_data_load
1a21:6e6a overwrites them**: `row[i].+6 = 0x8E3C + 66·i`, `+8 = +6 + 22`, `+12 = +6 + 44` (i = 0..5), i.e. row i
uses block-7 records 3i, 3i+1, 3i+2; then `DS:AAFA = 0xAAE4`. File values (LIB3 #, size):

| row | side | rear 3/4 | rear | front | records |
|---|---|---|---|---|---|
| 0 | 2082 #82 104×82m | 2001 #1 120×46m | 2006 #6 96×46m | 2007 #7 40×18m | 0,1,2 |
| 1 | 2010 #10 104×82m | 2040 #40 112×45m | 2041 #41 88×45m | 2008 #8 40×18m | 3,4,5 |
| 2 | 2014 #14 104×83m | 2042 #42 120×46m | 2043 #43 96×46m | 2044 #44 40×18m | 6,7,8 |
| 3 | 2014 | 2046 #46 120×46m | 2045 #45 96×46m | 2044 | 9,10,11 |
| 4 | 2015 #15 104×86m | 2016 #16 120×46m | 2017 #17 96×45m | 2018 #18 40×18m | 12,13,14 |
| 5 police | 0 (uses opponent side) | 2035 #35 120×52m | 2036 #36 96×55m | 2039 #39 40×22m | 15,16,17 |

(police: rear 3/4 = lights picture 1, rear = picture 2, as SR1 1241/1242/1246.)

#### SR2_DATA block 7 (`DS:8E3C`, 18 × 22 bytes) — shrink masks per viewing angle

Not bitmaps: each 22-byte record = **7 angles × 3 mask bytes + 1 pad byte (0)**. Byte `3·a + j` is the
row/column keep-mask (bit `0x80 >> (i & 7)` keeps row/column i, same mask for x and y) for angle `a`
(0..6 = −45°, −30°, −15°, 0°, +15°, +30°, +45°, table `DS:7FBE` = −45 −30 −15 0 15 30 45) and size step `j`.
The masks of one record have the same bit count for every angle (different dither patterns), so the scale
factor depends only on (record, j):

| record | role | popcount j=0,1,2 → scale |
|---|---|---|
| A (3i) | rear → levels 3,4,5 | 6,4,3 → 6/8, 4/8, 3/8 (SR1 5E56: 5,3,2) |
| B (3i+1) | level-5 picture → levels 6,7,8 | 6,4,3 → ×6/8, 4/8, 3/8 of level 5 (SR1 5E5A: 7,5,3) |
| C (3i+2) | front → levels 10,11,12 | 5,3,2 → 5/8, 3/8, 2/8 (SR1 5E5E: 5,3,2) |
| 15,16,17 (police) | only angle-0 bytes used | 15: f6 aa a4; 16: f3 b2 4a (unused); 17: 4f 46 22 |

Bit counts: rec 0,1,3,4,6,7,9,10,12,13 and 15,16 = 6,4,3; rec 2,5,8,11,14,17 = 5,3,2 (one data quirk: rec 6
angle 6 byte 0 = 0xFB has 7 bits). Records 11 and 8 are byte-identical; records 15–17 have only bytes 0..2.
Readers: straight-ahead modes use the **angle-3 triple (bytes 9..11)** (road_load_graphics; 8840 uses
`A[9]` and `A[11]`); the rotating mode reads `rec[3·a + j]` per frame in `0dcb:ad40` (§3) and the
angle-0 triple in `86d2`; police reads `rec15[0]` and `rec17[0..2]`.
Dump: `python tools/sr2data.py` block 7; values above from `Game/SR2_DATA` 0x73A.

Opponent sprite table (SR1 `DS:8ACE[13]`) in SR2 = `DS:B7B6[13]` far ptrs (B7B6 side, B7BA 3/4, B7BE rear,
B7C2/C6/CA levels 3–5, B7CE/D2/D6 6–8, B7DA front, B7DE/E2/E6 10–12); police table (SR1 `DS:7526[13]`) =
`DS:9A94[13]`. Mode 8 uses instead: `DS:9E04` side, `DS:9E64` 3/4, rotated sets `DS:9E72[7]` (level 2),
`DS:A1C6[7]` (level 5), `DS:A320[7]` (level 9) — element 3 (0°) is the unrotated picture (`9E7E`, `A1D2`,
`A32C`); levels 3,4 / 6–8 / 10–12 are shrunk on the fly by ad40.

### 1.8 SR2_SHP vector span shapes

Loaded by `shp_load_set` 1a21:75ea (sections 1a21:7304 / 7446 / 75ea below): directory `DS:80A2` (26 × {u32 offset,
u16 size, u16 0}), id lists per mode (table in 1.1), mode 8 = format B (sets 23..25, `DS:81DA` first id, `DS:81DC`
count). Format-A sets are drawn by `shape_draw` **0dcb:4154** (scenery 5158/51aa/51e0, banner text, UI dot 512e);
format-B groups by `0dcb:8288` (Aqueduct). LOD record = 6 × {u8 colour, u8 count} (12 bytes) + far pointer to its
spans (+12) = 16 bytes, **no pad**; span = {s8 dx, s8 dy, s8 w}. LOD index = clamp(9 − depth, 0, 6) (0..7 for set 2),
`lod >= n_lods` draws nothing. Shape set id = SR1 shape type number for 0..19.

## 2. Globals

DGROUP globals owned (or first defined) by this module. SR1 equivalents from ../StreetRod/port/spec/race_render.md §3.2.

| DS | type | name | meaning | SR1 DS eq |
|---|---|---|---|---|
| DS:1504 | u8[91] | sin_table | sin 0…90° × 255 | — |
| DS:4BBE | u16 | shp_handle | SR2_SHP DOS handle (0 = not open) | — |
| DS:4BC0 | char[10] | shp_name | "?:sr2_shp", [0] patched with the drive letter | — |
| DS:4BD6/4BEE/4C16/4C30 | s16[] | shp_ids_mode0/_mode1/_mode2_4/_mode16 | −1-terminated set id lists | — |
| DS:78E0 | {u16 id,x,y}[6] | hood_pics | per dash type: LIB3 id and position of the hood overlay | — |
| DS:7B9C | s16 | fall_dir | Mulholland fall-off request ±1 | — |
| DS:7BA0 / 7B9E | u16 | nudge_left_latch / nudge_right_latch | town heading nudges | 5E12 / 5E10 |
| DS:7BA2 | u16 | on_shoulder | set by 148a | 5E0E |
| DS:7C1C | s16[17] | hill_profile_17 | first-stretch hills | 5C98 |
| DS:7C3E | s16[10][13] | course_layouts | feature codes | 5CBC |
| DS:7D42 | s16[61] | course_height_deltas | HGT delta per 10 segs from 0x17C | — |
| DS:7DDA / 7DEC | Pic(18 B) | page1_desc / page2_desc | VGA page descriptors | 5DAC / 5DDC |
| DS:7DFE | s16 | course_layout | 0…9, saved | 5E0C |
| DS:7E02 / 7E04 | s16 | back_page / shown_page | init 1 / 2 | 5E3E / 5E40 |
| DS:7E06 | Rect | view_copy_rect | {320,82,0,18,0,18} | 5E42 |
| DS:7E12/7E14/7E16/7E18 | s16 | clip_xmin/ymin/ymax/xmax | clip rectangle (inclusive) | 5E4E/5E50/5E52/5E54 |
| DS:7E1A | u16[5] | sign_sheet_ids | 2047 2033 2032 2034 | 5E62 |
| DS:7E1E | s16[] | hill_peak_tab | words read by the Mulholland hillside peaks (values above) | — |
| DS:7E52 | s16[25] | dash_width | dash width per row; front row 14 = 7E6E, mirror row 7 = 7E7A | 5E6A / 5E84 |
| DS:7E84 | LodRec | dot_lod | dot shape LOD record | (60A0 data) |
| DS:7E94 | ShpSet | dot_shape | {1, far DS:7E84} | 60A0 |
| DS:7E9A | s16[13][2] | bush_group_dx | bush group spacing (a,b) | — |
| DS:7ECE | s16[13][2] | bush_group_hw | bush group (h,w) | — |
| DS:7F02 | {s16 srcx,w,h}[7] | sign_sizes | billboard sizes | 5E9C |
| DS:7FBE | i16[7] | rot_angles | −45 −30 −15 0 15 30 45 (degrees) | — |
| DS:7FCE | u16 | aq_bounce_flag | set by a1c0 | — |
| DS:7FD0/7FD6/7FDC/7FEE/8000 | s16[] | tree_lamp_dx/w/h/sy/dy | start-tree lamp blit tables | — |
| DS:80A0 | i16 | rot_into_scratch | rot_alloc uses DS:AB24 buffer | — |
| DS:80A2 | {u32 off,u16 size,u16}[26] | shp_dir | SR2_SHP directory | — |
| DS:8172 | ShpSet far*[23] | shp_sets_a | format-A sets by id (817A = set 2 hydrant) | 6078 (near) |
| DS:81CE | void far*[3] | shp_sets_b | format-B sets 23…25 | — |
| DS:81DA / 81DC | s16 | shp_b_first / shp_b_count | 23 / 3 | — |
| DS:86EC…8710, 871E | u16 | seg3726_* | constant 0x3726 | 6A2C…6A38 |
| DS:8E3C | u8[18][22] | rot_shrink_masks | SR2_DATA block 7, 7 angles × 3 shrink masks + pad | 5E56/5E5A/5E5E |
| DS:96A8 / 96AA / 96AC | s16 | build_yaw / build_seg / build_yaw_step | course builder | 6DFE / 6E00 / 6E02 |
| DS:96AE | Pic far*[2] | billboard_pics | 2049, 2050 (cruising) | (1261/1262) |
| DS:96B6/96BE | s16* | rp_xl / rq_xl | XL[p] / XL[q] row pointers | 6E08 / 6E10 |
| DS:96B8/96C0 | s16* | rp_xol / rq_xol | XOL[p] / XOL[q] | 6E0A / 6E12 |
| DS:96BA/96C2 | s16* | rp_ye / rq_ye | YE[p] / YE[q] | 6E0C / 6E14 |
| DS:96BC/96C4 | s16* | rp_xor / rq_xor | XOR[p] / XOR[q] | 6E0E / 6E16 |
| DS:96C6/96E0 | s16* | rp_xfl / rq_xfl | far-left x [p] / [q] | — |
| DS:96C8/96CA | s16* | rp_xfr / rq_xfr | far-right x [p] / [q] | — |
| DS:96CE/96D4 | s16* | rp_xr / rq_xr | XR[p] / XR[q] | 6E18 / 6E1C |
| DS:96D0/96D8 | s16* | rp_y / rq_y | Y[p] / Y[q] | 6E1A / 6E1E |
| DS:96D2/96D6 | s16* | rp_y79e / rq_y79e | Y79E[p] / [q] (rail/wall top) | — |
| DS:96DA/96CC | s16* | rp_yt1 / rq_yt1 | left terrain y [p] / [q] | — |
| DS:96DC/96DE | s16* | rp_yt2 / rq_yt2 | right terrain y [p] / [q] | — |
| DS:96E2 | far u16* | left_ptr | &L of the current row | 6E20 |
| DS:96E6 | far u16* | right_ptr | &R of the current row | 6E24 |
| DS:96EA | far s16* | x6_ptr | &X6 (obstacle lateral position 0..16) of the row | — |
| DS:96EE | s16* | dash_ptr | &dash_width[row] (table 7E52) | 6E28 |
| DS:96F0 | far u16* | centre_ptr | &C of the current row | 8B82 |
| DS:96F4 | s16 | rail_phase_seg | start_seg (drag-strip modes 2/4) or first seg with R&0x8000 rail start; parity of rail posts / wall colours | — |
| DS:96F6 | far ptr | aq_flag_ptr | segment-flag pointer, row k at −2k | — |
| DS:96FA | i16 | aq_star_scroll | (heading·2) % 320 | — |
| DS:96FC / 96FE | s16 | tree_lamp_right / tree_lamp_left | current lamp state 0…2, −1 none | — |
| DS:9708 | i16[4] | rot_saved | saved A540, A7A8, A7CC, BA90 (9708,970A,970C,970E) | — |
| DS:9732/9760 | i16[15]×2 | aq_rot_floor_l | rotated floor-left x/y, reversed | — |
| DS:9754 | far s16* | seg_lane_ptr | &LANE | — |
| DS:9758/9868, A7E8/AAF2 | i16 | aq_trough_side_l/r (+prev) | car edge side of the trough | — |
| DS:975E | s16 | ground_top_y | top row of the ground fill | 6E04 |
| DS:9856 | s16 | top_band_row | highest road row; farther bands are not drawn | 6E06 |
| DS:985C / A558 | s16 | ai_edge_left / ai_edge_right | edges 4 segs ahead | 70C0 / 7D2E |
| DS:985E,A2B8,A7A6,A5FA,9A5A,9B14 | i16 | aq_def_floor_h, _xl, _xr, top_h, top_xl, top_xr | channel deformation used by the next projection | — |
| DS:9862 | i16 | opp_variant | last bank variant | — |
| DS:986A/BA8E | i16 | aq_dy/aq_dx | projection dy/dx | — |
| DS:9A2A | Pic far*[4] | start_obj_sizes | 4 distance sizes of a start object | — |
| DS:9A3C | far u16* | seg_left_ptr | &L[seg] / &L2 | 72EE |
| DS:9A40 | Pic far* | start_panel_pic | DS:B7B2 picture | 6E60 |
| DS:9A48 | i16 | opp_contact | rear-end contact | 72F2 |
| DS:9A52 | Pic far* | pic_pillar | LIB3 #2003 A-pillar | 72F4 |
| DS:9A56 | s16 | view_bounce_y | added by road_proj_y | — |
| DS:9A80 | i16 | opp_sprite_prev | previous sprite | 6E2A |
| DS:9A92 | s16 | mirror_row_seg | seg−row of the mirror row being drawn | — |
| DS:9A94 | Pic far*[13] | police_sprites | police levels 0..12 | 7526 |
| DS:9AC8/9A58 | i16 | aq_car_xl/xr | player car left/right world x | — |
| DS:9AD8 | far s16* | seg_yaw_ptr | &YAW | 7560 |
| DS:9B0C | Pic far* | rooftops_pic | 2004 (released, stale) | 764A |
| DS:9B10 / 9E8E | far s16* | seg_bank_l_ptr / seg_bank_r_ptr | &BL / &BR | — |
| DS:9BC2 / AD0C / ADA2 | far Pic* | page0_ptr / page1_ptr / page2_ptr | A000 / A200 / A400 | 7678 / 8188 / 82B4 |
| DS:9BC6/9CFC | i16[15]×2 | aq_rot_top_l | rotated wall-top-left x/y, reversed | — |
| DS:9BE4 | Pic far* | horizon_pic | 2055 / 2056 | 767E |
| DS:9DC2 / 9BC0 | u16 | draw_seg / draw_off | back-page segment/offset | 787A / 767C |
| DS:9DC8 | far s16* | seg_hgt_ptr | &HGT | 7880 |
| DS:9DCE/9F0E | i16[15]×2 | aq_rot_top_r | rotated wall-top-right x/y, reversed | — |
| DS:9E04 | Pic far* | rot_side | mode 8 side view | — |
| DS:9E08 | s8[13][7] | anchor_x | sprite anchor x per level/angle | — |
| DS:9E64 | Pic far* | rot_r34 | mode 8 rear 3/4 | — |
| DS:9E70 | u16 | rot_enable | ≠0: plot_points rotates points (0379) | — |
| DS:9E72 | Pic far*[7] | rot_rear | rear at 7 angles ([3] = 9E7E original) | — |
| DS:9E94 | s8[13][7] | anchor_y | anchor y (from bottom) per level/angle | — |
| DS:A1C6 | Pic far*[7] | rot_rear5 | level-5 at 7 angles ([3] = A1D2) | — |
| DS:A2B4 | s16 | start_light_pos | +4/frame while moving | 7AAE |
| DS:A2B6/A55A | i16 | aq_car_sxl/sxr | their screen x at z=4 | — |
| DS:A2E2 9A5C 9EF0 AB5E AB2E A300 | s16[15] | row_xr xl y xor xol yeye | row tables (front+mirror) | 7ACE 72F8 7884 7FD8 7F8A 7AB0 |
| DS:A320 | Pic far*[7] | rot_front | front at 7 angles ([3] = A32C) | — |
| DS:A540 / BA90 | s16 | rot_sin / rot_cos | ×256 | — |
| DS:A55C | i16 | opp_bank | opponent bank angle | — |
| DS:A55E | s16 | seg | current segment | 7D30 |
| DS:A560 | s16 | substep | 0…63 within the segment | 7D32 (0…7) |
| DS:A562 | s16 | step_per_frame | sub-steps/frame (town 0x20/0x30) | 7D34 |
| DS:A564 | s16 | cam_x | car lateral position | 7D36 |
| DS:A566 | s16 | cam_y | eye height 0x76C | 7D38 |
| DS:A568 | s16 | — | cleared by 0736 | 7D3A |
| DS:A56A | s16 | heading | car heading | 7D3C |
| DS:A56C | s16 | cur_height | HGT[seg] | 7D3E |
| DS:A79E | far u16* | seg_centre_ptr | &C[seg] / &C2 | 7D62 |
| DS:A7A4 | s16 | front_row_seg | seg+row of the front row being drawn | — |
| DS:A7A8 / A7CC | s16 | rot_cx / rot_cy | rotation centre | — |
| DS:A7AC | i16 | start_active | start panel active | — |
| DS:A7D2 | u16 | aq_hit_bits | 1/4 wall openings, 0x20/0x40/0x60 floor obstacles (read by race) | — |
| DS:A7D4 | i16 | opp_sprite | current opponent sprite | 7D7C |
| DS:A7DA / B8D2 | s16 | edge_left / edge_right | road edges at seg | 7D82 / 8BD0 |
| DS:A7EA A5A8 A7AE ADB2 B89E | s16[15] | row_ykerb ybank_l ybank_r xfar_l xfar_r | new row tables | — |
| DS:A9F8 | u32 | race_road_seed | RNG state after the town build | — |
| DS:AA9C,B8BE,BB8A,B8E6 | i16 | aq_saved_* | copies of the above used this frame (mirror, opponent, probe) | — |
| DS:AA9E | Row[6] (14 B) | opp_sprite_sets | SR2_DATA block 8, sprite ids + mask record ptrs | 5E14 |
| DS:AAF8 | i16 | opp_last_sx | last screen x (s=2 smoothing) | — |
| DS:AAFA | near Row* | police_sprite_set | = 0xAAE4 (row 5) | — |
| DS:AB24 | Pic far* | start_pic2 / rot scratch | 2068/2071/extract; scratch for bb22 when DS:80A0 | — |
| DS:AB28 | i16 | dash_overlay_idx | index into DS:78E0 | — |
| DS:AD68 | i16 | aq_shape_variant | SR2_SHP format-B variant 0..6 | — |
| DS:ADA6/B162 | i16 | aq_trough_near_y/far_y | 9EF0[0] / 9EF0[N] | — |
| DS:ADD4 | s16 | obstacle14_hit | same for the C&0x1000 obstacle (shape 0x14) | — |
| DS:ADD6 | u16 | road_mode | 0 town cruise, 1 street drag race, 2/4 Grudge Night drag strip, 8 Aqueduct, 0x10 Mulholland | — |
| DS:B16A | far u16* | seg_right_ptr | &R[seg] / &R2 | 8634 |
| DS:B16E/B872 | i16 | aq_trough_near_xl/xr | trough edges at the nearest row | — |
| DS:B170/B1A0 | i16[15]×2 | aq_rot_floor_r | rotated floor-right x/y, reversed | — |
| DS:B190 | i16 | aq_roll_abs | abs(roll) | — |
| DS:B194 / 9A44 | s16 / far u16* | aq_rows / aq_flags_ptr | params for a094/9ce8 | — |
| DS:B198 | far ptr | opp_pic | sprite drawn last | — |
| DS:B7B0 | i16 | aq_pitch | roll-rate pitch (subtracted from dy) | — |
| DS:B7B2 | u16 | start_panel_id | 2067 / 2069 / 2070 | — |
| DS:B7B6 | Pic far*[13] | opp_sprites | opponent sprite levels 0..12 | 8ACE |
| DS:B7EA/B8D8 | i16 | aq_trough_far_xl/xr | trough edges at the farthest row | — |
| DS:B868/9E68 | i16 | aq_prev_roll/aq_prev_pitch | 9f9a history | — |
| DS:B8BC | s16 | wall_hit_shift | lateral view shift during the bounce (read by 8840/9f9a) | — |
| DS:B8C0 | i16 | opp_alongside |  | 8BC2 |
| DS:B8C6 | i16 | opp_last_d | last distance | — |
| DS:B8C8 | s16 | aq_wall_hit | ±1 Aqueduct wall side hit | — |
| DS:B8CC | Pic far* | pic_hood | hood overlay of the current car | 8BCA (#7) |
| DS:BA92 | i16 | rot_tmp_live | ad40 temp shrink to pop on next call | — |
| DS:BB88 | s16 | rock_hit | −1/0/+1 rock (C&4) in row 1 over the left/right wheels (Mulholland) | — |

## 3. Functions

### 0001:d2a8 aq_lane_target
`far int aq_lane_target(int x, int dx, int seg, int opp_x, int opp_seg)` — new; Aqueduct (mode 8) lane choice for the
autopilot (called by `road_advance_segment` with the player's values) and by race AI `0001:d4cc` (opponent). Returns a
lateral step to add to `x`.
```c
int aq_lane_target(int x, int dx, int seg, int ox, int oseg)
{
    int target = dx + x;
    int a = DS_AD6A + 0x438, b = DS_B8BE + 0x5C8;      /* channel x offsets of the current segment (set by 0dcb:1b02/af98) */
    int c = DS_B166 + 0xE88, e = DS_BB8A + 0xCF8;
    int lane[5];
    if (DS_9730 || DS_1F82) { lane = {0x514, 0x6A4, 0x884, 0xA3C, 0xBCC}; }   /* demo / auto */
    else {
        lane[0] = (b - a) / 3 + a;
        lane[1] = b + sgn(e-b) * (abs(e-b) >> 2);       /* cwd/xor/sub/sar 2/xor/sub */
        int m2 = (b + e) / 2;                           /* C division (rounds toward 0) */
        lane[2] = m2 - 0x1E;
        lane[3] = (m2 + e) / 2;
        lane[4] = (c + e) / 2;
    }
    int mid = (b + e) >> 1;                            /* sar */
    int o_rel = ox - mid, p_rel = x - mid, d = seg - oseg;
    int mode = d > 2 ? 1 : d < 0 ? 8 : (ox > x ? 2 : 4);
    int i, found = 0, idx;
    for (i = 0; i < 8; i++) {
        uint f = AQ[seg + i] & 0xFF;                    /* low byte, es = DS:871E */
        if (f & 0x10)      { idx = mode==2 ? 0 : mode==4 ? 4 : (p_rel < 0 ? 0 : 4);  found = 1; break; }   /* mode 1 and 8 use p_rel */
        else if (f & 0x80) { idx = mode==1 ? 2 : mode==2 ? 0 : mode==4 ? 4 : (p_rel < 0 ? 0 : 4); found = 1; break; }
    }
    if (found) target = lane[idx];
    else if (mode == 1) target = lane[o_rel < 0 ? 1 : 3];
    else if (mode == 8) target = lane[o_rel < 0 ? 3 : 1];
    if (i > 0) i--;  if (i > 0) i--;                  /* i = 8 when nothing found */
    return ((target - x) * 6 / (i + 1)) / 6;
}
```
Globals read: `AD6A B8BE B166 BB8A 9730 1F82`, far `AQ`.

### 0dcb:0006 plot_points_ega
New. Driver slot 19 of the EGA/VGA vector table (`DS:7B84` → active `DS:A0FA`); no direct callers (called through
`[A0FA]` by 0dcb:84c4, 0dcb:85a0, 2925:1e08).
`far void plot_points_ega(u8 far *buf, int xoff, Point *pts /*DS near, {int x,y}*/, int n, int colour)`
```c
for (i = 0; i < n; i++) {
    int x = xoff + pts[i].x, y = pts[i].y;            /* y is NOT offset */
    if (DS_9E70) rotate_about_centre(x, y, &x, &y);  /* 0dcb:0379 */
    if (x < DS_7E12 || x > DS_7E18 || y < DS_7E14 || y > DS_7E16) continue;
    set pixel (x,y) of the planar page buf (40 bytes/row, byte y*40 + (x>>3), bit 0x80>>(x&7)) to colour&15;
}
```
Hardware: bit mask = pixel bit, latch read, map mask 0xF write 0, map mask = colour write 0xFF; at the end map
mask 0xF, bit mask 0xFF. Port: plain pixel write into the back-page buffer.

### 0dcb:0108 plot_points_cga
New. CGA/Hercules table slot 19 (`DS:7A8C`). Same signature/loop/clip/rotation as 0006; pixel = 2 bpp interlaced:
byte `buf + (y&~1)*40 + (x>>2) + ((y&1) ? 0x2000 : 0)`, mask `0xC0 >> 2*(x&3)`, value `(colour<<6) >> 2*(x&3)`
(`*p = (*p & ~mask) | value`). Parked driver (only the EGA/VGA path is ported).

### 0dcb:01da plot_points_tdy
New. Tandy table slot 19 (`DS:7B28`). Same as 0006 but 4 bpp, 4 banks: byte
`buf + (y&3)*0x2000 + (y&~3)*40 + (x>>1)`, mask `0xF0 >> 4*(x&1)`, value `(colour<<4) >> 4*(x&1)`. Parked.

### 0dcb:02b4 sin256
New. `near int sin256(int deg)` — sine in 1/255 units from the 91-byte table `DS:1504` (sin 0..90°, 0…255).
```c
if (deg <= -360) deg += 360;  if (deg >= 360) deg -= 360;      /* once each */
neg = deg < 0;  if (neg) deg = -deg;
if (deg >= 90) {
    if (deg < 180)      deg = 180 - deg;
    else if (deg < 270) { deg -= 180; neg ^= 1; }
    else if (deg < 360) { deg = 360 - deg; neg ^= 1; }
    else deg = 0;
}
int r = DS_1504[deg];  return neg ? -r : r;
```
Callers: 031a, 0dcb:8774, a094, bb22 (cos = sin256(deg+90) presumably; callers' business).

### 0dcb:031a sin256_far
New. `far int sin256_far(int deg)` = `return sin256(deg);`. Callers: gauges `1a21:015c speedo_points`,
`1a21:038e tach_points`, `1a21:070e` (needle end points; race module).

### 0dcb:0327 line_x_at_y
New. `near int line_x_at_y(int x1,int y1,int x2,int y2,int Y)`:
`return (y2 == y1) ? x2 : (int)((long)(Y - y1) * (long)(x2 - x1) / (long)(y2 - y1)) + x1;`
(16-bit differences, 32-bit product and signed 32-bit division, low word). Used by 7de4, 7f30, 905c, a92a.

### 0dcb:0350 line_y_at_x
New. `near int line_y_at_x(int x1,int y1,int x2,int y2,int X)`:
`return (x2 == x1) ? y2 : (int)((long)(X - x1) * (long)(y2 - y1) / (long)(x2 - x1)) + y1;` Used by 7de4, 7f30, 8ece.

### 0dcb:0379 rotate_about_centre
New. `near void rotate_about_centre(int x,int y,int *ox,int *oy)` (mode-8 renderer and plot_points):
```c
int dx = x - DS_A7A8, dy = y - DS_A7CC;                /* centre */
*ox = MUL8(dx, DS_BA90) + MUL8(dy, DS_A540) + DS_A7A8;   /* BA90 = cos*256, A540 = sin*256 */
*oy = MUL8(dy, DS_BA90) + MUL8(-dx, DS_A540) + DS_A7CC;
/* MUL8(a,b) = (int16)(((int32)a * b) >> 8)  (bits 8..23 of the imul product) */
```

### 0dcb:03cc rotate_vec
New, no callers found (dead or reached by pointer; address not stored in DS tables). Same as 0379 without the
centre: `*ox = MUL8(x,BA90) + MUL8(y,A540); *oy = MUL8(y,BA90) + MUL8(-x,A540);`.

### 0dcb:0410 road_fillmode_begin
= SR1 road_fillmode_begin (2645:0008), ../StreetRod/port/spec/race_render.md §2/§4.1; DS remap: SR1 DS:8236 → SR2 DS:AD1C, DS:5C1E → DS:7B9A.

### 0dcb:0425 road_fillmode_end
= SR1 road_fillmode_end (2645:001d), ../StreetRod/port/spec/race_render.md §2/§4.1; DS remap: SR1 DS:8236 → SR2 DS:AD1C.

### 0dcb:043c ega_hline
Changed (driver slot 13, `DS:7B6C` → `[A0E2]`). **No colour parameter any more**: `far void ega_hline(int x1,int x2,int y)`;
the colour is whatever the last `[A0F2]` set_colour(c) put into the set/reset register (shape_draw, banner and the
mode-8 code call set_colour before their hlines). Clip/swap logic identical to SR1 §4.17 (reject if y outside
[7E14,7E16], or `xmin > x2 || xmax < x1` tested **before** the swap; clamp x1 up to xmin, x2 down to xmax; swap if
x1 > x2; fill x1..x2 inclusive). DS remap: 5E4E/50/52/54 → 7E12/7E14/7E16/7E18, 787A → 9DC2.
Port: `hline(x1,x2,y)` uses a module-static `cur_colour` set by `set_colour(c)`.

### 0dcb:04e6 ega_fill_rows
= SR1 ega_fill_rows (2645:00e7), ../StreetRod/port/spec/race_render.md §4.17; DS remap: SR1 DS:5E52 → SR2 DS:7E16, DS:787A → DS:9DC2, DS:5C1E → DS:7B9A; slot 78DA → A0E6 (table entry DS:7B70).

### 0dcb:054a ega_pixel_addr
= SR1 ega_pixel_addr (2645:014b), ../StreetRod/port/spec/race_render.md §2; DS remap: SR1 DS:787A → SR2 DS:9DC2.

### 0dcb:056a road_set_seg_ptrs
Changed (now **near**). `near void road_set_seg_ptrs(int seg)`; all far ptrs have segment 0x3726:
```c
if (!(DS_ADD6 & 6)) { DS_9A3C = &L[seg];  DS_B16A = &R[seg];  DS_A79E = &C[seg]; }
else                { DS_9A3C = &L2[seg]; DS_B16A = &R2[seg]; DS_A79E = &C2[seg]; seg += 0xC0; }   /* drag strip */
DS_9754 = &LANE[seg]; DS_9DC8 = &HGT[seg]; DS_9AD8 = &YAW[seg]; DS_9B10 = &BL[seg]; DS_9E8E = &BR[seg];
```
DS remap: SR1 72EE→9A3C, 8634→B16A, 7D62→A79E, 7880→9DC8, 7560→9AD8.

### 0dcb:062c road_proj_x
Changed: the camera x used is clamped in the Aqueduct: `cx = DS_A564; if (DS_ADD6 == 8) cx = clamp(cx, 0x564, 0xD5C);`
then SR1 formula with `cx` (`x += (z*dx)>>2; t = (cx - x) << 2; if (!z) z = 1; return 0xA0 - t/z;`).
DS remap: SR1 7D36 → SR2 A564.

### 0dcb:0684 road_proj_y
Changed: hill term halved (even-masked) and a global vertical offset added:
```c
int road_proj_y(int z, int y, int dy)
{   int t = (((z*dy) & 0xFFFE) << 1) + ((y - DS_A566) << 2);   /* imul low word */
    if (!z) z = 1;
    return t / z + DS_9A56 + 0x3C; }
```
`DS:9A56` = view bounce (0 normally, set by `a1c0`). DS remap: 7D38 → A566.

### 0dcb:06ba road_proj_x_mirror
Changed: same mode-8 clamp of the camera x as `road_proj_x` (0x564…0xD5C); otherwise SR1 (no ×4, centre 0x118).

### 0dcb:070e road_proj_y_mirror
= SR1 road_proj_y_mirror (2645:1828), ../StreetRod/port/spec/race_render.md §4.4; DS remap: SR1 DS:7D38 → SR2 DS:A566.

### 0dcb:0736 road_state_init
Changed (far; called by `drive_to_event` 0001:8c68 and `race_start` 0001:be82):
```c
if (DS_B7AE)                 DS_A55E = DS_9AE2;          /* race: start segment from the mode table (race spec, 0001:bc88) */
else switch (DS_1BC8) { case 1: DS_A55E = 0x0A; break; case 2: case 4: DS_A55E = 0x41; break; case 3: DS_A55E = 0x73; break; }
road_set_seg_ptrs(DS_A55E);
DS_A7DA = DS_985C = 0x5DC;  DS_B8D2 = DS_A558 = 0xCE4;
DS_A564 = 0xA00; DS_A566 = 0x76C; DS_A560 = 0; DS_A568 = 0; DS_A56C = 0; DS_A56A = 0; DS_A562 = 0;
DS_8012 = 1;  DS_B19C = (rnd(2) == 0);  DS_A7D6 = 1;
DS_A33E = DS_A564;  DS_7BA0 = 0; DS_7B9E = 0;  DS_9A80 = 0; DS_A7D4 = 0;
arena_clear();                                           /* 1a21:9940 (platform) */
```
DS remap: 8ACA→B7AE, 0284→1BC8, 7D30→A55E, 7D82→A7DA, 8BD0→B8D2, (ahead edges 70C0→985C, 7D2E→A558), 7D36→A564,
7D38→A566, 7D32→A560, 7D3A→A568, 7D3E→A56C, 7D3C→A56A, 7D34→A562, 54D4→8012, 8648→B19C, 7D7E→A7D6; nudge latches
(SR1 cleared in road_race_init) 5E12→7BA0, 5E10→7B9E. New: A33E, 9A80, A7D4 (race AI state), arena_clear.

### 0dcb:07e4 road_project_front
Changed substantially (11 row tables, all s16[15], row k at index k; **shared with the mirror**, see `0ce4`).

| table | SR1 eq | world value projected |
|---|---|---|
| DS:A2E2 `row_xr` | 7ACE | right edge |
| DS:9A5C `row_xl` | 72F8 | left edge |
| DS:9EF0 `row_y` | 7884 | road surface y 0x7D0 |
| DS:AB5E `row_xor` | 7FD8 | outer right `0xCE4 + w` |
| DS:AB2E `row_xol` | 7F8A | outer left `0x5DC − w` |
| DS:A300 `row_yeye` | 7AB0 | y 0x76C (now every row, no `C&1/C&0xF000` test) |
| DS:A7EA `row_ykerb` | — | y 0x79E |
| DS:A5A8 `row_ybank_l` | — | y `0x7D0 − BL[row seg]` |
| DS:A7AE `row_ybank_r` | — | y `0x7D0 − BR[row seg]` |
| DS:ADB2 `row_xfar_l` | — | x `−(w − 0x177)·4` (w=400: −100; w=200: 700) |
| DS:B89E `row_xfar_r` | — | x `(w + 0x339)·4` (w=400: 4900; w=200: 4100) |

`w = (DS_ADD6 == 0x10) ? 200 : 400`. Edges from `cm = (low byte of C) & 0xE0` (**changed** vs SR1):
`cm==0`: xr 0xCE4, xl 0x5DC; `cm&0x80`: xr 0xAF0, xl 0x7D0; else `cm&0x20`: xr = xl = 0xCE4; else (0x40): xr = xl = 0x5DC.
```c
void road_project_front(void)   /* pointers set by road_set_seg_ptrs(seg) just before */
{
    for (int k = 0; k < 15; k++) {
        uint cm = *(u8 far*)DS_A79E & 0xE0;  DS_A79E += 2;
        int dy;
        if (DS_ADD6 & 6) dy = 0;                                   /* drag strip: flat, HGT ptr not advanced */
        else { dy = DS_A56C - *DS_9DC8; DS_9DC8 += 2; }
        int dx = DS_A56A - *DS_9AD8;  DS_9AD8 += 2;
        if (k == 0 && DS_ADD6 == 0x10) dx = clamp(dx, -0x30, 0x30);  /* row 0 only */
        int z;                                                     /* q = A560/8 truncated toward 0 */
        if (k == 0) z = 4; else if (k == 1) z = 0x0C - q; else z += 8;
        if (k == 14) z += q;                                       /* → row 14 always z = 116 */
        if (k == 0) Y[0] = road_proj_y(4, 0x7D0, dy);              /* row 0: Y first */
        XR[k] = road_proj_x(z, xr_w, dx);  XL[k] = road_proj_x(z, xl_w, dx);
        if (k) Y[k] = road_proj_y(z, 0x7D0, dy);
        if (k == 0) { XOR[0] = road_proj_x(4, (w - DS_A564) + 0x19C8, dx);   /* row 0 quirk, keep */
                      XOL[0] = road_proj_x(4, -(w - DS_A564), dx); }
        else        { XOR[k] = road_proj_x(z, w + 0xCE4, dx);  XOL[k] = road_proj_x(z, 0x5DC - w, dx); }
        YE[k]  = road_proj_y(z, 0x76C, dy);
        YK[k]  = road_proj_y(z, 0x79E, dy);
        YBL[k] = road_proj_y(z, 0x7D0 - *DS_9B10, dy);  DS_9B10 += 2;
        YBR[k] = road_proj_y(z, 0x7D0 - *DS_9E8E, dy);  DS_9E8E += 2;
        XFL[k] = road_proj_x(z, -(w - 0x177) * 4, dx);
        XFR[k] = road_proj_x(z, (w + 0x339) * 4, dx);
    }
}
```
DS remap: 7D62→A79E, 7880→9DC8, 7560→9AD8, 7D3E→A56C, 7D3C→A56A, 7D32→A560 (now /8).

### 0dcb:0ce4 road_project_mirror
Changed: writes **the same tables as `07e4`** (rows 0…7; `row_ykerb` up to [14]), so it must run after the front view
has been drawn (road_frame order). Differences from the front pseudocode:
* pointers walk backwards: `A79E -= 2`, `9DC8 -= 2`, `9AD8 -= 2`, but **`9B10`/`9E8E` still `+= 2`** (banks read forward —
  original bug, keep); `dy = *HGT − A56C`, `dx = *YAW − A56A` (as SR1), dy = 0 in modes 2/4.
* row 0: z = 8; `if (DS_ADD6 & 0x10) { if (dx > 0x3C) dx = 0x46; else if (dx < -0x3C) dx = -0x46; }`; row-0 XOR/XOL use the
  normal values (`w+0xCE4`, `0x5DC−w`), no quirk.
* rows 1…7: z = q+8 at k=1, then +8 per row, and at k=5 `z -= q` → z = q+8k for k≤4, 8k for k≥5.
* uses `road_proj_x_mirror` / `road_proj_y_mirror`.
* order per loop row: XR, XL, Y, **YK**, XOR, XOL, YE, **YK**, YBL, YBR, XFL, XFR — `row_ykerb` gets two equal entries
  per row (pointer +4 per row: row k writes YK[2k−1] and YK[2k]); row 0 writes YK[0] once. Keep.

### 0dcb:11fc road_frame
Changed: no projection-then-draw-all; per view project → draw. No busy-wait before drawing; pacing at the end.
```c
void road_frame(void)
{
    if (DS_ADD6 == 8) 0dcb_18ec();                       /* Aqueduct front projection (§3) */
    else { road_set_seg_ptrs(DS_A55E); road_project_front(); }
    if (DS_ADD6 == 8) { DS_B194 = 14; DS_9A44 = FAR(0x3726, 0x4E62 + 2*DS_A55E) /* &AQ[seg] */; 0dcb_a094(); }
    else { road_set_seg_ptrs(DS_A55E); 0dcb_6a32(); }   /* draw front view (§3) */
    if (DS_ADD6 == 8) 0dcb_1b02();                       /* Aqueduct mirror projection */
    else { road_set_seg_ptrs(DS_A55E); road_project_mirror(); }
    if (DS_ADD6 == 8) { DS_B194 = 7; DS_9A44 = FAR(0x3726, 0x6570 - 2*DS_A55E) /* = &AQM[0x171-seg] */; 0dcb_9ce8(); }
    else { road_set_seg_ptrs(DS_A55E); 0dcb_5aee(); }   /* draw mirror */
    road_fillmode_end();                                 /* 0dcb:0425 */
    mirror_draw_background();                            /* 0dcb:573e */
    if (DS_B7AE && !(DS_ADD6 & 6)) 0dcb_a746();          /* race only, not on the drag strip */
    road_draw_cockpit_overlay();                         /* 0dcb:56e8 */
    road_page_flip();                                    /* 0dcb:354e */
    pace_ticks(8);                                       /* 1a21:1924, misnamed font_string_width in symbols.csv */
}
```
`pace_ticks(n)` (platform/video): `DS:1D02 (u32) += n; while (DS:1F66 (u32 ticks) < DS:1D02) ; DS:1D02 = DS:1F66;` —
deadline-based (SR1: wait ≥ 9 ticks from before the draw). Port must pump the host in the wait.

### 0dcb:12ca road_advance_segment
Changed:
```c
DS_A55E += DS_A560 >> 6;  DS_A560 &= 0x3F;
if (DS_ADD6 == 8) { DS_985C = DS_A7DA = DS_9A5A + 0x438;  DS_A558 = DS_B8D2 = DS_9B14 + 0xE88; }  /* no ptr reset */
else {
    road_set_seg_ptrs(DS_A55E);
    if (C[seg] & 0xE0) { DS_A7DA = 0x7D0; DS_B8D2 = 0xAF0; } else { DS_A7DA = 0x5DC; DS_B8D2 = 0xCE4; }   /* low byte via A79E */
    if (C[seg+4] & 0x80) { DS_985C = 0x7D0; DS_A558 = 0xAF0; } else { DS_985C = 0x5DC; DS_A558 = 0xCE4; }
}
DS_A56C = *DS_9DC8;
if (DS_ADD6 & 6) return;
if (!(((DS_9730 || DS_1F82) && !DS_A7AC) || DS_A55E > DS_A2AA)) return;   /* autopilot: demo/auto, or past the finish */
int t = 0x960;                                 /* (0x5C8+0xCF8)>>1 in mode 8, (0x5DC+0xCE4)>>1 else: both 0x960 */
if (DS_ADD6 == 8 || !(C[seg] & 0x80))          /* C read via es = DS:86EC */
    t += (DS_A9CC > t) ? -0x140 : 0xA0;        /* lane away from the opponent */
if (DS_ADD6 == 8) { t = aq_lane_target(DS_A564, t - DS_A564, DS_A55E, DS_A9CC, DS_A9C6) + DS_A564; DS_A564 = t; }
else if (abs(DS_A564 - t) > 100) DS_A564 = t + (DS_A564 > t ? 100 : -100);
else DS_A564 = t;
int y = *DS_9AD8;
if (abs(y - DS_A56A) > 0) { DS_A104 = (DS_A56A - y) * 2; if (abs(DS_A104) > 0x18) DS_A104 = DS_A104 > 0 ? 0x18 : -0x18; }
DS_A56A = y;
```
SR1 → SR2: finish test `(7EC0 ? 0x3FC : 0x17C) >= seg` → `seg > DS_A2AA` (finish segment from the mode table) and the
autopilot now also needs `!DS_A7AC`; edges simplified (any 0xE0 bit → narrow; ahead only tests 0x80). DS remap: 0610→9730,
0612→1F82, 7E90→A9CC, 78E4→A104, 70C0→985C, 7D2E→A558, plus §0736 list.

### 0dcb:1470 track_lane_x
New. `far int track_lane_x(int seg) { return LANE[seg] * 0x70 + 0x5DC; }` (far `3726:665A`, no mode offset). Caller:
race AI `0001:cf9a`.

### 0dcb:148a road_edge_collision
Changed (= SR1 2645:206b, rewritten). Called every timer tick from `race_isr` (0001:2622) when `DS_B7AE`; returns 1 = hit.
```c
int road_edge_collision(void)
{
    if (DS_ADD6 == 8) return aq_wall_collision();          /* 1cd2 */
    DS_7BA2 = 0;                                           /* on_shoulder (SR1 5E0E) */
    int l = DS_A564 - 0x1E, r = DS_A564 + 0x46, s = DS_A55E;
    uint c = C[s+1], rr = R[s+1];  int bl = BL[s], br = BR[s];
    int w = (DS_ADD6 == 0x10) ? 0x96 : 0x104;
    int eL = DS_A7DA, eR = DS_B8D2;
    if (bl > 0 && l < eL - (w>>2)) return 1;               /* rising bank left */
    if (br > 0 && r > eR + (w>>1)) return 1;
    if (l <= eL - w || r >= eR + w) {                      /* far off the road */
        if (!(DS_ADD6 & 0x10)) return 1;
        if (DS_7B9C) return 1;
        if (l < eL && bl < 0) DS_7B9C = -1;                /* Mulholland: fall off the left */
        if (r > eR && br < 0) DS_7B9C = 1;                 /* … off the right (road_step → 7da2) */
        return 1;
    }
    if ((c & 0x80) && (l < eL + (w>>2) || r > eR - (w>>1))) return 1;   /* bridge rails */
    int shoulder = 0;
    if (bl == 0 && l < eL) shoulder = 1;
    else if (br == 0 && r > eR) { if (c & 2) return 1; shoulder = 1; }   /* telephone pole */
    if (shoulder) { if (rr & 0xC0) return 1; DS_7BA2 = 1; }               /* tree / big tree beside */
    if (DS_ADD6 == 0x10) {
        if (DS_BB88) { DS_BB88 = 0;
            if (DS_9E92 > 0x1E) { if (!DS_9730 && !DS_1F82) return 1; 0001_cb00(0x1E, &DS_A100); } }
        if (DS_ADD4) { if (DS_9E92 > 0x32) DS_A56A += DS_ADD4 * -0x28;
                       if (DS_9E92 > 0x14) DS_A100 -= 10;  DS_ADD4 = 0; }
        if ((L[s] & 0x10) && l < 0x79E) return 1;
        if ((R[s] & 0x10) && r > 0xB22) return 1;
        if ((c & 0x60) && DS_9E92 > 0x3C) { if (!DS_9730 && !DS_1F82) return 1; 0001_cb00(0x3C, &DS_A100); }
        if (!(rr & 0x8000) || r < eR) return 0;
        if (DS_9E92 >= 0x28) return 1;
        (*DS_BBCC)(); (*DS_BBCC)();  DS_A564 -= 300;  DS_A56A += 0x1E;
    } else {
        if (!(DS_ADD6 & 6)) return 0;
        if (r < eR - 0xB4) return 0;                       /* drag strip: right wall */
        if (DS_9E92 >= 0x28) return 1;
        (*DS_BBCC)(); (*DS_BBCC)();  DS_A564 -= 200;  DS_A56A += 0x14;
    }
    DS_A100 = (DS_A100 > 10) ? DS_A100 - 10 : 0;
    (*DS_BBCC)();
    return 0;
}
```
`DS:9E92` mph shown on the speedo; `DS:BBCC` far fn ptr (no args; scrape/bump effect, set by race `0001:d908`);
`0001:cb00(int v, int *speed)` race (crash/damage); `DS:BB88`, `DS:ADD4` bump latch/direction set by the draw code `6a32`.

### 0dcb:1758 road_nudge_heading
= SR1 road_nudge_heading (2645:2126), §4.6; DS remap: SR1 DS:7D3C → SR2 DS:A56A.

### 0dcb:1774 road_step
Changed (once per frame from `race_run` 1a21:77f4):
```c
if (DS_A102 && DS_A100 && abs(DS_A56A - *DS_9AD8 - DS_A102) < 200) DS_A56A -= (DS_A102 >> 1) + 1;
if (!DS_B7AE) DS_A562 = DS_ADD8 ? 0x30 : 0x20;          /* town cruising */
if (DS_A562 > 0) DS_A2B4 += 4;                          /* start light (now increases, no upper test) */
road_frame();
if (!DS_9730 && !DS_1F82) {
    if (DS_9DC4) DS_A564 += 0x19;                        /* pulled over */
    else if (DS_A100) {
        DS_A564 -= ((DS_A562/8 /*trunc*/ + DS_A10C) * (DS_A56A - *DS_9AD8)) >> 2;
        if (DS_ADD6 == 0x10) DS_A564 = min(max(DS_A564, DS_A7DA - 0x96), DS_B8D2 + 0x96);   /* max first */
    }
}
if (!DS_B7AE) {                                         /* town: own advance */
    DS_A560 += DS_A562;  if (DS_A560 >= 0x40) road_advance_segment();
    if ((L[DS_A55E+7] & 1) || DS_7BA0) { DS_7BA0 = 1; road_nudge_heading(1); }
    if ((R[DS_A55E+6] & 1) || DS_7B9E) { road_nudge_heading(-1); DS_7B9E = 1; }
} else {
    if (DS_7B9C) { road_fall_off_edge(DS_7B9C); DS_7B9C = 0; }       /* 7da2 */
    if (DS_B8C8 && DS_ADD6 == 8) aq_wall_bounce(DS_B8C8);           /* a1c0 */
}
```
DS remap: 78E2→A102, 78E0→A100, 8ACA→B7AE, 82C6→ADD8, 7D34→A562, 7AAE→A2B4 (sign of the update flipped: SR1 `-= 4`
while >0), 0610→9730, 0612→1F82, 787C→9DC4, 78EC→A10C, 5E12→7BA0, 5E10→7B9E. Town speeds ×8 (64 sub-steps).

### 0dcb:18ec aq_project_front
`far void(void)` — mode-8 replacement of `road_project_front` (called by `road_frame` when ADD6==8).
```c
DS_986A = DS_A56C - DS_B7B0;            /* height diff used as dy (B7B0 = roll-rate pitch, 9f9a) */
DS_BA8E = DS_A56A;                      /* heading used as dx */
int mid = (DS_A7A6 + DS_A2B8 + 0x12C0) >> 1;
DS_B16E = road_proj_x(4, mid + DS_A2B8 - 0x28, DS_A56A);   /* trough near-left x  */
DS_B872 = road_proj_x(4, mid + DS_A7A6 + 0x28, DS_BA8E);   /* trough near-right x */
DS_9AC8 = DS_A564 - 0x6E;   DS_9A58 = DS_A564 + 0x208;     /* car left / right world x */
DS_A2B6 = road_proj_x(4, DS_9AC8, DS_BA8E);  DS_A55A = road_proj_x(4, DS_9A58, DS_BA8E);
int f = DS_A560 / 8;                    /* C truncation; A560 = 0..63 */
int z = 4;
for (int i = 0; i < 15; i++) {          /* z: 4, 12-f, +8 …; row 12 adds +f → rows 12..14 = 100,108,116 */
    if (i == 1)  z = 12 - f;
    if (i == 12) z += f;
    9EF0[i] = road_proj_y(z, DS_985E + 2000, DS_986A);
    9A5C[i] = road_proj_x(z, DS_A2B8 + 0x5C8, DS_BA8E);
    A2E2[i] = road_proj_x(z, DS_A7A6 + 0xCF8, DS_BA8E);
    A5A8[i] = road_proj_x(z, mid + DS_A7A6,   DS_BA8E);
    A300[i] = road_proj_y(z, DS_A5FA + 0x67C, DS_986A);
    AB2E[i] = road_proj_x(z, DS_9A5A + 0x438, DS_BA8E);
    AB5E[i] = road_proj_x(z, DS_9B14 + 0xE88, DS_BA8E);
    if (i < 14) z += 8;                 /* (loop form: next z = this z + 8) */
}
/* z = 116 here */
DS_B7EA = road_proj_x(z, mid + DS_A2B8 - 0x28, DS_BA8E);   /* trough far-left x  */
DS_B8D8 = road_proj_x(z, mid + DS_A7A6 + 0x28, DS_BA8E);   /* trough far-right x */
DS_ADA6 = 9EF0[0];  DS_B162 = 9EF0[14];                    /* trough near / far y */
```
`road_proj_x/y` are SR2 062c/0684 (mode 8: camera x `A564` clamped to 0x564…0xD5C inside road_proj_x).
Inputs `985E A2B8 A7A6 A5FA 9A5A 9B14` are the channel deformation set by `a094` (previous frame).

### 0dcb:1b02 aq_project_mirror
`far void(void)` — mode-8 replacement of `road_project_mirror`; uses the values saved by `a094`
(`AA9C`=985E, `B8BE`=A2B8, `BB8A`=A7A6, `B8E6`=A5FA of this frame) and `AD6A`/`B166` (outer-wall shifts,
set by race init 0001:8d9e).
```c
int dy = -DS_986A, dx = -DS_BA8E;
int mid = (DS_BB8A + DS_B8BE + 0x12C0) >> 1;
DS_B16E = road_proj_x_mirror(2, mid + DS_B8BE - 0x28, dx);
DS_B872 = road_proj_x_mirror(2, mid + DS_BB8A + 0x28, dx);
int f = DS_A560 / 8, z = 2;
for (int i = 0; i < 8; i++) {           /* z: 2, 8+f, +8, …; row 5 subtracts f → rows 5..7 = 40,48,56 */
    if (i == 1) z = f + 8;
    if (i == 5) z -= f;
    9EF0[i] = road_proj_y_mirror(z, DS_AA9C + 2000, dy);
    9A5C[i] = road_proj_x_mirror(z, DS_B8BE + 0x5C8, dx);
    A2E2[i] = road_proj_x_mirror(z, DS_BB8A + 0xCF8, dx);
    A5A8[i] = road_proj_x_mirror(z, mid + DS_BB8A, DS_BA8E);  /* QUIRK: +BA8E, not negated (verified) */
    A300[i] = road_proj_y_mirror(z, DS_B8E6 + 0x67C, dy);
    AB2E[i] = road_proj_x_mirror(z, DS_AD6A + 0x438, dx);
    AB5E[i] = road_proj_x_mirror(z, DS_B166 + 0xE88, dx);
    if (i < 7) z += 8;
}
DS_B7EA = road_proj_x_mirror(z, mid + DS_B8BE - 0x28, dx);   /* z = 56 */
DS_B8D8 = road_proj_x_mirror(z, mid + DS_BB8A + 0x28, dx);
DS_ADA6 = 9EF0[0];  DS_B162 = 9EF0[7];
```

### 0dcb:1cd2 aq_wall_collision
New. `far int aq_wall_collision(void)`: `DS_7BA2 = 0; if (DS_9AC8 < 0x439) { DS_B8C8 = 1; return 1; }
if (DS_9A58 >= 0xDED) { DS_B8C8 = -1; return 1; } return DS_A7D2 ? 1 : 0;` (`9AC8`/`9A58` car-side x vs Aqueduct walls,
set by `18ec`; `A7D2` set by the Aqueduct draw code).

### 0dcb:1d04 feat_curve_left
Changed (= SR1 track_feat_curve_left 2645:017a, feature code(s) 1,4,6 curve left). Common changes of all seven feature functions (1d04 … 23d4):
Changed versions of SR1 §4.3 features (near; `pos` = `DS:96AA`, `yaw` = `DS:96A8`, `step = n + DS:96AC` where the four
curve features take `int n` (always 1) and `96AC = ADD8 ? 5 : 6` → step 7 normal / 6 slow; SR1 step 3/2). Common changes:
`keep_side_flags` mask **0x9811** (SR1 0x1801); `prefill` and ramp loops bounded by the array end **0x3DE** (SR1 ramp:
0x442); lane loops now bounded too. `ramp(delta)` = SR1 ramp with this `step` and bound `pos < 0x3DE`.
Sequence: `prefill(0x32)`; `C[pos]
Hills (SR1 codes 3–6) no longer exist: codes 3/5 → curve right, 4/6 → curve left; heights come from `DS:7D42` (see 24d8).

### 0dcb:1dfa feat_curve_right
Changed (= SR1 track_feat_curve_right 2645:0293, feature code(s) 2,3,5 curve right). Common changes: see 0dcb:1d04.
Sequence: `prefill(0x32)`; `L,R[pos]&=0x9811`; `C[pos]

### 0dcb:1eea feat_s_bend_left
Changed (= SR1 track_feat_s_bend_left 2645:03a8, feature code(s) 10 S-bend). Common changes: see 0dcb:1d04.
Sequence: `prefill(0x5A)`; `pos+=1`; `L,R[pos]&=0x9811`; `R[pos]

### 0dcb:2034 feat_s_bend_right
Changed (= SR1 track_feat_s_bend_right 2645:0539, feature code(s) 11 S-bend). Common changes: see 0dcb:1d04.
Sequence: as 1eea with `R

### 0dcb:217e feat_roadworks_right
Changed (= SR1 track_feat_roadworks_right 2645:091a, feature code(s) 7 roadworks right). Common changes: see 0dcb:1d04.
Sequence: **`if (BL[pos]==BR[pos]) return;`** `s=pos`; `pos+=3`; `R[pos]

### 0dcb:2282 feat_narrow_bridge
Changed (= SR1 track_feat_narrow_bridge 2645:09f0, feature code(s) 9 narrow bridge). Common changes: see 0dcb:1d04.
Sequence: `s=pos`; `C[s]

### 0dcb:2350 (not a function)
Bogus entry inside `0dcb:2282` (bytes `3e 8e d8` of `mov ax,0x3E08 / mov ds,ax` at 234e). Its "callers" 0001:00c0/01ea
(video_menu / sound_options_menu) are near calls to **0001:2350**, a different address. Nothing to port.

### 0dcb:23d4 feat_roadworks_left
Changed (= SR1 track_feat_roadworks_left 2645:0abb, feature code(s) 8 roadworks left). Common changes: see 0dcb:1d04.
Sequence: **`if (BL[pos]==BR[pos]) return;`** `s=pos`; `pos+=4`; `L[pos]

### 0dcb:24d8 track_build_course
Changed (= SR1 track_build_course 2645:0b91). `far void track_build_course(int arg)`; `new_game` calls it with 100 (random
layout), `load_game_screen` with the saved `DS:7DFE`.
```c
aq_build();                                              /* 33ac */
build_banks();                                           /* same loop as in 2866, below */
DS_96AA = 0x186; DS_96A8 = 0; DS_96AC = DS_ADD8 ? 5 : 6;
for (i = 0x17C; i < 0x3DE; i++) {                        /* remove previous course, keep scenery */
    YAW[i] = 0; HGT[i] = 0; C[i] &= 0xF117; L[i] &= 0xF7FF; R[i] &= 0x97FF;
    if ((R[i] & 0x1000) && !(R[i+1] & 0x0010)) R[i] &= ~0x1000;   /* keep only bank markers (2866 step 14) */
    if ((L[i] & 0x1000) && !(L[i+1] & 0x0010)) L[i] &= ~0x1000;
    if ((i + 1) % ((DS_ADD8 + 1) * 4) == 0) C[i] |= 8;
}
if (arg == 100) DS_7DFE = rnd(10);
for (k = 0; k < 13 && DS_96AA < 0x3DE; k++)
    switch (DS_7C3E[DS_7DFE*13 + k]) {                    /* jump table 0dcb:2716 */
    case 1: case 4: case 6: feat_curve_left(1); break;   /* 1d04 */
    case 2: case 3: case 5: feat_curve_right(1); break;  /* 1dfa */
    case 7: feat_roadworks_right(); break;               /* 217e */
    case 8: feat_roadworks_left(); break;                /* 23d4 */
    case 9: feat_narrow_bridge(); break;                 /* 2282 */
    case 10: feat_s_bend_left(1); break;                 /* 1eea */
    case 11: feat_s_bend_right(1); break;                /* 2034 */
    }
for (i = 0x17C; i < 0x3DE; i += 10) for (j = 0; j < 10; j++) {   /* height profile */
    int s = i + j;
    if (!(C[s] & 0x80) && (i < 0x398 || i > 0x3C0)) HGT[s] = HGT[s-1] + DS_7D42[(i - 0x17C)/10];
    else HGT[s] = HGT[s-1];                              /* flat on bridges and before the Mulholland finish */
}
for (; DS_96AA < 0x3DE; DS_96AA++) YAW[DS_96AA] = DS_96A8;
C[0x3BF] |= 0x10;                                        /* finish line, Mulholland (finish seg 0x3C0) */
for (k = 0; k < 0x74; k++) { L2[k] = L[0xC0+k]; R2[k] = R[0xC0+k]; C2[k] = C[0xC0+k] | 8; }   /* drag strip */
C2[0x57] |= 0x10;                                        /* drag-strip finish line (finish seg 0x58) */
```
`build_banks()` (inline in 24d8 and 2866): for `i = 200; i < 0x3DE; i += 10`, `j = 0…9`, `s = i+j`:
`i < 0x17C`: `BL[s] = BR[s] = 0`; `i` in 380/390 (0x17C ≤ i < 0x18F), 600/610 (0x257 < i < 0x26B) or 700/710
(0x2BB < i < 0x2CF): `BL[s] = BL[s-1] + 200, BR[s] = BR[s-1] − 200`; `i` in 400/410 (0x18F < i < 0x1A3) or 500/510
(0x1F3 < i < 0x207): `BL −= 200, BR += 200` (relative to s−1); else copy `BL[s-1]`, `BR[s-1]`. Result: 0 up to 379,
BL ramps to +4000 (380–399), back to 0 (400–419), to −4000 (500–519), 0 (600–619), +4000 (700–719) and stays; BR = −BL.
Tables: `DS:7C3E` s16[10][13] layouts: `2 3 1 4 7 10 5 8 6 2 9 1 2 / 11 4 1 6 8 9 2 5 7 2 3 1 2 / 1 5 1 3 8 11 6 7 2 4 9 1 2 /
3 1 4 9 2 8 6 1 3 8 10 2 1 / 9 10 5 1 6 2 3 7 4 2 8 2 1 / 2 3 8 10 5 11 4 7 1 6 9 2 1 / 1 5 9 1 4 2 3 10 8 4 1 9 1 /
2 4 7 11 2 5 8 4 1 7 6 2 1 / 2 6 8 10 6 1 3 7 5 1 4 9 2 / 1 3 1 6 1 4 9 10 6 8 2 3 1`. `DS:7D42` s16[61] height deltas
per 10 segments: `1 -2 2 -2 2 -2 2 -1 0 1 0 1 0 -1 0 1 0 1 -1 2 1 0 1 -1 2 -1 0 1 -1 1 1 -1 0 -1 1 0 1 2 -1 0 0 1 -1 2 -1 2
-1 0 1 1 0 -1 0 1 1 -1 0 -1 1 0 1`.

### 0dcb:2866 track_build_all
Changed (= SR1 track_build_all 2645:0d48; listed under game_flow in sr2_modules.csv — **claimed here**).
`far void track_build_all(int full)`: `title_and_setup` and `new_game` call it with 1, `load_game_screen` with 0.
```c
int lvl = DS_ADD8;                                          /* [bp-4] */
if (DS_AD1C == -2) { DS_9BC2 = FAR(0x3723,0); DS_AD0C = FAR(DS,0x7DDA); DS_ADA2 = FAR(DS,0x7DEC); }   /* A000/A200/A400 */
else { DS_9BC2 = DS_AD10; DS_AD0C = DS_AD14; DS_ADA2 = FAR(0x371C, 0x0024); }                         /* parked */
build_banks();
if (full) {                                                 /* ---- town 0…0xB3 ---- */
    for (i = 0; i < 0xB4; i++) { /* exactly SR1 §4.2 town loop (same rnd calls/order) with lvl, except:          */
        /* C[i] = 1, or 9 when (i+1)%((lvl+1)*4)==0;  palm bit is 0x8000 instead of 0x20:                           */
        /*   rnd(lvl*0x28+6)==0 && !(L[i]&0x10) → L[i]|=0x8000;  rnd(lvl*0x28+7)==0 && !(R[i]&0x10) → R[i]|=0x8000 */
    }
    C[10 + rnd(0x14)] |= 0x4000;  C[35 + rnd(0x0F)] |= 0x8000;   /* town billboards, this order */
    C[65 + rnd(0x14)] |= 0x4000;  C[90 + rnd(0x1E)] |= 0x8000;
    C[115 + rnd(0x14)] |= 0x4000; C[140 + rnd(0x14)] |= 0x8000;
    R[0x2C] |= 1; L[0x6C] |= 1; L[0x9C] |= 1;               /* location signs; no town road works any more */
    DS_A9F8 = DS_4A84 (u32);                                /* remember the RNG state for the race road */
} else DS_4A84 = DS_A9F8 (u32);                             /* rebuild the same race road */
int cb = 0 /*[bp-2]*/, ca = 0 /*[bp-0xA]*/;
for (i = 0xB4; i < 0x3DE; i++, ca++, cb++) {                /* ---- race road ---- */
    HGT[i] = YAW[i] = C[i] = L[i] = R[i] = 0;
    if ((i+1) % ((DS_ADD8+1)*4) == 0) C[i] = 8;
    if (i % ((lvl+1)*4) == 0) { L[i] |= 1; R[i] |= 1; }     /* edge posts */
    if ((i+2) % ((lvl+1)*8) == 0) C[i] |= 2;                /* telephone pole */
    if (rnd(lvl*200+7) == 0) L[i] |= 0x8000;                /* palm left (no rnd for a right palm) */
    if (BR[i] < 0) { R[i] |= 0x8000; R[i] &= ~1; }          /* palm, no post, where the right side drops */
    if (i > 0x190 && rnd(0x40) == 0 && ca++ > 0x46) { C[i] |= 4; LANE[i] = rnd(0x10) + 1; ca = 0; }
    if (i > 0x190 && rnd(0x50) == 0 && !(C[i] & 4)) { C[i] |= 0x1000; LANE[i] = rnd(0x10) + 1; }
    if (lvl == 0) { if (rnd(0x23)==0) { L[i]|=0x40; L[i-1]|=0x40; L[i-2]|=0x40; }       /* palms NOT removed */
                    if (rnd(0x19)==0) { R[i]|=0x40; R[i-2]|=0x40; } }
    else { if (rnd(lvl*100+0x23)==0) L[i]|=0x40;  if (rnd(lvl*100+0x19)==0) R[i]|=0x40; }
    if (rnd(lvl*0x4B+4)==0 && !(L[i]&0x40)) { L[i]|=0x80; if (rnd(2)==0) L[i]|=2; }
    if (rnd(lvl*0x4B+6)==0 && !(R[i]&0x40)) { R[i]|=0x80; if (rnd(2)==0) R[i]|=2; }
    if (rnd(lvl*100+0x15)==0) L[i]|=4;   if (rnd(lvl*100+0x19)==0) R[i]|=4;
    if (rnd(lvl*100+0x17)==0 && !(L[i]&4)) L[i]|=8;   if (rnd(lvl*100+0x1E)==0 && !(R[i]&4)) R[i]|=8;
    if (rnd(lvl*0x32+2)==0) { L[i]|=0x100; if (rnd(2)==0) L[i]|=0x200; }
    if (rnd(lvl*0x32+2)==0) { R[i]|=0x100; if (rnd(2)==0) R[i]|=0x200; }
    if (i > 0xF0 && cb > 0x46) {                            /* bank markers where a bank rises next segment */
        if (rnd(lvl*100+0x50)==0 && BL[i+1] > 0) { L[i]|=0x10; L[i-1]|=0x1000; cb = 0; }
        if (rnd(lvl*100+0x50)==0 && BR[i+1] > 0) { R[i]|=0x10; R[i-1]|=0x1000; cb = 0; }
    }
}
L[0x17B] &= 0x1811; R[0x17B] &= 0x1811; C[0x17B] |= 0x10;  /* drag-race finish line (no banner) */
int s = rnd(0x8C) + 0xDC; if (R[s] & 2) s--;               /* one overhead sign only */
L[s] &= 0x9811; R[s] &= 0x9811; C[s] |= 0x8000;
for (i = 0xDC; i + 0x5A < 0x17C; ) {                       /* one hill pair 220…236, 257…273 */
    for (k = 0; k < 0x11; k++) HGT[i++] = DS_7C1C[k];  i += 0x14;
    for (k = 0; k < 0x11; k++) HGT[i++] = -DS_7C1C[k]; i += 0x1E;
}
C[0x181] |= 0x100;                                          /* speed-limit sign */
if (!full) rnd(-1);                                         /* reseed from the clock */
```
`DS:7C1C` s16[17] = `1 2 3 4 5 4 3 2 1 0 -1 -2 -3 -4 -3 -2 -1`. Page descriptors (18 bytes, VGA): `DS:7DDA` =
{0x140, 0xC8, 0x1F40, 0, **0xA200** (+8 segment), 0, 0, 0x28, 0xFE}, `DS:7DEC` same with 0xA400; `+6` = offset.

### 0dcb:33ac aq_build
New. `far void aq_build(void)` — Aqueduct flags (deterministic, no rnd), `AQ` = word array 3726:4E62[0x172]:
```c
for (i = 6; i < 0x172; i += 0x18) AQ[i] = AQ[i-1] = AQ[i-2] = AQ[i-3] = 2;   /* word stores */
for (i = 1; i < 0x172; i += 6) AQ[i] |= 1;
AQ[4] |= 0x10; AQ[76] |= 0x10; AQ[220] |= 0x10; AQ[316] |= 0x10; AQ[364] |= 0x10;
AQ[124] |= 0x80; AQ[268] |= 0x80;
for (i = 0x12; i < 0x172; i += 0x18) AQ[i-1] |= 4;
AQ[137] &= ~4; AQ[281] &= ~4; AQ[137] |= 0x100; AQ[281] |= 0x100;
for (i = 0x12; i < 0x172; i += 0x18) AQ[i-2] |= 8;
AQ[136] &= ~8; AQ[280] &= ~8; AQ[136] |= 0x200; AQ[280] |= 0x200;
for (i in {5,34,59,100,179,214,322}) AQ[i] |= 0x20;
for (i in {23,82,130,155,238,262,358}) AQ[i] |= 0x40;
AQ[341] |= 0x400;
for (i = 0; i < 0x172; i++) AQM[i] = AQ[0x171 - i];          /* 3726:628E */
```
Bit meanings belong to the Aqueduct draw part (`a094`/`9ce8`); `aq_lane_target` treats 0x10 and 0x80 as obstacles.

### 0dcb:3516 road_pages_init
Changed (= SR1 road_pages_init 2645:224e; now far, called by `race_run` 1a21:77f4 instead of road_first_frame):
VGA (`DS_AD1C == -2`): `DS_7E02 = 1; DS_9DC2 = 0xA200; DS_7E04 = 2; ega_show_page(2);` (SR1 kept the current pages);
else `DS_9DC2 = DS_AD0C->+8 (seg); DS_9BC0 = DS_AD0C->+6 (off)`. Remap: 8236→AD1C, 5E3E→7E02, 5E40→7E04, 787A→9DC2,
767C→9BC0, 8188→AD0C, 21a0:182e→2f32:1956; descriptor seg/off at +8/+6 (SR1 +0x26/+0x24).

### 0dcb:354e road_page_flip
= SR1 road_page_flip (2645:2288), §4.7; DS remap: 8236→AD1C, 5E3E→7E02, 5E40→7E04, 787A→9DC2, 767C→9BC0, 8188→AD0C,
822A→AD10, 5E42→7E06, blit slot 78A2→A0AE, show_page 21a0:182e→2f32:1956; descriptor seg +0x26→+8, off +0x24→+6.

### 0dcb:35c4 blit_clip_y
= SR1 blit_clip_y (2645:22fc), ../StreetRod/port/spec/race_render.md §4.11; DS remap: SR1 DS:5E50 → SR2 DS:7E14, DS:5E52 → DS:7E16.

### 0dcb:3602 blit_clip_xy
Changed: like SR1 (2645:2331, §4.11) with remap 5E4E → 7E12, 5E50 → 7E14, 5E52 → 7E16, but the bottom clip is now
inclusive: `if (DS_7E16 < r->dsty + r->h) r->h = DS_7E16 - r->dsty + 1;` (SR1 / SR2 blit_clip_y: no `+1`).
Left clip unchanged (`w -= d; srcx += d + 7; dstx = xmin`).

### 0dcb:3664 road_vline
= SR1 road_vline (2645:2381), ../StreetRod/port/spec/race_render.md §4.11; DS remap: SR1 DS:5E4E/50/52/54 → SR2 DS:7E12/7E14/7E16/7E18, line slot DS:78D2 → DS:A0DE.

### 0dcb:36ce clip_set_ymin0
= SR1 clip_set_ymin0 (2645:23f0), ../StreetRod/port/spec/race_render.md §2; DS remap: SR1 DS:5E50 → SR2 DS:7E14.

### 0dcb:36d6 clip_set_mirror
Changed values: `xmin = 0xF0, ymin = 0x16, xmax = 0x13F, ymax = 0x2A` (DS 7E12/7E14/7E18/7E16). SR1: x 0xF2…0x13D.
The SR2 mirror clip is the full x 240…319 strip.

### 0dcb:36f0 clip_set_view
= SR1 clip_set_view (2645:2410), ../StreetRod/port/spec/race_render.md §2; DS remap: SR1 DS:5E4E/50/52/54 → SR2 DS:7E12/7E14/7E16/7E18 (values 0, 0x12, 0x65, 0x13F unchanged).

### 0dcb:370a mirror_bg_prepare

`near void mirror_bg_prepare(int with_sky)` — new (SR1 loaded LIB2 1009/1243 as pictures and blitted them per
frame; SR2 pre-blits them once into the off-screen page).
```c
p = pic_get(2005, 2);                                   /* LIB3 #5 80x27 mirror frame/background */
Rect r = {80, 27, 0, 0, 232, 102};  A0AE(p, page ADA2, &r, 1);  heap_pop_hi(1);
if (with_sky) {
    p = pic_get(DS_ADD6 == 0x10 ? 2037 /* #37 80x20 */ : 2063 /* #63 80x11 */, 2);   /* mirror sky */
    r.h = p->h; r.srcx = r.srcy = 0; r.dsty = 102 + 27;  /* (232,129), w stays 80 */
    A0AE(p, page ADA2, &r, 1);  heap_pop_hi(1);
}
```
Called with 1 by road_load_graphics, 0 by 8840 (mode 8 has no mirror sky).

### 0dcb:37ce road_load_graphics

Changed (SR1 2645:2429, §4.15): all pictures from LIB3, new race types, police/opponent tables from block 8/7.
No heap_mark_reset at entry. Full SR2 behaviour:
```c
far void road_load_graphics(void)
{
    Rect r;
    if (!DS_B7AE || DS_ADD6 != 0x10) {
        p = pic_get(2002, 2);                       /* #2 320x52 sky */
        r = {p->w, p->h, 0, 0, 0, 0x66}; A0AE(p, page AD0C, &r, 1); heap_pop_hi(1);
        DS_9BE4 = pic_get(2055, 1);                 /* #55 320x37 horizon panorama */
        if (!DS_B7AE) {                             /* cruising: rooftops */
            DS_9B0C = pic_get(2004, 1);             /* #4 184x4m */
            r = {w, h, 0, 0, 0x2C, 0x0C}; A0B6(DS_9B0C, DS_9BE4, &r, 1); heap_pop_lo(1);
        }
        p = DS_9BE4; r = {p->w, p->h, 0, 0, 0, 200 - p->h};
    } else {                                        /* racing on Mulholland drive */
        p = DS_9BE4 = pic_get(2056, 1);             /* #56 320x118: only the top 98 rows */
        r = {320, 98, 0, 0, 0, 0x66};
    }
    A0AE(p, page AD0C, &r, 1); heap_pop_lo(1);
    int loc = DS_B7AE ? 0 : DS_1BC8;  if (loc == 4) loc = 2;
    p = pic_get(DS_7E1A[loc], 2);                   /* sign sheet 264x43: 2047, 2033, 2032, 2034 */
    r = {0x108, 0x2B, 0, 0, 0, 0};  dst = page ADA2;
    if (DS_AD1C == -2) { r.dsty = 100; dst = page 9BC2; }
    A0AE(p, dst, &r, 1); heap_pop_hi(1);
    if (!DS_B7AE) { DS_96AE = pic_get(2049, 2); DS_96B2 = pic_get(2050, 2); }  /* billboards #49/#50 264x43,
                                                     kept as pictures (drawn by 0dcb road code), not blitted */
    DS_B8CC = pic_get(DS_78E0[DS_AB28].id, 1);      /* hood/dash overlay of the player's car, see below */
    DS_9A52 = pic_get(2003, 1);                     /* #3 88x82m A-pillar (SR1 1006) */
    mirror_bg_prepare(1);
    if (!DS_B7AE) return;
    int cls = (s8)DS_BAAC[DS_9B0A * 10];  Row *e = &DS_AA9E[cls];
    start_props_load();                             /* 0dcb:a5aa */
    if (DS_9730 == 0 || !(DS_ADD6 & 2)) {           /* demo mode skips the reload in event17 races */
        B7B6 = pic_get(e->side, 1); B7BA = pic_get(e->r34, 1); B7BE = pic_get(e->rear, 2);
        for k<3: B7C2[k] = A0EA(B7BE, e->recA[9+k]);
        for k<3: B7CE[k] = A0EA(B7CA /* = B7C2[2] */, e->recB[9+k]);
        B7DA = pic_get(e->front, 1);
        for k<3: B7DE[k] = A0EA(B7DA, e->recC[9+k]);
    }
    Row *pz = DS_AAFA;                               /* police row 5 */
    DS_9A94 = B7B6;                                  /* police side = opponent side */
    DS_9A98 = pic_get(pz->r34, 1);                   /* 2035 */
    DS_9A9C = pic_get(pz->rear, 1);                  /* 2036 */
    DS_9A9C = DS_9AA0 = A0EA(DS_9A9C, pz->recA[0]); /* mask 0xF6 */
    heap_pop_lo(1);                                  /* frees the unshrunk 2036 */
    DS_9AA4 = DS_9AA8 = DS_9AAC = DS_9AA0;
    DS_9AB8 = pic_get(pz->front, 1);                 /* 2039 */
    for k<3: DS_9ABC[k] = A0EA(DS_9AB8, pz->recC[k]);  /* 4F 46 22 */
}
```
`DS:7E1A[5]` = 2047, 2033, 2032, 2034, (0x14 unused) (SR1 `DS:5E62` 1258/1237/1236/1238).
`DS:78E0[6]` = `{u16 id, u16 x, u16 y}`: {2061,64,93} {2020 #20 256×10m,64,90} {2062,64,93} {2059 #59 40×25,104,36}
{2025 #25 40×33,200,28} {2060 #60 32×44,184,18}; index `DS:AB28` = byte 1 of the player model's block-2 record
(set in drive_to_event / race setup); x,y are read by the drawing code (0dcb ~ line 19211 of the decompile).
SR1 start light 1013 and the hood line 1007 have no direct equivalent here (see a5aa and DS:78E0).

### 0dcb:3cca road_draw_sky_mulholland

New. `near void(void)`. Replaces `road_draw_sky` when `mode == 0x10` (one full-height panorama on page 1,
no separate horizon strip, no ground fill).

```c
int dy = cam_h - HGT_raw[seg - 4];          /* word 4726:46A6 + 2*(seg-4) (no +0xC0) */
if (dy > 8) dy = 8; else if (dy < -8 && fall_side == 0) dy = -8;   /* no lower clamp while falling */
int hy = road_proj_y(0xF0, 2000, dy);
Rect r; r.srcy = 0x6E - dy; r.h = 199 - r.srcy; if (r.h > 0x53) r.h = 0x53; r.dsty = 0x12;
int a, b;                                    /* scroll by heading<<1, exactly SR1 §4.9 scroll(): */
if (heading > 0) { a = ((heading << 1) % 0x140) & 0xFFF8; b = 0x140 - a; }
else             { b = ((abs(heading) * 2) % 0x140) & 0xFFF8; a = 0x140 - b; }
r.w = b; r.srcx = 0; r.dstx = a;
blit_clip_y(&r);                             /* 35c4 */
A0AE(AD0C, BACK, &r, 1);
r.w = a; r.srcx = b; r.dstx = 0;  A0AE(AD0C, BACK, &r, 1);       /* (h/srcy/dsty as clipped) */
int bottom = r.h + 0x12;
DS_975E = hy + 8; DS_9856 = 14;
for (k = 14; k != 0; k--) if (Y[k] < DS_975E) { DS_975E = Y[k]; DS_9856 = k; }   /* no ≥0x12 clamp */
if (fall_side != 0 && bottom < 0x65) A0E6(bottom, 0x0E);
```

### 0dcb:3e66 road_draw_sky

Changed vs SR1 `road_draw_sky` (2645:2982), ../StreetRod/port/spec/race_render.md §4.9. DS remap: 8ACA→B7AE,
7D3C→A56A, 8188→AD0C (sky source = page 1), BACK: 5E3E→7E02, 8188/82B4→AD0C/ADA2, [78A2]→A0AE, [78DA]→A0E6,
22fc→35c4, 7884 row_y→9EF0, 6E04→975E, 6E06→9856. Town path (`!in_race`) unchanged. Race path differences:
1. `hy = road_proj_y(0xF0, 2000, (mode & 6) ? 0 : cam_h - HGT_raw[seg-1])` (word 4726:46A6+2(seg−1); SR1 passed 7D3E).
2. Clamp: `if (hy >= 0x52 || hy <= 0x12) hy = (hy < 0x12) ? 0x12 : 0x52;` — quirk: hy == 0x12 becomes 0x52.
3. The second sky blit (wrap part) is done only `if (hy > 0x12)`.
4. Ground fill: `A0E6(max(DS_975E, 0x12), 9)` (DS_975E itself stays unclamped).
Rect formulas (h = hy−0x1E, srcy = 0xB8−hy, horizon h 0x14 srcy 0xA3 dsty hy−0xC, scroll ×2/×4) are as SR1.

### 0dcb:4154 shape_draw
Changed (SR1 2645:2c0a; FORMATS.md writes "1dcb:4154" = Ghidra segment, file address is **0dcb:4154**). Consumes an
SR2_SHP format-A set (or any `{u16 n; LodRec far *recs}` header) instead of SR1's `{nsizes, ShapeSize*}`.
`far void shape_draw(int x, int y, int lod, ShpSet far *set, int side)`
```c
typedef struct { u8 colour, count; } Part;                       /* colour = EGA palette index */
typedef struct { Part part[6]; struct {s8 dx, dy, w;} far *spans; } LodRec;   /* 16 bytes, spans at +12 */
typedef struct { u16 n_lods; LodRec far *recs; } ShpSet;           /* recs filled by the loader */

if (set->n_lods <= lod) return;                 /* signed compare */
LodRec r = set->recs[lod];                      /* 16-byte copy */
Span far *s = r.spans;
for (k = 0; k < 6; k++) {
    int n = r.part[k].count;  if (n <= 0) return;  /* count 0 ends the record */
    set_colour(r.part[k].colour);               /* [A0F2] */
    for (; n; n--, s++)
        if (side > 0) hline(x + s->dx + s->w, x + s->dx, y + s->dy);   /* NB (right, left) order */
        else          hline(x - s->dx - s->w, x - s->dx, y + s->dy);   /* mirrored, (left, right) */
}
```
Differences vs SR1: colour comes from the LOD's part list (up to 6 colour runs, spans in order), not per line; mirror
when `side <= 0` (SR1: `side == -1`); span = 3 signed bytes `{dx,dy,w}` (SR1 4 bytes with colour); hline argument
order. Faithful quirk: because ega_hline rejects before swapping, an unmirrored (`side > 0`) span that crosses
xmin or xmax is dropped entirely, while a mirrored span is clipped. Keep the argument order exactly.

### 0dcb:4268 slope16
= SR1 slope16 (2645:2cfc), ../StreetRod/port/spec/race_render.md §4.10; no DS globals.

### 0dcb:4294 quad_fill

Changed (replaces SR1 `quad_fill` 2645:2d20 scanline code by the generic polygon filler; same argument order).
`near void(int c, int xa, int xb, int ya0, int yb0, int yb1, int ya1)`:
```c
int pts[8] = { xa, ya0,  xb, yb0,  xb, yb1,  xa, ya1 };   /* left side xa: ya1(top)..ya0, right side xb: yb1..yb0 */
u8 buf[64]; int cnt;
if (poly_spans(pts, 4, buf, &cnt)) A0F6(buf, cnt, c, c);
road_fillmode_begin();                                      /* always */
```
(SR1's uninitialised-accA bug is gone; pixel coverage = 7f30's rasteriser.)

### 0dcb:42fe sign
= SR1 sign (2645:2f2b), ../StreetRod/port/spec/race_render.md §2 (returns 1/0/-1); no DS globals.

### 0dcb:4322 road_draw_band_front

Changed vs SR1 `road_draw_band_front` (2645:2f48) §4.10. DS remap: pointers per the table above (6E08→96B6,
6E10→96BE, 6E18→96CE, 6E1C→96D4, 6E1A→96D0, 6E1E→96D8, 6E0A→96B8, 6E12→96C0, 6E0E→96BC, 6E16→96C4,
6E28→96EE, 6E20→96E2, 6E24→96E6, 8B82→96F0); `hline(x1,x2,y,c)` → `A0F2(c); A0E2(x1,x2,y)`. Loop condition
`n != 0 && y < 0x66`. The cross path (`C & 1`: intersections return, shoulders 4/9, colour-2 lines with the
uninitialised `G`) is unchanged. Differences:
1. Outer edges `xol/xor` (+ slopes, + per-line advance) are computed when `(C & 1) || mode == 0x10`.
2. Dash (`C & 8`): `w = *96EE`, but `w = *96EE << 3` and colour 2 instead of 4 when `(mode & 6) && DS_A7A4 < start_seg + 5`
   (A7A4 = seg+row, set by 6a32: wide start-grid stripes in Grudge Night events). Dash hline drawn only `if (xl < 0x13F)`.
3. Non-cross line (replaces SR1's single asphalt hline), per scanline in this order:
```c
if (mode == 0x10 && !(C & 0x80) && HGT_raw[seg] - 0x0B <= cam_h) {   /* word 4726:46A6+2*seg */
    A0F2(2); A0E2(xol, xl, y); A0E2(xr, xor_, y);                   /* rails / kerbs */
}
A0F2(9);                                                              /* grass now drawn explicitly */
if (!(mode & 0x10)) { A0E2(0, xl, y); A0E2(xr, 0x13F, y); }
else {
    if (row > 9) {   /* recomputed every scanline; no net change of 96F0 */
        L16 = 0; for (k = 14; k > 9; k--) if (!(C[k] & 0x80) && (C[k-1] & 0x80)) L16 = 1;   /* C[k] = flags of seg+k */
        kk = 16 - row;
    }
    int ext = (L16 && row >= 10) ? word(DS:7E22 + 4*kk) : -1;
    if (YT1[q] == Y[q]) A0E2(ext < 0 ? 0 : xl - ext, xl, y);
    if (YT2[q] == Y[q]) A0E2(xr, ext < 0 ? 0x13F : xr + ext, y);
}
int f = C & 0x10;
A0F2((f && (mode != 0x10 || seg >= 0x3AD)) ? 0x0F : 1);             /* finish band; Mulholland only from seg 0x3AD */
A0E2(xl, xr, y);
```
(`ext < 0` above only means "not used"; the real code tests `L16 == 0 || row < 10`.)

### 0dcb:4852 road_draw_band_mirror

Changed vs SR1 `road_draw_band_mirror` (2645:324d) §4.10, same remaps as 4322. Loop `n != 0 && y < 0x2B`.
Cross path unchanged. Differences:
1. Outer edges computed when `(C & 1) || (mode & 0x10)`, but **advanced per line only when `C & 1`** (in Mulholland
   non-cross rows they stay at row p's values).
2. Dash: `w = *96EE` (mirror table entries are all 1; SR1 0), `<< 2` and colour 2 when `(mode & 6) && DS_9A92 < start_seg + 5`
   (9A92 = seg−row, set by 5aee); no x test.
3. Non-cross line:
```c
if (mode == 0x10 && !(C & 0x80) && HGT_raw[seg] - 0x0B <= cam_h) { A0F2(2); A0E2(xol - 6, xl, y); A0E2(xr, xor_ + 6, y); }
A0F2(9);
if (!(mode & 0x10)) { A0E2(0xF0, xl, y); A0E2(xr, 0x13F, y); }      /* SR1: 0xF2 / 0x13D */
else { if (YT1[q] == Y[q]) A0E2(0xF0, xl, y); if (YT2[q] == Y[q]) A0E2(xr, 0x13F, y); }
int f = C & 0x10;
A0F2((f && ((mode != 0x10 && seg >= 0x169) || (mode == 0x10 && seg >= 0x3AD))) ? 0x0F : 1);  /* SR1: never white */
A0E2(xl, xr, y);
```

### 0dcb:4c1c town_walls_front

= SR1 `town_walls_front` (2645:34da), ../StreetRod/port/spec/race_render.md §4.12, with `quad_fill` → 0dcb:4294
(same 7 arguments; the storey quad is written `4294(4, a, b, f, e, (e-d)/k + e, (f-c)/k + f)` with k = (F&8) ? 1 : 2,
identical results). DS remap: 6E20→96E2, 6E24→96E6, 8B82→96F0 (`C[-1]&4` = low byte of `96F0[-1]`), 5E4E→7E12,
5E54→7E18; `hline(..,c)` → `A0F2(c)` + `A0E2(x1,x2,y)`.

### 0dcb:4e70 town_walls_mirror

= SR1 `town_walls_mirror` (2645:37af) §4.12 with the same changes/remaps as 4c1c (returns unless `C & 1`; `Fn` =
`ptr[+1]`, `C[+1]&4`; left storeys "half" (k = 2), right storeys "double" (k = 1)).

### 0dcb:504a town_walls_front_left

= SR1 `town_walls_front_left` (2645:39de) §4.12; DS remap: 6E12→96C0, 6E0A→96B8, 6E1E→96D8, 6E1A→96D0, 6E0C→96BA,
6E14→96C2; calls 4c1c.

### 0dcb:507a town_walls_front_right

= SR1 `town_walls_front_right` (2645:**3a0d**) §4.12 (the matcher paired it with 3a86 by bytes; it calls
`town_walls_front`). DS remap: 6E0E→96BC, 6E16→96C4, 6E1A→96D0, 6E1E→96D8, 6E14→96C2, 6E0C→96BA.

### 0dcb:50a8 road_draw_row_front

= SR1 `road_draw_row_front` (2645:3a3b) §4.8; DS remap: 8ACA→B7AE; callees 504a, 507a, 4322.

### 0dcb:50c4 town_walls_mirror_left

= SR1 `town_walls_mirror_left` (2645:3a57) §4.12; remap as 504a; calls 4e70.

### 0dcb:50f4 town_walls_mirror_right

= SR1 `town_walls_mirror_right` (2645:**3a86**) §4.12 (calls `town_walls_mirror`); remap as 507a.

### 0dcb:5122 road_draw_row_mirror

= SR1 `road_draw_row_mirror` (2645:3ab4) §4.14: 50c4, 50f4, 4852 (no in_race test; walls gated by `C & 1` inside 4e70).

### 0dcb:512e shape_draw_dot
Changed data only (SR1 2645:3abf): `far void shape_draw_dot(int x,int y)` = `fillmode_begin(); shape_draw(x, y, 0,
(ShpSet far*)&DS_7E94, 1); fillmode_end();`. `DS:7E94` = `{1, far DGROUP:7E84}`, LodRec `DS:7E84` = part (colour 0x0B,
count 5), spans at exe `3725:0000` (image 0x37250): `{-1,-2,3} {-2,-1,5} {-2,0,5} {-2,1,5} {-1,2,3}` (5×5 cyan disc
centred on (x,y)). Callers: `1a21:11f6 wheel_knob_crash`, `1a21:1288 wheel_draw`.

### 0dcb:5158 scenery_draw_shape
Changed (SR1 2645:3aeb): 6th parameter and far set table.
`near void scenery_draw_shape(int x,int y,int depth,int side,int type,int noclamp)`
```c
int lod = 9 - depth;  if (lod > 6) lod = 6;  if (lod < 0) lod = 0;
if (noclamp == 0 && y > 0x65) y = 0x65;               /* SR1: always clamped */
shape_draw(x, y, lod, DS_8172[type] /* ShpSet far*, table of far ptrs indexed by SR2_SHP set id */, side);
```
`type` = SR2_SHP set id = SR1 type number (0 palm … 0x13, same pictures, see SR1 §5.4) plus new ids 20…22.
Only the ids loaded for the current road mode are valid (shp_load_set); others keep stale pointers.

### 0dcb:51aa scenery_draw_hydrant
= SR1 scenery_draw_hydrant (2645:3b3a, §4.11) with the SR2 set: `near void scenery_draw_hydrant(int x,int y,int depth,int side)`:
`lod = clamp(9 - depth, 0, 7); shape_draw(x, y, lod, DS_817A /* = DS_8172[2], set 2, 8 LODs */, side);` no y clamp.
(Classified "new" by the matcher; behaviour = SR1 apart from the far set pointer.) Set 2 is loaded only in mode 0.

### 0dcb:51e0 scenery_draw_bush_group
New. `near void scenery_draw_bush_group(int x,int y,int depth,int side)` — five bushes (sets 8 "bush" and 7 "small
bush"), three at the base and two one step farther (smaller) above them. Tables: `T1 = DS:7E9A {s16 a,b}[13]`
= (13,13)(11,8)(9,6)(7,5)(4,5)(3,3)(2,3)×7; `T2 = DS:7ECE {s16 h,w}[13]` = (15,17)(13,15)(10,12)(9,10)(7,10)(5,10)(5,10)(2,10)×6.
```c
int d2 = depth + 2;
if (depth == 0) x += side * 100;  if (depth == 1) x += side * 40;  if (depth == 2) x += side * 10;
int W = T2[depth].w, H = T2[depth].h;
scenery_draw_shape(x,                               y,     depth, side, 8, 1);
scenery_draw_shape(x + W * side,                    y,     depth, side, 7, 1);
scenery_draw_shape(x + (W + T1[depth].a) * side,    y,     depth, side, 8, 1);
scenery_draw_shape(x + (W >> 1) * side,             y - H, d2,    side, 7, 1);
scenery_draw_shape(x + ((W >> 1) + T1[d2].b) * side, y - H, d2,   side, 8, 1);
```
(16-bit imul for the products.) Callers 5aee (mirror) and 6a32 (front), `side` = ±1.

### 0dcb:52e2 scenery_draw_sign_picture
Changed (SR1 2645:3b79, §4.11). Size table `DS:5E9C` → `DS:7F02` (same values). Source per kind:
| kind | SR1 | SR2 source picture, srcy |
|---|---|---|
| 1 (location sign sheet) | page 2, srcy 0x66 | VGA (`AD1C == -2`): page 0 `DS:9BC2`, srcy **100**; other drivers: page 2 `DS:ADA2`, srcy 0 |
| 2 (town billboard A) | page 0 (VGA) srcy 0x64 | own picture `DS:96AE` (LIB3 #2049, loaded by road_load_graphics), srcy 0 |
| 3 (town billboard B) | page 0 srcy 0x8F | own picture `DS:96B2` (LIB3 #2050), srcy 0 |
other kinds: source and srcy uninitialised (never used). Destination = back page (`DS:7E02 == 1` ? `AD0C` : `ADA2`),
plain blit `[A0AE](src, dst, &r, 1)`. dstx/dsty/xclip logic unchanged (dsty = y − h/(align==0 ? 2 : 1), signed divide).

### 0dcb:5404 scenery_draw_truss
New (road mode 0x10 only, segments with `C & 0x80`). Wire-frame steel truss between the far row (frame 1) and the
near row (frame 2), all lines colour 10 through clip_line (7de4), in fill-mode-off.
`near void scenery_draw_truss(int xl1,int xr1,int yt1,int yb1,int xl2,int xr2,int yt2,int yb2,int parity,int near_flag)`
```c
fillmode_end();
for (int n = *DS_96EE + 1; n != 0; n--) {          /* thickness = dash width of the row + 1 */
    int a = xl1 - n, b = xr1 + n, c = xl2 - n, e = xr2 + n;
    clip_line(a, yt1, a, yb1, 10);  clip_line(b, yt1, b, yb1, 10);              /* far posts */
    clip_line(xl1, yt1 + n - 1, xr1, yt1 + n - 1, 10);                        /* far top bar */
    if (parity % 2 == 0 || near_flag == 0) { clip_line(a, yb1, c, yt2, 10); clip_line(b, yb1, e, yt2, 10); }
    else                                   { clip_line(a, yt1, c, yb2, 10); clip_line(b, yt1, e, yb2, 10); }  /* diagonals */
    clip_line(a, yt1, c, yt2, 10);  clip_line(b, yt1, e, yt2, 10);              /* top chords */
    clip_line(a, yb1, c, yb2, 10);  clip_line(b, yb1, e, yb2, 10);              /* bottom chords */
}
fillmode_begin();
```
(`parity % 2` = C signed remainder.) Callers pass frame 1 = row p+1 (`96BE, 96D4, 4*96C2 - 3*96D8, 96D8`), frame 2 = row p
(`96B6, 96CE, 4*96BA - 3*96D0, 96D0`), parity = `seg - row` (front: `A55E - row`; mirror: `A55E - row + 0xE`),
near_flag = row < 4 (front) / row < 7 (mirror).

### 0dcb:555e scenery_draw_truss_end
New. `near void scenery_draw_truss_end(int xl,int xr,int ytop,int ybot)`: `fillmode_end(); for (n = *DS_96EE + 1; n; n--)
{ clip_line(xl-n, ytop, xl-n, ybot, 10); clip_line(xr+n, ytop, xr+n, ybot, 10); clip_line(xl, ytop+n-1, xr, ytop+n-1, 10); }
fillmode_begin();` — the last portal of a truss (next segment without `C & 0x80`).

### 0dcb:55ce scenery_draw_banner
= SR1 scenery_draw_banner (2645:3ccc), ../StreetRod/port/spec/race_render.md §4.11, with: the cross-bar row is
`set_colour(10); hline(xl, xr, ytop + si - 1);` (new hline API), and the shape calls get `noclamp = 0`
(`scenery_draw_shape(mid, ytop, depth, 1, type, 0)`). DS remap: SR1 DS:6E28 → SR2 DS:96EE, DS:8B82 → DS:96F0.

### 0dcb:56e8 road_draw_cockpit_overlay
Changed (SR1 2645:3e3c): only one picture. `masked_blit([A0B6], DS_9A52 /* LIB3 #2003, 88×82 A-pillar */, back,
{w = pic->w, h = pic->h, srcx 0, srcy 0, dstx 0, dsty 0x12}, 1)`; back = `DS:7E02 == 1 ? AD0C : ADA2`.
SR1's hood line (#7) is drawn by 573e in SR2.

### 0dcb:573e road_draw_hood_overlay
Changed / renamed (SR1 order match `mirror_draw_background` 2645:3edb is wrong: it no longer draws the mirror
background). Draws the car's hood picture masked onto the back page, after the mirror and before the cockpit overlay:
```c
int t = DS_AB28;                          /* dash/hood type of the player's car = byte DS:BAAD[10*model] (SR2_DATA block 2 byte 1) */
Rect r = { DS_B8CC->w, DS_B8CC->h, 0, 0, DS_78E2[3*t], DS_78E4[3*t] };
masked_blit([A0B6], DS_B8CC, back, &r, 1);
```
Table `DS:78E0 {u16 lib3_id, x, y}[6]`: (2061, 64, 93) (2020, 64, 90) (2062, 64, 93) (2059, 104, 36) (2025, 200, 28)
(2060, 184, 18); `DS:B8CC` = `pic_get(78E0[3*t], 1)` loaded by road_load_graphics (LIB3 #2061/2062 256×7, #2020 256×10:
hood lines; #2059 40×25, #2025 40×33, #2060 32×44: hood ornaments / scoops).

### 0dcb:57a6 mirror_draw_backdrop

New (successor of SR1 `mirror_draw_background` 3edb's blits; the mirror pictures now live off-screen in page 2).
`near void(void)`. Rect = `{w,h,srcx,srcy,dstx,dsty}`, all `A0AE(ADA2, BACK, &r, 1)`:
1. `{0x10, 2, 0x108, 0x66, 0x110, 0x12}` (no clip).
2. `{0x50, 0x19, 0xE8, 0x68, 0xF0, 0x14}` (no clip).
3. `mode != 8`: `{0x50, mode==0x10 ? 0x14 : 2, 0xE8, 0x81, 0xF0, Y[7] - (mode==0x10 ? 10 : 2)}` (Y[7] = DS:9EFE,
   mirror horizon row), `blit_clip_xy` (3602) then blit. `mode == 8`: call 0dcb:85a0 instead (mode-8 part).

### 0dcb:58f4 road_poly_fill

New. `near void(int x0,int y0,int x1,int y1,int x2,int y2,int x3,int y3,int x4,int y4,int x5,int y5,int n,int colour)`:
copies the 12 values to a local `pts[12]`; `if (poly_spans(pts, n, buf, &cnt)) { A0F6(buf, cnt, colour, colour);
road_fillmode_begin(); }` (n = 3, 4 or 6; unused trailing vertices are passed as 0).

### 0dcb:597e road_draw_barrier_walls

New. `near void(int phase_seg, int n, int row)`; called per row in drag-strip modes 2/4 (`mode & 6`) with
(96F4, 14, row) by 6a32 and (96F4, 7, row) by 5aee. Vertical walls on both road edges from the road up to
world height 0x79E, colour blocks of 5 segments:
```c
int v = abs(n - row - seg + phase_seg) % 10;  int c = (v < 5) ? 0x0B : 4;
road_poly_fill(XR[p], Y79E[p], XR[p], Y[p], XR[q], Y[q], XR[q], Y79E[q], 0,0,0,0, 4, c);
road_poly_fill(XL[p], Y79E[p], XL[p], Y[p], XL[q], Y[q], XL[q], Y79E[q], 0,0,0,0, 4, c);
```

### 0dcb:5a2c mirror_finish

New split-out of SR1 `road_draw_mirror`'s tail (§4.14). `near void(void)`:
`clip_set_view(); road_vline(2,0xF0,0x16,0x2A); road_vline(4,0xF1,0x16,0x2A); road_vline(4,0x13E,0x16,0x2A);
road_vline(2,0x13F,0x16,0x2A);` (SR1 y1 = 0x17) then `A0AE(BACK, DS:9BC2 (page 0 far ptr, SR1 7678),
{0x20, 0x15, 0xF8, 0x16, 8, (DS:AD1C == -2 ? 0 : 100) + 3}, 1)` (SR1 h = #9.h − 6 = 0x15; now constant).

### 0dcb:5aee road_draw_mirror

New (SR1 `road_draw_mirror` 2645:43fd successor, rewritten). `near void(void)`, called by `road_frame` after
`road_project_mirror` (not in mode 8).
```c
set mirror pointers (table above, start row 7, flag ptrs = seg-8);
clip_set_mirror();  road_fillmode_end();  mirror_draw_backdrop();  road_fillmode_begin();
for (row = 7; row != 0; row--) {
    DS_9A92 = seg - row;
    if ((mode & 0x10) && !(C & 0x80)) mirror_hillsides(row);                  /* below */
    road_draw_row_mirror();                                                   /* 5122 */
    if (!((C & 4) && (C & 1))) {
        if ((C & 0xF000) && !(mode & 0x16)) banner(XL, XR, 4*YE - 3*Y, Y, row + 5, 1, C & 0xF000);
        cross = C & 1;
        if (row < 7) mirror_objects(row, d = (row >> 1) + 7);                  /* below */
        if (!cross && mode == 0x10) mulholland_extras_mirror(row);             /* below */
    }
    if (seg - row == opp_seg && in_race) traffic_draw();                       /* af98, no fillmode toggling */
    if (mode & 6) road_draw_barrier_walls(DS_96F4, 7, row);
    advance pointers (near −2, far +2);
}
mirror_finish();                                                               /* 5a2c */
```
`mirror_hillsides(row)` (fill colour 0x0E everywhere; `o` = fall_side; xmin/xmax/ymax = 7E12/7E18/7E16):
```c
if (fall_side != 1 && Y[q] != YT1[q]) {                                        /* left */
    if (row == 1 || !(Cm1 & 0x80)) {
        poly4((xmin,YT1[q]), (XOL[q]-o,Y[q]), (XOL[p]+o,Y[p]), (xmin,YT1[p]));
        if (row == 1 && XOL[p] > 0xF0)                 tri((xmin,YT1[q]), (XOL[p],Y[p]), (xmin,Y[p]));
        else if (Y[q-1] == YT1[q-1] && XOL[q] > 0xF0)  tri((xmin,YT1[q]), (XOL[q],Y[q]), (xmin,Y[q]));
    } else poly4((xmin,YT1[q]), (XOL[q],Y[q]), (XOL[p],Y[p]), (xmin,YT1[p]));
    if (row != 1 && (Cm1 & 0x80))
        poly6((xmin,YT1[q]), (XOL[q],Y[q]), (XL[q],Y[q]), (XL[q-1],Y[q-1]), (XL[q-1],ymax), (xmin,ymax));
}
if (fall_side != -1 && Y[q] != YT2[q])  /* right: same with xmax, YT2, XOR, XR, tests XOR < 0x13F */ ;
```
(`Y[q-1]` = `96D8[-1]`, `XL[q-1]` = `96BE[-1]`; poly via `road_poly_fill`.)
`mirror_objects(row, d)` in this exact order (`shape` side ±1, last arg = flag):
1. `mode != 0x10 && L&0x20 && cpu > 3`: shape(XL, Y, d, −1, 0, 0)
2. `L&0x100`: cross: shape(XL,Y,d,−1,4,1); else skip if `mode==0x10 && Y[q]!=YT1[q]`, else
   shape(L&0x200 ? 2·XOL−XL : XOL, Y, d, −1, mode==0x10 ? 0x15 : 4, 1)
3. `!(mode&7) && L&0x800`: shape(XL,Y,d,−1,3,0) 4. `!(mode&7) && L&0x1000`: shape(XL,Y,d,+1,6,0)
5. `!(mode&6) && L&1 && Y[q]==YT1[q]`: shape(XL,Y,d,+1,1,1) 6. `!(mode&6) && R&1 && Y[q]==YT2[q]`: shape(XR,Y,d,−1,1,1)
7. `cpu > 3`: if `(mode&7) || Y[q]==YT1[q]`: { `L&0x40`: shape(XOL,Y,d,−1,0xA,0); `L&0x80`: shape(!cross && L&2 ?
   (XOL+xmin)>>1 : XOL, Y, d, −1, mode==0x10 ? 0x16 : 0xB, 0) }; if `(mode&7) || Y[q]==YT2[q]`: { `R&0x40`:
   shape(XOR,Y,d,+1,0xA,0); `R&0x80`: shape(!cross && R&2 ? (XFR+xmax)>>1 : XOR, Y, d, +1, 0x16/0xB, 0) }
   (note left uses `XOL`, not `XFL`, in the `&2` case — as coded)
8. `mode&0x10`: `L&0x10` obj51e0(XL,Y,d,1); `R&0x10` obj51e0(XR,Y,d,−1)
9. `!cross`: if `cpu > 3`: { if `(mode&1)||Y[q]==YT1[q]`: L&4 shape(XL,Y,d,−1,7,1), L&8 shape(XL,Y,d,−1,8,1);
   if `(mode&1)||Y[q]==YT2[q]`: R&4 shape(XR,Y,d,+1,7,1), R&8 shape(XR,Y,d,+1,8,1) };
   cross: `L&0x40` hydrant(XL,Y,d,−1); `R&0x40` hydrant(XR,Y,d,+1)
10. `!cross && C&2 && (mode!=0x10 || Y[q]==YT2[q]) && !(mode&6)`: shape(XOR − ((XR−XL)>>3), Y, d, +1, 9, 0)
11. `mode != 0x10 && R&0x20`: shape(XR,Y,d,+1,0,0)
12. `R&0x100`: cross: shape(XR,Y,d,+1,4,1); else skip if `mode==0x10 && Y[q]!=YT2[q]`, else
    shape(R&0x200 ? 2·XOR−XR : XOR, Y, d, +1, 0x15/4, 1)
13. `R&0x2000` (XR,+1,0x12,0); `R&0x4000` (XR,+1,0x13,0); `R&0x800` (XR,+1,3,0); `R&0x1000` (XR,−1,6,0)
14. `!(mode&7) && C&4`: shape(XL + ((X6·(XR−XL))>>4), Y, d, X6 < 9 ? −1 : 1, 7, 1)
15. `mode==0x10 && C&0x1000`: same position/side, type 0x14, flag 1
16. `C&0x100` (XR,+1,5,0); `C&0x200` (…,0x10,0); `C&0x400` (…,0x11,0); `C&0x800` (…,0xF,0)
17. `(mode & 0x11) && seg − row == DS:9E6A`: a85c((XR+XL)>>1, Y, row − 1)

(`Y` = `Y[p]`; XFR = `*96C8`.) Item 7 left `&2` case: real code is `(*96B8 + 7E12) >> 1` = (XOL + xmin)>>1.
`mulholland_extras_mirror(row)`:
```c
if ((C & 0x80) && (Cp1 & 0x80) && (96F0[+2] & 0x80))
    tunnel_5404(XL[q], XR[q], 4*YE[q]-3*Y[q], Y[q], XL[p], XR[p], 4*YE[p]-3*Y[p], Y[p], seg - row, row < 4);
if ((C & 0x80) && !(Cm1 & 0x80)) tunnel_555e(XL[p], XR[p], 4*YE[p]-3*Y[p], Y[p]);
if ((R & 0x8000) && !(C & 0x80)) guard_rail(row, -10, 7, 1);                  /* see 6a32 */
```

### 0dcb:69d4 obstacle_hit_test

New. `near int(int x, int w, int y)`: whether a near obstacle span `[x, x+w)` covers one of the player car's
wheel columns. `if (y > 0x57) { if ((x < 0x65 && x+w > 99) || (x < 0x83 && x+w > 0x81)) return -1;
if ((x < 0x140 && x+w > 0x13E) || (x < 0x122 && x+w > 0x120)) return 1; } return 0;`

### 0dcb:6a32 road_draw_front

New (SR1 `road_draw_frame` 2645:4b8c successor; mirror, cockpit and page flip moved to `road_frame` 11fc).
`far void(void)`, called by `road_frame` after `road_project_front` (not in mode 8).
```c
if (fallen) return;                                       /* DS:7F2C: nothing drawn after the cliff fall */
int o = fall_side;
set front pointers (table above, row 14; far flag ptrs = seg+14);
if (mode & 6) DS_96F4 = start_seg;                        /* phase of barrier walls / rails */
else for (k = 14; k != 0; k--)                            /* else DS_96F4 keeps its previous value */
    if ((R[k] & 0x8000) && !(R[k-1] & 0x8000)) { DS_96F4 = seg + k; break; }
clip_set_view();
if (mode == 0x10) road_draw_sky_mulholland(); else road_draw_sky();
road_fillmode_begin();
L16 = 0; for (k = 14; k > 9; k--) if ((mode & 0x10) && !(C[k] & 0x80) && (C[k-1] & 0x80)) L16 = 1;
for (row = 14; row != 0; row--) {
    DS_A7A4 = seg + row;
    if ((mode & 0x10) && !(C & 0x80)) front_hillsides(row);
    if (row <= DS_9856) road_draw_row_front(row);                    /* 50a8 */
    if (!((C & 4) && (C & 1))) {
        cross = C & 1;
        if (row < 11) front_objects(row);
        if (!cross && mode == 0x10) mulholland_extras_front(row);
    }
    if (opp_seg - row == seg && in_race) traffic_draw();             /* af98 */
    if (mode & 6) road_draw_barrier_walls(DS_96F4, 14, row);
    advance all pointers by one row (−2 bytes);
}
if (seg == opp_seg && in_race) traffic_draw();
```
`front_hillsides(row)` (colour 0x0E; `dyaw = heading − YAW_raw[seg]`, word 4726:372E+2·seg; screen 0..0x13F,
bottom 0x65; `k = 15 − row`; `W0(k)=word(7E1E+4k)`, `H0(k)=word(7E20+4k)`, `W1(k)=word(7E22+4k)`,
`H1(k)=word(7E24+4k)`, `E(k)=word(7E3A+4k)`, word values below; divisions truncate toward 0):
```c
if (dyaw > -100 && Y[q] != YT1[q]) {                                    /* left hillside */
    if (L16 && YT1[p] <= Y[p]) {
        if (Cm1 & 0x80) {
            poly6((XL[q-1],0x65), (XL[q-1],Y[q-1]), (XL[q],Y[q]), (XOL[q],Y[q]), (XOL[q]-W1,Y[q]), (E,0x65));
            if (YT1[p] < Y[p]) tri((XOL[q],Y[q]), (XOL[q]-W1/2, Y[q]-H1), (XOL[q]-W1,Y[q]));
            else poly4((XOL[q],Y[q]), (XOL[q]-W1,Y[q]), (XOL[p]-W1,Y[p]), (XOL[p],Y[p]));
        }
        if (YT1[p] < Y[p]) poly4((XOL[q],Y[q]), (XOL[q]-W1/2, Y[q]-H1), (XOL[p]-W0/2, Y[p]-H0), (XOL[p],Y[p]));
    }
    if (!L16) {
        if (row == 1 || !(Cm1 & 0x80)) {
            if (XOL[q] + 30 < XOR[q]) {
                poly4((0,YT1[q]), (XOL[q]-o,Y[q]), (XOL[p]+o,Y[p]), (0,YT1[p]));
                if (XOL[q] > 0 && ((row != 1 && Y[q-1] == YT1[q-1]) || (row == 1 && Y[q] > YT1[q])))
                    tri((0,YT1[q]), (XOL[q],Y[q]), (0,Y[q]));
            }
        } else {
            poly6((0,YT1[q]), (XOL[q],Y[q]), (XL[q],Y[q]), (XL[q-1],Y[q-1]), (XL[q-1],0x65), (0,0x65));
            poly4((0,YT1[q]), (XOL[q],Y[q]), (XOL[p],Y[p]), (0,YT1[p]));
        }
    }
}
if (dyaw < 100 && Y[q] != YT2[q]) {                                     /* right hillside, mirrored */
    if (L16 && YT2[p] <= Y[p]) {
        if (Cm1 & 0x80) {
            poly6((XR[q-1],0x65), (XR[q-1],Y[q-1]), (XR[q],Y[q]), (XOR[q],Y[q]), (XOR[q]+W1,Y[q]), (0x13F-E,0x65));
            if (YT2[p] < Y[p]) tri((XOR[q],Y[q]), (XOR[q]+W1/2, Y[q]-H1), (XOR[q]+W1,Y[q]));
            else poly4((XOR[q],Y[q]), (XOR[q]+W1,Y[q]), (XOR[p]+W1,Y[p]), (XOR[p],Y[p]));
        }
        if (YT2[p] < Y[p]) poly4((XOR[q],Y[q]), (XOR[q]+W1/2, Y[q]-H1), (XOR[p]+W0/2, Y[p]-H0), (XOR[p],Y[p]));
    }
    if (!L16) {
        if (row == 1 || !(Cm1 & 0x80)) {
            if (XOL[q] + 30 < XOR[q]) {                                   /* sic: same test as left */
                poly4((0x13F,YT2[q]), (XOR[q]-o,Y[q]), (XOR[p]+o,Y[p]), (0x140,YT2[p]));   /* sic 0x140 */
                if (XOR[q] < 0x13F && ((row != 1 && Y[q-1] == YT2[q-1]) || (row == 1 && Y[q] > YT2[q])))
                    tri((0x13F,YT2[q]), (XOR[q],Y[q]), (0x13F,Y[q]));
            }
        } else {
            poly6((0x13F,YT2[q]), (XOR[q],Y[q]), (XR[q],Y[q]), (XR[q-1],Y[q-1]), (XR[q-1],0x65), (0x13F,0x65));
            poly4((0x13F,YT2[q]), (XOR[q],Y[q]), (XOR[p],Y[p]), (0x13F,YT2[p]));
        }
    }
}
```
Words DS:7E1E… (k = 0..14 index into these; for k > 5 the reads run into the dash table — faithful):
`7E1E: 2032 2034 20 10 35 15 50 30 80 60 200 100 200 100 70 0 45 0 20 0 -20 50 -2000 0 -2000 0` then
`7E52 (dash table, index = row): 8 6 5 4 3 2 2 2 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 (7E82)`.
`front_objects(row)` in this exact order (`Y` = Y[p]; depth = row unless stated):
1. `mode != 0x10 && L&0x20 && cpu > 3`: shape(XL, Y, row, −1, 0, 0) (SR1: XOL when not cross)
2. `L&0x100`: cross: shape(XL,Y,row,−1,4,1); else skip if `mode==0x10 && Y[q]!=YT1[q]`; else `L&0x200` ?
   shape(2·XOL−XL, Y, **row+2**, −1, t, 1) : shape(XOL, Y, row, −1, t, 1), t = mode==0x10 ? 0x15 : 4
3. `!(mode&7) && L&0x800`: shape(XL,Y,row,+1,3,0) 4. `!(mode&7) && L&0x1000`: shape(XL,Y,row,+1,6,0)
5. `!(mode&6) && L&1`: cross: fillmode_end; sign_picture(XL, (Y+YE)>>1, row, 1, 0, 1); fillmode_begin —
   else if `Y[q]==YT1[q]`: shape(XL,Y,row,+1,1,1)
6. `mode==0x10 && L&0x10`: obj51e0(XL,Y,row,1)
7. `cpu > 3`: if `(mode&7) || Y[q]==YT1[q]`: { `L&0x40`: shape(XOL,Y,row,−1,0xA,0); `L&0x80`: (!cross && L&2) ?
   shape((XFL+xmin)>>1, Y, **row+3**, −1, u, 0) : shape(XOL, Y, row, −1, u, 0), u = mode==0x10 ? 0x16 : 0xB };
   if `(mode&7) || Y[q]==YT2[q]`: { `R&0x40`: shape(XOR,Y,row,+1,0xA,0); `R&0x80`: (!cross && R&2) ?
   shape((XFR+xmax)>>1, Y, row+3, +1, u, 0) : shape(XOR + ((XOR−XR)>>1), Y, row, +1, u, 0) }
8. `!cross`: if `cpu > 3`: { if `(mode&1)||Y[q]==YT1[q]`: L&4 shape(XL−2,Y,row,−1,7,1), L&8 shape(XL−2,Y,row,−1,8,1);
   if `(mode&1)||Y[q]==YT2[q]`: R&4 shape(XR+2,Y,row,+1,7,1), R&8 shape(XR+2,Y,row,+1,8,1) };
   cross: `L&0x40` hydrant(XL,Y,row,1); `R&0x40` hydrant(XR,Y,row,−1)
9. `!cross && C&2 && (mode!=0x10 || Y[q]==YT2[q]) && !(mode&6)`: shape(XOR − ((XR−XL)>>3), Y, row, +1, 9, 0)
10. `mode != 0x10 && R&0x20`: shape(XR,Y,row,+1,0,0)
11. `R&0x100`: as item 2 on the right: cross shape(XR,Y,row,+1,4,1); else skip if `mode==0x10 && Y[q]!=YT2[q]`;
    `R&0x200` ? shape(2·XOR−XR, Y, **row+3**, +1, t, 1) : shape(XOR, Y, row, +1, t, 1)
12. `R&0x2000` (XR,+1,0x12,0); `R&0x4000` (XR,+1,0x13,0); `R&0x800` (XR,+1,3,0); `R&0x1000` (XR,−1,6,0)
13. `!(mode&6) && R&1`: cross: fillmode_end; sign_picture(XR, (Y+YE)>>1, row, −1, 0, 1); fillmode_begin —
    else if `Y[q]==YT2[q]`: shape(XR,Y,row,**−1**,1,1)
14. `mode==0x10 && R&0x10`: obj51e0(XR,Y,row,−1)
15. `mode==0x10 && C&4` (rock): `s = X6 < 9 ? -1 : 1; x = XL + ((X6*(XR-XL)) >> 4); shape(x,Y,row,s,7,1);
    if (row == 1) DS_BB88 = obstacle_hit_test(s == -1 ? x - 40 : x, 40, Y);`
16. `mode==0x10 && C&0x1000`: same with type 0x14 and `DS_ADD4 = obstacle_hit_test(s == -1 ? x - 50 : x, 50, Y)`
17. `(C & 0xF000) && !(mode & 0x16)`: banner(XL, XR, 4·YE − 3·Y, Y, row, 0, C & 0xF000)
18. `C&0x100` (XR,+1,5,0); `C&0x800` (…,0xF,0); `C&0x200` (…,0x10,0); `C&0x400` (…,0x11,0)
19. `(mode&6) && seg + row == start_seg + 4`: aab0((XR+XL)>>1, Y, row)

`mulholland_extras_front(row)` (any row, after the objects):
```c
if ((C & 0x80) && (Cm1 & 0x80))
    tunnel_5404(XL[q], XR[q], 4*YE[q]-3*Y[q], Y[q], XL[p], XR[p], 4*YE[p]-3*Y[p], Y[p], seg - row + 14, row < 7);
if ((C & 0x80) && !(Cp1 & 0x80)) tunnel_555e(XL[p], XR[p], 4*YE[p]-3*Y[p], Y[p]);
if ((R & 0x8000) && !(C & 0x80)) guard_rail(row, -30, 14, 0);
```
`guard_rail(row, post, n, m)` (right side; `m` = 1 in the mirror):
```c
if ((DS_96F4 - row - seg + n) % 2 != 0) {                  /* post on every other segment */
    int x = XOR[q] + post / (row + 1);                       /* truncating division */
    int v[8] = { x, Y79E[q]-1,  x, Y[q],  XOR[q], Y[q],  XOR[q], Y79E[q]-1 };
    if (poly_spans(v, 4, buf, &cnt)) A0F6(buf, cnt, 5, 2);
}
int w[8] = { XOR[p], YE[p]-m,  XOR[p], Y79E[p],  XOR[q], Y79E[q],  XOR[q], YE[q]-m };
if (poly_spans(w, 4, buf, &cnt)) { A0F6(buf, cnt, 5, 5);
    line(w[0], w[1], w[6], w[7], 2); line(w[2], w[3], w[4], w[5], 2); }
road_fillmode_begin();
```

### 0dcb:7da2 road_fall_off_edge
New. `far void road_fall_off_edge(int dir)` (dir −1 left / +1 right; Mulholland, from road_step when `DS_7B9C`):
`for (k = 1; k < 9; k++) { DS_A56C -= k; DS_A566 -= k; DS_A564 += dir*100; road_frame(); }  DS_7F2C = 1; DS_BBF2 = 1;`
(8 frames, camera drops 36 and slides 800; `7F2C`/`BBF2` crash flags read by `6a32`, crash_show, race_results.)

### 0dcb:7de4 clip_line
New. `far void clip_line(int x1,int y1,int x2,int y2,int colour)` — clip to the rectangle 7E12..7E18 × 7E14..7E16 and
draw with the line slot `[A0DE](x1,y1,x2,y2,colour)`.
```c
if (x1 < xmin && x2 < xmin) return;  if (x1 > xmax && x2 > xmax) return;
if (y1 < ymin && y2 < ymin) return;  if (y1 > ymax && y2 > ymax) return;
if (x2 < x1) swap(P1, P2);                                   /* sort by x */
if (x1 < xmin) { y1 = line_y_at_x(x1,y1,x2,y2,xmin); x1 = xmin; }
if (x2 > xmax) { y2 = line_y_at_x(x1,y1,x2,y2,xmax); x2 = xmax; }
if (y2 < y1) swap(P1, P2);                                   /* sort by y */
if (y1 < ymin) { x1 = line_x_at_y(x1,y1,x2,y2,ymin); y1 = ymin; }
if (y2 > ymax) { x2 = line_x_at_y(x1,y1,x2,y2,ymax); y2 = ymax; }
if (all four coordinates inside the rectangle, inclusive) line(x1,y1,x2,y2,colour);
```
(The y clip may move x out again; then nothing is drawn.)

### 0dcb:7f30 clip_polygon
New. `far int clip_polygon(Point *in, int n, Point *out, int *nout)` — Sutherland–Hodgman against xmin (7E12),
then xmax (7E18), then ymin (7E14), then ymax (7E16); intermediate buffers of **16 points** each (no overflow check),
`Point = {int x, y}` (DS near). For each stage, with P = previous vertex (starts at the last one) and C = current:
```c
if (!(P outside && C outside)) {
    if (P outside || C outside) {
        emit(intersection);               /* x-edges: (E, line_y_at_x(P.x,P.y,C.x,C.y,E)); y-edges: (line_x_at_y(P..,C..,E), E) */
        if (C outside) goto next;         /* leaving: intersection only */
    }
    emit(C);                              /* inside or entering */
}
next: P = C;
```
inside = `x >= xmin`, `x <= xmax`, `y >= ymin`, `y <= ymax`. Result: `*nout = count; return count >= 1;`.
Callers (4294 town walls, 58f4, 5aee, 6a32, 8b5c, 905c, a2d8) then call `[A0F6](out, *nout, c, c)`.

### 0dcb:8288 shp_b_draw
`far void(int x, int y, int group, ShpSetB far *set)` — draws one group of an SR2_SHP **format-B** set
(FORMATS `SR2_SHP`). `set->n_groups` = word +0; `set+4` = far ptr to the array of far group pointers; a group
= 7 LOD records of 16 bytes; here the 7 records are **7 pre-rotated variants** selected by the roll
(`DS:AD68`, 0..6, from 9f9a). Record: 6 × {u8 colour, u8 count} (stop at count 0), +12 far ptr to spans
{i8 dx, i8 dy, i8 w}, consumed sequentially over the pairs.
```c
fillmode_begin();
if (group < 0 || group >= set->n_groups) return;
Lod far *g = set->groups[group];
if (DS_B190 < 0x37) {                                /* |roll| < 55° */
    Lod r = g[DS_AD68];  int rx, ry;  rot(x, y, &rx, &ry);
    for each (colour,count) pair of r while count != 0:
        A0F2(colour);
        repeat count: { s = *sp++; A0E2(rx + s.dx, rx + s.dx + s.w, ry + s.dy); }
} else {                                             /* steep roll: unrotated variant 3, rotate each span */
    Lod r = g[3];
    for each pair while count != 0: {
        c = colour; if (DS_AD1C != -2 && DS_AD1C != -6 && c == 2) c = 5;
        repeat count: { s = *sp++; rot(x+s.dx, y+s.dy, &x1,&y1); rot(x+s.dx+s.w, y+s.dy, &x2,&y2);
                        line_clip(x1,y1,x2,y2,c); }
    }
}
```
(no fillmode_end at exit.)

### 0dcb:8482 aq_draw_row_shape
`near void(int x, int y, int row, int set)` — `group = (DS_B194 == 14) ? (row+1)>>1 : (row>>1) + 6;`
`shp_b_draw(x, y, group, DS:81CE[set])` where `DS:81CE` = 3 far pointers to SR2_SHP sets 23, 24, 25
(loaded by 1a21:7446). Only called with set = 2 (set 25, 9 groups); `row` = i−1.

### 0dcb:84c4 aq_draw_night_sky
`near void(void)` — stars and three sky shapes over the sky fill (front view).
```c
A0FA(BACK->+6, BACK->+8, DS_96FA >> 1, DS:7F2E, 18, DS_AD48);   /* 18 points */
A0FA(BACK->+6, BACK->+8, DS_96FA >> 1, DS:7F76,  6, DS_AD4A);   /* 6 points  */
shp_b_draw(DS_96FA + 0x5C, 0x20, 0, DS:81CE[0]);   /* set 23 */
shp_b_draw(DS_96FA + 0xD2, 0x2D, 0, DS:81CE[1]);   /* set 24 */
shp_b_draw(DS_96FA + 0xB4, 0x16, 0, DS:81CE[1]);   /* set 24 */
```
`BACK->+6/+8` = video far pointer of the back page descriptor. Point tables (i16 x,y; exe DGROUP):
7F2E = (16,15)(95,15)(96,18)(133,24)(137,54)(160,34)(158,36)(159,36)(158,38)(137,54)(175,52)(212,17)(213,17)
(238,16)(242,14)(249,13)(244,17)(244,22); 7F76 = (56,13)(73,26)(101,17)(190,21)(204,41)(248,16).
`AD48/AD4A/AD54` = plane masks set by gfx_init (video).

### 0dcb:85a0 aq_draw_mirror_stars
`near void(void)` — called by the mirror background 0dcb:57a6 (§3) in mode 8.
`A0FA(BACK->+6, BACK->+8, -(DS_96FA >> 2), DS:7F8E, 9, DS_AD48); A0FA(…, -(DS_96FA >> 2), DS:7FB2, 3, DS_AD54);`
Tables: 7F8E = (246,27)(285,9)(262,15)(270,31)(278,17)(284,28)(280,−5)(272,4)(290,33); 7FB2 = (296,29)(290,17)(265,28).

### 0dcb:862a poly_straddles_bottom
`near int(int x, int *pts, int n)` — over the n points (x,y): `L` = any point with y == 0x65 and px < x,
`R` = any with y == 0x65 and px > x; returns `L && R` (1/0). Used for wall-opening hits.

### 0dcb:8674 poly_bottom_hit_bits
`near uint(int *pts, int n)` — over points with y == 0x65: `nl` = count px < 0x50, `nr` = count px > 0x13F,
`any` = at least one such point. `r = 0; if (nl & 1) r = 0x20; if (nr & 1) r |= 0x40; if (any && nl == nr) r = 0x60;`
return r (obstacle-under-the-car bits for `DS:A7D2`).

### 0dcb:86d2 anchor_scale_rows

`near void anchor_scale_rows(int row, int n, const u8 *masks)` — new. Derives the sprite anchor offsets of the
shrunk levels from those of the rotated base level, for all 7 angles. Tables `DS:9E08[13][7]` (s8 anchor x) and
`DS:9E94[13][7]` (s8 anchor y from the bottom), indexed [level][angle].
```c
for (k = 1; k <= n; k++) { int f = popcount8(masks[k-1]);
    for (c = 0; c < 7; c++) { X[row+k][c] = (s8)(X[row][c] * f >> 3); Y[row+k][c] = (s8)(Y[row][c] * f >> 3); } }
```
(signed multiply, arithmetic shift). Uses only the angle-0 triple of the record (popcounts are equal for all angles).

### 0dcb:8774 rot_shrink_max_size

`near int rot_shrink_max_size(Pic far *pic, u8 mask)` — new. Worst-case byte size of "rotate then shrink" for the
angles −45, −30, −15, 0 (`DS:7FBE[0..3]`):
```c
int best = 0;
for (a in DS_7FBE[0..3]) {
    DS_A540 = isin(a); DS_BA90 = isin(a + 90); DS_A7A8 = DS_A7CC = 0;    /* isin = 0dcb:02b4 (8.8, deg) */
    Pic *t = rot_alloc(0, pic, 2, &d, &d, &c, &c, d, 0);                 /* 0dcb:bb22, bbox only */
    heap_pop_hi(1);
    Pic *s = pic_alloc_shrunk(t->w, t->h, 1, mask, mask);   heap_pop_hi(1);
    best = max(best, bitmap_bytes(s->w, s->h, 1));          /* 1a21:9bac */
}
return best;
```
(The headers are read after being popped; only w/h are used.) Leaves DS:A540/BA90 = values of angle 0.

### 0dcb:8840 race_load_graphics_rot

`far void race_load_graphics_rot(void)` — new; called by race_run 1a21:77f4 **instead of road_load_graphics when
`DS:ADD6 == 8`** (Aqueduct race). No sky/horizon/sign sheet/police here.
```c
DS_B8CC = pic_get(DS_78E0[DS_AB28].id, 1);  DS_9A52 = pic_get(2003, 1);
mirror_bg_prepare(0);  start_props_load();  DS_A7AC = 1;
Row *e = &DS_AA9E[(s8)DS_BAAC[DS_9B0A*10]];  DS_A2B4 = 0;
memset X(DS:9E08)[13][7] = 0, Y(DS:9E94)[13][7] = 0;
DS_9E04 = pic_get(e->side, -1);  DS_9E64 = pic_get(e->r34, -1);  DS_9E7E = pic_get(e->rear, -1);
DS_A1D2 = A0EA(DS_9E7E, e->recA[11]);            /* level-5 base, 3/8 */
DS_A32C = pic_get(e->front, -1);
X[2][3] = opaque_w(DS_9E7E) >> 1;  X[5][3] = opaque_w(DS_A1D2) >> 1;  X[9][3] = opaque_w(DS_A32C) >> 1;
       /* opaque_w = 1a21:a6e8: w - (#trailing set mask bits of the last mask byte column, AND over rows)/DS:AD22 */
arena_alloc(rot_shrink_max_size(DS_9E7E, e->recA[9]), 1);  /* reserve on the low stack */
for (i = 0; i < 7; i++) if (i != 3) {             /* angle DS_7FBE[i]; i==3 (0 deg) = originals */
    DS_9E72[i] = A0EE(DS_9E7E, DS_7FBE[i], 2, &X[2][i], &Y[2][i], X[2][3]);
    DS_A1C6[i] = A0EE(DS_A1D2, DS_7FBE[i], 2, &X[5][i], &Y[5][i], X[5][3]);
    DS_A320[i] = A0EE(DS_A32C, DS_7FBE[i], 2, &X[9][i], &Y[9][i], X[9][3]);
}
heap_pop_lo(1);                                    /* free the reserve: room for ad40's per-frame shrinks */
anchor_scale_rows(2, 2, e->recA);  anchor_scale_rows(5, 3, e->recB);  anchor_scale_rows(9, 3, e->recC);
DS_9862 = 4;  DS_B198 = DS_9E04;  DS_9DC8 = far 3722:0000;  DS_9AD8 = far 3722:0002;  DS_BA90 = 0xFF;
DS_A7D4 = DS_BA92 = DS_9E68 = DS_B868 = DS_A7E8 = DS_AAF2 = DS_B7B0 = DS_A7A8 = DS_A7CC = DS_A540
        = DS_9E70 = DS_B8BC = DS_B8C8 = DS_A7D2 = 0;
```
Note `DS:9E72[3]`, `A1C6[3]`, `A320[3]` are exactly `9E7E`, `A1D2`, `A32C` (contiguous arrays). Per frame
`0dcb:ad40(level, angle)` returns: level 0 → 9E04, 1 → 9E64, 2 → 9E72[a], 5 → A1C6[a], 9 → A320[a];
3,4 → A0EA(9E72[a], recA[3a+level−3]); 6..8 → A0EA(A1C6[a], recB[3a+level−6]); 10..12 → A0EA(A320[a],
recC[3a+level−10]) (temporary, high stack; popped on the next call via flag DS:BA92).

### 0dcb:8b16 aq_draw_sky
`near void(void)`:
```c
int save = DS_7E16;  if (DS_9E70 == 0) DS_7E16 = DS_A7CC;     /* no roll: sky only down to the far floor y */
A0E6(0x12, 1);                                                 /* fill rows 18.. with colour 1 */
DS_96FA = (DS_A56A * 2) % 0x140;                               /* C remainder (sign of dividend) */
DS_7E16 = save;
aq_draw_night_sky();
DS_975E = 0;  DS_9856 = 0xE;
```

### 0dcb:8b5c aq_draw_box
`near void(P1..P8 as 16 ints, int solid)` — a box from 8 projected corners: P1/P2 far bottom L/R, P3/P4 far
top L/R, P5/P6 near bottom L/R, P7/P8 near top L/R. All 8 are first rotated with `rot()`.
`e(A,B) = (abs(A.x-B.x) < 3 || abs(A.y-B.y) < 3) ? 2 : 0` (thin face → filled colour 2).
```c
if (P1.x < P5.x)  if (poly_clip({P1,P3,P7,P5},4,o,&n)) A0F6(o,n,2,e(P1,P7));   /* left side  */
if (P6.x < P2.x)  if (poly_clip({P2,P4,P8,P6},4,o,&n)) A0F6(o,n,2,e(P2,P8));   /* right side */
if (P3.y < P7.y)  if (poly_clip({P3,P4,P8,P7},4,o,&n)) {                       /* top        */
        if (solid == 0 && DS_B194 == 14) DS_A7D2 |= poly_bottom_hit_bits(o,n);
        A0F6(o,n,5,e(P3,P8)); }
if (poly_clip({P6,P5,P7,P8},4,o,&n)) A0F6(o,n,2,e(P6,P7));                     /* near face  */
if (solid == 1 && poly_clip({P1,P2,P6,P5},4,o,&n) && DS_B194 == 14)            /* footprint, not drawn */
        DS_A7D2 |= poly_bottom_hit_bits(o,n);
```
(`o` = 16-point buffer.)

### 0dcb:8ece aq_wall_opening_quad
`near int(int k, uint flags, int q[8], int enable)` — returns 0 if `!enable`. Side: flags & 0x104 → left
arrays (bottom 9732/9760, top 9BC6/9CFC); flags & 0x208 → right (B170/B1A0, 9DCE/9F0E); right wins if both;
none → return 0. With `Xb,Yb` bottom and `Xt,Yt` top arrays at index k (row) and k+1 (next nearer row):
```c
d0 = (Xb[k]-Xt[k]) / 4;  d1 = (Xb[k+1]-Xt[k+1]) / 4;       /* C truncation */
if (flags & 0x0C) { q0 = (Xb[k]-d0,   lerp_y(Xb[k],Yb[k],Xt[k],Yt[k], Xb[k]-d0));
                    q3 = (Xb[k+1]-d1, lerp_y(Xb[k+1],Yb[k+1],Xt[k+1],Yt[k+1], Xb[k+1]-d1)); }
else              { q0 = (Xb[k], Yb[k]);  q3 = (Xb[k+1], Yb[k+1]); }
q1 = (Xt[k]+d0,   lerp_y(Xb[k],Yb[k],Xt[k],Yt[k], Xt[k]+d0));
q2 = (Xt[k+1]+d1, lerp_y(Xb[k+1],Yb[k+1],Xt[k+1],Yt[k+1], Xt[k+1]+d1));
return 1;
```

### 0dcb:905c aq_draw_scene
`far void(void)` — draws channel, trough, features and the opponent for the current view (N = `DS:B194`,
14 front / 7 mirror; `rot` pivot and sin/cos already set). Uses the reversed rotated arrays: `L=(9732,9760)`,
`R=(B170,B1A0)`, `TL=(9BC6,9CFC)`, `TR=(9DCE,9F0E)`; `far` = index 0, `near` = index N.
```c
/* 1. walls + floor, all colour 5 */
if (DS_9E70 == 0) {
    if (N == 14) { A0E6(9EF0[14], 5); fillmode_begin(); }
    else { A0F2(5); for (y = 9EF0[7]; y <= DS_7E16; y++) A0E2(DS_7E12, DS_7E18, y); }
    poly({TL[N], TL[0], L[0], (0, L[0].y)})            -> A0F6(5,5)
    poly({TR[N], TR[0], R[0], (0x13F, R[0].y)})        -> A0F6(5,5)
} else {                                               /* extend near points: P' = 4·near − 3·far */
    L' = 4L[N]-3L[0]; R' = 4R[N]-3R[0]; TL' = 4TL[N]-3TL[0]; TR' = 4TR[N]-3TR[0];
    poly({TL', TL[0], L[0], L'}); poly({TR', TR[0], R[0], R'}); poly({L[0], R[0], R', L'});  /* each A0F6(5,5) */
    use L', R', TL', TR' as the "near" points below
}
/* poly(q) means: if (poly_clip(q,4,o,&n)) A0F6(o,n,…) */
line_clip(L[0], Lnear, 0);  line_clip(R[0], Rnear, 0);          /* floor edges black  */
line_clip(TL[0], TLnear, 0xF); line_clip(TR[0], TRnear, 0xF);    /* wall tops white    */
rot(B7EA,B162)->tfl; rot(B8D8,B162)->tfr; rot(B16E,ADA6)->tnl; rot(B872,ADA6)->tnr;   /* trough */

/* 2. per row band, far to near */
for (k = 0; k < N; k++) {
    w = F(N-k); i = N-k;
    if (w & 0x400) poly({L[k], R[k], R[k+1], L[k+1]}) -> A0F6(0xF,0xF);            /* white band */
    if ((w & 1) && 9EF0[N] < 9EF0[i]) {                                          /* joint lines */
        line_clip(L[k],R[k],0); line_clip(L[k],TL[k],0); line_clip(R[k],TR[k],0); }
    if (aq_wall_opening_quad(k, w, q, k > 1) && poly_clip(q,4,o,&n)) {
        if (N == 14) { if (poly_straddles_bottom(0x5A,o,n)) DS_A7D2 |= 1;
                       if (poly_straddles_bottom(0x13E,o,n)) DS_A7D2 |= 4; }
        A0F6(o,n,2,0);
    }
    if ((w & 0x20) && k > 0) aq_draw_row_shape(9A5C[i] + ((A2E2[i]-9A5C[i]) >> 2), 9EF0[i], i-1, 2);
    if ((w & 0x40) && k > 0) aq_draw_row_shape(A2E2[i] - ((A2E2[i]-9A5C[i]) >> 3), 9EF0[i], i-1, 2);
}
/* 3. centre trough, colour (2,0); interrupted where F has bit 2 */
a = first k in 0..N-1 with F(N-k)&2, else -1;  b = a+1; while (b < N && F(N-b)&2) b++;
if (b < 2) poly({tfl, tnl, tnr, tfr});
else {
    if (a > 0) { y = 9EF0[N-a]; pl = rot(lerp_x(B7EA,B162,B16E,ADA6,y), y); pr = rot(lerp_x(B8D8,B162,B872,ADA6,y), y);
                 poly({tfl, pl, pr, tfr}); }
    if (b < N-1) { y = 9EF0[N-b]; pl, pr as above; poly({pl, tnl, tnr, pr}); }
}                                                       /* each A0F6(2,0) */
/* 4. boxes, pillars, starter, opponent: rows far to near, raw (unrotated) arrays, i = row */
for (i = N; i > 0; i--) {
    w = F(i);
    if (w & 0x10) {                                     /* low box over the middle third */
        x0 = 9A5C[i] + (A5A8[i]-9A5C[i])/3;  x1 = A2E2[i] + (A5A8[i]-A2E2[i])/3;
        y0 = 9EF0[i];  yt = y0 + (A300[i]-y0)/7;
        n0 = 9A5C[i-1] + (A5A8[i-1]-9A5C[i-1])/3;  n1 = A2E2[i-1] + (A5A8[i-1]-A2E2[i-1])/3;
        ny = 9EF0[i-1];  nt = ny - (ny - A300[i-1])/7;
        aq_draw_box((x0,y0),(x1,y0),(x0,yt),(x1,yt),(n0,ny),(n1,ny),(n0,nt),(n1,nt), 0);
    }
    if (w & 0x80) {                                     /* two thin full-height pillars */
        d = (A2E2[i]-9A5C[i])/7;  e = (A2E2[i-1]-9A5C[i-1])/7;
        l0 = 9A5C[i]+d; l1 = l0 + (d>>3);  m0 = 9A5C[i-1]+e; m1 = m0 + (e>>3);
        aq_draw_box((l0,9EF0[i]),(l1,9EF0[i]),(l0,A300[i]),(l1,A300[i]),
                    (m0,9EF0[i-1]),(m1,9EF0[i-1]),(m0,A300[i-1]),(m1,A300[i-1]), 1);
        r0 = A2E2[i] - (d>>3) - d; r1 = r0 + (d>>3);  s0 = A2E2[i-1] - (e>>3) - e; s1 = s0 + (e>>3);
        aq_draw_box(same with r0,r1 / s0,s1, 1);
    }
    if (N == 14) { if (DS_A9C6 - DS_A55E == i) opponent_draw(); }
    else { if (DS_A55E - DS_9E6A - (N - i) == 1) starter_draw_mirror_aq(N - i);   /* drawn out of depth order */
           if (DS_A55E - DS_A9C6 == i) opponent_draw(); }
}
if (N == 14 && DS_A9C6 == DS_A55E) opponent_draw();
```

### 0dcb:9ce8 aq_draw_mirror
`far void(void)` — called by `road_frame` (mode 8) after it set `B194 = 7`, `9A44 = 0x6570 − 2·A55E`, `9A46 = 0x3726`.
```c
DS_96F6 = DS_9A44 + 0x10;  DS_96F8 = DS_9A46;        /* k=0 → C[seg-8] */
DS_A7A8 = (9A5C[7] + A2E2[7]) >> 1;  DS_A7CC = 9EF0[7];
for (i = 0; i < 8; i++) {                            /* reversed: k = 7-i */
    rot(9A5C[i],9EF0[i], &L[7-i]);  rot(A2E2[i],9EF0[i], &R[7-i]);
    rot(AB2E[i],A300[i], &TL[7-i]); rot(AB5E[i],A300[i], &TR[7-i]);
}
clip_set_mirror(); fillmode_end(); 0dcb:57a6(); fillmode_begin(); aq_draw_scene(); 0dcb:5a2c();
```
(uses the sin/cos of this frame's front roll; 57a6 = mirror background, 5a2c = mirror frame — see §3.)

### 0dcb:9d9c aq_climb_left
`near void(void)` — car (left edge `9AC8`) on the left wall: sets roll and next frame's channel deformation.
```c
DS_A7A8 = 9A5C[14];                                  /* pivot = far floor-left corner */
int e = (DS_9AC8 < 0x514) ? 0xB4 : 0x5C8 - DS_9AC8;
DS_9E70 = e * 0x6C / 400;
DS_A2B8 = e >> 2;  DS_985E = e - (e >> 3);  DS_A7A6 = DS_A2B8;  DS_9B14 = DS_A2B8;
if (!DS_9730 && !DS_1F82) DS_A564 += 0xF;            /* slide back (writes player x!) */
if (DS_9AC8 < 0x514) {                               /* over the wall top */
    if (!DS_9730 && !DS_1F82) DS_A564 += 0x19;
    DS_A5FA = 0x514 - DS_9AC8;  DS_9A5A = min(DS_A5FA, 200);
    DS_A2B8 += DS_9A5A;  DS_985E += DS_9A5A;  DS_A5FA = min(DS_A5FA, 200);
}
```

### 0dcb:9e4e aq_climb_right
`near void(void)` — right wall (car right edge `9A58` > 0xCF7).
```c
DS_A7A8 = A2E2[14];
int e = (DS_9A58 > 0xD80) ? 0x88 : DS_9A58 - 0xCF8;
DS_9E70 = e * -0x5A / 400;
DS_A7A6 = -3*e;  DS_985E = e - (e >> 2);  DS_A5FA = (e >> 2) - e;
DS_A2B8 += -2*e;  DS_9A5A += -2*e;                   /* both 0 on entry (a094) */
if (!DS_9730 && !DS_1F82) DS_A564 -= 0xF;
DS_9B14 = DS_985E;
if (DS_9A58 > 0xD80) {
    if (!DS_9730 && !DS_1F82) DS_A564 -= 0x19;
    if (DS_7FCC) DS_A56A += 1;
    int o = DS_9A58 - 0xD80;
    DS_9B14 = DS_985E - o;  DS_A7A6 -= o;  DS_A5FA += o;  DS_985E += o;
}
```
(asymmetric to 9d9c by design; keep.)

### 0dcb:9f08 aq_trough_bump
`near void(void)` — if the current segment has a trough (`!(F(0) & 2)`, byte `[96F6−0x1C]`):
```c
DS_9758 = (DS_A2B6 < DS_B16E) ? -1 : (DS_A2B6 > DS_B872) ? 1 : 0;   /* car left edge vs trough  */
DS_9868 = (DS_A55A < DS_B16E) ? -1 : (DS_A55A > DS_B872) ? 1 : 0;   /* car right edge vs trough */
if (DS_9758 == 0 || DS_A7E8 + DS_9758 == 0) DS_9E70 = -4;           /* in it, or crossed it */
if (DS_9868 == 0 || DS_AAF2 + DS_9868 == 0) DS_9E70 = 4;
if (DS_9E70 != 0 && DS_A100 > 10) DS_A100 -= 2;                     /* speed penalty (race global) */
```

### 0dcb:9f9a aq_roll_update
`near void(void)`:
```c
DS_9E70 += DS_B8BC;  wrap to -0x167..0x167 (±0x168);     /* B8BC = spin from 0dcb:a1c0 */
DS_B190 = abs(DS_9E70);
DS_AD68 = B190 < 8 ? 3 : B190 < 0x10 ? 2 : B190 < 0x1F ? 1 : 0;  if (DS_9E70 < 0) DS_AD68 = 6 - DS_AD68;
int d = DS_9E70 - DS_B868;
DS_B7B0 = d >= 8 ? 3 : d >= 6 ? 2 : d >= 4 ? 1 : d < -7 ? -3 : d < -5 ? -2 : d < -3 ? -1 : 0;
if (DS_9E70 < 0) DS_B7B0 = -DS_B7B0;
if (abs(DS_B7B0 - DS_9E68) > 1) DS_B7B0 = (DS_9E68 + DS_B7B0) >> 1;
DS_B868 = DS_9E70;  DS_9E68 = DS_B7B0;  DS_A7E8 = DS_9758;  DS_AAF2 = DS_9868;
```

### 0dcb:a094 aq_draw_front
`far void(void)` — mode-8 replacement of the front drawer 0dcb:6a32 (`road_frame` sets `B194 = 14`,
`9A44 = 0x4E62 + 2·A55E`, `9A46 = 0x3726` first).
```c
DS_96F6 = DS_9A44 + 0x1C;  DS_96F8 = DS_9A46;                     /* k=0 → C[seg+14] */
DS_A7A8 = (9A5C[14] + A2E2[14]) >> 1;  DS_A7CC = 9EF0[14];
DS_AA9C = DS_985E; DS_B8BE = DS_A2B8; DS_BB8A = DS_A7A6; DS_B8E6 = DS_A5FA;   /* save for mirror/opponent */
DS_985E = DS_A2B8 = DS_A7A6 = DS_A5FA = DS_9A5A = DS_9B14 = 0;
if (DS_9AC8 < 0x5C9) aq_climb_left();
else if (DS_9A58 > 0xCF7) aq_climb_right();
else DS_9E70 = 0;
aq_trough_bump();  aq_roll_update();
DS_A540 = isin(DS_9E70);  DS_BA90 = isin(DS_9E70 + 0x5A);
for (i = 0; i < 15; i++) { rotate 9A5C/A2E2 (with 9EF0) and AB2E/AB5E (with A300) into L/R/TL/TR[14-i]; }
clip_set_view();  aq_draw_sky();  fillmode_begin();  aq_draw_scene();
```

### 0dcb:a1c0 aq_wall_bounce
New. `far void aq_wall_bounce(int dir)` (mode 8, from road_step when `DS_B8C8`):
```c
DS_7FCE = 1;
int d = ((DS_A9C6 - DS_A55E) << 3) + (DS_A9C8 - DS_A560)/8;      /* /8 truncating toward 0 */
int k = (d >= 0x10) ? ((d + 8) >> 4) + 2 : -1;
int sp = DS_A100;
if (sp > 0x1E) {
    int s6 = sp / 6, h6 = DS_A56A / 6;
    int db = dir > 0 ? 0x20 : -0x22, dxc = dir > 0 ? 0x28 : -0x28;
    for (int i = 0; i < 8; i++) {
        if (k > 1) DS_A9C6 += 2;                                   /* opponent pulls away */
        DS_B8BC += db;  DS_A56A -= h6;  DS_A564 += dxc;
        DS_9A56 = (i < 4 ? i : 8 - i) * s6;                        /* view bounce for road_proj_y */
        0001_d908(); road_frame();
    }
    DS_B8BC = 0;                                                   /* 9A56 keeps its last value */
} else { DS_B8BC -= dir > 0 ? 6 : -6; DS_9A56 = 10; }
0001_d908(); road_frame();
DS_B8BC = 0;  DS_A100 = sp;
```

### 0dcb:a2d8 aq_collide_probe
`far uint(uint flags)` — called by the race code (0001:cee0) with a segment-flags word; projects the two
nearest rows and tests floor obstacles against the bottom screen row. **Overwrites rows 0–1 of the row
arrays** (rebuilt by the next `road_frame`).
```c
int mid = (DS_BB8A + DS_B8BE + 0x12C0) >> 1, z = 12 - DS_A560/8;
for (i = 0; i < 2; i++, z += 8) {   /* same formulas as 18ec rows, but with the saved values: */
    9EF0[i] = road_proj_y(z, DS_AA9C+2000, DS_986A);  9A5C[i] = road_proj_x(z, DS_B8BE+0x5C8, DS_BA8E);
    A2E2[i] = road_proj_x(z, DS_BB8A+0xCF8, DS_BA8E); A5A8[i] = road_proj_x(z, DS_BB8A+mid, DS_BA8E);
    A300[i] = road_proj_y(z, DS_B8E6+0x67C, DS_986A); AB2E[i] = road_proj_x(z, DS_AD6A+0x438, DS_BA8E);
    AB5E[i] = road_proj_x(z, DS_B166+0xE88, DS_BA8E); }
uint r = 0;  /* polygons are clipped with poly_clip(…,4,o,&n); r from poly_bottom_hit_bits(o,n) */
if (flags & 0x10) {     /* box footprint, point order as in the exe (bow-tie) */
    q = { (9A5C[1]+(A5A8[1]-9A5C[1])/3, 9EF0[1]+(A300[1]-9EF0[1])/7), (A2E2[1]+(A5A8[1]-A2E2[1])/3, same y),
          (9A5C[0]+(A5A8[0]-9A5C[0])/3, 9EF0[0]+(A300[0]-9EF0[0])/7), (A2E2[0]+(A5A8[0]-A2E2[0])/3, same y) };
    r = hit(q);
} else if (flags & 0x80) {   /* pillar footprints, rows 1 then 0 */
    d1 = (A2E2[1]-9A5C[1])/7; d0 = (A2E2[0]-9A5C[0])/7;
    q = { (9A5C[1]+d1, 9EF0[1]), (9A5C[1]+d1+(d1>>3), 9EF0[1]), (9A5C[0]+d0+(d0>>3), 9EF0[0]), (9A5C[0]+d0, 9EF0[0]) };
    r = hit(q);
    q = { (A2E2[1]-d1-(d1>>3), 9EF0[1]), (A2E2[1]-d1, 9EF0[1]), (A2E2[0]-d0, 9EF0[0]), (A2E2[0]-d0-(d0>>3), 9EF0[0]) };
    r |= hit(q);
}
return r;               /* 0, 0x20, 0x40, 0x60 */
```
No rotation is applied here.

### 0dcb:a5aa start_props_load

`near void start_props_load(void)` — new (replaces SR1 start light 1013). The Ghidra/cg string "O.K. !" at
DS:07E6 is a false hit: 0x7E6 is the picture id 2022.
```c
DS_A2B4 = 0;  DS_A7AC = 1;                       /* animation counter / active flag (drawn by 0dcb:a746) */
DS_9A40 = pic_get(DS_B7B2, 1);                   /* B7B2: 2067 (ADD6==8), 2069 (ADD6&6), else 2070 */
if (B7B2 == 2067 || B7B2 == 2070) { r = {48, 82, 0, 0, 184, 102}; A0AE(DS_9A40, page ADA2, &r, 1); heap_pop_lo(1); }
if (B7B2 == 2067) { DS_9A2A[0..3] = pic_get(2078..2081, -1); DS_AB24 = pic_get(2068, -1); }
else if (B7B2 == 2070) { DS_9A2A[0..3] = pic_get(2000, 2022, 2057, 2058, -1); DS_AB24 = pic_get(2071, -1); }
else if (B7B2 == 2069) { r = {128, 56, 0, 6}; DS_AB24 = bitmap_extract(DS_9A40, &r, -1); }   /* 1a21:af96 */
```
`DS:B7B2` is set in 0001:8d9e (race setup). 2067/2070 = 48×82 start panel (pre-blitted to page ADA2 at
(184,102)); 2068/2071 = 144×82m; 2078–2081 / 2000,2022,2057,2058 = 16×21m, 8×17m, 8×11m, 8×5m = 4 distance
sizes of a road-side object (`DS:9A2A[4]` far ptrs, drawn by the 0dcb road code); 2069 = 176×87m. `DS:AB24`
also serves as the fixed scratch buffer of the rotator when `DS:80A0 != 0` (see bb22).

### 0dcb:a746 road_draw_starter_slide
New (called by road_frame when `DS:B7AE` (opponent event) && `(ADD6 & 6) == 0`). Animates a 48-px wide figure
(picture `DS:AB24`, LIB3 #2068 0x814 or #2071 0x817, loaded by 0dcb:a5aa for event pictures 0x813/0x816) sliding
out to the left of the view while the car moves; `DS:A2B4` += 4 per moving frame (road_step).
```c
if (!DS_A7AC) return;
int d = (DS_A2B4 & ~3) * 2;
if (d > 0x58) { DS_A7AC = 0; return; }
Rect r = { 0x30 - d, 0x52, d + 0xB8, 0x66, 8, 0x12 };            /* {w,h,srcx,srcy,dstx,dsty} */
if (r.w > 0) {
    blit([A0AE], DS_ADA2 /* page 2: saved background at (0xB8,0x66) */, back, &r, 1);
    r.dstx += r.w;  r.w = 0x30;  r.srcx = 0;
} else { r.w += 0x30;  r.srcx = d - 0x30; }
r.srcy = 0;
if (DS_AD0A) r.srcx += 0x30;                        /* frame 2 */
if (DS_A7A2) { DS_AD0A = 1; r.srcx += 0x30; }       /* race state changed: latch next frame */
if (r.w > 0) masked_blit([A0B6], DS_AB24, back, &r, 1);
DS_9E6A = (DS_A55E < DS_A9C6) ? DS_A55E + 1 : DS_A9C6;
```

### 0dcb:a85c starter_draw_mirror
`near void(int x, int y, int row)` — the start-line figure (pictures `DS:9A2A[4]`, LOD by distance, loaded
by 0dcb:a5aa) in the mirror of the normal modes (caller 0dcb:5aee). Only while the start slide is over
(`DS:A7AC == 0`) and `row != 6`.
```c
lod = row < 3 ? row >> 1 : row - 1;  if (lod > 3) lod = 3;
Pic far *p = DS_9A2A[lod];
Rect r = { p->w, p->h, 0, 0, x - (bitmap_visible_width(p) >> 1), y - p->h - 3 };
if (y > 0) r.dsty -= y - 0x2A;          /* i.e. dsty = 0x27 - h for any y > 0 (as coded) */
blit_clip_xy(&r); fillmode_end(); bitmap_blit_masked_shift(p, BACK, &r, 0); fillmode_begin();
```

### 0dcb:a92a starter_draw_mirror_aq
`near void(int row)` — mode-8 version (caller aq_draw_scene, mirror). If `A7AC == 0 && row != 6`:
```c
int sw = 0; if (DS_7E12 == 0) { sw = 1; clip_set_mirror(); }
lod as in a85c; Pic far *p = DS_9A2A[lod];
int y = 9EF0[row], x = lerp_x(DS_B7EA, DS_B162, DS_B16E, DS_ADA6, y) + 1;   /* on the trough's left line */
int hx = bitmap_visible_width(p) >> 1, oy = 0;
if (DS_B190 >= 6) { DS_80A0 = 1; i8 ox8, oy8; p = A0EE(p, DS_9E70, -1, &ox8, &oy8, hx); DS_80A0 = 0;
                    hx = ox8; oy = oy8; }
if (DS_9E70 != 0) rot(x, y, &x, &y);
Rect r = { p->w, p->h, 0, 0, x - hx, y - p->h + oy };
blit_clip_xy(&r); fillmode_end(); bitmap_blit_masked_shift(p, BACK, &r, 0); fillmode_begin();
if (sw) clip_set_view();
```

### 0dcb:aab0 road_draw_start_tree
New (front view, road modes 2/4, at the row whose segment is `A7A4 - 4`, centred between the road edges).
`near void road_draw_start_tree(int x, int ybase, int row)` — composes the drag-race start light ("tree", picture
`DS:9A40` = event picture 0x815; clean copy `DS:AB24` = bitmap_extract(9A40, {0x80,0x38 at 0,6})) and blits it.
```c
fillmode_end();
Rect r;                                         /* by row: */
if (row == 0 || row == 1) r = {0x38, 0x57, 0x48, ...};          /* {w,h,srcx} */
else if (row == 2 || row == 3) r = {0x28, 0x45, 0x20, ...};
else if (row == 4) r = {0x20, 0x3A, 0, ...};                     /* row < 0 or > 4: r uninitialised */
if (DS_A7A2) {                                   /* lamp state changed this frame */
    if (DS_A7A2 == 0x88 || DS_A7A2 == 0x92) DS_96FE = DS_96FC = -1;
    else blit([A0AE], DS_AB24, DS_9A40, {0x80, 0x38, 0, 0, 0, 6}, 1);  /* restore clean tree */
    if (A7A2 & 1) DS_96FE = 0; else if (A7A2 & 2) DS_96FE = 1; else if (A7A2 & 4) DS_96FE = 2;
    if (A7A2 & 8) DS_96FC = 0; else if (A7A2 & 0x10) DS_96FC = 1; else if (A7A2 & 0x20) { DS_AD0A = 1; DS_96FC = 2; }
    for (i = 0; i < 3 && DS_96FE >= 0; i++) {    /* left lamp column, 3 sizes */
        k = DS_96FE*3 + i;
        masked_blit([A0B6], DS_9A40, DS_9A40, { W[i], H[k], 0x80, SY[k], DX[i], DY[k] }, 1);
    }
    for (i = 0; i < 3 && DS_96FC >= 0; i++) {    /* right lamp column */
        k = DS_96FC*3 + i;
        masked_blit([A0B6], DS_9A40, DS_9A40, { W[i], H[k], W[i] + 0x80, SY[k], W[i] + DX[i], DY[k] }, 1);
    }
}
DS_A7A2 = 0;
int t = ybase - r.h + 1;
if (t < DS_7E14) { r.dsty = DS_7E14; r.srcy = DS_7E14 - t; r.h -= r.srcy; } else { r.dsty = t; r.srcy = 0; }
int o = r.h - DS_7E16 + r.dsty;
if (o > 0) { if (o > 0x14) r.dsty -= 0x14; r.h = DS_7E16 - r.dsty; }
r.dstx = x - (r.w >> 1);
masked_blit([A0B6], DS_9A40, back, &r, 1);
fillmode_begin();
```
Tables (s16): `DX = DS:7FD0 [0,32,72]`, `W = DS:7FD6 [16,24,24]`, `H = DS:7FDC [7,8,11, 4,5,7, 7,8,11]`,
`SY = DS:7FEE [0,9,18, 66,72,78, 33,42,51]`, `DY = DS:8000 [6,7,9, 22,25,31, 34,41,51]` (index lamp*3 + size).
`DS:A7A2` (race start state bits) is written by race code (race_logic / 0001:8d9e).

### 0dcb:ad40 opp_sprite_aq
`far Pic far *(int s, int v)` — mode-8 opponent sprite for sprite index `s` (0..12, as SR1 §4.13) and bank
variant `v` (0..6, 3 = level). `c = (i8) DS:BAAC[10·DS_9B0A]` = SR2_DATA block 2 byte 0 of the opponent's
car (`9B0A` = opponent car index) = sprite-set number 0..5.
```c
if (DS_BA92) arena_release_top(1);   /* 1a21:9b56(1): free the shrunk copy made last call */
DS_BA92 = 0;
switch (s) { case 0: return DS_9E04;       /* far ptrs, loaded by 0dcb:8840 */
             case 1: return DS_9E64;
             case 2: return DS_9E72[v];    /* 7 far ptrs */
             case 5: return DS_A1C6[v];
             case 9: return DS_A320[v]; }
DS_BA92 = 1;
if (s == 3 || s == 4)      return A0EA(DS_9E72[v], ((u8*)AA9E[c].ptr6)[3*v + s - 3]);
if (s >= 6 && s <= 8)      return A0EA(DS_A1C6[v], ((u8*)AA9E[c].ptr8)[3*v + s - 6]);
/* else (10..12) */        return A0EA(DS_A320[v], ((u8*)AA9E[c].ptrC)[3*v + s - 10]);
```
`AA9E[c]` = SR2_DATA block 8 entry (14 bytes) `{u16 id0,id1,id2, u16 ptr6, u16 ptr8, u16 id3, u16 ptrC}`;
the three near pointers point into block 7 (`DS:8E3C`, 22-byte tables = 7 variants × 3 shrink masks + 1 pad);
the mask byte is the SR1 pic_alloc_shrunk row/column mask (§4.16). s = 3,4 use masks 0,1 of the rear table;
6..8 the far-rear table; 10..12 the front table. (s < 0 or > 12 would index garbage; see Open questions.)

### 0dcb:ae9c opp_bank_variant
`near int(void)` — opponent bank angle `DS:A55C` (degrees) and sprite variant.
```c
int e;
if      (DS_A9CC < DS_B8BE + 0x5FA) e = DS_B8BE + 0x5FA - DS_A9CC;     /* up the left wall  */
else if (DS_A9CC > DS_BB8A + 0xCC6) e = DS_A9CC - DS_BB8A - 0xCC6;     /* up the right wall */
else e = 0;
DS_A55C = e >= 100 ? 0x2D : e < 1 ? 0 : e * 0x2D / 100;
if (DS_A9CC - ((DS_BB8A + DS_B8BE + 0x12C0) >> 1) < 0) DS_A55C = -DS_A55C;
DS_A55C += DS_9E70;
if (DS_9E70 < 0 && DS_9E70 > -0x5A) DS_A55C += DS_9E70 >> 2;
wrap DS_A55C to ±0x167 (±0x168);
int a = abs(DS_A55C);
int v = a >= 0x38 ? -1 : a >= 0x25 ? 0 : a >= 0x17 ? 1 : a >= 0xD ? 2 : 3;
if (DS_A55C > 0 && v != -1) v = 6 - v;
return v;               /* -1 = needs a rotated bitmap */
```

### 0dcb:af98 opponent_draw
`far void(void)` — SR2 version of SR1 `opponent_draw` (2645:3f8d, ../StreetRod/port/spec/race_render.md
§4.13). Callers: 0dcb:6a32 (front, modes ≠8), 0dcb:5aee (mirror, modes ≠8), aq_draw_scene. Distances now
in 1/64 segment (`A560` 0..63); thresholds are SR1 ×8. Full pseudocode (ground truth: disassembly):
```c
if (DS_ADD6 == 2) return;
int oseg = DS_A9C6;
int d  = ((oseg - DS_A55E) << 6) - DS_A560 + DS_A9C8;
int ox = DS_A9CC - DS_A33E;
int wy = DS_AA9C + 2000;                         /* world y of the car (SR1 0x7D0) */
int v = 0;                                       /* bank variant (mode 8) */
if (DS_ADD6 == 8) {
    v = opp_bank_variant();
    if (DS_A9CC < DS_B8BE + 0x5C8)               /* on the left wall: raise */
        wy -= (long)(DS_B8BE - DS_A9CC + 0x5A0) * (DS_B8E6 - DS_AA9C + 0x154) / (400 - DS_AD6A + DS_B8BE);
    else if (DS_A9CC > DS_BB8A + 0xCF8)
        wy -= (long)(DS_B8E6 - DS_AA9C + 0x154) * (DS_A9CC - DS_BB8A - 0xCF8) / (400 - DS_BB8A + DS_B166);
}   /* (exe tests ADD6==0x10 for 200 vs 400 inside this branch: dead, always 400); 32-bit _aFldiv */
int p = DS_A7D4, s;
if (d >= 0x3C0) s = -1;
else if (d >= 0x80) { s = ((d + 0x40) >> 7) + 2; if (s > 8) s = 8; }
else if (d < -0x38) s = ((0x80 - d) >> 7) + 8;
else if (d <= 0x40) {
    if (abs(ox) < 0xB4) s = (p <= 8) ? 2 : 9;
    else s = (d < 0) ? 9 : (ox < 0) ? 0 : 2;     /* SR2: d<0 → 9 (new) */
} else s = (ox < -0xB4) ? 1 : 3;
int nodraw = 0;
DS_9A48 = (d >= 0 && d <= 0x40 && DS_9A80 < 9 && p < 9
           && abs(ox) < (DS_B7B4 < 4 ? 100 : 0xB4) && DS_A562 > DS_A9CA);      /* rear-end contact */
if (DS_7FCE == 0 && DS_B7B4 >= 4) {               /* hysteresis (SR1: 8ACC >= 4) */
    if (DS_A1BC == 3 || DS_9A48) s = 2;
    else if ((DS_ADD6 & 0x10) && abs(d - DS_B8C6) < 0x28 && p == 9 && s >= 9) { s = 9; d = DS_B8C6; }
    else if (s != p) {
        if (s < 0)            { if (p == 8)  return; s = p + 1; }
        else if (s > 12)      { if (p == 12) return; s = p + 1; }
        else if (s == DS_9A80) ;
        else if ((s <= 8 && p > 9) || (s >= 9 && p > 0 && p <= 8 && ox < 0)) s = p - 1;
        else if (s >= 9 && p > 0 && p <= 8)  nodraw = 1;          /* ox >= 0; s kept (exe re-tests 9A48: dead) */
        else if (p == 0) { if (s < 9) s = 1; else { s = 9; nodraw = 1; } }
        else if (p == 9) { if (s > 8) s = 10; else { s = (ox < 0) ? 0 : 2; nodraw = 1; } }
        else if (p == 1) s = (s >= 1) ? 3 : 0;
        else if (p == 3 && s == 1) ;
        else s = (s > p) ? p + 1 : p - 1;
    }
}
Pic far *pic;  i8 adx = 0, ady = 0;               /* bp-0x2a / bp-0x32 */
if (DS_ADD6 == 8) {
    if (s == 0 && v != 3) nodraw = 1;
    if (s == 1 && v != 3) s = 2;
    if (v == -1) { pic = opp_sprite_aq(s, 3); DS_80A0 = 1;
                   pic = A0EE(pic, DS_A55C, -1, &adx, &ady, (i8)DS:9E08[7*s + 3]); DS_80A0 = 0;
                   DS_B198 = pic; }
    else if (s != p || v != DS_9862) DS_B198 = opp_sprite_aq(s, v);   /* else keep last DS_B198 */
} else DS_B198 = DS_B7B6[s];                      /* 13 far ptrs (SR1 8ACE); s=-1 reads DS:B7B2 */
DS_B8C0 = (s == 0 || (s == 2 && d <= 0x50));      /* alongside (SR1 8BC2) */
DS_9A48 = (DS_9A48 && s == 2);
if (v != -1) { adx = DS:9E08[7*s + v]; ady = DS:9E94[7*s + v]; }   /* i8 tables built by 0dcb:86d2 */
DS_9A80 = p;  DS_A7D4 = s;  DS_9862 = v;  DS_B8C6 = d;
if (nodraw && !DS_9A48 && DS_A1BC != 3) return;
int z = d / 8;                                    /* C truncation */
if (s == 0) z += 0x10;
if (z < -2 && s == 2) z += 8;
int dy, dx;
if (DS_ADD6 == 8) { dy = DS_986A; dx = DS_BA8E; }
else { dy = (DS_ADD6 == 4) ? 0 : DS_A56C - HGT[oseg]; dx = DS_A56A - YAW[oseg]; }
          /* HGT = far word array [DS:8716]:46A6, YAW = [DS:8718]:372E (track module) */
int sx, sy;
if (s < 9) { z = abs(z) + 4; sx = road_proj_x(z, DS_A9CC, dx); sy = road_proj_y(z, wy, dy);
             if (s == 2 && sx > 0x13F && DS_AAF8 < 200) sx = (sx + DS_AAF8) / 2; }
else       { z = -z; sx = road_proj_x_mirror(z, DS_A9CC, 10 - dx); sy = road_proj_y_mirror(z, wy, -dy); }
int row = (z + 1) >> 3;
if ((s == 2 || s == 3) && sy > 100) sy = 100;
Rect r;                                           /* r.w/r.h loaded only below */
if (DS_ADD6 == 8) {
    if (DS_9A48 || DS_A1BC == 3) { sy = 0x68; if (sx + r.w < 0) sx = 0x1E - r.w; }  /* r.w STALE (uninit) */
    rot(sx, sy, &sx, &sy);  sx -= adx;  sy += ady;
}
DS_AAF8 = sx;
r.w = pic->w;  r.h = pic->h;  r.srcx = r.srcy = 0;
if (s == 0) { r.srcx = max(0, r.w - sx + 0x40); r.dstx = 0; r.dsty = sy; }
else if (s == 1) { r.dstx = sx - r.w; r.dsty = sy - r.h + 0x11;
    if (DS_A1BC == 3 || DS_9A48) { r.dsty = 0x66 - r.h; if (r.dstx + r.w < 0) r.dstx = 0x1E - r.w; } }
else {
    if (DS_ADD6 == 8) { r.dstx = sx + DS_AD20; r.dsty = sy - r.h; }  /* AD20 = video pixel shift (3 on EGA) */
    else {
        int base;
        if (s > 3 && s < 9) { r.dstx = sx - (r.w >> 1);
                              base = (row > 14) ? max(sy, DS_975E) : max(sy, 9EF0[row]); }
        else                { r.dstx = sx - (bitmap_visible_width(pic) >> 1); base = sy; }
        r.dsty = base - r.h;
        if ((s == 2 || s == 3) && r.h + r.dsty >= 0x65) r.dsty = 0x65 - r.h;
    }
    if (DS_A1BC == 3 || DS_9A48) {
        r.dsty = 0x66 - r.h;
        if (r.dstx + r.w < 0)       { r.dstx = 0x14 - r.w; r.dsty = 0x6B - r.h; }
        else if (r.dstx >= 0x13F)   { r.dstx = 0x12B;      r.dsty = 0x69 - r.h; }
    }
}
int sw = 0;
if (s >= 9) { if (DS_7E12 == 0) { sw = 1; clip_set_mirror(); } }
else        { if (DS_7E12 != 0) { sw = 1; clip_set_view(); } }
if (r.dstx + r.w < DS_7E12 || r.dstx >= DS_7E18) return;     /* clip not restored (as SR1) */
if (s >= 9) blit_clip_xy(&r); else blit_clip_y(&r);
fillmode_end();
if (s >= 9) bitmap_blit_masked_shift(pic, BACK, &r, 0); else A0B6(pic, BACK, &r, 1);
fillmode_begin();
if (sw) { if (DS_7E12 == 0) clip_set_mirror(); else clip_set_view(); }
```
Differences vs SR1 in short: 1/64 distances; `d<0` alongside-but-far-sideways → 9; contact flag
computed before hysteresis and forces s=2 (also when `A1BC==3`); Mulholland (mode 0x10) front-view freeze;
`nodraw` cases instead of drawing; mode-8 bank variants / rotated sprites / wall height; mirror dx is
`10 - dx`; ground clamp uses the row y table `9EF0`/`975E` instead of `6E04`; s=2 x smoothing with `AAF8`;
contact-pose placement at the screen bottom; mirror blit is `bitmap_blit_masked_shift(…,0)`; SR1 `5EC6`
is gone. SR1→SR2 DS remap: 7E8A→A9C6, 7E8C→A9C8, 7E90→A9CC, 7D36→A33E (ox) / A564 (camera, in proj),
7D30→A55E, 7D32→A560, 7D7C→A7D4, 6E2A→9A80, 8ACC→B7B4, 8BC2→B8C0, 72F2→9A48, 7E8E→A9CA, 7D34→A562,
8ACE→B7B6, 7D3E→A56C, 7D3C→A56A, 6E04→9EF0[]/975E, 5E4E→7E12, 5E54→7E18, 389b:56F0→[8716]:46A6,
389b:4570→[8718]:372E.

### 0dcb:b7da popcount8

= SR1 popcount8 (2645:528c), ../StreetRod/port/spec/race_render.md §4.16; no DS references.

### 0dcb:b7fa pic_alloc_shrunk

Changed (SR1 2645:52b7, §4.16): signature `far Pic far *pic_alloc_shrunk(int w, int h, int masked, u8 ymask,
u8 xmask)` (takes w/h/mask flag instead of the source picture). Same size rules (nw from xmask, nh from ymask,
partial last byte counted bit by bit, `nw = (nw + ~DS_AD24) & DS_AD24`); allocation
`arena_bitmap_alloc(nw, nh, masked, 2)` (**high stack**, SR1 mode 1), then `pic_clear` 1a21:b09e (data = 0,
mask = 0xFF, mask size = data bytes / DS:AD1E). DS remap: SR1 DS:823E → DS:AD24, DS:8238 → DS:AD1E.

### 0dcb:b8c8 pic_shrink_planar

= SR1 pic_shrink_planar (2645:5407), §4.16, driver slot 15 (EGA/VGA table DS:7B38), with these changes:
signature `far Pic far *(Pic far *src, u8 mask)`, the same mask is used for x and y; dest =
`pic_alloc_shrunk(src->w, src->h, src->mask != 0, mask, mask)`; the plane loop is done by the asm helper
`3488:000e(src->data, src->bpr, src->h, d->data, mask, mask, popcount8(mask))` (video module; same bit
compaction as SR1); then `make_mask_plane` 33e5:00b9 (SR1 2634:00c1) if src has a mask.

### 0dcb:b984 cga_expand_bits

= SR1 cga_expand_bits (2645:55f8), §4.16 note: `*hi` gets `3 << 6,4,2,0` for mask bits 0x80..0x10, `*lo` for
0x08..0x01.

### 0dcb:b9e0 pic_shrink_cga

= SR1 pic_shrink_cga (2645:5670) (parked), driver slot 15 of the CGA/Hercules table DS:7A40; one mask for x/y;
dest always masked (`pic_alloc_shrunk(w, h, 1, m, m)`); core `3488:00bf(src data, src mask, bpr, h, dst data,
dst mask, m, hi, lo)` with `cga_expand_bits(m, &hi, &lo)`.

### 0dcb:ba72 pic_shrink_tandy

= SR1 pic_shrink_tandy (2645:5885) (parked), driver slot 15 of the Tandy table DS:7ADC; one mask; core
`3488:01f3(src data, bpr, h, dst data, m, m)`, then Tandy mask build `3210:023c(dst data, dst mask, bytes)` if
masked. (Answer to "no known callers": b8c8/b9e0/ba72 are reached only through `DS:A0EA`.)

### 0dcb:bb22 rot_alloc

`far Pic far *rot_alloc(int angle, Pic far *src, int mode, int *u0, int *v0, s8 *ax, s8 *ay, int pivot_x,
int setup)` — new. Allocates the destination of a rotation and computes the inverse-mapping start.
Rotation helper `rot(x,y)` = 0dcb:0379: `x-=A7A8, y-=A7CC; x' = (x*cos>>8)+(y*sin>>8)+A7A8; y' = (y*cos>>8)+(-x*sin>>8)+A7CC`
with `sin = DS:A540`, `cos = DS:BA90` (8.8).
```c
if (setup) { save A540,BA90,A7A8,A7CC to DS:9708,970E,970A,970C;
             A540 = isin(angle); BA90 = isin(angle + 90); A7A8 = A7CC = 0; }
(x0,y0)=rot(0,h-1); (x1,y1)=rot(w-1,h-1); (x2,y2)=rot(w-1,0);           /* (0,0) maps to itself */
W = max(|x2-x0|, |x1|);  nw = (W + 1 + ~DS_AD24) & DS_AD24;
nh = max(|y2-y0|, |y1|) + 1;
if (heap_free() <= bitmap_bytes(nw, nh, 1)) mode = -1;                  /* 1a21:9b96, fall back to SHP arena */
d = DS_80A0 ? bitmap_alloc(nw, nh, 1, -1, DS_AB24:AB26)                 /* header+data into the scratch buffer */
            : arena_bitmap_alloc(nw, nh, 1, mode);
pic_clear(d);
minx = min(0, x0, x1, x2);  miny = min(0, y0, y1, y2);
if (setup) { (px,py) = rot(pivot_x, h-1);
             *ax = (s8)(px - minx);  *ay = (s8)(d->h - (py - miny));      /* bottom anchor in the dest */
             *u0 = BA90*minx - A540*miny;  *v0 = A540*minx + BA90*miny; }   /* 8.8 source coords of dest (0,0) */
return d;
```

### 0dcb:bd12 pic_rotate_planar

`far Pic far *pic_rotate_planar(Pic far *src, int angle, int mode, s8 *ax, s8 *ay, int pivot_x)` — new, driver
slot 16 (`DS:A0EE`, EGA/VGA). Nearest-neighbour rotation.
```c
d = rot_alloc(angle, src, mode, &u0, &v0, ax, ay, pivot_x, 1);
rowoff[y] = y * src->bpr  (y < src->h; local table of 60 entries → src->h <= 60);
splane = src->data_bytes/4; dplane = d->data_bytes/4;
for (row = 0; row < d->h; row++) {
    u = u0; v = v0; bool hit = false, stop = false;
    for (b = d->bpr; b; b--) {
        if (stop) { advance all 4 dest plane pointers by b; break; }
        p[0..3] = 0;
        for (j = 0; j < 8; j++) {
            sx = (s8)(u >> 8); sy = (s8)(v >> 8);
            if (sx < 0 || sx >= src->w || sy < 0 || sy >= src->h) { if (hit) { stop = 1; break; } }
            else { hit = 1; bit = 0x80 >> (sx & 7); o = rowoff[sy] + (sx >> 3);
                   for plane k: p[k] |= shift(src_plane_k[o] & bit, (sx & 7) - j); }  /* move bit to position j */
            u += BA90; v += A540;
        }
        dest_plane_k[pos++] = p[k] for k = 0..3;
    }
    u0 -= A540; v0 += BA90;
}
make_mask_plane(d);  restore A540,BA90,A7A8,A7CC from DS:9708..970E;  return d;
```
`shift(x, s)` = `x << s` if s > 0, `x >> -s` if s < 0. Out-of-range pixels stay colour 0 (transparent).

### 0dcb:c09a pic_rotate_cga

Same as bd12 for CGA/Hercules (slot 16 of DS:7A40): 2 bits/pixel, one data plane, 4 px per byte: source byte
`rowoff[sy] + (sx >> 2)`, pair mask `0xC0 >> 2*(sx & 3)`, moved by `2*(j − (sx & 3))` bits; inner loop j < 4.
The dest mask byte starts 0xFF and the pair is cleared when the source mask pair is 0 (opaque); no
make_mask_plane. Restores DS:A540/BA90/A7A8/A7CC.

### 0dcb:c342 pic_rotate_tandy

Same for Tandy (slot 16 of DS:7ADC): 4 bits/pixel, 2 px per byte: nibble mask `(u & 0x100) ? 0x0F : 0xF0`
(sx odd/even), source byte `rowoff[sy] + (sx >> 1)`; j = 0 with odd sx → nibble >> 4, j = 1 with even sx →
nibble << 4, else unchanged; then `3210:023c(d data, d mask, bytes)`. **Quirk: does not restore**
DS:A540/BA90/A7A8/A7CC (ends with the rotation's sin/cos left in place).

### 1a21:7304 shp_read_block_a
New (claimed from platform). `near void far *shp_read_block_a(int id, void far *dst)` — load SR2_SHP set `id` (format A).
```c
DS_8172[id] = dst;                                   /* far ptr table, 4 bytes per id (8172 off, 8174 seg) */
u32 off = DIR[id].offset; u16 size = DIR[id].size;   /* DIR = DS:80A2, {u32 offset, u16 size, u16 0}[26] */
if (DS_4BBE == 0 && dos_open(DS_4BC0 /* "?:sr2_shp" */, 0, &DS_4BBE) != 0) fatal_file_missing(DS_4BC0);  /* 1a21:61e2 */
if (lib_seek(DS_4BBE, off) != 0 || dos_read(DS_4BBE, dst, size, &got) != 0 || got != size) fatal_read_error(); /* 1a21:6220 */
u16 *h = dst;  LodRec *rec = (LodRec*)(h + 3);
h[1] = FP_OFF(rec); h[2] = FP_SEG(dst);              /* set->recs */
u8 *sp = (u8*)(rec + h[0]);                          /* span data follows the n_lods records */
for (l = 0; l < h[0]; l++, rec++) {
    rec->spans = far(sp);
    for (k = 0; k < 6 && rec->part[k].count; k++) sp += 3 * rec->part[k].count;
}
return dst + size;                                   /* offset add, same segment */
```
The file handle stays open (never closed here).

### 1a21:7446 shp_read_block_b
New. `near void far *shp_read_block_b(int i, void far *dst)` — load format-B set `id = i + DS:81DA` (81DA = 23).
`DS_81CE[i] = dst` (far table: 81CE off, 81D0 seg). Open/seek/read exactly as 7304 (directory entry `id`). Fix-up:
```c
u16 *h = dst;                         /* h[0] = n_groups, h[1] = lods_per_group (7) */
LodRec far **grp = (LodRec far**)(h + 4);  h[2] = FP_OFF(grp); h[3] = FP_SEG(dst);
LodRec *rec = (LodRec*)(grp + h[0]);
for (g = 0; g < h[0]; g++) { grp[g] = far(rec); rec += h[1]; }
u8 *sp = (u8*)rec;                    /* spans follow all records */
for (g = 0, rec = grp[0]; g < h[0]; g++) for (l = 0; l < h[1]; l++, rec++) {
    rec->spans = far(sp);  for (k = 0; k < 6 && rec->part[k].count; k++) sp += 3 * rec->part[k].count;
}
return dst + size;
```

### 1a21:75ea shp_load_set
New. `far void shp_load_set(void)` — called once per race/drive by `race_run 1a21:77f4` right after road_load_graphics
(or 0dcb:8840 in mode 8).
```c
void far *p = far(DS_9E6E, DS_9E6C + DS_79E0);     /* arena: shapes go behind the current arena use */
DS_4BC0[0] = *DS_B8E8;                             /* drive letter of the game directory */
if (DS_ADD6 == 8) for (i = 0; i < DS_81DC /* 3 */; i++) p = shp_read_block_b(i, p);   /* sets 23,24,25 */
else {
    int *list = ADD6 == 0x10 ? DS_4C30 : ADD6 == 0 ? DS_4BD6 : ADD6 == 1 ? DS_4BEE
              : (ADD6 == 2 || ADD6 == 4) ? DS_4C16 : /* other: uninitialised */ 0;
    for (; *list >= 0; list++) p = shp_read_block_a(*list, p);
}
if ((u16)(FP_OFF(p) - DS_9E6C) >= DS_A9D6) fatal_out_of_memory();   /* 1a21:6232 */
```
Id lists (s16, −1 terminated): `4BD6` (mode 0, town) = 0 2 3 4 5 6 1 7 8 10 11; `4BEE` (mode 1) = 0 1 3 4 5 6 7 8 9 10 11 12
13 14 15 16 17 19 18; `4C16` (modes 2, 4) = 0 4 5 7 8 10 11 15 16 17 19 18; `4C30` (mode 0x10) = 1 3 5 6 7 8 9 10 11 15 16
17 19 18 20 21 22. Sets are packed contiguously in list order. Directory `DS:80A2` (exe data, 26 entries; offsets
0x0…0x9356, sizes 2239 262 1298 1741 706 2119 1963 478 997 1138 2194 2917 1369 2263 1357 1912 1042 1039 1234 1237
556 1180 1507 3502 1468 5024). Port: read `Game/SR2_SHP` once, build the same pointer structures per mode.

**FORMATS.md §SR2_SHP check**: layout, directory, lists, mode 8 / format B and the mirror rule (`<= 0`) are confirmed.
Corrections: (1) the LOD record has **no** `u16 pad`: it is 6 × {colour,count} (12 bytes) + far span ptr at +12 = 16
bytes; (2) shape_draw is `0dcb:4154` (file segment), args (x, y, lod, set far*, side); (3) LOD = `clamp(9 − depth, 0, 6)`
(`0…7` for set 2), LOD 0 = farthest/smallest, `lod >= n_lods` draws nothing; (4) format B is drawn by 0dcb:8288 (not
shape_draw): set header `[0] n_groups`, `[2..3]` far ptr to the group table; it takes group `g` (checked < n_groups)
and LOD `DS:AD68` (no check against lods_per_group), rotates the origin with 0379 and draws unmirrored hlines
`(x+dx, x+dx+w, y+dy)`; when `DS:B190 >= 0x37` it uses LOD 3 and draws each span as a rotated clip_line instead.

### 1a21:9b96 heap_free

`far int heap_free(void)` — new: `return DS_B7EC[DS_A7D8] - DS_B7EC[DS_A31E];` (bytes between the high mark
stack top, index `DS:A7D8` counting down from 0x3C, and the low mark stack top, index `DS:A31E`). Platform.

### 1a21:9bac bitmap_bytes

`far int bitmap_bytes(int w, int h, unsigned flags)` — new (Ghidra label "bitmap_make" is wrong): `plane =
(((w-1) >> DS_AD20) + 1) * h; return 0x14 + (flags ? plane : 0) + ((flags & 2) ? 0 : plane * DS_AD1E);` = the
size arena_bitmap_alloc 1a21:9a68 would allocate (header 0x14, mask plane, DS:AD1E data planes). Platform/video.

### 1a21:a6e8 bitmap_visible_width
`far int(Pic far *p)` — width without the fully transparent right margin: AND together the last mask byte
of every row (`mask = far ptr p+0xA/+0xC`, `bpr = p+0xE`, rows `p+2`; byte at `mask + bpr - 1 + row·bpr`),
count trailing 1-bits from bit 0 (max 8) = `n`; return `p->w - n / DS_AD22` (AD22 = video pixels-per-mask-bit
divisor, 1 on EGA/VGA). Belongs with the bitmap helpers (video), only used by this module.

## 4. Handoffs

**Functions claimed from other modules** (port/sr2_modules.csv lists them elsewhere; the S3.7 coverage merge must drop
them from the other module if it also specified them):

| address | name here | listed under | reason |
|---|---|---|---|
| 0dcb:504a / 507a / 50c4 / 50f4 | town_walls_front_left / _front_right / _mirror_left / _mirror_right | race | SR1 race_render marshallers (2645:39de/3a0d/3a57/3a86); matcher names of 507a / 50f4 were swapped |
| 1a21:7304 / 7446 / 75ea | shp_read_block_a / shp_read_block_b / shp_load_set | platform | SR2_SHP loaders (assigned to race_render by the coordinator) |
| 0dcb:2866 | track_build_all | game_flow | = SR1 track_build_all 2645:0d48 |
| 0dcb:148a | road_edge_collision | sound | = SR1 2645:206b, called from race_isr every tick |
| 0dcb:1cd2 | aq_wall_collision | sound | helper of 148a (Aqueduct walls) |
| 0dcb:1470 | track_lane_x | race | reads the track `LANE` array; caller is race AI 0001:cf9a |
| 0001:d2a8 | aq_lane_target | race_render (kept) | Aqueduct lane choice used by road_advance_segment (autopilot) **and** race AI 0001:d4cc — candidate for race; keep a single owner |

**Specified here but used by / belonging partly to other modules:**
- `0dcb:a2d8` aq_collide_probe: render-math query called by race physics 0001:cee0.
- `0dcb:031a` / `02b4` sin256: also used by the gauges (`speedo_points` 1a21:015c, `tach_points` 1a21:038e, 1a21:070e
  — race / video modules own the gauge code).
- `0dcb:0108`, `0dcb:01da`, `0dcb:b984`, `b9e0`, `ba72`, `c09a`, `c342`: CGA / Tandy driver slots (parked drivers, video).
- `1a21:9b96` heap_free, `1a21:9bac` bitmap_bytes, `1a21:a6e8` bitmap_visible_width: generic heap / bitmap helpers
  (platform / video), specified here because only this module calls them.
- `1a21:1924` is **`pace_ticks(int n)`** (waits until the tick counter `DS:1F66` passes the deadline `DS:1D02`, then sets
  the next deadline), owned by video; port/symbols.csv misnames it `font_string_width`. Listed in the symbols file as
  an `xref` row (not a coverage row).

**Data passed to race (physics / AI / police):** `DS:A7D2` Aqueduct hit bits (1/4 wall openings, 0x20/0x40/0x60 floor
obstacles); `DS:BB88` / `DS:ADD4` rock / obstacle hits in row 1 (Mulholland); `DS:7B9C` / `DS:7F2C` cliff fall;
`DS:7BA2` on_shoulder; `DS:9A48` opponent rear-end contact, `DS:B8C0` alongside; `DS:985C` / `DS:A558` road edges 4
segments ahead; `DS:B8C8` Aqueduct wall hit side. The Aqueduct drawers write physics state (`A564`, `A56A`, `A100`)
— keep the call order of road_frame. Race-module callees referenced: 0001:d908, 0001:cb00, 0001:d4cc, 0001:cf9a,
0001:cee0, 0001:bc88, 0001:be82.

## 5. Open questions

- **FORMATS.md corrections (not edited here):** (1) SR2_SHP LOD record = 6 × {colour, count} (12 bytes) + far span
  pointer at +12 — there is no `u16 pad`; (2) `shape_draw` is file address **0dcb:4154** (1dcb is the Ghidra segment);
  format B is drawn by 0dcb:8288, not shape_draw; (3) SR2_DATA block 7 = shrink masks (18 records × {7 angles × 3 mask
  bytes + pad}), block 8 = opponent sprite id table (6 × 14 bytes, LIB3 ids, pointers into block 7 re-set by
  hot_data_load) — not "road graphics".
- `C&0x20` / `C&0x40` project both road edges to 0xCE4 / 0x5DC (zero-width road) in road_project_front — SR2 bug or
  intended for the roadworks placed only on Mulholland stretches (`BL != BR`)? Check in DOSBox.
- Hillside peak tables `DS:7E1E` / `7E3A` are indexed with 15−row (6a32) and 16−row (4322) and read into the dash table
  for near rows: looks like an original bug — reproduce faithfully.
- Visual meaning of Mulholland `C&0x80` (truss bridge vs tunnel) and of the bank arrays BL/BR (names are guesses).
- opponent_draw af98: in mode 8 with `9A48 || A1BC == 3`, `sx + r.w` uses an uninitialised Rect width; with s = −1 it
  reads `DS:B7B2` as a sprite pointer. Port: use the current picture width; return when s < 0 or s > 12.
- a85c `if (y > 0)` pins the starter figure to y 0x27 (probably meant `y - 0x2A > 0`); kept as coded.
- shape_draw: an unmirrored span crossing a clip edge is dropped entirely (hline rejects before swapping); faithful.
- `0dcb:03cc` rotate_vec has no callers (dead code?).
- What the start panels LIB3 #2067/#2070 (48×82), #2068/#2071 (144×82), #2069 (176×87) show; `DS:9DC8/9AD8` = far
  3722:0000/0002 set by 8840 (consumer unknown, probably race AI of mode 8); why demo mode skips the opponent sprite
  reload for modes 2/4.
- 69d4 column constants (100, 130, 289, 319) assumed to be the player's wheel columns.
