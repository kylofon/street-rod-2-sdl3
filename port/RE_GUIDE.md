# Reverse-engineering guide for the Street Rod 2 port

Target: **SR.EXE** (Street Rod 2: The Next Generation, 1991), VGA 16-colour path. The SR1 port
(`../StreetRod/port/RE_GUIDE.md`, `spec/`, `srport/`) is the base: look a function up in
`port/sr2_sr1_matches.csv` before analysing it.

## Files

| Path | What |
|---|---|
| `work/SR2_unp.exe` | EXEPACK-unpacked MZ (`python tools/unexepack.py Game/SR.EXE work/SR2_unp.exe`); image offsets exclude the MZ header |
| `work/survey.txt`, `work/survey_unp.txt` | `tools/survey.py` output (packed / unpacked) |
| `port/sr2_functions.json` / `.csv`, `sr2_starts.txt` | Capstone index (`python tools/sr2index.py work/SR2_unp.exe port/sr2`) |
| `port/sr2_sr1_matches.csv` | every SR2 function with its SR1 counterpart (`python tools/sr2match.py`) |
| `port/symbols.csv` | SR1 names carried over to SR2 addresses (notes: SR1 address, class, method, score) |
| `tools/x86dis.py work/SR2_unp.exe dis SSSS:OOOO LEN` | ground-truth disassembly |

## 1. Executable

* `SR.EXE` 264 468 bytes, **Microsoft EXEPACK** (the stub at 3F08:0010 carries the usual
  "Packed file is corrupt" message; `tools/unexepack.py` from SR1 works unchanged):
  258 176 packed → **288 912** bytes unpacked, entry **34b0:0018**, **2721 relocations**.
* Compiler: **Microsoft C 6.0** ("MS Run-Time Library - Copyright (c) 1990"); SR1 was MSC 5.1.
  The MSC 6 startup is `mov ah,30h / int 21h / cmp al,2 / jae +5 / xor ax,ax / push es / push ax /
  retf / mov di,DGROUP`; `tools/sr2index.py` accepts both forms.
* **DGROUP = 0x3E08** (image `0x3E080`). `DS:xxxx` = image `0x3E080 + xxxx`.
* Medium model, **34 code segments**, **913 functions** indexed (SR1: 31 / 769; 863 before the S1.5 seeds), 81 021
  instructions, 13 jump tables, 85 % of the code bytes covered (96 % without the 28 KB data block
  3725:0000-3726:6e20 that the segment scan counts as code). Seeds of `sr2index.py` (interrupt
  handlers, graphics and sound far-vector tables, far immediates): see §3.7.
* Addresses are written `SSSS:OOOO` with the segment as stored in the file; image offset =
  SSSS×16 + OOOO.

## 2. Correspondence with Street Rod 1 (S1.3)

SR2 shares SR1's game code and texts (239 of SR1's 345 strings of 8+ characters occur in SR2), but
the rebuild with MSC 6 changed the generated code, so only hand-written assembly and the graphics
library match instruction for instruction. `tools/sr2match.py` matches in passes: masked code
windows → distinctive strings → DGROUP globals (SR1→SR2 offset map from shared data strings and
code-matched operands) → call-sequence alignment → loose mnemonic windows → link order within
corresponding segments, repeated until stable.

Result: **56 same, 435 changed, 372 new** of 863; **491 of SR1's 769** functions found; 423 SR2
functions carry an SR1 name. Check: of the pairs whose callees are themselves matched, ~95 % map
their callees consistently (lowest: `calls` 32/36, `code` 21/23); spot checks right
(`video_menu`, `save_game_screen`; the sound-options menu is correctly `new`). `changed` means
"same source function, recompiled or edited": diff the behaviour against the SR1 spec, do not
assume an edit.

