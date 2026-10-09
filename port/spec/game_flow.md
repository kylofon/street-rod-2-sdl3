# game_flow — Street Rod 2 (1991), delta spec

Scope: everything around the garage and the races that is not the garage / race / video / sound / platform
internals: `main` after the hardware set-up, title sequence, copy protection, the new-game / load-game
loop, the garage dispatch loop `game_loop` 0001:4dd6 and its return codes, the game clock and calendar,
money and the status line, pools and new game, save / load (`?:HOTROD0..14.SAV`), hall of fame
(`?:hall_dat`), game over / ending, demo mode, and the new SR2 event trips: Burger's Drive-In (code 2, race
module), the gas station (code 15, `event15_run` 2925:2104), Grudge Night tournament (code 17, `event17_run`
2925:1970) with its bracket / screens, and the `SR2_end` ending player (2925:2714 / 27da).

SR2 segments: `0001` (main game segment, SR1 `0000`, address of SR1 + 0x10 at run time: see RE_GUIDE 3),
`1a21` (library, SR1 `0f38`), `2925` (new game code), a few callees in `0dcb`.
Builds on `../StreetRod/port/spec/game_flow.md` (SR1 spec, "SR1 §x" below) and `platform.md` (`ui_wait`).
Function counts (specified here): 99 functions = 2 same (by reference) + 54 changed (delta text) + 43 new (full text), plus 11 handoffs;
of the 72 functions that `port/sr2_modules.csv` lists under game_flow 61 are specified here and 11 are handed off, 38 more were claimed (see below). The split is in `game_flow_symbols.csv` (notes column).

Claimed beyond `port/sr2_modules.csv` (the neighbour vote put them in garage / race; they are SR1 game_flow or
the event trips named by the task): `pools_init`, `garage_full_check`, `part_alloc/free`, `car_alloc/release`,
`opponent_init`, `opponents_init`, `new_game`, `new_game_prompt`, `str_rtrim`, calendar/clock group
(`day_index` … `broke_check`), `drive_to_event`, `event15_run` + its gas-station helpers 2925:1ab8…20be,
`event17_run` + `000e/00fa/01fa/193e`, `cheapest_ad_price` 0001:0e94. Listed in my csv but belonging elsewhere
are in "Handoffs". If the garage/race specs also contain a claimed function, S3.7 keeps either.

