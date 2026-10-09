# Race delta spec (SR2 `SR.EXE`): driving, physics, opponents, Bob's, police, results, events

Scope: the drive engine (town cruise and event trips), Bob's Drive-In and opponent choice, street drag / road /
Aqueduct races, the Grudge Night drag-strip event (event 17), the gas station (event 15), car physics (engine,
gearbox incl. automatic, wear), opponent AI, police and tickets, crash, jail, the King, and the dashboard gauges.
Road / scenery / opponent-sprite rendering is **`port/spec/race_render.md`** (same S3 phase) — referenced, not repeated.

* SR2 segments: `0001:2610`, `0001:51f8`–`534c`, `0001:8c68`–`dca4`, `1a21:0008`–`1614`, `1a21:070e`, `1a21:77f4`,
  `1a21:9b0e`, `2925:000e`–`2104`; plus the physics half of `race_isr 0001:2622` (function owned by the sound spec).
* Builds on **`../StreetRod/port/spec/race.md`** (SR1, cited as "SR1 §x") and `../StreetRod/port/spec/garage.md` §4.14.
* Counts (functions in `race_symbols.csv`): **84** = 5 unchanged-by-reference, 56 changed, 23 new. Of the 87 functions
  listed for `race` in `port/sr2_modules.csv`, 5 were claimed by race_render (`0dcb:1470`, the four `town_walls_*`;
  `0001:d2a8` is race_render's anyway); `0001:51f8 opponent_init` and `0001:534c opponents_init` (listed under garage)
  are specified here at the coordinator's request. **21 functions are also specified in game_flow.md** (opponent_init,
  opponents_init, drive_to_event, the Grudge Night functions `2925:000e…1970` and the gas station `2925:1ab8…2104`);
  the CSV rows say so, S3.7 keeps one owner. The two texts agree except where noted in §5.
* Disassembly: all floating point of SR1 is gone from this module — SR2 computes the physics set-up, top speed,
  AI limits and race statistics with 16/32-bit integers (`_aFldiv 34b0:24f6`, `_aFlmul 34b0:2590`, `/=` helper
  `34b0:2662`, `*=` helper `34b0:2682`, all signed, C truncation). Gauges and clock use `sin256_far 0dcb:031a`
  (race_render §sin256: `sin(deg)` in 1/255). Notation `q(x, n)` = `sgn(x) * (|x| >> n)` (the compiler's
  `cdq/xor/sub/sar/xor/sub` idiom, i.e. division by 2ⁿ truncated toward zero).
* `0001:xxxx` addresses follow port/RE_GUIDE.md §3 (run time `0000:xxxx+0x10`); a `lcall 0000:yyyy` in the
  disassembly is `0001:(yyyy−0x10)`.

## 0. What changed, in one page

1. **Race kind `DS:ADD6`** replaces SR1's `race_type` (`DS:7EC0`): 0 cruise / event trip, 1 street drag race,
   2 Grudge Night time run, 4 Grudge Night rounds (bracket racing with breakout times), 8 **Aqueduct** race (the
   third Bob's race type and the King's pink-slip race), 0x10 **Mulholland Drive** road race. Start segment
   `DS:9AE2` and finish segment `DS:A2AA` come from the kind (§4 phys_reset). Track names as in race_render §1.1.
2. **Physics set-up is integer** and uses three new part slots (exhaust manifold `+1C`, muffler `+1E`, differential
   `+20`) and new tables (9 manifold grades, 9 classes) — `car_perf 0001:c02c`, `car_setup 0001:c294`.
3. **Track position resolution is 1/64 segment** (SR1 1/8): `speed_to_step` returns 64ths, `seg_frac` wraps at
   0x40.
4. Player physics step in the timer ISR is SR1's with five small changes (§3): no input at all in demo, wear and
   rev-limiter clamp off in demo / auto-drive, autopilot uses `track_end`, no demo up-shift counter.
5. New race-start logic: drag-strip "Christmas tree" (amber → green, 80 ticks), bracket start with per-lane
   handicap from the dial-in times, opponent waits for its own green.
6. Opponent AI: new skill formula (`redline + (skill−10)·200`), curve braking by fixed 3 / 30 mph, accelerates through
   curves after the first brake, band index capped at 2, King no longer forced alongside, drag-strip kinds without
   rubber band; street races: the opponent dodges traffic / road-works using track flag arrays; Aqueduct: lane AI.
7. Police: same states; abort distance 7 segments (SR1 5); police car model 0x15, driver record 10.
8. Results: new outcomes 0x101 (crossed the centre line / bumped the starter), drag-strip time display, `$150` bet,
   new fuel use (Aqueduct 1.0 gal), totaled also after a cliff fall, junk value integer.
9. Bob's: no dancer, no "Order" button; three race types; challenge acceptance is a new rating formula with
   per-driver ratings (`+0F/+10` of each person, `DS:AA00/AA01` for the player) that move after every race.
10. The King: three stages (street drag → Mulholland → Aqueduct for pink slips); needs road + Aqueduct wins and a
    Grudge Night win; the old opponent records are now the 22 SR2_DATA **people**.

## 1. Globals owned by this module