| SR1 module | same | changed |
|---|---|---|
| platform | 16 | 79 |
| video | 8 | 82 |
| sound | 1 | 14 |
| game_flow | 1 | 23 |
| garage | 1 | 83 |
| race | 5 | 88 |
| seed (runtime etc.) | 8 | 14 |
| unnamed in SR1 | 16 | 52 |
| **new in SR2** | | **372** |

Segment correspondence (SR2 → SR1, functions):

| SR2 seg | funcs | SR1 seg / new | Notes |
|---|---|---|---|
| `0001` | 189 | `0000` 136, new 50 | SR1 main game segment |
| `0dcb` | 127 | `2645` 46, new 80 | grew a lot |
| `1a21` | 185 | `0f38` 143, new 41 | |
| `2719` | 28 | `1c11` 21, new 7 | |
| `2925` | 36 | new 35 | new module; holds a second "Video Options" menu (2925:2e60) |
| `2c57` | 107 | new 92, `0f38` 13 | new module (AdLib/new features? S1.5) |
| `2f1b` | 6 | new | |
| `2f32` | 36 | `21a0` 33 | graphics library (EGA/VGA driver) |
| `3210`–`33e5` | 1–6 each | `2460`–`2634` | small assembly segments, as in SR1 |
| `3488`, `33f6` | 3, 1 | new | |
| `34b0` | 111 | `1e16` 62, new 49 | C runtime (MSC 6 vs 5.1: the new ones need `apmatch.py` against an MSC 6 library) |
| `371c`–`3723` | 7 | `2e3e`/`2e68` 2, new 5 | |

## 3. What changed (S1.5)

**Address convention.** The indexer splits the first code block at the first relocation target, so
`0001:xxxx` is the *same code* as `0000:(xxxx+0x10)` at run time (`lcall 0000:3388` = `0001:3378`;
image offset = `xxxx + 0x10`). Jump-table words and `cs:` operands in this block are image offsets.

### 3.1 Segment map (34 code segments, 913 functions)

| SR2 seg | funcs | Contents (SR1 equivalent) | Modules |
|---|---|---|---|
| `0000` | 1 | 16-byte stub | runtime |
| `0001` | 191 | main game segment (SR1 `0000`): `main` 07b0, `game_loop` 4dd6, ISRs, game_flow, garage, race glue; new: `sound_options_menu` 01ea, `car_info_sr2` 72f2, `drive_to_event` 8c68 | garage 54, race 51, game_flow 36, platform 34, sound 12, video 3 |
| `0dcb` | 127 | race: track, renderer, road/opponents (SR1 `2645`; 46 matched, 80 new) | race_render 118, race 5 |
| `1a21` | 187 | library (SR1 `0f38`): UI, pictures, LIB/data loading, hall of fame, shapes (`shp_*` new) | garage 61, video 58, platform 27, game_flow 21, race 15 |
| `26b3` `26c0` `26e6` | 1 / 3 / 0 | driver descriptor data and blit stubs (SR1 `0e92` / `0e6c` / data); `26c0:0002/00c1/0175` = slot 4 of the three vector tables | video |
| `2719` | 30 | CGA/Hercules driver C part (SR1 `1c11`); `2719:01a6` copies table `DS:7A40` | video |
| `2925` | 36 | **new game code**: tournament/round event (`ROUND %d`, `FINAL STANDINGS`, `Prepare to set your breakout`; 000e-1f38), `event15_run` 2104, `event17_run` 1970, ending (`end_data_load` 2714 reads `SR2_end`; 27da, 2b26), second `STREET ROD II:  Video Options` menu 2e60, stickers 3174 | race 18, game_flow 14, video 4 |
| `2c57` | 133 | **new sound module**: speaker effects (SR1 `0f38` sound part), two 25-slot sound vector sets, AdLib driver, `.SND`/`.MUS` loaders, MUS interpreter `mus_tick` 2875 | sound |
| `2f1b` | 7 | **music timer**: PIT programming + dedicated INT 8 ISR `mus_timer_isr` 00cb (state lives in the code segment) | sound |
| `2f32` | 36 | EGA/VGA driver (SR1 `21a0`); `tdy_install_vectors` 0906, `ega_install_vectors` 111c | video |
| `320d`-`3298` | 1+6+5+2+2+2 | driver assembly (SR1 `2460`-`24e7`); `3236` also holds Tandy slots 12-14 | video (platform 6) |
| `32a5` `3345` `33e5` | 1 / 1 / 2 | EGA plane routines (SR1 `24f4`, `2595` rows_mask_or `3345:000c`, `2634` rows_mask_blend `33e5:0002`) | video |
| `33f6` `33f8` `3422` `3458` `3488` | 1/6/6/5/3 | **new** per-driver assembly: `33f8:01ca` EGA/VGA, `3422:025d` CGA/Herc, `3458:0228` Tandy (slot 18 of the tables); `3488` helpers | video |
| `34b0` | 111 | MSC 6 C runtime (SR1 `1e16`): startup 0018, `_dos_getvect` 241a, `_dos_open` 242c, `_dos_setvect` 2474, `_harderr` 249e | runtime |
| `371b`-`3726` | 7 | runtime stubs and data (`3725:0000`-`3726:6e20` is data, not code) | runtime |

