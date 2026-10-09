# Street Rod 2 file formats

SR1 formats: `../StreetRod/FORMATS.md`. Only the differences and the new files are described here.

## Picture libraries `LIB1`, `LIB2`, `LIB3` (+ `C` = CGA) — `tools/srlib.py`

Same packing as SR1 (0xBC marker, 00/FF runs, 16 run tokens, planar 4-plane pictures), but the
directory record is **28 bytes** (SR1: 30), read by `lib_read_dir` 1a21:6432:

| Offset | Type | Field |
|---|---|---|
| 0 | u16 | width |
| 2 | u16 | height |
| 4 | u8 | depth: 4 = 16 colours, 2 = CGA (the SR1 `raw_size` field is gone: raw = w×h×depth/8) |
| 5 | u8 | masked (colour 0 transparent) |
| 6 | u8[16] | run tokens |
| 22 | u32 | offset from the end of the directory |
| 26 | u16 | packed size |

Libraries: `LIB1` 4 pictures (library id 1, index base 0), `LIB2` 212 (id 1000, base `DS:B8CA`),
`LIB3` 83 (id 2000, base `DS:B8CA + DS:B8D6`). In memory each record becomes 0x22 bytes
(+ `1C` = FF, `1E`/`20` = 0). The game EGA palette (SR1 `DS:0440`) is at **`DS:1DC8`** (also
`DS:1E2E`); SR1's credits palette does not occur. Output: `work/lib1..3[c]/NNN_WxH[m].png` +
`_sheet.png`.

## `MSR.SND` — AdLib timbre bank — `tools/sr2snd.py`

`u8 major (1), u8 minor (0), u16 count (30), u16 data_offset (0x114)`, then `count × char[9]`
names from offset 6, then at `data_offset` `count × 28 u16` parameters (modulator 13, carrier 13:
ksl, multi, feedback, attack, sustain, eg, decay, release, level, am, vib, ksr, fm; then the two
wave selects). Timbres: piano1 jazzguit tom1 bdrum1 snare1 cymbal1 hihat1 syn3 police turn1 crash
part turn paint hitcar motor guitar1 bass1 hihat2 tuexplos fuzguit2 moogsynt bass2 elpiano1 scratch
snakefl logdrum1 elguit1 logdrum2 motor1 — so the bank also holds **sound effects** (police, crash,
hitcar, motor, paint, turn …), played as AdLib notes.

## `*.MUS` — AdLib Visual Composer songs — `tools/sr2snd.py`

70-byte header (tick_beat at 36, total_ticks 38, data_size 42, nr_commands 46, sound_mode 58,
pitch_bend_range 59, basic_tempo 60), then MIDI-like events with running status and delay bytes
(F8 = 240 ticks). All six songs are **percussive mode** (6 melodic + BD SD TOM CYM HH), tick_beat
240, bend range 1:

| Song | Tempo | Length | Use (guess, S3.2 confirms) |
|---|---|---|---|
| `MSRBEG` | 120 | 37 s | title |
| `MSR2` | 120 | 64 s | in game |
| `MSR20` | 120 | 8 s | jingle |
| `MSR21` | 60 | 36 s | ? |
| `MSR24` | 120 | 26 s | ? |
| `PAUSE` | 120 | 4 s | pause |

Renders: `python tools/sr2snd.py render` → `work/sound/*.opl` (7-byte OPL write log) and `*.wav`
through `work/oplrender.exe` (Nuked-OPL3, source `tools/sound/oplrender.c`; build:
`gcc -O2 -I../F117-2/third_party/nuked-opl3 tools/sound/oplrender.c ../F117-2/third_party/nuked-opl3/opl3.c -o work/oplrender.exe`).
The player follows the Ad Lib Inc. toolkit driver; SR2's own driver is checked in S3.2.

## `SR2_SHP` — vector span shapes — `tools/sr2shp.py`

