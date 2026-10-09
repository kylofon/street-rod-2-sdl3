# Sound - delta spec (S3.2): AdLib driver + `.MUS` sequencer, sound options menu, speaker deltas

Scope: segment `2c57` (133 functions: speaker effects, two 25-slot vector sets, AdLib driver, `.SND`/`.MUS`
loaders, MUS interpreter `mus_tick` 2875), segment `2f1b` (music timer, 7 functions, ISR `mus_timer_isr` 00cb),
and in segment `0001` the sound-owned functions `sound_options_menu` 01ea, `music_stop` 1728,
`music_resume` 1784, `tune_start` 2002, `tune_rewind` 208a, `race_isr_enter` 20a0, 2164, `race_isr_leave` 2176.
Addresses as in `port/RE_GUIDE.md` (`SSSS:OOOO` = file segment, `DS:xxxx` = DGROUP 3E08). Confidence: every
statement was read in the disassembly (`x86dis`), the Ghidra decompile was used as cross-check only.

Counts (functions specified in `sound_symbols.csv`): 148 = 1 `same` + 21 `changed` + 126 `new` (SR2 index classes). By treatment:
26 are one-line references to SR1 code (recompiled / identical), 9 are "difference only" sections (changed vs SR1), 113 are full
specs (all AdLib driver, loaders, timer, slot routines, menu; dead leftovers get a one-liner). 11 functions of the neighbour-vote list
are handed off (section 8); `0001:2002` (`tune_start`, voted `game_flow`) is claimed here because it is SR1 `music_start`.

SR1 spec this builds on: `../StreetRod/port/spec/sound.md` (PC speaker only; sections quoted as "SR1 sx.y").
SR1 had **no AdLib code at all**; everything about the OPL2 driver is new. SR1's speaker code is reused.

Reference tool: `tools/sr2snd.py` (Ad Lib Inc. toolkit player). Section 7 lists every difference from the game.

---

## 0. What changed vs SR1 (summary)

1. **Two sound devices** selected at start-up. `main` calls `adlib_detect` 1733; with no chip the speaker is used
   silently, else `sound_options_menu` 01ea (text screen, key `1` AdLib / `2` Normal, any other key = AdLib).
   `DS:0050` = device (0 AdLib, 1 speaker, initial 666 = "not chosen yet"), `DS:004E` = 0 off, 1 initialised,
   2 = a `.MUS` song has been started (never goes back to 1). **No Tandy/PCjr sound exists.**
2. **25-slot dispatch table** `DS:BB8C` (25 far pointers, 100 bytes), filled by `speaker_sound_init` 05c8 from
   `DS:530A` or by `adlib_sound_init` 13d8 from `DS:73C2`. The game never calls an effect directly any more
   (exceptions: end of section 1.2); every game event is `(*DS:BB8C[4*slot])(arg)`. SR1's direct calls (`fx_click`,
   `loop_crank`, `siren_set`...) became slots. Both device tables have the same slot meaning (section 1.2).
3. **Speaker background music / loop sounds** are now one table-driven sequencer: `song_select` 2c57:0002
   (rewritten) + `tune_start` 0001:2002 + the sound part of `timer_isr` 0001:2246. Songs live in DS (near
   pointers, `DS:5490..6E48`), loop counts are *total plays* (SR1: extra plays), the section counter restarts at 1,
   there is a "hold" flag, an optional engine-note override, a rewritten vibrato amount. Details: sections 3
   (0002), 6.2.
4. **AdLib driver** (2c57:14aa..2837): OPL2 register access through port `DS:7830` (0x388), 18 operator slots,
   11 voices (9 melodic or 6 melodic + 5 percussion), pitch-bend with 25 steps per semitone, timbre bank `MSR.SND`,
   songs `*.MUS`, interpreter `mus_tick` driven by its own PIT-0 interrupt `mus_timer_isr` 2f1b:00cb.
5. **The game timer is no longer a plain 72.8 Hz IRQ0 when AdLib music plays** (section 1.4): the music ISR owns
   INT 8, runs at the song's tick rate and *chains* the game ISR when an accumulator expires. The chained rate is
   `R / ceil(0x4000 / D)` (R = 1193180 / D music rate) = 68.6 Hz while tempo-120 songs play, 60 Hz at tempo 60.
6. Speaker busy-loop effects (`fx_*`) are unchanged but moved to 2c57 and bracketed by slot 23/24 gate routines.

---

## 1. Architecture

### 1.1 Start-up sequence (`main` 0001:07b0, sound-relevant calls in order)

```
video_detect_menu();                         // other module
if (adlib_detect() == 0) choice = 1;         // 2c57:1733, no chip -> speaker, no question
else                     choice = sound_options_menu();      // 0001:01ea: 0 = AdLib, 1 = speaker
mem_pools_init(); driver_select(); ... gfx_init(); arena_init();
platform_install();                          // 0001:19e8 (other module, sound parts in section 5.1)
if (choice == 1) speaker_sound_init(); else adlib_sound_init();     // 2c57:05c8 / 2c57:13d8
lib_open_all(); ... lib_read_dir(1); ... pic_load_list(...);
if (pic_load_list(...) == 3) { lib_read_dir(2); if (DS:0050 == 0) mus_songs_load();  /* 2c57:0ade */ ... }
```
`title_and_setup` 0001:3cc4 also calls `mus_songs_load` (AdLib only, when its lib_read_dir(2) branch runs) and slot 18
(start music) before the title sequence. `platform_exit` 0001:18ce calls slot 21 when `DS:004E != 0`.

### 1.2 Slot table (25 slots, `DS:BB8C + 4*slot`)

All effect routines take one `int on` argument (cdecl, far; 1 = start, 0 = stop) except where "no arg". `[3bd5]` =
sound enabled (Ctrl key), `[3bd6]` = device gate/"effects live" (speaker: port 61h gate mirror; AdLib: effects
enabled flag). Speaker loop sounds are `song_select(song, loop_tbl, vib_tbl)` calls (data in section 4.2).

| slot | speaker routine (DS:530A) | AdLib routine (DS:73C2) | AdLib voice / timbre / vol / note | triggered by (callers) |
|---|---|---|---|---|
| 0 | `0608` loop "putt" (song 5544: 33 Hz pulses every 9 ticks) | `0c88` | v5 `part`(preloaded) vol 38h note 0Eh | `tc_wheel_swap` 1a21:b358 around each wheel-swap animation (also `[1F7C]=8`); resumed by `adlib_vec_on` when `[1F7C]==8` |
| 1 | `0634` no-op (`retf`) | `0cd8` tyre noise: sets `DS:72EE`=arg; v5 vol 60h note 78h / voice off | v5 | AdLib only: `race_isr` (squeal pulse train), `0001:8d9e` race wrapper calls `slot1(0)` at race end |
| 2 | `0662` loop (5534: 150 Hz buzz, vib rec 10) | `0dcc` | v5 vol 6Ah note 5Ah | `paint_job` 0001:6d5e: on when the spray starts, off when it ends |
| 3 | `0636` loop (5524: 100 Hz, vib rec 6) | `0d2c` | v4 vol 68h note 4Fh | **no caller** |
| 4 | `068e` loop (5490: 4 blips 130-155 Hz) | `0d7c` | v4 vol 6Ah note 54h | `FUN_2925_20be` (mode 1 on + `[1F7C]=4`, mode 3 off); `adlib_vec_on` when `[1F7C]==4` |
| 5 | `0712` loop (5570: 8 kHz hiss) | `0edc` | v4 vol 40h note 55h | `change_transmission` 0001:75b8, `engine_bay` 0001:80a8: bracket the bolt-remove/draw steps |
| 6 | `06e6` loop (5500: 8 kHz / 100 Hz alternation) | `0f2c` | v5 vol 4Ah note 1Ch | `tire_change_anim` 1a21:b9bc (first animation, `[1F7C]=0x10`); `adlib_vec_on` when `[1F7C]==0x10` |
| 7 | `06ba` loop (54bc: low clicks) | `0e1c` | v3 `turn1` vol 7Fh note 0Eh | `tire_change_anim` (second/third animation, on/off pairs) |
| 8 | `0836` `fx_clank` (noise 200,10,60) | `0e6c` (no arg) | v4 `hitcar` vol 7Fh note 28h | `engine_blown` 0001:b46e, `trans_blown` 0001:b4da |
| 9 | `085c` `fx_clank` again | `0ea4` (no arg) | v3 `hitcar` vol 70h note 2Eh | `customize` 0001:6edc, `trans_remove_anim` 1a21:322e, `bay_draw_parts` 1a21:3c14 |
| 10 | `073e` loop (54dc: 7 kHz / 30 Hz "wrench") | `0f7c` | v4 vol 5Ah note 52h | `change_transmission`, `engine_bay` (part slide in/out), `car_draw` 1a21:89fa (animation) |
| 11 | `0796` `siren_set` (`[72E2]=on!=0`) | `0fcc` police siren | v2 `police` vol 7Fh, bend range 5, note 50h | `FUN_0001_d908` (on when police state `AB2C` 2->3, off at 12908), `race_results` 0001:b584, `0001:8d9e` (off), `adlib_vec_on` (off) |
| 12 | `07a6` `fx_click` | `10fa` | v3 `piano1` vol 7Fh note 5Ch, voice off | text-edit reject beep: `1a21:5d5e` (illegal key in numeric field), `1a21:5fb8` (Enter on empty field) |
| 13 | `07d2` `fx_chirp_hi` | `113e` | v3 `turn` vol 75h note 61h+rand()%8, voice off | `change_transmission` x3, `engine_bay` x3 (after each bolt step) |
| 14 | `080a` `fx_chirp_lo` | `118e` | v3 `turn` vol 75h note 3Dh | `change_transmission`, `engine_bay` x2, `tc_wheel_swap` (end) |
| 15 | `0882` `fx_crash` | `11c6` | v4 `hitcar` 7Fh/14h + v5 `crash` 7Fh/19h | `crash_show` 0001:af0a (wreck) |
| 16 | `0930` `fx_hit` | `1246` | v4 `hitcar` 7Fh/14h + v3 `turn` 7Fh/3Dh+rand()%8, then `engine_on(1)` | `FUN_0001_d908` x4 (race collisions), `FUN_0dcb_148a` x5 (obstacle/edge hit) |
| 17 | `09a0` `fx_thud` | `12e0` | v3 `turn1` vol 75h note 10h | `change_transmission`, `engine_bay`, `trans_bolt_remove_anim` 1a21:2fc4, `bay_bolt_remove_anim` 1a21:369a |
| 18 | `076a` background tune (`song_select(5594,540C,5432)`) | `1318` play song list A | MUS list `DS:73A0` (1,1,5,2,5,3,3,loop) | `title_and_setup` 0001:3cc4 (before the title), `FUN_0001_a51c` (after the ending screen) |
| 19 | `0780` jingle (`song_select(6E48,542A,5468)`, 3 plays) | `1356` play song list B | MUS list `DS:73B0` (4,4,stop) | `FUN_0001_a51c` (ending/epilogue picture 0x43B) |
| 20 | `01a2` stop (clear hold + `tune_rewind`) | `13c6` `mus_stop` | - | no caller |
| 21 | `01a2` same as 20 | `13cc` `mus_stop` + `mus_timer_remove` | - | `platform_exit` 0001:18ce (if `[4E]!=0`) |
| 22 | `00f6` `music_mute(on)` | `13c4` `retf` | - | `tire_change_anim`: `(1)` after the first animation, `(0)` at the end; preceded by `music_stop()` |
| 23 | `05ac` `spk_vec_off`: port61 &= FCh, `[3bd6]=0` | `21b1` `adlib_vec_off` | - | `timer_isr`, `race_isr` (every tick when `3bd5==0 && 3bd6!=0`), `music_stop`, `tune_rewind`, `race_isr_leave`, `song_select`, `fx_noise`, tune screen |
| 24 | `05ba` `spk_vec_on`: port61 \|= 3, `[3bd6]=1` | `21f0` `adlib_vec_on` | - | `timer_isr`, `race_isr` (when `3bd6 != 3bd5`), tune screen |

Songs (AdLib, files read by `mus_file_load`, slot = descriptor index): 1 = `MSRBEG.MUS`, 2 = `MSR2.MUS`,
3 = `MSR24.MUS`, 4 = `MSR21.MUS`, 5 = `PAUSE.MUS`. `MSR20.MUS` exists in `Game/` but is never loaded.
Direct (non-slot) calls from game code: `adlib_engine_on/set` 0b2c/0ba6, `adlib_engine2_on/set` 0bd2/0c5c,
`adlib_siren_bend` 1050, `adlib_garage_timbres_load` 1394, `adlib_fx_tyre` 0cd8 (= slot 1) from `race_isr`, the tune
screen and `race_isr_enter/leave` (section 5).

Callers verified by a byte scan of the whole image for `ff 1e xx bb` (calls through `[BB8C+4k]`) and `9a xx xx 57 2c`.
Slot 12 is a *reject beep* in text fields, not the SR1 button click.

### 1.3 `DS:0050` / `DS:004E` / gate variables

| var | meaning |
|---|---|
| `DS:0050` | 0 AdLib, 1 PC speaker, 666 before the choice. Read by `timer_isr`, `race_isr`, `race_isr_enter/leave`, `music_stop/resume`, `tune screen`, `tire_change_anim` |
| `DS:004E` | 0 no sound, 1 initialised, 2 `.MUS` song started at least once (set by `mus_play`) |
| `DS:3BD5` | master "sound on" (init 1; toggled by the Ctrl key in `kbd_isr`; SR1 `47CE`) |
| `DS:3BD6` | gate (init 0). Speaker: mirror of port 61h bits 0-1 (SR1 `47CF`). AdLib: 1 = effects live; every AdLib effect starts with `if (DS:3BD6 == 0) return;` |

### 1.4 Music timer (`mus_timer_isr`, PIT channel 0) - **important for the port**

State lives in the code segment `2f1b` (offsets `cs:xx`); `cs:[1E]` holds the game DS:

| cs: | meaning | set by |
|---|---|---|
| `[06]` | flag: divisor was 0 | `mus_timer_set_divisor`, `_install` (never read) |
| `[08]` | PIT divisor D of channel 0 (initial 4000h) | `mus_timer_set_divisor`, `_install` |
| `[0A]` | 4000h (never read) | `_install` |
| `[0C]` | signed accumulator, initial 4000h | ISR |
| `[0E]` | far ptr: original BIOS INT 8 | `platform_install` (`0001:1b30`) |
| `[12]` | far ptr: **chained game ISR** (`timer_isr` or `race_isr`) | `platform_install`, `race_isr_enter/leave` |
| `[16]` | far ptr `timer_isr` 0001:2246; `[1A]` far ptr `race_isr` 0001:2622 | `platform_install` |
| `[1E]` | game DS (3E08) | `_install` |
| `[20]`,`[22]` | saved SS:SP | ISR |
| `[24]` | music countdown (interrupts until the next `mus_tick`) | `mus_set_countdown`, ISR |
| `[26]` | re-entrancy guard | ISR |