Module of every function: `port/sr2_modules.csv` (`tools/sr2_modules.py`: SR1 module for matched ones,
`34b0`/`371x`/`0000` = runtime, new ones by same-segment / game-segment neighbour vote, then segment
default 0dcb race_render, 2925 game_flow, 2c57+2f1b sound, driver segments video). Counts: video 177,
sound 158, race_render 123, runtime 119, garage 115, race 87, game_flow 72, platform 62.
`2925` and the unmatched `0001`/`1a21` functions are votes, not facts: treat as a first guess.

### 3.2 main and the state machine

* `main` = `0001:07b0` (SR1 `0000:066f`), `game_loop` = `0001:4dd6` (SR1 `0000:503f`),
  `title_and_setup` 3cc4, `garage_screen` 6912, `quit_menu` 630e, `your_cars` 4852,
  `save_game_screen` 5d82, `load_game_screen` 59e2 (`58b2` is a helper), `new_game` 5400.
* `main` order: `cpu_speed_calibrate`, switches (3.6), `video_detect_menu` 037c, `adlib_detect`
  2c57:1733, `sound_options_menu` 01ea (only if an AdLib answered), `mem_pools_init`, `driver_select`,
  `gfx_init`, `arena_init`, `platform_install` 19e8, `speaker_sound_init` 2c57:05c8 or
  `adlib_sound_init` 2c57:13d8, `lib_open_all`, `lib_read_dir(1)`, picture list, then `lib_read_dir(2)`,
  `mus_songs_load` (AdLib only), `hot_data_load` (`SR2_data`), `hall_load`, `wait_click_or_key`,
  `game_loop`, `platform_exit`.
* `game_loop` as in SR1 (title, new / load, garage dispatch by menu code) with these differences:
  (1) `title_and_setup` calls the manual-lookup protection (3.5);
  (2) `load_game_screen` scans `?:HOTROD<n>.SAV` ("No saved games found !", "Cannot read that game !");
  (3) codes 2 / 15 / 17 are three **event trips** that set `DS:1BC8` after `drive_to_event` 8c68:
  2 = Bob's Drive-In (`bob_drive_in` a626, `DS:1BC8`=2), 15 = `event15_run` 2925:2104 (`DS:1BC8`=3; needs
  money, car value `+0x24` < 0x96), 17 = `event17_run` 2925:1970 (`DS:1BC8`=4 then 2; only when
  `day_index() % 7 == 2`, else message 3; confirm box 1a21:1e50); SR1's "gas station" and "Wanna drink?"
  are gone/replaced;
  (4) code 14 (stickers) = `2925:3174`, code 16 = `0001:72f2` (car info with mph / gallons), code 10 =
  `1a21:9b26` + `engine_bay` 80a8;
  (5) juke-box code -40 toggles `DS:5304` with `music_resume`/`music_stop` (1784/1728) instead of `DS:58D0`;
  (6) game over (0x29A): `summer_over_screen` if `DS:1BC6 == 1`, `hall_of_fame_screen(DS:1BC6 & 2)`,
  `game_over_menu` 348a.