Not a bitmap library. 42742 bytes = 26 "shape sets" back to back (scenery / traffic seen on the road, drawn
as coloured horizontal lines; no pictures). Directory = 26 × 8 bytes at **DS:80A2** (exe data):
`{u32 file offset, u16 size, u16 0}`; offsets are contiguous and cover the file exactly. Shape id = set
number. Loaded by `1a21:75ea` (sets named by id lists at DS:4BD6 / 4BEE / 4C16 / 4C30, `-1`
terminated, chosen by `DS:ADD6`) with `1a21:7304` (format A, sets 0..22) or, in mode 8, `1a21:7446`
(format B, sets 23..25; first id `DS:81DA` = 23, count `DS:81DC` = 3). Both open `?:sr2_shp`
(DS:4BC0, first char = drive) once, `lib_seek` to the offset and read the set. Drawn by
`shape_draw` 0dcb:4154 (x, y, lod, set ptr, mirror; format A; LOD = clamp(9 − depth, 0, 6)); format B (Aqueduct, road mode 8) by 0dcb:8288, where the 7 "LODs" are 7 pre-rotated versions chosen by the roll angle (`port/spec/race_render.md`).

```
format A set:  u16 n_lods | u16 ptr_off | u16 ptr_seg   (last two are runtime pointers, file junk)
               n_lods x 16-byte LOD record
               span data
format B set:  u16 n_groups | u16 lods_per_group (=7) | u16,u16 runtime | n_groups x u32 runtime far ptrs
               n_groups x lods_per_group x 16-byte LOD record | span data      (set 25 has 9 groups, 23/24 one)
LOD record:    6 x {u8 colour, u8 count}  (stops at count 0; colour = EGA palette register 0..15)
               u32 runtime pointer to this LOD's span data (+12); 12 + 4 = 16 bytes, no pad
span data:     for each LOD record in order, for each (colour, count): count x {i8 x, i8 y, i8 w}
               = horizontal line from x to x+w at row y, relative to the object origin
                 (x mirrored when the mirror flag is <= 0)
```

LOD k is chosen by distance (7 levels, spans grow from 13 to 245 lines in set 0). The header + 16 × n +
3 × Σcount equals the set size for all 26 sets. Colours use the game EGA palette `DS:1DC8`. Output:
`work/shp/NN_[AB].png` (all LODs of every group side by side, 2×), `_sheet.png` (first group of every
set), `dump.txt` (parts and span counts).

## `SR2_DATA` — game tables (SR1 `HOT_DATA` successor) — `tools/sr2data.py`

3607 bytes read by `hot_data_load` **1a21:6e6a** (file name pointer `DS:14D8` → `"SR2_data"`, the next
string `"SR2_end"` is at `DS:14DA`; drive letter patched like SR1). Nine blocks are copied to DGROUP
(destination table `DS:14DC`, size table `DS:14F0`; the 8th destination `DS:14EA` is set to `8E3C` at
run time), then a **text pool** of 1277 bytes (the rest of the file) is read to `DS:8860`. The loader then
fills pointers: car `+12` = `DS:8860 + tbl[i]`, person `+6` = `DS:8860 + tbl[22+i]` (`tbl` = block 6).
SR1 had 13 blocks / 26 cars; SR2 has 9 blocks and **22 cars + 22 people**; message texts are *not* in the
file (they are plain strings in the exe, referenced by direct DS pointers instead of `DS:2C02 + id`).

| # | file off | size | DS dest | content | SR1 block |
|---|---|---|---|---|---|
| 0 | 0x000 | 396 | AB7E | 22 × 18 people (drivers / sellers), below | 0 (opponents) |
| 1 | 0x18C | 440 | A80C | 22 × 20 car models, below | 1 (10-byte models) |
| 2 | 0x344 | 220 | BAAC | 22 × 10 overlay pictures `{u8 race, u8 ?, u16 roof, u16 hood scoop, u16 rear bumper, u16 front bumper}` (LIB2 ids, 0 = none) | 2 |
| 3 | 0x420 | 264 | 9BF4 | 22 × 12 overlay positions (car compose, `1a21:b9bc`) | 3 |
| 4 | 0x528 | 440 | 9872 | 22 × 20 five hot-spot rectangles per model (`0001:a270`) | 4 |
| 5 | 0x6E0 | 2 | A542 | head of the for-sale chain (0) | 7 |
| 6 | 0x6E2 | 88 | BBF4 | 44 × u16 offsets into the text pool: [0..21] ad text of car i, [22..43] name of person i | (text block 6) |
| 7 | 0x73A | 396 | 8E3C | opponent sprite **shrink masks**: 18 records × 22 bytes = 7 viewing angles (DS:7FBE, −45°..+45° in 15° steps) × 3 size steps + 1 pad; normal races use the 0° masks (bytes 9..11), the Aqueduct all angles (0dcb:ad40). See `port/spec/race_render.md` §1.7 | new |
| 8 | 0x8C6 | 84 | AA9E | opponent sprite table (SR1 DS:5E14) 6 × 14: `{u16 side id, u16 rear-3/4 id, u16 rear id, u16 mask ptr A, u16 mask ptr B, u16 front id, u16 mask ptr C}` (LIB3 ids 2001..2082; ptrs set by the loader to 8E3C + 66·i (+22, +44)); row = opponent model's block-2 byte 0, row 5 = police | SR1 table in exe |
| – | 0x91A | 1277 | 8860 | text pool: NUL-terminated strings, `+` prefix = continuation line, backtick = space | – |