Player physics block `phys` = **DS:A100** (SR1 78E0), opponent block = **DS:A1E4** (SR1 79E2); layout as SR1 §3.1
for `+00`…`+BC`, except **`+BE` = steps** (SR1 `+C0`; SR1's `+BE steps_at_stop` is gone) — the block is 0xC0 bytes
and is followed by `bet_kind`/`bet_amount`/`race_state`. Player fields are written `A1xx`, e.g. speed `A100`,
steer_in `A102`, wheel `A104`, blowout_seg `A10A`, tire_slip `A10C`, rpm `A10E`, rpm_up `A114`, rpm_down `A116`,
gear `A118`, gear_dirty `A11A`, ngears `A11C`, redline `A11E`, automatic `A120`, auto_shift_rpm `A122`, auto_mode
`A124`, ratio[] `A134`, grip `A13E`, cur_ratio `A140`, band `A142`, curve `A144`, result `A1BC`, steps `A1BE`.
Opponent: speed `A1E4`, rpm `A1F2`, band_hi `A1F6`, band_up `A1F8`, gear `A1FC`, ngears `A200`, redline `A202`,
auto_shift `A206`, auto_mode `A208`, ratio[0] `A218`, cur_ratio `A224`, band `A226`, curve `A228`, result `A2A0`,
steps `A2A2`.

| DS | Type | Name / meaning | SR1 DS |
|---|---|---|---|
| A1C0 / A1C2 | i16 | bet_kind (0 kicks,1 $10,2 $50,3 $25,4 $150,5 pink) / bet_amount | 79A2 / 79A4 |
| A1C4 | i16 | race_state | 79A6 |
| ADD6 | i16 | race kind (§0) | (7EC0 race_type) |
| 9AE2 / A2AA | i16 | start segment / finish segment (`track_end`) | (0xC8) / 6CAA |
| A2A8 | i16 | opponent has its green (drag strip) — opp_ai runs only then in kind 4 | – |
| B8E4 | i16 | opp_vmax | 6CAC |
| A5F8 | i16 | opp_slack 10 / 16 | 6CAE |
| A2B0 / A33C | i16 | opp_v80 / opp_v70 (police: v60) | 6CB0 / 6CB2 |
| 9700 / 9702 / 9704 / 9706 | i16 | opp_skill / opp_lat_target / opp_band_tmp / opp_shift_rpm | 6CB4 / 6CB6 / 6CB8 / 6CBA |
| 523C | i16 | ai_steps_done (player steps already simulated by opp_ai) | 54D0 |
| 523E | i16 | yield toggle (race_step bump) | 54D2 |
| 8012 / B19C | i16 | AI side / recentre flags | 54D4 / 8648 |
| 8014 | i16 | AI "braked for this curve" | 54D6 |
| 809C / 809E | i16 | alongside counter / ai_brake (ISR) | 54D8 / 54DA |
| 809A | i16 | Aqueduct obstacle latch (player_advance) | – |
| 5240 / AA10 | i16 | holeshot decided / player had the holeshot | – |
| B18E | i16 | Grudge Night: player steps when the reference car reached seg 0x50 (time run) | – |
| 9DC4 | u8 | tires_blown | 787C |
| 72E0 | u16 | squeal | 58DA |
| A55E / A560 / A562 | i16 | seg / seg_frac (**64ths**) / seg_step | 7D30 / 7D32 / 7D34 |
| A564 / A56A | i16 | lat / heading | 7D36 / 7D3C |
| A56E | i16 | vs_king | 7D40 |
| A9C6 / A9C8 / A9CA / A9CC | i16 | opp_seg / opp_seg_frac (64ths) / opp_seg_step / opp_lat | 7E8A / 7E8C / 7E8E / 7E90 |
| A33E | i16 | lat at the end of the last opp_advance_steer | – |
| AB2C / 9ACA | i16 | police_state / police_seg | 7F88 / 755A |
| 5148 / B168 | i16 | stats_done / player_ahead | 53E6 / 8632 |
| ADAE:ADB0 | i32 | distance (Σ speed per step) | 82BC |
| 9ADE / B8C2 / ADDA | i16 | avg_speed / race time in **tenths of s** (int) / pct_covered | 7644 / 8BC4 (float) / 82C8 |
| 9B3E | i16 | player_won | 7676 |
| 9124 | i16 | cruise_gain | 6C82 |
| 9118 / 911A:911C | i16 / u32 | Grudge rounds: player tree state (0,1 amber,2 green,3 moving) / its tick deadline | – |
| 911E / 9120:9122 | i16 / u32 | same for the opponent lane | – |
| 9ADC / ADA8 | i16 | opponent / player dial-in (breakout) time, tenths | – |
| 9B0A / A80A / A7AA / AB00 | | opp_model / opp_rec (person*) / opp_model_ptr (car rec*) / opp_index | 7648 / 7D84 / 7D7A / 7F82 |
| AB02 | i16 | opp_present | 7F86 |
| AD6E / AD70 / AD72 / AD74 | i16 | opponent car picture / paint / tyre grade / face | 827A / 827C / 827E / 8280 |
| 9128 / 912A[21] / 5034 | i16 | cand_count / cand[] / last_pick | 6C86 / 6C8A / 51F8 |
| 9126 / 502E | i16 | car-hop state (−1,0,1) / car-hop gate counter | 6C84 / – |
| 7916 | i16 | opponent shown as "no time for you" (King) | – |
| 9154 | i16 | first visit flag at Bob's | 6CA2 |
| AD66 | i16 | last bet button | 8278 |
| 479C / 479E / 47A0 / 47A2 / 47A4 / 47A6 | i16 | counted races / street-drag wins / Mulholland wins / Aqueduct wins / Grudge Night wins / King stage 0..2 (the 12 bytes saved at `DS:479C`) | 4970 / 4972 / 4974 / – / – / 4976 |
| AA00 / AA01 | i8 | player ratings 1..8 (race wins / holeshots); save header bytes +0x28/+0x29 | – |
| AD03 | u8 | = person[21].status, used as "King ready" | 817D |
| AD6C | i16 | Grudge Night round (−1 time run, 0..3) | – |
| 90C8 / 9106 / 90CA[10]×6 | | Grudge field: opponent entry / player entry / entries `{i16 person (−1 = player), i16 state, i16 dial-in}` | – |
| 20EC | i16 | races started at Bob's (incremented by challenge) | – |
| 1FFC | i16 | 1 while race_start runs from challenge / event17 | – |
| A7A2 | i16 | start lights bit mask (read by race_render): 1 green (street), 0x10/0x20 player amber/green, 2/4 opponent amber/green; initial 0x88 (kind 2) / 0x92 (kind 4), 9 after staging | 7D66 (0/1) |
| B164 | i16 | gear label cache | 8630 |
| 9DCC | i16 | has_clock (automatic transmission) | 58E6 |
| AB28 | i16 | dash type 0..2 = byte 1 of the model's SR2_DATA block-2 record | – |
| B7B2 | i16 | start panel id 0x815 (kind&6) / 0x813 (kind 8) / 0x816 | – |
| ADD0 | i16 | wheel_shown | 82C2 |
| 8806/8808, 880E/8810 | i16 | speedo / tach centre (x, y) of the current dash | – |
| 1C3A | i16 | needle colour (−1 until first use, then 0xB VGA/−6 driver, else 3) | 02D2 |
| 9156 / 9158 / 915A:915C / 915E / 9160 | | gas station: nozzle hot spot on / pump hot spots on / meter picture / shown meter bits / meter counter | – |
| 9A46+4p / 9AC8+4p | far* | gas station: saved pump images (p = 1, 2) | – |

Ticks / input / system globals used here (owned by platform / sound, remapped from SR1): ticks `1F66` (05F8),
ticks18 `1F6E` (0600), tick_div4 `1F76` (0608), phys_div3 `1F77` (0609), phys_busy `2008` (06E8), fire_prev `2009`
(06E9), limiter_count `2006` (06E6), menu_timeout `1F3A` (05CC), game_clock `1F6A` (05FC), racing `B7AE` (8ACA),
cpu_class `B7B4` (8ACC), demo `9730` (0610), auto_drive `1F82` (0612), sound_alt `1BCA` (0286), drive_dest `1BC8`
(0284), game_end_flags `1BC6` (0282), video driver `AD1C` (8236), joystick `3B20/3B22` (4718/471A), dead zones
`3B2A/3B2C/3B2E/3B30` (4722…4728), joy_buttons `3B24` (471C), joy_present `3B25` (471D), steer_step `3B32` (472A),
pedal_step `3B34` (472C), key_x/key_y `3B36/3B37` (472E/472F), fire `3B3A` (4732), keys_idle `3B48` (4740),
shift_request `3BD4` (47CD), mouse dx/dy `1F50/1F52` (05E2/05E4), mouse_left/right `1F5C/1F5E` (05EE/05F0),
mouse_present `1F60` (05F2), drive_abort `3BF6` (47EE), saved mouse position `3B4E/3B50` (4746/4748), `1F44` (05D6),
money `A9FC:A9FE` (7EAA), car `AA02` (7EB0), car2 `AA04` (7EB2), spare-parts head `AA0A`. Track arrays are far word
arrays owned by race_render (segment in a DS word): `CURVE` = `[871C]:372E+2s`, `TRACK_C` = `[871A]:3EEA+2s`,
`TRACK_D` (Aqueduct flags) = `[871E]:4E66+2s`, cruise stop flags `[86E4]:27B8+2s` and `[86E6]:2F74+2s`, side
obstacles `[8720]:27B6+2s` / `[8722]:2F72+2s`; road edges at the player `A7DA` (left) / `B8D2` (right) (SR1
7D82/8BD0), 4 segments ahead `985C` (left) / `A558` (right) (SR1 70C0/7D2E); `A7D4` opp_pose (7D7C), `A7D6`
bump_allowed (7D7E), `B8C0` opp alongside (8BC2), `9A48` rear-end contact (72F2), `7BA2` on shoulder (5E0E),
`7FCE` Aqueduct bounce (opponent frozen), `A7AC` start panel active, `BBF2` cliff fall (race_render §1).

### 1.1 Data records used (SR2_DATA, FORMATS.md)

**Car record** (garage spec; only the fields read here): `+00` value, `+02` model, `+04` i8 paint, `+05` class,
`+06` transmission*, `+08` engine*, `+0A` manifold*, `+0C/+0E/+10` carburettors*, `+12` tyres*, `+14/+15`
transmission state bytes, `+16…+1B` engine-bay state bytes, `+1C` exhaust manifold*, `+1E` muffler* (may be 0),
`+20` differential*, `+22` byte engine link (SR1 `+1F`), **`+23` i8 ignition timing** (SR1 `+20`), **`+24` fuel**
(0.1 gal, ≤ 0xAA; SR1 `+22`), **`+26` flags** (`0x2000` roof chopped, `0x4000` rear bumper off, `0x8000` front bumper
off; SR1 `+24`), `+28` next car. Part record `{+00 value, +02 wear, +04 catalogue index, +06 next}`; grade
`G(p) = (int8)DS:4504[10·p->type]`, catalogue `DS:4502` (FORMATS.md).

**Car model** = SR2_DATA car record `DS:A80C + 20·m` (m = 0…21): `+00` price, `+04` u8 class (0…8), `+08` spec A
(bits 1-3 make, 4-5 tyre grade, 6-7 transmission grade, 8-9 engine grade, 10-13 manifold grade, 14-15 carb grade),
`+0C` text ptr (first line = model name), `+10` picture, `+13` u8 = high byte of the flag word: **reference
quarter-mile time** used by the Grudge Night field. Overlay record `DS:BAAC + 10·m` = `{u8, u8 dash type, roof,
scoop, rear bumper, front bumper}` (words; 0 = the model cannot have it).

**Person record** (opponent; `DS:AB7E + 18·i`, i = 0…21, 21 = the King; replaces SR1's opponent record):
`+00` i8 `a` (skill: AI shift point `redline + (a−10)·200`; initial holeshot rating), `+01` i8 `b` (steadiness:
bump chance `rand(((15−b)>>2)+2)`; initial win rating), `+02/+03` unused here, `+04` face picture, `+06` name ptr,
`+08` car model, `+0A` i8 paint, `+0B` i8 status (−1 gone, 0 unknown, 1 met), `+0C` i8 races vs the player, `+0D` u8
customisation (1 front bumper, 2 rear bumper, 4 roof, 0x20 scoop), `+0E` car re-rolled, `+0F` i8 win rating 1…8,
`+10` i8 holeshot rating 1…8, `+11` unused.

## 2. In-exe tables (DGROUP)

| DS | Content | SR1 |
|---|---|---|
| 521E | tyre slip `[tyre*5 + wear/2000]`: 2 5 8 11 14 · 0 2 4 6 8 · 0 0 1 2 4 | 54B2 (same) |
| 5242 | gear spread % `[tr*5+g]`: 0 25 50 100 0 · 0 25 50 100 0 · 0 25 50 75 100 · 0 30 50 80 100 | 54DC (same) |
| 526A | torque `[e*5+g]`: 500 600 300 100 70 · 600 700 350 120 90 · 600 760 380 140 90 | 5504 (same) |
| 5288 | band limits `[e*2+k]`: 2900 4500 · 2900 5000 · 2500 5200 | 5522 (same) |
| 5294 | intake bonus `[carb*9 + man]` (man = manifold grade 0…8): c0: 0 80 0 0 0 0 0 90 100 · c1: 0 40 0 70 60 50 60 90 100 · c2: 0 0 0 40 80 70 85 90 100 | 552E (3×5) |
| 52CA | engine bonus `[e]`: 0 50 100 | 554C |
| 52D0 | transmission bonus `[tr]`: 0 33 66 100 | 5552 |
| 52D8 | redline `[e]`: 4500 5000 5200 | 555A |
| 52DE | gears `[tr]`: 3 3 4 4 | 5560 |
| 52E6 | grip base `[tyre]`: 9 8 5 | 5568 |
| 52EC | class weight `[class 0…8]`: 46 50 55 68 73 78 91 96 100 | 556E (40 44 48 60…) |
| 4E6E | carburettors per manifold grade: 1 1 3 2 2 1 2 0 0 | 5126 |
| 4790 | initial wear of category 2 (= −128), used for temporary stock parts | 496A |
| 46F6 | wear of a bought tyre (50) | 4946 |
| 501A / 5024 | cruise_accel `[gear]` 42 26 12 8 4 0 / cruise_ratio 0 200 125 71 56 0 | 51D8 / 51E2 |
| 8016 | speed_to_step, 131 bytes `[mph 0…130]` (64ths of a segment per frame): 0 8 8 8 9 9 9 10 10 11 11 11 12 12 12 13 13 14 14 15 15 16 16 17 17 18 18 19 19 20 20 21 21 22 22 23 23 23 24 24 25 25 26 26 27 27 28 28 29 29 30 30 31 31 32 32 33 33 34 35 36 36 37 37 38 38 39 39 40 40 41 41 42 42 43 43 44 44 45 45 46 47 48 49 50 51 52 53 54 55 56 57 58 59 60 61 62 63 65 67 69 71 73 75 77 79 81 83 85 87 89 91 93 95 97 99 101 103 105 107 109 111 113 115 117 119 121 123 125 127 127 | d544 code |
| 5038 | refusal message numbers `[(kind≠1)*5 + r]`: 0x32 0x33 0x34 0x35 0x2A · 0x2A 0x2B 0x2C 0x2D 0x2E | 51FC |
| 504C | accept message numbers `[(kind≠1)*3 + r]`: 0x36 0x37 0x38 · 0x2F 0x30 0x31 | 5210 |
| 5054 | King refusal `[king_status]` (index 2…6 used): 0x39 0x3A 0x3B 0x3C 0x3D | (ids in code) |
| 512A | win message `[bet]`: 0x49 0x4A 0x4C 0x4B 0x4D 0x4E | 53C8 |
| 5136 | crash cracks 3 × `{pic, x, y}`: {0x804, 0x40, 0x54} {0x806, 0xD0, 0x52} {0x805, 0x110, 0x7B} | 53D4 |
| 506A | jail decorations 7 × `{DS ptr to far pic, sx, sy, w, h, dx, dy}` (5 × pic 0x483 16×149 at x 40/96/152/208/264, y 5; pic 0x481 32×149 from sx 0 at x 0 and from sx 32 at x 288) | 5338 |
| 4D1E | paint colours 6 × `{EGA reg 0Ch value, reg 03h value, u8}` (3-byte stride) | 50C2 (2-byte) |
| 1C0A/1C10/1C16/1C1C | tach centre x 126 220 203 / centre y 49 46 40 (+`B8C4`) / tip radius 8 11 7 / inner radius 4 4 2 `[dash type]` | 0x91/0x2A, far table |
| 1C22/1C28/1C2E/1C34 | speedo centre x 170 172 157 / centre y 41 40 46 (+`B8C4`) / tip radius −,15,16 / inner radius −,4,3 | 0xC4/0x2A, far table |
| 1C3C / 1C42 | clock hand base radius 4 5 2 / hand length 11 11 7 | constants |
| 1C50…1CB3 | steering-wheel knob `{u8 x, u8 y}` for w = −24…24 (base `1C80` = w 0) | 031E |
| 1CB4 | wheel picture rows in the spare page: 102 132 162 | – |
| 78CE | wheel pictures `[dash type*3 + i]`: 2072 2073 2074 · 2021 2024 2023 · 2077 2076 2075 (LIB3) | 58E8 |
| 1CD0 | shifter records 3 × `{n, gate pic, knob pic, label h, near* positions}`: {2,2028,2031,8,1CBA} {4,2026,2029,19,1CBE} {5,2027,2030,19,1CC6} | 036E |

## 3. Player physics — `race_isr 0001:2622`, part from `28fa` (function owned by the sound spec)

= SR1 §4.7 `phys_step` (`0000:253b…2b8d`) with the DS remap of §1 and these differences (all verified):

1. The whole race part (from the `ticks18` increment at `28fa`) is skipped while `DS:1F7A != 0` (1 until
   `platform_install` clears it; always 0 in a race).
2. Input block: `if (keys_idle == 0 && demo == 0) { keyboard; joystick (if joy_present); mouse (if mouse_present) }`
   — SR1 read the keyboard also in demo and tested `demo` only for joystick / mouse; `DS_6C4A` is no longer set.
3. Autopilot (`demo || auto_drive`):
   ```c
   if (p->rpm_up*4 + p->rpm < 0x1644) {
       if ((race_kind & 6) || opp_seg > seg || seg > track_end - 0x3C) gas = 1;   /* track_end = DS:A2AA */
   } else if (p->gear < p->ngears && DS_AD0A) { shift = 1; gas = 1; }          /* no demo_shifts++ */
   ```
   (the following two lines `rpm < 2000 …` and `automatic && gear > 0 → shift = 0` are unchanged).
4. Past the finish: `if (seg > track_end)` (SR1 used the constant 0x17C/0x3FC).
5. Wear: the block `if (rpm > 5000) {eng.wear++; trn.wear++}  if ((ticks18 & 0xF) == 0) eng.wear++;  if (eng.wear >=
   0x26AC) {result |= 5; race_ctrl_reset();}` runs only when `!demo && !auto_drive` (SR1 always).
6. Rev limiter: `if (p->rpm > 0x1644) { rpm = 0x1644; if (w/1000 … < ++limiter_count) blown }` runs only when
   `!demo && !auto_drive` (in demo / auto-drive the rpm is not clamped at 5700 there; the automatic 0x163A clamp
   stays).

Everything else — order of the steps, gas / brake / neutral coasting (`speed--; rpm = (speed + 0x32) * 0x1E`, equal to
SR1), shift and stall logic, transmission wear `+0xF/+3` and blow test `tw/100 + 100 > rand(0xCCD)`, band choice,
braking `speed -= (speed>>4) + 2` (also on the shoulder `7BA2` above 8 mph), steering and wheel decay, the collision
call (now `0dcb:148a road_edge_collision`, race_render) with `result |= 0x21` — is unchanged. The race uses
`race_ctrl_reset 0001:2610` (below) where SR1 called `race_stop_inputs`.

## 4. Functions (by address)

### 0001:2610 race_ctrl_reset
Changed. SR1 `race_stop_inputs 0000:2374` (SR1 §4.7) minus the `steps_at_stop` copy:
`DS_1F44 = 0; player.wheel = player.steer_in = player.rpm_down = player.rpm_up = 0;` (far).

### 0001:51f8 opponent_init

> **Secondary view (S3.7):** owned by `garage.md`; on any conflict the owner's section wins.
Changed (SR1 `0000:542e`, garage.md §4.14). `far void opponent_init(int i, int *pool)` — `pool[0]` = count,
`pool[1..]` = car model numbers, −1 = taken. Person `p = DS:AB7E + 18·i`:
```c
do c = rand(6); while (c == 5 && rand(2) != 1);        /* colour 5 only half as likely */
p->paint = c; p->status = p->races = p->custom = p->rerolled = 0;   /* +0A, +0B..+0E */
do k = rand(pool[0]); while (pool[k+1] == -1);
p->model = pool[k+1]; pool[k+1] = -1;                  /* +08 */
int ch = (CLASS(p->model) >> 1) + 1;                   /* class byte DS:A810+20m, unsigned */
if (OVL(p->model).rear > 0 && OVL(p->model).front > 0) p->custom |= (rand(25) < ch) ? 3 : 0;   /* BAB2 / BAB4 */
if (OVL(p->model).roof > 0)                           p->custom |= (rand(30) < ch) ? 4 : 0;   /* BAAE */
p->custom |= (rand(20) < ch) ? 0x20 : 0;               /* scoop, no model test */
p->win_rating      = 7 - (p->b >> 1);                  /* +0F, byte sar */
p->holeshot_rating = 6 - (p->a >> 2);                  /* +10 */
```
No sticker (SR1 had one); the rand order above is the call order.

### 0001:534c opponents_init

> **Secondary view (S3.7):** owned by `garage.md`; on any conflict the owner's section wins.
Changed (SR1 `0000:5584`). Builds three pools over the car models 0…20 (`DS:A810` class byte, stride 20, stop
before `A9B4`): class < 3 → pool A, 3…5 → pool B, ≥ 6 → pool C (each `{count, models…}` on the stack). Then
`opponent_init(i, A)` for i = 0…6, `(i, B)` for 7…13, `(i, C)` for 14…20 (in that order), and `DS:AD03` (= King's
status) = 0. No demo handling.

### 0001:8c68 drive_to_event
New (replaces SR1 `drive_to 0000:8d26`, SR1 §4.1). `far void drive_to_event(void)`; callers game_loop, bob_drive_in,
event_drive_home, gas_leave.
```c
if (drive_dest == 0) return;
int save = race_kind; race_kind = 0;
car->fuel = car->fuel > 2 ? car->fuel - 2 : 0;         /* +24, 0.2 gal */
(*DS_A0CA)();                                          /* clear pages */
ui_mouse_mode(-4);                                     /* 1a21:2d90 */
dash_type = (int8)DS_BAAD[10 * car->model];            /* AB28 */
has_clock = (G(car->trans) < 1);                       /* 9DCC */
racing = 0;                                            /* B7AE */
phys_reset(&player);
road_state_init();                                     /* 0dcb:0736, race_render */
race_state = 0;
drive_run();
race_kind = save;
```
(SR1 cleared fire / buttons / drive_abort here; SR2 does it in drive_run.)

### 0001:8d0a opp_palette_set
= SR1 `opp_palette_set 0000:8d9c` (SR1 §4.1), recompiled. DS remap: 8236→AD1C, 51AC/51AD→4FCC/4FCD,
044C→1DB2, 0443→1DA9, 8249→AD2F, call 0f38:1f4b→1a21:1b9a; colour table 50C2 (2-byte entries) → **4D1E with a
3-byte stride** (`DS:4D1E[3i]`, `[3i+1]`).

### 0001:8d9e drive_run
New body (SR1 `drive_run 0000:8e2d`). `far int drive_run(void)`:
```c
if (racing) (*DS_A0CA)();
lights = 0 /*A7A2*/; DS_AD0A = 0;
int oc = DS_AD70;                                      /* opponent paint */
pal_reg12_13(0x1DC8);                                  /* 1a21:198a: game palette */
set_paint_palette(car->paint);                         /* 0001:672c */
if (racing) opp_palette_set(oc);
start_panel = (race_kind & 6) ? 0x815 : race_kind == 8 ? 0x813 : 0x816;     /* B7B2 */
DS_985E = DS_AA9C = DS_A2B8 = DS_B8BE = DS_A7A6 = DS_BB8A = DS_A5FA = DS_B8E6 = DS_9A5A = DS_AD6A
        = DS_9B14 = DS_B166 = DS_9A56 = DS_BBF2 = 0;  /* road-mode state, race_render */
wait_ticks(0xF);
fire = joy_buttons = 0 (bytes 3B3A, 3B24); mouse_left = 0; drive_abort = 0;
wait_ticks(0x19);
drive_abort_poll();                                    /* 0001:8c2c (platform): mouse/joystick button, Esc / 0x1F */
DS_7FCE = DS_7B9C = DS_7F2C = 0;
int r;                                                 /* uninitialised in the skip path (quirk) */
if ((DS_4B64 || drive_abort) && !racing && drive_dest != 4) {   /* 4B64 is set by hot_data_load */
    race_view_reset(); DS_79E0 = 0;                    /* trip skipped: no driving at all */
} else {
    if (!DS_4FCE) { lib_read_dir(3); DS_4FCE = 1; }    /* LIB3 directory, once */
    r = race_run();
}
racing = 0;
(*DS_BB90)(0); (*DS_BBB8)(0);                          /* engine sound off, siren off */
if (DS_3B4E || DS_3B50) { DS_3B32 = DS_3B4E; DS_3B34 = DS_3B50; DS_3B4E = DS_3B50 = 0; }
return r;
```
The skip path returns garbage in the original; the port returns 0.

### 0001:8ef4 race_logic
Changed (SR1 `0000:8ea8`, SR1 §4.3). `far uint race_logic(void)`; `R = player.result | opp.result` below means
`DS_A2A0 | DS_A1BC`. Common start sequence `START(text)`: `DS_1F44 = 0; race_isr_enter(); input_reset(); race_state =
1; status_print(-1, text, 0);`.

**race_kind == 2 (Grudge Night time run):**
* state 0: `START("Get ready !"); tree = ticks + 0x6E; return R;` (no random delay; `tree` = `DS:1F3A`).
* states 1 and 2: if `player.speed > 0` → `race_isr_leave(); player.result |= 0x81; return 1;`. If `ticks >= tree`
  (signed 32-bit): `old = race_state++;` if `old == 1` { `lights = 0x10; tree = ticks + 0x50;` } else
  { `lights = 0x20; status_print(-1,"Go !",0);` }; in both cases `opp.steps = 0; player.steps = 0; DS_A2A8 = 1;`.
  Return R.
* state 3: `race_step(); if (player.result || opp.result) return 1; if (player.speed > 0) { status_print(-1,"",0);
  race_state = 4; } return 0;`
* state 4: `if (player.result || opp.result) return 1; race_step(); return 0;`

**race_kind == 4 (Grudge Night round):**
* state 0: `START("Get ready !"); P_state = O_state = 0 /*9118, 911E*/; O_time = P_time = ticks + 0x6E;
  d = DS_9ADC - DS_ADA8 /* opp dial-in − player dial-in, tenths */; DS_9124 = d;`
  `if (d > 0) P_time = O_time + 13*d; else O_time -= 13*d;` (32-bit; 13 ticks per tenth: the faster dial-in starts
  later). Return R.
* state 1: if `player.speed > 0` → jump start as above. Falls into state 2.
* state 2: `race_step();` then
  ```c
  if (P_state < 2 && ticks >= P_time) {                 /* player's tree */
      if (P_state) { P_state = race_state = 2; status_print(-1,"Go !",0); lights |= 0x20; opp.steps = player.steps = 0; }
      else         { P_time = ticks + 0x50; P_state = 1; lights |= 0x10; }
  }
  if (O_state < 2 && ticks >= O_time) {                 /* opponent's tree */
      if (O_state) { O_state = 2; DS_A2A8 = 1; ai_steps_done = player.steps; lights |= 4; }
      else         { O_time = ticks + 0x50; O_state = 1; lights |= 2; }
  }
  if (P_state == 2 && player.speed > 0) { status_print(-1,"",0); P_state = 3; }
  if (P_state == 3 && O_state == 2) race_state = 3;
  return R;
  ```
* state 3: `if (player.result) return 1; if (opp.result) { if (seg - opp_seg > 7) return 1; race_state = 4; return 0; }
  race_step(); return 0;`
* state 4: `race_step(); return (opp_seg - seg > 6) ? 1 : 0;`

**racing, other kinds (1, 8, 0x10):** as SR1 except: state 0 `START("Get ready !"); tree = ticks + rand(0x67) + 0x6E;`
(no engine-start sound call; the ISR install is `race_isr_enter 0001:20a0`); state 1 green sets `lights = 1`.
States ≥ 4 return `race_state − 3` (unreachable).

**cruise (`!racing`):** as SR1 with: state 0 `START("Cruising in town...")`, then `player.gear_dirty = player.gear = 1;
if (player.auto_mode == 1) player.auto_mode = 2; cruise_gain = cruise_accel[0];` (no `skip_fx` / engine sound).
State 1: `drive_abort_poll();` (always, also in demo; SR1 polled key / mouse / joystick only when `!demo`) then the
stop test reads `[86E4]:27B8+2·seg` and `[86E6]:2F74+2·seg` (bit 0) and `drive_abort`; the arrival branch clears
`DS_3B3E` and `fire`, calls `race_isr_leave()` and returns 1. The rpm / gear table logic is unchanged (tables 501A /
5024). States ≥ 2 return `race_state − 1` (unreachable).

### 0001:9432 waitress_toggle
Changed (SR1 `0000:914c`). `near int waitress_toggle(int mode, int arg, int pct_in, int pct_out)`:
```c
int drawn = 0;
if (mode == 0) {
    if (carhop == 0 /*9126*/ && DS_502E >= 3 && rand(100) < pct_in) {
        ui_mouse_mode(-4); carhop_anim(0, arg); drawn = 1; carhop = 1;      /* 1a21:945a */
        if (rand(100) < pct_out) { wait_vretraces(100); carhop_anim(1, arg); carhop = 0; DS_502E = 0; }
    }
} else if (mode == 1 && carhop == 1 && rand(100) < pct_in) {
    ui_mouse_mode(-4); carhop_anim(1, arg); drawn = 1; carhop = 0; DS_502E = 0;
}
if (drawn) ui_mouse_mode(-2);
return drawn;
```
New: the car-hop only appears after `DS:502E` (incremented by opponent_show) reached 3.

### 0001:9506 str_drop_last_word
= SR1 `0000:9256` (SR1 §2), recompiled; no globals.

### 0001:953c opponent_show
Changed (SR1 `0000:92a4`, "likely" there). `near void opponent_show(int mode)`:
```c
int noTime = DS_7916;
char s[80]; strcpy(s, opp_rec->name /*+06*/); strcat(s, DS:5030 /* "  " */); strcat(s, opp_model_ptr->text /*+0C*/);
int jail = (drive_dest == 6);
if (mode == 4) carhop = -1;
opp_present = (mode == 2);
if (mode == 1 && !waitress_toggle(0, opp_present, demo*8 + jail*10 + 13, 10))
    waitress_toggle(1, opp_present, 20, 0);
if (s[strlen(s)-1] == '.') s[strlen(s)-1] = 0;
while (text_width(s) > DS_AD3A - 15) str_drop_last_word(s);     /* 1a21:1954 */
status_print(-1, s, 0);
car_draw(mode, opp_model, opp_rec->custom /*+0D*/, 1, 0xA8, 0x4D30 + 8*DS_AD72);   /* 1a21:89fa */
int m = noTime ? 2 : mode;
if (m == 2) { DS_502E++; status_print(-1, "", 0); }
if (m == 4) carhop = 0;
opp_present = (m != 2);
if (!jail || !opp_present)
    if (!waitress_toggle(0, opp_present, demo*8 + jail*10 + 13, opp_present ? 0 : 8))
        waitress_toggle(1, opp_present, 0x28, 0);
```

### 0001:975e king_status
Changed (SR1 `0000:9556`). Counters: `S = DS_479E` drag wins (not used here), `M = DS_47A0` Mulholland wins,
`Q = DS_47A2` Aqueduct wins.
```c
if (no person 0..20 has status != -1) return 0;
int t = M + Q;
if (t < 3) return 1;
if (t >= 7 && M < 6 && Q < 6) return 3;
if (t < 7 && M < 6 && Q < 6) return 2;
if (M < 6) return 5;
if (Q < 6) return 6;
if (DS_47A4 == 0) return 4;                     /* never won Grudge Night */
return ((DS_479C >> 1) <= t) ? 0 : 3;           /* ≥ half of the counted races won */
```
Messages (challenge): 2 → 0x39 "I got no time for amateurs like you!", 3 → 0x3A "You're doing OK kid, but you're
gonna…", 4 → 0x3B "You're gonna hafta win …", 5 → 0x3C, 6 → 0x3D ("…win more …"); 1 → no message.

### 0001:9802 king_chance
Changed (SR1 `0000:95f1`): returns 0 when `opp_model == 0x15` (SR1: 0x19) — in SR2 0x15 is both the King's and the
police car, so the King never shows up right after either. Otherwise unchanged: status 0 → `DS_AD03 == 1 ? 10 : 50`,
1 → 5, 2 → 8, else 12.

### 0001:9848 opp_sort
Changed (SR1 `0000:9646`, SR1 §4.14). Same algorithm over the people 0…20 with: `spd[i] = |car_max_speed(person[i].model)
− car_max_speed(-1)|` (the player's speed is computed first, then the opponent's), `cls[i] = |CLASS(person.model) −
CLASS(car->model)|` (class bytes `DS:A810+20m`), `used[i] = (person.status == −1)`; limit `all ? 21 : 9`; the
selection threshold starts at 32000 and the tail-replacement quirk (index by list position, k > 6) is kept. **No demo
injection** (SR1 added `DS_4FF4` twice in demo). Lists: `DS:9128` count, `DS:912A` entries.

### 0001:999a pick_opponent
= SR1 `0000:984e` (SR1 §4.14), recompiled. DS remap: 6C86→9128, 6C8A→912A, 51F8→5034; rand 0f38:5eb6→1a21:6180.

### 0001:9a02 opponent_load
Changed (SR1 `0000:98ba`). `far void opponent_load(int i)`:
```c
opp_index = i; opp_rec = &person[i]; opp_model = opp_rec->model; opp_model_ptr = &model[opp_model];
if (i == 0x15) vs_king = 1;
else {
    vs_king = 0; opp_rec->status = 1;
    int ch = (CLASS(opp_model) >> 1) + 1;
    if (OVL.rear > 0 && OVL.front > 0 && opp_rec->races > 8) opp_rec->custom |= (rand(25) < ch) ? 3 : 0;
    if (OVL.roof > 0 && opp_rec->races > 8)                 opp_rec->custom |= (rand(30) < ch) ? 4 : 0;
    opp_rec->custom |= (rand(50) < ch) ? 0x20 : 0;          /* scoop, any time */
}
DS_AD6E = opp_model_ptr->picture;                           /* +10 */
DS_AD72 = (opp_model_ptr->spec_a & 0x30) >> 4;              /* tyre grade → wheel set */
DS_AD70 = opp_rec->paint;  DS_AD74 = opp_rec->face;
```
No sticker logic, no palette call (moved to opponent_select).

### 0001:9b26 opponent_select
Changed (SR1 `0000:9a27`). `near void opponent_select(int i, int mode, int flag)`: `opponent_load(i);
set_paint_palette(DS_AD70); if (flag == 1) DS_7916 = 1; if (flag == 2 && opp_index == 0x15) DS_7916 =
(king_status() == 1); opponent_show(mode);`

### 0001:9b78 opp_known_list
Changed (SR1 `0000:9a83`). `near void(int *names, int *idx, int *count)`: `person[W(DS:4CAC)].status = 0` (DS:4CAC
holds 0, so person 0); `DS_AD03 = (king_status() == 0)` — `AD03` is the King's status byte, so the King appears in the
list exactly when he can be challenged; then every person 0…21 with status 1 is listed (`idx[n] = i; names[n] =
person.name`), `*count = n`.

### 0001:9be0 opponent_search
Changed (SR1 `0000:9af4`). `status_print(-1,"",0); opp_known_list(DS:A61C, list, &n); b = list_box_run(0x10, 0,
DS:A61C, 0, n)` (1a21:55ea); `b == -4` → return 0; `b == 1002` → `msg_box(0x29 "What's his name?")`, return 0;
`b == -3` (picked) → `if (opp_present) { status_print(-1,"",0); opponent_show(2); } opponent_select(list[DS_B88E],
mode, 0); return 1;` any other code → returns the leftover `ax` (= b + 3 − 0x3ED; port: 0).

### 0001:9c8e challenge_accept
Rewritten (SR1 `0000:9bc5`). `near bool challenge_accept(int bet)`:
```c
int big = (bet >= 3);
int dr = drivetrain_rating(opp_model) - drivetrain_rating(car->model);  /* stock model of the player's car */
int dv = car_max_speed(opp_model) - car_max_speed(-1);                  /* -1 evaluated first */
int dw = opp_rec->win_rating - DS_AA00;                                  /* i8 fields */
int dh = opp_rec->holeshot_rating - DS_AA01;
int c;
if (!dr && !dv && !dw && !dh) c = 50;
else {
    int wa = big ? 1 : 2, wb = big ? 2 : 1;
    int den = (|dr| + |dw|) * wb + (|dv| + |dh|) * wa;
    long num = (long)((max(dh,0) + max(dv,0)) * wa + (max(dw,0) + max(dr,0)) * wb) * 100;  /* 16-bit sum, 32-bit product */
    c = (int)(num / den);                                                 /* idiv 32/16 */
}
if (bet <= 1) c *= 2;
else if (bet >= 4) { if (c > 50) c += c >> 2; else if (c < 50) c -= c >> 2; }   /* sar */
if (demo) c += (c >> 2) + 10;
return rand(100) <= c;
```
(c = share of the opponent's advantages in percent; "just for kicks" and $10 are accepted twice as easily.)

### 0001:9e3c challenge
Changed (SR1 `0000:9d6d`). `near int challenge(int king)`; returns 2 when a race was run.
```c
ui_reset();                                                   /* 1a21:9b26 */
if (king) {
    int s = king_status();
    if (s) { if (s != 1) msg_box(W5054[s]); return 0; }
    switch (king_stage /*47A6*/) {
    case 0: if (car->fuel < 7)    { msg_box(0x41); return 0; } race_kind = 1;    msg_box(0x3E); bet_kind = 0; break;  /* "Let's hit the drag race!" */
    case 1: if (car->fuel < 0x11) { msg_box(0x41); return 0; } race_kind = 0x10; msg_box(0x3F); bet_kind = 0; break;  /* "…mulholland drive!" */
    case 2: if (car->fuel < 0xC)  { msg_box(0x41); return 0; } race_kind = 8;    msg_box(0x40); bet_kind = 5; break;  /* "Get your pink slip out," */
    default: goto run;                                         /* stage > 2: no message, kind unchanged */
    }
    bet_amount = 0;
run:
    DS_1FFC = 1; race_start();
    game_clock += race_kind == 1 ? 0x444 : race_kind == 0x10 ? 0x777 : 0x555;
    return 2;
}
if (car->fuel < 7) { msg_box(0x41 /*"You're gonna hafta get more gas"*/); return 0; }
save = ui_save_bg(0x13, &w, &h, 1);  ui_push(0x13);          /* "Drag race" / "Mulholland" / "Aqueduct" */
menu_timeout = ticks + 800;
do b = ui_wait(3000); while (!b);
switch (b) {
case -0x17: goto cancel;                                       /* no "Chicken!" (SR1 had one) */
case 1:  menu = 0x11; race_kind = 1;    break;
case 2:  menu = 0x12; race_kind = 0x10; break;
case 3:  menu = 0x12; race_kind = 8;    break;
case 1000: msg_box(0x43 /*"Chicken!"*/); goto cancel;
}                                                              /* other codes keep stale menu / kind */
if ((race_kind == 0x10 && car->fuel < 0x11) || (race_kind == 8 && car->fuel < 0xC)) { msg_box(0x41); goto cancel; }
ui_pop(1); DS_AD26 = 0; screen_put_bitmap(save…); ui_push(menu); page_copy_rect(…); DS_AD26 = 1;
int last = -1;
for (int tries = 0; tries < 2; tries++) {
    menu_timeout = ticks + 800;
    do b = ui_wait(3000); while (!b);
    bet_button = b;                                            /* AD66 */
    switch (b) {                                               /* a / k keep old values on unknown codes */
    case 1: a = 0;   k = 0; break;   case 2: a = 10;  k = 1; break;   case 3: a = 50; k = 2; break;
    case 7: a = 25;  k = 3; break;   case 8: a = 150; k = 4; break;   case 9: a = 0;  k = 5; break;
    case -0x17: case 1000: msg_box(0x43); goto cancel;
    }
    if (a > money) { msg_box(0x42 /*"You don't have enough dough for that bet!"*/); continue; }   /* (long)a vs money */
    if (challenge_accept(k)) {
        bet_kind = k; bet_amount = a;
        ui_pop(1); screen_put_bitmap(save…); save = 0;
        msg_box(W504C[(race_kind != 1)*3 + rand(3)]);
        DS_1FFC = 1; race_start(); DS_20EC++;
        game_clock += race_kind == 1 ? 0x444 : race_kind == 0x10 ? 0x777 : 0x555;
        return 2;
    }
    do r = rand(5); while (r == last); last = r;
    msg_box(W5038[(race_kind != 1)*5 + r]);
}
cancel: ui_pop(1); screen_put_bitmap(save…); ui_reset(); return 0;
```
Bet amounts: SR1 `$100` became **`$150`** (button 8).

### 0001:a270 bob_hotspots
Changed (SR1 `0000:a0f8`). `near void bob_hotspots(int opp_present)`: walk the hot-spot list `*DS:3B00` (18-byte
records `{u8 flags (1 on, 2 last), …, +06 id, +08 x0, +0A y0, +0C x1, +0E y1}`) and remember ids 1…4 (r1…r4).
`r4.y1 = 0x77`. If `!opp_present`: switch r2 and r3 off, return. Else, with `H = DS:9872 + 20*opp_model` (SR2_DATA
block 4) and the screen offset `(DS_AD5C, DS_AD5E)`: `r2 = {x0 = H[4]+AD5C, y0 = H[5]+AD5E, x1 = x0 + H[6] − 1, y1 =
r1.y0 − 1}`, on; `x = H[0xC]+AD5C, y = H[0xD]+AD5E; r3 = {x0 = (x > r2.x1) ? x : r2.x1 + 1, y0 = y, x1 = x + H[0xE] − 1,
y1 = y + H[0xF] − 1}`, on; `if (r4.y1 >= r2.y0) r4.y1 = r2.y0 − 1;` then `list_box(0x14)` (1a21:4d7c, attach the list).

### 0001:a3ba opponent_car_view
Changed (SR1 `0000:a265`). `ui_push(6); ui_reset();` temporary car with five local part-record pointers;
`car_new(opp_model, 0, &tmp)` (0001:42aa); `engine_bay(&tmp)` (0001:80a8); `save = screen_save_rect(0x18, 0, 0xC8,
0xBD, …)`; `bay_draw_parts(0, 0)`; `ui_mouse_mode(-2); status_print(-1,"",0); wait_ticks(800); DS_AD18 = DS_AD1A = 0;
screen_put_bitmap_mirror(save, 0x18, 0); ui_reset(); ui_pop(1);` (no click abort).

### 0001:a588 bob_screen_draw
Changed meaning (the matcher's SR1 `dancing_girl` is wrong; SR1 had this inline as `load_screen(0x445)`):
`pal_reg12_13(0x1DDA); DS_AD26 = 0; show_picture(0x431, -1); bob_sign_draw() /*0001:a4ea, game_flow: LIB pic 0x47D at
(0x2C,…) */;` on drivers −2 / −6 clear pages; on −2 `2f32:1956(1)` + show; `page_copy_rect(page AD14 → AD10, 0, 0,
0x13F, 0xBD)`; on −2 `2f32:1956(0)` else show; `DS_AD26 = 1; status_label(DS_4844)`. There is **no dancer** in SR2.

### 0001:a626 bob_drive_in
Changed (SR1 `0000:ab8b`, SR1 §4.14). Differences from the SR1 pseudocode:
* Entry: `status_print(-1,"",0); mouse pos (0x9E,0x80); bob_screen_draw(); DS_A2DE = -1; ui_mouse_mode(1);
  ui_mouse_mode(-2); carhop = -1; DS_9154 = 1; drive_dest = 2; fire = joy_buttons = 0; opp_present = 0; mouse_left =
  0;` — no dancer, no engine sound, no `drink`.
* Loop: `bob_hotspots(opp_present); ui_push(0x14);` timeout as SR1; `pct = mode==4 ? 0 : king_chance();
  summer_over_check(0); if (game_end_flags) return;` wait; then **`ui_pop(1)` right after the event**.
* Buttons: 1 back home (`status_print(-1,"",0); drive_dest = 1; drive_to_event(); return`), 2 challenge (as SR1;
  sets `DS_AD3C = 1` when an opponent is present), 3 "See it" (as SR1, "Chicken!" = msg 0x43, `game_clock += 0x111`),
  4 opponent_search (as SR1), 1000 timeout (as SR1, `opponent_show(2)` keeps him). No case 5 ("Order" is gone).
* After a race (`challenge` returned 2):
  ```c
  r = race_results(); DS_1FFC = 0; (*DS_A0CA)(); opp_palette_set(-1);
  if (race_kind != 1 && bet_button != 1) races_total++;                 /* 479C: street drags never count */
  if (vs_king) {
      if (!player_won) { king_stage = 0; DS_AD03 = 0; DS_47A0 -= 3; DS_47A2 -= 2; }
      else if (race_kind == 1)    king_stage = 1;
      else if (race_kind == 0x10) king_stage = 2;
      else { ending_king(); /*0001:a51c, game_flow*/ game_end_flags |= 2; return; }    /* Aqueduct won: the end */
  }
  if (r == 4) { jail(); ui_mouse_mode(-4); game_end_flags |= 4; return; }
  if (player_won) { if (race_kind == 1) DS_479E++; else if (race_kind == 0x10) DS_47A0++; else if (race_kind == 8) DS_47A2++;
                    if (DS_AA00 < 8) DS_AA00++;  if (opp_rec->win_rating > 1) opp_rec->win_rating--; }
  else            { if (DS_AA00 > 1) DS_AA00--;  if (opp_rec->win_rating < 8) opp_rec->win_rating++; }
  opp_rec->races++;                                                      /* also "just for kicks"; no wins counter */
  if (holeshot /*AA10*/) { if (DS_AA01 < 8) DS_AA01++; if (opp_rec->holeshot_rating > 1) opp_rec->holeshot_rating--; }
  else                   { if (DS_AA01 > 1) DS_AA01--; if (opp_rec->holeshot_rating < 8) opp_rec->holeshot_rating++; }
  if (r == 2 || r == 1) { drive_dest = (r == 2) ? 0 : 1; drive_to_event(); return; }   /* no skip_fx */
  bob_screen_draw(); mode = 4; carhop = -1; opp_present = 0; seen = 0;
  if (king_stage == 0) continue;                                        /* main loop */
  opponent_select(0x15, 4, 0);                                          /* the King stays for the next stage */
  /* and loop back to status_print(-1,"",0); challenge(vs_king) … */
  ```
  When `challenge` returns 0: `opponent_show(2); seen = 0;` and back to the main loop.

### 0001:a9fc jail
Changed (SR1 `0000:b08c`). `far int jail(void)`: `DS_AD26 = 0; pal_reg12_13(0x1DDA); ui_mouse_mode(-3);
race_view_reset();` pictures `ADAA = pic_get(0x481,1)`, `9750 = pic_get(0x483,1)`; `show_picture(0x431, -1)` (Bob's
background); bars `pic_get(0x482,2)` blitted twice (rows 0…4 at y 0 and the rest from y 5, bottom at 0xBE); the 7
decorations of table `DS:506A`; `status_print(-1,"",0); 1a21:2888(); DS_AD26 = 1; status_label(DS_4844);
opp_present = 0; carhop = -1; ui_mouse_mode(1); ui_mouse_mode(-1); drive_dest = 6;` loop: `ui_push(0x15)`, timeout
`ticks + (mode==4 ? 0x8C : 0x118)`, `pct = mode==4 ? 0 : king_chance()`, wait (3000, non-zero); `b == 1` →
`ui_pop(1); drive_dest = 1; return 0;` `b == 1000` → `if (rand(100) < 6) { ui_mouse_mode(-4); status_print(-1,"",0);
ui_mouse_mode(-2); }` then `opponent_select(pick_opponent(pct, 1), mode, 1); mode = 1;` any other code: nothing;
`ui_pop(0)` and loop. (SR1: 6 % showed the dancer.)

### 0001:ac68 tires_replace
Changed (SR1 `0000:bed4`). Returns the unpaid amount. `if (!car) return 0; car->value -= part_release(car->tyres)
/*0001:46f0*/; car->tyres = 0; parts_find(4 /*tyres*/, list, 0, 0, &n) /*0001:70e2*/; if (n > 0) { part_fit(
part_take(list[1]) /*71ce*/, &car->tyres) /*716a*/; return 0; } money -= 50 (32-bit); if (money < 0) { unpaid =
-money_lo; money = 0; } part_new(0x32 /*plain tyres*/, DS_46F6 /*50*/) /*0001:427c*/; p = DS_AA0A; DS_AA0A = p->next;
part_fit(p, &car->tyres); return unpaid;` (SR1 bought type 0x28.)

### 0001:ad36 text_draw_clipped
New. `far void text_draw_clipped(int x, int y, char *s, int maxw)`: if `text_width(s) > maxw`, cut the string one
character at a time from the end (the removed character is remembered, the cut position gets 0) until it fits; draw
with `(*DS_A0BA)(page DS:AD14, x, y, s)`; put the last removed character back.

### 0001:adc0 ticket_show
Changed (SR1 `0000:bf94`). `race_view_reset();` `T = pic_get(0x7D9,1)` (ticket), `F = pic_get(0x803,1)`; blit F
(full size) at (0, 0x45) and T at (F.w? — rect `{T.w, T.h, 0, 0, F.w, 0x14}`) into page AD14 (driver blits
`A0B6` / `A0AE`); font record `*(DS:9DC6)+0x41` (5 bytes) → `DS:AD28`; `text_draw_clipped(0x5E, 0x48, player name
DS:A9E8, T.w − 8)`; `text_draw_clipped(0x5E, 0x63, model[car->model].text, T.w − 8)`; page copy AD14→AD10;
`DS_AD26 = 1; ui_mouse_mode(-2); input_reset();`

### 0001:af0a crash_show
Changed (SR1 `0000:c1d8`). `DS_7B9C = 0 (byte); DS_7F2C = 0; split = (AD1C == -2 && DS_0200 == 0); (*DS_BBC8)()
/*crash sound*/; race_view_reset();` `A = pic_get(0x810,1)`, `B = pic_get(0x812,1)`, `C = pic_get(dash==0 ? 0x800 :
dash==1 ? 0x811 : —, 1)`; blit A rows 0…0x59 to y 0xB and A rows 0x59… to y `0x64 + split`; if `dash_type != 2`: C
(w 0x100) to (0x40, `0x5A + split`); masked B to (0, `0x77 + split`); `wheel_knob_crash(ADD0)`; show. If
`player.speed > 0x5A`: for i = 0…2: `P = pic_get(W5136[3i],1)`, `wait_vretraces(20 − 8i)`, blit at (`W5138[3i]`,
`W513A[3i]`), show. Then `ui_mouse_mode(-2); (*A0CE)(); input_reset(); wait_ticks(100); input_reset();
race_view_reset();`

### 0001:b1b2 finish_stats
New body (SR1 `0000:c407`; integer). `far void finish_stats(void)`:
```c
if (stats_done) return;
avg_speed = (int)(distance / (long)player.steps);                     /* _aFldiv */
race_time10 = player.steps * 10 / 6;                                  /* 16-bit, tenths of a second */
player_ahead = ((opp_seg - seg) * 8 + q(opp_seg_frac - seg_frac, 3) <= 0);
pct_covered = player_ahead ? 100
            : min(99, (int)((long)((seg - start) * 100) / (long)(opp_seg - start)));   /* imul → dx:ax */
stats_done = 1;
```
(SR1: `steps / 6.0688` as float, pct from doubles, 100 not set.)

### 0001:b256 race_msg
New body (SR1 `0000:c497`). `t = race_time10`:
`if (player_won) { sprintf(DS:A570, "Your``winning``time %d.%d sec.,`avg. speed %2d mph", t/10, t%10, avg_speed);
strcpy(DS:A5C6, "Congratulations !"); } else { sprintf(A570, "In %d.%d sec you covered only %2d%%", t/10, t%10,
pct_covered); sprintf(A5C6, "of the distance. Your avg. speed %2d mph.", avg_speed); } ui_push(0x17); ui_pop(1);`

### 0001:b46e engine_blown
Changed (SR1 `0000:c52c`). `if (car) { for slots +08…+10 (engine, manifold, 3 carbs): car->value -=
part_release(slot), slot = 0; bytes +16…+1B = 0; byte +22 = 0; } input_reset(); (*DS_BBAC)(); message_at(-msg, -1,
0x49);` (SR1 cleared `+16…+1D` and `+1F`.)

### 0001:b4da trans_blown
Changed (SR1 `0000:c5ac`). `+06` released and cleared, bytes `+14, +15` cleared; then as engine_blown.

### 0001:b538 patch_dollar_amount
New (SR1 did it inline). `far void patch_dollar_amount(char *text, int amount)`: `p = strchr(text, '$'); p[2] =
p[3] = ' '; sprintf(p+1, "%d", amount); p[1 + strlen(p+1)] = ' ';` (the NUL written by sprintf becomes a blank).

### 0001:b584 race_results
Changed (SR1 `0000:c613`, SR1 §4.13). Full SR2 flow (`bet = bet_kind`, `amount = bet_amount`, `eng = result & 4`,
`trn = result & 0x10`, `breakdown = result ∈ {5, 0x11, 0x15}`):
```c
clamp trans, engine, tyres wear to 0x26AC;
player_won = 0; stats_done = 0; distance = 0; shift_request = 0; fire = 0;
if (player.automatic) player.auto_mode = 1;
if (tires_blown) player.result |= 8;
int used = race_kind == 8 ? 10 : race_kind == 0x10 ? 15 : 5;            /* no menu_open(1) frame in SR2 */
car->fuel = used < car->fuel ? car->fuel - used : 0;
(*DS_A0CE)();
if (opp.result & 1) {                                                    /* ---- won ---- */
    music_resume_if_on();                                                /* 0001:2164 (sound) */
    player_won = 1;
    if (race_kind & 6) dragstrip_result(race_kind);                      /* 0001:b356 (game_flow): times screen, no msg */
    else { race_msg(); ui_mouse_mode(-2);
           msg_box(vs_king ? (race_kind == 8 ? 0x50 : 0x4F) : W512A[bet]); }
    money_add(amount);
    if (bet == 5) {
        car_t *n = car_new(opp_model, opp_model_ptr->price, 0);
        n->tyres->wear = rand(500);
        n->flags = (n->flags & ~0x2000) | ((opp_rec->custom & 4) ? 0x2000 : 0);   /* bit-merge as SR1, no sticker */
        n->flags = (n->flags & ~0x4000) | ((opp_rec->custom & 2) ? 0x4000 : 0);
        n->flags = (n->flags & ~0x8000) | ((opp_rec->custom & 1) ? 0x8000 : 0);
        n->paint = opp_rec->paint;
        if (car->klass < n->klass) { car->next = car2->next /*+28*/; car2 = car; car = n; car_setup(n, &player); }
        if (!demo) {
            if (!opp_rec->rerolled && !vs_king) {
                int list[22], c = 0;
                for (m = 0; m < 21; m++) if (CLASS(m) < opp_model_ptr->klass) list[++c] = m;
                if (c > 0) { list[0] = c; opponent_init(opp_index, list); opp_rec->rerolled = 1; goto done; }
            }
            opp_rec->status = -1;
        }
    }
done:
    if ((player.result & 8) && tires_replace()) return 2;
    return bet == 5 ? 1 : 3;
}
if (player.result & 0x40) {                                              /* ---- police ---- */
    (*DS_BBB8)(0); music_resume_if_on(); ticket_show();
    int fine = (player.result & 8) ? tires_replace() : 0;
    fine += player.speed > 0x50 ? 75 : player.speed > 0x32 ? 50 : 20;
    ui_push(6); ui_wait(0);
    sprintf(buf, "You have been fined $%2d for speeding", fine); status_print(-1, buf, 0x140 - DS_AD36);
    wait_ticks(0xFA);
    if ((long)fine > money) { msg_box(0x52); ui_pop(1); money_add(-money_lo); status_print(-1, " ", 0x140-AD36); return 4; }
    money_add(-fine);
    save = menu_timeout; wait_ticks(DS_1F4A); menu_timeout = save;
    status_print(-1, "", 0x140 - AD36); ui_pop(1); return 3;
}
if (player.result == 0x81) {                                             /* jumped the light */
    music_resume_if_on(); ui_mouse_mode(-2);
    msg_box((race_kind & 6) ? -0x51 /*"Ya jumped the light, speedy!"*/ : -0x48 /*"Didn't your momma teach ya not to cheat,"*/);
    return 3;
}
if (player.result == 0x101) {                                            /* NEW: lane violation */
    music_resume_if_on(); ui_mouse_mode(-2);
    msg_box((race_kind & 6) ? (DS_AD6C >= 2 ? 0x6C : 0x6D) /*"You crossed the center line. You lose."*/
                            : 0x78 /*"You just bumped the starter. Good thing…"*/);
    return 1;
}
if (bet == 5) { garage_remove_car(car) /*0001:4716*/; car = 0; }
money_add(-amount);
if (player.result == 1) {                                                /* lost */
    music_resume_if_on();
    if (race_kind == 4) dragstrip_result(race_kind);
    else { race_msg(); ui_mouse_mode(-2);
           int id = bet == 5 ? 0x53 : 0x54;
           if (vs_king) id = race_kind == 1 ? 0x55 : 0x56;
           msg_box(id); }
    return bet != 5 ? 3 : 2;
}
if (breakdown) {
    music_resume_if_on(); ui_mouse_mode(-2);
    if (eng) engine_blown(bet == 5 ? 0x5A : 0x58);
    else if (trn) trans_blown(bet == 5 ? 0x59 : 0x57);
    return 2;
}
crash_show(); music_resume_if_on();                                      /* ---- crash ---- */
if (bet == 5) { if (player.result & 8) { msg_box(0x5C); tires_replace(); } else msg_box(0x5B); return 2; }
if (player.speed > 0x5A || DS_BBF2 /*cliff fall*/) {                      /* totaled */
    int got = garage_remove_car(car) / 10 + 0x13; car = 0; money_add(got);
    patch_dollar_amount(MSG[0x5D].continuation /*W(DS:141C)*/, got); input_reset(); msg_box(0x5D); return 2;
}
int slow; int cost;
if (player.speed > 0x3C)      { slow = 0; cost = rand(0x28) + 100; }
else if (player.speed > 0x1E) { slow = 1; cost = rand(0x14) + 100; }
else { if (player.result & 8) { msg_box(0x5F); tires_replace(); return 2; } msg_box(0x5E); return 1; }
char *t = slow ? DS:22B4 /*"Damn. It'll cost $123 to get this baby running."*/ : DS:227C /*"$123 for parts …"*/;
patch_dollar_amount(t, cost);
int tires = player.result & 8;
if (tires) strcpy(DS:9AEA, "Plus tires !"); else DS_9AEA[0] = 0;
int menu = slow ? 0x19 : 0x18;
*(int *)(W(DS:3AD8 + 2*menu) + 0x12) = ((long)cost <= money) ? 0x11 : 0;   /* "Fix it" button state */
switch (ui_menu(menu)) {                                                  /* 1a21:4cf4 */
case 1: money_add(-cost); if (tires) tires_replace(); return 1;
case 2: money_add(car->value * 4 / 10); car = 0; return 2;               /* 16-bit value*4 (overflows ≥ 8192) */
default: return code - 2;                                                 /* quirk: returns the menu code minus 2 */
}
```
Message numbers are `msg_box` numbers (game_flow; text = `DS:11EA + 6n` record `{style, text, continuation}`;
a negative number shows without waiting).

### 0001:bc88 phys_reset
Changed (SR1 `0000:cdaf`, SR1 §4.5). `near void phys_reset(phys_t *p)`:
```c
shift_request = 0;                                   /* SR1's mouse save / cli block is gone */
p->wheel = p->steer_in = p->band = p->gear = p->speed = p->steps = p->result = 0;
if (p->automatic) { p->curve[2][2].rpm_lo = p->curve[2][1].rpm_hi = p->redline; p->auto_mode = 1; }
p->f26 = p->f28 = -1; p->rpm = 0x5DC; p->cur_ratio = p->f2A;     /* quirk kept */
p->rpm_up = p->curve[0][0].up; p->rpm_down = p->curve[0][0].down;
p->f12 = p->curve[0][0].rpm_hi; p->f10 = p->curve[0][0].rpm_lo;
DS_1F44 = DS_1F48 = DS_1F46 = 0;  gear_label_cache = -1;          /* B164 */
switch (race_kind) {                                 /* jump table, other values: unchanged */
case 0: case 1: start = 0xC8; track_end = 0x17C; break;
case 2: case 4: start = 8;    track_end = 0x58;  break;
case 8:         start = 8;    track_end = 0x156; break;
case 0x10:      start = 0x172; track_end = 0x3C0; break;
}
part_t *t = car->tyres;                              /* always the player's tyres */
p->tire_slip = W521E[G(t)*5 + t->wear/2000];
if (t->wear >= 8000 && rand(2000) + t->wear > 0x251C) {
    int h = (track_end - start) >> 1;                /* sar */
    p->blowout_seg = rand(h >> 1) + h + start;
} else p->blowout_seg = 0;
tires_blown = 0; ai_steps_done = 0;                  /* 9DC4, 523C */
opp_vmax = car_max_speed(opp_model);
opp_v70  = (int)((long)opp_vmax * 7000 / 10000);
opp_v80  = (int)((long)opp_vmax * 8000 / 10000);
opp_slack = (cpu_class >= 4) ? 16 : 10;
DS_523E = 0; stats_done = 0; DS_5240 = 0;
```

### 0001:be82 race_start
Changed (SR1 `0000:d031`, SR1 §4.9).
```c
ui_mouse_mode(-4); ui_mouse_mode(1); input_reset();
racing = 1;                                          /* set before the resets (SR1: after) */
phys_reset(&player); road_state_init(); opponent_setup();
if (race_kind == 0x10 && !vs_king) { police_state = (rand(4) == 0); if (police_state) police_seg = rand(0xF0) + 0x1E0; }
else police_state = 0;
race_state = 0;
drive_run(); race_isr_leave(); menu_timeout = 0; input_reset(); status_label(DS_4844);
if (seg <= start) return;                            /* never left the line */
if (rand(3) > 0) {                                   /* timing drift, car +23 */
    int t = car->timing;
    if (t >= 1)      { if (t < rand(3)) t = rand(3); }
    else if (t < 0)  { if (t > -rand(5)) t = -rand(5); }
    else             t = rand(6) - 3;                /* byte arithmetic */
    car->timing = t;
}
part_t *ty = car->tyres; int base;
if (G(ty) == 0) base = (race_kind & 7) == 0 ? 700 : 300;   /* road kinds 8 / 0x10 wear more */
else            base = (race_kind & 7) == 0 ? 400 : 200;
ty->wear += base + rand(300);  if (ty->wear > 10000) ty->wear = 10000;
```
(SR1: police only for the road race with `rand(0xFF)+0x1FE`, drift ranges 4 / 8 / 8−4, road ↔ `race_type`.)

### 0001:c02c car_perf
New (SR1's `perf()` was inlined in `car_setup` / `car_max_speed`). `near long car_perf(car_t *c)`; integer
"percent" ≈ SR1 `f·100`.
```c
int tr = G(c->trans), e = G(c->engine), man = G(c->manifold), dif = G(c->diff /*+20*/);
int carb = (W4E6E[man] > 0) ? G(c->carb[0] /*+0C*/) : 0;
int m = c->model, cls = DS_A810[20*m];                              /* class byte */
int roof  = (c->flags & 0x2000) || OVL(m).roof  == 0;               /* flags word +26 */
int front = (c->flags & 0x8000) || OVL(m).front == 0;
int rear  = (c->flags & 0x4000) || OVL(m).rear  == 0;
int t = (int8)c->timing;                                            /* +23 */
int s = 10 * (((abs(t) * 4) / -3 + 2) * 44 + W52EC[cls])           /* idiv by −3, truncation toward 0 */
      + 25 * (((roof*2 + front)*4 + rear)*4 + W52CA[e])             /* 32·roof + 16·front + 4·rear */
      + 15 * (W5294[carb*9 + man] + W52D0[tr]);                     /* 16-bit sum, then sign-extended */
long S = s;
if (dif == 0)      S += S*5 / 100;                                  /* 2.78:1 → +5 % */
else if (dif == 2) S += S*5 / -100;                                 /* 4.11:1 → −5 % */
S /= 100;
S *= 10000 - c->engine->wear;
S /= 10000;
return S * (tr < 1 ? 8 : 10) * 3 / 100 + 70;                        /* automatic ×0.8 */
```
Each `/` is a signed long division truncated toward zero, in exactly this order.

### 0001:c294 car_setup
Changed (SR1 `0000:dcbe`, SR1 §4.4; now integer). `near void car_setup(car_t *c, phys_t *p)`:
```c
int e = G(c->engine), tr = G(c->trans), ty = G(c->tyres);
int muf = c->muffler ? G(c->muffler) : -1;                         /* +1E may be empty */
int exh = G(c->exhaust /*+1C*/), dif = G(c->diff /*+20*/);
long f = car_perf(c);
long g = f * 13 / 10;  if (g > 130) g = 130;                        /* top speed, mph */
for (int gear = 0; gear <= 4; gear++) {
    long r = (long)W5242[tr*5 + gear] * g;  if (r == 0) r = 20;
    p->ratio[gear] = (int)(570000L / r);                            /* rpm per mph */
    crv_t *b = p->curve[gear];
    b[0].rpm_lo = 0;              b[0].rpm_hi = W5288[e*2];
    b[1].rpm_lo = W5288[e*2];     b[1].rpm_hi = W5288[e*2+1];
    b[2].rpm_lo = W5288[e*2+1];   b[2].rpm_hi = 0x1644;
    int v = (int)((long)W526A[e*5 + gear] * f / 100);
    int up0 = v / (gear ? 5 : 1);
    if (gear == 1) up0 *= (ty == 2) ? 3 : 2;                         /* launch: ×2, slicks ×3 */
    if (muf < 0)       up0 += up0 * 5 / 100;                         /* no muffler +5 % */
    else if (muf == 1) up0 += up0 * 3 / 100;                         /* glass pack +3 % */
    if (exh == 1)      up0 += up0 * 5 / 100;                         /* tube headers +5 % */
    if (dif == 0)      up0 += up0 * 5 / -100;                        /* 2.78:1 −5 % */
    else if (dif == 2) up0 += up0 * 5 / 100;                         /* 4.11:1 +5 % */
    b[0].up = up0;  b[1].up = v;  b[2].up = v / (gear ? 10 : 1);
    for (k = 0; k < 3; k++) b[k].down = gear ? v / (5 - gear) : v;
}
p->automatic = p->auto_mode = (tr < 1);
if (p->automatic) { p->f28 = p->f26 = -1; p->auto_shift_rpm = p->curve[4][1].rpm_hi; }
p->redline = W52D8[e];
p->grip    = (0x82 - cls) * 5 / 50 + W52E6[ty];                     /* cls = class byte of c->model */
p->ngears  = W52DE[tr];
p->gear_dirty = 1;
```
16-bit `int` divisions (`up0*5/100`, `/(5−gear)`) are C `idiv`; the exhaust/muffler/differential terms change only
band 0 (the launch band). Worked check (verified arithmetic): car 3 (1965 Mustang: Ford 302 V-8 e=1, 4-speed tr=2,
4-bbl carb on a 4-bbl manifold, brand tyres, stock exhaust / muffler, 3.73 differential), class 4, timing 0, wear 0,
no customisation: `s = 10·(88+73) + 25·50 + 15·(40+66) = 4450`, `S = 44`, `f = 44·10·3/100 + 70 = 83`, top speed
`83·13/10 = 107` mph, `ratio[1] = 570000/(25·107) = 213`, `ratio[4] = 570000/10700 = 53`.

### 0001:c5d8 model_car_setup
Changed (SR1 `0000:e0e2`). Builds the temporary stock car of model `m` from the SR2 car record (`w = model[m].spec_a`,
`make = (w & 0xE) >> 1`), then `car_setup(&tmp, p)`:
* engine type `((w >> 8) & 3)*3 + make`; transmission `((w >> 6) & 3 + 3)*3 + make + (((w>>6)&3) == 3)` — the SR1
  "+1 for racing" quirk is kept, so a Chrysler racing 4-speed (car 20, Dodge Charger) becomes catalogue 21 (a 2-bbl
  carburettor, grade 0) and the stock car is treated as an **automatic** with 3 gears;
* manifold `(((w >> 10) & 0xF) + 8)*3 + make`; carburettors `0x15 + (w >> 14)`, count `W4E6E[(w>>10)&0xF]`;
  tyres `0x32 + ((w >> 4) & 3)`; all temporary parts get wear `DS_4790` (−128);
* the temporary records are laid out from the engine slot: slot k (k = 0…count+1) at car `+08 + 2k`, types
  overwritten so that `+08` = engine, `+0A` = manifold, `+0C…` = carburettors; `+0E`, `+10` = 0 first; timing `+23`
  = 0.
* **Not initialised:** the flags word `+26` and the exhaust / muffler / differential pointers `+1C/+1E/+20` (stack
  garbage in the original, read by car_perf / car_setup). Port: see Open questions (use spec B stock parts, flags 0).

### 0001:c6f6 car_max_speed
Changed (SR1 `0000:e218`). `far int car_max_speed(int m)`: `c = (m < 0) ? car : stock car of m` (same builder as
model_car_setup, same uninitialised fields); `g = car_perf(c) * 13 / 10; return g > 130 ? 130 : g;` (long, then low
word).

### 0001:c866 drivetrain_rating
Changed meaning (SR1 `model_class3 0000:9524`). `near int drivetrain_rating(int m)`: `m < 0` →
`G(car->engine) + G(car->trans) + 1`; else `((spec_a >> 8) & 3) + ((spec_a & 0xC0) >> 6) + 1` of model m (1…7).

### 0001:ca28 opponent_setup
Changed (SR1 `0000:cf5c`, SR1 §4.9). `far void opponent_setup(void)`:
```c
model_car_setup(opp_model, &opp);
opp_skill = (int8)opp_rec->a;                        /* person +00 (SR1: record +01) */
phys_reset(&opp);
DS_A2A8 = 0; opp.gear = 0; opp.cur_ratio = opp.ratio[0];
opp.rpm = 0x5DC; opp.speed = 0x5DC / opp.cur_ratio;
/* band choice as SR1 → opp_band_tmp (9704), opp.band, opp.band_hi = crv->rpm_hi, opp.band_up = crv->up */
opp_shift_rpm = opp.auto_mode /*+24*/ ? 0x14B4 : (opp_skill - 10) * 200 + opp.redline;
opp_seg = start;                                     /* 9AE2 (SR1 200) */
opp_lat = opp_lat_target = ((road_left4 + road_right4) >> 1) - ((race_kind & 6) ? 200 : 0) - 200;   /* 985C, A558 */
opp_seg_frac = 0; opp_close = 0 /*B8C0*/; rear_ended = 0 /*9A48*/;
```

### 0001:cb00 opp_gear
= SR1 `opp_gear 0000:d1de` (SR1 §4.10) — same algorithm, now `far void opp_gear(int v, phys_t *p)` (args at
bp+6/bp+8). DS remap: 6CB8→9704.

### 0001:cb98 opp_ai
Changed (SR1 `opp_ai 0000:d29d`, SR1 §4.10). `near void opp_ai(void)`:
```c
if (race_kind == 2 && opp_seg >= 0x50) {             /* time-run reference car stops at seg 0x50 */
    if (!DS_B18E) DS_B18E = player.steps;
    opp.speed = 0; return;
}
int v0 = opp.speed, lim = 0;                         /* lim keeps its value across iterations */
cli(); int n = player.steps - ai_steps_done; ai_steps_done = player.steps; sti();
if ((race_kind & 0x10) && (TRACK_C[opp_seg] & 0x60) && opp.speed > 0x3B) {   /* Mulholland slow section */
    opp_gear(0x3B, &opp);
    goto tail;                                       /* no per-step simulation this frame */
}
for (int i = 0; i < n; i++) {
    if (!(race_kind & 0xE) && CURVE[opp_seg] != CURVE[opp_seg+1]) {          /* street drag / Mulholland */
        if (DS_8014 == 0) {
            DS_8014 = 1;
            lim = opp_vmax - (opp_seg > seg ? 0x1E : 3);                        /* SR1: clumsiness */
            if (lim < opp.speed) opp_gear(lim, &opp);
            continue;
        }
        /* already braked for this curve: falls through to the normal logic (SR1 did nothing) */
    } else DS_8014 = 0;
    if (!(race_kind & 6)) {                          /* no rubber band on the drag strip */
        int d = opp_seg - seg;
        if (d > 0xC) {
            lim = (player.speed >= 0x61 || vs_king) ? opp_v80 : opp_v70;
            if (lim < opp.speed) { opp_gear(lim, &opp); continue; }
        } else if (d > -5 && d < 5) {                /* SR1 also entered here for the King's road race */
            lim = player.speed + opp_slack;
            if (seg >= 0x1E && lim < opp_v70) lim = opp_v70;
            if (lim < opp.speed) { opp_gear(lim, &opp); continue; }
        }
        if (police_state >= 2 && player.speed > 0x32 && (lim = player.speed + 0x20) < opp.speed) { opp_gear(lim, &opp); continue; }
    }
    /* accelerate (SR1 code) with two changes */
    opp.rpm += opp.band_up;  if (opp.rpm > opp_shift_rpm) opp.rpm = opp_shift_rpm;
    if (opp.rpm > opp.band_hi) {
        if (opp.band < 2) opp.band++;                /* NEW cap */
        crv = &opp.curve[opp.gear][opp.band]; opp.band_hi = crv->rpm_hi; opp.band_up = crv->up;
    }
    if (opp.gear < opp.ngears && ((opp.auto_mode && opp.rpm >= opp.auto_shift_rpm) || opp.rpm >= opp_shift_rpm)) {
        opp.gear++;
        if (!opp.auto_mode) {
            if (opp.gear >= opp.ngears) opp_shift_rpm = (cpu_class >= 4 || video_driver == -6) ? 0x1518 : 0x14B4;
            else                        opp_shift_rpm = (opp_skill - 10) * 200 + opp.redline;   /* NEW formula */
        }
        /* cur_ratio, rpm = speed*ratio, band 0/1/2 choice → 9704, band, band_hi, band_up: as SR1 */
    }
    opp.speed = opp.rpm / opp.cur_ratio;
}
tail:
if (race_kind == 4) return;
if (opp_seg >= track_end) {
    int dec = (seg + 5 >= opp_seg) ? 10 : 5;
    opp.speed = (dec < v0) ? v0 - dec : 0;
}
```
`TRACK_C`/`CURVE` = far arrays of §1; the `& 0x60` test reads the word at `[871A]:3EEA + 2·opp_seg`.

### 0001:ce9c speed_to_step
Changed (SR1 `0000:d544`; now 64ths). `far int speed_to_step(int v)`: `v <= 0` → 0; `cpu_class < 4` → `v <= 0x47 ?
v + 1 : (v − 4) * 2`; else `DS:8016[min(v, 0x82)]` (byte table §2).

### 0001:cee0 player_advance
Changed (SR1 `0000:d624`). `near void player_advance(void)`:
```c
seg_step = speed_to_step(player.speed);
if (race_kind == 8) {                                /* Aqueduct obstacles (race_render 0dcb:a2d8) */
    if (DS_809A == 0) DS_809A = 1;
    else if (seg_frac + seg_step > 0x38) {
        int fl = TRACK_D[seg];                       /* word [871E]:4E66+2·seg */
        if (fl & 0x90) {
            int old = seg_frac; seg_frac = 0x38;
            if (aq_collide_probe(fl)) { seg_step = 0x38 - seg_frac; DS_809A = 0; }   /* stopped at the obstacle */
            else seg_frac = old;
        }
    }
}
seg_frac += seg_step;
if (seg_frac >= 0x40) road_advance_segment();        /* 0dcb:12ca: seg += frac >> 6, frac &= 0x3F */
if (seg >= track_end && stats_done && player.speed <= 0) {
    if (player_ahead) opp.result |= 1; else player.result |= 1;
    race_ctrl_reset();
}
```

### 0001:cf9a opp_dodge_street
New. `near int opp_dodge_street(int x /*opp_lat*/, int dx /*target − x*/, int s /*opp_seg*/)` — returns the lateral
step for street races (kinds 1 and 0x10). It scans the 7 segments `s+6 … s` (k = 6 down to 0) for blocked lateral
spans `[L, R]` and steers around them. (Two dead locals at the start are not used.)
```c
int found = 0, L, R;
for (int k = 6; k >= 0; k--) {
    int j = s + k;
    if (TRACK_C[j] & 0x80) {                         /* byte test at [871A]:3EEA+2j: special section ahead */
        int d = abs((s - seg) * 8 + opp_seg_frac + seg_frac);
        if (d < 0x40 && opp.speed > 0x32 && opp.speed > player.speed) opp_gear(max(player.speed, 0x32), &opp);
        int mid = (road_left4 + road_right4) >> 1;   /* 985C, A558 */
        return (mid - x) / 2;                        /* C division (toward 0) */
    }
    if ((TRACK_C[j] & 0x1004) && opp.speed > 0x32) {           /* word test: car in a lane */
        int xo = track_lane_x(j);                              /* 0dcb:1470 = LANE[j]*0x70 + 0x5DC (race_render) */
        if (!found)                         { L = xo; R = xo + 0x70; }
        else if (L > xo && L - xo < 0xE1)    L = xo;
        else if (R < xo && xo - R + 0x70 < 0xE1) R = xo + 0x70;
        found = 1; continue;
    }
    if (SIDE_L[j] & 0x1010) {                        /* [8720]:27B6+2j: obstacle on the left (x 0x5DC) */
        if (!found || R + 0xE1 < 0x79E) { L = 0x5DC; R = 0x5DC + 0x1C2; }
        else if (L - 0xE1 < 0x5DC + 0x1C2) L = 0x5DC;
        found = 1; continue;
    }
    if (SIDE_R[j] & 0x1010) {                        /* [8722]:2F72+2j: obstacle on the right (x 0xB22) */
        if (!found || L > 0xB22) { L = 0xB22; R = 0xB22 + 0x1C2; }
        else if (0xB22 - 0xE1 > R) R = 0xB22 + 0x1C2;
        found = 1; continue;
    }
    if (j - seg != 1) continue;                      /* only the segment just ahead of the player */
    if (k == 0 && found) break;
    if (k < 2 && abs(opp_lat - lat) < 0x118) {        /* right behind the player */
        if (found && opp.speed > 0x28) opp_gear(max(opp.speed - 0x1E, 10), &opp);
        return 0;
    }
    int lo = lat - 0x136, hi = lat + 0x15E;            /* the player as an obstacle */
    if (!found) { L = lo; R = hi; }
    else {
        if (lo < L && R < hi) { L = lo; R = hi; }
        else {
            if (L - 0xE1 < hi && R > hi) L = lo;
            if (R < lo && R + 0xE1 > lo) R = hi;
        }
    }
    found = 1;
}
if (!found) return q(dx, 3);                          /* dx/8 toward zero */
int a = x - 0x82, c = x + 0xAA, step = 0;
if (c >= L && a <= R) {                               /* the opponent's span overlaps the blocked span */
    int toL = c - L + 0x64, toR = R - a + 0x64;
    if (L - 0x12C < road_left4)            step = toR;
    else if (R + 0x12C > road_right4)      step = -toL;
    else if (toR < toL)                     step = (x + toR >= road_right4) ? -toL : toR;
    else                                    step = (x - toL > road_left4) ? -toL : toR;
}
return q(step, 3);
```
Note the "k == 0 && found → break" test happens before the player-obstacle code, and only when `j == seg+1`.

### 0001:d4cc opp_advance_steer
Changed (SR1 `0000:d67d`, SR1 §4.11). `near void opp_advance_steer(void)` with `mid = (road_left4 + road_right4)
>> 1` (985C / A558), `side` = 8012, `recentre` = B19C:
```c
if (opp_seg - seg == -1 && abs(opp_lat - lat) < 0xB4) opp_seg_step = 0;      /* NEW: blocked right behind you */
else opp_seg_step = speed_to_step(opp.speed);
if (!DS_7FCE) {                                      /* not bouncing off an Aqueduct wall */
    opp_seg_frac += opp_seg_step;
    if (opp_seg_frac >= 0x40 && opp_seg_step > 0) { opp_seg += opp_seg_frac >> 6; opp_seg_frac &= 0x3F; }
}
if (opp_seg >= track_end && stats_done && player.speed <= 0) {
    if (player_ahead) opp.result |= 1; else player.result |= 1; race_ctrl_reset(); return;
}
int fl, c;
if (race_kind == 8) fl = c = 0;
else { fl = ((TRACK_C[opp_seg+4] | TRACK_C[opp_seg]) & 0x80) != 0;    /* byte tests; SR1: TRACK_C[opp_seg+4] & 0xE0 */
       c = CURVE[seg+4] - CURVE[seg+5]; }
int d = opp_seg - seg, dl = opp_lat - lat;
if (race_kind & 4) goto move;                        /* bracket rounds: keep the lane */
if (d > 0xC) { target = mid; if (c) side = (c < 0); recentre = 1; goto move; }
if (d > 6) {                                         /* 7…12 ahead: "near" */
    DS_A7D6 = 1;
    if (c) goto curve_side;
    target = fl ? mid : (side ? mid - 200 : mid + 300); goto move;
}
if (d > (cpu_class >= 4 ? 2 : 4)) {                  /* 3…6 ahead */
    if (c) goto curve_side;
    if (fl) { target = mid; goto move; }
    if (!recentre) goto move;
    target = side ? mid + 300 : mid - 200; recentre = 0; side = !side; goto move;
}
if (d > -4) {                                        /* level with the player */
    if (opp_seg_step > seg_step) {
        if (opp_pose >= 9) {                         /* side view: overtaking — REWRITTEN */
            if ((dl > -0xB4 && dl < 0xB4) || fl) {   /* no room: drop back */
                opp_seg = seg - 2; opp_gear(max(player.speed, 0x32), &opp);
            }
            if (lat - road_left < road_right - lat) { side = 0; target = lat + 0x212; }   /* road_left A7DA, road_right B8D2 */
            else                                    { side = 1; target = lat - 0x14A; }
            goto move;
        }
        if (opp_pose != 0 && opp_pose != 2) goto move;
        target = (opp_lat >= lat) ? lat + 0x212 : lat - 0x14A; goto move;
    }
    if (opp_pose != 0) goto move;
    target = lat - 0x14A; goto move;
}
if (d > -6) {                                        /* −4, −5 — NEW offsets */
    DS_A7D6 = 1;
    if (c < 0) { target = mid - 100; side = 1; goto move; }
    if (c > 0) { target = mid + 400; side = 0; goto move; }
    target = fl ? mid + 300 : (side ? mid : mid + 500); goto move;
}
target = mid + 200; recentre = 1; goto move;         /* ≤ −6 behind (SR1: mid) */
curve_side:
    if (c < 0) { target = mid - 200; side = 1; } else { target = mid + 300; side = 0; }
move:
if (race_kind & 0x11) opp_lat += opp_dodge_street(opp_lat, target - opp_lat, opp_seg);
else if (race_kind == 8) opp_lat += aq_lane_target(opp_lat, target - opp_lat, opp_seg, lat, seg);  /* 0001:d2a8, race_render */
else opp_lat += (target - opp_lat) >> 3;             /* drag strip: arithmetic shift, no overshoot test */
DS_A33E = lat;
```
`target` = `opp_lat_target` (9702). SR1's no-overshoot "move" step is gone.

### 0001:d85c police_spawn
Changed (SR1 `0000:d992`, SR1 §4.12). `int v = opp.speed; opp_rec = &person[10] /*DS:AC32*/; model_car_setup(0x15,
&opp); opp_skill = (int8)opp_rec->a; opp_gear(v, &opp); opp_palette_set(5); memcpy(DS:B7B6, DS:9A94, 0x30) /*police
sprite pointers (race_render)*/; opp_vmax = car_max_speed(0x15); opp_v70 = (long)opp_vmax*6000/10000; opp_v80 =
(long)opp_vmax*8000/10000;` (SR1: model 0x19, float ×0.6/×0.8.)

### 0001:d908 race_step
Changed (SR1 `0000:da25`, SR1 §4.8). `far void race_step(void)`:
```c
int start_lane = (DS_A7AC && (start_panel == 0x813 || start_panel == 0x816));   /* starter on the road */
/* gear label: as SR1 (auto_mode → label auto_mode+4, cache B164) */
if (player.blowout_seg && player.blowout_seg < seg) tires_blown = 1;
/* squeal: as SR1 (on at |wheel| >= 0xC, off below 10) */
if (race_kind != 4 || DS_A2A8 > 0) opp_ai();         /* bracket: opponent waits for its green */
switch (police_state) {                              /* AB2C */
case 1: if (police_seg <= seg) { if (seg < opp_seg + 7) police_state = 0;           /* SR1: +5 */
                                 else { police_state = 2; police_spawn(); } } break;
case 2: if (seg <= opp_seg + 0xF) { police_state = 3; siren(1); } break;           /* (*DS_BBB8)(1) */
case 3: if (seg < opp_seg - 2) { player.result |= 0x41; race_ctrl_reset(); return; } break;
}
int p = seg * 8 + q(seg_frac, 3), o = opp_seg * 8 + q(opp_seg_frac, 3);           /* eighths */
if (!DS_5240 && (p >= 8 || o >= 8)) { DS_5240 = 1; holeshot = (o < p); }          /* AA10 */
player_advance();
opp_advance_steer();
if (race_kind & 6) { if (lat < (0xCE4 + 0x5DC) / 2 + 0x96) goto lane; }            /* 2550: crossed the centre line */
if (start_lane) {
    int a = race_kind == 8 ? 0xCF8 : 0xCE4, b = race_kind == 8 ? 0x5C8 : 0x5DC;
    if ((a + b) / 2 + 0xA0 > lat) goto lane;                                        /* bumped the starter */
}
if (track_end <= seg || track_end <= opp_seg) finish_stats();
if (player.result || opp.result) {
    if (police_state >= 2) { if (player.result) player.result |= 0x41; siren(0); }
    race_ctrl_reset(); return;
}
if (demo || !opp_close /*B8C0*/ || seg >= track_end) return;
if (rear_ended /*9A48*/ && police_state <= 1) { player.result |= 3; race_ctrl_reset(); return; }   /* SR1: = 3 */
if (opp_pose == 0 && seg_step > 1) {
    DS_809C++;
    if (player.steer_in < 0) {
        if (CURVE[seg] < CURVE[seg+1] && mid4 < lat) return;
        screech();
        if (!DS_523E) { opp_seg--; opp_gear(max(opp.speed - 10, 10), &opp); DS_523E = 1; }   /* NEW slow-down */
        else DS_523E = 0;
        return;
    }
    if (!DS_A7D6 || start_lane || (race_kind & 6) || DS_809C <= 3 || police_state >= 2) return;
    if (rand(((15 - opp_rec->b) >> 2) + 2) == 0) {   /* SR1: clumsiness >> 2 */
        screech(); screech(); lat += 300; if (player.speed > 0x14) ai_brake = 2; DS_A7D6 = 0;
        heading += rand(2) == 0 ? -0xF : 0xF; screech();
    }
    DS_809C = 0; return;
}
if (opp_pose != 0) DS_809C = 0;
return;
lane: player.result |= 0x101; race_ctrl_reset(); return;
```
`screech` = `(*DS_BBCC)()`, `siren(x)` = `(*DS_BBB8)(x)` (sound vectors). With the 1/64 positions the holeshot test
is decided on the first call (both cars start at `start ≥ 8`), see Open questions.

(`0dcb:1470 track_lane_x`, `0dcb:504a/507a/50c4/50f4 town_walls_*` and `0001:d2a8 aq_lane_target` are specified in
race_render.md, which claimed them.)

### 1a21:0008 dash_restore_rect / 1a21:00b2 dash_copy_rect
Changed (SR1 `0f38:000a` / `0f38:0098`, SR1 §4.16): same rectangles (x rounded down to 8, width + 0x10, kind 1/2/3 →
row offset 5 / 0xC / 0x23) with these differences: VGA row base **0x66** (SR1 0x64), pages: source = `DS_78C6 ?
page 9AD4 : page 9BC2`, destination = VGA ? `9BC2` : `ADA2`; blit vector `A0AE`.

### 1a21:015c speedo_points
Changed (SR1 `0f38:0137`; now near, three dash types). `near void speedo_points(int v, int *x0, int *y0, int *x1,
int *y1)`: `v = clamp(v, 0, 0x82)`; centre `(cx, cy) = (DS_8806, DS_8808)`:
* `dash_type == 0` (bar gauge): `*y0 = cy − 8; *y1 = cy; *x0 = (v − 0x3C)*6/10 + cx; *x1 = (v − 0x3C)*2/10 + cx;`
* otherwise: `a = (dash_type == 2) ? (0x69 − v)*2 : (0x46 − v)*0x5A/0x23 + 0x5A` degrees;
  `*x0 = cx + q(sin256(a+90)*Rin, 8); *y0 = cy − q(sin256(a)*Rin, 8); *x1 = cx + q(sin256(a+90)*Rtip, 8); *y1 = cy −
  q(sin256(a)*Rtip, 8)` with `Rin = W1C34[t]`, `Rtip = W1C2E[t]`.

### 1a21:02ac speedo_init
Changed (SR1 `0f38:01d2`). `cx = W1C22[t]; cy = W1C28[t] + DS_B8C4;` colour `(AD1C == −2 || −6) ? 0xB : 3`; points of
`v`; `dash_copy_rect(min x, min y, |dx|+1, |dy|+1, 2)`; line through the driver vector `A0DE` with the video segment
(A000 / B000 for −4 / B800) and `DS_9BC0 = 0`, both restored afterwards.

### 1a21:038e tach_points
Changed (SR1 `0f38:02ae`). `near`: `v = clamp(rpm, 0, 0x1644); a = (3000 − v)*9/200 + 90;` points as speedo with
centre `(DS_880E, DS_8810)`, `Rin = W1C1C[t]`, `Rtip = W1C16[t]`.

### 1a21:0484 tach_init
Changed (SR1 `0f38:0349`). `DS_880E = W1C0A[t]; DS_8810 = W1C10[t] + DS_B8C4;` colour `AD1C == −2 ? 0xB : 3` (no −6
test); then as speedo_init with kind 3.

### 1a21:055e needle_update
Changed (SR1 `0f38:0425`). Colour `DS_1C3A` initialised on first use (`−2/−6 → 0xB`, else 3); tach: tach_points(old)
→ `dash_restore_rect(…, 3)`, tach_points(new) → `dash_copy_rect(…, 3)`; speedo: `DS_9E92 = new`, the same with
speedo_points and kind 2; draw; return new.

### 1a21:070e clock_draw
Changed (SR1 `0f38:05f2`; integer, function ends at `1a21:0da5`). Centre `(880E, 8810)` from the tach tables;
colour as needle_update; `clock_hm(&mm, &hh)` (0001:6426). Hour: `h = ((hh >> 1) + 0x14) % 12 + 1; a = (3 − h)*30 −
(mm >> 1)`; δ = 8 when `|a mod 90| < 10 or > 80` (`|a + (a / −90)*90|`), else 5; minute: `a = (15 − mm)*6`, δ 8 / 4.
Points `P(L, a) = (cx + q(sin256(a+90)*L, 8), cy − q(sin256(a)*L, 8))`; hour: base `B = P(W1C3C[t], a)`, tip `T =
P(q(3·W1C42[t], 2), a)`, sides `S± = P(q(3·LT, 2), a ± δ)` (LT = the tip length); minute: tip length `W1C42[t]`
(sides likely the same 3/4 rule). Lines B–T, B–S+, B–S−, T–S+, T–S− as SR1.

### 1a21:1174 wheel_load
Changed (SR1 `0f38:0f53`). For i = 0…2: `pic_get(W78CE[3*dash_type + i], 1)` blitted with rect `{0xB8, 0x1E, 0, 0, 0,
W1CB4[i]}` into page ADA2, then `heap_pop(1)` (1a21:9b40). (SR1: three LIB2 pictures 184×26 kept in memory.)

### 1a21:11f6 wheel_knob_crash
Changed (SR1 `0f38:1094`). Video segment `A200` on VGA; `clip_set_ymin0`; temporarily `7E16 = 0xBE, 7E14 = 0x64`;
`shape_draw_dot(K[w].x, K[w].y + 0x64)` with K at `DS:1C80 + 2w`; restore; `clip_set_view`.

### 1a21:1288 wheel_draw
Changed (SR1 `0f38:1130`). Index `(w != 0) + (w > 0)` as SR1; the picture comes from the spare page row `W1CB4[i]`
(cached index `DS:1CB2`), y offset `DS_B8C4` (non-CGA); knob from `DS:1C80` table, `+B8C4`.

### 1a21:13fe shifter_draw
Changed (SR1 `0f38:12f7`). Records `DS:1CD0` (§2, LIB3 ids 2026…2031), current record pointer `DS:8816`; gate 48×43
at (0x108, `B8C4`-relative); label positions from the near list; `DS:8818 = −1` at the end.

### 1a21:1614 shifter_knob
= SR1 `0f38:14f2` logic with the record pointer `DS:8816`, cache `DS:8818`, pages `9BC2` / `ADA2`.

### 1a21:77f4 race_run
Changed (SR1 `0f38:7b22`, SR1 §4.2). Cockpit set-up is race_render's/dashboard's (dash type `AB28`, wheel_load,
shifter, gauges, clock when `has_clock`), the outer redraw loop repeats while `DS_7904 > 1`. Before the frame loop:
VGA split at 99 (`2f32:0010(0x63)`, `DS_AD30 = 0x64`); if `race_kind & 6`: `lights = (race_kind == 2) ? 0x88 : 0x92`;
`road_first_frame 0dcb:3516; road_frame 0dcb:11fc`; load time: `race_kind == 8 ? 0dcb:8840 : road_load_graphics();
shp_load_set()`. For `race_kind == 4`: copy the staged view, `ui_mouse_mode(-2); msg_box(0x70 "Drivers stage your
vehicles!"); ui_mouse_mode(-4); lights = 9;`. Frame loop:
```c
for (;;) {
    if (!DS_3B4E && !DS_3B50) { DS_3B4E = DS_3B32; DS_3B50 = DS_3B34; }     /* save mouse position */
    DS_3B38 = DS_3B39 = 0; DS_3B32 = DS_3B34 = 0;
    road_step();                                                        /* 0dcb:1774 */
    if (demo && key_read() == 'Q') { race_isr_leave(); platform_exit(); }
    int r = race_logic();
    if (r) break;
    int w = player.wheel;
    if (player.gear_dirty) shifter_knob(page 9BC2, &player);
    if (abs(player.speed - shown_speed) > 1) shown_speed = needle_update(player.speed, shown_speed, 0);
    if (!has_clock && abs(player.rpm - shown_rpm) > 1) shown_rpm = needle_update(player.rpm, shown_rpm, 1);
    if (w != wheel_shown) { wheel_draw(w, wheel_shown, page 9BC2); wheel_shown = w; }
}
race_view_reset(); DS_79E0 = 0;  /* then copy the last frame / split off / page mode as SR1 (VGA rows 0x64/0x65+DS_0200) */
```

### 1a21:9b0e race_view_reset
New. `far void race_view_reset(void)`: `DS_A31E = DS_BC4C = DS_B7EC = 0; DS_A7D8 = 0x3C; DS_B864 = DS_A0FE;`
(view / page state used by race_render and the UI; called before every full-screen picture).

### 2925:000e grudge_seed_field

> **Secondary view (S3.7):** owned by `game_flow.md`; on any conflict the owner's section wins.
New. `near void grudge_seed_field(void)`: picks the 9 people whose car top speed is closest to the player's.
```c
int pv = car_max_speed(-1), cnt = 0, worst_d = 0, worst, tmp[21];
entry *out = DS:90CA;                                  /* entries {person, state, dialin}, 6 bytes */
for (i = 0; i < 21; i++) {
    if (person[i].status == -1) continue;
    tmp[i] = car_max_speed(person[i].model);
    int d = abs(tmp[i] - pv);
    if (out < DS:9100) { out->person = i; out++; cnt++; if (worst_d < d) { worst_d = d; worst = cnt - 1; } }
    else if (d < worst_d) {
        entry[worst].person = i; worst_d = 0;
        for (j = 0; j < cnt; j++) { int dd = abs(tmp[entry[j].person] - pv); if (dd > worst_d) { worst_d = dd; worst = j; } }
    }
}
```

### 2925:00fa grudge_dialins

> **Secondary view (S3.7):** owned by `game_flow.md`; on any conflict the owner's section wins.
New. `entry[9] = {−1 (player), 0, ADA8 = B8C2}` (the player's time-run result, tenths); for entries 0…8:
`dial = (long)(MODEL[person.model].qtime * 10 / 6 * B18E) / (long)(u8)DS_A9C3;  state = 0;` (`qtime` = byte `+13` of
the car record, `B18E` = steps of the reference car in the time run; the first product is a 16×16→32 imul); then a
selection sort of the 10 entries by ascending dial-in (start minimum 0x7D00, swap 6-byte entries).

### 2925:01fa grudge_pair

> **Secondary view (S3.7):** owned by `game_flow.md`; on any conflict the owner's section wins.
New. Chooses the round's opponent (`DS:90C8` = entry index, loaded with `opponent_load(entry.person)`) and plays the
other pairs; at the end `DS_9ADC = entry[90C8].dial`.
* Round 0 (`AD6C == 0`): `9106` = index of the first entry with person < 0 (the player); opponent = `9106 ^ 1`
  (odd → −1, even → +1). Then exactly one other pair is decided: `k = 9; if (k == 9106 || k == 9106 + 1) k -= 2;
  if (rand(100) >= 60) entry[k−1].state = −1; else entry[k].state = −1;`
* Later rounds: if the player won the last round: `entry[90C8].state = (AD6C == 3) ? 2 : −1`; else
  `entry[9106].state = 2`. Then
  * `AD6C == 3` (final): find the first two entries with state 0 (a < b): if one of them is the player, the other is
    the opponent; else `rand(100) < 60 ? entry[b].state++ : entry[a].state++` and repeat the search with state 2
    (first two entries with state 2), same rule.
  * otherwise: for i = 0…9: when `entry[i].state == 0`, a = i, b = next with state 0: if a or b is the player the
    other is the opponent; else the loser `rand(100) < 60 ? b : a` gets `state = (AD6C == 2) ? 2 : −1`; continue after b.

### 2925:193e event_drive_home

> **Secondary view (S3.7):** owned by `game_flow.md`; on any conflict the owner's section wins.
New. `near void event_drive_home(int r)`: `(*A0CA)(); opp_palette_set(-1); drive_dest = (r == 2) ? 0 : 1;
drive_to_event(); race_kind = 0;`

### 2925:1970 event17_run (Grudge Night)

> **Secondary view (S3.7):** owned by `game_flow.md`; on any conflict the owner's section wins.
New. `far void event17_run(void)`:
```c
race_kind = 2; bet_kind = bet_amount = 0; DS_AD6C = -1;
grudge_seed_field();
opponent_load(0x15);                                  /* the reference car = the King's model */
vs_king = 0; DS_B18E = 0;
grudge_intro();                                       /* 2925:1642, game_flow UI */
DS_1FFC = 1; race_start(); int r = race_results(); DS_1FFC = 0;
if (!player_won) goto out;
grudge_dialins(); DS_AD6C = 0;
for (;;) {
    grudge_pair(); grudge_round_show();               /* 2925:16c8, game_flow */
    race_kind = 4; DS_1FFC = 1; race_start(); DS_1FFC = 0; r = race_results();
    if (!DS_B18E) DS_B18E = (u8)DS_A9C3;
    if (holeshot) { if (AA01 < 8) AA01++; if (opp_rec->holeshot_rating > 1) opp_rec->holeshot_rating--; }
    else          { if (AA01 > 1) AA01--; if (opp_rec->holeshot_rating < 8) opp_rec->holeshot_rating++; }
    if (player_won) { if (AA00 < 8) AA00++; if (opp_rec->win_rating > 1) opp_rec->win_rating--; }
    else {
        if (AA00 > 1) AA00--; if (opp_rec->win_rating < 8) opp_rec->win_rating++;
        if (DS_AD6C < 2) goto out;                    /* lost in round 0 or 1 */
        if (r == 2) goto standings;
    }
    if (++DS_AD6C >= 4) break;
}
standings:
grudge_standings();                                   /* 2925:1786, game_flow: payout, FINAL STANDINGS */
if (entry[9106].state == 0) DS_47A4++;                /* W(DS:90CC + 6·9106): the player won the event */
out:
event_drive_home(r);
```

### 2925:1ab8 gas_hotspots_clear … 2925:1c06 gas_hotspots_none

> **Secondary view (S3.7):** owned by `game_flow.md`; on any conflict the owner's section wins.
Changed set (SR1 `gas_hotspots_*` `0000:b3ca…b617`, "likely" there). Hot-spot list `*DS:3B04` (18-byte records as at
Bob's):
* `1ab8 gas_hotspots_clear`: clear bit 0 of every record up to the one with bit 1 (last); `DS_9158 = DS_9156 = 0`.
* `1ad2 gas_hotspot_enable(id)`: find id, set bit 0.
* `1af0 gas_hotspot_set(id, x, y, w, h)`: find id, `x0 = x, x1 = x + w − 1, y0 = y, y1 = y + h − 1`, enable.
* `1b28 gas_hotspots_pumps` (pumps on the island): clear; enable 2…8; `list_box(0x16)`; `DS_9158 = 1`.
* `1b84 gas_hotspots_b` (= SR1 match kept; nozzle in the car): clear; `gas_hotspot_set(1, AD62 − 10, AD64 − 5,
  0x12, 0xC)`; `list_box(0x16)`; `DS_9156 = 1`.
* `1bba gas_hotspots_handle(pump)`: clear; enable `pump == 1 ? 3 : 4`; `gas_hotspot_set(1, AD62 − 1, AD64 − 1, 0xA,
  9)`; `list_box(0x16)`; `9158 = 9156 = 1`.
* `1c06 gas_hotspots_none`: clear; `0001:1c5c()`.

### 2925:1cae gas_pump_draw

> **Secondary view (S3.7):** owned by `game_flow.md`; on any conflict the owner's section wins.
New. `far void gas_pump_draw(page far*, int i)`: blit from the pump sheet `B89A` rect `{W50F8[i], 0x3A, W5104[i], 0,
W50F0[i], 0x7B}` (vector `A0B6`, mode 1).

### 2925:1d00 gas_nozzle_draw

> **Secondary view (S3.7):** owned by `game_flow.md`; on any conflict the owner's section wins.
New. `near void gas_nozzle_draw(int *carpos, int pic)`: car picture pointer = `carpos[0x14/2] == 1 ? carpos+8 :
carpos+4` (far); `ui_mouse_mode(-3); P = pic_get(pic, 2); ui_mouse_mode(-1);` allocate a 2-plane bitmap of the
car-picture size, copy the car picture into it, masked-blit P at `((AD62 & ~AD24) − P.w + 0x18, 0xC − P.h)`, put the
result mirrored at `(AD62 − 0x10, AD64 − 5)`, free (`heap_pop(2)`). Pictures 0x456 (nozzle in the car) / 0x457 (none).

### 2925:1e08 gas_meter_draw

> **Secondary view (S3.7):** owned by `game_flow.md`; on any conflict the owner's section wins.
New. `far void gas_meter_draw(int pump)`: x offset `pump == 1 ? 0 : 0x9B`; six lamps k = 0…5 with masks 1, 4, 16,
2, 8, 32 (`1 << 2k` for k < 3, `(1 << 2k) >> 5` after); a lamp whose bit differs between `DS:9160` (counter) and
`DS:915E` (shown) is filled (`A0FA`) with colour `9160 & mask ? DS_AD44 : DS_AD46` at `{W5110[k] + xoff, W511C[k]}`;
`915E` takes the new bits; then two `page_copy_rect` strips (x `xoff&~7 + 0x2C … 0x86` and `+0x33 … 0x8B`). `DS:7E16` is
set to 0xC8 during the drawing and restored.

### 2925:1f38 gas_leave

> **Secondary view (S3.7):** owned by `game_flow.md`; on any conflict the owner's section wins.
New (replaces SR1 `gas_leave 0000:b717`). `near void gas_leave(state *g)` (`g` = event15's locals: `+0C` handle
state, `+12` nozzle in the car, `+14` pump, `+18` "driving off" flag):
```c
if (g->handle != 1) ui_mouse_mode(1);
if (g->nozzle_in_car) { gas_nozzle_draw(g, 0x457); if (g->drive_off) { money_add(money >= 10 ? -10 : -money_lo); g->drive_off = 0; } }
if (g->pump) { ui_mouse_mode(-3); gas_pump_draw(page AD14, g->pump); ui_mouse_mode(-1); page_copy_rect(pump strip);
               if (g->drive_off) money_add(money >= 10 ? -10 : -money_lo); }
car->fuel = min(car->fuel, 0xAA);                     /* 17.0 gal */
DS_9160 = 0; DS_915E = -1; gas_meter_draw(1); DS_9160 = 0; DS_915E = -1; gas_meter_draw(2);
car_draw(2, g->car_value, g->car_flags, 0, 0xB9, 0x4D30 + 8*G(car->tyres));
ui_pop(1); race_view_reset(); drive_dest = 1; drive_to_event();
```

### 2925:20be gas_pump_ctl

> **Secondary view (S3.7):** owned by `game_flow.md`; on any conflict the owner's section wins.
New. `far void gas_pump_ctl(int pump, int op)`: op 1 → `(*DS_BB9C)(1); DS_1F7C = 4;` (pump sound on); op 2 →
`DS_9160++; gas_meter_draw(pump);`; op 3 → `DS_1F7C = 0; (*DS_BB9C)(0);`.

### 2925:2104 event15_run (gas station)

> **Secondary view (S3.7):** owned by `game_flow.md`; on any conflict the owner's section wins.
New (replaces SR1 `gas_station 0000:b8a1`). Set-up: `status_print(-1,"",0); drive_dest = 3; race_view_reset();
DS_AD26 = 0; pal_reg12_13(0x1DDA); DS_1DB2 = 4; show_picture(0x430, -1); DS_915A = pic_get(0x47E, 0);` for pumps
p = 0, 1: two save bitmaps (`DS:9A4A[p]`, `DS:9ACC[p]`) of `W50FA[p] × 0x3A` at `(W50F2[p], 0x7B)`; `B89A =
pic_get(0x3E8, 0)`; draw pump parts 0…3; `status_label(DS_4844)`; page copy; `DS_AD26 = 1`; show; mouse mode 1; draw
the car (`car_draw(4, car->value, car_flags(car), 0, 0xB9, 0x4D30 + 8·G(tyres))`), save the two 0x18×0xC rects at
`(AD62 − 0x10, AD64 − 5)`, show the nozzle-less pump images. State: `wait = 160` ticks of patience, `step = 20`
(fuel per $1, 0.1 gal units), `attended = 1`, `drive_off = 1`, `pump = 0`, `pumping = paid = nozzle = 0`,
`handle = 1`, meters 0. `gas_nozzle_draw(0x457); ui_push(0x16); gas_hotspots_pumps(); ui_mouse_mode(-2);` then loop
on `ev = ui_wait(7)` (jump table at `2925:2702`):
* **0 (tick):** if `pumping`: `gas_pump_ctl(pump, 2)`; every 8th tick: `car->fuel += step; paid = 1; money_add(money >= 1
  ? -1 : -money_lo);` and when `0xB9 − fuel < step` or `money < 1`: `gas_pump_ctl(pump, 3)`; if `!attended` →
  `gas_leave`, else `pumping = 0`. If not pumping: `if (--wait <= 0)` the attendant comes: `gas_hotspots_none();
  step = (pump == 2) ? 16 : 12; 2925:1c10()` (attendant animation, game_flow); `msg_box(0x45 "Yo, you need full service,
  dummy!"); attended = 0;` if `paid` → `gas_leave`; else `drive_off = 0`; if `handle == 1` { restore pump 1 image,
  `handle = 2`, mouse 2, `pump = 1` }; `gas_nozzle_draw(0x456); nozzle = 1; handle = 1; mouse 1;
  gas_pump_ctl(pump, 1); pumping = 1;`
* **1 (nozzle / car filler):** only when `9156`. If `handle == 1` (pumping): `gas_pump_ctl(pump, 3);
  gas_nozzle_draw(0x457); nozzle = 0; handle = 2; mouse 2;` if `0xB9 − fuel >= step && money >= 1` → `msg_box(0x46
  "It's not full, speedy!")`; `gas_hotspots_handle(pump); pumping = 0`. Else: if the tank is full (`0xB9 − fuel <
  step`) or `money < 1` → `msg_box(money >= 1 ? 0x25 "You've got a full tank!" : 0x26 "Gas ain't free!")`; otherwise
  `gas_nozzle_draw(0x456); nozzle = 1; handle = 1; mouse 1; gas_pump_ctl(pump, 1); gas_hotspots_b(); pumping = 1`.
* **2…5 (pump handles):** only when `9158`. If `handle == 1`: codes 2 and 5 → `msg_box(0x62 "Use another pump,
  dummy!")`; codes 3/4 → `pump = (ev == 3) ? 1 : 2`, show its taken-handle image (`9AC8+4·pump`), `handle = 2`, mouse 2,
  `gas_hotspots_handle(pump)`, `step = (ev == 3) ? 20 : 15`, `pumping = 0`. If `handle == 2` (hanging it back): if
  `money < 1 || paid` → `drive_off = 0; gas_leave`; else `handle = 1`, mouse 1, restore the pump image
  (`9A46+4·pump`), `pump = 0`, `gas_hotspots_pumps()`, `step = 20`, `pumping = 0`.
* **6, 7:** `msg_box(0x64 "Take a pump, dummy!")`.
* **8 (drive away):** `car->fuel > 4` → `drive_off = 0; gas_leave`; else `msg_box(0x63 "Get some gas first, speedy!")`.
So pump 1 gives 2.0 gal per $1, pump 2 1.5 gal; full service 1.2 / 1.6 gal per $1; the tank stops at 18.5 gal (0xB9)
during filling and is clamped to 17.0 gal when leaving.

## 5. Handoffs

* **race_render** (claimed there, not in this CSV): `0dcb:1470 track_lane_x`, `0dcb:504a/507a/50c4/50f4
  town_walls_*` (matcher names of 507a / 50f4 were swapped), `0001:d2a8 aq_lane_target` (Aqueduct opponent lanes,
  called by opp_advance_steer).
* **sound**: `0001:2622 race_isr` is owned by the sound spec; its physics half (from `28fa`) is specified in §3 here.
  `0001:2164 music_resume_if_on` (resume music when `DS:B8E0`), sound vectors `BB90` (engine), `BBB8` (siren),
  `BBCC` (screech), `BBC8` (crash), `BBAC` (breakdown), `BB9C` (pump).
* **game_flow**: `0001:b2ea` / `0001:b356 dragstrip_result` (time and round result screens, msgs 0x66–0x6F, 0x79),
  `2925:1642 / 16c8 / 1786 / 1c10` (Grudge Night UI, payout, attendant), `0001:a51c ending_king`, `0001:a4ea
  bob_sign_draw`, `0001:3db4 msg_box` (message table `DS:11EA`).
* **platform**: `0001:8c2c drive_abort_poll`, `0001:20a0 / 2176 race_isr_enter / leave`, `0001:17fa input_reset`.
* **garage**: car / part helpers `car_new 42aa`, `garage_remove_car 4716`, `part_release 46f0`, parts list
  `70e2 / 716a / 71ce / 427c`, `engine_bay 80a8`, `car_draw 1a21:89fa`. Both `opponent_init 0001:51f8` and
  `opponents_init 0001:534c` are listed under garage in sr2_modules.csv but specified here (S3.7 must keep one owner).
* **Duplicates with game_flow.md** (same functions, game_flow names): `2925:000e tournament_pick_field` = grudge_seed_field,
  `00fa tournament_seed` = grudge_dialins, `01fa tournament_next_round` = grudge_pair, `193e event17_leave` =
  event_drive_home, plus `event17_run`, `event15_run`, `gas_*`, `drive_to_event`, `opponent_init`, `opponents_init`.
  One factual difference to resolve in S3.7: game_flow's `drive_to_event` says `has_clock` tests the **engine** part;
  the code reads car `+06` = the **transmission** (`G(trans) < 1` = automatic), as written here.
* Data to race_render: `race_kind`, `start`, `track_end`, `lights` (`A7A2` bits), `start_panel` (`B7B2`),
  `dash_type` (`AB28`), player/opponent positions; from race_render: `opp_pose`, `opp_close`, `rear_ended`,
  `A7D6`, `7FCE`, `7BA2`, `BBF2`, road edges, Aqueduct probe.

## 6. Open questions

* **Uninitialised stock-car fields** (model_car_setup / car_max_speed): the flags word `+26` and the exhaust /
  muffler / differential pointers are stack garbage in the original and influence opponent top speed and curves
  (±5 %, plus up to +13 % via the "missing customisation" terms if bits happen to be set). Port proposal: initialise
  flags = 0 and build the three parts from spec B like `car_new 0001:42aa` (`exhaust grade*3+make+0x35`, muffler
  `0x3B+n`, differential `0x3D+n`); a DOSBox comparison of an opponent's `opp_vmax` (`DS:B8E4`) would settle it.
* The holeshot flag `AA10` is decided on the first `race_step` call when both cars stand at `start ≥ 8`; it is then
  always 0 unless the opponent moved first — looks like the intended "first to move 8 eighths" test was meant
  relative to the start. Reproduce literally.
* `drive_run` returns an uninitialised value when the trip is skipped (`DS:4B64` or abort) — port returns 0.
* `race_results` crash menu returns `code − 2` for codes other than 1/2 (timeout 1000 → 998). Callers treat it like
  "stay at Bob's" (any value other than 1, 2, 4). Kept.
* The King and the police share car model 0x15 (`king_chance` returns 0 after either).
* Minute-hand side length of the clock (assumed 3/4 of the tip length like the hour hand).