* The `quit_menu` result switch is the jump table at image `0x50fc` (index = code + 0x17; -0x17 Save,
  -0x16 / -4 Forget it, -0x14 Quit).

### 3.3 File loading

`?:` is replaced by the current drive letter (floppy: prompt "Insert disk ? into drive ?").

| File | Opened / read in | Lands in |
|---|---|---|
| `LIB1` / `LIB1C` | `lib_open_all` 1a21:6be0 (name table `DS:4B56` lib1, `4B58` lib1c; non-`c` only for video mode 2 or 5), `lib_select` 62c4 (pic index < 4), `lib_read_dir` 6432, `pic_load` 652a | handle `DS:4B50`, entry count `DS:B8CA`, name ptr `DS:B8DE` |
| `LIB2` / `LIB2C` | same (`DS:4B5A` / `4B5C`); pic index 4..0xD7; header word must be 0xD4 | handle `DS:4B52`, count `DS:B8D6`, name ptr `DS:B8E2` |
| `LIB3` / `LIB3C` | **new**: opened only from a hard disk (`cur_drive` > 2); pic index >= 0xD8; header must be 0x53 | handle `DS:4B54`, count `DS:B8DA`, name ptr `DS:B8E8` (`4B5E` / `4B60`) |
| `SR2_data` (ptr `DS:14D8` -> `DS:14C6`) | `hot_data_load` 1a21:6e6a (SR1 `hot_data`): 9 blocks to `DS:14DC[9]` sizes `DS:14F0[9]`, then `DS:BC4A+0x50` bytes to `DS:8860` | game tables in DGROUP; floppy re-check `data_disk_check` 7052 |
| `SR2_shp` (`DS:4BC0`) | existence check in `hot_data_load`; `shp_load_set` 1a21:75ea (mode `DS:ADD6` 0/1/2/4/8/16) calls `shp_read_block_a` 7304 / `_b` 7446; offsets from the 8-byte table at `DS:80A2`; handle `DS:4BBE` | shape sets at `DS:9E6C:9E6E + DS:79E0` |
| `SR2_end` (ptr `DS:14DA` -> `DS:14CF`) | `end_data_load` 2925:2714: 0x1f8 bytes to `DS:9714`, 0x2dc to `DS:9710` | ending data |
| `hall_dat` | `hall_load` 1a21:711c / `hall_save` 71cc (`DS:4BCA`) | as SR1 |
| `?:HOTROD<n>.SAV` | `load_game_screen` 0001:59e2 + 58b2, `save_game_screen` 5d82 | car / part chains |
| `MSR.SND` | `snd_bank_and_intro_load` 2c57:0aa2 -> `snd_bank_load` 14aa (name `DS:72F0`), `snd_bank_init` 151b | AdLib timbre bank, handle `DS:AB2A` |
| `MSRBEG.MUS` | 0aa2 -> `mus_file_load` 09ca (name `DS:72F8`, slot 1) | song slot 1, descriptor `DS:B930` |
| `MSR2` `MSR24` `MSR21` `PAUSE` `.MUS` | `mus_songs_load` 0ade -> `mus_file_load` slots 2-5 (names `DS:7304/7310/731C/7328`, drive letter patched by `lib_open_all`), descriptors `DS:B976` | song slots 2-5 |

### 3.4 Sound

* **Menu** `sound_options_menu` 0001:01ea: text mode 3, "STREET ROD II:  Sound Options", `1 - AdLib`,
  `2 - Normal` (string table `DS:01F8`); returns 0 (AdLib) for key `1`, 1 (speaker) for `2`, 0 for
  any other key. Shown only if `adlib_detect` 2c57:1733 finds a chip (timer 1/2 status test on the
  base port `DS:7830`, set to 0x388 by `adlib_set_port` 2514); no AdLib -> speaker without asking.