Car record (DS:A80C + 20·i; 21 cars + #21 = the King's car):

| Off | Type | Field |
|---|---|---|
| 0 | i16 | price ($) |
| 2 | i16 | trade-in / sell value |
| 4 | u8 | class (0 slow .. 8 fast; copied to car `+5`) |
| 5 | u8 | next model in the Used Cars chain (head `DS:A542`; 255 = end) |
| 6 | u8 | runtime `random(5)` (loader) |
| 8 | u16 | spec A: bits 1-3 make (0 GM, 1 Ford, 2 Chrysler), 4-5 tires, 6-7 transmission grade, 8-9 engine grade, 10-13 manifold grade, 14-15 carburettor type |
| 10 | u16 | spec B: bits 0-4 exhaust-manifold grade, 5-9 muffler, 10-15 differential |
| 12 | u16 | runtime ptr to the ad text (file value is a leftover) |
| 14 | u16 | ad text lines |
| 16 | u16 | picture (LIB2 id) |
| 18 | u16 | flag/colour bits (e.g. 0x90CD), not decoded |

`car_new` **0001:42aa** turns spec A/B into part catalogue indices: engine `grade*3+make`, transmission
`(grade+3)*3+make`, carburettor `0x15+n`, manifold `(grade+8)*3+make`, tires `0x32+n`, exhaust
`grade*3+make+0x35`, muffler `0x3B+n`, differential `0x3D+n`. The decoded stock parts of every car are in
`work/data/data_blocks.txt`. Person record (DS:AB7E + 18·i): `u8 a,b,c,d` (race parameters), `u16 face`
(LIB2 id 1161..), `u16 name ptr` (runtime), the rest is runtime state (saved in savegames). Car i and person i
form the seller/car pair of the used-car ads (0 = "Phil", "1951 Ford Victoria").

Reading functions: `hot_data_load` 1a21:6e6a (all); `car_new` 0001:42aa, `classifieds_run` 0001:0ac8;
`opponent_init` 0001:51f8 / `opponents_init` 0001:534c; hot-spots 0001:a270; opponent sprites 0dcb:37ce, 0dcb:ad40.

## `SR2_END` — ending animation — `tools/sr2data.py`

1236 bytes, read by `FUN_2925_2714` into `DS:9714` (0x1F8 bytes) and `DS:9710` (0x2DC bytes); played by
`FUN_2925_27da`. It cuts sprites from LIB2 picture 1181 (0x49D) over the backdrop 1211 (0x4BB), final screen 1195 (0x4AB).

| Off | Size | Content |
|---|---|---|
| 0 | 84 × 6 | frames `{u8 w, u8 h, u16 src_x, u8 src_y, u8 0}` (rectangle in the sheet; `bitmap_extract`) |
| 0x1F8 | 183 × 4 | script `{i8 frame (-1 = end), u8 dst_x, u8 dst_y, i8 flag}`; flag ≠ 0: redraw the dirty rectangle and wait 10 retraces; flag < 0: switch to the final screen |

182 steps + terminator `FF 00 00 00` fill exactly the 732 bytes; 80 retraces after the last step.

## Exe tables (DGROUP; DS offset = exe file offset − 0x40C80)

Dumped by `tools/sr2data.py` to `work/data/exe_tables.txt`.

| DS | Content |
|---|---|
| 4502 | **Part catalogue**, 64 × 10 bytes `{i16 price, u8 grade, u8 make mask (1 GM, 2 Ford, 4 Chrysler, 7 all), u8 category, u8 0, u16 text ptr (DS), u16 text lines}`. Categories: 0 engine (idx 0-8), 1 transmission (9-20), 2 carburettor (21-23), 3 manifold (24-49), 4 tires (50-52), 5 exhaust manifold (53-58), 6 muffler (59-60), 7 differential (61-63). SR1: 43 × 8, 5 categories |
| 478C | initial wear per category, 8 × i16 (`0 0 -128 -128 0 -128 -128 -128`; −128 = does not wear) |
| 4E6E | carburettors per manifold grade, 9 × i16 `1 1 3 2 2 1 2 0 0` (SR1 DS:5126) |
| 4D1E | paint colours, 6 × `{EGA reg 6, EGA reg 7, u8}` (SR1 DS:50C2, 2 bytes each) |
| 47A8, 47C4, 47E0, 4E80, 4E8C, 4E9A, 4EB4 | engine-bay / compatibility / picture tables for `engine_bay` 0001:80a8 and `car_runnable` 0001:7a44 (dumped raw; 47E0 holds LIB2 ids 1061-1082) |
| 1DC8 | game EGA palette registers |
| 80A2 | SR2_SHP directory (26 × 8) |
| 4BD6 / 4BEE / 4C16 / 4C30 | shape-set id lists per road mode |
| 2200-2840, 3BF0-4500, 4A80-4E50, 72E0-7830 | message and part text pools (direct DS pointers; no `TEXT_BASE + id`) |

The SR1 stock top-speed tables (`DS:556E/552E/554C/5552`) and the sticker table were not located (not needed
for the file formats); `car_max_speed` 0001:c706 reads `4E6E` and the car/part records.

## Save games `X:HOTRODn.SAV` — save_game_screen 0001:5d82, load_game_screen 0001:59e2, save_slots_scan 0001:58b2

Name = drive of the data disk + `":HOTROD"` (DS:4CAE) + decimal n + `".SAV"` (DS:4CB6). **n runs 0..14** (15
slots; `save_slots_scan` loops `DS:8FC8 + 0x10·k`, absent slots are 0xFF in the table at `DS:90B8`; SR1 used 1..15
and 0 for absent). No magic or checksum; a raw dump; near pointers only mean "non-zero = present / chain follows".

```
0      0x38   header = DS:A9D8..AA0F verbatim (SR1: 0x24); +00 char name[16] (list shows 15 chars);
              +2A u16 cur_car (<>0: one CAR BLOCK follows)  +2C u16 car_list (<>0: CAR BLOCKs until next == 0)
              +32 u16 spare_parts (<>0: a PART CHAIN follows)   (money and counters: not decoded)
       [CAR BLOCK] if cur_car; [CAR BLOCK]* if car_list; [PART CHAIN] if spare_parts
+0     12     DS:479C  (SR1: 8 bytes DS:4970, counters)
+0C    0x18C  DS:AB7E  people table (22 x 18)
+198   4      DS:1F6A  game clock (u32)
+19C   4      DS:1F6E - DS:1F72 elapsed ticks (u32); on load DS:1F72 = DS:1F6E - value
+1A0   2      DS:7DFE  (SR1 DS:5E0C)
+1A2   0x80   DS:9B40  new in SR2 (128 bytes, not decoded)
```

(The offsets after the chains are relative to the end of the chains, as in SR1.)

CAR BLOCK: a **0x2A-byte** car record (SR1 0x28), then one PART CHAIN for each non-zero pointer field in the order
`+06, +08, +0A, +0C, +0E, +10, +12, +1C, +1E, +20` (SR1: 7 fields; SR2 has 11 part slots: the extra ones are
exhaust manifold, muffler, differential); next-car pointer at `+28` (SR1 `+26`). PART CHAIN: 8-byte part records
`{value, wear, type = catalogue index, next}`; a record with `+6 <> 0` is followed by another (same as SR1).
Messages: "No saved games found !", "Cannot read that game !", "Disk error during Save".

## Still to decode
- Meaning of the raw engine-bay / compatibility tables (47A8..4EB4) and of the 0x80-byte save block at DS:9B40.
- Save header fields (money, counters), the 12 bytes at DS:479C, car record flag word `+18` in SR2_DATA.
- SR1 sticker table and top-speed tables (not located in SR2).
- Sound driver playback (S3.2).