ISR (`2f1b:00cb`, entered from INT 8):
```
cs:[0C] -= cs:[08];
if (cs:[0C] > 0 (signed)) { outb(0x20, 0x20); }                    // EOI, music part only
else { cs:[0C] = 0x4000; pushf; call far cs:[12]; }                // game ISR (does its own EOI, iret)
if (DS:788A != 0) {                                                  // song playing
    if (--cs:[24] == 0 && cs:[26] == 0) {
        save regs; SS:SP -> private stack at DS:782C (top); cs:[26]++; sti;
        n = mus_tick();                                              // returns ticks to wait (>=1, 0 after stop)
        cli; cs:[26]--;
        e = -(s16)cs:[24];                                           // interrupts that arrived meanwhile (cs:[24] went negative)
        cs:[24] = (e < n) ? cs:[24] + n : n;                         // = n - e, or n if e >= n (unsigned compare)
        restore; 
    }
}
iret
```
In a single-threaded port `e` is always 0: `cs:[24] = n` after each `mus_tick`.

Programming the rate: `mus_set_tempo(tempo, tick_beat)` 2c57:2542: `rate = (u32)tick_beat * tempo / 60` (integer); if `rate == 0`
divisor = 0 (PIT 65536) else `rate = max(rate, 19); D = 0x1234DC / rate`; then `mus_timer_set_divisor(D)`
(PIT ch0 mode 3, `out 43h <- 36h`, `out 40h <- lo, hi`). Idle value (no song / after `mus_stop`) is D = 4000h.

**Consequence (faithful behaviour):** the game ISR is chained when the accumulator (reloaded to exactly 4000h, remainder
discarded) drops to <= 0, i.e. after `k = ceil(16384 / D)` music interrupts. Shipped songs have `tick_beat = 240`:
tempo 120 -> rate 480 -> D = 2485 -> k = 7 -> game tick = 1193180/2485/7 = **68.6 Hz** (instead of 72.82 Hz);
tempo 60 -> D = 4971 -> k = 4 -> **60.0 Hz**. `timer_isr` still divides the chained rate by 4 for the BIOS tick (18.2 Hz
nominal). Everything driven by the game tick (animations, `wait_ticks`, race physics divider...) therefore runs 6 %
slower (tempo 120) or 18 % slower (tempo 60) while AdLib music plays. A port that keeps a fixed 72.8 Hz tick is not
faithful here (open question 1). While music is paused/stopped the rate is 72.82 Hz again (D = 4000h, k = 1).

### 1.5 SR1 -> SR2 DS remap (speaker path)

| SR1 DS | SR2 DS | name |
|---|---|---|
| 47CE / 47CF | 3BD5 / 3BD6 | snd_enabled / gate |
| 58D0 / 58D2 / 58D4 | 5304 / 52FE / 5300 | music_enabled / music_cur (state ptr) / music_mode |
| 58D6 / 58D8 | 5306 / 5308 | music_enabled_saved / music_muted |
| 58DA / 58DC | 72E0 / 72E2 | squeal_on / siren_on |
| 5E0E (off_road) | 7BA2 | off-road buzz flag |
| 58E2 | 742A | delay scale |
| 5826 / 57F6 | 539E / 536E | vib records (now 12 x 5 words) / vib waves |
| 5858 / 5882 | 540C / 5432 | bg loop counts / bg vib index |
| 587E / 58CC | 542E / 5470..548E | fx loop counts / fx vib index tables |
| 72EC / 82C4 / 06E2 / 6C52 / 6C54 | 9A3A / ADD2 / 2002 / B870 / 8822 | vib enable / vib rec ptr / countdown / amount / last div |
| 79AA / 79C6 (SongState 1Ch) | 9DEC / 9DAA (state 18h, near pointers) | bg_song / fx_song |
| 06E4 / 06EA / 0608 | 2004 / 200A / 1F76 | siren_count / squeal_phase / tick_div4 |
| 78EE / 0286,5A0E / 825E,8B7C | A10E / 1BCA / AD5C,B866 | rpm / animation flag / animation values |
| 09F8 button table (label +0, id +2) | pointer at DS:3AD8, record 12h bytes, id at +6, label ptr at +2 | jukebox button -40 |
| `2BA9/2BAB` label offsets | direct pointers `DS:22FA` "Squelch it !", `DS:2308` "Catch some tunes" | labels |

---

## 2. Globals

DGROUP globals owned by the module (`type`: u8/u16/s16, `[n]` array, `far*` = far pointer). Speaker sequencer state is
`SongState18` = `{u16 note, u16 dur, u16 rep, u16 playing, u8 regate(+8), u8 pad, u16 base, u16 sec_start, u16 cur,
u16 loop_tbl, u16 loop_ptr, u16 vib_tbl, u16 vib_ptr}` (offsets 00,02,04,06,08,0A,0C,0E,10,12,14,16; near DS pointers).

| DS | type | name | meaning (SR1 equivalent) |
|---|---|---|---|
| 004E | u16 | snd_state | 0/1/2, see 1.3 |
| 0050 | u16 | snd_device | 0 AdLib, 1 speaker, init 666 |
| 01AE,01B8 | str | menu_item_strs | "1 - AdLib", "2 - Normal"; table of 2 near ptrs at DS:01F8 |
| 01C3 | str | menu_title | "STREET ROD II:  Sound Options"; DS:01E4 "Recommended mode: #" |
| 2002 | u16 | vib_countdown | speaker vibrato countdown (SR1 06E2) |
| 2004 | u16 | siren_count | speaker driving siren counter (SR1 06E4) |
| 200A | u8 | squeal_phase | toggled every tick of the siren/squeal phase (SR1 06EA) |
| 22FA,2308 | str | lbl_squelch, lbl_tunes | jukebox button labels |
| 3BD5 | u8 | snd_enabled | master sound switch (SR1 47CE) |
| 3BD6 | u8 | snd_gate | gate / effects-live (SR1 47CF) |
| 52FE | u16 | music_cur | near ptr to the active `SongState18` (SR1 58D2) |
| 5300 | u16 | music_mode | 1 background tune, 2 loop sound/tune screen (SR1 58D4) |
| 5302 | u16 | music_hold | NEW: while nonzero the duration counter is frozen (see 6.2) |
| 5304 | u16 | music_enabled | music switch, init 1 (SR1 58D0) |
| 5306 | s16 | music_enabled_saved | -1 = none (SR1 58D6) |
| 5308 | u16 | music_muted | (SR1 58D8) |
| 530A | far*[25] | spk_vec_init | speaker slot table (copied to BB8C) |
| 536E | s16[3][8] | vib_wave | rows `0 1 2 1 0 -1 -2 -1`, `0..7`, `0 1 0 0 0 0 0 0` (SR1 57F6) |
| 539E | s16[12][5] | vib_rec | `{phase,row,depth,period,step}`: see 4.3 (SR1 5826) |
| 540C | s16[15] | bg_loop_tbl | total plays per section, -1 end (SR1 5858, values +1) |
| 542A | s16[2] | jingle_loop_tbl | `{3,-1}` |
| 542E | s16[2] | fx_loop_tbl | `{1,-1}` (SR1 587E was `{0,-1}`) |
| 5432 | s16[27] | bg_vib_tbl | vibrato record index per section play, -1 end (SR1 5882) |
| 5468..548E | s16[2..] | fx_vib_tbls | `5468 {1,2,3,-1}`, `5470 {0,-1}`, `5474 {0,-1}`, `5478 {5,-1}`, `547C {6,-1}`, `5480 {7,-1}`, `5484 {8,-1}`, `5488 {9,-1}`, `548C {10,-1}` |
| 5490..5593 | u16[] | spk_loop_songs | speaker loop songs, 4.2 |
| 5594..6E47 | u16[] | spk_bg_song | background tune (header `0,0` then `(dur,div)` pairs) |
| 6E48..72DF | u16[] | spk_jingle_song | jingle, one section of 291 events, played 3 times |
| 72E0 | u16 | squeal_on | tyre squeal flag, hysteresis written by `FUN_0001_d908` (on when abs(A104)>11, off when <10) |
| 72E2 | u16 | siren_on | siren flag (SR1 58DC) |
| 72EC | u16 | adlib_vol_enable | init 1, never written; 0 would mute every level register (`1ae2`) |
| 72EE | u16 | tyre_noise_on | AdLib tyre noise currently keyed |
| 72F0 | str | name_msr_snd | "msr.snd"; 72F8 "msrbeg.mus"; 7304 " :msr2.mus"; 7310 " :msr24.mus"; 731C " :msr21.mus"; 7328 " :pause.mus" (first char = drive letter patched by `lib_open_all`) |
| 7334..73BC | str | timbre_names | "motor"7334 "motor"733A "paint"7340 "turn1"7346 "hitcar"734C "hitcar"7353 "police"735A "piano1"7361,7368,736F "turn"7376 "turn"737B "hitcar"7380 "crash"7387 "hitcar"738D "turn"7394 "turn1"7399 "paint"73B6 "part"73BC |
| 73A0 | s16[8] | song_list_a | `1,1,5,2,5,3,3,-1` |
| 73B0 | s16[3] | song_list_b | `4,4,-2` |
| 73C2 | far*[25] | adlib_vec_init | AdLib slot table (copied to BB8C) |
| 7426 | u16 | pulse3_ptr | dead (`pulse3_play`) |
| 7428 | u16 | pulse3_delay | dead, init 1 |
| 742A | u16 | delay_scale | `delay()` scale from calibration (init 100h) |
| 782C | u8[11] | perc_mask | indexed by voice 6..10: `10 08 04 02 01`; DS:782C is also the top of the private ISR stack (`mov sp,782C`) |
| 7830 | u16 | opl_port | 0388h |
| 7838 | u8[28] | voice_slot_tab | per voice `{mod slot, car slot}`: `00 03, 01 04, 02 05, 06 09, 07 0A, 08 0B, 0C 0F, 0D 10, 0E 11`, then `0C 0F` (v6 BD), single slots at 783E+2*voice: v7 `10`, v8 `0E`, v9 `11`, v10 `0D` |
| 7854 | u8[18] | slot_reg_off | operator register offsets: `00 01 02 03 04 05 08 09 0A 0B 0C 0D 10 11 12 13 14 15` |
| 7866 | u8[18] | slot_is_carrier | `00 00 00 01 01 01` x3 |
| 7878 | u8[18] | slot_voice | `00 01 02 00 01 02 03 04 05 03 04 05 06 07 08 06 07 08` |
| 788A | u8 | mus_playing | song running |
| 788C,788E | u16 | io_delay_index,io_delay_data | 3 and 12 (empty-loop iteration counts after the OPL index/data write) |
| 7890 | u16[11] | voice_vol_cache | last volume given to `set_volume(...,1)`; written, never read |
| 78AA | s32 | bend_cache_v | last `v` of `adlib_bend_calc`, init 0000FFFFh |
| 78AE,78AF | str | empty_name, fmt_timbre_nf | "" ; "\nTimbre not found: %d" |
| 9162 | u8 | am_depth | OPL reg BD bit 7 (init 0) |
| 9163 | u8 | perc_mode | 1 = rhythm mode (6 melodic + 5 percussion voices); default 1 after reset |
| 9164 | s16[11] | half_tab | per-voice semitone offset from pitch bend |
| 917A | u8 | note_sel | OPL reg 08 bit 6 |
| 917C | s16 | bend_step | `bend_range * 25` |
| 917E | u8[96] | note_in_oct | `i % 12` |
| 91DE | u16 | wave_sel_en | 20h = waveform-select enabled (reg 01h value) |
| 91E0 | u8[96] | oct_tab | `i / 12` |
| 9240 | u8 | vib_depth | OPL reg BD bit 6 (init 0) |
| 9242 | u16[11] | row_ptr | per-voice pointer into the fnum table (`9362 + 24*fptr`) |
| 9258 | u8 | drum_bits | OPL reg BD bits 0-4 |
| 925A | u8[11] | last_note | raw note last given to `adlib_set_freq` |
| 9266 | u8[18][14] | slot_param | per operator: ksl, multi, feedback, attack, sustain, eg_sust, decay, release, level, am, vib, ksr, fm, wave |
| 9362 | u16[25][12] | fnum_tab | built by `adlib_tables_init` |
| 95BA | u16 | bend_range | 1..12 |
| 95BC | u8[11] | key_flag | last key-on flag |
| 95C8 | u8[18] | rel_vol | per operator relative volume 0..7F (init 7F) |
| 95DA,95DC | u16 | bend_cache_row,bend_cache_half | cached results for `bend_cache_v` |
| 986E | far* | mus_ptr | MUS read pointer (4 bytes at 986E/9870) |
| 9A3A | u16 | vib_enabled | speaker vibrato enable (SR1 72EC) |
| 9A7E | u8 | mus_running_status | |
| 9AE4,9AE6,9AE8 | u16 | list_start,list_cur,list_single | MUS playlist (list_single never nonzero) |
| 9B08 | u16 | engine_vib_amount | dead engine-override helper |
| 9BE8 | u16[6] | mus_size_words | low word of data size per song slot |
| 9DAA | SongState18 | fx_song | loop sounds/tunes (SR1 79C6) |
| 9DEC | SongState18 | bg_song | background tune (SR1 79AA) |
| A7CE | u32 | mus_elapsed | summed delays, never read |
| A7DC | u8[11] | vel_cache | last note-on velocity written per voice |
| A9C4 | u16 | engine_note_div | dead override value |
| AB2A | near* | bank_block | `MSR.SND` control block |
| ADD2 | u16 | vib_rec_ptr | current vibrato record (SR1 82C4) |
| B192 | u16 | mus_desc | near ptr to the current 70-byte song descriptor |
| B19E | u16 | mus_delay | current delay |
| B86E | u16 | bank_handle | bank block used by `adlib_play_timbre`/`mus_tick` |
| B870 | u16 | vib_amount | speaker vibrato amount (SR1 6C52) |
| B8EA | u8[6][70] | mus_desc_tab | song header per slot 0..5 (slot k at B8EA+46h*k) |
| BA94 | far*[6] | mus_data_tab | song data pointer per slot |
| BB8C | far*[25] | snd_vec | the active slot table |

---

## 3. Functions

Conventions: "far" = far call (`retf`), "near" = `ret`; arguments cdecl, first argument at `[bp+6]` (far) / `[bp+4]`
(near). `rng(n)` = `1a21:6180` (game RNG, = SR1 `0f38:5eb6`: LCG seed `DS:4A84`, `n<=0` reseeds). `delay(n)` = 2c57:1406.
`adlib_write(reg,val)` = 2c57:16e1. "slot[k]" = `(*(void far (*)(int))DS:BB8C[k])(arg)`.