* **Device variable** `DS:0050`: **0 = AdLib, 1 = PC speaker** (read by `timer_isr`,
  `race_isr_enter/leave`, `music_*`, `title_and_setup`). `DS:004E`: 0 off, 1 initialised, 2 = MUS song
  playing. `DS:3BD6` = sound gate on. **There is no Tandy sound**: the only ports are 0x42/0x43/0x61
  and the AdLib port; Tandy exists only as a video driver.
* **Dispatch**: both init routines copy 25 far pointers (0x32 words) into the vector table `DS:BB8C`:
  `speaker_sound_init` 2c57:05c8 copies `DS:530A`, `adlib_sound_init` 2c57:13d8 copies `DS:73C2`
  (then sets `DS:0050`, `DS:004E`=1, calibrates the delay / loads `MSR.SND`). Game code calls
  `lcall [BB8C+4k]`. Slots 0-22 are effects / songs (speaker 0608..09a0 are tone / PWM routines, AdLib
  0c88..13cc play instruments `motor turn turn1 hitcar crash police piano1 part paint`); slot 22 =
  `00f6` / `13c4` (music mute), 23 = silence (`spk_vec_off` 05ac / `adlib_vec_off` 21b1), 24 = resume
  (`spk_vec_on` 05ba / `adlib_vec_on` 21f0), called from both timer ISRs (`BBE8`, `BBEC`).
* **AdLib driver** (2c57): `adlib_write(reg,val)` 16e1 (index port, delay `DS:788C`=3, data port, delay
  `DS:788E`=12), `adlib_reset` 24ad, voice off 1fd6, note / volume 2024, timbre 20cf / 2128, note 1c6c,
  `snd_bank_load` 14aa (message "Timbre not found: %d"). **MUS player** `mus_tick` 2875 interprets
  the MIDI-like stream: status 0x80-0xEF with running status (`DS:9A7E`), `F0 7F 00 b1 b2 F7` tempo,
  `F8` = timing byte (delay `DS:B19E` ticks), `FC` = end of song / next in list `DS:9AE6`.
  `mus_play` 26d9(song record, stream far ptr), `mus_stop` 25aa, `mus_resume` 25f0, `mus_set_tempo` 2542.
* **Timer**: `platform_install` programs PIT ch0 with divisor 0x4000 (**72.8 Hz**, as SR1) and hooks INT 8
  = `timer_isr` 0001:2246 (`race_isr` 0001:2622 while driving). With AdLib music, `adlib_music_init` 26b9
  calls `mus_timer_install` 2f1b:0069, which takes over INT 8 with **`mus_timer_isr` 2f1b:00cb**.
  `mus_timer_set_divisor` 2f1b:0045 sets the PIT divisor `cs:[8]` = 1193180 / max(19, ppqn*tempo/60)
  (`mus_set_tempo` 2542, song header `+0x3c`, `+0x24`), i.e. the **music tick rate is the song's own
  rate** (idle: 0x4000). The ISR subtracts the divisor from a counter (`cs:[0xC]`, start 0x4000); when it
  is <= 0 it reloads and **chains to the game ISR** (`cs:[0x12]` = `timer_isr` / `race_isr`: the game
  still ticks at 72.8 Hz and the BIOS gets its 18.2 Hz from it), else it sends EOI itself. Every
  `cs:[0x24]` interrupts it calls `mus_tick` on a private stack (`DS:782C`) when `DS:788A` != 0.
  `race_isr_enter/leave` (20a0 / 2176) swap the chained vector (`cs:[12]`) instead of INT 8 when
  `DS:0050 == 0`; with the speaker they call `_dos_setvect(8, ...)`.

### 3.5 Copy protection

