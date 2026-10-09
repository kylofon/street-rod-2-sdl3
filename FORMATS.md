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

## Still to decode
`SR2_SHP` (S2.2), `SR2_DATA`, `SR2_END`, exe tables (S2.3), save games (S2.5).