Conventions: `SSSS:OOOO` as stored in the file; `DS:xxxx` = DGROUP 0x3E08. `W[x]`/`B[x]` = word/byte at DS:x.
Picture ids: `id - 1000` = LIB2 index (SR1 convention, e.g. 0x3E9 = LIB2 #1), ids >= 2000 = LIB3.
Message ids: `msg_box(id)`, see "Messages" (the SR2 message table is at DS:11EA, 6 bytes per id).
"Driver vector N" = far pointer `DS:A0AE + 4N` (video spec); "sound vector N" = `DS:BB8C + 4N` (sound spec).

## State diagram: SR2 game loop vs SR1

```
SR1 (0000:503f)                                   SR2 (0001:4dd6)
-----------------------------------------------   -----------------------------------------------------------
main                                              main (07b0): + adlib detect/menu, SR2_data/hall/MUS loads
 └ game_loop                                       └ game_loop
    code = title_and_setup()  (-1)                    code = title_and_setup()  (-1)
       title_sequence 3 pics, protection SKIPPED         title_sequence 3 pics (+2 waits), copy_protection 3378
                                                         (asks, NEVER checks), road seed init 0dcb:2866(1)
    ┌─ new_or_load ◄────────────────────┐             ┌─ new_or_load ◄────────────────────┐ (same shape)
    │  -1 → new_game_prompt (licence)   │             │  -1 → new_game_prompt (licence+trim)
    │  else → load_game_screen          │             │  else → load_game_screen (slots 0..14, 0x38 header)
    │  -0x29A → code=-1 (new game)      │             │  -0x29A → code=-1
    │  0 → leave                        │             │  0 → leave
    └─► newspaper (new game only)       │             └─► newspaper_front (new game only; ads inside)
    garage loop: code = garage_screen(redraw)         garage loop: code = garage_screen(redraw)
      0x29A game over → summer_over_screen?             0x29A game over → summer_over_screen if W[1BC6]==1,
         hall_of_fame_screen, game_over_menu               hall_of_fame_screen(W[1BC6]&2), game_over_menu 348a
      1  newspaper                                       1  newspaper_front
      2  Bob's Drive-In (drive 8d26, DS:0284=2)          2  Burger's Drive-In  (drive_to_event, DS:1BC8=2)
      15 gas station  (DS:0284=3)                        15 gas station = event15_run 2925:2104 (DS:1BC8=3)
      17 "Wanna drink?" → msg 0x18F6 only                17 GRUDGE NIGHT tournament = event17_run 2925:1970
                                                            (Wednesday only: day_index()%7==2; entry $30)
      3 calendar  4 quit_menu  5 sell parts             3 calendar  4 quit_menu (+confirm)  5 sell parts
      6 cars 7 paint 8 tires 9 transmission             6 cars 7 paint 8 tires 9 transmission
      10 hood  11/12/13 customize  14 stickers          10 hood (+1a21:9b26)  11/12/13 customize
      16 car info                                        14 stickers_menu_sr2 (2925:3174)
      -40 juke box (DS:58D0)                             16 car_info_sr2 (0001:72f2: mph / gallons)
                                                         -40 juke box toggles DS:5304 (music_resume/stop)
    quit_menu: Save -3, Old -2, New -1, Quit -20,     quit_menu 630e: Save -0x17, Old -2, New -1, Quit -0x14
               Forget -22                                           (+confirm menu 0x23 "-0x15 = back"), Forget -0x16/-4
    win: Bob's → ending_sequence a544 (long)          win: bob_drive_in a626 → ending_trigger a51c → 2925:27da
                                                          (SR2_end script player) → DS:1BC6 |= 2 (race code)
    demo: kbd scancode injection every 128 ticks      demo: cursor-driven (1a21:4806 picks a menu element, ISR moves
                                                          the mouse cursor there and injects a click); Q quits
```

Return-code/flow notes common to both: `again` (=1 after title / game over, 0 inside the garage loop) makes the
new/load loop run once when entered from the garage; a failed load (-0x29A) forces the new-game screen.

## Messages used here (SR2 table DS:11EA: `{u16 box_type, near line1, near line2}`, box 0/1 = one line, 2 = two lines)

| id | text | id | text |
|---|---|---|---|
| 0x03 | Not until Wednesday! | 0x25 | You've got a full tank! |
| 0x09 | You've got no car, dummy! | 0x26 | Gas ain't free! |
| 0x22 | Sell some cars first! | 0x27 | Your car's not running, speedy! |
| 0x23 | Sell some parts first! | 0x28 | Pick a car first! |
| 0x24 | You don't got enough gas to cruise down / to Burger's. | 0x45 | Yo, you need full service, dummy! |
| 0x46 | It's not full, speedy! | 0x60 | Looks like ya took too much time, jr! / You're done for the summer. |
| 0x62 | Use another pump, dummy! | 0x63 | Get some gas first, speedy! |
| 0x64 | Take a pump, dummy! | 0x65 | Get Your breaking time (sic) |
| 0x66 | Your breakout time is   .  / Run any faster and you will be disqualified. | 0x67 | You won round   with a time of   .  / You advance to the next round. |
| 0x68 | You won round   with a time of   .  (final) | 0x69 | You lose the race by   .  seconds. |
| 0x6A | (0x69) + You're out of the running for the big money. | 0x6B | You beat some of the best drivers. / Your number ? finish earns you $   . |
| 0x6C/0x6D | You crossed the center line. You lose. (+out of the running) | 0x6E/0x6F | You ran faster than your breakout time. (+out of the running) |
| 0x70 | Drivers stage your vehicles! | 0x71 | Your breakout time is too weak. (+out) |
| 0x72 | You have no money for Grudge Night stake! | 0x79 | Opponent ran faster that his breakout time. / You've won this round! |
| 0x44 | I knew you could do it! You're my hero! (after the ending; race code) | | |

## Globals

SR1 → SR2 address rules that cover most of this module: `DS:05xx/06xx` (timer, clock, demo) → `+0x196E`
(`05F8 ticks → 1F66`), `DS:82xx` (video state: `8240 draw_through`, `8242..8245` colours, `8250..8258` status line) →
`+0x2AE6` (`8240 → AD26`), `DS:7Exx/7Fxx` player block → see table. SR1 `DS:02xx` flow flags moved `+0x1944`
(`0282 → 1BC6`, `0284 → 1BC8`).

### Player state (saved) — header `DS:A9D8..AA0F` (0x38 bytes) is written verbatim

| SR2 DS | type | meaning | SR1 DS |
|---|---|---|---|
| A9D8 | char[16] | **save name** (slot label, ≤15 chars shown; typed in the save screen; NEW) | (SR1: the only name, 7E9A) |
| A9E8 | char[16] | **driver (licence) name** `player_name`; hall of fame / tournament lists use it | 7E9A |
| A9F8 | u32 | road generator seed (`0dcb:2866(1)` stores `DS:4A84:4A86` here; `2866(0)` restores it); NEW | – |
| A9FC | i32 | `money` | 7EAA |
| AA00 / AA01 | u8, u8 | player skill values 1..8 (both 1 on a new game); changed by Grudge Night (event17) and read by the race code; NEW | – |
| AA02 | u16 | `cur_car` (near ptr into car pool, 0 = none) | 7EB0 |
| AA04 | u16 | `car_list` (other cars, chained by `+0x28`) | 7EB2 |
| AA06 | u16 | `car_freelist` | 7EB4 |
| AA08 | i16 | `car_count` (max 16) | 7EB6 |
| AA0A | u16 | `spare_parts` (chain via `+6`) | 7EB8 |
| AA0C | u16 | `part_freelist` | 7EBA |
| AA0E | i16 | `part_count` (limit `>= 0xB5` = full) | 7EBC (limit 0x83) |
| AEC2 | 16 × 0x2A | `car_pool` (SR1 83B0, 16 × 0x28); free-list link at `+0x28`, `+2 = 0xFFFF` when free | 83B0 |
| B1BE | 0xBE × 8 | `part_pool` (SR1 866A, 0x8C × 8); link at `+6`, `+4 = 0xFFFF` when free | 866A |
| 479C … 47A6 | 6 × u16 | counters (SR1 4970..4976, 8 bytes): 479C races run (`king_status` tests `>>1`), 47A0 Mulholland/road wins, 47A2 drag wins, **47A4 Grudge Night wins**, 479E / 47A6 unknown; all 0 on a new game | 4970 |
| AB7E | 22 × 0x12 | people / opponents (SR1 7FF8, 22 × 0x12): `+6` name ptr, `+8` model, `+0A` colour, `+0B` beaten (0xFF), `+0D` flags (3 bumpers, 4 roof, 0x20 new), `+0F`, `+10` skill (7-(b>>1), 6-(a>>2)); #21 = the King (`+0B` at AD03) | 7FF8 |
| 1F6A | u32 | `game_clock` (0x222 = 546 per game hour) | 05FC |
| 1F72 | u32 | `rt_base` (`rt_ticks − elapsed` on load) | 0604 |
| 7DFE | u16 | passed to `0dcb:24d8` after load (100 on new game); saved | 5E0C |
| 9B40 | 0x80 | **new saved block** (garage / classifieds state: `9B40 = 0xFFFF`, `9B42..` = 0xFF-terminated list of ad car indexes (bit 7 = sold), `9B5C..9B85` 42 bytes cleared, `9B86 = 0x2A`); initialised in `new_game`, content is the garage spec's | – |

### Flow state (not saved)

| SR2 DS | type | meaning | SR1 DS |
|---|---|---|---|
| 1BC6 | u16 | `game_over` bits: 1 summer over, 2 King beaten, 4 debts, 0x10 broke | 0282 |
| 1BC8 | i16 | `location`: 0 idle, 1 garage (return trip), 2 Burger's, 3 gas, 4 Grudge Night trip, 6 jail | 0284 |
| 1F3A | u32 | `wait_until` (0 = off) | 05CC |
| 1F3E | u16 | `wait_any_key` (`ui_wait` returns 0x3EB on any key / click) | 05D0 |
| 1F40 | u16 | key / state returned with 0x3EB | 05D2 |
| 1F4A | i16 | `msg_ticks` = 0x15E | 05DC |
| 1F4C | u16 | `click_returns` (click ends `ui_wait` with 0x3EB) | 05DE |
| 1F66 | u32 | `ticks` (72.8 Hz) | 05F8 |
| 1F6E | u32 | `rt_ticks` | 0600 |
| 9730 | u16 | `demo` (runtime; = `B8D0`) | 0610 |
| B8D0 | u16 | `sw_demo` command-line switch | 8BCE |
| 986C | u16 | `title_resident` (title pics cached) | 70E2 |
| 5304 | u16 | `music_on` (juke box; also DS:5300 = sound present, A2B2 = ?) | 58D0 |
| 4C58 | u16 | cleared after game over (car drawing cache) | 4F4A |
| A61C | u16[16] | list-box entries (near ptrs to 16-byte labels) | 49E0 |
| 8FC8 | 15 × 16 | save-slot labels (`8FC8 + 16n` = first 15 chars of the header name) | 4618 |
| 90B8 | u8[15] | `slot_present[n]` = n, or 0xFF if HOTRODn.SAV absent (SR1: 4FC2, 1-based, 0 absent) | 4FC2 |
| 22EA | char[16] | save-name edit field | 2A2E |
| 9AE0 | i16 | `hall_count` (0..10) | 7646 |
| ADDC | 10 × 23 | `hall` records: `{char name[16]; char hours[7]}` | 82CA |
| 8842 | i16 | `hall_new_score` | 6C5E |
| 4922 / 491A | u16[] | resident picture list (176 ids, 0-terminated) / title list `{3,1,4,0}` | 4C28 / 4C20 |
| 2362 / 2334 / 2341 | | copy-protection edit buffer / prompt text / ordinal suffix chars | – |
| 20EA / B196 | i16 | demo cursor target x / y (20EA = −1 clicked, −2 = choose next) | – |
| 2110 / 2132 / 20EE | | demo garage shuffle bag: used flags `W[2110+2i]` (17), remaining count (17), button codes `{2,17,1,11,6,14,13,8,4,5,9,10,7,16,12,15,3}` | – |
| 2148 / 2134 | | demo script index / script for menu 12: `{1,2,1,2,-2,2,1,2,1,-1}` | – |
| 2164 / 214E | | demo script index / script for menu 22: `{6,5,3,1,0x3EB×4,1,3,0}` | – |
| 2166 / 214A / 214C / 20EC | | demo: previous menu / counters | – |

### Tournament (Grudge Night) state

| DS | type | meaning |
|---|---|---|
| 90CA | 10 × 6 | table `T[i] = {i16 id (−1 = player), i16 state, i16 dial_in_time}`; i = 0..8 opponents, 9 = player before sorting |
| 90C8 | i16 | table index of the opponent of the current round |
| 9106 | i16 | table index of the player |
| 9100 / 9102 / 9104 | | the unsorted player entry `{-1, 0, B8C2}` |
| 9B3E | i16 | 1 = player alive / qualified, 0 = out |
| AD6C | i16 | round index −1 (qualifier), 0..3 |
| B8C2 | i16 | measured time in tenths of a second (race code), also the player's dial-in after the qualifier |
| ADA8 | i16 | player's dial-in (copy of the qualifier time) |
| 9ADC | i16 | opponent's dial-in for this round (`T[90C8].time`) |
| B18E / A9C3 | u16 / u8 | race-code reference time / constant used to scale opponent dial-ins |
| 9108/910A, 9110/9112, 9114/9116, 910C/910E | far ptr | saved bitmaps: background pic, face, panel, photo |
| 4FB6 | u16[3] | prizes 150, 100, 50 ($) |
| 4FBC | rect | clip rect `{x0=0x18, y0=0x1E, x1=0x11F, y1=0x4F}` for the wipe |

## Functions

Order: segment `0001`, `0dcb`, `1a21`, `2925`, each by address. "Δ" = real behaviour change vs SR1.
Driver vectors (`DS:A0AE + 4N`, same numbering as SR1 `DS:78A2 + 4N`): 3 = text, 7 = SR1 `DRV_78BE` (`A0CA`), 8 = SR1 `78C2`
(`A0CE`, show), 10 = SR1 `78CA` (`A0D6`, page copy), 0 = rectangle blit list (`A0AE`).

### 0001:07b0 main  (changed, SR1 `0000:066f`, SR1 §4.1)
`void far main(int argc, char **argv)`. Differences to SR1:
* switches (strings DS:0212…): `1` → drives = 1 (default 2); `nomouse` → `W[020E]=0` (default 1; SR1 0262); `demo` → `W[B8D0]=1`;
  `nouemem` → `W[0210]=1` (SR1 0280); video override `e`=2 EGA, `c`=1 CGA, `h`=4 Hercules, `v`=3 VGA, `t`=5 Tandy → `video_detect_menu(mode)`.
  No `auto` switch (the self-driving flag `DS:1F82` stays 0).
* order: `0001:07ae` (empty far stub), `B[AD2D]=0`, `W[B7B4]=cpu_speed_calibrate()` (SR1 `8ACC`), `W[1F82]=0`, switches,
  `mode = video_detect_menu(forced)` 037c, `snd = adlib_detect()==0 ? 1 : sound_options_menu()` (key 1 → 0 AdLib, key 2 → 1 speaker, other → 0),
  `mem_pools_init()`, `driver_select(mode)`, `bios_set_mode_for(mode)` (0001:0628), `gfx_init(0, 0x471C, 0x12, 0x471C)`, `arena_init()`,
  `platform_install()`, `snd == 1 ? speaker_sound_init() : adlib_sound_init()`, on EGA/Tandy (`W[AD1C] == −2 || −6`) `ega_set_palette(DS:1DA6)`,
  `lib_open_all(drives, mode)`, `slow_flag_set()`, **`rnd(−1)`** (seeds the RNG from the clock, see `rnd`), `lib_read_dir(1)`, `cursor_init()`, `gfx_screen_mode(0)`,
  on VGA with `W[01FC] != 0`: `DS:9AD4 = bitmap_alloc(0x140, 100, 0, −1, 0, 0)` (SR1 755C); **no** picture work-buffer `halloc`.
* `r = pic_load_list(DS:491A /* {3,1,4,0} */, 0)`; `r == 3` → `W[986C]=1; lib_read_dir(2); if (W[0050]==0) mus_songs_load(); hot_data_load(); hall_load();
  if (driver == −2) pic_park_list(DS:4922); pic_load_list(DS:4922, 1)`; else `W[986C]=0` (nothing freed).
* then `wait_click_or_key(1)`, `game_loop()`, `platform_exit()`.
DS remap: SR1 0262→020E, 0280→0210, 8BCE→B8D0, 70E2→986C, 4C20→491A, 4C28→4922, 755C→9AD4, 0250→01FC; `W[0050]` = sound device (0 AdLib, 1 speaker).

### 0001:0e94 cheapest_ad_price  (new, helper of `broke_check`)
`int far cheapest_ad_price(void)`: `min = 10000`; for `i = 0; (b = B[9B42+i]) != 0xFF; i++`: skip if `b & 0x80`; price = `b < 0x15 ? W[A80C + 0x14·b] : W[A66A + 0x14·b]`
(the latter `= W[A80C + 0x14·(b−21) + 2]`, the model's second price field); `min = min(min, price)`. Returns 0 if the list is empty, else `min` (DX = sign extension).
The list `DS:9B42` is the classified-ad state of the saved block `9B40` (garage spec).

### 0001:17ea input_reset  (changed, SR1 `0000:0e24`, platform SR1 §4.5)
As SR1 minus the tick-timer queue: wait until 10 ticks (`W[1F66]+10`) pass without a key (loop `while (key_read()) n++`; `n != 0` restarts the 10-tick window); then
`1F3A:1F3C = 0` (wait_until), `if (W[9730]==0 && B[1F60]) { mouse_poll(); W[1F54]=W[1F56]=0; }`, `B[3B3E]=0`, clear `DS:3BE0..3BEF` (16 bytes key state), `B[3B36..3B39]=0`, `B[3B3A]=0`.
DS remap: 0610→9730, 05CC→1F3A, 05F8→1F66, 05F2→1F60, 05E6/05E8→1F54/1F56, 4736→3B3E, 47D8→3BE0, 472E..4732→3B36..3B3A.

### 0001:18ce platform_exit  (changed, SR1 `0000:0fea`)
`void far platform_exit(void)` (does not return): `if (W[9730]) fatal_message(DS:1F84 "Quit demo"); arena_low_shrink(); if (W[0044]) timer_restore(); if (W[004E]) sound_vec(21)();
if (W[0048]) { driver_vec(8)(); video_restore(); } exit(5)` (`34b0:01d5(5)`).

### 0001:1d02 ui_wait  (changed, SR1 `0000:1417`, platform SR1 §4.5)
Returns 0 after `ticks_max` ticks, 1000 when `wait_until` expires, 0x3EB for "any key/click" waits, otherwise a menu / hot-key code. SR2 has no tick-timer queue, no
palette rotation and no special screens 0x16/0x25.
```c
int ui_wait(int ticks_max)
{
    timed = (i32)W32[1F3A] > (i32)ticks;                  /* SR1: wait_until != 0 */
    until = ticks + (i32)ticks_max;
    for (;;) {
        if (W[9730] && W[20EA] == -2) demo_step();        /* 1a21:4806 */
        k = (i8)key_read();
        if (!W[9730]) {
            if (k < 0) {
                k += 0x32;
                if (k != -0x28) return k;                 /* per-screen hot key */
                if (W[1F3E]) { W[1F40] = 0x4D; return 0x4D; }
                if (W[5300] == 1 && W[A2B2] == 0) { if (!W[5304]) { W[5304] = 1; music_resume(); } else { music_stop(); W[5304] = 0; } }
            } else if (k > 0) {
                if (W[1F3E]) { W[1F40] = k; return 0x3EB; }
                if (k == 0x7F) platform_exit();
            }
        } else {                                          /* demo */
            if (k == 'Q') platform_exit();
            if (k == -0x5A && W[5300] == 1 && W[A2B2] == 0) toggle the music as above;
        }
        if (!W[9730] && B[1F60]) {                        /* mouse */
            mouse_poll();
            if (W[1F50] || W[1F52]) W[881C] |= 1;
            if (W[881C]) { W[3B32] += W[1F50]; W[3B34] += W[1F52]; }   /* cursor x, y */
        }
        if (!B[3B3E] && !B[3B3F]) {                       /* no click event */
            if (timed && (i32)W32[1F3A] < (i32)ticks) { W32[1F3A] = 0; return 1000; }
        } else {
            if (W[9730] && W[20EA] == -1) W[20EA] = -2;
            B[3B3E] = B[3B3F] = 0;
            if (W[1F4C]) { W[1F40] = 0; return 0x3EB; }   /* click_returns */
            if (W[B87C]) {                                /* a menu is active */
                id = W[B898];
                if (!(W[B87E] <= W[3B40] && W[3B40] < W[B880] && W[B882] <= W[3B42] && W[3B42] < W[B884]))
                    id = hotspot_find(W[3B40], W[3B42]);
                if (id) return id;
            }
            if (W[1F3E]) { W[1F40] = 0; return 0x3EB; }
        }
        if ((i8)B[3B3B] > 0 && W[881C]) {                 /* cursor drawn and moved */
            W[3B32] = clamp(W[3B32], 0, 0x13A); W[3B34] = clamp(W[3B34], 0, 0xC2);
            W[881C] = 0; cursor_redraw(W[3B32], W[3B34]);
            if (W[B87C] && cursor outside [B87E,B880) × [B882,B884)) {
                prev = W[B898]; id = hotspot_find(W[3B32], W[3B34]);
                if (id == 0) { if (prev) status_print(-1, DS:1FFA /* "" */, 0); }
                else if (id != prev) status_print(-1, W[W[B896] + 2] /* element text */, 0);
            }
        }
        if (until <= (i32)ticks) return 0;
    }
}
```
Key codes: positive = ASCII, `k < 0` = hot key (`k + 0x32` is the menu code, −0x28 = juke-box key `M`). `B87C` menu depth, `B87E/B880/B882/B884` cached free rectangle (x0,x1,y0,y1), `B898` code under the
cursor, `B896` its element, `3B40/3B42` click position, `3B32/3B34` cursor.

### 0001:3245 kbd_inject_scancode  (changed, SR1 `0000:2e93`)
Not the demo player: it applies the keypad-emulated mouse state to the emulated cursor (called from the keyboard ISR path, no direct callers). `c = B[3B6E]` (the mapped code):
`if (c != 0) { if (c <= 0 && c > −0x1F) { if (!B[3BF0+c]) { B[3BF0+c] = 1; B[3B36] += B[3B6E+c]; B[3B37] += B[3B6F+c]; W[3B38] = W[3B6E+c]; B[3B48] = 0; } }
else if (c == 0x1F) { if (!W[3B3A]) { B[3B3E] = 0x1F; W[3B40] = W[3B32]; W[3B42] = W[3B34]; B[3B48] = 0; } W[3B3A] |= 1; }
else if (c == 0x1E) { if (!W[3B3A]) { B[3B3F] = 0x1E; W[3B44] = W[3B32]; W[3B46] = W[3B34]; B[3B48] = 0; } } else if (c == 0x1D) B[3BD5] ^= 1; }`
(`3B3E/3B3F` = left / right click pending, `3B36/3B37` = direction increments). SR1 DS:47CC → 3B6E.

### 0001:3378 copy_protection  (changed, SR1 `0000:2fc8`, SR1 §4.12)
`int far copy_protection(void)` — a manual-lookup question; **the answer is never checked**; always returns 1.
```c
cursor(−2); driver_vec(10)(front, back copy);                     /* black screen */
if (driver EGA/Tandy) ega_set_palette(DS:48D8); else driver_vec(8)();
W[AD2F] = 1;  W[AD26] = 1;
i = rnd(0x18);   e = DS:4878 + 4·i;      /* 24 entries {u8 n (word number), u8 page, u8 line, u8 letter} */
sprintf(DS:2334, DS:4848 /* "What`is`the`%d  `letter`in`line`%d`on`page`%d?" */, e.n, e.line, e.page);
DS:2341/2342 := n==1 ? "st" : n==2 ? "nd" : "th";                 /* ordinal suffix */
B[2362] = 0;
r = ui_edit(0x1F, 1, DS:2362);                                    /* menu 0x1F, edit buffer */
if (r == −0x14 /* OK */) { *(u16 *)(e + 3) = 1; W[2362] = 1; }    /* no comparison with e.letter */
status_print(−1, DS:4877 /* "" */, 0);
return 1;
```
Table (n, page, line, letter): 3,3,1,R · 4,3,4,E · 3,3,5,M · 4,8,2,N · 5,8,3,V · 2,8,6,O · 4,10,1,K · 2,10,3,L · 3,10,4,Y · 1,12,4,T · 4,12,5,K · 1,12,6,X ·
4,16,1,L · 1,16,2,F · 1,16,3,S · 4,19,1,C · 4,19,2,U · 1,19,3,P · 1,20,1,R · 5,20,3,W · 3,20,4,A · 1,28,1,B · 1,28,2,H · 2,28,4,O.
Quirk: the `jne` after `ui_edit` (0001:345C) targets 0x346B, the middle of the next instruction (`06 62 23 01 00` = `push es; bound sp,[bp+di]; add [bx+si],ax`), so any result other than
−0x14 (cancel) leaves the stack unbalanced and crashes at `retf`. Faithful port: show the prompt, run the edit, return 1 whatever the result. `title_and_setup` calls it unless `sw_demo`.

### 0001:348a game_over_menu  (changed, SR1 `0000:315b`, SR1 §4.9)
As SR1 with: `pal_reg12_13(DS:1DDA)` and `status_init(0xBE)` first; colours saved/restored in `AD28..AD2B` (SR1 8242..8245; EGA/Tandy: fg 0xF, bg 1, line 0xD, fill 3; else 3,0,2,1);
`W[AD26]=1`; `screen_fill_rect(0,0,0x140,0xBE,bg)`; double frames (100,0x32,0x78,0x5A) and (0x66,0x34,0x74,0x56) on both pages (`draw_box1`); `cursor(−2)`; `driver_vec(8)()`;
**`input_reset()`**; `r = ui_menu(0x20)` (SR1 0x29: Load Game −2, New Game −1, Quit 4); `cursor(−4)`; return r. No reload of the resident pictures (SR1 `4F4C` is gone).

### 0001:35e8 hall_is_slower / 360c hall_insert / 3682 hall_score  (changed / changed / same)
= SR1 `hall_is_slower` (`0000:32d0`), `hall_insert` (`32f4`), `hall_score` (`3375`), SR1 §4.9. DS remap: `hall_new_score 6C5E → 8842`, `hall_count 7646 → 9AE0`, hall records `82CA → ADDC`
(23 bytes: name[16], hours[7]). `hall_insert` copies the **driver name `DS:A9E8`** (SR1 7E9A) and `itoa(8842)`. `hall_score` = `day_index()·24 + hour + 12` (`clock_hm`), code identical.

### 0001:36ac hall_of_fame_screen  (changed, SR1 `0000:339f`, SR1 §4.9)
Same algorithm. Differences: picture **0x4B9** (LIB2 #209; SR1 LIB1 #4) via `show_picture(0x4B9, −1)` at clip height 200 (`W[1E44]`); `driver_vec(7)()` first; palette swap `pal_shadow_swap()`;
colour scheme pointer `W[9DC6]` (SR1 787E): saves `scheme+0x28/0x2A/0x2C` and `scheme+0x23/0x25/0x27` and, on EGA/Tandy, overwrites them from `DS:4902..4906` (`03 05 05 04 01`) and `DS:4908..490C` (`03 04 04 03 01`);
text colours `AD28 = 4, AD29 = 0` (EGA/Tandy) else `3, 2`; row y = `W[48EA + 2i]` (= 0x3B + 11·i, 10 rows), name at x = 0x2E, hours right-aligned (`x = 0xA1 + 6k`, k = number of ×10 steps until the value reaches 1000),
"hr" (DS:48FE) at x = 0xBF; `page_copy_rect(0x2E, W[48EA], 0xD3, W[48FC] + 8)`; no new row (`row < 0`) → `wait_click_or_key(700)`, else the name is edited with `text_edit(0x2E, W[48EA+2row], tmp, 15)` (`1a21:5fb8`),
copied back and `hall_save()`. A new row is only made when the score beats an entry or the table has < 10 rows.

### 0001:3934 pools_init  (changed, SR1 `0000:3614`, SR1 §4.6)
`void near pools_init(int full)`: `car_freelist(AA06) = AEC2`; for the 16 cars at `c = AEC2 + 0x2A·i`: `W[c+0x28] = c + 0x2A; W[c+2] = 0xFFFF`; last `W[c+0x28] = 0`;
`part_freelist(AA0C) = B1BE`; for the 0xBE parts at `p = B1BE + 8i`: `W[p+6] = p + 8; W[p+4] = 0xFFFF`; last `W[p+6] = 0`; `part_count = car_count = 0`;
`if (full) { cur_car = car_list = spare_parts = 0; money = 0; }` (no `7EAE` store).

### 0001:3e08 garage_full_check  (changed, SR1 `0000:3b0c`)
`if (car_count >= 16) { msg_box(0x22); return 1; } if (part_count >= 0xB5) { msg_box(0x23); return 1; } return 0;`

### 0001:3f28 part_alloc / 3f3e part_free / 423a car_alloc / 4250 car_release  (changed)
= SR1 `part_alloc 3c1b`, `part_free 3c3e`, `car_alloc 3fb1`, `car_release 3fd4` (SR1 §4.6). DS remap: `part_freelist 7EBA → AA0C`, `part_count 7EBC → AA0E`, `car_freelist 7EB4 → AA06`,
`car_count 7EB6 → AA08`; car link `+0x26 → +0x28`; part link `+6` and `+4 = 0xFFFF` unchanged.

### 0001:3cc4 title_and_setup  (changed, SR1 `0000:39c0`, SR1 §4.2)
`int near title_and_setup(void)` returns −1.
```c
gfx_screen_mode(0);
sound_vec(18)();                       /* title music, DS:BBD4 (SR1 tune_start(2,10)) */
title_sequence();                      /* 1a21:0e82 */
driver_vec(7)();  gfx_screen_mode(1);
if (!W[B8D0]) { if (copy_protection() == 0) platform_exit(); cursor(−4); }     /* SR1: skipped */
road_seed_init(1);                     /* 0dcb:2866(1): new road seed, stored at DS:A9F8 (SR1 f2645_0d48) */
B[A9D8] = 0;  B[A9E8] = 0;             /* save name and driver name := "" */
W[9730] = W[B8D0];                     /* demo */
if (!W[986C]) {
    lib_read_dir(2); if (W[0050] == 0) mus_songs_load(); hot_data_load(); hall_load();
    if (driver == −2) pic_park_list(DS:4922);
}
pic_load_list(DS:4922, 1);  cursor(−2);  return −1;
```
Δ: no `rnd(−1)` here (done in `main`), `hall_load` is called explicitly (SR1 `hot_data_load` called it), `mus_songs_load` for AdLib.

### 0001:3d80 wait_ticks  (changed, SR1 `0000:3a83`, SR1 §4.5)
= SR1: `1F3A:1F3C = ticks + (i32)n; W[1F3E] = 1; ui_wait(30000); 1F3A:1F3C = 0; W[1F3E] = 0`. DS remap 05CC→1F3A, 05D0→1F3E, 05F8→1F66.

### 0001:3db4 msg_box  (changed, SR1 `0000:3ab8`)
`saved = 1F3A:1F3C; msg_draw(id) /* 1a21:2530 */; ui_push(6) /* SR1 0x18 */; wait_ticks(W[1F4A] /* 0x15E */); msg_erase() /* 1a21:27d2 */; ui_pop(1) /* 4cac */; 1F3A:1F3C = saved`.
`msg_draw(id)`: table `DS:11EA + 6·id = {u16 box_type, near line1, near line2}`; box rectangle table `DS:1E46 + 0x12·type` (`+2` line count); negative id: draw then erase at once.

### 0001:4dd6 game_loop  (changed, SR1 `0000:503f`, SR1 §4.3/§4.4)
`int near game_loop(void)`; variables as SR1 (`again`, `code`, `prev`, `redraw`). SR2 version (Δ vs SR1 noted):
```c
again = 1;  code = title_and_setup();
new_or_load:
do {
    prev = code;
    if (code == −1) { again = 1; code = new_game_prompt(); } else code = load_game_screen();
    if (code == −0x29A) { code = −1; again = 1; }
} while (code != 0 && again);
status_label(W[4844] /* "`Bankroll:`$" */);                                  /* SR1 status_line */
redraw = 1;
if (prev == −1 && car_count < 16 && part_count < 0xB5) newspaper_front();    /* Δ: no classifieds() call here, limit 0xB5 */
again = 0;
for (;;) {
    code = garage_screen(redraw);
    if (code == 0) goto new_or_load;
    if (code == 0x29A) {                                                      /* game over */
        if (W[1BC6] == 1) summer_over_screen();
        hall_of_fame_screen(W[1BC6] & 2);
        cursor(1); W[1BC6] = 0; again = 1;
        code = game_over_menu();
        if (code == 4) return;                                                /* Quit */
        cursor(−2); W[4C58] = 0; goto new_or_load;                            /* −1 new, −2 load */
    }
    redraw = 2;
    switch (code) {
    case 1:  if (garage_full_check()) { redraw = 0; break; }
             if (!cur_car) redraw = 1;
             newspaper_front();
             if (!cur_car) redraw = car_list ? 1 : 2;
             game_clock += 0x222; continue;                                   /* no redraw = 0 */
    case 2: case 15: case 17:  /* event trips, below */
    case 3:  calendar_show(0); redraw = 0; break;
    case 4:  redraw = 0; r = quit_menu();
             switch (r) { case −0x17: if (garage_full_check()) { redraw = 0; continue; }
                                      save_game_screen(); redraw = 2; if (!cur_car && car_list) redraw = 1; continue;
                          case −0x16: case −4: continue;                      /* Forget it */
                          case −0x14: return;                                 /* Quit */
                          default: code = r; goto new_or_load; }              /* −1 New, −2 Old */
    case 5:  sell_spare_parts(); redraw = 0; break;
    case 6:  redraw = your_cars(); continue;
    case 7:  paint_job(); redraw = 0; break;
    case 8:  change_tires(); redraw = 0; break;
    case 9:  change_transmission(); continue;
    case 10: ui_reset_fx() /* 1a21:9b26 */; engine_bay(cur_car); continue;
    case 11: customize(2); redraw = 0; break;   case 12: customize(1); redraw = 0; break;   case 13: customize(4); redraw = 0; break;
    case 14: redraw = stickers_menu_sr2(); continue;                          /* 2925:3174 */
    case 16: car_info_sr2(); redraw = 0; break;
    case −40: if (!W[5304]) { W[5304] = 1; music_resume(); } else { music_stop(); W[5304] = 0; }
             redraw = 0; wait_ticks(10); continue;                           /* Δ wait_ticks(10) */
    default: goto new_or_load;
    }
}
```
`break` = `redraw = 0` path of the original (label 4EFB); `continue` = back to `garage_screen` with the current `redraw`.
**Event trips** (codes 2, 15, 17; `garage_full_check()` first — a full garage shows its message and `redraw = 0`):
```c
if (code == 17 && day_index() % 7 != 2) { msg_box(3); redraw = 0; wait_ticks(10); continue; }   /* "Not until Wednesday!" (day 0 = Monday ⇒ day%7==2 is Wednesday) */
if (!cur_car) { msg_box(car_list ? 0x28 : 0x09); redraw = 0; continue; }                       /* Pick a car first / you've got no car */
if (!car_runnable(cur_car, 1)) { msg_box(0x27); redraw = 0; continue; }
car_perf_compute(cur_car, DS:A100);                                                            /* 0001:c294 */
fuel = W[cur_car + 0x24];                                                                      /* 0.1 gal units; SR1 +0x22 */
case 2:  if (fuel > 8)    { W[1BC8] = 2; drive_to_event(); clock(); bob_drive_in(); clock(); redraw = 1; continue; }
         msg_box(0x24); redraw = 0; continue;                       /* not enough gas to cruise down to Burger's */
case 15: if (fuel >= 0x96) { msg_box(0x25); redraw = 0; continue; }                            /* full tank (SR1 0x8C) */
         if (money == 0)   { msg_box(0x26); redraw = 0; continue; }                            /* "Gas ain't free!" */
         W[1BC8] = 3; drive_to_event(); clock(); event15_run(); clock(); redraw = 1; continue;
case 17: if (fuel <= 0x17) { msg_box(0x24); redraw = 0; continue; }
         if (!pay_or_msg(0x1E, 0x72)) { redraw = 0; continue; }                                /* $30 entry stake, else msg 0x72 */
         W[1BC8] = 4; drive_to_event(); clock(); W[1BC8] = 2; event17_run(); clock(); redraw = 1; continue;
```
`clock()` = `game_clock += 0x222` (u32). DS remap: 0282→1BC6, 0284→1BC8, 58D0→5304, 4F4A→4C58, 49DC→4844.
Garage dispatch codes unchanged otherwise (hot keys: N 1, D 2, X 3, Q 4, L 5, C 6, P 7, T 8, A 9, H 10, B 11, F 12, R 13, S 14, G 15, I 16; 17 and −40 are mouse only; the SR2 hover labels
come from the garage spec).

### 0001:5494 str_is_blank  (same, SR1 `0000:5eb3`, SR1 §4.11)
`int near str_is_blank(const char *s)`: 1 if every character is ' ' or '_'.

### 0001:54ba str_rtrim  (new)

> **Secondary view (S3.7):** owned by `garage.md`; on any conflict the owner's section wins.
`int near str_rtrim(char *s)`: if `s != NULL` and `s[strlen(s)−1] != 0`: walk `p = s + strlen(s) − 1` backwards while `*p == ' '`: `*p = 0`, count++, stop at the first non-space (or at the start);
return the number of trailing blanks removed (0 if none / NULL). Used by `new_game_prompt` (trim the licence name) and `save_game_screen` (remember the count to put the blanks back on "Forget it").

### 0001:51f8 opponent_init  (changed, SR1 `0000:542e`, SR1 §4.6)

> **Secondary view (S3.7):** owned by `garage.md`; on any conflict the owner's section wins.
`void far opponent_init(int i, int *models)`; `r = AB7E + 0x12·i`; the `rnd` call order matters:
```c
do { col = rnd(6); } while (col == 5 && rnd(2) != 1);
B[r+0x0A] = col;  B[r+0x0B] = B[r+0x0C] = B[r+0x0D] = B[r+0x0E] = 0;
do { k = rnd(models[0]); } while (models[k+1] == −1);   m = models[k+1];  W[r+8] = m;  models[k+1] = −1;
lvl = (B[A80C + 0x14·m + 4] >> 1) + 1;                                 /* class byte */
if (W[BAB2 + 10m] > 0 && W[BAB4 + 10m] > 0) B[r+0x0D] |= (rnd(0x19) < lvl) ? 3 : 0;   /* both bumper pictures */
if (W[BAAE + 10m] > 0)                      B[r+0x0D] |= (rnd(0x1E) < lvl) ? 4 : 0;   /* roof picture */
B[r+0x0D] |= (rnd(0x14) < lvl) ? 0x20 : 0;                                            /* NEW */
B[r+0x0F] = 7 − (B[r+1] >> 1);   B[r+0x10] = 6 − (B[r+0] >> 2);                      /* NEW: skills from the person's bytes a, b */
```
Δ vs SR1: flags byte is `+0x0D` (SR1 `+0x0E`), no "transmission == 2 → 8" bit, no `rnd(9)` value, the `0x20` flag and the two skill bytes are new. DS remap: 7FF8→AB7E, car model records 7D86→A80C (0x14 bytes),
overlay block 8DF2..→BAAC (10 bytes, `+2` roof, `+6` rear bumper, `+8` front bumper).

### 0001:534c opponents_init  (changed, SR1 `0000:5584`, SR1 §4.6)

> **Secondary view (S3.7):** owned by `garage.md`; on any conflict the owner's section wins.
`void near opponents_init(void)`: for the 21 models `m = 0..20` (the 22nd is the King) build three lists (`slow[0]`/`mid[0]`/`fast[0]` = count, entries from `[1]`) by class byte `B[A810 + 0x14m]`:
`< 3` slow, `< 6` mid, else fast. Then `opponent_init(i, slow)` for i = 0..6, `(i, mid)` for 7..13, `(i, fast)` for 14..20; `B[AD03] = 0` (the King's `+0B`). The demo special case ("The Geek") is gone.

### 0001:5400 new_game  (changed, SR1 `0000:56a9`, SR1 §4.6)

> **Secondary view (S3.7):** owned by `garage.md`; on any conflict the owner's section wins.
```c
pools_init(1);
money = W[9730] * 40000 + 0x4B0;          /* $1200; demo $41200 (SR1 $750 / $10000) */
B[AA00] = B[AA01] = 1;                    /* NEW skills */
road_seed_init(1);                        /* 0dcb:2866(1) */
road_set_param(100);                      /* 0dcb:24d8(100) (SR1 f2645_0b91(100)) */
opponents_init();
game_clock = 0;  rt_base = rt_ticks;
W[479C] = W[479E] = W[47A0] = W[47A2] = W[47A4] = W[47A6] = 0;
W[9B40] = 0xFFFF;  B[9B42] = 0xFF;  B[9B86] = 0x2A;  memset(DS:9B5C, 0, 0x2A);       /* garage / classified-ad block */
```

### 0001:550e new_game_prompt  (changed, SR1 `0000:570c`, SR1 §4.6)

> **Secondary view (S3.7):** owned by `garage.md`; on any conflict the owner's section wins.
As SR1: `W[AD26] = 0; show_picture_at(0x3EA, 0x10, 6, 0)` (LIB2 #2 licence); `cursor(1)`; `W[AD26] = 1`; `pal_shadow_patch()`; `page_copy_rect(0,0,0x13F,0xBD)`; `driver_vec(8)()`; `pal_reg12_13(DS:1DDA)`;
`r = ui_edit(2, 15, DS:A9E8)` (menu 2: name field, "Old Game" −2, "OK" −3; Enter 0); **`str_rtrim(DS:A9E8)`**; `r == −3 or 0` → `new_game(); return 0`; `r == −2` → −2; otherwise return r.

### 0001:55b0 save_part_chain / 55fa load_part_chain  (changed)
= SR1 `save_part_chain 0000:5795`, `load_part_chain 57dc` (SR1 §4.11); identical logic (8-byte records, link `+6`); `load_part_chain(fd, u16 *head)`.

### 0001:56a4 save_car_chain / 5776 load_car_chain  (changed)
= SR1 `0000:586e`, `590f`. Δ: car record **0x2A** bytes (link `+0x28`; SR1 0x28 / `+0x26`); each car is followed by **10** part chains in this order: `+6`, `+8, +0A, +0C, +0E, +10`, `+12`, `+1C`, `+1E`, `+20`
(SR1: `+6`, five at `+8`, `+12`). `one != 0` stops after the first car. `load_car_chain`: read 0x2A bytes; load the 10 chains (any failure → 1); `c = car_alloc()`, copy 0x15 words, link `prev+0x28 = c`; stop when `+0x28 == 0` or `one`.

### 0001:58b2 save_slots_scan  (changed, SR1 `0000:59f6`, SR1 §4.11)
`void near save_slots_scan(int for_save, int *count)` (SR1 stored the count in `W[49E0]`). Slot index `n = 0..14` (SR1 1..15).
```c
k = 0;
for (n = 0; n < 15; n++) {
    name = data_disk_check() + ":HOTROD" (DS:4CAE) + itoa(n) + ".SAV" (DS:4CB6);
    if (_dos_open(name, 0, &fd) == 0) {
        if (_dos_read(fd, buf, 0x38, &got) == 0 && got == 0x38) { B[90B8+n] = n; W[A61C+2k] = 0x8FC8 + 16n; strncpy(0x8FC8 + 16n, buf, 15); k++; }
        else B[90B8+n] = 0xFF;
        _dos_close(fd);
    } else B[90B8+n] = 0xFF;
    if (for_save && B[90B8+n] == 0xFF) { W[A61C+2k] = 0x8FC8 + 16k; B[0x8FC8 + 16k] = 0; k++; }   /* empty label "" (SR1 "_______________") */
}
*count = k;
```
In save mode `k == n` always, so list row r ⇔ slot r.

### 0001:59e2 load_game_screen  (changed, SR1 `0000:5b4a`, SR1 §4.11)
```c
cursor(−4); cursor(1); save_slots_scan(0, &cnt);
s = W[AD26]; W[AD26] = 0; show_picture(0x3E9, 0);                         /* LIB2 #1 Highway Patrol book */
if (cnt > 0) { show_picture_at(0x3EC, 0xF0, 100, −1); show_picture_at(0x3EE, 0xF0, 0x8C, −1); }   /* "load", "forget it" */
W[AD26] = s; if (s) page_copy_rect(0, 0, 0x140, 0xBE);  cursor(−2);
if (cnt == 0) { dialog_msg("No saved games found !"); return −1; }
r = list_box_run(0x1A, 0, A61C, 0, cnt);                                   /* −4 Forget it, −3 Load (row in W[B88E]) */
if (r == −4) return −1;  if (r == −3) r = W[B88E];
slot = the r-th present slot (B[90B8+n] >= 0);  cursor(−4);
open "X:HOTROD<B[90B8+slot]>.SAV", read 0x38 bytes into DS:A9D8;
pools_init(0);
ok = !load_car_chain(fd,&cur_car,1) && !load_car_chain(fd,&car_list,0) && !load_part_chain(fd,&spare_parts)
     && read(fd, DS:479C, 12) && read(fd, DS:AB7E, 0x18C) && read(fd, DS:1F6A, 4) && read(fd, &elapsed, 4)
     && read(fd, DS:7DFE, 2) && read(fd, DS:9B40, 0x80);
if (ok) { close(fd); if (cur_car) set_paint_palette(B[cur_car+4]); road_seed_init(0) /* 0dcb:2866(0): regenerate the road from DS:A9F8 */;
          road_set_param(W[7DFE]) /* 0dcb:24d8 */; rt_base = rt_ticks − elapsed; cursor(−2); return 0; }
cursor(−2); dialog_msg("Cannot read that game !"); cursor(−3);
if (fd >= 0) { close(fd); new_game(); }  cursor(−1); return −0x29A;
```
Δ vs SR1: layout in FORMATS.md (header 0x38 vs 0x24, counters 12 bytes vs 8, extra 0x80 block, road reseed); pictures 0x3E9 / 0x3EC / 0x3EE (SR1 0x3F7 / 0x3FA / 0x3FC); `dialog_msg` (= `ui_dialog(0x1B)` with a text, "OK") instead of
`ui_menu(0x0F / 0x1F)`; list menu 0x1A (`list_box_run`, SR1 `ui_list(3,…)`).

### 0001:5d82 save_game_screen  (new body, SR1 `0000:5eea`, SR1 §4.11)
`void near save_game_screen(void)`. Same flow as SR1; exact differences:
```c
cursor(−4); cursor(1); save_slots_scan(1, &cnt);                           /* cnt = 15 */
W[AD26] = 0; show_picture(0x3E9, 0); sel = −1; show_picture_at(0x3EB, 0x48, 0xA9, −1) /* name field */;
show_picture_at(0x3ED, 0xF0, 100, −1) /* save */; show_picture_at(0x3EE, 0xF0, 0x8C, −1) /* forget it */; page_copy_rect(0,0,0x140,0xBE); W[AD26] = 1;
strcpy(DS:22EA, DS:A9D8);                                                  /* edit field starts with the current save name */
for (;;) {
  for (;;) {
    first = 0; while (first < cnt && B[90B8+first] != 0xFF) first++;  if (first >= cnt) first = cnt − 1;   /* first free slot */
    if (sel < 0) sel = first;
    cursor(−2);
    r = list_edit_run(0x1C, sel, A61C, cnt);                               /* 1a21:565c: −0x18 Forget it → return, −0x17 Save (row in W[B88E]) */
    if (r == −0x18) return;  sel = (r == −0x17) ? W[B88E] : r;
    trimmed = 0;
    if (str_is_blank(DS:22EA)) { if (!str_is_blank(list[sel])) break;      /* keep the existing label */
                                 dialog_msg(W[4CAA] /* "The game must have a name !" */); cursor(−4); continue; }
    trimmed = str_rtrim(DS:22EA);
    for (k = 0; k < 15; k++) if (strcmp(list[k], DS:22EA) == 0) break;     /* typed name equals an existing label → that slot */
    if (k < 15) { draw_box1 ×2 on row k (colours scheme[W[B890+4]·5]); sel = k; }
    break;
  }
  name = "X:HOTROD" + itoa(sel) + ".SAV";
  ans = (B[90B8+sel] != 0xFF) ? ui_dialog(0x1D) : 0;   /* "The old game will be replaced !": Go ahead −0x14 / Forget it −0x15 */
  cursor(−4);
  if (ans == −0x15) { append `trimmed` blanks to DS:22EA; continue; }
  if (ans == −0x14) remove(name);
  strcpy(DS:A9D8, str_is_blank(DS:22EA) ? list[sel] : DS:22EA);
  elapsed = rt_ticks − rt_base;  name[0] = data_disk_check();
  create + write: 0x38 @A9D8, save_car_chain(cur_car,1), save_car_chain(car_list,0), save_part_chain(spare_parts),
                  12 @479C, 0x18C @AB7E, 4 @1F6A, 4 &elapsed, 2 @7DFE, 0x80 @9B40;
  all ok → close, cursor(−2), return;
  else { cursor(−2); dialog_msg("Disk error during Save"); cursor(−4); if (fd >= 0) { close(fd); remove(name); } save_slots_scan(1, &cnt); }
}
```
Slot numbers are 0-based (`HOTROD0..14.SAV`). The typed name goes to the header save-name field `A9D8`, not to the driver name `A9E8`.

### 0001:630e quit_menu  (changed, SR1 `0000:641e`, SR1 §4.11)
```c
t0 = rt_ticks;                                    /* u32 DS:1F6E */
bg = ui_save_bg(1, &x, &y, 2);
do {
    r = ui_menu(1);          /* Save Game −0x17, Old Game −2, New Game −1, Quit −0x14, Forget it −0x16 */
    if (r != −0x14) break;
    bg2 = ui_save_bg(0x23, &x2, &y2, 2);  r = ui_menu(0x23);        /* confirm: Quit −0x14 / back −0x15 */
    restore bg2;
} while (r == −0x15);
restore bg;
rt_base += rt_ticks − t0;                         /* time spent in the menus does not count */
return r;
```
New: the confirmation sub-menu 0x23 and the clock compensation (SR1 had neither).

### 0001:72f2 car_info_sr2  (new, game_loop code 16)

> **Secondary view (S3.7):** owned by `garage.md`; on any conflict the owner's section wins.
`void far car_info_sr2(void)`: `if (!cur_car) { msg_box(9); return; }`; else with `W[AD26]` saved and cleared, cursor hidden:
`bg = screen_save_rect(0x1D, 0x16, 0x106, 0x92)`; colours from scheme `W[9DC6] + 5..9`; `fill_rect(0x20, 0x19, 0x100, 0x8C)`; `draw_frame3(0x20, 0x19, 0x100, 0x8C)`; title text `W[4E68]` centred (x = (0x100 − w)/2 + 0x20, y = 0x19); `hline(0x20, 0x22, 0x100)`.
`parts[0..9] = {W[car+8], W[car+0xA], W[car+0xC], W[car+0xE], W[car+0x10], W[car+6], W[car+0x12], W[car+0x1C], W[car+0x1E], W[car+0x20]}`; `y = 0x24`; for `i = 0..9`: if `parts[i]`:
`part_line(x = W[4E54 + 2i] + 0x20, y, name = W[4508 + 10·W[p+4]], 0x110, has_wear = (W[p+2] != −0x80), wear = W[p+2] / 100)` (`1a21:4e56`), `y += 10`.
`speed = car_runnable(cur_car, 1) ? car_max_speed(−1) : 0`; `sprintf(W[4E6C] + 9, " %3d mph", speed)`; `sprintf(W[4E6A] + 5, " %2d gallons", W[cur_car+0x24] / 10)`; texts at (0x20, y+10) and (0x20, y+0x14);
`page_copy_rect(0x1D, 0x16, 0x123, 0xA8)`; restore `W[AD26]`; `W[1F3E] = 1; ui_menu(6); W[1F3E] = 0` (any key / click closes); `screen_put_bitmap_mirror(bg, 0x1D, 0x16)`; `ui_reset_fx(1)` (1a21:9b56).

### 0001:63e0 day_index / 6426 clock_hm / 64a2 summer_over_check / 64b2 month_of_day  (changed)

> **Secondary view (S3.7):** owned by `garage.md`; on any conflict the owner's section wins.
= SR1 `day_index 6476`, `clock_hm 64a3`, `summer_over_check 6529`, `month_of_day 653c` (SR1 §4.7); same arithmetic: `T = game_clock + (rt_ticks − rt_base)` (u32 at 1F6A / 1F6E / 1F72), `day = min(T / 0x1998, 0x5B)`;
`clock_hm`: T clamped to 0x91908, `r = T % 0x1998; h = r / 0x222; m = (r % 0x222)·60 / 0x222`; `summer_over_check`: `day > 0x5A → B[1BC6] |= 1`; `month_of_day` unchanged (June 16–30, July, August, September 1–14, then 15).
**The year is 1969** (SR1 1963): day 0 is a **Monday**, June 16, 1969.

### 0001:650a calendar_show  (changed, SR1 `0000:65af`, SR1 §4.7)

> **Secondary view (S3.7):** owned by `garage.md`; on any conflict the owner's section wins.
Same flow with: `row = (d + 0x16) / 7 − W[4D52 + 2m]` (`4D52 = {0, 5, 9, 14}`), `x = ((d + 0x16) % 7)·0x12 + W[4D48]` (0x21) — weekday offset **0x16** (SR1 0x15); `y = row·7 + W[4D4A + 2m]` (`4D4A = {0x0E, 0x6B, 0x0F, 0x69}`);
base picture `m < 2 ? 0x478 : 0x47A` (LIB2 #112 / #114; SR1 0x4C3 / 0x4C5) composed with the X mark **0x479** at (x, y) by `pic_get(base, 2, 0x479, x, y, 0)` (`1a21:6b04`); the bitmap is shown at (0x50, 4) with
`page_copy_rect(0x50, 4, w+0x4F, h+3)`; a copy of that screen area is saved first (`arena_bitmap_alloc` + `screen_save_rect(0x50,4,…)`); non-final (`final == 0`): `status_print(−1, DS:4D5A /* "" */, 0); wait_ticks(500)` and the saved area is put back.
`summer_over_check()` first; `if (!final && W[1BC6]) return`.

### 0001:66ae date_print  (changed, SR1 `0000:6750`)

> **Secondary view (S3.7):** owned by `garage.md`; on any conflict the owner's section wins.
`sprintf(buf, "%s %d, 1969", month_name[m] /* W[4D82 + 2m]: June, July, August, September */, dom)`, colours from the scheme (`W[9DC6] + 0x41/0x43/0x45`), text at (0x61, y) via driver vector 3. Only the year string (DS:4D76) differs.

### 0001:68e2 broke_check  (changed, SR1 `0000:6ad8`)

> **Secondary view (S3.7):** owned by `garage.md`; on any conflict the owner's section wins.
`return cur_car == 0 && car_list == 0 && spare_parts == 0 && (i32)cheapest_ad_price() > money;` (SR1: `money < 400`). `cheapest_ad_price` is 0 when the ad list is empty ⇒ not broke then.

### 0001:8c68 drive_to_event  (new, SR1 `drive_to` 8d26)

> **Secondary view (S3.7):** owned by `race.md`; on any conflict the owner's section wins.
`void far drive_to_event(void)`: cruise scene to / from an event. `saved = W[ADD6]`; if `W[1BC8] != 0`: `W[ADD6] = 0`; `fuel = W[cur_car+0x24]; W[cur_car+0x24] = fuel < 3 ? 0 : fuel − 2` (0.2 gal);
`driver_vec(7)()`; `cursor(−4)`; `B[AB28] = B[BAAD + 10·W[cur_car+2]]` (byte 1 of the model's overlay record); `W[9DCC] = (B[4504 + 10·W[W[cur_car+6]+4]] == 0)` (the fitted engine part is of category 0);
`B[B7AE] = 0`; `car_perf_prepare(DS:A100)` (0001:bc88); `road_state_init()` (0dcb:0736); `W[A1C4] = 0`; `cruise_scene()` (0001:8d9e: `set_paint_palette`, `opp_palette_set`, `lib_read_dir`, `race_run`, waits; race module).
Finally `W[ADD6] = saved`. `W[1BC8]` selects the trip: 1 = back to the garage, 2 / 3 / 4 = Burger's / gas station / Grudge Night, 0 = no scene (used by `event17_leave` after a disqualification).

### 0001:a4ea bob_sign_draw

New. `far void bob_sign_draw(void)` (Bob's Burgers screen helper, called by `bob_screen_draw`, see `race.md`): `DS:AAFC:AAFE = pic_get(0x47D, 2)` (1a21:6b04, LIB2 picture 0x47D, mode 2 = arena 2); `dancer_blit(DS:AD14:AD16)` (0001:a4aa, `video.md`: 24x44 sprite at (290, 87) on the current draw page); `arena_free_high(1)` (1a21:9b56). Despite the name `dancer_blit` this is the sign picture, there is no dancer in SR2.

### 0001:a51c ending_trigger  (new; front of SR1 `ending_sequence a544`)
Called by `bob_drive_in` (race) when the King is beaten. `void near ending_trigger(void)`:
`h = W[1E44]; cursor(−4); status_init(0xBE); pal_reg12_13(DS:1DEC); W[1E44] = 200; show_picture_at(0x43B, 8, 0x14, 8)` (LIB2 #51 "Well, you've done it…"); `W[1E44] = h; driver_vec(8)(); wait_ticks(0x2EE)`
(750 ticks, input skips); `sound_vec(19)()` (`DS:BBD8`); `ending_anim()` (2925:27da); `arena_clear()`; `sound_vec(18)()` (`DS:BBD4`). SR1's long girlfriend / lips sequence does not exist in SR2;
afterwards the race code shows msg 0x44 and sets `W[1BC6] |= 2`.

### 0001:b2ea tournament_time_text  (new)
`void far tournament_time_text(int t, int msg)`: `p = strchr(W[11EC + 6·msg] /* line 1 */, '.')`; `sprintf(buf, "%2d.%d", t / 10, t % 10)` (format DS:51DD); `memcpy(p − 2, buf, strlen(buf))` — patches the time (tenths of a second)
into the blank `"  . ."` of the message template.

### 0001:b356 tournament_result_message  (new; called by `race_results`)
`void near tournament_result_message(int mode)` (mode 2 = qualifier). Inputs: `B8C2` time (tenths), `ADA8` dial-in, `AD6C` round, `9B3E` alive flag, `ADDA` percent handicap (race code). Meaning of the first branch is inferred.
```c
cursor(−2);
if (W[9B3E] == 0) {                                              /* the player lost this race */
    if (W[B8C2] < W[ADA8]) { msg_box(0x79); W[9B3E] = 1; return; }          /* the opponent broke out: you advance */
    W[B8C2] = min(999, (100 − W[ADDA]) · W[B8C2] / 100);                    /* margin by which you lost */
    tournament_time_text(W[B8C2], 0x69);  id = (W[AD6C] < 2) ? 0x6A : 0x69;
} else if (mode == 2) {                                          /* qualifier */
    if (W[B8C2] < 0x321) { tournament_time_text(W[B8C2], 0x66); id = 0x66; }
    else { W[9B3E] = 0; id = 0x71; }                                        /* breakout time too weak */
} else if (W[B8C2] < W[ADA8]) { W[9B3E] = 0; id = (W[AD6C] < 2) ? 0x6F : 0x6E; }   /* ran quicker than the dial-in */
else { *(strchr(W[1456], 'd') + 2) = '1' + W[AD6C]; tournament_time_text(W[B8C2], 0x67); id = (W[AD6C] < 3) ? 0x67 : 0x68; }
msg_box(id);
```
(`W[1456]` = line 1 of message 0x67, "You won round   with …": the round digit goes two characters after the `d` of "round".)


### 0dcb:2866 / 0dcb:24d8 — see Handoffs (race_render: `road_seed_init`, `road_set_param`).

### 1a21:0dca wait_click_or_key  (changed, SR1 `0f38:0b51`)
`void far wait_click_or_key(int t)`: `until = ticks + (i32)t;` loop while `until > ticks` (signed 32):
`if (B[3B3E]) { B[3B3E] = 0; return; }` (click pending; SR1 4736) · `if (B[1F60]) { mouse_buttons(); if (W[1F5C]) return; }` · `if (B[3B25]) { joy_read(); if (B[3B24]) { B[3B24] = 0; B[3B3E] = 0; return; } }` ·
`k = key_read(); if (k == 'M') { if (!W[5304]) { W[5304] = 1; music_resume(); } else { music_stop(); W[5304] = 0; } } else if (k == '\r' || k == 0x1B) return;`.
DS remap: 05F2→1F60, 05EE→1F5C, 471D/471C→3B25/3B24, 58D0→5304 (SR1 spec platform §4.5).

### 1a21:0e82 title_sequence  (changed, SR1 `0f38:0bf0`, SR1 §4.2)
`void far title_sequence(void)`; `h = W[1E44]` (clip height, SR1 04C2). Compared with SR1 it has a third wait and no credits-palette switch:
```c
if (driver == −2 || driver == −6) driver_vec(7)();
W[1E44] = 200;
if (driver is not EGA/Tandy) cga_set_palette(0);
show_picture_at(3, 0x28, 0x28, 0);  pic_free(3);                 /* LIB1 #2 "California Dreams presents" */
driver_vec(8)();  W[AD26] = 0;
show_picture(1, −1);  pic_free(1);                               /* LIB1 #0 title, drawn off screen */
wait_click_or_key(400);
driver_vec(8)();                                                 /* title visible */
if (driver is not EGA/Tandy) cga_set_palette(1);
driver_vec(10)(front, back);                                     /* page copy */
show_picture(4, −1);  pic_free(4);                               /* LIB1 #3 credits, off screen (SR1 passed 0) */
wait_ticks(0x14);  wait_click_or_key(400);
driver_vec(10)();                                                /* credits visible; SR1 set palette DS:02D4 here — gone */
wait_ticks(0x14);  wait_click_or_key(600);                       /* NEW: second credits wait, 600 ticks */
W[1E44] = h;  screen_fill_rect(0, 0, 0x140, 200, 0);  driver_vec(10)();   /* black */
```
`wait_click_or_key(n)` ends early on Enter, Esc, a click or the joystick button; `M` toggles the music.

### 1a21:0fe8 summer_over_screen  (changed, SR1 `0f38:0d62`, SR1 §4.9)
`msg_box(0x60); driver_vec(7)(); cursor(−4); W[AD26] = 1; screen_fill_rect(0, 0, 0x140, 200, 0); calendar_show(1); pal_reg12_13(DS:1DDA); driver_vec(8)(); wait_ticks(300)`.

### 1a21:1cde status_print / 1e86 status_label  (changed)
= SR1 `f0f38_20a1` (`status_text(x, str, width)`) and `f0f38_2213` (`status_label(str)`), SR1 §4.5; same code with the scheme at `W[9DC6]+0x37/0x39/0x3B` and the status-line variables
moved `+0x2AE6`: x0 `AD36` (SR1 8250), y `AD38`, width `AD3A`, clear-flag `AD3C`, money x `AD3E` (SR1 8258), text end `AD32` (SR1 824C). `status_label(str)` fills `(0, y, x0&~7, 8)`, draws the label at x = 0, sets `AD3E = (AD32 + 7) & ~7`
and, unless `str[1]` is 'G' or ' ' (i.e. for "`Bankroll:`$"), calls `money_add(0)` to print the amount; then copies the strip to the visible page if `W[AD26]`.

### 1a21:1df4 money_add  (changed, SR1 `0f38:21c0`, SR1 §4.5)
`void far money_add(int delta)`: `saved = W[AD3C]; W[AD3C] = 1; money += (i32)delta` (sign-extended; only 16-bit deltas); `ltoa(money, buf, 10)`; `status_print(W[AD3E], buf)`; `W[AD36] = W[AD32]; W[AD3A] = 0x140 − W[AD32]`; `W[AD3C] = saved`.
DS remap: 7EAA→A9FC (money).

### 1a21:1e50 pay_or_msg  (new)
`int far pay_or_msg(unsigned price, int msg)`: `if ((i32)money < (i32)price) { msg_box(msg); return 0; } money_add(−price); return 1;` (used for the $30 Grudge Night stake: `pay_or_msg(0x1E, 0x72)`).

### 1a21:43dc hotspot_find  (new, SR1 `0f38:4dad`)
`void far hotspot_find(int x, int y)` — finds the menu element under (x, y) and caches a rectangle in which the answer stays the same. Menu elements start at `W[B894]` (0x12 bytes each: `+0` flags: bit 0 active, bit 1 last; `+2` text ptr; `+6` code;
`+8,+0A,+0C,+0E` = x0, y0, x1, y1). Result: `W[B898]` = code (0 if none), `W[B896]` = element, cached rectangle `W[B87E]=x0, W[B880]=x1, W[B882]=y0, W[B884]=y1`.
```c
if (W[B894] == 0) { W[B882] = W[B87E] = −1; W[B884] = W[B880] = 1000; }          /* no menu: empty cache, B898 = 0 */
else {
    W[B882] = W[B87E] = 0;  W[B880] = 0x13F;  W[B884] = 199;
    for (e = W[B894]; ; e += 0x12) {  W[B896] = e;
        if (e.flags & 1) {
            if (x < e.x0)            { W[B880] = e.x0; break; }                   /* list is ordered by x0 */
            if (x < e.x1) {
                if (y < e.y0)        { if (e.y0 < W[B884]) W[B884] = e.y0; }
                else if (y < e.y1)   { W[B87E] = e.x0; W[B880] = e.x1; W[B882] = e.y0; W[B884] = e.y1; W[B898] = e.code; return; }
                else if (W[B882] < e.y1) W[B882] = e.y1;
            } else if (W[B87E] < e.x1) W[B87E] = e.x1;
        }
        if (e.flags & 2) break;
    }
    W[B896] = 0;
}
W[B898] = 0;
```

### 1a21:44a8 draw_element

New (dialog / menu item system; screens and `UIEL` layout in `garage.md`: flags +0, text +2 near `char *`, scheme +4, code +6, x0 +8, y0 +0A, x1 +0C, y1 +0E, key bytes +10/+11). `far int draw_element(UIEL *el)` draws one element on the draw page `DS:AD14:AD16` and returns 1 if it drew something (counts for the bounding box of `draw_screen`), else 0.
1. If `el->scheme > 0`: copy the 5-byte colour record `DS:[9DC6] + 5 * scheme` to `DS:AD28..AD2C` (fg word, fill word, frame byte used by the primitives).
2. `t = (flags & 0xFF00) | (flags & 0xD8)` (bits 0x01, 0x02, 0x04, 0x20 ignored), then by `t`:
   * `0x08` (panel) or `0x40` (frame): `w = x1 - x0`, `h = y1 - y0`; `fill_rect(page, x0, y0, w, h)`; `draw_frame3(page, x0, y0, w, h)`; if `el->text`: caption `x = x0 + (w - text_width(text)) / 2` (`text_width` = 1a21:1954), `screen_text(page, x, y0, text)` (driver slot 3 `[DS:A0BA]`) and `hline(page, x0, y0 + 9, w)` (1a21:3fba) under it. Returns 1.
   * `0x10` (button): inner rectangle inset by 1: `x = x0 + 1, y = y0 + 1, w = x1 - x0 - 2, h = y1 - y0 - 2`; `fill_rect` then `draw_frame3` on it. Label string `s`: by `el->code`: -10 -> `DS:20E8` (`"]"`), -9 -> `DS:20E6` (`"^"`), -8 -> `DS:20E4` (`"\"`), -7 -> `DS:20E2` (`"["`) (scroll / arrow glyphs of the UI font), otherwise `el->text`. Drawn centred: `screen_text(page, x + (w - text_width(s)) / 2, y + (h - 8) / 2, s)`. Returns 1.
   * `0x80` (static text): `screen_text(page, x0, y0, text)`. Returns 1.
   * `0x100` (centred text): `x = x0 + (x1 - x0 - text_width(text)) / 2`, `y = y0 + (y1 - y0 - 8) / 2`; `screen_text(page, x, y, text)`. Returns 1.
   * `0x200` (edit field): `DS:B892 = 1`, then as `0x400`.
   * `0x400` (title / selection bar): if `DS:B892 == 0`: `fill_rect(page, x0, y0, x1 - x0, 8)` and the text centred horizontally at `y0` (`screen_text`). If `DS:B890 == 0`: `DS:B890 = el` (first title / edit element). Returns `DS:B892 == 0` (so a title bar after an edit field is neither drawn nor counted).
   * `0x800` (list row): `if (DS:B88A++ == 0) DS:B888 = el` (first row element, count in `DS:B88A`); nothing drawn; returns 0.
   * anything else (hot spot only, `0x20` list-area flag, ...): returns 0.
`DS:B888/B88A/B890/B892` are reset by the code that activates a screen (`ui_push`, garage.md "UI toolkit"). Port: straight transcription onto the existing `fill_rect`, `draw_frame3`, `hline`, `screen_text` primitives.

### 1a21:473e draw_screen

New. `far void draw_screen(UIEL *els)`: `el = els; do { if (draw_element(el)) { DS:8824 = min(DS:8824, el->x0); DS:8826 = min(DS:8826, el->y0); DS:8828 = max(DS:8828, el->x1); DS:882A = max(DS:882A, el->y1); } last = el; el++ (0x12 bytes); } while (!(last->flags & 2));`. The bounding box `DS:8824/8826/8828/882A` (min x, min y, max x, max y) is not initialised here; the caller sets it (garage.md: `DS:AD26 = 0; draw_screen(els)`).

### 1a21:4806 demo_step  (new; replaces SR1 `demo_step` 0000:1bae + `kbd_inject_scancode`)
Demo mode (`W[9730] != 0`): the game plays by itself with the **mouse cursor**. `ui_wait` calls `demo_step()` whenever `W[20EA] == −2` ("choose the next action"); it picks a menu element, stores its centre as target `(W[20EA], W[B196])` and returns;
the timer ISR (`0001:2246`, 72.8 Hz) then moves the cursor one pixel per tick towards the target in both axes (`W[3B32] += sgn(W[20EA] − W[3B32]); W[3B34] += sgn(W[B196] − W[3B34]); W[881C] = 1` when `W[20EA] >= 0`) and, when it is
there, injects a click: `B[3B3E] = B[3B3F] = 1; W[3B40] = W[20EA]; W[3B42] = W[B196]; W[20EA] = −1`. `ui_wait` turns −1 into −2 when it consumes the click.
```c
void demo_step(void)
{
    M = W[B872 + 2·W[B87C]];             /* menu number on top of the menu stack (ui_push argument) */
    el0 = W[B894];  target = 0;
    if (el0 == 0) {                      /* no menu (message box / animation) */
        wait_click_or_key(0x8C);  W[20EA] = 0xA0;  W[B196] = 100;
        if (W[3B32] == 0xA0) W[20EA] = 0xD2;   return;
    }
    if (M == 0 && car_count > 2)            target = 6;               /* garage: "Cars" */
    else if (M == 0 && part_count > 0x14)   target = 5;               /* "Sell parts" */
    else switch (M) {
    case 0:  W[2164] = 0; W[20EC] = 0;                                /* garage */
             if (!cur_car) target = 1;                                /* newspaper */
             else if (W[cur_car + 0x24] <= 0x28) target = 15;         /* low fuel → gas station */
             else { k = rnd(W[2132]); i = k; while (W[2110 + 2i] > 0) i++;
                    target = W[20EE + 2i];  W[2110 + 2i] = 1;
                    if (--W[2132] == 0) { W[2132] = 17; memset(DS:2110, 0, 34); } }   /* shuffle bag of garage codes */
             break;
    case 1:  target = −0x16; break;                                   /* quit menu: Forget it */
    case 2:  target = −3; break;                                      /* licence: OK */
    case 3:  if (!spare_parts) { spare_add(0x34, W[470A]); spare_add(0x36, W[471E]); spare_add(0x3F, W[4778]); }   /* gift: tires, exhaust manifold, differential */
             if (!cur_car) target = 1; break;                         /* newspaper menu */
    case 6:  wait_click_or_key(0x8C); break;
    case 7: case 9: case 16:
             if (W[2166] == 0x0C || W[2166] == 0x22) target = −3;
             else if (++W[214A] >= 5) { target = (M == 9) ? −1 : −3; W[214A] = 0; }
             break;
    case 12: target = W[2134 + 2·W[2148]];  e = first element with code == target;
             if (!(e.flags & 1)) { W[20EA] = −2; return; }
             if (target == −1) W[2148] = 0; else W[2148]++;           /* script {1,2,1,2,−2,2,1,2,1,−1} */
             goto aim;
    case 14: if (W[2166] == 0x1E) target = −1;
             else { r = rnd(4); target = (r < 0) ? 0 : (r <= 1) ? −3 : (r == 2) ? −2 : (r == 3) ? −1 : 0; }
             break;
    case 20: target = (W[20EC] == 0x0F) ? 1 : 2; break;
    case 22: target = W[214E + 2·W[2164]]; W[2164]++;                 /* script {6,5,3,1,0x3EB×4,1,3,0} */
             if (target == 0) W[2164] = 0;
             else if (target == 0x3EB) { W[20EA] = 0xA0; W[B196] = 100; if (W[3B32] == 0xA0) W[20EA] = 0xD2; return; }
             break;
    case 27: case 33: W[20EA] = 2; break;                              /* overwritten below */
    case 32: target = −1; break;
    case 34: if (++W[214C] >= 5) { target = −1; W[214C] = 0; } break;
    default: break;                                                   /* all other menus: random element */
    }
    if (target == 0) {                                                /* random active element */
        n = number of elements with flags&1 (up to and including the one with flags&2);  k = rnd(n);  e = the k-th active element (0-based);
    } else e = first element from el0 with code == target;            /* no flag test */
aim:
    W[20EA] = (e.x0 + e.x1) >> 1;  W[B196] = (e.y0 + e.y1) >> 1;
    if (W[3B32] == W[20EA]) W[20EA] += (e.x1 − W[20EA]) >> 1;         /* cursor already at the centre column: aim at the right half */
    W[2166] = M;                                                      /* previous menu */
}
```
Esc is not special; `Q` (ASCII 0x51) in `ui_wait` or the race loop quits the demo (`platform_exit` shows "Quit demo"). Demo differences elsewhere: `copy_protection` skipped, start money $41200, classifieds give money when short (garage spec), `W[9730]`
disables mouse / joystick reads.

### 1a21:4cf4 ui_menu / 4d26 ui_dialog  (changed)
= SR1 `ui_menu 0f38:4f14`, `ui_dialog 0f38:4ec1`. `ui_menu(n)`: `ui_push(n); do r = ui_wait(16000 /* 0x3E80 */); while (r == 0); ui_pop(1); return r`. `ui_dialog(n)`: `bg = ui_save_bg(n, &x, &y, 2); r = ui_menu(n); screen_put_bitmap_mirror(bg, x, y); ui_reset_fx(1); return r`.
Menu numbers are renumbered in SR2 (garage 0, quit 1, licence 2, newspaper 3, game over 0x20, load list 0x1A, save list 0x1C, replace dialog 0x1D, confirm quit 0x23, OK dialog 0x1B; element tables in the garage / UI spec).

### 1a21:610c ui_edit  (changed, SR1 `0f38:5e54`)
`int far ui_edit(int menu, int maxlen, char *buf)`: `ui_push(menu)`; colours from `scheme[W[B890+4]·5]` (`AD28/2A/2C`); `W[1F3E] = 1; r = text_edit(W[B890+8], W[B890+10], buf, maxlen) /* 1a21:5fb8 */; W[1F3E] = 0; ui_pop(1); return r > 0 ? 0 : r`. (SR1 identical except the wait flag.)

### 1a21:6180 rnd  (new generator, SR1 `rnd 0f38:5eb6`)
`int far rnd(int n)`. **The generator changed from Wichmann-Hill to a 32-bit LCG.** `n < 1`: `seed = time(NULL)` (`DS:4A84` low, `DS:4A86` high) and return −1. Otherwise
`seed = seed · 0x015A4E35 + 1` (u32); `return ((seed >> 16) & 0x7FFF) % n`. Seeded once by `main` (`rnd(−1)`); `0dcb:2866` re-seeds from the road seed.

### 1a21:6e6a hot_data_load  (changed, SR1 `0f38:6b79`, SR1 §4.13)
```c
path = { B[W[B8E2]], B[W[B8E2]+1] /* ':' */ } + string at W[14D8] (advances W[14D8] past it: "SR2_data");     /* DS:8850 */
if (_dos_open(path, 0, &fd)) { fatal_message("Can't open data file"); platform_exit(); }
W[14EA] = 0x8E3C;
for (k = 0; k < 9; k++) if (_dos_read(fd, W[14DC + 2k], W[14F0 + 2k], &n) || n != W[14F0 + 2k]) { fatal_message("Data disk read fail"); platform_exit(); }
if (_dos_read(fd, 0x8860, W[BC4A] + 0x50, &n) || n < W[BC4A]) fail as above;           /* text pool */
_dos_close(fd);
for (k = 0; k < 6; k++) { p = 0x8E3C + 0x42·k; W[AAA4 + 14k] = p; W[AAA6 + 14k] = p + 0x16; W[AAAA + 14k] = p + 0x2C; }     /* road bitmaps */
W[AAFA] = 0xAAE4;
for (i = 0; i < 22; i++) { W[A80C + 0x14i + 0xC] = W[BBF4 + 2i] + 0x8860; B[A80C + 0x14i + 6] = rnd(5); }     /* car ad text ptr, random byte */
for (i = 0; i < 22; i++) W[AB7E + 0x12i + 6] = W[BC20 + 2i] + 0x8860;                                             /* person name ptr */
if (W[8846] < 3 && W[9860] == 1) { "X:sr2_shp" (DS:4BC0, drive letter from DS:8850); if (_dos_open fails) { W[4B64] = 1; return; } _dos_close(); }
```
Block destinations `DS:14DC[9]` = `AB7E, A80C, BAAC, 9BF4, 9872, A542, BBF4, 8E3C, AA9E`, sizes `DS:14F0[9]` = `0x18C, 0x1B8, 0xDC, 0x108, 0x1B8, 2, 0x58, 0x18C, 0x54` (FORMATS.md "SR2_DATA"). Δ: 9 blocks (SR1 13), no hall load (separate call),
no message table (texts are exe strings), new road-bitmap and text-pointer fix-ups, `SR2_shp` presence test (`W[4B64] = 1` = missing). `W[8846]` = current drive (< 3 = floppy).

### 1a21:7052 data_disk_check  (changed, SR1 `data_disk`)
`char far data_disk_check(void)`: floppy (`W[8846] < 3`): `tries = 0; while (_dos_open(DS:8850, 0, &fd) != 0) { if (++tries > 3) platform_exit(); prompt = W[4B66]; prompt[0xC] = (EGA/Tandy) ? '2' : '3'; prompt[0x19] = B[W[B8E2]];
fatal_message(prompt); }` (prompt "insert disk n into drive X"); `_dos_close(fd); lib_select(1000); return B[8850]` (drive letter). Hard disk: return `'@' + W[8846]` ('C' for 3).

### 1a21:70fc hall_scramble / 710c hall_unscramble  (changed)
= SR1 `hall_scramble 6ccf` / `hall_unscramble 6cec` (SR1 §4.13): for the first 0xE6 bytes of the hall (`DS:ADDC`) `|= 0x80` / `&= 0x7F`.

### 1a21:711c hall_load / 71cc hall_save  (changed, SR1 `0f38:6d09` / `6dd2`, SR1 §4.13)
`hall_load`: path `"?:hall_dat"` (DS:4BCA) with `[0] = B[W[B8E2]]`; `data_disk_check()`; `_dos_open`: ok → read `W[9AE0]` (2 bytes) then `W[9AE0]` records of 0x17 bytes to `ADDC + 0x17i` (any short read → fatal "bad file format", `1a21:6220`); close; `hall_unscramble()`; open failed → `W[9AE0] = 0`.
`hall_save`: `hall_scramble(); data_disk_check();` create (`W[8846] < 3`: up to 3 prompts `fatal_message(W[4B6A])` "insert disk 1 into drive X", then `platform_exit`; hard disk failure → `fatal_message(W[4B6C])` + `platform_exit`); write count + records; close; any
failure: `fatal_message(W[4B6C]); remove(path); fatal_message(W[4B6C])`; finally `hall_unscramble()`. Same file format as SR1 (§5.3), DS remap 9AE0 / ADDC.


### Segment 2925 — Grudge Night (tournament), gas station, ending

Common conventions for the drawing code of this segment: `W[AD26]` = "draw through to the visible page" flag, `AD14:AD16` draw page, `AD10:AD12` visible page, `W[AD1C]` driver (−2 EGA, −6 Tandy,
−3 VGA, …), colours `W[AD28]` fg, `W[AD2A]` bg / fill, `B[AD2C]` frame; `cursor(n)` = `1a21:2d90`, `ui_reset_fx(n)` = `1a21:9b56`, `ui_fx(n)` = `1a21:9b40`, `pic_get(id, mode)` = `1a21:6b04` (returns far ptr in DX:AX),
`screen_save_rect(x,y,w,h,…)` = `1a21:236e`, `screen_put_bitmap(bmp, x, y)` = `1a21:246c`, `screen_text(x,y,str)` = driver vector 3 on the draw page, `page_copy_rect(dst, src, x0,y0,x1,y1)` = `1a21:b028`.

### 2925:000e tournament_pick_field  (new; called by event17_run)
`void near tournament_pick_field(void)`: picks the 9 not-yet-beaten opponents whose car's top speed is closest to the player's.
```c
mine = car_max_speed(−1);                       /* player's current car */
n = 0; worst = 0; worst_d = 0;
for (id = 0; id < 21; id++) {                   /* persons AB7E + 0x12·id; the King (21) is excluded */
    if (B[AB7E + 0x12·id + 0x0B] == 0xFF) continue;          /* already beaten */
    spd[id] = car_max_speed(W[AB7E + 0x12·id + 8]);         /* model */
    d = abs(spd[id] − mine);
    if (n < 9) { T[n++].id = id; if (d > worst_d) { worst_d = d; worst = n − 1; } }
    else if (d < worst_d) {                                  /* replace the farthest one */
        T[worst].id = id;  worst_d = 0;
        for (j = 0; j < n; j++) { dj = abs(spd[T[j].id] − mine); if (dj > worst_d) { worst_d = dj; worst = j; } }
    }
}
```
`T` = table `DS:90CA`, 6 bytes per entry (id, state, time). (If fewer than 9 candidates exist the remaining entries keep stale data; `worst` starts undefined in the original — use 0.)

### 2925:00fa tournament_seed  (new)
`void near tournament_seed(void)`: dial-in times and seeding after the qualifier.
```c
T[9] = { −1, 0, W[B8C2] };  W[ADA8] = W[B8C2];                       /* DS:9100/9102/9104: the player; B8C2 = qualifier time (tenths of a second) */
for (i = 0; i < 9; i++) {
    car = A80C + 0x14·W[AB7E + 0x12·T[i].id + 8];
    T[i].time = ((B[car + 0x13] · 10) / 6) · W[B18E] / B[A9C3];     /* 32-bit multiply, then divide by the byte A9C3 */
    T[i].state = 0;
}
for (i = 0; i < 10; i++) {                                           /* selection sort by time, ascending */
    best = 32000; for (j = i + 1; j < 10; j++) if (T[j].time < best) { best = T[j].time; bj = j; }
    if (best < T[i].time) swap(T[i], T[bj]);                        /* 3-word swap */
}
```
`B[car+0x13]` = high byte of the car's flag word (`+0x12`, e.g. 0x90CD → 0x90). `B18E` / `A9C3` come from the race code (reference time / constant).

### 2925:01fa tournament_next_round  (new)
`void near tournament_next_round(void)`: prepares round `W[AD6C]` (0..3). States: 0 = still in, −1 = eliminated early, 2 = lost in round 2/3 (race for 3rd place), later +1 steps mark the final placings. `P = W[9106]` (player's index), `o = W[90C8]` (opponent's index).
```c
if (AD6C == 0) {
    P = 0; while (T[P].id >= 0) P++;                       /* find the player (id −1) */
    o = (P even) ? P + 1 : P − 1;                          /* neighbour in the sorted order */
    opponent_load(T[o].id);
    k = 9;  if (P == k || k − P == 1) k −= 2;              /* pick another pair (8,9) or (6,7) to pre-resolve */
    if (rnd(100) < 60) T[k].state = −1; else T[k − 1].state = −1;
}
else {
    if (!W[9B3E]) T[P].state = 2;                          /* the player lost the last round */
    else          T[o].state = (AD6C == 3) ? 2 : −1;       /* the opponent lost */
    if (AD6C == 3) {                                       /* final round: two races */
        a = first i with state == 0; b = next i > a with state == 0;          /* finalists */
        if (T[a].id < 0 || T[b].id < 0) { o = (T[a].id < 0) ? b : a; opponent_load(T[o].id); }   /* the player is a finalist */
        else { if (rnd(100) < 60) a = b; T[a].state++; }                       /* simulated final: winner state 1 */
        a = first i with state == 2; b = next i > a with state == 2;           /* the two semi-final losers */
        if (T[a].id < 0 || T[b].id < 0) { o = (T[a].id < 0) ? b : a; opponent_load(T[o].id); }   /* the player races for 3rd */
        else { if (rnd(100) < 60) a = b; T[a].state++; }                       /* winner state 3 */
    } else {
        for (i = 0; i < 10; i++) if (T[i].state == 0) {
            j = next index > i with state == 0;                                 /* sequential pairing of the undecided entries */
            if (T[j].id < 0)      { o = i; opponent_load(T[o].id); }           /* the player's pair: he races i */
            else if (T[i].id < 0) { o = j; opponent_load(T[o].id); }
            else { loser = (rnd(100) < 60) ? j : i;  T[loser].state = (AD6C == 2) ? 2 : −1; }
            i = j;
        }
    }
}
W[9ADC] = T[o].time;  W[90C8] = o;  W[9106] = P;
```
Note: with `AD6C == 3` the "winner" bump `T[a].state++` takes a state 0 → 1 (final) and a state 2 → 3 (third place); the per-round loser states 2 are given in round 2 (`AD6C == 2`).

### 2925:0522 panel_frame_draw  (new)
`void near panel_frame_draw(bitmap *dst /* far */, bitmap *border /* far */)`: decorates `dst` (the photo / face area saved from the screen) with a bevel cut from the border picture `border` (`DS:9108`): builds a rect list `{w, h, sx, sy, dx, dy}` (local_16…local_c) and blits (driver vector 0, `A0AE`, count 1) edge strips of 4 px (left/top/bottom/right, the left strip `((x>>9)<<8 | (x>>1)) & 0xFFF8` wide) and 8×4 corner pieces: 4 strips for `dst.h < border.h` plus, depending on `border.h > dst.h`, an extra set of 8-wide corners stepped by `(border.h − (dst.h − 8)) >> 1`. Pixel-level port: `port/decomp/sr2_ds.c` lines 41532–41725 (all constants there).

### 2925:0812 bitmap_remap_colors  (new)
`void near bitmap_remap_colors(bitmap *b /* far */, int from1, uint to1, int from2, uint to2)`: recolours a 4-plane EGA bitmap in place. Layout: `rows = b.h (+2)`, `bytes/row = b[+0xE]`, plane `p` at `data + p · rows · bytes/row` (data `b[+6]`:`b[+8]`). For each pixel (bit `m` of byte `i` of the planes) the colour is `p0 + 2·p1 + 4·p2 + 8·p3`; if equal to `from1` it is replaced by `to1`
(bit `k` of `to1` → plane `k`), else if equal to `from2` by `to2`. Called with `(img, 6, 0xC, 7, 3)` on EGA only.

### 2925:0a0c tournament_screen_build  (new)
`void near tournament_screen_build(int with_photo)` builds the framed Grudge Night screen in the draw page and saves the pieces for the wipes:
```c
h = W[AD26];  W[9108:910A] = pic_get(with_photo ? 0x4B7 : 0x7F6, 2);          /* border picture: LIB2 #207 / LIB3 #38 */
W[AD26] = 0;  screen_fill_rect(0, 0, 0x140, 200, 0);
save AD28/AD2A/AD2C; W[AD2A] = 0x0D09; B[AD2C] = 1;  draw_frame3(page, 3, 3, 0xD2, 0x9E);  restore;
W[9114:9116] = screen_save_rect(0, 0, 0xD8, 0xA4);                               /* the framed panel */
if (with_photo) {
    W[910C:910E] = screen_save_rect(8, 8, 200, 0x42);                            /* photo area */   panel_frame_draw(photo, border);
    car_compose(&img, W[9B0A] /* opponent model */, (i8)B[W[A80A] + 0xD] /* flags */, 0, 0x4D30 + 8·W[AD72] /* tyre set */, 0, 1);
    img = driver_vec(15)(img, class colour byte B[A80C + 0x14·W[9B0A] + 4]);     /* A0EA recolour */
    if (driver == −2) bitmap_remap_colors(img, 6, 0xC, 7, 3);
    blit img into the photo, centred: x = ((photo.w − img.w) >> 1) & ~7, y = (photo.h − img.h) >> 1 (1a21:ad48);
    ui_reset_fx(1); ui_fx(1);
    W[9110:9112] = screen_save_rect(8, 8, 0x28, 0x23);                           /* face area 40×35 */  panel_frame_draw(face, border);
    blit pic_get(W[W[A80A] + 4] /* opponent face id */, 2) into the face at (8, 8); ui_reset_fx(1);
}
driver_vec(5)(front ← back);   W[AD26] = h;
```
(`W[A80A]` = pointer to the current opponent's person record, loaded by `opponent_load`.)

### 2925:0c4c clip_rect_split  (new)
`int near clip_rect_split(rect *clip, rect *r, rect *out1, rect *out2)`: clips a drawing rectangle `r` (words `{w, h, sx, sy, dx, dy, …}`, 10 words) against the horizontal band `clip = {x0, y0, x1, y1}` (words +2 = y0, +6 = y1) and returns the number of pieces (0..2) written to `out1` / `out2` (10-word copies of `r`):
piece above the band: if `r.dy < clip.y0` (`r[5] < clip[1]`): copy, and if `clip.y0 < r.dy + r.h` set `piece.h = clip.y0 − r.dy`; piece below: if `r.dy + r.h > clip.y1`: copy, `d = clip.y1 − r.dy + 1`; if `d > 0` then `piece.h −= d; piece.sy += d; piece.dy += d`; additionally `h` is limited to 6 (`if h > 6 { sy += h − 6; dy += h − 6; h = 6 }`).

### 2925:0d02 panel_wipe  (new)
`void near panel_wipe(unsigned mode, bitmap *bmp /* far */, int x, int y, rect *clip /* or 0 */)`: animates a saved bitmap sliding onto the screen 2 pixels per step (one driver blit + `page_copy_rect` per step, background restored from the saved area `screen_save_rect(...)` first). If `mode & 1` it first builds three more pixel-shifted copies of the bitmap (`bitmap_make_shifted(bmp, shift = 2, 4, 6 …)`, `arena_bitmap_alloc`) so that odd-pixel positions can be drawn byte-aligned on EGA. Modes (`mode & 2` selects the direction of the cycling shift index):
* `0` — from the bottom up: starts at y = 183, `dy = −2`, visible height grows by 2 per step up to the bitmap height; used for the opponent's face at (0xD0, 0x52); steps = `(200 − y − 0x10) / 2`.
* `1` — from the right: starts at x = 0x13E, `dx = −2`, steps = `(0x140 − x) / 2`; used for the border picture to (0x18, 0x1E).
* `2` — from the top down: `dy = +2`, starts at `y = −(h − 2)`, steps = `(h + y) / 2`, clipped to `clip` through `clip_rect_split` (rect `DS:4FBC` = {0x18, 0x1E, 0x11F, 0x4F}); the framed panel to (0x30, 0x18).
* `3` — the photo to (0x38, 0x69): grows left to right from x0 = 8 (`dx = +2`, source window `0x30` wide, 8-pixel column steps with the 4 shifted copies); steps = `(w + x − 0x38) / 2`.
After the last step `ui_reset_fx(2 or 4)`. Exact pixel arithmetic: `port/decomp/sr2_ds.c` lines 41862–42218 (the decompile is readable); the effect is cosmetic, only the final images matter for correctness.

### 2925:12f2 tournament_round_header  (new)
Colours `AD28 = (driver EGA/Tandy) ? 4 : 3`, `AD2A &= 0xFF00`. `sprintf(buf, "ROUND %d", W[AD6C] + 1)` (DS:4F44), centred: `x = (0x140 − width) / 2`, y = 0x52. `d = W[9ADC] − W[ADA8]` (opponent's dial-in minus the player's);
`sprintf(buf, "%s (%+d.%d)", name = W[W[A80A] + 6], d / 10, abs(d) % 10)` (DS:4F4D) at (0x40, 0x60); the opponent's car text `W[W[A7AA] + 0xC]` at (0x48, 0xAD). Colours restored.

### 2925:13d0 tournament_entrants_list  (new)
Same colours. `x = 0x38, y = 0x61`; for `i = 0..8`: `sprintf(buf, "%2d %s", i + 1, W[AB84 + 0x12·T[i].id] /* person name */)` (DS:4F59) at (x, y); `y += 10`, except after the 5th entry: `x = 0x8C, y = 0x61`. Then `sprintf(buf, "%2d %s", 10, DS:A9E8)` at (x, y) (the player, 5th line of column 2), and "Prepare to set your breakout time" (DS:4F67) at (0x40, y + 0x14).

### 2925:14b6 tournament_standings  (new)
`void near tournament_standings(int *ids /* 4 entries: person id or −1 = player, best first */)`: colours as above; "FINAL STANDINGS:" (DS:4F89) at (0x38, 0x52); `y = 0x5E`; for `r = 0..3`: `id = ids[r]`; if `id < 0` name = `DS:A9E8`, model = `W[cur_car + 2]`, else name `W[AB84 + 0x12·id]`, model `W[AB86 + 0x12·id]`;
`car = W[A818 + 0x14·model]` (model name text); `sprintf(buf, "%2d.", r + 1)` (DS:4F9A) at (0x38, y); name at (0x4C, y); `r < 3`: `sprintf(buf, "$%3d", prize[r])` (DS:4F9F, prizes `W[4FB6] = {150, 100, 50}`) at (0xCE, y); car text at (0x56, y + 10); `y += 0x14`.
Then `page_copy_rect(0x38, 0x5E, 0xFF, y)` and "Congratulations!" (DS:4FA4) at (0x40, y + 2). All text via driver vector 3 on the draw page.

### 2925:1642 tournament_intro_screen  (new)
`cursor(1); tournament_screen_build(0); screen_put_bitmap(W[9114], 0x30, 0x18); screen_put_bitmap(W[9108], 0x18, 0x1E); page_copy_rect(0x18, 0x18, 0x11F, 0xBB); tournament_entrants_list(); cursor(−2); wait_ticks(2000)`.

### 2925:16c8 tournament_round_screen  (new)
`cursor(−4); tournament_screen_build(1); panel_wipe(1, W[9108], 0x18, 0x1E, 0); panel_wipe(2, W[9114], 0x30, 0x18, DS:4FBC); opp_palette_set(W[AD70]); panel_wipe(3, W[910C], 0x38, 0x69, 0); panel_wipe(0, W[9110], 0xD0, 0x52, 0);
ui_reset_fx(4); tournament_round_header(); cursor(−2); wait_ticks(1000)`.

### 2925:1786 tournament_finish  (new)
Final placings, prize, standings. `P = W[9106]`, `o = W[90C8]`.
```c
cursor(−4);
if (W[AD6C] < 4) {                                  /* the event ended early (eliminated / quit) */
    T[P].state = 3;  k = 0;  for (i = 0; i < 10; i++) if (T[i].state == 0) T[i].state = k++;
} else T[W[9B3E] ? o : P].state++;                  /* all four rounds raced: the last winner */
for (i = 0; i < 10; i++) if (T[i].state >= 0) ids[T[i].state] = T[i].id;      /* 4 places */
pl = T[P].state;
if (pl < 3) {                                       /* 1st..3rd place: prize */
    msg 0x6B text patched: digit at strchr(W[1470], 'e') + 3 := pl + 1 (format "%1d"), `$` amount at strchr(…, '$') + 1 := prize[pl] ("%d" then '.');
    msg_box(0x6B); money_add(W[4FB6 + 2·pl]);                            /* 150 / 100 / 50 */
}
tournament_screen_build(0); screen_put_bitmap(W[9114], 0x30, 0x18); screen_put_bitmap(W[9108], 0x18, 0x1E); page_copy_rect(0x18, 0x18, 0x11F, 0xBB);
tournament_standings(ids);  cursor(−2);  wait_ticks(2000);
```

### 2925:193e event17_leave  (new)
`void near event17_leave(int last_result)`: `driver_vec(7)(); opp_palette_set(−1); W[1BC8] = (last_result != 2); drive_to_event(); W[ADD6] = 0;` — drives back to the garage (`1BC8 = 1`), or skips the scene when the last race result was 2.

### 2925:1970 event17_run  (new) — Grudge Night, game_loop code 17
`void far event17_run(void)`; `W[1BC8] = 2` and the stake ($30) are handled by `game_loop`. Race calls (`race_start`, `race_results`, `opponent_load`, `opp_palette_set`) are the race module's; `race_results` calls `tournament_result_message` (0001:b356).
```c
W[ADD6] = 2;  W[A1C2] = W[A1C0] = 0;  W[AD6C] = −1;                /* ADD6: race mode word, 2 = qualifier, 4 = round */
tournament_pick_field();
opponent_load(0x15);  B[A56E] = 0;  W[B18E] = 0;                   /* the King's record is the reference car of the qualifier */
tournament_intro_screen();
W[1FFC] = 1;  race_start();  r = race_results();  W[1FFC] = 0;     /* qualifier: sets the player's dial-in (B8C2) and 9B3E */
if (W[9B3E]) {
    tournament_seed();
    W[AD6C] = 0;
    do {
        tournament_next_round();  tournament_round_screen();
        W[ADD6] = 4;  W[1FFC] = 1;  race_start();  W[1FFC] = 0;  r = race_results();
        if (!W[B18E]) W[B18E] = B[A9C3];
        if (B[AA10] == 0) { if (B[AA01] > 1) B[AA01]--; if (B[W[A80A] + 0x10] < 8) B[W[A80A] + 0x10]++; }   /* AA10: start comparison flag (race code) */
        else              { if (B[AA01] < 8) B[AA01]++; if (B[W[A80A] + 0x10] > 1) B[W[A80A] + 0x10]--; }
        if (!W[9B3E]) {
            if (B[AA00] > 1) B[AA00]--;  if (B[W[A80A] + 0xF] < 8) B[W[A80A] + 0xF]++;
            if (W[AD6C] < 2) goto leave;                           /* lost in round 0 or 1: no standings */
            if (r == 2) break;
        } else {
            if (B[AA00] < 8) B[AA00]++;  if (B[W[A80A] + 0xF] > 1) B[W[A80A] + 0xF]--;
        }
        W[AD6C]++;
    } while (W[AD6C] < 4);
    tournament_finish();
    if (T[W[9106]].state == 0) W[47A4]++;                          /* won Grudge Night */
}
leave: event17_leave(r);
```
`AA00` / `AA01` are the player's skill values, the opponent record bytes `+0xF` / `+0x10` the opponent's (set by `opponent_init`); winning raises the player's `AA00` and lowers the opponent's `+0xF`, `AA10` (set by the race code at the start of each race) steers `AA01` / `+0x10`.

### Gas station — event15_run 2925:2104 and helpers (SR1 `gas_station b8a1`, hotspots `b3ca…b617`)
Game loop code 15. Fuel is `W[cur_car+0x24]` in 0.1 gallons; the tank holds `0xB9` while pumping and is clamped to `0xAA` when leaving. Medium confidence on the exact click sequencing (decompile unusable, read from the disassembly:
`python tools/x86dis.py work/SR2_unp.exe dis 2925:2104 0x600`).

**2925:2104 event15_run** `void far event15_run(void)`: *setup* — `status_print(−1, "", 0)`; `W[1BC8] = 3`; `ui_reset_fx(0)`; `W[AD26] = 0`; `pal_reg12_13(DS:1DDA)`; `B[1DB2] = 4`; `show_picture(0x430, −1)` (LIB2 #72 gas station); `W[915A:915C] = pic_get(0x47E, 0)` (pump dial frames);
for the two pump slots k = 0,1 (x = `W[50F2 + 2k]`, y = 0x7B, w = `W[50FA + 2k]`, h = 0x3A) save two copies of the screen strip (`arena_bitmap_alloc` + `screen_save_rect`); `W[B89A:B89C] = pic_get(0x3E8, 0)` (LIB2 #0 clean background); `pump_strip_draw(page, k)` for k = 0..3; `status_label(W[4844])`;
`driver_vec(5)()`; `W[AD26] = 1`; `driver_vec(8)()`; `cursor(1)`; draw the player's car with `car_draw(4, model = W[cur_car+2], car_flags(cur_car), 0, 0xB9, 0x4D30 + 8·B[4504 + 10·W[W[cur_car+0x12]+4]])` and save/restore the strips around the nozzle (two bitmaps at `AD62−0x10, AD64−5`, 0x18×0x0C) with mirror blits.
*State* (locals): `pump` (0 none, 1, 2), `hand` (1 = nozzle not in hand, 2 = nozzle in hand; starts 1), `step` (fuel per $1, starts 0x14), `filling`, `filled` (has pumped), `fee` (starts 1: $10 service charge), `idle` (starts 0xA0), `tick` (0..8), `attendant` (1 until the idle timeout fired).
Draw nozzle: `nozzle_draw(P, 0x457 or 0x456)` (picture LIB2 #79 / #78 = nozzle hanging / in the car); menu `ui_push(0x16)`; `gas_hotspots_a()`; `cursor(−2)`.
*Loop* `code = ui_wait(7)` (menu 0x16: hotspot codes 1..8):
* `0` (timeout) while **filling**: `fuel_sound(pump, 2)` (gauge counter +1); every 8th timeout: `fuel += step; filled = 1; money_add(money >= 1 ? −1 : 0)` ($1 per step); if `0xB9 − fuel >= step` and money > 0 keep filling; else stop (`fuel_sound(pump, 3)`); if `attendant` is still 1 go on with the loop (nozzle stays), else leave (`gas_leave`).
* `0` while **not filling**: `idle--`; at 0: `gas_hotspots_none()`; `step = (pump == 2) ? 0x10 : 0xC`; `service = 1`; `pump_dial_anim()`; `msg_box(0x45)` ("Yo, you need full service, dummy!"); `attendant = 0`; if `filled` leave; else `fee = 0` and the attendant starts filling at that slower step (pump 1 if none, nozzle drawn in the car, `fuel_sound(pump, 1)`, `filling = 1`).
* `3`, `4` (and `2`, `5`) — the pump hotspots (`W[9158]` must be 1): hand == 1: codes 2 and 5 → `msg_box(0x62)` ("Use another pump, dummy!"); code 3 → pump 1 (`step = 0x14`), code 4 → pump 2 (`step = 0xF`): the nozzle is taken (`hand = 2`, strip restored from the saved copy, `gas_hotspots_c(pump)`). hand == 2: if `money >= 1 && !filled`: hang the nozzle back (`hand = 1`, `pump = 0`, `gas_hotspots_a()`, `step = 0x14`) else `fee = 0` and **leave**.
* `1` — the car's tank hotspot (`W[9156]` must be 1; element placed by `gas_hotspots_b/c` at the nozzle position): hand == 1 (nozzle in the car): stop filling (`fuel_sound(pump, 3)`), nozzle in hand again (`hand = 2`), and if the tank still has room (`0xB9 − fuel >= step`) and money >= 1 → `msg_box(0x46)` ("It's not full, speedy!"). hand == 2: if room (`0xB9 − fuel >= step`) and money >= 1 → insert (`hand = 1`, draw 0x456, `fuel_sound(pump, 1)`, `gas_hotspots_b()`, `filling = 1`) else `msg_box(money >= 1 ? 0x25 : 0x26)` ("full tank" / "Gas ain't free!").
* `6`, `7` — `msg_box(0x64)` ("Take a pump, dummy!"); `8` — exit door: if `fuel <= 4` `msg_box(0x63)` ("Get some gas first, speedy!") else `fee = 0` and **leave**.
**Leave** = `gas_leave(P)` (2925:1f38).
Costs: $1 per fill step (2.0 gal on pump 1, 1.5 gal on pump 2, 1.2 / 1.6 gal after the attendant took over), plus $10 (or all the money if less) when the service charge flag `fee` is still set at leaving. The trip costs two game hours in `game_loop`.

**2925:1ab8 gas_hotspots_clear** — for each element of the active menu table (`W[3B04]`, 0x12 bytes, until one with flag bit 1): clear flag bit 0; `W[9158] = W[9156] = 0`.
**2925:1ad2 gas_hotspot_enable(code)** — find the element with `+6 == code`, set flag bit 0.
**2925:1af0 gas_hotspot_set(code, x, y, w, h)** — find the element, `+8 = x, +0xC = x + w − 1, +0xA = y, +0xE = y + h − 1`, flag bit 0 set.
**2925:1b28 gas_hotspots_a** — clear; enable codes 2..8; `list_box(0x16)` (refresh the hotspot cache of menu 0x16); `W[9158] = 1`.
**2925:1b84 gas_hotspots_b** (SR1 `gas_hotspots_b b525`) — clear; set code 1 at (`W[AD62] − 10`, `W[AD64] − 5`, 0x12, 0xC) (cursor position); `list_box(0x16)`; `W[9156] = 1`.
**2925:1bba gas_hotspots_c(pump)** — clear; enable code (pump == 1 ? 3 : 4); set code 1 at (`W[AD62] − 1`, `W[AD64] − 1`, 10, 9); `list_box(0x16)`; `W[9158] = W[9156] = 1`.
**2925:1c06 gas_hotspots_none** — clear; `kbd_remap_restore()` (0001:1c5c).
**2925:1c10 pump_dial_anim** (far) — 6 times: `wait_ticks(0x10); cursor(−3); blit rect {w=0x18, h=0x14, sx = 0x18·k, sy = 0, dx = 0xC0, dy = 0x59}` of the dial bitmap `W[915A:915C]` to the draw page (driver vector 0); `cursor(−1)`; `page_copy_rect(0xC0, 0x59, 0xD7, 0x6C)`.
**2925:1cae pump_strip_draw(page, k)** (far; also called by the Tandy/EGA animation code) — blit rect `{w = W[50F8 + 2k], h = 0x3A, sx = W[5104 + 2k], sy = 0, dx = W[50F0 + 2k], dy = 0x7B}` from `W[B89A:B89C]` (clean background) to `page` with driver vector 2 (`A0B6`).
**2925:1d00 nozzle_draw(P, pic)** — `P` = the locals block; bitmap `bg` = `P.pump == 1 ? P[+8] : P[+4]` (saved strip), `m = pic_get(pic, 2)` (0x457 / 0x456), copy `bg` into a new arena bitmap, mask-blit `m` into it at x = `((~W[AD24] & W[AD62]) − m.w) + 0x18`, y = `0xC − m.h`, then `screen_put_bitmap_mirror(bmp, W[AD62] − 0x10, W[AD64] − 5)`; `ui_reset_fx(2)`.
**2925:1e08 counters_draw(pump)** — redraws the six LED digits (three gallons, three money): for each digit `i` with `(W[915E] ^ W[9160])` bit set, blit a 2×? glyph (driver vector 19, `A0FA`) at (`W[5110 + 2i] + (pump == 1 ? 0 : 0x9B)`, `W[511C + 2i]`) using colour `W[AD44]` (lit) / `W[AD46]`; then `page_copy_rect(x+0x2C … x+0x33, 0x86)` and `(…, 0x8B)`; `W[915E]` is updated.
**2925:1f38 gas_leave(P)** — `if (hand != 1) cursor(1)`; if the nozzle is drawn in the car: `nozzle_draw(P, 0x457)` and charge the fee; if a pump is active: restore its strip (`pump_strip_draw`, `page_copy_rect(W[50F0+2pump], 0x7B, …, 0xB4)`) and charge the fee; fee = `money_add(money < 10 ? −money : −10)` (once); `W[cur_car+0x24] = min(fuel, 0xAA)`;
`W[9160] = 0; W[915E] = 0xFFFF; counters_draw(1); …(2)` (zero the gauge on both pages); `car_draw(2, model, flags, 0, 0xB9, tyre set)`; `ui_pop(1)`; `ui_reset_fx(0)`; `W[1BC8] = 1; drive_to_event()` (back to the garage).
**2925:20be fuel_sound(pump, op)** (far) — `op == 1`: `sound_vec(4)(1)` (`DS:BB9C`) and `W[1F7C] = 4` (pump running); `op == 2`: `W[9160]++; counters_draw(pump)`; `op == 3`: `W[1F7C] = 0; sound_vec(4)(0)`.

### 2925:2714 end_data_load  (new)
`void near end_data_load(void)`: places the two `SR2_end` tables in the shape-set arena and loads them: `W[9714:9716] = (W[9E6C] + W[79E0] : W[9E6E])` (frame table, 0x1F8 bytes), `W[9710:9712] = (W[79E0] + 0x1F8 + W[9E6C] : W[9E6E])` (script, 0x2DC bytes), `W[79E0] += 0x4D4`; `_dos_open(W[14DA] /* "SR2_end", no drive prefix */)`
(failure → `fatal_message("Can't open end data file")`, `platform_exit`), read 0x1F8 bytes → frames, 0x2DC → script (short read → `fatal_message("End Data disk read fail")`, `platform_exit`), close. Formats: FORMATS.md "SR2_END".

### 2925:27da ending_anim  (new) — the `SR2_end` player
`void far ending_anim(void)`; frames table `F` = `W[9714:9716]` (84 × 6 bytes `{u8 w, u8 h, u16 sx, u8 sy, u8 0}`), script `S` = `W[9710:9712]` (4-byte steps `{i8 frame (−1 ends), u8 dx, u8 dy, i8 flag}`).
```c
ui_reset_fx(0);  W[79E0] = 0;
sheet = pic_get(0x49D, 1);                  /* LIB2 #181 sprite sheet */
bg    = pic_get(0x4BB, −1);                 /* LIB2 #211 backdrop */
end_data_load();
screen_put_bitmap(bg, 0, 0);  driver_vec(10)(front ← back);
cur = prev = empty rect (min = 1000, max = −1);  restore = false;
for (s = S; (f = (i8)s[0]) >= 0; s += 4) {
    if (restore) {                                                   /* erase the sprites of the last frame */
        blit rect {w = cur.maxx − cur.minx + 1, h = cur.maxy − cur.miny + 1, sx = cur.minx, sy = cur.miny, dx = cur.minx, dy = cur.miny} from bg to the draw page (driver vector 0);
        prev = cur;  cur = empty;  restore = false;
    }
    fr = F + 6·f;  spr = bitmap_extract(sheet, {fr.w, fr.h, fr.sx, fr.sy}, 2);
    masked_shift_blit(spr → draw page, rect {w, h, 0, 0, dx = s[1], dy = s[2]});   /* 1a21:a9ac, colour 0 transparent */
    ui_reset_fx(1);
    union (cur, prev) with (s[1] & ~7, s[2], ((s[1] + fr.w + 7) & ~7) − 1, s[2] + fr.h − 1);
    flag = (i8)s[3];
    if (flag != 0) {
        wait_vretraces(10);
        page_copy_rect(front ← back, union rect);   restore = true;
        if (flag < 0) {                              /* final picture */
            ui_fx(1);  sheet = pic_get(0x4AB, 2);    /* LIB2 #195 */
            blit {w = 0x88, h = 0x2E, sx = 0x60, sy = 0x68, dx = 0x88, dy = 0x92} from sheet to the draw page; cur = {0x88, 0x92, 0x88 + 0x87, 0x92 + 0x91}; restore = true;
        }
    }
}
wait_vretraces(0x50);  W[79E0] = 0;
```
(`wait_vretraces(n)` = `1a21:18e2`.) 182 steps + terminator `FF 00 00 00`; see FORMATS.md for the table contents.


## File formats (code-verified; see also FORMATS.md)

### `X:HOTRODn.SAV`, n = 0..14 (`X` = `data_disk_check()` drive; written by `save_game_screen`, read by `load_game_screen`)
Raw dump, no magic / checksum. In file order:
1. `0x38` bytes = `DS:A9D8..AA0F`: `+00` save name[16], `+10` driver name[16], `+20` road seed u32, `+24` money i32, `+28` skill AA00, `+29` skill AA01, `+2A` cur_car, `+2C` car_list, `+2E` car free-list, `+30` car_count, `+32` spare_parts, `+34` part free-list, `+36` part_count.
   Pointer fields only mean "non-zero = present"; `pools_init(0)` rebuilds both free lists and counts after the read.
2. if `cur_car != 0`: one CAR BLOCK; if `car_list != 0`: CAR BLOCKs until a car's `+0x28 == 0`; if `spare_parts != 0`: one PART CHAIN.
   CAR BLOCK = 0x2A-byte car record, then PART CHAINs for each non-zero field among `+06, +08, +0A, +0C, +0E, +10, +12, +1C, +1E, +20` (10 slots, in this order; FORMATS.md says "11" by mistake).
   PART CHAIN = 8-byte records `{value, wear, type, next}` as long as `next != 0` (the loader zeroes `next` when it re-links).
3. `DS:479C` 12 bytes (counters), `DS:AB7E` 0x18C (people / opponents), `DS:1F6A` 4 (game clock), elapsed u32 (`rt_ticks − rt_base`), `DS:7DFE` 2, `DS:9B40` 0x80.

### `X:hall_dat`
`u16 count (0..10)`, then `count` records of 23 bytes `{char name[16]; char hours[7]}` (decimal text); the first 0xE6 bytes of the whole table (all ten records) have bit 7 set while stored (`hall_scramble` / `hall_unscramble`). Missing file ⇒ empty hall.

### `SR2_data`, `SR2_end`
`SR2_data`: FORMATS.md (9 blocks + 1277-byte text pool; `hot_data_load` above). `SR2_end`: 0x1F8 + 0x2DC bytes (`ending_anim` above).

## Handoffs

Functions in `port/sr2_modules.csv` under `game_flow` that belong to another module (not specified here):
| address | what | belongs to |
|---|---|---|
| 0001:0090 | `bios_set_cursor_pos(page, row/col)` — `int 10h AH=2` helper | video |
| 0001:0628 | `video_mode_init(mode)` — BIOS mode set per driver (CGA mode 4 + palette, EGA/VGA mode 0Dh, Hercules, Tandy mode 9 + port 3DFh); `main` calls it after `driver_select` | video |
| 0001:07ae | empty far stub, called first by `main` | platform |
| 0001:1c5c | `kbd_remap_restore()` — rebuilds the keyboard hot-key remap list (`DS:3BA0…`, `DS:3B6E` table) for the active menu (`B894`, 9-word elements) | platform / UI |
| 0001:2002 | `tune_start(song, repeat)` — PC-speaker music start, only caller `song_select` 2c57:0002 | sound |
| 0001:2f7f | `bios_mode_b_set(a, b)` — stores `DS:3BF4/3BF5` | video |
| 0001:39cc | far wrapper around `34b0:134a` | runtime |
| 0001:3f5a | `sell_spare_parts` (garage code 5; "Will you take $%d ?" haggling) | garage |
| 0001:a4ea | Bob's/Burger's picture helper (`pic_get(0x47D, 2)` + blit) | race |
| 1a21:b028 | `page_copy_rect(dst, src, x0, y0, x1, y1)` | video |

Other modules' functions this spec calls, with the data passed:
* race / race_render: `drive_to_event` calls `0001:bc88`, `0dcb:0736`, `0001:8d9e`; `event17_run` calls `race_start` (0001:be82), `race_results` (0001:b584, which calls `tournament_result_message` and uses `W[9B3E]`, `B8C2`, `ADA8`, `AD6C`, `ADDA`), `opponent_load` (0001:9a02), `opp_palette_set` (0001:8d0a), `car_max_speed` (0001:c6f6).
  `bob_drive_in` (0001:a626) must set `W[1BC8] = 2`, call `ending_trigger` (a51c) after beating the King, then show msg 0x44 and `W[1BC6] |= 2`.
* **race_render owns two functions the flow depends on**: `0dcb:2866 road_seed_init(int fresh)` (`fresh == 1`: new random road, stores the RNG seed `DS:4A84:4A86` into the save header `DS:A9F8:A9FA`; `0`: restores `DS:4A84:4A86` from there and regenerates the identical road tables) and `0dcb:24d8 road_set_param(int)` (`100` on a new game, `W[7DFE]` on load).
* The demo cursor mover lives in the timer ISR `0001:2246` (sound / platform list): when `W[9730] != 0 && W[20EA] >= 0` it steps the cursor `W[3B32], W[3B34]` by `sgn(target − cursor)` (`0001:2222`) per tick and injects the click described under `demo_step`.
* garage: `newspaper_front` 0001:3e30 (now also runs the classifieds), `garage_screen` 0001:6912, `your_cars`, `paint_job`, `change_tires`, `change_transmission`, `customize`, `engine_bay`, `car_runnable` 0001:7a44, `car_perf_compute` 0001:c294, `spare_add` 0001:426c, `stickers_menu_sr2` 2925:3174 (video list).
* The gas-station helper functions 2925:1ab8…20be and `event15_run` are specified here although `sr2_modules.csv` lists them under `race`.

## Open questions

1. `race_results` return 2, `B[AA10]`, `W[B18E]`, `B[A9C3]`, `W[ADDA]`: meanings inferred from use in Grudge Night (race agent). The first branch of `tournament_result_message` (`9B3E == 0`) is inferred.
2. Gas station: the click sequencing of `event15_run` is read from the disassembly without a debugger run (confidence medium); `counters_draw` digit/bit mapping and `nozzle_draw` pixel offsets were not decoded further.
3. `copy_protection` crash-on-cancel quirk (stack imbalance) — confirm with a DOSBox run that Esc really crashes; the port returns 1.
4. Block `DS:9B40` (0x80 bytes): `9B40` word −1, `9B42…` ad list, `9B5C…9B85` 42 cleared bytes, `9B86 = 0x2A`; meaning belongs to the garage spec (classifieds / stickers). Counters `479E` and `47A6` unused so far.
5. `panel_wipe` / `panel_frame_draw` pixel-exact behaviour is only summarised (cosmetic); port from the decompile if needed.
6. Menu numbers → element tables (menus 0, 1, 2, 3, 6, 0x16, 0x1A, 0x1B, 0x1C, 0x1D, 0x1F, 0x20, 0x23): UI / garage spec; menus 12 and 22 (demo scripts) not identified.
7. Whether a floppy-only install re-asks for `SR2_end` (it is opened without a drive prefix).