### 0001:01ea sound_options_menu   (new)
`int near sound_options_menu(void)`; returns 0 = AdLib, 1 = PC speaker ("Normal"). Called by `main` only when
`adlib_detect()` returned 1. Text-mode screen (no graphics yet). Helpers: `print_line(str, indent)` = `0000:0000`
(runtime, near: builds `indent` spaces + str + `"\n\r$"` and prints it with DOS INT 21h AH=09h) and `gotoxy(col,row)` =
`0001:0090` (INT 10h AH=02h, page 0).
```
int10(AX = 0x0003);                      // 80x25 text mode, clears the screen
gotoxy(0, 8);
print_line("STREET ROD II:  Sound Options", 23);     // DS:01C3
print_line("", 0);                                   // DS:01E1
print_line("1 - AdLib", 31); print_line("2 - Normal", 31);   // string pointer table DS:01F8: {01AE, 01B8}
print_line("", 0); print_line("", 0);                // DS:01E2, DS:01E3
buf = "Recommended mode: #" /* DS:01E4, 20 bytes */ + table[0] /* "1 - AdLib" */;
print_line(buf, 23);
while (kbhit()) getch();                             // flush type-ahead (34b0:1762 / 1788)
while (!kbhit()) ;
i = getch() - '1';
return (i >= 0 && i <= 1) ? i : 0;                   // any other key = AdLib
```
Port: show the same text (or skip it; the Enhanced repo may replace it with a launcher option) and return the choice.

### 0001:1728 music_stop   (changed)
= SR1 `music_stop` (0000:0d99), SR1 s4.1, but device-aware. Finds the jukebox button record: `p = *(u16*)DS:3AD8` (pointer to
the active button table), advance `p += 0x12` until `*(s16*)(p+6) == -40` (no bound in the original: if the screen has
no button -40 it runs away; the port must bound the search and skip the label write).
```
void music_stop(void) {
    rec = find_button(-40);
    if (DS:0050 == 1) {                                  // speaker
        s = (SongState18*)DS:52FE;
        if (s->playing == 0) return;                     // nothing to stop (no label change, no click_clear)
        s->playing = 0; slot[23](); s->regate = 1;       // gate off via BBE8
        *(u16*)(rec+2) = 0x2308;                         // label "Catch some tunes"
    } else if (DS:0050 == 0) {                           // AdLib
        *(u16*)(rec+2) = 0x2308;
        if (DS:3BD6 != 0) mus_stop();                    // 2c57:25aa
    } else return;
    click_clear();                                       // 0001:1708 (handoff): [881C]=1, [B880]=-1, [B898]=32000, [3B3E]=[3B3F]=0, key_read()
}
```
SR1 differences: the label is a direct string pointer; the early `return` when not playing skips `click_clear`
(SR1 always requested a redraw); AdLib branch added. DS remap: SR1 `58D2`->`52FE`, `09F8`/`2BAB` -> button table via `DS:3AD8`/`DS:2308`.

### 0001:1784 music_resume   (changed)
= SR1 `music_resume` (0000:0dd8), SR1 s4.1, device-aware:
```
void music_resume(void) {
    rec = find_button(-40);
    if (DS:0050 == 1) {
        if (DS:5304 == 0) return;                        // music disabled
        s = (SongState18*)DS:52FE;
        if (s->playing != 0) return;
        if (s->base == s->sec_start) return;             // [+0A] == [+0C]: never started / just rewound
        s->playing = 1; s->regate = 1;
        *(u16*)(rec+2) = 0x22FA;                         // "Squelch it !"
    } else if (DS:0050 == 0) {
        *(u16*)(rec+2) = 0x22FA;
        if (DS:3BD6 != 0) mus_resume();                  // 2c57:25f0
    } else return;
    click_clear();
}
```
Note SR1 checked `music_enabled` for both; here the AdLib branch does not (callers toggle `DS:5304` first).

### 0001:2002 tune_start   (changed; voted game_flow, claimed here)
= SR1 `music_start` (0000:1a05), SR1 s4.1. `void far tune_start(int word_off, int delay)`; always called through
`song_select` as `(2, 0)`.
```
if (DS:5304 != 0) {
    s = (SongState18*)DS:52FE;
    s->playing = 0;  s->rep = 1;  s->regate = 1;
    s->cur = s->sec_start = s->base + 2*word_off;
    s->dur = delay + 1;
    s->vib_ptr = s->vib_tbl;
    DS:ADD2 = 0x539E + 10 * *(u16*)s->vib_ptr;
    DS:9A3A = 1;  s->playing = 1;
} else ((SongState18*)DS:52FE)->playing = 0;
```
Differences: `rep` starts at **1** (SR1 0, because SR2 loop tables are total play counts), no `SPK_OFF()`, near pointers
(`base+2*word_off`), `loop_ptr` still not reset (same quirk as SR1).

### 0001:208a tune_rewind   (changed)
Replaces SR1 `music_rewind` (0000:1a7c), but does **not** rewind: `s=(SongState18*)DS:52FE; s->playing = 0; slot[23](); s->regate = 1;`
(stop + gate off). Callers: `sound_stop` 2c57:01a2 (slot 20/21 speaker).

### 0001:20a0 race_isr_enter   (changed; sound parts, rest is platform)
Switches to the driving ISR. Sound part, in order (the rest sets `DS:A2B2=1`, `DS:1F7C=1`, `DS:1F78=0` first and the
mouse/input resets last):
```
if (DS:0050 == 1)      _dos_setvect(8, race_isr);                       // INT 8 = 0001:2622
else if (DS:0050 == 0) {
    *(far*)(2f1b:0x12) = *(far*)(2f1b:0x1A);                            // chained game ISR := race_isr
    if (DS:1BCA == 0) { DS:B8E0 = DS:5304; if (DS:5304 != 0) { music_stop(); DS:5304 = 0; } }   // music off while driving
    adlib_engine_on(1);                                                  // 2c57:0b2c
    adlib_engine_set(0, 0x50);                                           // bend 0, volume 50h
}
```
INT 8 stays `mus_timer_isr` for AdLib; only the chained pointer changes.

### 0001:2164 music_resume_after_race   (new)
`if (DS:B8E0 != 0) { DS:5304 = 1; music_resume(); }` (restores the music that `race_isr_enter` switched off).

### 0001:2176 race_isr_leave   (changed; sound parts)
```
DS:A2B2 = 0;
if (DS:0050 == 1) { _dos_setvect(8, timer_isr /*0001:2246*/); slot[23](); }
else if (DS:0050 == 0) {
    *(far*)(2f1b:0x12) = *(far*)(2f1b:0x16);                            // chained := timer_isr
    if (DS:1BCA == 0 && DS:1FFC == 0) music_resume_after_race();
    adlib_garage_timbres_load();                                         // 2c57:1394 (voices 4/5 get "paint"/"part" back)
    adlib_engine_on(0);                                                  // 2c57:0b2c(0)
}
DS:1F7C = 0; DS:72E2 = 0; DS:72E0 = 0; DS:7BA2 = 0;                     // siren, squeal, off-road flags cleared
... mouse reset (platform) ...
```

### 2c57:0002 song_select   (changed - rewritten)
SR1 `song_select` (0f38:6f0a), SR1 s4.2. `void far song_select(u16 song, u16 loop_tbl, u16 vib_tbl)` (near DS pointers; the
SR1 seg word is gone). Speaker only.
```
if (song == 0x5594) {                                   // back to the background tune
    if ((s16)DS:5306 != -1) { DS:5304 = DS:5306; DS:5306 = 0xFFFF; }
    if (DS:5304 == 0) slot[23]();  else bg.playing = 1;           // bg = state 9DEC, playing at 9DF2 (SR1 never touched it)
    DS:52FE = 0x9DEC;
    DS:ADD2 = 0x539E + 10 * *(u16*)bg.vib_ptr;                    // *(u16*)DS:9E02
    DS:5300 = 1;
} else {                                                          // loop sound / jingle
    if ((s16)DS:5306 == -1) { DS:5306 = DS:5304; DS:5304 = 1; }   // plays even when music is off
    DS:5300 = 2;
    fx.base = fx.sec_start = fx.cur = song;                       // DS:9DB4, 9DB6, 9DB8
    fx.loop_tbl = fx.loop_ptr = loop_tbl;                         // DS:9DBA, 9DBC
    DS:5302 = (*(u16*)loop_tbl == 0);                             // hold flag (new)
    fx.vib_tbl = fx.vib_ptr = vib_tbl;                            // DS:9DBE, 9DC0
    DS:52FE = 0x9DAA;                                             // fx_song
    tune_start(2, 0);                                             // first event on the next tick
}
DS:9A3A = 1;
```
Differences to SR1: three arguments (loop and vibrato tables are arguments, SR1 hard-coded 587E/58CC); the
background-tune branch **starts** the tune (`bg.playing = 1`) when music is enabled; `DS:5302` hold flag.

### 2c57:00b8 vib_set   (changed; dead - no callers)
`void far vib_set(int row, int period, int depth, int step)`: `DS:539E = 0 (phase); DS:53A0 = row; DS:53A2 = depth /*arg 3*/;
DS:53A4 = period /*arg 2*/; DS:53A6 = step; DS:9A3A = 1;` (SR1 `vib_set(on,row,period,depth,step)` had an `on` argument
and wrote `72EC=on`). Record 0 is also the only record whose fields are not constant.

### 2c57:00f6 music_mute   (changed - recompilation only)
= SR1 `music_mute` (0f38:7001), SR1 s4.1; DS remap 58D0->5304, 58D8->5308; calls `music_stop` 0001:1728 / `music_resume`
0001:1784. Slot 22 speaker.

### 2c57:0140 spk_set_vib_amount   (new, dead)
`DS:B870 = arg;` no callers (also not reachable through a table).

### 2c57:014c spk_set_engine_note   (new, dead)
`void far (u16 amount, u16 div)`: `DS:B870 = DS:9B08 = amount; *(u16*)(*(u16*)DS:52FE) = div; DS:A9C4 = div;`. Partner of the
"note 6000 override" in `timer_isr` (6.2); no song in the data contains a 6000 note.

### 2c57:016a spk_set_vib_step   (new, dead)
`((u16*)DS:ADD2)[4] = arg;` (vibrato record `step`).

### 2c57:017a spk_set_vib_row_period   (new, dead)
`((u16*)DS:ADD2)[1] = a /*row*/; ((u16*)DS:ADD2)[3] = b /*period*/;`

### 2c57:0194 spk_set_note_div   (new, dead)
`*(u16*)(*(u16*)DS:52FE) = arg;` (current note divisor of the active state).

### 2c57:01a2 sound_stop   (new; speaker slots 20 and 21)
`DS:5302 = 0; tune_rewind();` (stops the active song state, gate off, `regate=1`).

### 2c57:01ae snd_calibrate_delay   (changed - recompilation only)
= SR1 `snd_calibrate_delay` (0f38:703e), SR1 s4.7; DS remap `58E2` -> `742A` (set to 100h while measuring, result returned).
Called by `speaker_sound_init` with n = 10.

### 2c57:0234 spk_set_hz   (changed - recompilation only)
= SR1 `spk_set_hz` (0f38:70d5): `long hz` -> `div = (u16)(0x1234DC / hz)`, `out 42h <- lo`, `out 42h <- hi` (long divide helper `34b0:24f6`).

### 2c57:025c fx_tone   (changed - recompilation only)
= SR1 `fx_tone` (0f38:714c): `spk_set_hz(hz); delay(d)`. Args `(long hz, u16 d)`.

### 2c57:0278 fx_burst   (changed - recompilation only)
= SR1 `fx_burst` (0f38:7169): `(lo, hi, d, n)`; n tones `rng(hi-lo)+lo` of `d`; loop body runs only when `n > 0`.

### 2c57:02b8 fx_sweep_down   (changed - recompilation only)
= SR1 `fx_sweep_down` (0f38:71b2): `(from, to, step, d)`; `for (f = from; f >= to; f -= step) fx_tone(f, d);`

### 2c57:02e6 fx_warble   (changed - recompilation only)
= SR1 `fx_warble` (0f38:71e4): `(c, dev, step, d, n)`: n x (up `c-dev .. c+dev-step`, down `c+dev .. c-dev+step`).