SR1's juke-box colour question was never called in SR.EXE. SR2: `copy_protection` **0001:3378** (SR1
`0000:2fc8`, rewritten), called from `title_and_setup` 3cc4 after the title sequence unless the `demo`
switch (`DS:B8D0`) is set; a 0 result would call `platform_exit`. It is a **manual lookup**: `rnd(24)`
picks an entry of the table at `DS:4878` (4-byte entries {n, page, line, expected letter}) and prompts
"What`is`the`%d  `letter`in`line`%d`on`page`%d?" (format `DS:4847`; ordinal suffix st / nd / th
patched into `DS:2341`) with `ui_edit` 610c. **The typed letter is never compared with the expected
one**: the function always returns 1, so there is nothing to bypass. Faithful port: show the prompt,
ignore the answer (the `demo` switch skips it).

### 3.6 Command line (`main`, strings at `DS:0212`)

| Arg | Effect |
|---|---|
| `1` | `lib_open_all(1, ...)` (default 2; used for the one / two floppy-drive logic) |
| `nomouse` | `DS:020E = 0` (mouse off; SR1 `DS:0262`) |
| `demo` | `DS:B8D0 = 1` (SR1 `sw_demo`; also skips the protection) |
| `nouemem` | `DS:0210 = 1` (read by `pic_park_list`) |
| `e` `c` `v` `h` `t` | force video mode 2 EGA / 1 CGA / 3 VGA / 4 Hercules / 5 Tandy (`video_detect_menu(mode)`) |

SR1's `auto` switch (`DS:0612`) no longer exists in `main`.

### 3.7 Interrupt handlers, far vector tables (now seeds in `tools/sr2index.py`)

| What | Where |
|---|---|
| INT 8 timer | `0001:2246` `timer_isr`, set by `platform_install` 19e8 (old vector `DS:A2AC/A2AE`; PIT 0x4000); `0001:2622` `race_isr`; `2f1b:00cb` music ISR (3.4) |
| INT 9 keyboard | `0001:3014` `kbd_isr`, set by `kbd_install` 3175 (old vector `DS:3BDC/3BDE`), `kbd_restore` 3194; `kbd_inject_scancode` 3245 |
| INT 24h | `0001:191a` `crit_err_handler` via `_harderr` (249e) in 19e8 |
| other | only the MSC startup vectors in `34b0` (`mov ax,35xx` at image 34c10, 34d6a) |
| Graphics vector tables | **20 far pointers** each (SR1: 15), copied to **`DS:A0AE`** (`rep movsw`, 0x28 words): `DS:7A40` CGA/Hercules (`2719:01a6`), `DS:7ADC` Tandy (`2f32:0906`), `DS:7B38` EGA/VGA (`2f32:111c`); slot 4 = `26c0:00c1 / 0002 / 0175`, slot 18 = `3422:025d / 3458:0228 / 33f8:01ca` |
| Sound vector tables | `DS:530A` (speaker) and `DS:73C2` (AdLib), 25 far pointers, copied to `DS:BB8C` (3.4) |
| Far immediates | `3345:000c` rows_mask_or, `33e5:0002` rows_mask_blend (SR1 `2595:0004`, `2634:000a`); unreferenced AdLib stubs `2c57:1072`, `10b6` |

Effect of these seeds: 863 -> 913 functions, coverage 84 % -> 85 % of the code bytes (96 % of the real
code). Matcher: same 56 / changed 435 / new 372, 491 of 769 SR1 functions found, 423 named -> same 56 /
changed 442 / new 415, 498 found, 428 named (`copy_protection`, `kbd_isr`, `kbd_inject_scancode` are now
found). `port/symbols.csv` is regenerated from the matches (`tools/sr2_symbols_from_matches.py`) plus 42
hand-named new functions (`sound_options_menu`, `adlib_*`, `mus_*`, `shp_*`, `event15_run`,
`end_data_load`, ...).