### 2c57:0374 pwm_up   (changed - recompilation only; dead)
= SR1 `pwm_up` (0f38:7292), SR1 s4.7 "dead code". No caller in SR2 either (SR1's two callers `775a`/`778a` are gone). Not to be ported.

### 2c57:03de pwm_down   (changed - recompilation only; dead)
= SR1 `pwm_down` (0f38:7306). Dead, not to be ported.

### 2c57:0448 ramp_fill   (changed - recompilation only; dead)
= SR1 `ramp_fill` (0f38:737a). Dead, not to be ported.

### 2c57:048e pwm_swell   (changed - recompilation only; dead)
= SR1 `pwm_swell` (0f38:73ed). Dead (calls 0374/03de/0448), not to be ported.

### 2c57:0506 fx_noise   (new in the index; = SR1 fx_noise 0f38:7464)
`void far fx_noise(int range, int ormask, int n)`: `slot[23]();` (gate off, `[3bd6]=0`), then n times `port61 ^= 2; delay(rng(range) | ormask);`.
SR1 did the gate-off inline (`and 61h,FCh`, no `47CF` write); the only difference is that `[3bd6]` is now cleared.

### 2c57:0544 fx_noise_down   (new in the index; = SR1 fx_noise_down 0f38:74b0)
`(a, b, sub, step, n)`: `if (b < a) { x = a; do { fx_noise(a, a-sub, n); a -= step; x -= step; } while (b < x); }`. The call is
`fx_noise(range=a, ormask=a-sub, n)` exactly as SR1 (the `ormask` of a noise block is `a - sub`).

### 2c57:0578 fx_noise_up   (new in the index; = SR1 fx_noise_up 0f38:74fc)
`(a, b, sub, step, n)`: `if (a < b) { x = a; do { fx_noise(a, a-sub, n); a += step; x += step; } while (x < b); }`.

### 2c57:05ac spk_vec_off   (new; slot 23 speaker)
`port61 &= 0xFC; DS:3BD6 = 0;` (SR1 `spk_off` 0f38:713d, with `47CF`->`3BD6`).

### 2c57:05ba spk_vec_on   (new; slot 24 speaker)
`port61 |= 3; DS:3BD6 = 1;` Unlike SR1 `spk_on` (0f38:7122) it does **not** program the PIT to 25 kHz first.

### 2c57:05c8 speaker_sound_init   (new)
```
memcpy(DS:BB8C, DS:530A, 100);      // 0x32 words
DS:0050 = 1;
DS:742A = snd_calibrate_delay(10);
outb(0x43, 0xB6);                    // channel 2, lo/hi, mode 3
spk_set_hz(25000);
DS:004E = 1;
```
(= SR1 `snd_init` 0f38:7101 plus the table copy; SR1 had the PIT B6 write in `timer_install` too - here `platform_install` writes it again at its end.)

### Speaker loop-sound slot routines (0608 0636 0662 068e 06ba 06e6 0712 073e)
All eight have the same shape `void far (int on)`: `if (on) song_select(SONG, 0x542E, VIB); else song_select(0x5594, 0x540C, 0x5432);`

### 2c57:0608 spk_loop_putt   (new; speaker slot 0)
`SONG = 0x5544`, `VIB = 0x5474`: 4 pulses of 1 tick @ 32.7/36.7/38.9/36.7 Hz, each followed by 8 ticks of rest (idle putt-putt).

### 2c57:0636 spk_loop_hum100   (new; speaker slot 3)
`SONG = 0x5524`, `VIB = 0x547C`: 7 ticks @ 100 Hz, vibrato record 6 (SR1 `loop_hum100`); no caller.

### 2c57:0662 spk_loop_hum150   (new; speaker slot 2)
`SONG = 0x5534`, `VIB = 0x548C`: 7 ticks @ 149.6 Hz, vibrato record 10.

### 2c57:068e spk_loop_tick   (new; speaker slot 4)
`SONG = 0x5490`, `VIB = 0x5480`: blips 130.8/147/155.6/147 Hz, 1 tick + 3 ticks rest.

### 2c57:06ba spk_loop_blips   (new; speaker slot 7)
`SONG = 0x54BC`, `VIB = 0x5484`: 2-tick clicks 32.7 Hz / 49 Hz / 32.7 Hz, 3 ticks @ 41 Hz, 17 ticks rest.

### 2c57:06e6 spk_loop_pulse   (new; speaker slot 6)
`SONG = 0x5500`, `VIB = 0x548C`: 3 ticks 8008 Hz / 2 ticks 100 Hz alternation (+2 more pairs).

### 2c57:0712 spk_loop_spray   (new; speaker slot 5)
`SONG = 0x5570`, `VIB = 0x548C`: 8 kHz hiss in 3/2/4-tick groups separated by rests.

### 2c57:073e spk_loop_wrench   (new; speaker slot 10)
`SONG = 0x54DC`, `VIB = 0x5488`: 7 kHz / 30 Hz alternation (SR1 `loop_wrench`).

### 2c57:0634 spk_slot_nop   (new; speaker slot 1)
`retf`.

### 2c57:076a spk_bg_tune   (new; speaker slot 18)
`song_select(0x5594, 0x540C, 0x5432)` (same call as the "off" branches above).

### 2c57:0780 spk_jingle_tune   (new; speaker slot 19)
`song_select(0x6E48, 0x542A, 0x5468)`: one section of 291 events (2515 ticks) played 3 times (`542A = {3,-1}`), vibrato records 1,2,3.

### 2c57:0796 siren_set   (new in the index; = SR1 siren_set 0f38:7728)
`DS:72E2 = (arg != 0);` (SR1 `58DC`). Slot 11 speaker; the AdLib partner is `0fcc`.

### Speaker one-shot effects (07a6 07d2 080a 0836 085c 0930 09a0)
= SR1 s4.7. Each starts with `if (DS:3BD5 == 0) return;` then `spk_vec_on()`, the effect, `spk_vec_off()`. Difference to SR1: `spk_vec_on` does not program 25 kHz.

### 2c57:07a6 fx_click   (new in the index; = SR1 fx_click 0f38:77ba; speaker slot 12)
`fx_warble(3000, 10, 10, 60, 9)`.

### 2c57:07d2 fx_chirp_hi   (new in the index; = SR1 fx_chirp_hi 0f38:77e1; speaker slot 13)
`fx_warble(rng(100) + 6200, 40, 10, 1, 10)` (6200 = 0x1838).

### 2c57:080a fx_chirp_lo   (new in the index; = SR1 fx_chirp_lo 0f38:7817; speaker slot 14)
`fx_warble(5200, 40, 10, 1, 10)` (0x1450).

### 2c57:0836 fx_clank   (new in the index; = SR1 fx_clank 0f38:7841; speaker slot 8)
`fx_noise(200, 10, 60)`.

### 2c57:085c fx_clank2   (new in the index; = SR1 fx_clank 0f38:7841, second identical copy; speaker slot 9)
`fx_noise(200, 10, 60)`.

### 2c57:0882 fx_crash   (changed - recompilation only)
= SR1 `fx_crash` (0f38:7863): 3 x { `spk_vec_on`; `fx_burst(3000,8000,60,10)`; `fx_noise_down(200, rng(30)+18, 15, 5, 2)`;
`fx_noise(rng(300)+40, 2, 30)`; `fx_noise_up(16, rng(100)+50, 15, 5, 4)` } then `spk_vec_off`; guard `3BD5`. RNG call order
unchanged.

### 2c57:0930 fx_hit   (new in the index; = SR1 fx_hit 0f38:790f; speaker slot 16)
Guard; `spk_vec_on; fx_burst(2000,8000,100,10); fx_noise(120,2,30); spk_vec_on; fx_burst(4000,9000,100,10); fx_noise(220,2,20); spk_vec_off`.

### 2c57:09a0 fx_thud   (new in the index; = SR1 fx_thud 0f38:7973; speaker slot 17)
Guard; `spk_vec_on; fx_sweep_down(130,90,4,1000); spk_vec_off`.

### 2c57:09ca mus_file_load   (new)
`void far mus_file_load(const char *name, int slot)`: loads a `.MUS` file completely.
```
if (_dos_open(name, 0, &h) != 0) fatal_open_error(name);                     // 1a21:61e2 (fatal_message + platform_exit)
desc = 0xB8EA + 0x46 * slot;
if (_dos_read(h, desc, 0x46, &n) != 0 || n != 0x46) fatal_read_error();      // 1a21:6220
DS:9BE8[slot] = *(u16*)(desc + 0x2A);                                         // low word of data_size
far p = farmalloc(*(u16*)(desc + 0x2A));  DS:BA94[slot] = p;  if (!p) fatal_out_of_memory();   // 1a21:6232
if (_dos_read(h, p, DS:9BE8[slot], &n) != 0 || n != DS:9BE8[slot]) fatal_read_error();
_dos_close(h);
```
(`34b0:242c` = `_dos_open`, `34b0:2444` = `_dos_read`, `34b0:135d` = far malloc, `34b0:23e2` = close.) Header = first 70 bytes of
the file (format: `FORMATS.md`, same as `tools/sr2snd.py read_mus`); song data = the following `data_size` bytes.

### 2c57:0aa2 snd_bank_and_intro_load   (new)
```
h = snd_bank_load("msr.snd" /*DS:72F0*/);  DS:AB2A = h;   // snd_bank_load ignores its (pushed) second arg 0
snd_bank_init(h);
mus_file_load("msrbeg.mus" /*DS:72F8*/, 1);
mus_set_bank(0xB930, DS:AB2A);                              // descriptor of slot 1
```
Called only by `adlib_sound_init`.

### 2c57:0ade mus_songs_load   (new)
`mus_file_load(DS:7304, 2); (DS:7310, 3); (DS:731C, 4); (DS:7328, 5); mus_set_bank(0xB976 /*slot 2 desc*/, DS:AB2A);`
Names are `X:msr2.mus` etc.; the drive letter (`X`) is patched into byte 0 of each string by `lib_open_all`. Called from `main`
(AdLib only, after `lib_read_dir(2)`) and from `title_and_setup`.

### 2c57:0b2c adlib_engine_on   (new)
`void far adlib_engine_on(int on)` - engine drone on voice 3. Nothing happens if `DS:3BD6 == 0`.
```
if (on) {
    adlib_play_timbre(3, 0x40, "motor"/*DS:7334*/);
    adlib_set_bend_range(6);
    adlib_pitch_bend(3, 0);                 // lowest pitch: 6 semitones down
    adlib_voice_off(3);
    adlib_note_on(3, 0x18);                 // note 24 -> octave 1 (note-12 = 12)
} else {
    adlib_set_volume(3, 0, 1);
    adlib_voice_off(3);
}
```
Callers: `race_isr_enter` (1), `race_isr_leave` (0), `adlib_vec_on` (mode 1 and 2), the tune screen (1 on entry, 0 on exit),
`adlib_fx_hit` (1, restarts the drone after it borrowed voice 3).

### 2c57:0ba6 adlib_engine_set   (new)
`void far (u16 bend, u16 vol)`: `if (DS:3BD6) { adlib_pitch_bend(3, bend); adlib_set_volume(3, vol, 1); }`. Called every
odd driving tick by `race_isr` (formulas in 5.3) and by the tune screen. `bend` is clamped to 3FFFh, `vol` to 7Fh by the callees.

### 2c57:0bd2 adlib_engine2_on   (new)
Second drone on voice 4 (tune screen only). `if (!DS:3BD6) return;`
on: `adlib_play_timbre(4, 0x40, "motor"/*DS:733A*/); adlib_set_bend_range(6); adlib_pitch_bend(4, 0); adlib_voice_off(4); adlib_note_on(4, 0x18);`
off: `adlib_voice_off(4); adlib_pitch_bend(4, 0x2000); adlib_play_timbre(4, 0x6A, "paint"/*DS:7340*/);` (restores the garage timbre).

### 2c57:0c5c adlib_engine2_set   (new)
`void far (u16 bend, u16 vol)`: `if (DS:3BD6) { adlib_pitch_bend(4, bend); adlib_set_volume(4, vol, 1); }`.

### AdLib one-shot slot routines: shared shape
Routines marked "key" below have the shape `(int on)`: `if (DS:3BD6 == 0) return; if (on) { adlib_set_volume(V, VOL, 1);
adlib_voice_off(V); adlib_note_on(V, NOTE); } else adlib_voice_off(V);` - they rely on the timbre already loaded on voice V
(voice 4 = `paint`, voice 5 = `part`, loaded by `adlib_garage_timbres_load` 1394 at start-up and after each race).
Routines marked "timbre" load a timbre first with `adlib_play_timbre(V, VOL, name)`.

### 2c57:0c88 adlib_fx_putt   (new; slot 0)   key: V=5, VOL=38h, NOTE=0Eh
### 2c57:0cd8 adlib_fx_tyre   (new; slot 1)
`DS:72EE = on;` then (if `DS:3BD6`) key shape with V=5, VOL=60h, NOTE=78h (on) / `adlib_voice_off(5)` (off). `race_isr` drives it: see 5.3.
### 2c57:0d2c adlib_fx_hum   (new; slot 3, no caller)   key: V=4, VOL=68h, NOTE=4Fh
### 2c57:0d7c adlib_fx_tick   (new; slot 4)   key: V=4, VOL=6Ah, NOTE=54h
### 2c57:0dcc adlib_fx_spray   (new; slot 2)   key: V=5, VOL=6Ah, NOTE=5Ah
### 2c57:0edc adlib_fx_bolt   (new; slot 5)   key: V=4, VOL=40h, NOTE=55h
### 2c57:0f2c adlib_fx_airwrench   (new; slot 6)   key: V=5, VOL=4Ah, NOTE=1Ch
### 2c57:0f7c adlib_fx_slide   (new; slot 10)   key: V=4, VOL=5Ah, NOTE=52h

### 2c57:0e1c adlib_fx_lugnut   (new; slot 7)
`(int on)`, guard `DS:3BD6`. on: `adlib_play_timbre(3, 0x7F, "turn1"/*DS:7346*/); adlib_voice_off(3); adlib_note_on(3, 0x0E);` off: `adlib_voice_off(3)`.

### 2c57:0e6c adlib_fx_clank   (new; slot 8, no argument)
Guard; `adlib_play_timbre(4, 0x7F, "hitcar"/*DS:734C*/); adlib_voice_off(4); adlib_note_on(4, 0x28);` (key left on; decays by itself).

### 2c57:0ea4 adlib_fx_clank2   (new; slot 9, no argument)
Guard; `adlib_play_timbre(3, 0x70, "hitcar"/*DS:7353*/); adlib_voice_off(3); adlib_note_on(3, 0x2E);`

### 2c57:0fcc adlib_fx_siren   (new; slot 11)
`(int on)`, guard `DS:3BD6`. on: `adlib_play_timbre(2, 0x7F, "police"/*DS:735A*/); adlib_set_bend_range(5); adlib_pitch_bend(2, 0x2000);
adlib_voice_off(2); adlib_note_on(2, 0x50); DS:72E2 = 1;` off: `adlib_voice_off(2); adlib_pitch_bend(2, 0x2000); DS:72E2 = 0;`
Note the bend range 5 is global (affects the engine drone pitch while the siren is on, see `adlib_bend_calc`).

### 2c57:1050 adlib_siren_bend   (new)
`void far (int x)`: `if (DS:3BD6) adlib_pitch_bend(2, (u16)(0x1FFF * x + 0x2000));` `race_isr` calls it with 0 (bend 2000h) and 1 (bend
3FFFh) alternately, 20 ticks each (5.3).

### 2c57:1072 adlib_fx_piano_a   (new, dead)
Guard; `adlib_play_timbre(3, 0x65, "piano1"/*DS:7361*/); adlib_voice_off(3); adlib_note_on(3, 0x3C); adlib_voice_off(3);`. No callers (not in the table).

### 2c57:10b6 adlib_fx_piano_b   (new, dead)
Same with `"piano1"/*DS:7368*/`, note `0x43`. No callers.

### 2c57:10fa adlib_fx_click   (new; slot 12)
Guard; `adlib_play_timbre(3, 0x7F, "piano1"/*DS:736F*/); adlib_voice_off(3); adlib_note_on(3, 0x5C); adlib_voice_off(3);`
### 2c57:113e adlib_fx_chirp_hi   (new; slot 13)
Guard; `adlib_play_timbre(3, 0x75, "turn"/*DS:7376*/); adlib_voice_off(3); adlib_note_on(3, rand() % 8 + 0x61); adlib_voice_off(3);`
(`rand()` = C runtime `34b0:23a8`, NOT `rng`; `idiv 8` remainder of a signed value.)
### 2c57:118e adlib_fx_chirp_lo   (new; slot 14)
Guard; `adlib_play_timbre(3, 0x75, "turn"/*DS:737B*/); adlib_voice_off(3); adlib_note_on(3, 0x3D);` (key stays on)
### 2c57:11c6 adlib_fx_crash   (new; slot 15)
Guard; `adlib_play_timbre(4, 0x7F, "hitcar"/*7380*/); adlib_voice_off(4); adlib_note_on(4, 0x14); adlib_play_timbre(5, 0x7F, "crash"/*7387*/);
adlib_voice_off(5); adlib_note_on(5, 0x19); adlib_voice_off(4); adlib_voice_off(5);` (voices keyed off immediately; the timbres' release tails sound)
### 2c57:1246 adlib_fx_hit   (new; slot 16)
Guard; `adlib_play_timbre(4, 0x7F, "hitcar"/*738D*/); adlib_voice_off(4); adlib_note_on(4, 0x14); adlib_play_timbre(3, 0x7F, "turn"/*7394*/);
adlib_voice_off(3); adlib_note_on(3, rand() % 8 + 0x3D); adlib_voice_off(4); adlib_voice_off(3); adlib_engine_on(1);`
### 2c57:12e0 adlib_fx_thud   (new; slot 17)
Guard; `adlib_play_timbre(3, 0x75, "turn1"/*DS:7399*/); adlib_voice_off(3); adlib_note_on(3, 0x10);`

### 2c57:1318 adlib_music_list_a   (new; slot 18)
```
if (DS:3BD6) {
    DS:9AE4 = DS:9AE6 = 0x73A0;  DS:9AE8 = 0;                 // song list A = {1,1,5,2,5,3,3,-1}
    i = *(s16*)0x73A0;                                          // 1
    mus_play(0xB8EA + 0x46*i, DS:BA94[i] /*far*/, DS:AB2A);
}
```
### 2c57:1356 adlib_music_list_b   (new; slot 19)
Same with list `0x73B0` = {4,4,-2}: `MSR21.MUS` twice, then the `-2` entry stops the music.

### 2c57:1394 adlib_garage_timbres_load   (new)
`if (DS:3BD6) { adlib_play_timbre(4, 0x6A, "paint"/*DS:73B6*/); adlib_play_timbre(5, 0x70, "part"/*DS:73BC*/); }`. Called by `platform_install`
(only if `DS:0050 == 0`, i.e. never at the first call because the device is still 666) and by `race_isr_leave`.

### 2c57:13c4 adlib_slot_nop   (new; slot 22)   `retf`.
### 2c57:13c6 adlib_music_stop_slot   (new; slot 20)   `mus_stop();`
### 2c57:13cc adlib_music_shutdown   (new; slot 21)   `mus_stop(); mus_timer_remove();`

### 2c57:13d8 adlib_sound_init   (new)
```
memcpy(DS:BB8C, DS:73C2, 100);
DS:3BD6 = 1;
adlib_music_init();            // 2c57:26b9
snd_bank_and_intro_load();     // 2c57:0aa2
DS:0050 = 0;  DS:004E = 1;
```

### 2c57:1406 delay   (same)
= SR1 `delay` (2196:0002), SR1 s4.7; `DS:58E2` -> `DS:742A`. `p = (u32)n * DS:742A; inner = (u16)(p >> 8); outer = p >> 24; do { for (c = inner; c--; ) ; } while (outer-- != 0)`
(LOOP with CX = inner, 0 = 65536). Port: a no-op/real-time wait scaled by the calibration (speaker effects block the game).

### 2c57:1421 pulse3_play   (changed; dead)
= SR1 `pulse3_play` (2196:001d), unreferenced in both games; DS remap `58DE`->`7426`, `58E0`->`7428`. Not to be ported.

### 2c57:14aa snd_bank_load   (new)
`u16 *far snd_bank_load(const char *name)`: `blk = malloc(6)` (fatal if NULL: `1a21:6232`); `h = open(name, O_RDWR|O_BINARY /*0x8002*/, 0x80)`
(`34b0:0f1a`; -1 -> `1a21:61e2(name)` fatal); `blk->handle = h; blk->loaded = 0 (byte +2); byte +3 = 0;` returns `blk`.
Control block: `{s16 handle; u8 loaded; u8 pad; u16 data_ptr (+4)}`.

### 2c57:151b snd_bank_init   (new)
`int far snd_bank_init(u16 *blk)`: `lseek(h,0,0); read(h, hdr, 6);` `count = hdr word +2`; `size = 0x41*count + 6` (6-byte header + 9-byte name +
56-byte parameter record per timbre); `buf = malloc(size)` (fatal if NULL); `lseek(h,0,0); read(h, buf, size); blk->data_ptr = buf; blk->loaded = 1; return 1;`
In memory: `buf+2` = count, `buf+4` = offset of the parameter block (from `tools/sr2snd.py`: `6 + 9*count`), names at `buf+6` (9 bytes each),
parameters at `buf + data_off + 56*k` (28 u16: 13 modulator, 13 carrier, modulator wave, carrier wave).

### 2c57:15c9 snd_timbre_lookup   (new)
`int far (const char *name, int *index, u8 *out56, u16 *blk)`: `if (blk->loaded) return snd_timbre_find(name, index, out56, blk);` (no return value otherwise).

### 2c57:15fb snd_timbre_find   (new)
`int far snd_timbre_find(const char *name, int *index, u8 *out56, u16 *blk)`:
```
buf = blk->data_ptr; count = *(s16*)(buf+2); k = *index;
if (name[0] != 0) {                         // search by name (strcmp, case-sensitive, whole string)
    for (k = 0; k < count; k++) if (strcmp(name, (char*)(buf + 6 + 9*k)) == 0) break;
}                                           // name "" -> use *index as given
if (k < count) { memcpy(out56, buf + *(u16*)(buf+4) + 0x38*k, 0x38); *index = k; }
return k < count;
```

### 2c57:16c6 adlib_delay_init   (new)   `DS:788C = 3; DS:788E = 0xC;`
### 2c57:16e1 adlib_write   (new)
`void far adlib_write(u8 reg, u8 val)`: `outb(DS:7830, reg); wait (DS:788C empty-loop iterations); outb(DS:7830 + 1, val); wait (DS:788E iterations);`.
Port: write `reg`/`val` to the OPL emulator (no waits needed).

### 2c57:1733 adlib_detect   (new)
`int far adlib_detect(void)`: standard Ad Lib timer test. `adlib_delay_init(); adlib_write(4,0x60); adlib_write(4,0x80); s1 = inb(DS:7830);
adlib_write(2,0xFF); adlib_write(4,0x21); 200 x inb(DS:7830); s2 = inb(DS:7830); adlib_write(4,0x60); adlib_write(4,0x80);
return ((s1 & 0xE0) == 0) && ((s2 & 0xE0) == 0xC0);` (port base `DS:7830` = 0388h at that time). Port: return 1 (emulated chip).

### 2c57:17ef adlib_voice_clear   (new)   `adlib_write(0xA0 + v, 0); adlib_write(0xB0 + v, 0);`

### 2c57:1825 adlib_set_freq   (new)
`void far adlib_set_freq(int v, int note, int key)` - programs F-number/block/key of voice `v`.
```
DS:95BC[v] = (u8)key;  DS:925A[v] = (u8)note;                  // remember raw note and key flag
n = note + DS:9164[v];  n = clamp(n, 0, 0x5F);                  // half-tone offset from pitch bend
fn = *(u16*)(DS:9242[v] + 2 * DS:917E[n]);                      // fnum row (bend sub-step) x semitone n%12
adlib_write(0xA0 + v, fn & 0xFF);
adlib_write(0xB0 + v, DS:91E0[n]*4 + (key ? 0x20 : 0) + ((fn >> 8) & 3));      // block = n/12
```

### 2c57:18c4 adlib_slot_wave_write   (new)
`(int slot)`: `adlib_write(0xE0 + DS:7854[slot], DS:91DE ? (DS:9266[slot*14+13] & 3) : 0);`

### 2c57:1914 adlib_bd_write   (new)
`adlib_write(0xBD, (DS:9162 ? 0x80:0) | (DS:9240 ? 0x40:0) | (DS:9163 ? 0x20:0) | DS:9258);`

### 2c57:195d adlib_slot_reg20 (new)   `adlib_write(0x20 + off[slot], am?0x80:0 | vib?0x40:0 | eg_sust?0x20:0 | ksr?0x10:0 | (multi & 0xF));`
### 2c57:19d0 adlib_slot_reg80 (new)   `adlib_write(0x80 + off[slot], (sustain << 4) | (release & 0xF));`
### 2c57:1a1a adlib_slot_reg60 (new)   `adlib_write(0x60 + off[slot], (attack << 4) | (decay & 0xF));`
### 2c57:1a64 adlib_slot_regC0 (new)
`if (DS:7866[slot] == 0) adlib_write(0xC0 + DS:7878[slot], (feedback << 1) | (fm == 0));` (carrier slots do not write C0).
### 2c57:1abb adlib_reg08_write (new)   `adlib_write(0x08, DS:917A ? 0x40 : 0);`
(`off[slot]` = `DS:7854[slot]`; parameter fields are `DS:9266 + 14*slot + {0 ksl,1 multi,2 feedback,3 attack,4 sustain,5 eg_sust,6 decay,7 release,8 level,9 am,10 vib,11 ksr,12 fm,13 wave}`.)

### 2c57:1ae2 adlib_slot_level_write   (new)
```
void far adlib_slot_level_write(int slot) {
    if (DS:72EC == 0) lvl = 0x3F;                       // never (72EC is constant 1)
    else lvl = 0x3F - ((((0x3F - (level[slot] & 0x3F)) * rel_vol[slot]) * 2 + 0x7F) / 0xFE);     // unsigned 16-bit arithmetic
    adlib_write(0x40 + off[slot], (ksl[slot] << 6) | lvl);
}
```
`rel_vol` = `DS:95C8[slot]` (0..7F). With `rel_vol == 7F` this returns the timbre's `level` unchanged (used for modulators).

### 2c57:1b6a adlib_slot_write_all   (new)
Writes every register of operator `slot` in this order: `adlib_bd_write(); adlib_reg08_write(); adlib_slot_level_write(slot); adlib_slot_regC0(slot);
adlib_slot_reg60(slot); adlib_slot_reg80(slot); adlib_slot_reg20(slot); adlib_slot_wave_write(slot);`

### 2c57:1bc5 adlib_slot_param_apply   (new, dead)
`(int slot, int param)` jump table (`cs:[1BE1]`, 18 entries) re-writing the register group of one parameter index 0..17: 0,8 -> level (`1ae2`);
1,5,9,10,11 -> reg20; 2,12 -> regC0; 3,6 -> reg60; 4,7 -> reg80; 13 -> wave; 14,15,17 -> BD; 16 -> reg08. Only called by `1d0c`.

### 2c57:1c6c adlib_slot_load   (new)
`void far (int slot, const u16 *words13, int wave)`: copies the **low byte** of 13 words into `DS:9266[slot*14 + 0..12]`, stores `wave & 3` at `[+13]`, then
`adlib_slot_write_all(slot)`.

### 2c57:1cc7 adlib_slot_load_bytes (new, dead) - widens 13 bytes to words in a local array and calls `adlib_slot_load`. 
### 2c57:1d0c adlib_slot_set_param (new, dead) - `DS:9266[slot*14 + param] = val; adlib_slot_param_apply(slot, param);`

### 2c57:1d40 adlib_bend_calc   (new)
`void far adlib_bend_calc(int v, u16 bend)` - fills `DS:9164[v]` (half tones) and `DS:9242[v]` (fnum row) for a pitch-bend value.
```
long x = (long)(s16)(bend - 0x2000) * (s16)DS:917C;           // 16x16 -> 32 signed multiply (917C = range*25)
if (x == DS:78AA(32-bit)) { DS:9242[v] = DS:95DA; DS:9164[v] = DS:95DC; return; }     // one-entry cache, not per voice
t = (s16)(x / 0x2000L);                                         // C division, truncates toward zero
if (t < 0) { h = (24 - t) / -25;  r = (-t) % 25;  if (r) r = 25 - r; }      // = floor division of t by 25
else       { h = t / 25;  r = t % 25; }
DS:9164[v] = DS:95DC = h;   row = 0x9362 + 24*r;  DS:9242[v] = DS:95DA = row;   DS:78AA = x;
```
Interpretation: the 25 sub-steps per semitone are columns of 25 pre-scaled F-number rows.

### 2c57:1e1f adlib_calc_prem_fnum   (new)
`long far (int num, int den)`: `d100 = 100*den (long); f8 = (d100 + 6*num) * 52088 / (d100 * 25); return f8 * 147456 / 111875;` (52088 = 26044*2;
147456 = 16384*9; 111875 = 179*625; signed long arithmetic via the MSC helpers `34b0:2590` mul, `24f6` div). Identical to `tools/sr2snd.py calc_prem_fnum`.

### 2c57:1e8e adlib_fnum_row   (new)
`void far (u16 *row, int num, int den)`: `v = calc_prem_fnum(num, den); row[0] = (u16)(v + 4) >> 3; for (k = 1; k < 12; k++) { v = v * 106; v = v / 100; row[k] = ((u16)v + 4) >> 3; }`
(`34b0:2682` = `v *= x`, `34b0:2662` = `v /= x`, signed long.) `adlib_tables_init` calls it with `den = 100`, `num = 0,4,...,96`.

### 2c57:1f06 adlib_tables_init   (new)
```
x = 0; for (row = 0x9362; row < 0x95BA; row += 24, x += 4) adlib_fnum_row(row, x, 100);     // 25 rows x 12 words
for (i = 0; i < 11; i++) { DS:9242[i] = 0x9362; DS:9164[i] = 0; }
for (oct = 0; oct < 8; oct++) for (n = 0; n < 12; n++) { DS:91E0[oct*12+n] = oct; DS:917E[oct*12+n] = n; }
```
### 2c57:1fae adlib_relvol_init   (new)   `for (i = 0; i < 18; i++) DS:95C8[i] = 0x7F;`

### 2c57:1fd6 adlib_voice_off   (new)
```
void far adlib_voice_off(int v) {
    if (DS:9163 != 0 && v >= 6) { DS:9258 &= ~DS:782C[v]; adlib_bd_write(); }
    else adlib_set_freq(v, DS:925A[v], 0);              // key-off with the remembered note
}
```
### 2c57:2024 adlib_note_on   (new)
```
void far adlib_note_on(int v, int note) {
    note -= 12; if (note < 0) note = 0;                 // MID_C - CHIP_MID_C
    if ((u16)v < 6 || DS:9163 == 0) { adlib_set_freq(v, note, 1); return; }
    if (v == 6)      adlib_set_freq(6, note, 0);        // bass drum: pitch, key bit stays 0
    else if (v == 8) { adlib_set_freq(8, note, 0); adlib_set_freq(7, note + 7, 0); }       // tom drives the snare pitch too
    DS:9258 |= DS:782C[v];  adlib_bd_write();           // voices 6..10 = BD,SD,TOM,CYM,HH
}
```
### 2c57:20cf adlib_pitch_bend   (new)
```
void far adlib_pitch_bend(int v, u16 bend) {
    if (DS:9163 != 0 && (u16)v > 6) return;             // only voices 0..6 bend in rhythm mode
    if (bend > 0x3FFF) bend = 0x3FFF;
    adlib_bend_calc(v, bend);
    adlib_set_freq(v, DS:925A[v], DS:95BC[v]);          // re-write A0/B0 with the stored note and key flag
}
```
### 2c57:2128 adlib_set_volume   (new)
```
void far adlib_set_volume(int v, u16 vol, int store) {
    if (store) DS:7890[v] = vol;                        // cache, never read
    if (DS:9163 != 0 && v >= 6) slot = DS:783E[2*v + (v == 6)];     // 6 -> 0F (BD carrier), 7 -> 10, 8 -> 0E, 9 -> 11, 10 -> 0D
    else                         slot = DS:7839[2*v];               // carrier operator of the voice
    if (vol > 0x7F) vol = 0x7F;
    DS:95C8[slot] = vol;  adlib_slot_level_write(slot);             // only the carrier / percussion operator changes
}
```
### 2c57:21b1 adlib_vec_off   (new; slot 23)
```
if (DS:004E == 2) { mus_stop(); DS:3BD6 = 0; for (i = 0; i < 11; i++) adlib_voice_off(i); }
```
(Nothing happens until the first song has been started, because `004E` stays 1 before.)
### 2c57:21f0 adlib_vec_on   (new; slot 24)
```
if (DS:004E == 2) {
    if (DS:5304 != 0 && (DS:1F7C & 0x1A) == 0) mus_resume();
    DS:3BD6 = 1;
    switch (DS:1F7C) {                                   // activity word set by the garage/race code
      case 1:  adlib_engine_on(1); if (DS:72E2 != 0) slot[11](0); break;        // driving: drone back, siren forced off
      case 2:  adlib_engine_on(1); adlib_engine2_on(1); break;                  // tune screen
      case 4:  slot[4](1); break;   case 8: slot[0](1); break;   case 0x10: slot[6](1); break;
    }
}
```
### 2c57:22bc adlib_voice_timbre   (new)
`void far (int v, const u8 *buf /*56 bytes: 28 words*/)`: mod params = words 0..12, car params = words 13..25 (byte offset 0x1A), `wave_mod` = word 26
(offset 0x34), `wave_car` = word 27 (offset 0x36).
```
if (DS:9163 != 0 && v >= 6) {
    if (v == 6) { adlib_slot_load(DS:784A[0], mod, wave_mod); adlib_slot_load(DS:784B[0], car, wave_car); }   // BD: slots 0Ch,0Fh
    else          adlib_slot_load(DS:783E[2*v], mod, wave_mod);          // single operator (SD 10h, TOM 0Eh, CYM 11h, HH 0Dh); car params ignored
} else { adlib_slot_load(DS:7838[2*v], mod, wave_mod); adlib_slot_load(DS:7839[2*v], car, wave_car); }
```
### 2c57:237f adlib_set_depths   (new)   `(am, vib, notesel)`: `DS:9162 = am; DS:9240 = vib; DS:917A = notesel; adlib_bd_write(); adlib_reg08_write();` (only called with 0,0,0).
### 2c57:23aa adlib_set_bend_range   (new)   `r = clamp(arg, 1, 12); DS:95BA = r; DS:917C = r * 25;`
### 2c57:23e3 adlib_wave_select   (new)
`DS:91DE = (arg != 0) ? 0x20 : 0; for (i = 0; i < 18; i++) adlib_write(0xE0 + DS:7854[i], 0); adlib_write(0x01, DS:91DE);`
### 2c57:2439 adlib_set_perc_mode   (new)
```
void far adlib_set_perc_mode(u8 mode) {
    DS:9163 = mode;  DS:9258 = 0;  adlib_bd_write();
    if (mode) { adlib_voice_clear(6); adlib_voice_clear(7); adlib_voice_clear(8);
                adlib_set_freq(8, 0x18, 0); adlib_set_freq(7, 0x1F, 0); }       // fixed tom / snare pitch presets
}
```
### 2c57:24ad adlib_reset   (new)
`adlib_tables_init(); adlib_set_perc_mode(1); adlib_set_depths(0,0,0); adlib_set_bend_range(1); for (i = 0; i < 9; i++) adlib_voice_clear(i);
adlib_wave_select(1); adlib_relvol_init();` Resulting chip state: reg 01h = 20h, BD = 20h, reg 08 = 0, all E0.. = 0, A0/B0 of voices 0..8 = 0.
### 2c57:2514 adlib_set_port   (new)   `DS:7830 = arg; adlib_reset();`
### 2c57:252e mus_timer_remove_wrap   (new)   `mus_timer_remove();` (2f1b:00a7)
### 2c57:2542 mus_set_tempo   (new)
`void far (u16 tempo, u16 tick_beat)`: `rate = (u32)tick_beat * tempo / 60; if (rate == 0) D = 0; else { if (rate < 19) rate = 19; D = (u16)(0x1234DC / rate); } mus_timer_set_divisor(D);`
(Only the low 16 bits of the quotient are used.)
### 2c57:25aa mus_stop   (new)
`if (DS:788A) { DS:788A = 0; for (i = 0; i < 11; i++) adlib_voice_off(i); mus_timer_set_divisor(0x4000); }`
### 2c57:25f0 mus_resume   (new)
```
if (!DS:788A) {
    DS:A7CE = DS:A7D0 = 0;                                    // elapsed
    for (i = 0; i < 11; i++) adlib_voice_off(i);
    d = DS:B192;                                              // current descriptor
    adlib_set_perc_mode(d[0x3A]); adlib_set_bend_range(d[0x3B]); mus_set_tempo(*(u16*)(d+0x3C), d[0x24]);
    DS:788A = 1;  mus_set_countdown(2);                       // continues at the saved read pointer DS:986E
}
```
### 2c57:2677 mus_wait_end   (new, dead)   `while (DS:788A) if (kbhit() && getch() == 0x1B) mus_stop();`
### 2c57:26b9 adlib_music_init   (new)   `adlib_set_port(0x388); mus_timer_install();`
### 2c57:26d9 mus_play   (new)
`void far mus_play(u16 desc, u16 ptr_off, u16 ptr_seg, u16 bank)`
```
DS:986E/9870 = far ptr; DS:B192 = desc; DS:B86E = bank; DS:A7CE = DS:A7D0 = 0;
for (i = 0; i < 11; i++) { DS:A7DC[i] = 0; adlib_voice_off(i); }
adlib_set_perc_mode(d[0x3A]); adlib_set_bend_range(d[0x3B]); mus_set_tempo(*(u16*)(d+0x3C), d[0x24]);
mus_set_countdown(0);
DS:B19E = *p++;                                              // first delay byte (raw)
DS:004E = 2;  DS:788A = 1;
mus_set_countdown(max(DS:B19E, 1));
```
Does not reset `rel_vol`, bend state, running status `9A7E`, timbres.
### 2c57:27a0 mus_set_bank   (new)
`(u16 desc, u16 bank)`: `DS:B192 = desc; DS:B86E = bank; DS:A7DC[0..10] = 0; adlib_set_perc_mode(((u8*)desc)[0x3A]);`
### 2c57:27e5 adlib_play_timbre   (new)
`void far (int v, int vol, const char *name)`: `adlib_set_volume(v, vol, 1); idx = 1; if (snd_timbre_lookup(name, &idx, buf56, DS:B86E)) adlib_voice_timbre(v, buf56);`
(timbre not found -> silently unchanged; the SFX names all exist in `MSR.SND`).
### 2c57:2837 mus_skip_unknown   (new)
`p = DS:986E; if (*p < 0x80) { do p++; while (*p < 0x80); } if (*p != 0xF8) p--; DS:986E = p;` (resynchronisation for unknown status bytes; never hit by the shipped songs).

### 2c57:2875 mus_tick   (new)
`int far mus_tick(void)` - called by `mus_timer_isr` when the countdown expires; returns the number of PIT interrupts to wait (>= 1), 0 after the playlist ended.
`p` = far pointer `DS:986E/9870`, `d` = `DS:B192`, `desc(idx)` = `0xB8EA + 0x46*idx`.
```
if (!DS:788A) return 1;
for (;;) {
    DS:A7CE += DS:B19E;                                              // (u32) elapsed ticks, never read
    b = *p;
    if (b == 0xF8) { p++; delay = 0xF8; goto have_delay; }           // continuation of a long delay
    if (b == 0xFC) {                                                 // end of song
        if (DS:9AE8 == 0) {                                          // playlist mode (always)
            DS:9AE6 += 2;  e = *(s16*)DS:9AE6;
            if (e != -2) {
                if (e == -1) DS:9AE6 = DS:9AE4;                      // wrap to list start
                i = *(s16*)DS:9AE6;                                  // song slot
                p = DS:BA94[i]; d = DS:B192 = 0xB8EA + 0x46*i;
                adlib_set_bend_range(d[0x3B]); mus_set_tempo(*(u16*)(d+0x3C), d[0x24]);
                DS:B19E = *p++;  return DS:B19E;                     // first delay returned raw (no F8/0 handling); no set_perc_mode, no voice_off, caches kept
            }
        }
        mus_stop();  return 0;
    }
    if (b == 0xF0) {                                                 // system exclusive
        p++; b1 = *p++; b2 = *p++;
        if (b1 == 0x7F && b2 == 0) {                                 // tempo change: F0 7F 00 int frac F7
            ip = *p++; fr = *p++;  base = *(u16*)(d+0x3C);
            t = (u16)(ip * base) + (u16)(((u32)fr * base) >> 7);
            mus_set_tempo(t, d[0x24]);  p++;                         // skip F7
        } else { p -= 2; do { b = *p++; } while (b != 0xF7); }       // skip to after F7
        delay = *p++;  goto have_delay;
    }
    if (b >= 0x80) { p++; DS:9A7E = b; }                             // new status, else running status
    v = DS:9A7E & 0x0F;  hi = DS:9A7E & 0xF0;
    switch (hi) {
      case 0x80: p += 2; adlib_voice_off(v); break;
      case 0x90: note = *p++; vel = *p++;
                 if (vel == 0) adlib_voice_off(v);
                 else { if (DS:A7DC[v] != vel) { DS:A7DC[v] = vel; adlib_set_volume(v, vel, 1); } adlib_note_on(v, note); }
                 break;
      case 0xA0: adlib_set_volume(v, *p++, 1); break;                // does not touch DS:A7DC
      case 0xB0: p += 2; break;
      case 0xC0: idx = *p++;                                         // program change = timbre index into MSR.SND
                 if (snd_timbre_lookup(DS:78AE /*""*/, &idx, buf56, DS:B86E)) adlib_voice_timbre(v, buf56);
                 else printf("\nTimbre not found: %d", idx);        // DS:78AF
                 break;
      case 0xD0: p += 1; break;
      case 0xE0: lo = *p++; hi7 = *p++; adlib_pitch_bend(v, lo + (hi7 << 7)); break;
      default:   mus_skip_unknown(); break;
    }
    delay = *p++;                                                    // delay of the NEXT event
have_delay:
    DS:B19E = delay;
    if (delay == 0) continue;                                        // same tick: next event immediately
    if (delay == 0xF8) { DS:B19E = 0xF0; if (*p != 0xF8) DS:B19E += *p++; }     // 240 + d, or 240 and the next F8 is re-read next call
    return DS:B19E;
}
```
Note the event stream layout: `delay, event, delay, event, ...` (first delay is read by `mus_play`). Delay values 0xF8 mean "240 ticks, continue".
Tick unit = one music-timer interrupt (rate set by `mus_set_tempo`).

### 2f1b:0027 pit_set_divisor   (new; register call)
Input `AX` = divisor. `push ax; outb(0x43, 0x36); pop ax; outb(0x40, al); outb(0x40, ah);` (channel 0, lo/hi, mode 3; `AX = 0` programs 65536 = 18.2 Hz).

### 2f1b:0036 irq_flag_set   (new, dead)
`void far (int on)`: `if (on == 0) cli; else sti;` (no callers).

### 2f1b:0045 mus_timer_set_divisor   (new)
`void far (u16 div)`: `pushf; cli; cs:[08] = div; cs:[06] = (div == 0); pit_set_divisor(div); popf;`

### 2f1b:0069 mus_timer_install   (new)
```
pit_set_divisor(0x4000);
cs:[06] = 0;  cs:[08] = 0x4000;  cs:[0A] = 0x4000;  cs:[0C] = 0x4000;  cs:[26] = 0;  cs:[1E] = DS;
_dos_setvect(8 /*INT 21h AH=25h*/, 2f1b:00CB);
```
`cs:[0E]` (original BIOS INT 8), `cs:[12]` (chained game ISR) were stored earlier by `platform_install`.

### 2f1b:00a7 mus_timer_remove   (new)
`pit_set_divisor(0); _dos_setvect(8, *(far*)cs:[0E]);` (original BIOS handler, 18.2 Hz).

### 2f1b:00bc mus_set_countdown   (new)
`void far (u16 n)`: `pushf; cli; cs:[24] = n; popf;`

### 2f1b:00cb mus_timer_isr   (new)
See 1.4 for the full algorithm (accumulator `cs:[0C] -= cs:[08]`, chain `cs:[12]` when `<= 0` with reload 4000h, EOI otherwise;
then `if (DS:788A && --cs:[24] == 0 && cs:[26] == 0)` run `mus_tick` on the private stack `DS:782C`).

---

## 4. Data

### 4.1 Playlists and files (AdLib)
Song descriptor slot = file: 1 `MSRBEG.MUS` (37 s), 2 `MSR2.MUS` (64 s), 3 `MSR24.MUS` (26 s), 4 `MSR21.MUS` (36 s, tempo 60), 5 `PAUSE.MUS` (4 s).
All five are rhythm-mode songs (`sound_mode` 1), pitch-bend range 1, `tick_beat` 240, basic tempo 120 (MSR21: 60) -> music tick 480 Hz
(240 Hz). List A (`DS:73A0`, slot 18): MSRBEG, MSRBEG, PAUSE, MSR2, PAUSE, MSR24, MSR24, then back to the start (-1). List B (`DS:73B0`,
slot 19): MSR21, MSR21, then stop (-2).
`MSR.SND` timbres (30): `piano1 jazzguit tom1 bdrum1 snare1 cymbal1 hihat1 syn3 police turn1 crash part turn paint hitcar motor guitar1 bass1
hihat2 tuexplos fuzguit2 moogsynt bass2 elpiano1 scratch snakefl logdrum1 elguit1 logdrum2 motor1`. Names used by effects: `motor`, `paint`, `turn1`,
`hitcar`, `police`, `piano1`, `turn`, `crash`, `part` (all present). `MSR20.MUS` is on disk but never referenced.

### 4.2 Speaker songs (DS, near pointers, little-endian u16)
Format = SR1 s5.1: header `0,0`, events `(dur ticks, PIT divisor)` (divisor 0 = rest), section marker `0,m` (m != 0), end `0,0`.
Loop tables (s16 per section, **total plays**, -1 = restart the whole song): `540C` {3,2,1,3,2,1,3,3,1,3,1,1,1,1,-1} (background tune, 14 sections),
`542A` {3,-1}, `542E` {1,-1} (all one-section effect songs).

| song | addr | content | used by |
|---|---|---|---|
| background tune | 5594..6E47 | 14 sections (66..2885 ticks each, 12 042 ticks if every section were played once); with the repeats of `540C` 22 161 ticks = 5 min 04 s at 72.82 Hz; divisor range 1015..28960 | slot 18 (`DS:540C`, vib `DS:5432`) |
| jingle | 6E48..72DF | 1 section, 291 events, 2515 ticks (34.5 s), played 3 times (`542A`), vib records 1,2,3 (`5468`) | slot 19 |
| 5490 | 5490 | `(1,9121)(3,0)(1,8126)(3,0)(1,7670)(3,0)(1,8126)(3,0)` | slot 4 |
| 54BC | 54BC | `(2,36488)(2,0)(2,24350)(2,36488)(3,28960)(17,0)` | slot 7 |
| 54DC | 54DC | `(3,170)(3,39772)(3,175)(3,13257)(7,192)(7,29829)` | slot 10 |
| 5500 | 5500 | `(3,149)(2,11931)(3,135)(2,13257)(4,142)(4,8522)` | slot 6 |
| 5524 | 5524 | `(7,11931)` | slot 3 |
| 5534 | 5534 | `(7,7977)` | slot 2 |
| 5544 | 5544 | `(1,36488)(8,0)(1,32502)(8,0)(1,30680)(8,0)(1,32502)(8,0)` | slot 0 |
| 5570 | 5570 | `(3,149)(2,0)(3,135)(2,0)(4,142)(4,0)` | slot 5 |

No song contains a note with divisor 6000 (1770h), so the "engine note override" in `timer_isr` (6.2) never triggers.

### 4.3 Vibrato (speaker)
Waves `DS:536E` (row 0 `0 1 2 1 0 -1 -2 -1`, row 1 `0..7`, row 2 `0 1 0 0 0 0 0 0`). Records `DS:539E + 10*i` = `{phase, row, depth, period, step}`:
`0:{0,0,0,8,20}` (target of `vib_set`), `1:{0,0,3,8,1}`, `2:{0,1,1,8,1}`, `3:{0,2,5,2,1}`, `4:{0,2,10,2,1}`, `5:{0,0,5,8,1}`, `6:{0,0,3,8,1}`,
`7:{0,1,15,8,1}`, `8:{0,1,10,8,1}`, `9:{0,1,7,8,1}`, `10:{0,2,25,2,1}`, `11:{3,2,1,3,2}` (unused).
Per-section record index tables: `5432` (background tune: 26 entries `3 2 1 3 2 4 3 2 1 4 3 1 3 2 1 1 3 2 2 3 2 1 3 2 1 4`, then -1, one per section *play*),
`5468 {1,2,3,-1}` (jingle), `5470 {0,-1}`, `5474 {0,-1}`, `5478 {5,-1}`, `547C {6,-1}`, `5480 {7,-1}`, `5484 {8,-1}`, `5488 {9,-1}`, `548C {10,-1}`.

---

## 5. Sound parts of functions owned by other modules

(Listed so the other specs can reference them; the functions themselves are in section 8 "Handoffs".)

### 5.1 `platform_install` 0001:19e8 - sound-relevant part
```
DS:9DF2 = 0;                                   // bg.playing
bg = SongState18 at DS:9DEC: dur(9DEE)=1, rep(9DF0)=1, regate(9DF4)=1, base(9DF6)=0x5594, sec_start(9DF8)=cur(9DFA)=0x5598,
     loop_tbl(9DFC)=loop_ptr(9DFE)=0x540C, vib_tbl(9E00)=vib_ptr(9E02)=0x5432;
DS:ADD2 = 0x539E + 10 * *(u16*)0x5432;  DS:9DB0 = 0;          // fx.playing = 0
outb(0x43,0x36); outb(0x40,0x00); outb(0x40,0x40);              // PIT ch0 divisor 0x4000 (72.82 Hz)
DS:A2AC/A2AE = old = _dos_getvect(8);  2f1b:[0E..11] = old;       // saved BIOS handler (restored by timer_restore / mus_timer_remove)
_dos_setvect(8, race_isr);  2f1b:[1A..1D] = _dos_getvect(8);        // far pointers of both ISRs are stored in segment 2f1b
_dos_setvect(8, timer_isr); 2f1b:[16..19] = _dos_getvect(8);  2f1b:[12..15] = 2f1b:[16..19];   // chained game ISR := timer_isr
DS:1F7A = 0;  if (DS:0050 == 0) adlib_garage_timbres_load();     // not taken at start-up (DS:0050 = 666)
kbd_install(); DS:1F4E = 1; outb(0x43, 0xB6); input_reset(); DS:0044 = 1;
```
INT 8 ends up = `timer_isr` (speaker or no sound). For AdLib `adlib_music_init` (called later by `adlib_sound_init`) replaces INT 8 by `mus_timer_isr`.

### 5.2 `timer_isr` 0001:2246 - sound part (every game tick; SR1 s4.3 `isr1fe2_sound`)
Order inside the ISR: tick counters / BIOS chain (`DS:1F76` counts 3,2,1,0; BIOS handler `DS:A2AC` called on 0) - then the sound block - then the
mouse/joystick part (platform).
```
if (DS:3BD5 == 0 && DS:3BD6 != 0) slot[23]();                               // sound switched off -> silence (both devices)
if (DS:0050 == 1 && ((SongState18*)DS:52FE)->playing) speaker_music_tick();   // 6.2
if (DS:0050 == 0 && DS:3BD5 != 0 && DS:3BD6 == 0) slot[24]();                // AdLib: sound switched on again -> adlib_vec_on
```
(For the speaker, `slot[24]` is called inside `speaker_music_tick` when a note starts.) For AdLib the whole speaker block is skipped; the music
itself runs from `mus_timer_isr`.

### 5.3 `race_isr` 0001:2622 - sound part (driving)
Runs when `DS:1F7A == 0`, after the tick counter `DS:1F66++`. `DS:1F76` (3,2,1,0) is the sub-tick, bit 0 selects the phase.
```
if (DS:3BD5 == 0) { if (DS:3BD6) slot[23](); }
else {
    if (DS:3BD6 != DS:3BD5) slot[24]();
    if (DS:0050 == 1) {                                  // ---- speaker
        if (DS:1F76 & 1) {                               // engine phase
            if (DS:1BCA == 0) {
                bonus = (DS:A10E == 0x1644) ? 6000 : 0;                   // limiter
                u = (u16)(((s32)rpm * rpm) >> 10);   // rpm = (s16)DS:A10E
                div = (u16)(60000 - (u16)(u + (u >> 3) + (u >> 2)) - bonus);
                if (*(s16*)(DS:AA02 + 0x1E) == 0 && DS:881E++ < 1) { outb(0x42, lo(div)); outb(0x42, 0xF0); /* pulse: very low tone */ }
                else { outb(0x42, lo(div)); outb(0x42, hi(div)); DS:881E = 0; }
            } else {                                      // animation (garage) player
                d = abs((s16)(DS:AD5C - DS:B866)); t = (u16)((d >> 1) * d); if (t > 20000) t = 20000;
                div = 60000 - t;
                if (DS:B8D4 == 0 && DS:881E++ < 2) { outb(0x42, lo(div)); outb(0x42, 0xF0); }
                else { outb(0x42, lo(div)); outb(0x42, hi(div)); DS:881E = 0; }
            }
        } else {                                          // siren / squeal phase
            DS:200A ^= 1;
            if (DS:72E2) { n = DS:2004++; div = (n < 10) ? 0x054B : 0x0712; outb(0x42, lo); outb(0x42, hi); if (DS:2004 > 20) DS:2004 = 0; }
            else if ((DS:72E0 || DS:7BA2) && DS:200A) { outb(0x42, 0x95); outb(0x42, 0x00); }      // 8 kHz blip
        }
    } else if (DS:0050 == 0) {                            // ---- AdLib (every tick)
        if (DS:1BCA == 0) {
            bend = (DS:A10E == 0x1644) ? 0x3FFF : (u16)(DS:A10E * 3 - 0x1194);           // idle 1500 rpm -> 0
            a = ((s16)(DS:A10E - 0x5DC)) / 128 (truncating: |x|>>7 with the sign restored);
            vol = (*(s16*)(DS:AA02 + 0x1E) != 0) ? a + 0x50 : 0x7F00;                     // clamped to 7Fh by set_volume
        } else {
            d = abs((s16)(DS:AD5C - DS:B866)); t = (u16)((d >> 1) * d); if (t > 20000) t = 20000;
            bend = t >> 1;  vol = (DS:B8D4 == 0) ? 0x7F00 : 0x50;
        }
        adlib_engine_set(bend, vol);
        if (DS:72E2) { n = DS:2004++; if (n > 0x28) DS:2004 = 0; if (DS:2004 == 0) adlib_siren_bend(0); else if (DS:2004 == 0x14) adlib_siren_bend(1); }
        DS:200A = (DS:200A > 14) ? 0 : DS:200A + 1;       // 0..15 phase counter (byte, post-increment compare)
        if (DS:72E0 == 0 && DS:7BA2 == 0) { if (DS:72EE) adlib_fx_tyre(0); }
        else if (DS:200A == 0) adlib_fx_tyre(1);
        else if (DS:200A == 8) adlib_fx_tyre(0);
    }
}
EOI / game-tick chain part (platform): `if (DS:1F76 != 0) { outb(0x20,0x20); DS:1F76--; } else { DS:1F6E++...; call old BIOS; DS:1F76 = 3; ... physics }`
```
Speaker engine note examples (divisor, Hz = 1193180/div): 1500 rpm 56980 (20.9 Hz), 3000 rpm 47916 (24.9 Hz), 5000 rpm 26432 (45.1 Hz),
5699 rpm 16390 (72.8 Hz), limiter (5700) 10374 (115.0 Hz). SR1 differences: base 50000/limiter bonus 10000 and a buggy
`x >> ((t & 255) + 2)` term are replaced by `60000 - 1.375*u` and bonus 6000; the `DS:881E` low-tone pulse (when the throttle word `car+1E == 0`
or the animation flag `B8D4 == 0`) is new; `DS:200A`, `DS:2004` as SR1 `06EA`, `06E4`. Phase: SR2 reads `DS:1F76` *before* its decrement and runs the sound block on all four sub-ticks (3,1 = engine, 2,0 = siren/squeal); SR1 ran it on three of four (engine only on one).
(`DS:AA02` = current car pointer; word `+1E` zero = no throttle/neutral; the exact meaning belongs to `race`.)

### 5.4 Tune-up screen `FUN_0001_7aec` (garage) - sound part (SR1 s4.4)
```
music_stop();  DS:5300 = 2;
if (DS:0050 == 0) { adlib_engine_on(1); adlib_engine2_on(1); }
DS:1F7C = 2;
do {                                                   // per frame
    if (DS:3BD5 && DS:3BD6 != DS:3BD5) slot[24]();
    p = (s8)car[0x23];                                  // carburettor/setting -5..3
    if (DS:0050 == 1) { lo = (u8)((-16 - p) * 4); outb(0x42, 0); tog ^= 1; if (tog && p != 0) lo = 0xF0; outb(0x42, lo); }      // divisor hi byte alternates
    else {
        adlib_engine_set (((u8)((p + 16) * 2)) << 8,  (p == 0 ? -8 : 0) + 0x52);
        adlib_engine2_set(((u8)(p * 3 + 0x20)) << 8, (p == 0 ? -8 : 0) + 0x52);
    }
    ... draw, wait for key; -10 (+): p++ (max 3), -7 (-): p-- (min -5) ...
} while (key != -1);
on exit: if (DS:0050 == 0) { adlib_engine_on(0); adlib_engine2_on(0); }  DS:1F7C = 0;  if (DS:0050 == 1) slot[23]();  DS:5300 = 1;  if (DS:5304) music_resume();
```

---

## 6. Speaker sequencer (delta vs SR1 s4.3)

### 6.1 State
`SongState18` at `DS:9DEC` (background) and `DS:9DAA` (loop sounds, jingle); active one = `*(u16*)DS:52FE`. Fields in section 2. Songs/tables are in DS, so
all pointers are 16-bit near. Loop counts are total plays; `rep` is set to 1 whenever a new section starts (and by `tune_start`).

### 6.2 `speaker_music_tick` (inside `timer_isr`, when `DS:0050 == 1 && s->playing`)
```
s = (SongState18*)DS:52FE;
if (DS:5302 == 0) s->dur--;                              // hold flag: duration frozen while nonzero (never set by the shipped tables:
                                                          //   it needs a loop-table entry of 0, all entries are >= 1)
if (s->dur != 0) {                                        // ---- inside a note: vibrato
    if (DS:9A3A && s->note != 0 && --DS:2002 == 0) {
        r = (s16)rec[0] % (s16)rec[3];  rec[0] = r;                                // rec = (u16*)DS:ADD2: {phase,row,depth,period,step}
        w = *(s16*)(0x536E + 2*(rec[1]*8 + r));  div = (u16)(w * (s16)DS:B870) + s->note;  DS:8822 = div;
        rec[0]++;  outb(0x42, lo(div)); outb(0x42, hi(div));  DS:2002 = rec[4];
    }
    return (to the sound-switch check of 5.2);
}
for (;;) {                                                // ---- fetch events until a note/rest is set
    s->dur = *(u16*)s->cur; s->cur += 2;
    if (s->dur == 0) {                                    // marker
        m = *(u16*)s->cur; s->cur += 2;
        if (m == 0) { slot[23](); s->playing = 0; s->regate = 1; s->cur = s->sec_start = s->base; return; }     // end of song
        s->vib_ptr += 2;  s->rep++;
        if (*(s16*)s->loop_ptr >= (s16)s->rep) s->cur = s->sec_start;                 // play the section again
        else {
            s->loop_ptr += 2;
            if (*(s16*)s->loop_ptr < 0) { s->cur = s->base + 4; s->loop_ptr = s->loop_tbl; s->vib_ptr = s->vib_tbl; }
            if (*(s16*)s->loop_ptr == 0) DS:5302 = 1;                                    // (dead in practice)
            s->sec_start = s->cur;  s->rep = 1;
        }
        DS:ADD2 = 0x539E + 10 * *(u16*)s->vib_ptr;
        continue;
    }
    s->note = *(u16*)s->cur; s->cur += 2;
    if (s->note == 0) { s->regate = 1; slot[23](); return; }                  // rest: gate off, divisor unchanged
    if (DS:9A3A) {
        if (s->note == 6000) { s->note = DS:A9C4; DS:B870 = DS:9B08; }        // engine override (never in the data)
        else { DS:2002 = rec[4]; DS:B870 = (s->note / 500) * rec[2]; }        // 16-bit div, 16-bit mul
    }
    outb(0x42, lo(s->note)); outb(0x42, hi(s->note));
    if (DS:3BD5 && (DS:3BD6 == 0 || s->regate)) { slot[24](); if (s->regate) s->regate = 0; }
    return;
}
```
Differences to SR1 (apart from address remap in 1.5):
1. Vibrato amount `B870 = (note / 500) * depth` (SR1 `((depth * note) & 0xFFFF) / 1000`); phase update unchanged; wave table index `row*8 + phase`.
2. Section repeat test `loop >= rep` is unchanged but `rep` restarts at **1** and the tables hold total plays (SR1: extra plays) -> same audible repeat counts.
3. Hold flag `DS:5302`, engine override (note 6000) - both dead in the shipped data.
4. The gate is driven through slots 23/24 (`[3bd6]`) instead of inline port writes; `regate` handling is equivalent to SR1's `47CF != 47CE || regate`.
5. Playback of the background tune is started by `song_select` (slot 18) instead of `music_start`; end-of-song/restart behaviour is the same.

---

## 7. Differences from tools/sr2snd.py

Checked against the game code (driver 2c57:14aa..2875, timer 2f1b). "Same" items are listed first so nobody has to re-verify them.

### 7.1 Identical to the game (no change needed)
| item | value |
|---|---|
| F-number table | `calc_prem_fnum(num,den)`: `f8=(100*den+6*num)*52088/(100*den*25); f8*16384*9/(179*625)`; game calls it with `den=100, num=0,4,..,96` (ratio `i/25`; identical quotients), row = `(v+4)>>3` for `v, v*106/100, ...` (12 columns); 25 rows (`DS:9362`) |
| semitone map | note-in-octave `n % 12`, block `n / 12`, pitch clamp 0..95 after adding the bend half-tones; `MID_C - CHIP_MID_C = 12` subtracted from every note-on |
| volume | carrier (or the single percussion operator) only: `level_reg = (ksl<<6) \| (63 - (((63 - (lvl & 63)) * rel_vol * 2 + 127) / 254))`, `rel_vol` = velocity clamped to 127; all `rel_vol` start at 127 (so modulators = timbre level) |
| operator / voice tables | `OFFSET_SLOT`, `VOICE_OF_SLOT`, `IS_CARRIER`, `SLOT_VOICE`, `SLOT_PERC`, `PERC_MASK = 10 08 04 02 01`, `TOM_TO_SD = 7` (DS:7854, 7878, 7866, 7838, 783E.., 782C) |
| timbre -> registers | reg20 `am<<7 \| vib<<6 \| eg_sust<<5 \| ksr<<4 \| multi&15`; reg60 `attack<<4 \| decay&15`; reg80 `sustain<<4 \| release&15`; regC0 (modulator slot only) `feedback<<1 \| (fm==0)`; regE0 `wave&3`; BD `0x20` in rhythm mode, drum bits OR'd; reg01 = 20h |
| header | `tick_beat` at +36, `sound_mode` +58, `pitch_bend_range` +59, `basic_tempo` +60; timbre bank layout (`6 + 9*count` names/params, 56 bytes per timbre) |
| tempo event | `F0 7F 00 int frac F7`: `tempo = (u16)(basic*int) + (u16)((basic*frac) >> 7)` |
| delay | byte `0..239`; `F8` = 240 and continue (consecutive F8 add up); delay 0 = next event in the same tick |
| events | `9n` note-on (vel 0 = off), `8n` off, `An` volume, `Cn` timbre index, `En` bend (lsb + msb<<7), running status. Shipped songs contain only `9n`, `An`, `Cn`, `En`, one tempo event `1.0`, end `FC`; first delay is always 0 |

### 7.2 Differences (the port must follow the game)
| # | topic | tools/sr2snd.py | game (exact) |
|---|---|---|---|
| 1 | tick rate | `ticks/s = tempo * tick_beat / 60.0` (float) | `rate = (u32)tick_beat * tempo / 60` (integer division), `if rate == 0: divisor 0 (65536)`, else `rate = max(rate, 19)`; PIT divisor `D = 1193180 / rate` (integer); real tick rate `1193180 / D`: tempo 120 -> rate 480, D 2485, 480.15 Hz; tempo 60 -> 240, D 4971, 240.03 Hz. Recomputed at every tempo event and every song start. All delays count these interrupts (integer) |
| 2 | first event | `t = first_delay / rate` | first delay byte (always 0 here) is read raw; countdown = `max(delay, 1)` interrupts, i.e. the first event happens 1 tick (2 ms) after start |
| 3 | song structure | one song, `--loops N` repeats it from the data start | **playlists**, never a single song: list A (slot 18) `MSRBEG, MSRBEG, PAUSE, MSR2, PAUSE, MSR24, MSR24`, then restart; list B (slot 19) `MSR21, MSR21`, then silence. At `FC` the next song starts immediately: bend range and tempo are reloaded from its header (`set_bend_range`, `set_tempo`), but **no** `set_perc_mode`, no key-off of the 11 voices, no velocity-cache clear, running status kept. Its first delay byte is returned **raw** from `mus_tick`: all six shipped songs start with delay `0`, so `mus_tick` returns 0, the ISR sets `cs:[24] = 0` and the next `dec cs:[24]` wraps to FFFFh: the next `mus_tick` runs only after **65 536 music interrupts** (136.5 s at 480 Hz, 273 s at 240 Hz). By the code every playlist transition is therefore followed by a very long silence (unlike the first song, where `mus_play` uses `max(delay,1)`) - see open question 7 |
| 4 | game timer coupling | none | the music PIT rate drives the game tick through the accumulator rule of 1.4: while a song plays the game tick is `1193180/D / ceil(16384/D)` = 68.6 Hz (tempo 120) / 60.0 Hz (tempo 60) instead of 72.82 Hz |
| 5 | state at song start (`mus_play`) | fresh `Driver` per song (all registers reset, bend 0, rel_vol 127) | chip is reset **once** (`adlib_reset`); per song only: `voice_off(0..10)`, velocity cache `A7DC[0..10]=0`, `set_perc_mode(mode)`, bend range, tempo. Timbres, `rel_vol`, bend half-tone/fnum-row state (`9164`, `9242`), running status persist from the previous song/SFX. SFX use voices 2..5 of the same chip (timbres `motor police turn1 hitcar piano1 turn crash paint part`) and overwrite whatever the music loaded there |
| 6 | rhythm-mode presets | none until a tom note | `set_perc_mode(mode != 0)` (also from `adlib_reset`, `mus_play`, `mus_resume`, `mus_set_bank`): `A0/B0` of voices 6,7,8 cleared, then tom (voice 8) `set_freq(note 24, key 0)` and snare (voice 7) `set_freq(note 31, key 0)` written raw (no -12). `BD` written with drum bits 0 |
| 7 | note range | `pitch = note - 12` (may go negative, stored negative) | `note - 12`, **clamped to >= 0 before storing**; the stored (clamped) value is what a later key-off / bend re-uses |
| 8 | pitch bend, arithmetic | `t = (bend-0x2000) * (range*25) // 0x2000` (Python floor) | `x = (s16)(bend-0x2000) * (s16)(range*25)` (32-bit); `t = x / 0x2000` **C truncation toward zero** (low 16 bits); then `half = floor(t/25)`, `step = t - 25*half` in 0..24 (negative t handled as `(24-t)/-25`, `(-t)%25`, `25-r`). Result differs from the tool only for `x` not a multiple of 0x2000 and negative (tool is 1/25 semitone lower) |
| 9 | bend range | `bend_range or 1` | clamp to **1..12** (`set_bend_range`) |
| 10 | bend scope | applies to any voice < 9 | in rhythm mode (`DS:9163 != 0`) only voices **0..6** are bent (tool also bends 7 and 8); bend value clamped to 3FFFh; after a bend the voice's `A0/B0` are rewritten with the *stored* note and key flag (also for voices that never played: key-off, note 0) |
| 11 | velocity | every note-on: `volume(v, vel)` then key-on | `if (A7DC[v] != vel) { A7DC[v] = vel; set_volume }` - an `An` volume event does **not** update the cache, so a later note-on with the cached velocity keeps the `An` level. Cache cleared at song start only (not at playlist change) |
| 12 | voices >= 9 in melodic mode | ignored | not guarded: `A0+9..`, `B0+9..` writes (non-existent channels on a real OPL2; ignore them) |
| 13 | parameters | full integers | each of the 13 parameters is truncated to its **low byte**, wave `& 3`; waves applied only because `DS:91DE` = 20h (always) |
| 14 | register order / extras | 20,40,60,80,C0,E0 per timbre | per timbre/operator: `BD`, `08`, `40+off`, `C0+voice` (modulator only), `60`, `80`, `20`, `E0` (BD and 08 rewritten each time with the current depth/rhythm bits, i.e. `BD = 20h \| drum bits`, `08 = 0`). Chip reset also writes `E0+off = 0` for all 18 operators, then `01h = 20h`, `A0/B0 = 0` for voices 0..8 |
| 15 | unknown status bytes | counted as `unknown`, pos+1 | `mus_skip_unknown` (2837): skip data bytes < 80h, stop at the next status/F8, step back one; `Bn` skips 2 data bytes, `Dn` 1 byte (none occur in the shipped files) |
| 16 | timbre index out of range | event ignored | event ignored; the game also prints `"\nTimbre not found: %d"` to the text console (ignore) |
| 17 | OPL access | writes only the registers above | every `adlib_write` = index write, 3-iteration delay, data write, 12-iteration delay (real-hardware settle); irrelevant for an emulator |
| 18 | end of data | stops at `len(data)` | relies on the `FC` byte (all six files have exactly one at the end) |

Port recipe: implement the 2c57 driver literally on top of an OPL2/OPL3 emulator (`adlib_write` -> emulator), run `mus_tick` from a simulated
PIT-0 at `1193180/D` Hz with the countdown/chaining of 1.4, and render with the emulator's own clock (49716 Hz output). `tools/sr2snd.py` can stay
as an offline renderer once items 1, 3, 5, 6, 7, 8, 9, 10, 11 are applied (items 4 and 14 only matter for the real-time port).

---

## 8. Handoffs

Functions of the neighbour-vote list that belong to other modules (not in `sound_symbols.csv`), with the sound-relevant data they consume or produce:

| function | owner | notes |
|---|---|---|
| `0001:1708` click_clear (SR1 `ui_request_redraw` 0d78) | platform | `[881C]=1; [B880]=-1; [B898]=32000; [3B3E]=[3B3F]=0; key_read()`; called at the end of `music_stop/resume` |
| `0001:189e` timer_restore | platform | `outb(43h,36h); outb(40h,0); outb(40h,0)`; `_dos_setvect(8, DS:A2AC/A2AE)`; `kbd_restore()`; `[1F4E]=0; [0044]=0` (after slot 21 has already restored INT 8 for AdLib) |
| `0001:19e8` platform_install | platform | sound part in 5.1 |
| `0001:2002` tune_start | **claimed here** (voted game_flow) | section 3 |
| `0001:2222` sign(int) | platform | returns 0/1/-1; used by the mouse auto-move in `timer_isr` |
| `0001:2246` timer_isr | platform | sound part in 5.2 / 6.2 |
| `0001:2622` race_isr | race | sound part in 5.3 (speaker tones, AdLib engine/siren/tyre) |
| `0001:7aec` tune-up screen | garage | sound part in 5.4 |
| `0dcb:148a` FUN_0dcb_148a | race | collision / edge test: calls slot 16 (hit) up to 5 times |
| `0dcb:1cd2` FUN_0dcb_1cd2 | race | no sound |
| `1a21:61e2` fatal_file_error(name) | platform | copies `name` behind the message `DS:4B6E+0x10`, `fatal_message(DS:4B6E)`, `platform_exit()` (used by `snd_bank_load`, `mus_file_load`) |
| `1a21:6232` fatal_out_of_memory | platform | `fatal_message(DS:4B70); platform_exit();` |

Also used but owned elsewhere: `rng` 1a21:6180, `1a21:6220` fatal read error, `0000:0000 print_line(str, indent)` (runtime), `0001:0090 gotoxy(col,row)` (game_flow), `34b0:*` C runtime
(`_dos_open` 242c, `_dos_read` 2444, close 23e2, far malloc 135d, malloc 1321, open 0f1a, lseek 0e86, read 10bc, printf 1904, rand 23a8, kbhit 1762, getch 1788, int86 17b0,
long mul 2590, long div 24f6, `*=` 2682, `/=` 2662).

Data passed to other modules: `DS:3BD5`/`3BD6` (kbd ISR toggles `3BD5`), `DS:1F7C` activity word (written by garage/race code: 1 driving, 2 tune screen, 4/8/10h animations,
read by `adlib_vec_on`), `DS:72E0` squeal flag and `DS:7BA2` off-road flag (written by race), `DS:A10E` rpm, `DS:AA02`+1Eh car word, `DS:B8D4`,`AD5C`,`B866`,`1BCA` (animation
player values), `DS:5300`/`5304` (read by `ui_wait`/`game_loop`/`wait_click_or_key` for the jukebox toggle: button id -40 in `game_loop`/`music_stop`, id -90 and key `M` elsewhere; toggle =
`if (DS:5304 == 0) { DS:5304 = 1; music_resume(); } else { music_stop(); DS:5304 = 0; }`, only when `DS:5300 == 1` and not driving).

---

## 9. Open questions

1. **Game tick vs music (1.4).** With AdLib music the original game logic runs at 68.6 Hz / 60.0 Hz instead of 72.82 Hz. A faithful port should reproduce the accumulator rule;
   needs a U-check (race speed with/without music, DOSBox) before deciding. The SR1 port ticks at a fixed 72.82 Hz.
2. `FUN_0001_a51c` is only named by its effect (ending/epilogue picture 0x43B, plays list B then list A); the game_flow spec should name it.
3. Slot 3, slot 20 and AdLib `1072`/`10b6` have no callers; `DS:5302` (hold) and the note-6000 override are dead with the shipped data. Kept in the spec for completeness.
4. `music_stop/resume` search the active button table for id -40 without a bound; if the active screen has no jukebox button the original would run away (never observed).
5. The meaning of the car word at `DS:AA02`+1Eh (selects the loud/idle engine variant) and of `DS:B8D4` should be taken from the race spec.
6. Exact sound device behaviour on `Ctrl` (sound toggle) while AdLib music plays was derived from `timer_isr`/`race_isr`/`adlib_vec_*` code only (no run-time check).
7. **Playlist transition stall** (7.2 item 3): derived from the ISR/`mus_tick` code only (`cs:[24]` wraps after a raw first delay of 0). If a DOS run shows music continuing right away, the cause is not in these two functions. U-check wanted: listen to the title music for > 3 minutes (DOSBox, AdLib).
