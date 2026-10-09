# garage - Street Rod 2: The Next Generation (1991), delta spec (S3.4)

Scope: the garage and everything done from it: the garage screen and its hot spots, the UI toolkit that the shops share (screens as element arrays,
message boxes, list boxes, edit fields, cursor, arena helpers), used cars / classified ads (SR2_DATA cars and people), the auto-parts shop with the
**64-part catalogue in 8 categories** (`DS:4502`; automatic transmissions are new), buying / selling cars and spare parts, paint, customising, the
"Under the Car" screen (exhaust manifold / muffler / differential), tyre and transmission changing, the engine bay (**`engine_bay` 0001:80a8**),
tune-up, "Car info", the calendar and newspaper, new game / opponents, the car pictures (`car_compose`, `car_draw`), Bob's car-hop helpers, the
juke-box toggle and the gas-station *pictures*. Decoded here from the code that reads them: the engine-bay / compatibility / picture tables
`DS:47A8-4EB4` (and the bay geometry tables `DS:201A-20CE`).

SR2 segments: `0001:0a68-0f7e/1048-11a2` (classifieds), `0001:3934-4d90` (pools, car/part records, your cars), `0001:51f8-550e` (opponents, new game),
`0001:63e0-7a44` (calendar, garage screen, shops), `0001:80a8` (engine bay), `1a21:1954-2d90` (message boxes, cursor), `1a21:2e92-3d10` (transmission
screen + engine-bay drawing), `1a21:4284-60d2` (UI toolkit), `1a21:8268-b9bc` (car drawing, animation scripts, tyre change), `2925:2b26-3174` (under-car screen).
Function counts: **115 listed in `port/sr2_modules.csv`** (1 same, 83 changed, 31 new by the matcher) **+ 8 functions claimed from other modules** (see Handoffs).
Of the 123 function sections: 29 are "= SR1" references (recompilation / DS remap only), 60 describe changed behaviour, 34 are new functions.

Builds on the SR1 spec `../StreetRod/port/spec/garage.md` (and the SR1 port code `../StreetRod/srport/src/game/garage_*.c`, `ui_*.c`); section references
"§x.y" are to that file. SR2 reuses the SR1 structure (car / part pools, ads, haggling, bay layers) but rebuilt almost every data layout; the main
changes are listed first:

* **CAR record is 0x2A bytes** (SR1 0x28) with three new part slots (exhaust manifold `+1C`, muffler `+1E`, differential `+20`), one bolt per
  carburettor, `engine_link` at `+0x22`, ignition at `+0x23` (range -5..+3), gas at `+0x24`, flags word at `+0x26` (no sticker field), link at `+0x28`.
* **PART catalogue** `DS:4502`: 64 records of 10 bytes in 8 categories (SR1: 43 x 8, 5 categories); automatic transmissions (grade 0), V-6 engines for sale.
* **Pools**: 16 cars at `DS:AEC2`, 190 part nodes at `DS:B1BE` (the garage is full at 181 parts).
* **UI**: screens are arrays of 18-byte *elements* (`DS:3AD8[36]`), not records of a global table; message boxes are looked up by **message number** (`DS:11EA`);
  list boxes use near text pointers (`DS:A61C`) and 0-based rows; a new text-edit module; sound calls go through vector slots (`DS:BB8C + 4k`).
* **Classified ads** are a weekly rolling list of 15-25 car ads (new and used copies, 42 ids) + all 64 parts (`DS:9B40` 128-byte block, saved in games).
* **Engine bay** has 6 bolts, a single bolt sprite sheet, layout codes 4..16 and new fit tables; **tune-up** is a timing-light animation with engine sound.
* **No stickers, no `part_value`, no demo cheats**; "Under the Car" (garage code 14) replaces the sticker screen; "Car info" is hand-drawn.

Conventions: code `SSSS:OOOO` as stored in the file (Ghidra segment = file + 0x1000); `DS:xxxx` = DGROUP (image `0x3E080 + off`; exe file offset `0x40C80 + off`).
Pictures are written as LIB2 ids (`1000 + n`, hex shown where the code uses it). `rnd(n)` = `1a21:6180` (0..n-1). `msg_box(id)` = message box by number (table
below), `ui_menu(s)` = `ui_push(s); wait for a non-zero code; ui_pop(1)`, `ui_dialog(s)` = `ui_save_bg + ui_menu + restore`, `ui_wait(t)` = poll hot spots with
time-out `t` ticks (returns 0 on time-out). `cursor_ctl` = `1a21:2d90`. `present` = `DS:AD26`; `back` = `DS:AD14/AD16`, `front` = `DS:AD10/AD12` (far page pointers).
Driver vector slot `k` = far pointer at `DS:A0AE + 4k` (video spec): 0 blit, 1 shifted blit, 2 masked blit, 3 text, 5 copy page, 8 present/palette,
9 anim step, 11 slide sprite (SR2: 2 extra mask arguments), 12 line. Sound vector slot `k` = `DS:BB8C + 4k` (sound spec). Sound slot roles named in this spec are
inferred from the call context.

---

## Records

```c
typedef struct PART {         /* 8 bytes, pool DS:B1BE (190 nodes), free list DS:AA0C */
    i16 value;      /* +0 price paid / catalogue price when new */
    i16 wear;       /* +2 0..10000 (hundredths of a percent), -128 = does not wear */
    i16 type;       /* +4 index into the catalogue DS:4502 (0..63), -1 = free */
    PART *next;     /* +6 spare list DS:AA0A / free list */
} PART;
typedef struct CAR {          /* 0x2A bytes, pool DS:AEC2 (16 records), free list DS:AA06 */
    i16 value;      /* +00 */
    i16 model;      /* +02 index into DS:A80C (0..21), -1 = free */
    i8  colour;     /* +04 0..5 */
    u8  klass;      /* +05 copy of MODELS[model].class */
    PART *trans;    /* +06 */
    PART *engine;   /* +08 */
    PART *manifold; /* +0A */
    PART *carb[3];  /* +0C +0E +10 */
    PART *tyres;    /* +12 */
    i8  trans_bolt[2];   /* +14 +15  0 out, 1 loose .. 3 tight */
    i8  bolt[6];         /* +16 engine, +17/+18 manifold, +19/+1A/+1B carburettor 0/1/2 */
    PART *exhaust_manifold; /* +1C */
    PART *muffler;       /* +1E */
    PART *differential;  /* +20 */
    i8  engine_link;     /* +22 0 no engine, -1 unplugged, 1 connected */
    i8  ignition;        /* +23 -5..+3 */
    i16 gas;             /* +24 tenths of a gallon (full tank: 170 max at the station, initial 40) */
    u16 flags;           /* +26 high byte: 0x20 roof chopped, 0x40 rear bumper stripped, 0x80 front bumper stripped; low byte unused */
    CAR *next;           /* +28 */
} CAR;                   /* the savegame order of the part pointers is +06,+08,+0A,+0C,+0E,+10,+12,+1C,+1E,+20 (FORMATS.md) */
typedef struct MODEL {        /* 20 bytes, DS:A80C (SR2_DATA block 1), 22 models (0..20 cars, 21 = the King's car) */
    i16 price, trade_in; u8 class; u8 next_for_sale; u8 b6 /*runtime rnd(5) = preview colour*/; u8 b7;
    u16 specA, specB;   /* bit fields of FORMATS.md; specA low nibble = make mask 1 GM / 2 Ford / 4 Chrysler */
    u16 ad_ptr; u16 ad_lines; u16 picture /*LIB2 id*/; u16 flags_unused; } MODEL;
typedef struct OVERLAY { u8 race, b1; u16 roof, scoop, rear_bumper, front_bumper; } OVERLAY;   /* 10 bytes, DS:BAAC; ids are LIB2 ids, 0 = none */
/* layout 12 bytes DS:9BF4: wheel x rear/front, driver x/y, roof x, scoop x/y, rear bumper y, front bumper x/y, gas-cap x/y; hot-spot rects 20 bytes DS:9872: 5 x {dx,dy,w,h} */
typedef struct PERSON { u8 a, b, c, d; u16 face, name_ptr; u16 model; u8 colour, b0B, b0C, flags, b0E, b0F, b10, b11; } PERSON;   /* 18 bytes, DS:AB7E, 22 persons */
typedef struct UIEL { u16 flags, text; i16 scheme, code, x0, y0, x1, y1; u16 keys; } UIEL;   /* 18 bytes, screens DS:3AD8[0..35] */
```
`CAR.value` of a stock car = price paid; `PART.value` = catalogue price; selling/ripping parts changes `CAR.value`. `MODEL.b6` is rolled by
`hot_data_load` (1a21:6e6a) when SR2_DATA is read.

## UI toolkit (shared by every shop screen)

* **Screens**: `DS:3AD8` = 36 near pointers; screen `s` = array of 18-byte elements ending with the element whose flag word has bit 1. Flag bits: `0x01` active hot spot,
  `0x02` last, `0x04/0x08` panel box, `0x10` button, `0x20` (first element) fill the list area, `0x40` frame, `0x80` static text, `0x100` centred text, `0x200` edit field,
  `0x400` title/selection field, `0x800` list row. `code` is what `ui_wait` returns when the element is clicked or its hot key pressed (`+0x10` low/high byte = two
  scancodes, 0xFF none): negative = buttons, positive = row/hot-spot numbers. Colour schemes: `DS:9DC6 + 5*scheme` (fg word, bg word, frame byte).
* `ui_push(s)` / `ui_pop(1)` activate / deactivate a screen (stack `DS:B874`, depth `DS:B87C`); current element array `DS:B894`; list rows are discovered while drawing
  (`DS:B888` first row element, `DS:B88A` row count, `DS:B890` title/edit element); `DS:B898` = element under the pointer; `DS:B88E` = selected list row (0-based).
* **Message boxes**: `msg_box(id)`, `msg_draw_at`, `message_at`, `msg_close` use the message table (below) and box table `DS:1E46`.
* **Lists**: `list_box(screen, sel, texts, wear, n)` (`1a21:55ea`) for 0-based arrays of near text pointers `DS:A61C` and optional wear bytes (0x80 = none).
* **Screens used by the garage code** (element codes; negative = button): 0 garage; 2 new-game name; 3 newspaper (1 Used Cars, 2 Auto Parts, 3 garage); 4 classified page (-1 prev page, -2 next page, -3 garage, rows 1..24);
  5 buy dialog (-25 See it, -20 Yeah, -21 Forget it); 6 click anywhere; 7 spare-part list (-3 Change / Sell it, -4 Forget it, -7..-10 scroll); 8 "Will you take" (-20 OK, -21 No thanks); 9 your cars (-1 Switch it, -2 Sell it, -3 Forget it);
  10 haggle (-24 Offer, -22 Never mind); 11 "How about" (-20 OK, -21 No); 12 transmission (-2 Select/Parts, -1 Done, bolts 1,2); 13 paint (1 Previous, 2 Next, 3 Go ahead, -23 Forget it); 14 engine bay (bolts 1..9, -13 Wires, -3 Tune, -1 Done, -2 Parts);
  15 customise (-20 OK, -22 Forget it); 20 Bob's; 21/22 gas pumps; 26/28 load/save slot lists; 27 error box; 30 timing light (-7 Retard, -10 Advance, -1 Done); 33 fatal message; 34 under the car (-2 exhaust manifold, -3 muffler, -4 differential, -1 Done).
  The full dump of the 36 screens (element rectangles, labels, hot keys) is in `work/` terms: `python` over `DS:3AD8`; the garage screen 0 elements are listed below.

Garage screen 0 elements (code: label (x0,y0)-(x1,y1)): 2 Hit the street (0,53)-(39,124); 17 Grudge Night, Entry fee - $30 (198,63)-(219,82); 1 Check out the newspaper (4,162)-(38,184); 11 Rear bumper, 13 The roof,
9 Change transmission, 10 Pop the hood, 12 Front bumper (rectangles set by `garage_hotspots_car`); 6 Cars (72,43)-(266,54); 14 Under the Car (80,172)-(135,182); 8 Change tires (144,169)-(175,185); 4 Time to quit (161,64)-(177,74);
5 Sell parts (139,70)-(151,84); 7 New paint job - $20 (208,170)-(219,187); -40 Squelch it ! (237,74)-(258,83); 16 Car info (232,97)-(252,103); 15 Wanna tank ? (269,163)-(302,181); 3 Calendar (76,65)-(104,95); title field (flag 0x100) at (75,46)-(264,53) showing `DS:220A`.

**Garage dispatch (`game_loop` 0001:4dd6, game_flow spec)**: 1 newspaper (needs `garage_full_check`), 2 Bob's drive-in (car runnable, gas > 8), 3 `calendar_show(0)`, 4 `quit_menu`, 5 `sell_spare_parts`, 6 `your_cars`, 7 `paint_job`, 8 `change_tires`, 9 `change_transmission`,
10 `arena_release_levels(); engine_bay(cur_car)`, 11/12/13 `customize(2/1/4)`, 14 `under_car_menu`, 15 gas (needs runnable car, gas < 0x96 = 15.0 gal, money > 0 → `event15_run` = the gas station; messages 37 "full tank", 38 "Gas ain't free!"),
16 `car_info_sr2`, 17 Grudge Night (day % 7 == 2, gas > 0x17, `pay_or_complain(30, 114)`), -40 juke box (below). Each service advances `game_clock` as documented per function (hour = 0x222). Messages for refusals: 9 no car, 36 not enough gas,
39 car not running, 40 pick a car first.

**Juke box (code -40, "Squelch it !")**: toggles `DS:5304` (music on): off → `music_resume()` (`DS:5304 = 1`), on → `music_stop()` (`DS:5304 = 0`) (sound module); SR1's `DS:58D0`.

**Gas station**: the SR2 gas station is `event15_run` (2925:2104, DS:1BC8 = 3, race module's list) with the pumps/hose helpers `2925:1ab8-1f38/20be`; from the garage side this spec covers its pictures (`car_draw` location 3, `pump_overlay`), the
fuel limit (`gas` capped at 170 = 0xAA by the station) and the messages 36-38, 69, 70, 98-100. The SR1 `gas_station`/`gas_leave` are not in this module.

## Globals

Owner state and records (all DGROUP):

| DS | name | type | meaning | SR1 DS |
|---|---|---|---|---|
| A9FC/A9FE | `money` | i32 | bankroll ($); new game: 1200 (+40000 in demo) | 7EAA/7EAC |
| AA02 | `cur_car` | CAR* | the car in the garage (0 = none) | 7EB0 |
| AA04 | `other_cars` | CAR* | list of other owned cars (link `+0x28`) | 7EB2 |
| AA06 | `car_free_list` | CAR* | free car records | 7EB4 |
| AA08 | `cars_used` | i16 | allocated car records (limit 16) | 7EB6 |
| AA0A | `spares` | PART* | spare-part list (link `+6`) | 7EB8 |
| AA0C | `part_free_list` | PART* | free part nodes | 7EBA |
| AA0E | `parts_used` | i16 | allocated nodes (limit 181 = 0xB5; the pool has 190) | 7EBC |
| AEC2 | `car_pool` | 16 x 0x2A | car records | 83B0 |
| B1BE | `part_pool` | 190 x 8 | part nodes | 866A |
| 1F6A/1F6C | `game_clock` | u32 | game time (0x222 = 1 h, 0x1998 = 12-h day) | 05FC/05FE |
| 1F6E/1F70 | `tick_clock` | u32 | timer ticks | 0600/0602 |
| 1F72/1F74 | `clock_origin` | u32 | `tick_clock` at the start of the game | 0604/0606 |
| 1BC6 | `end_flags` | u16 | bit0 summer over, bit4 broke (0x10); non-zero ends the game (0x29A) | 0282 |
| 1BC8 | `location` | u16 | 1 garage, 2 street/Bob's, 3 gas station, 4 Phyllis event, 5 newspaper, 6 Bob's drive-in (selects `car_draw` tables) | 0284 (`drive_dest`) |
| 9730 | `demo_flag` | u16 | copy of the `demo` command-line switch (`DS:B8D0`) | 0610 |
| A9E8 | `player_name` | char[16] | driver's licence name (header of the savegame) | |
| 4C54 / 4C56 | `switch_old` / `switch_new` | CAR* | cars leaving / arriving at "Switch it" | 4F46 / 4F48 |
| AD5C / AD5E / AD60 | `car_x/y/h` | i16 | where `car_draw` put the car (hot spots, pumps) | 825E/8260/8262 |
| AD62 / AD64 | `gascap_x/y` | i16 | gas-cap point (layout `L10/L11` + car position) | 826E/8270 |
| AD6E / AD70 / AD72 / AD74 / AD76 / AD78 | `garage_car_*` | i16 | picture, colour, tyre grade, driver flag/pic, model, flags (roof/bumper bits) of the garage car | 827A-8284 |
| 220A | `garage_title` | char[44] | ad title of `cur_car` shown on the garage screen | 29FF |
| 4D8A | `blank_title` | char[32] | blank source for `garage_name_clear` | 5094 |
| 4844 | `status_money_fmt` | char* | pointer to the money format for `status_label` | |
| 5304 | `music_on` | i16 | juke-box flag | 58D0 |

Classified ads (SR2 new; the 128-byte block `DS:9B40..9BBF` is saved in the savegame after `DS:1F72`, FORMATS.md "0x80 bytes at DS:9B40"):

| DS | name | type | meaning |
|---|---|---|---|
| 9B40 | `ad_week` | u16 | week (0..11) of the stored ad list; 0xFFFF after `new_game` |
| 9B42 | `ad_ids[26]` | u8[26] | ids 0..41 of this week's car ads (0..20 new, 21..41 used = model-21), `0xFF` end, bit 0x80 = sold |
| 9B5C | `ad_cool[42]` | u8[42] | per-id counters: 0 = available; 1 = shown this week; +3 when bought |
| 9B86 | `ad_pool` | u16 | number of ids with counter 0 (42 at a new game) |
| 9B88 | `ad_part_row` | u16 | first part row of the classified list |
| 9B8A | `ad_price[27]` | u16[27] | price shown per car row |
| 9F2C | `ad_lines[]` | i16[] | `[0]` = number of rows, `[i]` = text lines of row i |
| A61C | `list_text[]` | char*[] | near text pointers of the current list (0-based; also classified rows by number) |
| 1EC8 / 1ECA / 1ECC / 1ECE:1ED0 / 1ED2 | page vars | | first row shown / last row shown / `DS:9F2C` / paper picture far ptr / redraw flag |
| 1ED4..1EDE | paper rect | i16[6] | `{0x40, 0xBE, 0, 0, 0, 0}` (strips at dx 0 and 0x100) |

Tables (const, in the exe; details in "Tables"):

| DS | name | meaning |
|---|---|---|
| 4502 | `PARTS` | catalogue, 64 x 10 bytes |
| 478C | `INIT_WEAR` | 8 words by category: {0, 0, -128, -128, 0, -128, -128, -128} (SR1 4966) |
| 4E6E | `CARBS_PER_MANIFOLD` | 9 words {1,1,3,2,2,1,2,0,0} (SR1 5126) |
| 4D1E | `PAINT` | 6 x {reg6, reg7, strip bg} (SR1 50C2) |
| 4D30 | `TYRE_PICS` | 3 x 4 wheel picture ids: {1050-1053}, {1014-1017}, {1084-1087} (SR1 5030) |
| 483A | `TRANS_PICS` | {1077, 1078, 1079, 1080} (SR1 49D2) |
| 4C5A | `NEWS_PICS` | 12 headline photos by week |
| 4D48-4D5A | calendar tables | x0 33; `CAL_Y0` {14,107,15,105} @4D4A; `CAL_ROW0` {0,5,9,14} @4D52 |
| 47A8 / 47C4 / 47E0 | `MANIFOLD_FITS`, `CARB_FITS`, `BAY_PICS` | engine-bay compatibility and picture tables |
| 4E80 / 4E8C / 4E9A / 4EA8 / 4EB4 | `BOLTS_PER_LAYER`, `BOLT_CTX`, `BOLT_SIDE`, `LAYER_CAT`, `BAY_LAYOUT` | engine-bay bolt / layer tables |
| 201A-20CE | bay geometry | see "Tables" |
| 9872 / 9BF4 / BAAC | `MODEL_HOTSPOTS`, `MODEL_LAYOUT`, `MODEL_OVERLAYS` | per model (SR1 70E4, 7682, 8DF0) |
| A80C / AB7E | `MODELS`, `PEOPLE` | SR2_DATA blocks 1 and 0 (SR1 7D86, 7FF8) |
| 11EA / 1E46 / 1E7C:1E7E / 881A | message table / box table / open-box background / current box | message system |
| 3AD8 | `SCREENS` | 36 near pointers to element arrays |

Engine-bay runtime (SR1 equivalents in brackets): `AD84[5]` slot pictures (8290), `AD8E[6]` bolt tightness (829A), `AD9A` connector picture (82AC), `AD9C` context (82AE), `ADA0` side (82B2), `AD18/AD1A` bay bitmap (8232),
`47F2` manifold-0 picture (V-6 0x429 / V-8 0x42A), `4E8E`/`4E9C` bolt context / side arrays, `209C` first-carburettor context per manifold grade, `2018` animate flag (06FC), `AD7A` transmission picture (8286), `AD26` present flag.

UI toolkit runtime: `B874[]` screen stack, `B87C` depth, `B894` current screen, `B888/B88A/B890/B892` list row / row count / title field / edit flag, `B886` list fill flag, `B88C/B88E` texts / selected row, `B898` hover element, `B87E-B884` hover rect,
`8824-882A` dirty bounds, `882C-8840` edit-field state, `1F3D/1F3F` text mode / last key, `2182` numeric edit, `3B3B` cursor counter, `A2D2-A2DE` cursor state, `A31E/A7D8/B7EC/B864` arena state.

Car-drawing runtime: `7906` strip bitmap, `7912/7914` strip x / width, `7916` two-pass state, `1BCA` animation running, `A808` car running during animation, `B8D4` has-muffler, `95DE-95E4` saved dirty rect, `95EE:95F0` town background, `9A4A..9ACE` pump bitmaps,
`AD58:AD5A` last view bitmap, `AA3C` frame tails, tyre change: `9600-96A6`, `95FC` jack pictures, `79E4` y base, `9668` car x.

## Tables

### Part catalogue `DS:4502` (64 parts, 8 categories)
Record (10 bytes): `i16 price`, `u8 grade`, `u8 make mask` (1 GM, 2 Ford, 4 Chrysler, 7 = fits all), `u8 category`, `u8 0`, `u16 text ptr` (near pointer to the NUL-terminated name), `u16 text lines`.
Categories: 0 engine (idx 0-8: grade x 3 makes), 1 **transmission (idx 9-20: grade 0 automatic, 1 three-speed, 2 four-speed, 3 racing four-speed, x 3 makes)**, 2 carburettor (21-23), 3 manifold (24-49: grade 0 two-barrel ... 4 racing, 5 tunnel
ram, 6 dual tunnel ram, 7 fuel injection, 8 supercharger (only GM and Chrysler exist)), 4 tires (50-52), 5 exhaust manifold (53-58: stock / tube headers x 3 makes), 6 muffler (59-60), 7 differential (61-63).
Index rules used by `car_new`: trans `(grade+3)*3+make`, engine `grade*3+make`, manifold `(grade+8)*3+make`, exhaust `grade*3+make+0x35`, carburettor `0x15+grade`, tires `0x32+grade`, muffler `0x3B+grade`, differential `0x3D+grade`.
A part fits a car when `make mask & (MODEL.specA & 0x0F) != 0`; engines/transmissions/manifolds/exhaust manifolds are make specific. `INIT_WEAR[category]` (DS:478C) = {0, 0, -128, -128, 0, -128, -128, -128}: engines, transmissions and tires wear;
carburettors, manifolds, exhaust manifolds, mufflers and differentials do not. Selling price rules: `sell_spare_parts`. Used-car purchases wear the engine and transmission to 60-90 %.

| idx | price | grade | make | category | lines | text (DS ptr) |
|---|---|---|---|---|---|---|
| 0 | 140 | 0 | GM | 0 engine | 1 | `3C0E` GM V-6 engine. |
| 1 | 140 | 0 | Ford | 0 engine | 1 | `3C26` Ford V-6 engine. |
| 2 | 140 | 0 | Chrysler | 0 engine | 1 | `3C40` Chrysler V-6 engine. |
| 3 | 365 | 1 | GM | 0 engine | 2 | `3C5E` GM V-8 engine. 327 cu.in. |
| 4 | 325 | 1 | Ford | 0 engine | 2 | `3C90` Ford V-8 engine. 302 cu.in. |
| 5 | 390 | 1 | Chrysler | 0 engine | 2 | `3CC2` Chrysler V-8 eng 318 cu.in. |
| 6 | 540 | 2 | GM | 0 engine | 2 | `3CF0` GM V-8 engine. 427 cu.in. |
| 7 | 575 | 2 | Ford | 0 engine | 1 | `3D1E` Ford V-8 eng 429 cu.in. |
| 8 | 650 | 2 | Chrysler | 0 engine | 2 | `3D3E` Chrysler V-8 eng 426 cu.in. |
| 9 | 175 | 0 | GM | 1 transmission | 1 | `3D70` Auto Trnsmsn GM. |
| 10 | 175 | 0 | Ford | 1 transmission | 1 | `3D8A` Auto Trnsmsn Ford |
| 11 | 175 | 0 | Chrysler | 1 transmission | 1 | `3DA4` Auto Trnsmsn Chrysler. |
| 12 | 210 | 1 | GM | 1 transmission | 1 | `3DC4` Trnsmsn 3-spd GM. |
| 13 | 200 | 1 | Ford | 1 transmission | 1 | `3DDE` Trnsmsn 3-spd Ford. |
| 14 | 220 | 1 | Chrysler | 1 transmission | 1 | `3DFA` Trnsmsn 3-spd Chrysler. |
| 15 | 325 | 2 | GM | 1 transmission | 1 | `3E1A` Trnsmsn 4-spd. GM. |
| 16 | 315 | 2 | Ford | 1 transmission | 1 | `3E36` Trnsmsn 4-spd. Ford. |
| 17 | 320 | 2 | Chrysler | 1 transmission | 1 | `3E54` Trnsmsn 4-spd. Chrysler. |
| 18 | 420 | 3 | GM | 1 transmission | 1 | `3E76` Racing 4-spd. GM. |
| 19 | 475 | 3 | Ford | 1 transmission | 1 | `3E90` Racing 4-spd. Ford. |
| 20 | 505 | 3 | Chrysler | 1 transmission | 1 | `3EAC` Racing 4-spd. Chrysler. |
| 21 | 25 | 0 | all | 2 carburettor | 1 | `3ECC` 2-brl. carb. Fits all. |
| 22 | 40 | 1 | all | 2 carburettor | 1 | `3EEA` 4-brl. carb. Fits all. |
| 23 | 60 | 2 | all | 2 carburettor | 2 | `3F08` 4-brl racing carb. Fits all models. |
| 24 | 20 | 0 | GM | 3 manifold | 1 | `3F38` GM 2-brl manifold. |
| 25 | 20 | 0 | Ford | 3 manifold | 1 | `3F52` Ford 2-brl manifold. |
| 26 | 20 | 0 | Chrysler | 3 manifold | 1 | `3F6E` Chrysler 2-brl manifold. |
| 27 | 40 | 1 | GM | 3 manifold | 1 | `3F8E` GM 4-brl manifold. |
| 28 | 40 | 1 | Ford | 3 manifold | 1 | `3FA8` Ford 4-brl manifold. |
| 29 | 40 | 1 | Chrysler | 3 manifold | 1 | `3FC4` Chrysler 4-brl manifold. |
| 30 | 50 | 2 | GM | 3 manifold | 1 | `405C` GM manifold. For 3-2brl.carbs. |
| 31 | 50 | 2 | Ford | 3 manifold | 1 | `4082` Ford manifold.'For''3-2brl.carbs. |
| 32 | 50 | 2 | Chrysler | 3 manifold | 2 | `40AA` Chrysler manifold.'For''3-2brl.carbs. |
| 33 | 70 | 3 | GM | 3 manifold | 1 | `3FE4` GM manifold.'For 2-4brl.carbs. |
| 34 | 70 | 3 | Ford | 3 manifold | 1 | `400A` Ford manifold.'For''2-4brl.carbs. |
| 35 | 70 | 3 | Chrysler | 3 manifold | 2 | `4032` Chrysler manifold.'For''2-4brl.carbs. |
| 36 | 90 | 4 | GM | 3 manifold | 1 | `40D4` GM racing manifold. |
| 37 | 90 | 4 | Ford | 3 manifold | 1 | `40EE` Ford racing manifold. |
| 38 | 90 | 4 | Chrysler | 3 manifold | 1 | `410A` Chrysler racing manifold. |
| 39 | 110 | 5 | GM | 3 manifold | 1 | `412A` Tunnel ram for GM |
| 40 | 110 | 5 | Ford | 3 manifold | 1 | `4148` Tunnel ram for Ford |
| 41 | 110 | 5 | Chrysler | 3 manifold | 1 | `4168` Tunnel ram for Chrysler |
| 42 | 140 | 6 | GM | 3 manifold | 1 | `418C` dual Tunnel ram for GM |
| 43 | 140 | 6 | Ford | 3 manifold | 1 | `41B0` dual Tunnel ram for Ford |
| 44 | 140 | 6 | Chrysler | 3 manifold | 1 | `41D6` dual''Tunnel''ram''for''Chrysler |
| 45 | 450 | 7 | GM | 3 manifold | 1 | `4206` Fuel Injection for GM |
| 46 | 450 | 7 | Ford | 3 manifold | 1 | `4228` Fuel Injection for Ford |
| 47 | 450 | 7 | Chrysler | 3 manifold | 1 | `424C` Fuel''Injection''for''Chrysler |
| 48 | 550 | 8 | GM | 3 manifold | 1 | `427A` Supercharger for GM |
| 49 | 550 | 8 | Chrysler | 3 manifold | 1 | `429A` Supercharger''for''Chrysler |
| 50 | 50 | 0 | all | 4 tires | 1 | `42C4` Tires. |
| 51 | 80 | 1 | all | 4 tires | 2 | `42DC` Tires. Brand name. |
| 52 | 150 | 2 | all | 4 tires | 2 | `4310` Racing slicks. |
| 53 | 15 | 0 | GM | 5 exhaust manifold | 1 | `433C` GM stock exhaust manifold. |
| 54 | 10 | 0 | Ford | 5 exhaust manifold | 1 | `435E` Ford stock exhaust manifold. |
| 55 | 20 | 0 | Chrysler | 5 exhaust manifold | 1 | `4382` Chrysler''stock''exhaust''manifold. |
| 56 | 60 | 1 | GM | 5 exhaust manifold | 2 | `43AE` GM tube headers exhaust manifold. |
| 57 | 55 | 1 | Ford | 5 exhaust manifold | 2 | `43D4` Ford''tube''headers''exhaust''manifold. |
| 58 | 65 | 1 | Chrysler | 5 exhaust manifold | 2 | `4400` Chrysler''tube''headers''exhaust''mnf. |
| 59 | 5 | 0 | all | 6 muffler | 1 | `442C` Stock Muffler. Fits all. |
| 60 | 15 | 1 | all | 6 muffler | 1 | `444C` Glass Pack Muffler. Fits all. |
| 61 | 50 | 0 | all | 7 differential | 2 | `4470` Differential. Gear ratio 2.78:1. |
| 62 | 80 | 1 | all | 7 differential | 2 | `44A0` Differential. Gear ratio 3.73:1. |
| 63 | 150 | 2 | all | 7 differential | 2 | `44D0` Differential. Gear ratio 4.11:1. |

### Engine-bay tables decoded (`engine_bay` 0001:80a8, `car_runnable` 0001:7a44)
Layer index L: 0 engine, 1 manifold, 2..4 carburettor 0..2; "grade" = catalogue byte `+2`.
* `DS:4E6E` `CARBS_PER_MANIFOLD[manifold grade 0..8]` = {1, 1, 3, 2, 2, 1, 2, 0, 0} words.
* `DS:4E80` `BOLTS_PER_LAYER[L]` words = {1, 2, 1, 1, 1} (sixth word 1 unused): engine 1 bolt, manifold 2, each carburettor 1.
* `DS:4EA8` `LAYER_CAT[L]` words = {0, 3, 2, 2, 2}: the catalogue category fitted next when `layers == L`.
* `DS:47A8` `MANIFOLD_FITS[engine grade 0..2][manifold grade 0..8]` bytes (1 = fits): V-6 (grade 0): {1,0,1,0,0,0,0,0,0} (only the 2-barrel and the 3x2-barrel manifold); V-8 grades 1 and 2: {1,1,0,1,1,1,1,1,1} (everything except the 3x2-barrel manifold).
* `DS:47C4` `CARB_FITS[manifold grade][carb grade 0..2]` bytes: grade 0: {1,0,0}; 1: {0,1,1}; 2: {1,0,0}; 3, 4, 5, 6: {0,1,1}; 7, 8: {0,0,0} (2-barrel carburettor on the 2-barrel / 3x2-barrel manifold, 4-barrel / racing 4-barrel on the others, none on injection / supercharger).
* `DS:47E0` `BAY_PICS[L][grade 0..8]` words (LIB2 ids; 0 = n/a), one row of 9: L0 engine {1061, 1062, 1063 (0x425-0x427), 0 x 6}; L1 manifold {1066 (0x42A), 1067, 1064 (0x428), 1068, 1082, 1210 (0x4BA), 1197, 1196, 1208 (0x4B8)};
  L2, L3, L4 carburettor {1069, 1070, 1071 (0x42D-0x42F), 0 x 6}. The first manifold entry `DS:47F2` is overwritten at run time: 0x429 (1065) for a V-6 engine (engine picture 0x425) else 0x42A (1066).
* `DS:4E8C` `BOLT_CTX[b]` words (b = 0..6): {0, 0, 1, 1, 5, 5, 5}: bolt 1 context 0 (engine), bolts 2-3 context 1 (manifold); bolts 4-6 get the carburettor layout code at run time (`BAY_LAYOUT`). `DS:4E9A` `BOLT_SIDE[b]` = {0, 0, 0, 1, 0, 0, 0} (bolt 3 is the second manifold bolt).
  `engine_bay`/`bay_hotspots` use the arrays shifted by one entry: `DS:4E8E[k]`, `DS:4E9C[k]` for bolt `k+1`.
* `DS:4EB4` `BAY_LAYOUT[manifold grade][carburettor 0..2]` words (carburettor placement code = bay context, 4..16): g0 {4/5, 4/5 (both set at run time: 4 = V-6, 5 = V-8), 0}, g1 {6,0,0}, g2 {7,8,9}, g3 {10,11,0}, g4 {12,13,0}, g5 {14,0,0}, g6 {15,16,0}, g7 {0,0,0}, g8 {0,0,0};
  `DS:209C[grade]` bytes {5 (4/5), 6, 7, 10, 12, 14, 15, 0, 0} = the first carburettor code, written with 4/5 by `bay_draw_car`.
* Geometry (bay bitmap coordinates, the bitmap is shown at screen (0x20, 4)): `DS:201A` transmission bolt y {78, 93}; `DS:201E` engine seat x,y {(25,168), (26,171), (14,178)}; `DS:202A` engine bolt {(147,144), (166,144), (174,149)}; `DS:2036` connector {(150,175), (168,176), (174,184)};
  `DS:204E` carburettor seat by code 4..16 {(80,71), (87,66), (80,66), (56,71), (80,71), (104,71), (64,66), (104,66), (64,67), (104,67), (88,49), (64,49), (104,49)}; `DS:2082` carburettor bolt offset by carburettor picture {(20,-5), (32,-3), (30,-3)};
  `DS:208A + code` (bytes, code 4..16) = slot of the code: {2,2,2,2,3,4,2,3,2,3,2,2,3}; manifold seat (0x38, 0x5B); manifold bolts x 0x4B / 0x84 or 0x9A, y 0x5A; `DS:2092 + 20*tight` bolt rectangles ({8x12}, {8x8, dy 4}, {8x4, dy 8}).
* Engine pictures 0x425/0x426/0x427 = V-6, small V-8, big V-8 (engine grade 0/1/2); connector pictures 0x432 (unplugged), 0x433 (plugged); bolt sprite 0x4A2; backdrops 0x41E (bay), 0x41F (transmission), 0x47B (tool bar).

### Message boxes: table `DS:11EA`
Entry `n` (6 bytes): `{u16 box, u16 line1, u16 line2}`; box 0/1/2 (1, 1, 2 lines; see `msg_draw`). Ids used by the garage code in this spec: 1 no go (patched), 2 It's yours!, 3 Not until Wednesday!, 4/5 parts gone / none,
6/7 Will you take (patched), 8/9 cars gone / no car, 10-13 haggling, 14 How about (patched), 15/16 accepted, 17 Here it is!, 18-20 garage full (20 patched), 21 outta dough, 22-24 won't fit, 26 can't tune, 27-30 no spare parts, 31/32/33 customise,
34/35 sell first, 36-40 gas/car, 97 need better graphics, 114 no Grudge stake, 116-118 no spare exhaust manifolds / mufflers / differentials, 119 Sold!. Full table (ids 1..121):

| id | box | line ptr | text (~ = narrow space in the font) |
|---|---|---|---|
| 1 | 1 | 0234 | No go, hot shot. You're $12345 short. |
| 2 | 0 | 025A | It's yours~! |
| 3 | 0 | 0268 | Not until Wednesday~! |
| 4 | 0 | 027E | All parts gone~! |
| 5 | 1 | 0290 | You've got no spare parts, dummy~! |
| 6 | 0 | 02B4 | Will you take $1234 ? |
| 7 | 1 | 02CA | I'll take it off your hands for nothing. |
| 8 | 0 | 02F4 | All cars gone~! |
| 9 | 1 | 0304 | You've got no car, dummy~! |
| 10 | 0 | 0320 | You must be kidding~! |
| 11 | 0 | 0336 | No way~! |
| 12 | 0 | 0340 | Get lost~! |
| 13 | 0 | 034C | No thanks~! |
| 14 | 0 | 0358 | How about $1234 ? |
| 15 | 1 | 036A | You must be crazy. I'll take it~! |
| 16 | 0 | 038C | I'll take it~! |
| 17 | 0 | 039C | Here it is~! |
| 18 | 2 | 03AA | You've got no room for all these cars~! / You're gonna hafta sell one. |
| 19 | 2 | 03F0 | You've got~~too~~much~~junk~~in~~your~~garage~! / You're gonna hafta sell a part. |
| 20 | 2 | 03F0 | You've got~~too~~much~~junk~~in~~your~~garage~! / Must~~sell~~12~~parts~~to~~make~~enough~~room. |
| 21 | 2 | 0470 | You're outta dough. / Looks like you're done for the summer~! |
| 22 | 2 | 04AC | Where will you put it, speedy~? / Make some room first ! |
| 23 | 1 | 04E4 | You've got no spare transmissions~! |
| 24 | 1 | 0508 | Pay attention, dummy~! That won't fit~! |
| 25 | 0 | 0530 | Paint ain't free~! |
| 26 | 1 | 0544 | Can't tune that mess, klutz~! |
| 27 | 1 | 0562 | You've got no spare engine~! |
| 28 | 1 | 0580 | You've got no spare manifold~! |
| 29 | 1 | 05A0 | You've got no spare carburator~! |
| 30 | 1 | 05C2 | You've got no spare tires, dummy~! |
| 31 | 1 | 05E6 | Ya can't chop this roof, dummy~! |
| 32 | 1 | 0608 | You've already lost your bumper~! |
| 33 | 0 | 062A | Customization ain't free~! |
| 34 | 0 | 0646 | Sell some cars first~! |
| 35 | 0 | 065E | Sell some parts first~! |
| 36 | 2 | 0676 | You don't got enough gas to cruise down / to Burger's. |
| 37 | 0 | 06AC | You've got a full tank~! |
| 38 | 0 | 06C6 | Gas ain't free~! |
| 39 | 1 | 06D8 | Your car's not running, speedy~! |
| 40 | 0 | 06FA | Pick a car first~! |
| 41 | 0 | 070E | What's his name~? |
| 42 | 0 | 0720 | Get lost~! |
| 43 | 0 | 072C | Give me a break~! |
| 44 | 0 | 073E | Get outta my face~! |
| 45 | 0 | 0752 | Buzz off~! |
| 46 | 0 | 075E | Whadda loser~! |
| 47 | 0 | 076E | You're on~! |
| 48 | 0 | 077A | Alright~! |
| 49 | 0 | 0784 | Let's go~! |
| 50 | 0 | 0790 | Blow it out your ear~! |
| 51 | 0 | 07A8 | Forget it, geek~! |
| 52 | 0 | 07BA | Beat it, twerp~! |
| 53 | 0 | 07CC | No way, ya nerd~! |
| 54 | 0 | 07DE | Cool ! |
| 55 | 0 | 07E6 | O.K. ! |
| 56 | 0 | 07EE | Let's hit it~! |
| 57 | 1 | 07FE | I got no time for amateurs like you~! |
| 58 | 2 | 0824 | You're doing OK kid, but you're gonna / need to win more races~! |
| 59 | 2 | 0864 | You're gonna hafta win  / Grudge Night first~! |
| 60 | 2 | 0892 | You're gonna hafta win more / Mulholland Races~! |
| 61 | 2 | 08C2 | You're gonna hafta win more / Aqueduct Races~! |
| 62 | 1 | 08F0 | You're on! Let's hit the drag race~! |
| 63 | 1 | 0916 | Let's hit the mulholland drive~! |
| 64 | 2 | 0938 | Cool, dude~! Get your pink slip out, / and let's go~! |
| 65 | 2 | 096E | You're gonna hafta get more gas / before you race. |
| 66 | 1 | 09A0 | You~~don't~~have~~enough~~dough~~for~~that~~bet~! |
| 67 | 0 | 09D2 | Chicken~! |
| 68 | 1 | 09DC | I knew you could do it~! You're my hero~! |
| 69 | 1 | 0A06 | Yo, you need full service, dummy~! |
| 70 | 0 | 0A2A | It's not full, speedy~! |
| 71 | 0 | 0A42 | Pay attention, dummy~! |
| 72 | 2 | 0A5A | Didn't your momma teach ya not to cheat, / You lose. |
| 73 | 1 | 0A8E | You won~! Too~~bad~~you~~didn't~~bet,~~though. |
| 74 | 1 | 0ABE | Big deal. Ten lousy bucks. |
| 75 | 1 | 0ADA | Big deal. Twenty-five bucks. |
| 76 | 1 | 0AF8 | Fifty smackers. Not bad. |
| 77 | 1 | 0B12 | $150 bucks, not bad for a days driving. |
| 78 | 1 | 0B3A | You just won a helluva car. |
| 79 | 1 | 0B56 | Beginner's luck, punk~! |
| 80 | 2 | 0B6E | You ran a good race. / You deserve to be King of the County~! |
| 81 | 1 | 0BAC | Ya jumped the light, speedy~! |
| 82 | 1 | 0BCA | You've been sentenced for debts, Ace~! |
| 83 | 0 | 0BF2 | Take a walk, sucker. |
| 84 | 0 | 0C08 | You lose, sucker. |
| 85 | 1 | 0C1A | Close, but no cigar punk~! |
| 86 | 2 | 0C36 | I~knew~you'd~choke~when~the~pressure~got~to~ya. / Better luck next time, sucker~! |
| 87 | 1 | 0C86 | You just dropped your~transmission,klutz~! |
| 88 | 1 | 0CB2 | Blew your engine again, huh, Ace~? |
| 89 | 2 | 0CD6 | You just dropped your~transmission~! / You lose. Take a walk, sucker. |
| 90 | 2 | 0CFC | Blew your engine again~? / You lose. Take a walk, sucker. |
| 91 | 1 | 0D16 | You lose. Take a walk, sucker. |
| 92 | 2 | 0D36 | Blew your tires again~? / You lose. Take a walk, sucker. |
| 93 | 2 | 0D4E | Lucky you weren't killed, cause the car / is totaled. You get $123 for scrap. |
| 94 | 2 | 0D9A | Lucky you're so slow. You only lost / the race. |
| 95 | 2 | 0D9A | Lucky you're so slow. You only lost / the race. But you need new tires. |
| 96 | 2 | 0DEA | Looks like ya took too much time, jr~! / You're done for the summer. |
| 97 | 1 | 0E2E | You need~~a~~better graphics card~~to~~paint~! |
| 98 | 0 | 0E5E | Use~~another~~pump,~~dummy~! |
| 99 | 0 | 0E7C | Get~~some~~gas~~first,~~speedy~! |
| 100 | 0 | 0E9E | Take~~a~~pump,~~dummy~! |
| 101 | 1 | 0EB6 | Get Your breaking time |
| 102 | 2 | 0ECE | Your breakout time is   . . / Run~~any~~faster~~and~~you~~will~~be~~disqualified. |
| 103 | 2 | 0F1E | You won round   with a time of   . . / You advance to the next round. |
| 104 | 1 | 0F1E | You won round   with a time of   . . |
| 105 | 1 | 0F64 | You lose the race by   .  seconds. |
| 106 | 2 | 0F64 | You lose the race by   .  seconds. / You're~~out~~of~~the~~running~~for~~the~~big~~money. |
| 107 | 2 | 0FBE | You beat some of the best drivers. / Your number ? finish earns~~you~~$   . |
| 108 | 1 | 100A | You crossed the center line. You lose. |
| 109 | 2 | 100A | You crossed the center line. You lose. / You're~~out~~of~~the~~running~~for~~the~~big~~money. |
| 110 | 1 | 1032 | You ran faster than your breakout time. |
| 111 | 2 | 1032 | You ran faster than your breakout time. / You're~~out~~of~~the~~running~~for~~the~~big~~money. |
| 112 | 1 | 105A | Drivers stage your vehicles~! |
| 113 | 2 | 1078 | Your breakout time is too weak. / You're~~out~~of~~the~~running~~for~~the~~big~~money. |
| 114 | 1 | 1098 | You~~have~~no~~money~~for~~Grudge~~Night~~stake! |
| 115 | 0 | 10CA | Put engine first~! |
| 116 | 1 | 10DE | You've got no spare exhaust manifolds~! |
| 117 | 1 | 1106 | You've got no spare mufflers~! |
| 118 | 1 | 1126 | You've got no spare differentials~! |
| 119 | 0 | 114A | Sold~! |
| 120 | 2 | 1152 | You just bumped the starter. Good thing / she's not hurt. You're disqualified~! |
| 121 | 2 | 11A0 | Opponent~~ran~~faster~~that~~his~~breakout~~time. / You've won this round~! |

---

## Functions (ordered by address)

### 0001:0a68 classifieds_open
New. `void classifieds_open(int *list, int first_row)` (near). Prepares the classified-ads page (SR1 `0000:08b4`):
`arena_reset_all()` (1a21:9b0e); `DS:1ECC = list` (pointer to the line-count array, see `classifieds`); colours `DS:AD28/AD2A/AD2C` =
scheme record `DS:9DC6 + 0x41` (fg, bg, frame); page paper source rect `DS:1ED4 = {w 0x40, h 0xBE}`, `DS:1ED8/1EDA/1EDC/1EDE = 0`;
`DS:1EC8 = first_row` (first row shown); `DS:1ED2 = 1` (page must be (re)drawn); `DS:1ED0 = DS:1ECE = 0` (paper picture not loaded);
then `ui_push(4)` (screen 4).

### 0001:0ac8 classifieds_run
Changed (SR1 classified-page loop `0000:09cb`). `int classifieds_run(void)`; returns -3 (Go to the garage) or the 1-based row number of
the clicked ad. Data: `list = DS:1ECC` (`list[0]` = number of rows `N`, `list[i]` = number of 9-pixel text lines of row `i`),
row texts `DS:A61C[i]` (near pointers to a sequence of NUL-terminated lines), `DS:9B88` = first part row, `DS:9B8A[i]` (u16) =
price shown in car row `i`. Screen 4 (elements, 0x12 bytes each, see "UI toolkit"): element 0 "Previous page" code -1 (0,0)-(64,190),
elements 1..24 the ad rows (code 1..24, x 64..256, y set here), element 25 "Next page" -2 (256,0)-(319,153), element 26 "Go to the
garage" -3 (268,154)-(319,188).
```c
for (;;) {
  if (DS:1ED2 /*redraw*/) {
     cursor_ctl(-4);
     if (!DS:1ECE:1ED0) DS:1ECE = pic_get(0x3F5, 3);                     /* paper picture LIB2 #13, kept in arena level 3 */
     blit(paper -> back, rect{0x40, 0xBE, sx 0, sy 0, dx 0, dy 0});       /* left margin strip */
     blit(paper -> back, rect{0x40, 0xBE, sx 0x40, sy 0, dx 0x100, dy 0});  /* right margin strip */
     colours = scheme+0x41;  back->height = 0xBE (saved, restored at the end);  fill_rect(back, x 0x40, y 0, w 0xC0, h 0xBE);
     y = 0; row = DS:1EC8; j = 0; el = DS:B894 /*element array of screen 4; the ad shown j-th uses el[j+1]*/;
     do {
        nl = list[row]; text = DS:A61C[row];
        hline(back, 0x44, y + 1, 0xB8);                                    /* separator above the ad */
        if (y + 3 > 0xBD) break;                                           /* next ad does not fit: stop (row not shown) */
        el[j+1].y0 = y + 1; el[j+1].flags = 1;
        el[j+1].code = (text == DS:1EE8 || text == DS:1EEE) ? 0 : row;      /* the two header rows are not clickable */
        price_patch = el[j+1].code >= 1 && row < DS:9B88;                        /* car ads: the '$' in the ad text is replaced by DS:9B8A[row] */
        y += 3;
        for (line = 0; line < nl; line++) {
           x = 0x44;
           for (;;) {                                                       /* one text line = segments joined by '+' */
              if (price_patch && (p = strchr(text, '$'))) sprintf(p + 1, "%d", DS:9B8A[row]);   /* fmt DS:1EF4 / 1EF7 */
              text_at(x, y, text);                                           /* driver vector slot 3 */
              text += strlen(text) + 1;  if (*text == 0) text++;            /* skip an empty string */
              if (*text != '+') break;
              text++;  x = DS:AD32 /*x after the text just drawn*/;          /* continuation on the same line */
           }
           y += 9;  if (y > 0xBD) break;                                     /* clipped */
        }
        el[j+1].y1 = min(y + 1, 0xBD);  j++;
        row = row >= N ? 1 : row + 1;                                       /* the list wraps around */
     } while (y < 0xBD);
     DS:1ECA = (row != 1) ? row - 1 : N;                                    /* last row shown */
     el[j].y1 = min(y + 1, 0xBD);  if (y + 1 < 0xBD) hline(back, 0x44, y + 1, 0xB8);   /* closing separator; the last ad reaches the bottom */
     for (k = j + 1; el[k + 1].code >= 0; k++) el[k].flags = 0;    /* unused row elements, up to the "Next page" button */
     DS:1ED2 = 0;  back->height restored;  cursor_ctl(-1);
     if (present) page_copy_rect(back, front, 0, 0, 0x140, 0xBE);  click_clear();
  }
  do r = ui_wait(3000); while (!r);
  if (r == -3) return -3;
  if (r == -2) first = DS:1ECA;                                             /* Next page: restarts at the LAST row shown (one row overlap) */
  else if (r == -1) {                                                       /* Previous page: walk back until a page's worth of rows fits */
     first = DS:1EC8 - 1; if (first < 1) first = N;
     for (room = 0xBD; (room -= 9 * list[first] + 3) > 0; ) { first--; if (first < 1) first = N; } }
  else { DS:1ED2 = 1; return r; }                                           /* an ad was clicked: it will be redrawn next time */
  DS:1ED2 = 1;  DS:1EC8 = first;
}
```
The elements of screen 4 are used in order: the first ad shown uses element 1, the next element 2, ... (the code walks `el = el + 1` once per
displayed ad). A row of text taller than the remaining page is clipped at y = 0xBD. SR1 differences: 24 element rows instead of 9; no
"page index" table (`DS:0518`); pages are computed from the current first row only.

### 0001:0e94 cheapest_ad_price

> **Secondary view (S3.7):** owned by `game_flow.md`; on any conflict the owner's section wins.
New. `long cheapest_ad_price(void)`: minimum price of the car ads that are still for sale. `min = 10000; for (k = 0; DS:9B42[k] != 0xFF; k++)
if (!(DS:9B42[k] & 0x80)) { id = DS:9B42[k]; price = id < 21 ? MODELS[id].price : MODELS[id-21].trade_in; min = min(min, price) }`; returns
0 if the list is empty, else `min` (sign-extended). Used by `broke_check`.

### 0001:0efe classifieds_add_car_row
New. `void classifieds_add_car_row(int id, int row, char *item)`: `*item = id;` `m = id > 20 ? id - 21 : id;` (ids 0..20 = the
model new at the dealer price, ids 21..41 = the same model "used" at the trade-in price); `DS:A61C[row] = MODELS[m].ad_ptr (+12)`;
`DS:9F2C[row] = (s8)MODELS[m].ad_lines (+14)`; `DS:9B8A[row] = id > 20 ? MODELS[m].trade_in (+2) : MODELS[m].price (+0)`.

### 0001:0f7e classifieds_roll_ads
New. `int classifieds_roll_ads(int row, char *items)`: chooses this week's car ads (called when the week changed). State (all in the save
block `DS:9B40`, see Globals): `DS:9B5C[42]` per-id cool-down counters, `DS:9B86` = number of ids with counter 0 ("pool"),
`DS:9B42[]` = chosen ids (byte `DS:9B40 + row`, i.e. `ads[row-2]`).
```c
total = rnd(11) + 15;                                   /* 15..25 ads */
want = total;
if (DS:9B86 <= total) {                                  /* pool too small: take every available id, age the others */
    want = total - DS:9B86;  DS:9B86 = 0;
    for (id = 0; id < 42; id++) {
        if (DS:9B5C[id] == 0) { DS:9B5C[id] = 1; DS:9B40[row] = id; classifieds_add_car_row(id, row, items); row++; items++; }
        else if (--DS:9B5C[id] == 0) DS:9B86++;
    }
}
while (want-- > 0) {                                     /* random ids from the pool */
    k = rnd(DS:9B86);  zeros = 0;  i = 0;
    do { if (DS:9B5C[i] == 0) zeros++; i++; } while (zeros <= k);       /* i-1 = id of the k-th id with counter 0 */
    id = i - 1;  DS:9B5C[id]++;                          /* = 1 */
    DS:9B40[row] = id;  classifieds_add_car_row(id, row, items);
    DS:9B86--;  row++;  items++;
}
return total;
```
A bought car adds 3 to its counter (see `classifieds`), so it stays out of the pool for several re-rolls. If the pool is ever empty while
`want > 0`, `rnd(0)` and the search loop would not terminate sensibly (not reachable in normal play).

### 0001:1048 classifieds_rebuild_ads
New. `int classifieds_rebuild_ads(int row, char *items)`: same week, rebuild the rows from the saved ids: `for (k = 0; DS:9B42[k] != 0xFF; k++)
{ classifieds_add_car_row(DS:9B42[k] & 0x7F, row, items); row++; items++; } return k;` (the 0x80 "sold" flag stays in `DS:9B42[k]`).

### 0001:107c classifieds_add_engines
New. `int classifieds_add_engines(int row, char *items)`: adds the 9 engines (catalogue index 0..8): for `i` = 0..8 and the text-pointer field
address `q = DS:4508 + 10*i`: `DS:A61C[row+i] = *(u16*)q; DS:9F2C[row+i] = (s8)q[2] /*lines byte*/; items[i] = i;` returns 9.

### 0001:10b6 classifieds_add_transmissions
New. Same for catalogue indices 9..20 (4 grades x 3 makes, automatic first), returns 12.

### 0001:10f2 classifieds_add_manifolds
New. Same for indices 24..49 (manifolds: 2-barrel ... supercharger), returns 26.

### 0001:112e classifieds_add_misc_parts
New. Same for indices 21..23 (carburettors) at rows `row..row+2`, then for indices 50..63 (tires 50-52, exhaust manifolds 53-58, mufflers
59-60, differentials 61-63) at rows `row+3 ..`; returns 17. (So the order of the parts section is: engines, transmissions, manifolds,
carburettors, tires/exhaust/muffler/differentials; all 64 parts are for sale - V-6 engines included, SR1 hid them.)

### 0001:11a2 classifieds
New (the SR1 `classifieds` 0000:442b, §4.4, restructured). `void classifieds(int section, int reroll)`; section 1 = Used Cars, 2 = Auto Parts;
`reroll` != 0 starts a new week of ads.
```c
DS:1BC8 = 5;                                    /* trip/location 5 = newspaper (SR1 location 5) */
DS:9F2E = 3; DS:A61E = DS:1EE8;                 /* row 1 = 3-line header "---------- USED CARS ----------" */
if (!reroll) n = classifieds_rebuild_ads(2, items);
else { memset(DS:9B42, 0xFF, 26); n = classifieds_roll_ads(2, items); }
DS:9F30[n] /* = lines of row n+2 */ = 3; DS:A61C[n+2] = DS:1EEE;      /* header row "------------ PARTS ------------" */
DS:9B88 = n + 3;                                /* first part row */
n += classifieds_add_engines(DS:9B88, items + n);           /* 9  */
n += classifieds_add_transmissions(n + 3, items + n);       /* 12 */
n += classifieds_add_manifolds(n + 3, items + n);           /* 26 */
n += classifieds_add_misc_parts(n + 3, items + n);          /* 17 */
DS:9F2C /*row count N*/ = n + 2;
classifieds_open(&DS:9F2C, section == 1 ? 1 : DS:9B88 - 1);   /* page starts at the section header */
for (;;) {
   r = classifieds_run();  if (r == -3) break;
   scr5 = DS:3AD8[5] elements; see_it = element with code -25;
   if (r < DS:9B88) {                                          /* a car ad */
       if (DS:9B40[r] & 0x80) { msg_box(119 /*Sold!*/); continue; }
       id = items[r - 2]; used = id > 20; m = used ? id - 21 : id;
       text = MODELS[m].ad_ptr; price = used ? MODELS[m].trade_in : MODELS[m].price;  see_it.flags = 0x11;
   } else {                                                     /* a part */
       t = items[r - 3]; text = CAT[t].text; price = CAT[t].price;  see_it.flags = 0;     /* "See it" greyed out */
   }
   e = first element of screen 5 after see_it whose flags have bit 0x100 (centred text);  e.text = text;    /* the ad title line */
   sprintf(strchr(e_next.text /*"You want to buy it for $"*/, '$') + 1, "%-3d ?", price);  /* DS:1EFA */
   while ((b = ui_menu(5)) != -20) {                            /* -25 See it, -20 Yeah, -21 Forget it */
       if (b != -25) goto next_click;
       see_it_preview(m);                                       /* below */
   }
   if (money < price) {                                         /* i32 compare */
       sprintf(strchr(MSG1_LINE /*"No go, hot shot. You're $12345 short."*/, '$') + 1, "%d short.", (u16)(price - money));
       msg_box(-1);  continue; }                                /* negative id: the box stays on screen */
   money_add(-price);
   if (r < DS:9B88) {
       c = car_new(m, price, 0);
       c->colour = (s8)MODELS[m].preview_colour;                /* model +6 (u16 loaded as rnd(5)): the colour shown by "See it" */
       if (used) { c->trans->wear = rnd(3000) + 6000; c->engine->wear = rnd(3000) + 6000; id = m + 21; } else id = m;   /* second-hand: 60-90 % worn */
       DS:9B42[r - 2] |= 0x80;  DS:9B5C[id] += 3;               /* sold flag; the same id is banned for the next re-rolls */
       msg_box(-2 /*It's yours!*/);  game_clock += 0x444;  break;
   }
   spare_add(t, price);  game_clock += 0x222;
   if (parts_used /*AA0E*/ > 0xB4) break;
   next_click: ;
}
hotspots_reset();                                               /* 0001:0e74: arena_reset_all, DS:1ECC = 1ED0 = 1ECE = 0, ui_pop(1) */
if (cur_car) set_paint_palette(cur_car->colour);
```
`see_it_preview(m)` (the "See it" strip, cars only):
```c
cursor_ctl(-4); arena_reset_all();
bg = arena_bitmap_alloc(0x140, 0x52, 0, 0); screen_save_rect(0, 0x40, 0x140, 0x52, bg);
colours: DS:AD29 = hi(scheme[+0x37]), DS:AD2B = hi(scheme[+0x39]), DS:AD2C = scheme[+0x3b];
set_paint_palette(preview_colour);
bgcol = (driver is EGA(-2) or VGA(-6)) ? (DS:1C04 = PAINT[preview_colour][2]) : DS:1BF6;
screen_fill_rect(0, 0x41, 0x140, 0x46, bgcol);  screen_fill_rect(0, 0x87, 0x140, 10, DS:AD29);
if (driver not EGA/VGA) { vline x 0 and 0x13F, y 0x40, h 0x52 on both pages; screen_hline(0,0x40,0x140); screen_hline(0,0x91,0x140); }
flags = (((specA >> 8) & 0x3C) == 0x20) ? 8 : 0;                /* supercharger visible */
car_draw(7, m, flags, 0, 0x86, &TYRE_PICS[(specA & 0x30) >> 4]);
screen_text(0x88-row y, x = (0x140 - text_width(ad)) / 2, MODELS[m].ad_ptr);   /* caption (first line only) */
cursor_ctl(-2); DS:1F3D = 1; ui_menu(6); DS:1F3D = 0;           /* screen 6: click anywhere */
screen_put_bitmap_mirror(bg, 0, 0x40); arena_reset_all();
```
Differences to SR1: weekly rolling ad list with cool-downs and "used" copies of cars (trade-in price, 60-90 % worn engine and transmission);
the part list contains all 64 parts; the demo money cheat is gone; the car you buy is painted with the model's fixed preview colour
(`MODELS[m]+6`) instead of a fresh random(5) (the preview colour itself is rolled when SR2_DATA is loaded); parts purchases stop the
session when `parts_used > 0xB4` (SR1 0x82); "It's yours" costs 2 hours (0x444), a part 1 hour (0x222) as before.

### 0001:3934 pools_init

> **Secondary view (S3.7):** owned by `game_flow.md`; on any conflict the owner's section wins.
= SR1 `pools_init` (0000:3614), ../StreetRod/port/spec/garage.md §4.2; DS remap: car pool `DS:83B0` → `DS:AEC2`
(16 records, now **0x2A** bytes, link at `+0x28` instead of `+0x26`, `model` at `+2` set to -1); part pool
`DS:866A` → `DS:B1BE` (**0xBE = 190** nodes of 8 bytes, link `+6`, `type` `+4` = -1; SR1 140); `car_free_list`
7EB4 → AA06; `cars_used` 7EB6 → AA08; `part_free_list` 7EBA → AA0C; `parts_used` 7EBC → AA0E. With
`reset_owner != 0`: `cur_car` (7EB0 → AA02), `other_cars` (7EB2 → AA04), `spares` (7EB8 → AA0A) and `money`
(7EAA/7EAC → i32 **A9FC**/A9FE) are zeroed; SR1's `DS:7EAE = 10` no longer exists.

### 0001:3e08 garage_full_check

> **Secondary view (S3.7):** owned by `game_flow.md`; on any conflict the owner's section wins.
= SR1 `garage_full_check` (0000:3b0c), §4.2. Differences: `cars_used` (AA08) >= 16 → `msg_box(34)` ("Sell some
cars first!"); `parts_used` (AA0E) >= **0xB5 = 181** (SR1 131) → `msg_box(35)` ("Sell some parts first!"). Returns 1
if either box was shown, else 0. Messages are now **message numbers** (table in §"Messages"), not text offsets.

### 0001:3e30 newspaper_front
Changed (SR1 0000:3b33, §4.4): now `void`, no random headline, and it runs the classifieds itself.
```c
void newspaper_front(void)
{   save = present_flag(DS:AD26); present_flag = 0;
    show_picture(0x3EF /*LIB2 #7 front page*/, -1);                 /* 1a21:21be */
    week = classified_week();                                        /* 0001:640e = min(day_index()/7, 11) */
    show_picture_at(NEWS_PICS[week], 0, 0x88, -1);                   /* 1a21:22a8; headline photo */
    show_picture_at(0x4A4 /*1188*/, 0, 0xA7, -1);                    /* bottom strip */
    cursor_ctl(-3); date_print(0x79); cursor_ctl(-1);               /* 1a21:2d90 hide / show */
    present_flag = save;
    if (save) page_copy_rect(back, front, 0, 0, 0x13F, 0xBD);
    (*DS:A0CE)();                                                    /* graphics-driver vector slot 8 (A0AE+4*8): palette/present refresh (video spec) */
    cursor_ctl(1);
    r = ui_menu(3);                                                  /* screen 3: 1 Used Cars, 2 Auto Parts, else leave */
    if (r == 1 || r == 2) {
        classifieds(r, week != DS:9B40);                             /* 0001:11a2; 2nd arg = "new week, re-roll the ads" */
        DS:9B40 = week; }
}
```
`NEWS_PICS` = `DS:4C5A`, 12 words (one per week of the summer): 1187, 1008, 1009, 1011, 1010, 1012, 1189, 1190,
1191, 1192, 1193, 1194 (LIB2 ids; week 0..11). `DS:9B40` is the week of the ad list currently stored
(-1 after `new_game`, so the first visit rolls the ads). game_loop uses the function only for its side effects
(there is no return code any more; code 4 "garage" simply returns).

### 0001:3f28 part_alloc

> **Secondary view (S3.7):** owned by `game_flow.md`; on any conflict the owner's section wins.
= SR1 `part_alloc` (0000:3c1b), §4.2; DS remap: `part_free_list` 7EBA → AA0C, `parts_used` 7EBC → AA0E.

### 0001:3f3e part_free

> **Secondary view (S3.7):** owned by `game_flow.md`; on any conflict the owner's section wins.
= SR1 `part_free` (0000:3c3e), §4.2; same remap (`next` at `+6`, `type` at `+4` = -1).

### 0001:3f5a sell_spare_parts
Changed (SR1 `sell_spare_parts` 0000:3c8d, §4.5; the matcher left it `new` and the module vote put it into game_flow). `void sell_spare_parts(void)`
("Sell parts" in the garage, code 5). Uses screen 7 (spare-part list) and screen 8 ("Will you take $.. ?" OK / No thanks):
```c
void sell_spare_parts(void)
{   rounds = 0; sel = 0;
    for (;;) {
        n = 0;
        for (p = spares /*AA0A*/; p; p = p->next) {                 /* link +6 */
            node[n] = p; DS:A61C[n] = *(u16*)(DS:4508 + 10*p->type);        /* catalogue text pointer */
            wear[n] = (p->wear == -128) ? 0x80 : p->wear / 100;  n++; }
        e = element of screen 7 (DS:3AD8[7]) with code -3; e->text = DS:226C /*"Sell it"*/;      /* relabel the "Change" button */
        r = list_box(7, min(sel, n - 1), DS:A61C, wear, n);   e->text = DS:2274 /*"Change"*/;
        if (r == -4) return;                                        /* Forget it */
        if (r == -3) sel = DS:B88E;                                 /* Sell it (the button is code -3) */
        else if (r == 0x3EA) { msg_box(rounds == 0 ? 5 : 4); return; }     /* "no spare parts, dummy" / "All parts gone" */
        p = node[sel]; v = p->value; q = (10000 - p->wear) / 100;   /* remaining life in %; 101 for parts that do not wear (-128) */
        switch (CAT[p->type].category) {                            /* byte DS:4506 + 10*type */
        case 0: case 1: price = v * q * 9 / 1000;       break;      /* engine, transmission: 90 % of value x remaining life */
        case 2: case 3: price = v * (rnd(20) + 70) / 100; break;     /* carburettor, manifold: 70..89 % */
        case 4: price = 0; break;                                    /* tires */
        case 5: case 6: case 7: price = v * 7 / 10;     break;      /* exhaust manifold, muffler, differential: 70 % */
        }                                                           /* all in 32-bit arithmetic */
        price = (price / 5) * 5;                                    /* down to a multiple of $5 */
        if (price == 0) id = 7; else { id = 6; sprintf(DS:1210 /*message 6 line*/, "Will you take $%-d ?" /*DS:4C72*/, price); }
        msg_draw_at(id, -1, -1);  r = ui_dialog(8);  msg_close();   /* -21 = No thanks */
        if (r != -21) {
            unlink node[sel] from the spare list (if sel == 0 spares = p->next else node[sel-1]->next = p->next);
            part_free(p);  game_clock += 0x444;  money_add(price); }
        rounds++;
    } }
```
Differences to SR1: no V-6 special value (parts always sell relative to `value`); fixed rules per category (the SR1 asm used the same
idea with the old five categories); message numbers 4, 5, 6, 7; list is 0-based; sold parts are freed (`part_free`).

### 0001:423a car_alloc
= SR1 `car_alloc` (0000:3fb1), §4.2; remap: `car_free_list` 7EB4 → AA06, `cars_used` 7EB6 → AA08; link offset
`+0x26` → **`+0x28`**.

### 0001:4250 car_release
= SR1 `car_release` (0000:3fd4), §4.2; link offset `+0x28`, `model` (`+2`) = -1, remap as `car_alloc`.

### 0001:426c spare_add
= SR1 `spare_add` (0000:3ff0), §4.2; `p->wear = INIT_WEAR[PARTS[type].category]` with `PARTS` now at **`DS:4502`**
(10-byte records, category at `+4`, so the byte read is `DS:4506 + 10*type`) and `INIT_WEAR` = `DS:478C` (8 words,
see "Part catalogue"); `spares` 7EB8 → AA0A.

### 0001:42aa car_new
Changed (SR1 `car_new` 0000:4036, §4.3). `CAR *car_new(int model, int value, CAR *reuse)`. Same structure; new record layout
(0x2A bytes, see "Records"), model record 20 bytes at `DS:A80C` (SR2_DATA block 1), SR2 part slots and specs:
```c
M = &MODELS[model]  /* DS:A80C + 20*model: +0 price +2 trade-in +4 class +5 next_for_sale +8 specA(u16) +10 specB(u16) +12 ad ptr +14 ad lines +16 picture */
c = reuse ? reuse : car_alloc();
c->model = model; c->value = value; c->gas /*+0x24*/ = 40; c->klass /*+5*/ = M->class;
do { col = rnd(6); } while (col == 5 && rnd(2) != 1);   c->colour /*+4*/ = col;            /* rnd = 1a21:6180 */
c->engine_link /*+0x22*/ = 1;           c->ignition /*+0x23*/ = rnd(9) - 5;                /* -5..+3 (SR1: rnd(13)-8) */
make = (specA & 0x0E) >> 1;             /* specA bits 1..3 are a one-hot mask 1 GM / 2 Ford / 4 Chrysler -> 0 / 1 / 2 */
#define PART(slot, idx, cat) { p = reuse ? slot : part_alloc(); slot = p; p->type = idx; p->value = PARTS[idx].price;      \
                               p->wear = INIT_WEAR[cat]; p->next = 0; }           /* PARTS = DS:4502, INIT_WEAR = DS:478C (8 words) */
PART(c->trans    /*+06*/, (((specA & 0xC0) >> 6) + 3) * 3 + make,        1);   /* 9..20: automatic (grade 0) .. racing 4-speed */
PART(c->engine   /*+08*/, ((specA >> 8) & 3) * 3 + make,               0);   /* 0..8 */
PART(c->tyres    /*+12*/, ((specA & 0x30) >> 4) + 0x32,                4);   /* 50..52 */
c->bolt[0x19..0x1B] = 0;                                                      /* carb bolts cleared (bytes +0x19..+0x1B) */
g = (specA >> 10) & 0x0F;                                                     /* manifold grade 0..8 */
idx = (g + 8) * 3 + make;  if (g == 8 && make == 4) idx--;                    /* make is never 4: dead code in the exe */
PART(c->manifold /*+0A*/, idx,                                         3);   /* 24..49 */
ctype = (specA >> 14) + 0x15;                                                 /* carburettor 21..23 */
n = CARBS_PER_MANIFOLD[g] - 1;                                                /* DS:4E6E words {1,1,3,2,2,1,2,0,0}; n = -1 for g 7, 8 */
for (k = 2; k > n; k--) c->carb[k] = 0;                                       /* carb[k] at +0x0C+2k */
for (k = n; k >= 0; k--) { PART(c->carb[k], ctype, 2);  c->bolt[0x19 + k] = 3; }   /* ONE bolt per carburettor (SR1: two) */
c->bolt[0x16..0x18] = 3;                                                      /* engine bolt + 2 manifold bolts */
c->trans_bolt[0..1] /*+0x14,+0x15*/ = 3;
c->flags /*word +0x26*/ = 0;
PART(c->exhaust_manifold /*+1C*/, (specB & 0x1F) * 3 + make + 0x35,    5);   /* 53..58 */
PART(c->muffler          /*+1E*/, ((specB & 0x3E0) >> 5) + 0x3B,       6);   /* 59..60 */
PART(c->differential     /*+20*/, (specB >> 10) + 0x3D,                7);   /* 61..63 */
if (!reuse) { if (cur_car /*AA02*/) { c->next /*+0x28*/ = other_cars; other_cars = c; } else cur_car = c; }   /* other_cars = AA04 */
return c;
```
Note: manifold `idx` for grade 8 / make 1 (Ford) would be 49 (a Chrysler supercharger) - no stock car uses grade 8; every
stock car is listed in `work/data/data_blocks.txt`. `car->value` stays the price paid; each part's `value` is the catalogue
price at the time (SR1 `PARTS[].price`). The `specA/specB` bit fields are those of FORMATS.md.

### 0001:46f0 part_release
Changed (SR1 `part_release` 0000:4386, §4.2): `int part_release(PART *p)` - no `keep` argument any more: `if (!p) return 0; v = p->value;
part_free(p); return v;`.

### 0001:4716 car_free
New (the SR1 `car_free` 0000:43bf, §4.2, without the `keep` argument): `int car_free(CAR *c)`: `c->value -= part_release(c->trans);`
then for the five slots `+0x08..+0x10` (engine, manifold, carb 0..2) `c->value -= part_release(slot)`; then
`part_release(c->tyres /*+12*/); part_release(c->exhaust_manifold /*+1C*/); part_release(c->muffler /*+1E*/);
part_release(c->differential /*+20*/);` (these four do not reduce `value`); `v = max(c->value, 0); car_release(c); return v;`. All parts
are freed (never moved to the spare list).

### 0001:478e how_about
Changed (SR1 `how_about` 0000:4940, §4.6):
```c
int how_about(uint *offer)
{   r = *offer % 5;  if (r) { *offer -= r; if (*offer == 0) *offer = 5; }
    p = strchr(MSG14_LINE /*pointer variable DS:1240 -> "How about $1234 ?"*/, '$');
    sprintf(p + 1, "%-d ?" /*DS:4C87*/, *offer);
    msg_draw_at(14, -1, -1);                       /* 1a21:274e */
    r = ui_menu(11);                               /* screen 11: -20 OK, -21 No */
    msg_close();                                   /* 1a21:27d2 */
    return r; }
```

### 0001:4804 car_flags
Changed (SR1 `car_flags` 0000:49a2, §4.17): `unsigned car_flags(CAR *c)`: bits from the high byte of `c->flags` (byte `c+0x27`): `0x20 -> 4`
(roof chopped), `0x40 -> 2` (rear bumper stripped), `0x80 -> 1` (front bumper stripped); `+ 0x20` when `c->muffler (+0x1E) == 0` (new);
`+ 8` when the **manifold** (`c+0x0A`) has catalogue grade 8 (byte `DS:4504 + 10*type`, the supercharger; SR1: engine grade 2).
The value 0x20 is also consumed by `car_draw` (see there).

### 0001:4852 your_cars
Changed (SR1 `your_cars` 0000:49ed, §4.6; the matcher left it `new`). `int your_cars(void)`, returns 0 or 3 (after Switch it). Uses the
0-based list conventions of the new toolkit (list texts `DS:A61C[k]`, `DS:B88E` = selected row, list box = `1a21:55ea`):
```c
int your_cars(void)
{   rounds = 0; sel = 0; saved = 0;
    for (;;) {
        n = 0; if (cur_car) { node[1] = cur_car; DS:A61C[0] = MODELS[cur_car->model].ad_ptr; n = 1; }   /* ad_ptr = DS:A80C+12+20*m, a char* */
        for (c = other_cars; c; c = c->next) { node[n + 1] = c; DS:A61C[n] = MODELS[c->model].ad_ptr; n++; }
        r = list_box(9, min(sel, n - 1), DS:A61C, /*wear*/0, n);            /* screen 9: -1 Switch it, -2 Sell it, -3 Forget it; 0x3EA = n == 0 */
        if (r == -3) return 0;
        if (r == 0x3EA) { msg_box(rounds == 0 ? 9 : 8); return 0; }        /* "no car, dummy" / "All cars gone" */
        sel = DS:B88E;
        if (r == -1) {                                                      /* Switch it */
            switch_new /*DS:4C56*/ = node[sel + 1];  switch_old /*DS:4C54*/ = cur_car;
            if (switch_new == cur_car) { switch_new = 0; return 3; }
            if ((cur_car != 0) == sel) other_cars = switch_new->next; else node[sel]->next = switch_new->next;
            if (cur_car) { cur_car->next = other_cars; other_cars = cur_car; }
            cur_car = switch_new; return 3; }
        /* Sell it */
        car = node[sel + 1];
        base = offer = (u32)car->value * (rnd(20) + 75) / 100;               /* 32-bit divide */
        strcpy(DS:2238 /*element 3 of screen 10*/, MODELS[car->model].ad_ptr);  DS:2266[0] = 0;
        saved = ui_save_bg(10, &x, &y, 1);  ui_push(10);                      /* screen 10: "I'll take $" field, -24 Offer, -22 Never mind */
        #define ASK() do { r = edit_number(&ask, DS:2266); if (r == -22) goto close; } while (ask == 0)
        ASK();
        if ((u16)(car->value * 115 / 100) < ask) { msg_box(10); goto close; }      /* "You must be kidding!" */
        if (car->value < ask) { msg_box(11); ASK(); if (car->value < ask) { msg_box(12); goto close; } }  /* No way! / Get lost! */
        if (base < ask) { offer = base * (rnd(5) + 85) / 100; if (offer < ask && how_about(&offer) == -20) ask = offer; }
        if (offer < ask) ASK();
        if (offer < ask) { offer = base * (rnd(5) + 91) / 100; if (offer < ask && how_about(&offer) == -20) ask = offer; }
        if (offer < ask) ASK();
        if (offer < ask * 9 / 10) { msg_box(13); goto close; }                  /* "No thanks!"  (integer ask*9/10; SR1 used a double) */
        msg_box(ask * 2 < car->value ? 15 : 16);                                 /* "You must be crazy. I'll take it!" / "I'll take it!" */
        game_clock += 0x888;  money_add(ask);
        if (sel == 0 && cur_car) {                                                /* the car in the garage */
            screen_put_bitmap_mirror(saved, x, y); arena_free_low(1); saved = 0;   /* 1a21:9b40(1) */
            cursor_ctl(-4);
            car_draw(6, cur_car->model, car_flags(cur_car), 0, 0x9E, &TYRE_PICS[tyre_grade(cur_car)]);     /* drives out */
            ui_pop(1); garage_name_clear(); ui_push(0); cursor_ctl(-2);
            car_free(cur_car); cur_car = 0;
        } else { if ((cur_car != 0) == sel) other_cars = car->next; else node[sel]->next = car->next;  car_free(car); }
      close:
        ui_pop(1);
        if (saved) { screen_put_bitmap_mirror(saved, x, y); arena_free_low(1); saved = 0; }
        rounds++;
    } }
```
Differences to SR1: messages by number (8 All cars gone, 9 no car, 10-13, 15, 16); the asking-price limit is `value*115/100` as `u16`
with the same 4-tier haggling; the "No thanks" test uses integer arithmetic; the car is drawn with the 6-argument `car_draw`; the list is
0-based; all parts are freed (see `car_free`). `edit_number` returns -22 for "Never mind" (Esc / N), -24 for "Offer" (O) or 0 for Enter;
the typed value is in `ask`.

### 0001:4d90 message_at
= SR1 `message_at` (0000:4ffa), §2 table (race results): `msg_draw_at(id, x, y); wait_ticks(DS:1F49); msg_close();` with the
wait deadline (`DS:1F38/1F3B`) saved and restored around the call. Message numbers instead of text offsets.

### 0001:51f8 opponent_init
Changed (SR1 `opponent_init` 0000:542e, §4.14). `void opponent_init(int i, int *pool)`; `o` = person record `DS:AB7E + 18*i` (SR2_DATA block 0:
`+0..+3` race parameters a,b,c,d, `+4` face, `+6` name ptr, `+8` model (u16), `+0A` colour, `+0D` car flags, `+0F`, `+10`):
```c
do { col = rnd(6); } while (col == 5 && rnd(2) != 1);
o->colour(+0A) = col;  o->b0B = o->b0C = o->flags(+0D) = o->b0E = 0;
do { k = rnd(pool[0]); } while (pool[k + 1] == -1);  o->model(+8) = pool[k + 1];  pool[k + 1] = -1;
lvl = (MODELS[model].class(+4) >> 1) + 1;
if (OVERLAY[model].rear_bumper(+6) > 0 && OVERLAY[model].front_bumper(+8) > 0) if (rnd(25) < lvl) o->flags |= 3;   /* OVERLAY = DS:BAAC, 10 bytes per model */
if (OVERLAY[model].roof(+2) > 0)                                                   if (rnd(30) < lvl) o->flags |= 4;
if (rnd(20) < lvl) o->flags |= 0x20;                                               /* "no muffler" bit of car_flags */
o->b0F(+0F)  = 7 - ((s8)o->b(+1) >> 1);        /* arithmetic shifts */
o->b10(+10)  = 6 - ((s8)o->a(+0) >> 2);
```
Removed vs SR1: the "big engine" flag 8, the random sticker and the zeroing of `+0B..+10` beyond `+0E`. The two last bytes are new (race
parameters derived from the person's `b` and `a`); `o->flags` bits: 1/2/4 bumpers/roof as `car_flags`, 0x20 no muffler.

### 0001:534c opponents_init
Changed (SR1 `opponents_init` 0000:5584, §4.14): classes scanned over the **21 models 0..20** (`DS:A810 + 20*m` byte, `m < 21`; model 21 is the
King's car): class `< 3` cheap, `3..5` mid, `>= 6` rich - each list is `{count, model...}`; then 7 opponents per list: persons 0..6 from the cheap
list, 7..13 mid, 14..20 rich (`opponent_init(i, list)`). No demo adjustments any more (no "Geek"); at the end `DS:AD03 = 0` (SR1 `king_ready`).
(Person 21 "The King" is set up by the race module.)

### 0001:5400 new_game
Changed (SR1 `new_game` 0000:56a9, §4.14):
```c
pools_init(1);
money /*i32 DS:A9FC*/ = demo_flag(DS:9730 /*copy of DS:B8D0*/) * 40000 + 1200;       /* SR1: 750, demo 10000 */
DS:AA01 = 1; DS:AA00 = 1;                                  /* two new bytes (race: Bob's / events available) */
race_render 0dcb:2866(1);  0dcb:24d8(100);                 /* SR1 track_build_course(100) and a new call, see race */
opponents_init();
game_clock /*DS:1F6A,1F6C*/ = 0;  clock_origin /*DS:1F72,1F74*/ = tick_clock /*DS:1F6E,1F70*/;
DS:479C..47A6 (6 words) = 0;                               /* SR1 race_wins[4] at 4970..4976: win counters, see race */
DS:9B40 = 0xFFFF;                                          /* classified ads: no week rolled yet */
DS:9B42 = 0xFF;  DS:9B86 = 42;  memset(DS:9B5C, 0, 42);   /* all 42 car ids available */
```

### 0001:54ba trim_trailing_spaces
New. `int trim_trailing_spaces(char *s)` (near): used on the player name. If `s` is non-null and the string is non-empty: starting at the last
character, while it is a blank replace it by NUL and count it, moving left until a NUL byte is met (for an all-blank buffer the scan stops
at the byte before the buffer); returns the number of blanks removed (0 for an empty or NULL string).

### 0001:550e new_game_prompt
Changed (SR1 `new_game_prompt` 0000:570c, §4.14): `int new_game_prompt(void)`:
```c
present_flag = 0; show_picture_at(0x3EA /*LIB2 #2*/, 0x10, 6, 0); cursor_ctl(1); present_flag = 1;
pal_shadow_patch(); page_copy_rect(back, front, 0, 0, 0x13F, 0xBD); (*DS:A0CE)(); pal_reg12_13(0x1DDA);
r = ui_edit(2, 15, DS:A9E8);              /* screen 2: name field (15 chars) + "Old Game" -2 + "OK" -3; DS:A9E8 = player name */
trim_trailing_spaces(DS:A9E8);
if (r == -3 || r == 0) { new_game(); return 0; }        /* OK button or Enter */
return r;                                  /* -2 = Old Game (load), anything else is passed on */
```
(SR1: `new_game(r)` was called with the result; the screen number 2 replaces SR1's inline picture 0x3F8.)

### 0001:63e0 day_index
= SR1 `day_index` (0000:6476), §4.15; DS remap: `game_clock` 05FC/05FE → **DS:1F6A** (u32), `tick_clock` 0600/0602 →
**DS:1F6E** (u32, advanced by the timer ISR), `clock_origin` 0604/0606 → **DS:1F72** (u32). Same formula:
`min(((1F6E - 1F72) + 1F6A) / 0x1998, 0x5B)`. One game day is still 0x1998 ticks, an hour 0x222.

### 0001:640e classified_week
New. `int classified_week(void)`: `w = day_index() / 7; return w > 11 ? 11 : w;` (signed division). The week number selects
the front-page headline photo (`DS:4C5A[w]`) and decides when the classified ads are re-rolled
(compared with `DS:9B40`, see `newspaper_front`, `classifieds`).

### 0001:6426 clock_hm
= SR1 `clock_hm` (0000:64a3), §4.15; same arithmetic (clock = `(1F6E-1F72)+1F6A`, capped at 0x91908; `h = r / 0x222`,
`m = (r % 0x222) * 60 / 0x222` with `r = clock mod 0x1998`). Clock remap as `day_index`. No longer called from
`calendar_show`.

### 0001:64a2 summer_over_check
= SR1 `summer_over_check` (0000:6529), §4.15; DS remap: `end_flags` 0282 → **DS:1BC6** (bit 0 = summer over; bit 1 is used
by game_loop for the hall of fame call `hall_of_fame_screen(1BC6 & 2)`).

### 0001:64b2 month_of_day
= SR1 `month_of_day` (0000:653c), §4.15; identical results (d < 15: June, dom = d+16; d < 46: July, d-14; d < 77: August,
d-45; d > 90: September 15; else September, d-76), only the compare order differs.

### 0001:650a calendar_show
= SR1 `calendar_show` (0000:65af), §4.15, with these changes: the clock / `clock_hm` call is gone; the weekday offset is
`(d + 22)` instead of `(d + 21)` (the calendar is now 1969: June 16 is a Monday); tables: `x = ((d+22) % 7) * 0x12 + DS:4D48`
(`DS:4D48` = 33), `row = (d+22)/7 - CAL_ROW0[m]` with `CAL_ROW0` = `DS:4D52` words {0, 5, 9, 14}, `y = row*7 + CAL_Y0[m]` with `CAL_Y0` =
`DS:4D4A` words {14, 107, 15, 105}; pictures: month sheet LIB2 ids `0x478` (1144; June/July, m < 2) or `0x47A` (1146;
August/September), day marker `0x479` (1145); text pointer for the non-quick status line `DS:4D5A`. The saved rectangle
(0x50,4) uses `arena_bitmap_alloc(w,h,0,2)` and is released with `arena_free_high(1)` (1a21:9b56) twice (SR1 `arena_pop(1)`).
`summer_over_check()` is called first; `if (!quick && end_flags) return` is unchanged (`end_flags` = DS:1BC6).

### 0001:66ae date_print
= SR1 `date_print` (0000:6750), §4.15. Differences: format `DS:4D76` = `"%s %d, 1969"` (was 1963); month names
`DS:4D82` (4 near pointers: June, July, August, September); colours from scheme record `DS:9DC6 + 0x41` (as before,
`_g_scheme` is now `DS:9DC6`); drawn with driver vector slot 3 (`DS:A0BA`, text) at x = 0x61, y = argument, on the back page.

### 0001:672c set_paint_palette
= SR1 `set_paint_palette` (0000:67cf), §4.8; DS remap: `PAINT` `DS:50C2` (2 bytes per colour) → **`DS:4D1E`, 3 bytes per
colour** `{ega_reg6, ega_reg7, strip_bg}`: {4,12,5}, {3,11,8}, {2,10,2}, {5,13,2}, {1,9,12}, {7,15,8}; `g_pal_visible` 8249 →
**DS:AD2F**; shadow bytes 0446/0447 → **DS:1DAC/1DAD** (and a second copy in DS:1DE0/1DE1); driver id `DS:AD1C` (-2 EGA, -6 VGA)
as before. The third byte is new: it is the background colour of the "See it" strip in the classifieds (see `classifieds`).

### 0001:6796 garage_hotspots_nocar
Changed (SR1 `garage_hotspots_nocar` 0000:68b3, §4.7): the garage screen (UI screen 0) is now an array of 0x12-byte
element records (see "UI toolkit"). Walk the records of `DS:3AD8[0]` (= `DS:3AD8` first word's array) until the record whose
flag byte has bit 1 (0x02, "last") set, inclusive; for every record whose word `+6` (return code) is in 9..13 clear bit 0
of the flag byte (`flags &= 0xFE`, element disabled). SR1 set a type 0x0E instead. Called when there is no `cur_car`.

### 0001:67b8 garage_hotspots_car
Changed (SR1 `garage_hotspots_car` 0000:68e8, §4.7): `void(int model, int x, int y)`. Same data (`MODEL_HOTSPOTS` SR1
`DS:70E4` → **`DS:9872`**, 22 models × 20 bytes = five `{dx, dy, w, h}` byte rectangles) and same code→rectangle map
(code 11 rect 0 rear bumper, 13 rect 1 roof, 9 rect 2 transmission, 10 rect 3 hood, 12 rect 4 front bumper), but the records are
found by scanning screen 0 (`DS:3AD8[0]`, until the "last" flag 0x02) instead of `hotspot_find`: for a record with `+6 == code`
set `+8 = dx + x`, `+0xA = dy + y`, `+0xC = +8 + w`, `+0xE = +0xA + h` (words) and `flags |= 1` (enable). Records of other
codes are untouched. `hotspot_find` / `garage_hotspots_register` no longer exist.

### 0001:68c0 garage_name_clear
= SR1 `garage_name_clear` (0000:6aa6), §4.7; DS remap: blank source `DS:5094` → **`DS:4D8A`** (32 spaces), `garage_title`
`DS:29FF` → **`DS:220A`**.

### 0001:68e2 broke_check
Changed (SR1 `broke_check` 0000:6ad8, §4.7). `int broke_check(void)`: returns 1 iff the player owns nothing (`cur_car` AA02 == 0,
`other_cars` AA04 == 0, `spares` AA0A == 0) **and** `cheapest_ad_price()` (0001:0e94, sign-extended to 32 bits) is greater than
`money` (i32 `DS:A9FC`; high words compared signed, low words unsigned). SR1 used the fixed limit `money < 400`.
With no ads in the list (`cheapest_ad_price` = 0) the result is 0.

### 0001:6912 garage_screen
Changed (SR1 `garage_screen` 0000:6b06, §4.7). `int garage_screen(int mode)`; modes: 0 only hot spots, 1 full redraw (new
car / after a trip from the shops), 2 redraw after a trip with the car "parked", 3 after "Switch it". Returns the UI code
or 0x29A (game over). Steps (DS remap below):
```c
if (DS:3B3B > 0) cursor_ctl(-4);                       /* 1a21:2d90; 3B3B = cursor show counter */
summer_over_check();
if (end_flags /*1BC6*/) { if (end_flags & 1) cursor_ctl(-2); return 0x29A; }
cursor_ctl(0);  status_print(-1, DS:4DAB /*"" clears the status line*/, 0);
DS:1BC8 = 1;                                            /* trip destination "garage" (SR1 drive_dest) */
if (mode > 0) {
  if (mode == 1 || mode == 2) goto redraw;
  if (mode == 3) {                                      /* Switch it */
     if (switch_new /*DS:4C56*/) {
        if (switch_old /*DS:4C54*/) {                  /* old car drives out */
           f = car_flags(switch_old); save = cur_car; cur_car = switch_old;       /* car_draw reads cur_car */
           car_draw(6, switch_old->model, f, 0, 0x9E, &TYRE_PICS[tyre_grade(switch_old)]);
           cur_car = save; garage_name_clear(); ui_push(0); ui_pop(1);
        }
        goto redraw;
     }
     cursor_ctl(-2); msg_box(17 /*Here it is!*/); cursor_ctl(-4); goto hotspots;
  }
}
goto hotspots;
redraw:                                                 /* labelled 6a57 */
  present_flag = 0;
  if (mode != 3) {                                      /* background: LIB2 #18 + two overlays */
     p = pic_get(0x3FA, 2); pic_draw_masked(p, 0x3FC, 0x90, 0xA3, 0); pic_draw_masked(p, 0x4AE, 0x50, 0xAC, 0);
     screen_put_bitmap_mirror(p, 0, 0); arena_free_high(1);       /* 1a21:9b56(1) */
     pal_reg12_13(0x1DDA);
     ega_set_palreg(12, cur_car ? PAINT[cur_car->colour][0] : 5);
  }
  present_flag = 1;
  if (!cur_car) { if (mode != 3) copy_back_to_front();          /* 1a21:2888 */  garage_title[0] = 0; }
  else {
     DS:AD6E = MODELS[model].picture (+16);  DS:AD70 = car->colour;  DS:AD72 = PARTS[car->tyres->type].grade;
     set_paint_palette(AD70);  DS:AD74 = 0;  DS:AD76 = car->model;
     DS:AD78 = (car->flags & 0xE000) | (DS:AD78 & 0x1FFF);       /* only the roof/bumper bits are copied now */
     for (i = 0; i < 5; i++) DS:AD84[i] = (&car->engine)[i]->type;   /* engine, manifold, carb 0..2 (empty carb slots read garbage) */
     f = car_flags(cur_car);  strcpy(DS:220A /*garage_title*/, MODELS[model].ad);
     if (mode == 2) m = 3; else { if (mode != 3) copy_back_to_front(); m = 4; }
     car_draw(m, DS:AD76, f, 0 /*driver*/, 0x9E /*y*/, &TYRE_PICS[DS:AD72]);   /* 3 parked, 4 drives in */
  }
hotspots:
  if (!cur_car) garage_hotspots_nocar(); else garage_hotspots_car(DS:AD76, DS:AD5C, DS:AD5E);   /* car_x / car_y */
  ui_sort_screen(0);                                 /* 1a21:4d7c (decompiled as list_box(0)): re-sort the garage hot spots by position */
  status_label(DS:4844);                               /* money on the status line (SR1 money_draw) */
  cursor_ctl(-2);
  if (cars_used >= 16) msg_box(18);                    /* "no room for all these cars, sell one" */
  else if (parts_used == 0xB5) msg_box(19);            /* "too much junk, sell a part" */
  else if (parts_used >  0xB5) {                       /* message 20, 2nd line "Must sell 12 parts ..." */
      q = line2_of_message_20 /*DS:1266*/; q += strcspn(q, "1234567890" /*DS:4DAC*/);
      itoa(parts_used - 0xB4, q, 10); q[strlen(q)] = ' '; msg_box(20); }
  if (broke_check()) { msg_box(21 /*You're outta dough*/); end_flags |= 0x10; }
  else { input_reset(); code = ui_menu(0); }           /* screen 0 = the garage hot spots */
  summer_over_check();
  if (end_flags & 0x10) end_flags = 0x10;
  if (end_flags) { code = 0x29A; if (!(end_flags & 1)) cursor_ctl(-4); }
  return code;
```
Differences to SR1 in words: (1) the garage background is composed from LIB2 #18 (`0x3FA`) with overlays `0x3FC` at (144,163)
and `0x4AE` at (80,172) instead of the full screen picture #32; palette entry 12 gets the car's paint register-6 value (SR1:
constant 5, `pal_reg12_5`); (2) mode 2 no longer promotes `other_cars` when there is no `cur_car`; (3) the flags word copied
into `DS:AD78` no longer contains the sticker field; `car_draw` has lost its sticker and smoke arguments (6 arguments now); (4) all limits: 16 cars, 181
(0xB5) parts, number in message 20 = `parts_used - 0xB4`; (5) message ids 17 (Here it is!), 18, 19, 20, 21; (6) no
`music_mute(0)` before waiting. DS remap: `cur_car` AA02, `other_cars` AA04, `end_flags` 1BC6, `garage_car_pic/colour/tyre/driver/
model/flags` (SR1 827A-8284) → AD6E, AD70, AD72, AD74, AD76, AD78; `bay_slot_pic[5]` 8290 → **AD84**; `car_x/car_y` 825E/8260 →
AD5C/AD5E; `switch_old/new` 4F46/4F48 → 4C54/4C56; `present_flag` 8240 → AD26; `location` (0284) is gone (replaced by `DS:1BC8`).

### 0001:6d5e paint_job
Changed (SR1 `paint_job` 0000:6f6b, §4.8). Flow:
```c
void paint_job(void)
{   if (!cur_car /*AA02*/) { msg_box(9 /*no car, dummy*/); return; }
    if (driver_id /*DS:AD1C*/ != -2 && driver_id != -6) { msg_box(97 /*need a better graphics card*/); return; }
    bg = ui_save_bg(13, &x, &y, 2);  ui_push(13);              /* screen 13: 1 Previous color, 2 Next color, 3 Go ahead, -23 Forget it */
    c = cur_car->colour;  i = c;
    for (;;) { ega_set_palreg(12, PAINT[i][0]);                /* PAINT = DS:4D1E, 3 bytes per colour */
               do b = ui_wait(3000); while (!b);
               if (b == 1) { if (--i < 0) i = 5; continue; }
               if (b == 2) { if (++i > 5) i = 0; continue; }
               break; }
    if (b == 3 && pay_or_complain(20, 25 /*Paint ain't free!*/)) {    /* 1a21:1e50: if money < cost msg_box(msg), return 0; else money_add(-cost), return 1 */
        sound_vec[2](1);                                       /* DS:BB94 far call, arg 1: start the spray effect */
        present_flag = 1; ui_pop(1);
        screen_put_bitmap_mirror(bg, x, y); arena_free_high(1);
        cur_car->colour = i; set_paint_palette(i);
        game_clock += 0x888;
        sound_vec[2](0);                                       /* stop it */
        return; }
    ui_pop(1);  ega_set_palreg(12, PAINT[cur_car->colour][0]); /* restore the sample colour (SR1: constant 5) */
    screen_put_bitmap_mirror(bg, x, y); arena_free_high(1);
}
```
Differences to SR1: money is charged by `pay_or_complain` (20 dollars, message 25) before the animation, instead of the `money >= 20`
test; the sticker is no longer removed; palette entry 12 is left at the new colour (SR1 reset it to 5); sound vector slot 2
(`DS:BB94`) is called with 1 / 0 around the redraw. Elapsed time unchanged (0x888 = 4 h).

### 0001:6edc customize
Changed (SR1 `customize` 0000:711a, §4.8). `void customize(int what)`: 4 roof, 2 rear bumper, 1 front bumper. Called only
when a car exists (the hot spots are disabled otherwise; the null test of SR1 is gone). Differences:
* overlay table: `MODEL_PICS` `DS:8DF0` → **`DS:BAAC`** (22 × 10 bytes `{u8 race, u8 ?, u16 roof, u16 hoodscoop, u16 rear_bumper,
  u16 front_bumper}`); roof test = word at `BAAC + 10*model + 2`, rear bumper `+6`, front bumper `+8` (0 = not available).
* messages: roof missing → `msg_box(31)`; bumper missing → `msg_box(32)`; no `money < cost` pre-test, but the charge is made
  with `pay_or_complain(cost, 33 /*Customization ain't free!*/)` after the dialog.
* flag bits live in the high byte of the word at `car+0x26` (byte `car+0x27`): roof `0x20`, rear bumper `0x40`, front bumper `0x80`
  (the same bit positions 0x2000 / 0x4000 / 0x8000 of the word). Toggling = XOR on that byte.
* dialog: title text pointer is stored into the first element of screen 15 (`DS:3AF6` → element 0, field `+2`): roof `DS:4E02`
  "Chop roof - $70" / `DS:4E04` "Rebuild roof - $70"; bumper `DS:4E06` "Strip bumper - $15" / `DS:4E08` "Restore bumper - $15"
  (these four words hold near pointers). Cost 70 / 15; time 0x2220 / 0x888.
```c
bg = ui_save_bg(15, &x, &y, 2); ui_push(15); do b = ui_wait(3000); while (!b); ui_pop(1);   /* 15: -20 OK, -22 Forget it */
screen_put_bitmap_mirror(bg, x, y); arena_free_high(1);
if (b == -20 && pay_or_complain(cost, 33)) {
    toggle bits; cursor_ctl(-4);
    car_draw(8, model, car_flags(cur_car), 0, 0x9E, &TYRE_PICS[tyre_grade(cur_car)]);
    sound_vec[9]();                                  /* DS:BBB0, the clank */
    cursor_ctl(-2); game_clock += time; }
```
(SR1 pushed the dialog and only popped it on Forget it; SR2 always pops and restores the background first.)

### 0001:70d2 spares_collect
Changed (SR1 `spares_collect` 0000:76a7, §4.2): new signature `void spares_collect(int cat, PART **nodes, char *wear, int n0, int *count)`.
Lists are now **0-based** (SR1 1-based with the count in `list_buf[0]`): for every spare `p` (list `AA0A`, link `+6`) whose
catalogue category (`byte DS:4506 + 10*p->type`) equals `cat`: `nodes[n0] = p; DS:A61C[n0] = word at DS:4508 + 10*type`
(catalogue text pointer, a near pointer to the NUL-terminated text, not a text id); `wear[n0] = (p->wear == -128) ? 0x80 : p->wear / 100`
(signed idiv); `n0++`. At the end `*count = n0`. The list text array is `DS:A61C` (SR1 `DS:49E0`, which also held the count).
`n0 > 0` is used by callers to reserve leading entries (e.g. "Remove current muffler").

### 0001:715a part_install
Changed (SR1 `part_install` 0000:7741, §4.2): `void part_install(PART *p, PART **slot)` with the **catalogue value no longer
adjusted by `part_value`**: if `*slot` is set it is pushed on the spare list (`AA0A`) and `cur_car->value -= (*slot)->value`; then
`*slot = p; p->next = 0; cur_car->value += p->value` (`cur_car` = `DS:AA02`). (`part_value` and the V-6 = 100 rule are gone.)

### 0001:7196 part_uninstall
Changed (SR1 `part_uninstall` 0000:7789, §4.2): `void part_uninstall(PART **slot)`: `(*slot)->next = spares; spares = *slot;
cur_car->value -= (*slot)->value; *slot = 0;` (no `part_value`).

### 0001:71be spare_unlink
= SR1 `spare_unlink` (0000:77b7), §4.2; DS remap `spares` 7EB8 → AA0A. Callers push the target slot before the part, so after the
call the slot is still on the stack for `part_install(part, slot)`: use the C form `part_install(spare_unlink(p), slot)`.

### 0001:71f6 change_tires
Changed (SR1 `change_tires` 0000:7800, §4.9):
```c
void change_tires(void)
{   car = cur_car; if (!car) { msg_box(9); return; }
    spares_collect(4 /*tires*/, node, wear, 0, &n);
    r = list_box(7, 0, DS:A61C, wear, n);              /* 1a21:55ea; screen 7: -3 Change, -4 Forget it */
    if (r == -4) return;
    if (r == -3) sel = DS:B88E;                         /* 0-based selection */
    else if (r == 0x3EA) { msg_box(30 /*no spare tires*/); return; }
    old_g = PARTS[car->tyres->type].grade; new_g = PARTS[node[sel]->type].grade;      /* catalogue byte +2 */
    part_install(spare_unlink(node[sel]), &car->tyres);
    tire_change_anim(&TYRE_PICS[old_g], &TYRE_PICS[new_g]);    /* TYRE_PICS = DS:4D30, 8 bytes per grade */
    game_clock += 0x444; }
```
`TYRE_PICS` `DS:5030` → **`DS:4D30`** (3 grades × 4 words: 1050-1053, 1014-1017, 1084-1087). The list uses screen 7
(SR1 screen 4), see "UI toolkit".

### 0001:72f2 car_info_sr2
New (replaces SR1 `car_info`/`car_info_list` 0000:79eb/78f1, §4.18; reached from garage code 16 "Car info"). `void car_info(void)`:
```c
if (!cur_car) { msg_box(9); return; }
present_flag = 0; cursor_ctl(-3);
bg = screen_save_rect(0x1D, 0x16, 0x106, 0x92);                      /* panel background */
colours = scheme+0x05;  fill_rect(0x20, 0x19, 0x100, 0x8C); draw_frame3(0x20, 0x19, 0x100, 0x8C);
text_at(0x20 + (0x100 - text_width(DS:4E68 /*"Car parts"*/)) / 2, 0x19, "Car parts");  hline(0x20, 0x22, 0x100);
parts[0..4] = cur_car->{engine, manifold, carb0, carb1, carb2} (+8..+0x10); parts[5] = trans (+6); [6] = tyres (+0x12);
parts[7] = exhaust manifold (+0x1C); [8] = muffler (+0x1E); [9] = differential (+0x20);
y = 0x24;
for (i = 0; i < 10; i++) if (parts[i]) {
    list_row(x = DS:4E54[i] + 0x20 /*indent: 0,5,5,5,5,0,0,0,0,0*/, y, CAT[type].text, right = 0x110,
             show_wear = (wear != -128), pct = wear / 100);     /* 1a21:4e56 */
    y += 10; }
v = car_runnable(cur_car, 1) ? car_max_speed(-1) : 0;                 /* 0001:c6f6 (race) */
sprintf(DS:4E6C /*"Max speed      mph"*/ + 9, " %3d mph", v);         /* DS:4E3E */
sprintf(DS:4E6A /*"Fuel                  "*/ + 5, " %2d gallons", cur_car->gas / 10);   /* DS:4E47 */
text_at(0x20, y + 10, DS:4E6A);  text_at(0x20, y + 20, DS:4E6C);
cursor_ctl(-1); page_copy_rect(back, front, 0x1D, 0x16, 0x123, 0xA8);
present_flag = saved; DS:1F3D = 1; ui_menu(6); DS:1F3D = 0;           /* any click / key closes */
screen_put_bitmap_mirror(bg, 0x1D, 0x16); arena_free_high(1);
```
`list_row(x, y, text, right, show_wear, pct)` = `1a21:4e56`: draws `text` at (x, y) with text vector slot 3; if `show_wear`, draws `wear_text(pct)` right
aligned so that it ends at `right` (x = right - 6*strlen(wear_text)). (Part list order is engine, manifold, carbs, trans, tyres, exhaust
manifold, muffler, differential - the scale of the panel allows 11 text rows.)

### 0001:75b8 change_transmission
Changed (SR1 `change_transmission` 0000:7aa4, §4.10). Same state machine (bolt states `st[1..2]`: 0 out, 1 tightening, 2 loosening;
`car->trans_bolt[i]` +0x14/+0x15; times 0x666 fit, 0x888 reject, 0x666 removal) with these changes:
* screen 12 (`ui_push(12)`: -2 "Select" (= Parts), -1 Done, bolt hot spots code 1 and 2) replaces SR1 screen 0x14; the two bolt hot spots are elements 2 and 3 of
  screen 12 (`DS:3AF0 + 0x24` and `+0x36`, flag byte): enabled (`|= 1`) while a transmission is fitted, disabled (`&= 0xFE`) otherwise. Fitting a part also
  enables them (`SCREEN_REC[200..201].type = 0xB` of SR1 is gone).
* `trans_pic` (DS:AD7A, SR1 `DS:8286`) = `TRANS_PICS[grade]` with `TRANS_PICS` = **`DS:483A`** = {1077, 1078, 1079, 1080} (grade 0..3 = automatic, 3-speed,
  4-speed, racing 4-speed; SR1 `DS:49D2`), `grade` = catalogue byte `DS:4504 + 10*type`. Backdrop picture `0x41F` (1055) at (0x20, 10) (SR1 0x42E).
* fit test `CAT[type].make_mask(+5) & MODELS[car->model].specA_low_byte & 0x0F` (same rule); list = `spares_collect(1 /*transmission*/, ...)` + `list_box(7, 0, ...)`
  (screen 7, -3 Change, -4 Forget it), selection `DS:B88E`; messages 22 (already has one: "Where will you put it, speedy?"), 23 (no spare transmissions), 24 (won't fit).
* sound: vector slot numbers (index k, `DS:BB8C + 4k`): slot 5 (`DS:BBA0`, arg 1/0) brackets every bolt animation, then slot 13 (`DS:BBC0`, chirp) after it; tighten/loosen
  without removal plays slot 17 (`DS:BBD0`, thud); the fit sequence uses slot 10 (`DS:BBB4`, arg 1/0 = wrench loop) around `trans_slide_anim(pic, 0)` and then slot 14
  (`DS:BBC4`, chirp low); a failed fit calls `trans_slide_anim(pic, 1)`, `msg_box(24)`, `trans_remove_anim(pic, 1)`, `game_clock += 0x888`. (Roles inferred from the call
  positions: the sound module owns the slot table.)
* after a successful fit `trans_bolt[0..1] = 1` are set and drawn (`trans_bolt_draw(1, i)` for i = 1, 0), the hot spots enabled, `click_clear()` and `kbd_hotspot_refresh()`
  (0001:1c5c, rebuilds the keyboard hot-spot map of the current screen) are called.
Removing: loosen both bolts to 0 (each loosening below 2 drops the bolt: `trans_bolt_remove_anim(i)`); when both are out `trans_remove_anim(pic, 0)`, `game_clock += 0x666`,
`trans_pic = 0`, `part_uninstall(&car->trans)`, bolt hot spots disabled. Time per bolt click is not charged.

### 0001:7a44 car_runnable
Changed (SR1 `car_runnable` 0000:7ee6, §4.12):
```c
int car_runnable(CAR *c, int full)
{   if (!c->engine || c->engine_link /*+0x22*/ <= 0 || !c->manifold) return 0;
    n = CARBS_PER_MANIFOLD[CAT[c->manifold->type].grade];            /* DS:4E6E; 0 for fuel injection / supercharger */
    for (k = 0; k < n; k++) if (!c->carb[k]) return 0;                /* +0x0C + 2k */
    for (i = n + 2; i >= 0; i--) if (c->bolt[0x16 + i] != 3) return 0;   /* engine bolt, 2 manifold bolts, one bolt per carburettor */
    if (full) { if (!c->trans || !c->tyres) return 0; if (c->trans_bolt[1] != 3 || c->trans_bolt[0] != 3) return 0; }
    return 1; }
```
(Exhaust manifold, muffler and differential play no role. SR1: two bolts per carburettor, bolts at `+0x16..+0x1E`.)

### 0001:7aec ignition_tune
New (SR1 `ignition_tune` 0000:7fb1, §4.13, completely redrawn; the module vote put it into sound). `void ignition_tune(CAR *car)` (near), called from
`engine_bay` button "Tune". `car->ignition` (+0x23) ranges **-5..+3** (SR1 -8..+4); 0 is best. Screen 30: -7 Retard, -10 Advance, -1 Done.
```c
present_flag = 0;
screen_fill_rect(0x1C, 4, 0xC0, 0xB5, 0); screen_fill_rect(0xF2, 0x1F, 0x27, 9, 5); screen_fill_rect(0xF2, 0x96, 0x27, 9, 5);
show_picture_at(0x420 /*LIB2 #56 engine front*/, 0x40, 0x21, -1);
with colour DS:AD42: hline(back, 0x96, 0x71, 3);
A = arena_bitmap_alloc(0x10, 0x1E, 0, 2); copy back rect {0x10,0x1E, sx 0x90, sy 0x5E} -> A;                   /* background under the strobe */
P1 = pic_get(0x424); B = arena_bitmap_alloc(0x60, 0x2D, 0, 2);                                                /* fan / belt strip sheet */
   copy back rect {0x20, 0x2D, 0x48, 0x63} -> B at 0; blit P1 (w 0x40) into B at x 0x20 (wheel frames); bw = 0x20 frame step
P2 = pic_get(0x434); C = arena_bitmap_alloc(0x30, 0x21, 0, 2);                                                /* second sprite sheet */
   copy back rect {0x10, 0x21, 0x60, 0x3C} -> C at 0; blit P2 into C at x 0x20 (16 px per frame, 3 frames)
present_flag = 1; page_copy_rect(back, front, 0x1C, 0, 0x127, 0xBC);
ui_push(30); music_stop(); DS:5300 = 2;
if (DS:0050 == 0 /*AdLib*/) { 2c57:0b2c(1); 2c57:0bd2(1); }  DS:1F7C = 2;           /* engine-sound mode on */
changed = 1; fan_x = 0 (B source x, steps +0x20, wraps after 0x40); belt_x = 0x20 (C source x, steps -0x10, wraps below 0 to 0x20)
do {
   if (DS:3BD5 && DS:3BD6 != DS:3BD5) (*DS:BBEC)();                                 /* sound resume vector */
   ign = car->ignition;
   if (DS:0050 == 1 /*speaker*/) { hi = (-0x10 - ign) * 4; out(0x42, 0); toggle = !toggle; if (toggle && ign != 0) hi = -0x10; out(0x42, hi); }
   else { adlib_note(0ba6)(((ign + 0x10) * 2) << 8, (ign == 0 ? 0 : -8) + 0x52);  adlib_note(0c5c)((ign * 3 + 0x20) << 8, same); }
   cursor_ctl(-3);
   if (changed) {                                                                     /* timing light flash pattern */
      blit(A -> back at 0x90,0x5E);
      for (k = 0; k < 5; k++) { a = DS:4F1C[(ign*5 + k)*2]; b = DS:4F1D[(ign*5 + k)*2];     /* signed bytes, table below */
          if (a >= 0) line(0x90 + a, (ign + 0x25) * 3 + k, 0x90 + b, same y, colour DS:AD40);   /* driver slot 12 (DS:A0DE) */ }
   }
   blit(B frame fan_x -> back (0x48,0x63)); blit(C frame belt_x -> back (0x60,0x3C));
   fan_x += 0x20; if (fan_x > 0x40) fan_x = 0;  belt_x -= 0x10; if (belt_x < 0) belt_x = 0x20;
   cursor_ctl(-1);
   page_copy_rect(back, front, 0x48, 0x63, 0x67, 0x8F); page_copy_rect(back, front, 0x60, 0x3C, 0x6F, 0x5C);
   if (changed) { page_copy_rect(back, front, 0x90, 0x5E, 0x9F, 0x7B); changed = 0; }
   b = ui_wait(4);
   if (b == -10) { if (++car->ignition > 3) car->ignition = 3; changed = 1; }
   else if (b == -7) { if (--car->ignition < -5) car->ignition = -5; changed = 1; }
} while (b != -1);
if (DS:0050 == 0) { 2c57:0b2c(0); 2c57:0bd2(0); }  DS:1F7C = 0;  if (DS:0050 == 1) (*DS:BBE8)();  DS:5300 = 1;  if (DS:5304) music_resume();
game_clock += 0x444;                                  /* fixed 2 hours (SR1: 3 x elapsed ticks, max 2 h) */
ui_pop(1); arena_free_high(5);
```
Flash table `DS:4F1C` (signed byte pairs `{a,b}`, 5 per ignition value, index `ign*5+k`, so the data starts at `DS:4EEA` for ign = -5): ign -5: (-1,0)(7,7)(5,8)(7,8)(9,9);
-4: (-1,0)(8,9)(6,9)(8,10)(10,10); -3: (10,10)(9,10)(7,10)(9,11)(11,11); -2, -1, 0: (11,11)(10,11)(8,11)(10,11)(11,11); 1: (11,11)(9,11)(7,10)(9,10)(10,10);
2: (10,10)(8,10)(6,9)(8,9)(-1,0); 3: (9,9)(7,8)(5,8)(7,7)(-1,0). A pair with `a < 0` draws nothing. The engine tone is the only sound; its exact
AdLib/speaker programming belongs to the sound module (`2c57:0b2c/0bd2/0ba6/0c5c`, PIT channel 2 reload bytes via port 0x42 for the speaker).

### 0001:80a8 engine_bay
Changed (SR1 `engine_bay` 0000:81ab, §4.11): the layer model is the same (engine, manifold, up to 3 carburettors; a layer can only be removed when the one above
it is gone) but bolts, pictures, tables and messages changed. `void engine_bay(CAR *car)` (any car; interactive only for `cur_car`).
Data (all DS offsets; see "Engine-bay tables" for the decoded values): car bolts are bytes `car+0x16..+0x1B` = **bolt b = 1..6** at `car+0x15+b`: b1 engine,
b2/b3 manifold, b4/b5/b6 carburettor 0/1/2 (SR1: 9 bolts). Runtime: bolt state `st[1..6]` (stack), bolt picture `DS:AD8E[b-1]` = tightness 0..3 (not a picture id), slot pictures
`DS:AD84[5]` (engine, manifold, carb0..2), connector picture `DS:AD9A` (0 none, 0x432 disconnected, 0x433 connected), bay context `DS:AD9C`, bolt side `DS:ADA0`.
```c
for (b = 1; b <= 6; b++) { v = car->bolt[0x15 + b]; st[b] = v == 0 ? 0 : (v == 1 ? 1 : 2); DS:AD8C[b] /*=AD8E[b-1]*/ = v; }
DS:AD9A = car->engine_link == 0 ? 0 : (car->engine_link < 0 ? 0x432 : 0x433);
if (car->engine) DS:47F2 = 0x42A - (CAT[car->engine->type].grade == 0);            /* manifold-grade-0 picture: 0x429 with a V-6, 0x42A with a V-8 */
for (s = 0; s < 5; s++) DS:AD84[s] = (&car->engine)[s] ? DS:47E0[CAT[type].grade + 9*s] : 0;     /* BAY_PICS, 5 rows x 9 grades */
DS:4EB4 = DS:4EB6 = (DS:AD84[0] == 0x425) ? 4 : 5;                                  /* BAY_LAYOUT[0][0..1]: V-6 / V-8 layout code */
layers = number of non-zero DS:AD84[0..4];
if (!car->manifold) max_layers = 5;
else { g = CAT[car->manifold->type].grade; max_layers = CARBS_PER_MANIFOLD[g] + 2;
       for (k = 3; k < 6; k++) T1[k+1] /* DS:4E8C[4..6] */ = BAY_LAYOUT[g*3 + k - 3];    /* layout code of carburettor k-3 */ }
cursor_ctl(-4); bay_draw_car(car);                    /* rebuild the bay bitmap */
if (cur_car != car) return;                           /* somebody else's engine (race opponent_car_view): view only; returns with the cursor still hidden, no hot spots/ui_push */
bay_draw_parts(1, 0); cursor_ctl(-2); bay_hotspots(&DS:4E8E, &DS:4E9C); ui_push(14);       /* screen 14 */
for (;;) {
    do b = ui_wait(3000); while (!b);
    if (b == -13) {                                   /* "Wires": engine connector */
        car->engine_link = -car->engine_link;
        if (car->engine_link == 0) continue;
        game_clock += 0x6D;
        if (car->engine_link > 0) { DS:AD9A = 0x433; bay_connector_draw(); bay_hotspots(); continue; }
        DS:AD9A = 0x432; bay_connector_draw(); DS:AD9C = 0; bay_hotspots();  goto check_removal;       /* may drop the engine */
    }
    if (b == -3) {                                    /* Tune */
        if (!car_runnable(car, 0)) msg_box(26 /*Can't tune that mess, klutz!*/);
        else { ignition_tune(car); cursor_ctl(-4); bay_draw_parts(1, 0); cursor_ctl(-2); }
        continue; }
    if (b == -2) { fit_next_layer(); continue; }      /* "Parts", below */
    if (b == -1) { ui_pop(1); DS:AD1A = DS:AD18 = 0; arena_release_levels(); return; }
    /* bolt b (1..6): the layer is the first L whose cumulative BOLTS_PER_LAYER (DS:4E80 = {1,2,1,1,1}) reaches b */
    L = 0; sum = 0; while (L < 5 && (sum += BOLTS_PER_LAYER[L]) < b) L++;
    if (!DS:AD84[L]) continue;
    game_clock += 0x36;
    ctx = DS:AD9C = T1[b]; side = DS:ADA0 = T2[b];    /* T1 = DS:4E8C (bolt -> context 0 engine, 1 manifold, >= 4 carburettor position), T2 = DS:4E9A (side 0/1) */
    if (st[b] == 2) {                                 /* loosen */
        if (--car->bolt[0x15 + b] >= 2) { DS:AD8C[b] = car->bolt[0x15+b]; bay_bolt_draw(DS:AD8C[b]); sound_vec[17](); goto check_removal; }
        car->bolt[0x15+b] = 0; st[b] = 0; DS:AD8C[b] = 0;
        sound_vec[5](1); bay_bolt_remove_anim(); sound_vec[5](0); sound_vec[13]();
      check_removal:
        do {
            slot = -1; cascade = 0;
            if (DS:AD9C == 0) {                       /* engine bolt (or the connector): the engine leaves when it is the last layer, unplugged and bolt 1 is out */
                if (layers == 1 && car->engine_link == -1 && st[1] == 0) { car->engine_link = 0; slot = 0; DS:AD9A = 0; game_clock += 0x666; }
            } else if (DS:AD9C == 1) {                /* manifold bolts: layers == 2 and both bolts out */
                if (layers == 2 && st[2] == 0 && st[3] == 0) { slot = 1; cascade = 1; game_clock += 0x333; }
            } else if (st[b] == 0) {                  /* a carburettor bolt (b = 4..6) is out: that carburettor drops */
                slot = b - 2; if (layers == 3) cascade = 2; game_clock += 0x199;
            }
            if (slot >= 0) {
                layers--; part_uninstall(&(&car->engine)[slot]); cursor_ctl(-4); DS:AD84[slot] = 0;
                if (cascade) DS:AD9C = cascade - 1;                  /* re-test the layer below */
                bay_draw_car(car); bay_draw_parts(1, 1); bay_hotspots();
                car->ignition = rnd(9) - 5; cursor_ctl(-2); }
        } while (cascade);
    } else if (st[b] == 1) {                          /* tighten */
        if (car->bolt[0x15+b] == 3) st[b] = 2;
        else { DS:AD8C[b] = ++car->bolt[0x15+b]; bay_bolt_draw(DS:AD8C[b]); sound_vec[17](); }
    } else {                                          /* insert a bolt */
        sound_vec[5](1); DS:AD8C[b] = 1; bay_bolt_draw(1); sound_vec[5](0); sound_vec[13]();
        st[b] = 1; car->bolt[0x15+b] = 1; }
}
```
`fit_next_layer` (the "Parts" branch):
```c
cat = LAYER_CAT[layers];                                          /* DS:4EA8 = {0 engine, 3 manifold, 2, 2, 2 carburettor} */
spares_collect(cat, nodes, wear, 0, &n);
for (;;) {
    r = list_box(7, 0, DS:A61C, wear, n);                          /* screen 7 */
    if (r == -4) break;                                             /* Forget it */
    if (r == -3) sel = DS:B88E;
    else if (r == 0x3EA) { if (cat == 0) msg_box(27); if (cat == 3) msg_box(28); if (cat == 2) msg_box(29); break; }   /* no spare engine / manifold / carburator */
    else break;
    if (layers >= max_layers) { msg_box(22 /*Where will you put it, speedy?*/); break; }
    t = nodes[sel]->type; mk = CAT[t].make_mask(+5) & MODELS[car->model].specA_lowbyte;  slot = layers; extra = 0;
    if (layers == 0) {                                              /* engine */
        fits = (mk & 0xF) != 0;  DS:AD9C = 0; first_bolt = 0;  car->engine_link = -1;  DS:AD9A = 0x432;
        game_clock += fits ? 0x666 : 0x888;
    } else if (layers == 1) {                                       /* manifold */
        g = CAT[t].grade;
        fits = (mk & 0xF) && DS:47A8[g + 9*CAT[car->engine->type].grade] != 0;        /* MANIFOLD_FITS, bytes [engine grade][manifold grade] */
        DS:AD9C = 1; first_bolt = 1; max_layers = CARBS_PER_MANIFOLD[g] + 2; T1[4..6] = BAY_LAYOUT[g*3 + 0..2];
        game_clock += fits ? 0x333 : 0x444;
    } else if (layers >= 2 && layers <= 4) {                        /* carburettor */
        mg = CAT[car->manifold->type].grade; cg = CAT[t].grade;
        fits = (mk & 0xF) && DS:47C4[cg + 3*mg] != 0;                                  /* CARB_FITS, bytes [manifold grade][carb grade] */
        game_clock += fits ? 0x199 : 0x222;
        first_bolt = 3; slot = 2; DS:AD9C = BAY_LAYOUT[mg*3];                           /* DS:4EB4 */
        switch (DS:AD9C) {
        case 7:  if (car->carb[0]) { if (!car->carb[1]) { DS:AD9C = 8; first_bolt = 4; slot = 3; } else { DS:AD9C = 9; first_bolt = 5; slot = 4; } } break;
        case 10: case 12: case 15: if (car->carb[0]) { DS:AD9C++; first_bolt = 4; slot = 3; } break;
        }                                                           /* other layout codes (4,5,6,14): one carburettor only */
    }
    DS:AD84[slot] = DS:47E0[CAT[t].grade + 9*slot];                                   /* picture of the new part */
    if (!fits) {
        sound_vec[10](1); bay_part_in_anim(DS:AD84[slot], 1); sound_vec[10](0); sound_vec[14]();
        msg_box(24 /*That won't fit!*/);
        sound_vec[10](1); bay_part_out_anim(); sound_vec[10](0);
        DS:AD84[slot] = 0; if (slot == 0) { car->engine_link = 0; DS:AD9A = 0; }
        continue; }                                                 /* the part stays a spare; the same list is shown again */
    part_install(spare_unlink(nodes[sel]), &(&car->engine)[slot]);
    cursor_ctl(-4); sound_vec[10](1); bay_part_in_anim(DS:AD84[slot], 0); sound_vec[10](0); sound_vec[14]();
    if (slot == 0) {
        if (DS:AD84[0] == 0x425) { DS:47F2 = 0x429; DS:4EB4 = 4; } else { DS:47F2 = 0x42A; DS:4EB4 = 5; }
        DS:AD9A = 0x432; DS:4EB6 = DS:4EB4; bay_connector_draw(); }
    for (k = BOLTS_PER_LAYER[layers] - 1; k >= 0; k--) {            /* fresh, loose bolts */
        i = first_bolt + k; DS:ADA0 = k;                            /* side = k */
        sound_vec[5](1); DS:AD8E[i] = 1; bay_bolt_draw(1); sound_vec[5](0); sound_vec[13]();
        st[i + 1] = 1; car->bolt[0x16 + i] = 1; }
    bay_hotspots(); cursor_ctl(-2); layers++; car->ignition = rnd(9) - 5;
    break;                                                           /* back to the bay (SR1 stayed in the list) */
}
```
Notes: (1) a carburettor on a manifold that takes 2 or 3 carburettors goes into the first free carburettor slot at the next layout code; (2) unlike SR1 the
manifold of an opponent's car is drawn with the same code (view only); (3) fitting messages: 22, 24, 26, 27, 28, 29. Making a car run again after an engine
swap needs engine bolt 3, connector clicked (-1 → +1), manifold bolts 3, carburettor bolts 3.

### 1a21:1954 text_width
New. `int text_width(const char *s)` (far): sum of the glyph advance of every character: `w += *(u8*)(DS:0006 + 9*((s8)c - 32))` (the font is a table of 9-byte glyph records at `DS:0006`,
the first byte of a record is the advance; the exe addresses it as `c*9 - 0x11A`). Used for centring (the SR1 equivalent was inline `font_string_width` 0f38:...).

### 1a21:2018 dialog_msg
Changed (SR1 `FUN_0f38_239c`): `void dialog_msg(const char *text)`: `strcpy(DS:AB04, text); ui_dialog(27);` - screen 27 = a 204x32 box with the text line (element 2, text pointer
`DS:AB04`) and an "OK" button (-20).

### 1a21:204c fatal_message
Changed (SR1 `FUN_0f38_23ce`): `void fatal_message(const char *text)`: the error/"hit return" box. `saved = DS:AD26; demo = DS:9730; DS:9730 = 0;` copies the text to `DS:AA12`. If the display is in text mode
(`DS:AD2D <= 0`): prints `DS:1E2C`, the text and `"Hit Return when ready ..."` (DOS calls) and waits for a key (`getch`, or `key_read` polling when `DS:1F4E` is set). Otherwise (graphics): clear the screen if
`DS:7904`; if the cursor-shape/page state `DS:AD2F` is 0 set it to 1 and call driver slot 8; if `DS:AD30`, shift the y coordinates (+10, +14) of the elements of screen `DS:3B1A` (=screen 33) by -0x4B;
`DS:AD26 = 1; input_reset();` draw on the front page: `ui_dialog(33)` (screen 33: box, "OK" -20, text `DS:AA12`, "Hit Return when ready ..." `DS:231A`), undo the shift, restore `DS:AD2F`, `DS:AD26 = saved`;
finally `DS:9730 = demo`. Not part of the garage screens proper; it is the generic error box.

### 1a21:2530 msg_draw
Changed (SR1 `FUN_0f38_28a4`, garage.md §5.8): message boxes are now selected by **message number** `id` (table `DS:11EA`, 6 bytes per message: `{u16 box, u16 line1, u16 line2}`, line pointers are near pointers
to NUL-terminated texts; message texts: see "Messages"). Box records (`DS:1E46`, 18 bytes): `{u16 pic, u16 nlines, u16 x, u16 y, u16 w, u16 h, u16 text_dx, u16 text_dy, u16 extra}`: box 0 =
{1057, 1, 72, 30, 152, 37, 0, 6, 144}, box 1 = {1058, 1, 80, 30, 232, 37, 0, 6, 223}, box 2 = {1059, 2, 80, 20, 232, 46, 0, 6, 223}.
```c
void msg_draw(int id)       /* id < 0: the box stays on screen (not restorable) */
{   if (DS:B7AE) { save present (DS:AD26) and the page pointers DS:AD10/12/14/16; present = 1; draw on the visible page DS:AD0C/0E (EGA double buffering: DS:ADA2/A4 when driver id == -2) }
    cursor_ctl(-3);
    if (DS:1E7C:1E7E /*saved background of an open box*/) { screen_put_bitmap(saved, box.x, box.y); arena_free_high(1); saved = 0; }
    neg = id < 0; id = abs(id);
    box = &DS:1E46[DS:11EA[id].box]; DS:881A = box;
    saved = screen_save_rect(box.x, box.y, box.w, box.h, 0, 0);      /* -> DS:1E7C:1E7E */
    pic_draw_masked(back, box.pic, box.x & 0xFF, box.y & 0xFF, 0);
    colours = scheme(DS:9DC6) + 0x46 (fg word, bg word, style byte);
    y = box.y + box.text_dy;
    for (i = 0; i < box.nlines; i++) { line = DS:11EC[3*id + i];          /* consecutive words from line1 */
        text_at(back, (box.w - text_width(line)) / 2 + box.x + box.text_dx, y, line); y += 9; }
    cursor_ctl(-1);
    if (present) page_copy_rect(back, front, box.x, box.y, box.x + box.w - 1, box.y + box.h - 1);
    if (neg) { arena_free_high(1); saved = 0; }
    DS:1F4C = 1;                                                       /* a message box is up */
    restore the pointers saved above; }
```
Messages with two lines use `line1`, `line2`; the line pointers of a 1-line message are followed by the next message's `box` word, so the box's `nlines` must match.

### 1a21:274e msg_draw_at
Changed (SR1 `FUN_0f38_2a62`): `void msg_draw_at(int id, int x, int y)`: like `msg_draw` but temporarily overrides the box's `x` (if `x >= 0`) and `y` (if `y >= 0`) in `DS:1E46`; restores them; `DS:1F4C = 0`.

### 1a21:27d2 msg_close
New. `void msg_close(void)` (SR1 `0f38:2adf`): if a saved background exists (`DS:1E7C:1E7E`) `screen_put_bitmap_mirror(saved, box.x, box.y)`; `arena_free_high(1)`; clear it; `DS:1F4C = 0`. With `DS:B7AE` the page pointers are swapped as in `msg_draw`.
(`msg_box(id)` = `msg_draw(id); ui_push(6); wait_ticks(DS:1F49); msg_close(); ui_pop(1);` with the wait deadline saved - game_flow.)

### 1a21:2888 present_back_page
New. `void present_back_page(void)`: `if (driver == -2) { ega_show_page(1); driver_slot8(); }` then `page_copy_rect(back, front, 0, 0, 0x13F, 0xBD)`; `if (driver == -2) { ega_show_page(0); return; } driver_slot8();`
(slot 8 = `DS:A0CE`). Used by `garage_screen` to show the garage backdrop at once.

### 1a21:2d90 cursor_ctl
Changed (SR1 `0f38:2d7e` "mouse_cursor"): `void cursor_ctl(int a)`; `x = clamp(DS:3B32, 0, 0x13F)`, `y = clamp(DS:3B34, 0, 199)` (pointer position).
`a >= 0`: shape number: `DS:A2D6 = a; DS:3B4A/3B4C = hot spot words at DS:200C + 4a / DS:200E + 4a; DS:A2D2 = cursor_shape_offset(a); cursor_redraw(x, y)`.
`a == -3` or `-4` (hide): `if (--DS:3B3B >= 0) { DS:A2D8 = -((a == -3) + 1); if (DS:A2DE >= 0) { x = DS:A2DA; y = DS:A2DC; } cursor_redraw(x, y); DS:A2D8 = 0; }`.
`a == -1` or `-2` (show): `if (++DS:3B3B > 0) { DS:A2D8 = (a == -1) + 1; if (DS:A2DE >= 0) { x = DS:A2DA; y = DS:A2DC; } cursor_redraw(x, y); DS:A2D8 = 1; }`.
(`DS:3B3B` is the signed show counter; -3 / -1 are the page-aware variants used between drawing and `page_copy_rect`, -4 / -2 the plain ones.)

### 1a21:2e92 trans_bolt_draw
= SR1 `trans_bolt_draw` (0f38:3074, ../StreetRod/srport/src/game/garage_bay.c), garage.md §2; DS remap / constants: bolt picture LIB2 `0x44F` → **`0x439`** (1081);
bolt y table `DS:06FE` → **`DS:201A`** (words {78, 93}, `y = table[bolt] + 10`); tightness frame = `tight << 4` (16x6 frames, unchanged); the "new bolt
slides in" call uses driver slot 11 (`DS:A0DA`) with two extra zero arguments (mask far pointer = none); `_g_mirror` → `DS:AD26`, `_g_back` → `DS:AD14/16`,
`_g_front` → `DS:AD10/12`, `arena_pop(1)` → `arena_free_high(1)` (1a21:9b56).

### 1a21:2fc4 trans_bolt_remove_anim
= SR1 `trans_bolt_remove_anim` (0f38:31a5); same remap as `trans_bolt_draw` (picture `0x439`, y table `DS:201A`); the thud `fx_thud()` is sound vector slot 17 (`DS:BBD0`),
the falling-bolt slide uses driver slot 11 (`DS:A0DA`, 2 extra zero args).

### 1a21:30f2 trans_slide_anim
= SR1 `trans_slide_anim` (0f38:32ce) except the reject offset: `x = 200 - w` (accepted) but **`184 - w`** when `reject != 0` (SR1: `200 - w - 0x18 = 176 - w`); `y = 0x7D - h`;
the save width/x use `x - 8`, `w + 8` as before; driver slide = slot 11 (`DS:A0DA`, 2 extra zero args), masked blit = slot 2 (`DS:A0B6`).

### 1a21:322e trans_remove_anim
Changed (SR1 `trans_remove_anim` 0f38:3404): the transmission panel picture is `0x41F` (1055) at (0x20, 10); sound vector slot 9 (`DS:BBB0`, the clank) is played first; arguments unused.

### 1a21:328c trans_screen_draw
= SR1 `trans_screen_draw` (0f38:345b) with a third (unused) argument; `DS:06FC` → **`DS:2018`** (animation flag: 0 while composing, 1 after); rectangle copied to the front page
unchanged (0x20,10)-(0xE7,0xB8).

### 1a21:332e bay_engine_idx
Changed (SR1 `FUN_0f38_34fb`): engine picture `DS:AD84` (bay slot 0): `0x425 → 0`, `0x426 → 1`, `0x427 → 2`, any other value `v` returns `v - 0x427` (an empty slot gives
garbage, never used). (SR1 pictures 0x434-0x436.)

### 1a21:334a bay_carb_pos
Changed (SR1 `FUN_0f38_351c`): `int bay_carb_pos(int *x, int *y)`: `slot = DS:208A[ctx]` (byte table indexed by `ctx = DS:AD9C`, values for ctx 4..16: 2,2,2,2,3,4,2,3,2,3,2,2,3);
`pic = DS:AD84[slot]`; `idx = pic == 0x42D ? 0 : pic == 0x42E ? 1 : pic == 0x42F ? 2 : (uninitialised)` (returned); `*x = word DS:204E + 4*(ctx-4)`, `*y = word DS:2050 + 4*(ctx-4)`.
Carburettor positions (ctx: x,y): 4: 80,71; 5: 87,66; 6: 80,66; 7: 56,71; 8: 80,71; 9: 104,71; 10: 64,66; 11: 104,66; 12: 64,67; 13: 104,67; 14: 88,49; 15: 64,49; 16: 104,49.
(SR1 returned 0/2/4 for pictures 0x43C-0x43E; positions at `DS:0742`, slot table `DS:077E`.)

### 1a21:33aa bay_part_pos
Changed (SR1 `FUN_0f38_3589`): ctx 0 (engine): `idx = bay_engine_idx()`, `(x,y) = words DS:201E + 4*idx, DS:2020 + 4*idx` = (25,168), (26,171), (14,178); ctx 1 (manifold): `(0x38, 0x5B)`
(SR1 `(0x38, 0x4C)`); else `bay_carb_pos`. Coordinates are bay-bitmap coordinates (the bitmap is shown at screen (0x20, 4)).

### 1a21:33fe bay_bolt_pos
Changed (SR1 `FUN_0f38_35e4`): ctx 0: `idx = bay_engine_idx()`, `x = word DS:202A + 4*idx`, `y = word DS:202C + 4*idx` = (147,144), (166,144), (174,149).
ctx 1 (manifold, `DS:ADA0` = side 0/1): side 0 → `x = 0x4B`; side 1 → `x = 0x84` if the manifold picture `DS:AD86` is `0x429` or `0x428` else `0x9A`; if `DS:AD86 == 0x4B8` then `x += 1`;
`y = 0x5A`. Otherwise (carburettor, ctx >= 4): `idx = bay_carb_pos(&x,&y)`; `x += word DS:2082 + 4*idx; y += word DS:2084 + 4*idx` with idx offsets (20,-5), (32,-3), (30,-3) (no side term:
one bolt per carburettor).

### 1a21:34a2 bay_bolt_draw
Changed (SR1 `FUN_0f38_3697`; same job, different construction; arguments taken from the disassembly). `void bay_bolt_draw(int tight)`: draws bolt state `tight` (1 loose, 2, 3 tight) at `bay_bolt_pos()`. The bolt is no longer a separate picture per tightness: all states are cut from one
sprite, LIB2 `0x4A2` (1186), by the 20-byte rectangle records at **`DS:2092 + 20*tight`** (rect fields `w, h, sx, sy, dx, dy`; tight 1 = `DS:20A6` `{8, 12, 0, 0, 0, 0}`, tight 2 = `DS:20BA` `{8, 8, 0, 0, 0, 4}`, tight 3 = `DS:20CE` `{8, 4, 0, 0, 0, 8}`: the bolt sinks into its hole).
```c
bay_bolt_pos(&x, &y);  cursor_ctl(-4);
P = pic_get(0x4A2, 2);
T = arena_bitmap_alloc(8, 12, 1, 2);  bitmap_clear(T) /*1a21:b09e*/;
slot0_blit(P -> T, &DS:2092[tight], flags 3);                                          /* the bolt image in its 8x12 cell */
y -= 12;                                                                               /* top of the cell */
slot1_blit_shifted(bay bitmap DS:AD18/1A -> back page, {w 8, h 12, sx x-8, sy y-4, dx x+0x18, dy y});     /* restore the background under the cell */
x += 0x18;  xa = ~DS:AD24 & x;                                                         /* pixel remainder for the video alignment */
animate = (tight == 1 && present);
if (animate) { T2 = arena_bitmap_alloc(8, 12, 1, 2); DS:20AA += 8; slot0_blit(P -> T2, &DS:20A6, 3); DS:20AA -= 8;      /* 2nd sprite frame: sx + 8 */
               if (xa) { T3 = arena_bitmap_alloc(0x10, 12, 1, 2); bitmap_make_shifted(T2, xa, T3); } }                 /* results unused by the slide call below */
if (xa) bitmap_make_shifted(T, xa, P /*scratch*/), spr = P; else spr = T;                  /* 1a21:a758(src, shift, dst) */
x &= DS:AD24;
if (animate) slot11_slide(top 4, bottom 0xB9, x, from -8, to y, spr, spr);              /* a new bolt drops in from above (-8 .. y) */
else         slot11_slide(top 4, bottom 0xB9, x, from y, to y, spr, none);              /* drawn in place */
cursor_ctl(-2);  arena_free_high(2);  if (animate) arena_free_high(xa ? 2 : 1);
```
(`present` = `DS:AD26`; `bay_bolt_pos` coordinates are bay-bitmap coordinates, the cell is drawn at `(x+0x18, y)` on the page.)

### 1a21:369a bay_bolt_remove_anim
Changed (SR1 `FUN_0f38_37fc`; disassembly checked). `void bay_bolt_remove_anim(void)`: `bay_bolt_pos(&x, &y); cursor_ctl(-4); P = pic_get(0x4A2, 2);` `T1 = arena_bitmap_alloc(8,12,1,2)` filled from `P` with the rect `DS:20A6` (flags 3); `T2 = arena_bitmap_alloc(8,12,1,2)` filled from `P` with the same rect and `DS:20AA += 8`
(second sprite frame; restored after); `sx = x - 8; x += 0x18; y -= T1.h;` restore the cell with `slot1_blit_shifted(bay bitmap -> back, {w T1.w, h T1.h, sx, y-4, dx x, dy y})`; if `present`: `page_copy_rect(back, front, x, y, x+w-1, y+h-1)`, sound slot 17 (`DS:BBD0`, thud),
`x &= DS:AD24`, `slot11_slide(top 4, bottom 0xB8, x, from y+3, to 0xB9, T1, T2)` - the loosened bolt falls out of the bay; `cursor_ctl(-2); arena_free_high(3)`.

### 1a21:381e bay_part_in_anim
Changed (SR1 `FUN_0f38_3a58`). `void bay_part_in_anim(int pic, int reject)`: `(x,y) = bay_part_pos()`; `if (pic == 0x4B8) x -= 1`; `if (DS:2018) cursor_ctl(-4)`; `P = pic_get(pic, 2)`; `h = P.h`; `y -= h`
(**all** parts are bottom-aligned now, SR1 excluded the engine pictures); `sh = x & ~DS:AD24`; if `sh` a shifted copy `Q` (width +8, masked if `P` has a mask) else `Q = P`; `x &= DS:AD24`;
`if (!reject) blit_masked(Q -> bay bitmap DS:AD18, {Q.w, h, 0, 0, x-8, y-4}, flags 1)` (the part is composed into the bay bitmap) `else y -= 0x14` (a rejected part stops 20 px above its seat and is not composed);
`if (DS:2018)`: `from = present ? 4 - h : y; slide_sprite(top 4, bottom 0xB9, x + 0x18, from, y, Q)` (slot 11) and `cursor_ctl(-2)`; free the arena blocks. The special `bay_carb_overlay`
of SR1 (loose manifold bolt drawn over a carburettor) no longer exists.

### 1a21:39a4 bay_part_out_anim
Changed (SR1 `FUN_0f38_3c19`): `void bay_part_out_anim(void)`: `(x,y) = bay_part_pos(); y -= 0x14;` `cursor_ctl(-3)`; restores the strip of the bay bitmap above the rejected part into the back page:
blit `DS:AD18` with `{w = bay.w - (x-8), h = y-3, sx x-8, sy 0, dx x+0x18, dy 4}` (slot 0, flags 1), `cursor_ctl(-1)`, and with present copies that rectangle to the front page. (No arguments.)

### 1a21:3a5a bay_connector_draw
Changed (SR1 `FUN_0f38_3d1c`): `idx = bay_engine_idx(); (cx, cy) = (word DS:2036 + 4*idx, word DS:2038 + 4*idx)` = (150,175), (168,176), (174,184); erase the old connector by copying
`{w 0x25, h 0x17, sx cx-0x2C, sy cy-0x1A, dx cx-0x0C, dy cy-0x16}` from the bay bitmap (`DS:AD18`, slot 1 shifted blit); then draw the connector picture `DS:AD9A` masked at
(`cx-9`, `cy-0x16`) when it is 0x432 (disconnected) or (`cx-0xC`, `cy-0x14`) when 0x433 (connected); with present copy `(cx-0xC, cy-0x16)-(cx+0x18, cy)` to the front page.
(One position pair per engine; SR1 had two per engine and erased the other one.)

### 1a21:3b34 bay_draw_car
Changed (SR1 `FUN_0f38_3e35`). `void bay_draw_car(CAR *car)`: rebuilds the bay bitmap: `ctx = DS:AD9C` saved; `DS:2018 = 0`; if a bitmap exists (`DS:AD18/AD1A`) `arena_release_levels()` (1a21:9b26); `DS:AD18 = arena_bitmap_alloc(0xB8, 0xB5, 0, 1)` (184 x 181),
cleared (`1a21:b09e`) - the background picture is no longer decoded into it (the backdrop is `0x41E`, drawn by `bay_draw_parts`). Then bottom-up, using `bay_part_in_anim(pic, 0)` (no animation, `DS:2018 == 0`):
engine (`DS:AD84`, ctx 0); if present `DS:209C[0] = (AD84 == 0x425) ? 4 : 5`; manifold (`DS:AD86`, ctx 1); then `ctx = DS:209C[grade of the manifold]` (bytes {5(4/5),6,7,10,12,14,15,0,0}, the first carburettor
context per manifold grade) and for the three carburettor pictures `DS:AD88/AD8A/AD8C` in order: `part_in_anim(pic, 0)` if non-zero, `ctx++`. Finally `DS:2018 = 1`, `DS:AD9C` restored.

### 1a21:3c14 bay_draw_parts
Changed (SR1 `FUN_0f38_3f2d`). `void bay_draw_parts(int own, int clank)`: `present` temporarily 0; `show_picture_at(0x41E, 0x18, 0, -1)` (bay backdrop), `screen_put_bitmap(bay bitmap, 0x20, 4)`; for `i = 0..5`: if `DS:AD8E[i]` (bolt tightness) != 0:
`DS:AD9C = DS:4E8E[i]` (bolt context), `DS:ADA0 = DS:4E9C[i]` (side), `bay_bolt_draw(DS:AD8E[i])`; if `DS:AD9A` (connector) `bay_connector_draw()`; if `own` `show_picture_at(0x47B, 0xE0, 0, -1)` (tool bar at the right) and the copied
width is 0x110 instead of 200; if `clank` play sound vector slot 9 (`DS:BBB0`); restore `present`; if set: `page_copy_rect(back, front, 0x18, 0, width + 0x17, 0xBC)`; `DS:AD9C/ADA0` restored.

### 1a21:3d10 bay_hotspots
New (SR1 `bay_draw_bolts` 0f38:40ca). `void bay_hotspots(int *ctx_tbl, int *side_tbl)`: rebuilds the hot spots of screen 14 for the bolts that exist and the connector. Elements of screen 14 (`DS:3AD8[14]`):
buttons (negative codes: -13 Wires, -3 Tune, -1 Done, -2 Parts) and nine bolt elements (codes 1..9). For `e = 0..5` (bolt `e+1`, walking the layers with `DS:4E80 = {1,2,1,1,1,1}` bolts each): if the layer's slot picture
(`DS:AD84[layer]`) is set: `DS:AD9C = ctx_tbl[e]; DS:ADA0 = side_tbl[e]; bay_bolt_pos(&x, &y);` take the next element whose code is >= 0 (skipping negative-code buttons, remembering the `-13` one),
set `code = e + 1`, rect `{x0 = x + 0x13, y0 = y - 0x0D, x1 = x + 0x1D, y1 = y + 2}`, `flags = 1`, hot-key byte `+0x10 = e + 2` (scancode of the digit key); then all later bolt elements get `flags &= 0xFE`.
The Wires element is disabled when there is no engine, else its rect is derived from the connector position `(cx, cy)` (`DS:2036/2038 + 4*engine_idx`) with `(a, c) = (0x21, 0x16)` for picture 0x432 or `(0x24, 0x14)`
for 0x433: `x0 = cx - a + 0x0E`, `x1 = cx - a + 0x22`, `y0 = cy - c - 7`, `y1 = cy - c + 8`, enabled. Finally `ui_sort_screen(14)` (1a21:4d7c) is called and `DS:AD9C/ADA0` restored.

### 1a21:4284 ui_bounds
= SR1 `ui_bounds` (0f38:4c1a) with the new element array: walks the elements of screen `DS:3AD8[screen]` (0x12 bytes each, until a flag byte with bit 1) accumulating
`min x0 (+8)`, `min y0 (+0xA)`, `max x1 (+0xC)`, `max y1 (+0xE)` over **all** elements (disabled ones included) in `DS:8824/8826/8828/882A`; results: `*x = max(minx-3, 0); *y = max(miny-3, 0);
w = min(maxx+3, 0x140) - *x; h = min(maxy+3, 200) - *y`.

### 1a21:434a ui_save_bg
Changed (SR1 `ui_save_bg` 0f38:4d10): `FarPtr ui_save_bg(int screen, int *x, int *y, int kind)`: `ui_bounds`; the saved rectangle is now byte-aligned: `x0 = *x & DS:AD24` (alignment mask), `right = (*x + w + 7) & DS:AD24` clamped
to 0x140, `bottom = min(*y + h, 200)`; `bmp = arena_bitmap_alloc(right - x0, bottom - *y, 0, kind)` (kind 1 = low arena freed with `arena_free_low`, 2 = high arena freed with `arena_free_high`);
`screen_save_rect(x0, *y, right - x0, bottom - *y, bmp)`; returns `bmp` (`*x` is left unaligned, callers pass it back to `screen_put_bitmap_mirror`).

### 1a21:4798 ui_redraw_element
New. `void ui_redraw_element(UIEL *el)`: `if (present) cursor_ctl(-3); draw_element(el) /*1a21:44a8*/; if (present) { cursor_ctl(-1); page_copy_rect(back, front, el->x0 - 3, el->y0 - 3, el->x1 + 6, el->y1 + 6); }`.

### 1a21:4b96 ui_push
Changed (SR1 `hotspots_push` 0f38:4a66; **UI screens are element arrays now**). `void ui_push(int screen)`:
```c
saved_present = DS:AD26; cursor_ctl(-3);
DS:B874[DS:B87C++] = screen;                              /* screen stack (SR1 limited to 4 levels; no limit check now) */
DS:B884 = DS:B882 = DS:B880 = DS:B87E = 0xFFFF;           /* hover rectangle: none */
DS:B894 = els = DS:3AD8[screen];                          /* near pointer to the first 18-byte element */
DS:B888 = DS:B890 = DS:B892 = DS:B88A = 0;                /* first list row, edit/title field, edit-mode flag, row count (set by draw_element) */
DS:B886 = els[0].flags & 0x20;                            /* 0x20 on the first element: list area is filled before the rows are drawn */
DS:8826 = DS:8824 = 1000; DS:882A = DS:8828 = -1;         /* dirty bounds */
DS:AD26 = 0; draw_screen(els) /*1a21:473e: draw_element for every element until the 'last' flag, accumulating bounds*/;
restore colours DS:AD28/2A/2C; cursor_ctl(-1); DS:AD26 = saved_present;
if (saved_present && DS:8828 >= 0) page_copy_rect(back, front, DS:8824 - 3, DS:8826 - 3, DS:8828 + 6, DS:882A + 6);
kbd_hotspot_refresh() /*0001:1c5c: rebuilds the keyboard map DS:3B6E from the hot-key bytes (+0x10/+0x11) of the enabled elements*/; click_clear();
if (DS:9730 /*demo*/ && screen != 4) demo_input_hook(0x1000);   /* 1a21:4806 */
```
Element record (18 bytes, little endian): `+0 u16 flags` (bit0 active hot spot, bit1 last element of the screen, bit2/3 box, 0x10 button, 0x20 first element: fill list area, 0x40 frame box, 0x80 static text,
0x100 centred text, 0x200 edit field, 0x400 title/selection field, 0x800 list row), `+2 u16 text ptr` (0 = none), `+4 i16 colour-scheme index` (scheme record = `DS:9DC6 + 5*idx`: fg word, bg word, frame byte),
`+6 i16 code` (value returned by `ui_wait` when clicked; negative = button, positive = hot spot / row), `+8,+0A,+0C,+0E` x0, y0, x1, y1, `+10 u16` keyboard hot keys (low byte scancode 1, high byte scancode 2, 0xFF = none).
The screen table is `DS:3AD8[36]` (near pointers); SR1 used a global record table with `next` links and `DS:2BAD[screen]`.

### 1a21:4cac ui_pop
Changed (SR1 `hotspots_pop` 0f38:4ba1): `void ui_pop(int all)`: `kbd_remap_restore(0); depth = DS:B87C; DS:B894 = 0; DS:B87C--;` `if (DS:B87C != 0 && all != 0) { DS:B87C = depth - 2; ui_push(DS:B874[DS:B87C]); return; }`
(i.e. with `all != 0` the screen below is pushed again so that it is redrawn/active); else hover rectangle `DS:B87E..B884 = 0xFFFF`. (`ui_pop(1)` is what the garage code calls; `all == 0` is never used here.)

### 1a21:4d7c ui_sort_screen
Changed (SR1: the matcher paired it with `list_box` 0f38:4f4f, which is actually `1a21:55ea`; this function replaces SR1 `garage_hotspots_register` 0f38:4265). `void ui_sort_screen(int screen)`: sorts the elements of `DS:3AD8[screen]` by (x0, y0) ascending
(selection sort: for each anchor element, every later element with smaller `x0`, or equal `x0` and smaller `y0`, is swapped with it - the whole 18-byte records are swapped); the "last" flag (bit 1 of the flag byte)
is kept on the final position (`anchor.flags &= ~2; last.flags |= 2` when the swap carried it). Finally `kbd_hotspot_refresh()` (0001:1c5c). Used after the garage/bay hot spots were moved so that keyboard navigation follows screen order.

### 1a21:4e0c wear_text
= SR1 `wear_text` (0f38:4fc1), §4.19: `pct == 0` returns `DS:2174` ("(`new`)"); otherwise builds "(NN% worn)" in place: `DS:216B = ' '`, `itoa(pct, DS:2169, 10)`, then the NUL after the digits is replaced by `'%'`; returns `DS:2168`
(template "(xx% worn)"). A 3-digit value yields "(100%worn)" (no blank), as in SR1.

### 1a21:4e56 list_row
New (part of SR1's `list_box_run`). `void list_row(int x, int y, const char *text, int right, int show_wear, int pct)`: `text_at(back, x, y, text)` (driver slot 3); if `show_wear`: `w = wear_text(pct); text_at(back, right - 6*strlen(w), y, w)`.

### 1a21:4ebc list_box_run
Changed (SR1 `list_box_run` 0f38:500e, §4.19). `int list_box_run(int screen, int sel, const char **texts, const char *wear, int n)` (far; `texts` = `DS:A61C`). Returns `0x3EA` if `n == 0`, else the (negative) button code that ended it
(-3 / -4 / ... all codes other than -7..-10 and positive row hits). Uses screens 7, 9, 16, 26 (any screen whose rows are elements with flag 0x800 and an optional title field 0x400):
```c
ui_push(screen);   rows = DS:B88A; DS:B88C = texts; DS:B88E = sel;                  /* DS:B88E = selected index (0-based): the caller reads it after the call */
top = 0; if (rows < n) { top = sel; if (n < sel + rows) top = n - rows; }
field = DS:B890;  if (field) { strcpy(field.text_buf /*the word at field+2*/, texts[sel]); if (wear && wear[sel] != 0x80) { strcat(buf, DS:217C /*"   "*/); strcat(buf, wear_text(wear[sel])); }  ui_redraw_element(field); }
row geometry: x0,y0,x1,y1 of the first row element DS:B888; row_h = y1 - y0 (= 8); colours = scheme of the row element (+4).
redraw: cursor_ctl(-3); if (DS:B886) fill_rect(list area);                           /* list backgrounds for screens with flag 0x20 */
        for (i = 0; i < min(rows, n - top); i++) list_row(row[i].x0, row[i].y0, texts[top + i], row[i].x1, wear && wear[top+i] != 0x80, wear[top+i]);
        if (!DS:B886) draw_box1(row[sel - top] frame);                                /* selection frame when there is no title field */
        cursor_ctl(-1); if (present) page_copy_rect(list area);
loop: r = ui_wait(1);
   r == 0: hover - the row under the pointer (DS:B898 > 0, within the visible rows) is redrawn in the highlight colours (bg/fg swapped via DS:AD28/AD29), the previously hovered row is redrawn normal;
   r > 0 (row r of the screen): if (top + r <= n) { sel = top + r - 1; DS:B88E = sel; update the title field text as above; (if !DS:B886 the old/new selection frames are redrawn); continue }  /* a click only selects */
   r == -7: if (top) top--;  r == -8: top = max(top - rows, 0);  r == -9: top += rows; if (top >= n) top = n - 1;  r == -10: if (top + 1 < n) top++;      /* then redraw */
   else: ui_pop(1); return r;
```
Differences to SR1: one call per screen, no `extra` string table (texts are near pointers, SR1 text ids), no negative-id indirection, no -6 edit code; 0-based indices; the "selected" text is mirrored in the title field element when
the screen has one; the first element flag 0x20 selects "fill background, no selection frame".

### 1a21:55ea list_box
Changed (SR1 `list_box` 0f38:4f4f, §4.19; the symbol table calls this one `FUN_1a21_55ea`). `int list_box(int screen, int sel, const char **texts, const char *wear, int n)`:
`if (n == 0) return 0x3EA; bg = ui_save_bg(screen, &x, &y, 2); r = list_box_run(screen, sel, texts, wear, n); screen_put_bitmap_mirror(bg, x, y); arena_free_high(1); return r;`

### 1a21:565c slot_list_edit
New (module vote: garage; it is the save-game picker, called by `save_game_screen` 0001:5d82 with screen 28). `int slot_list_edit(int screen, int sel, const char **texts, int n)`: list of up to 15 rows whose element codes are **negative** (-1..-15 = slot 1..15), plus a text
field below (element with flag 0x200/0x400, buffer `field+2`). `ui_push(screen); DS:B88C = texts; DS:B88E = sel`; draws the rows (`list_row`), the selection frame on row `sel`, and starts an edit session on the field
(`edit_begin(field.x0, field.y0, field.buf, (field.x1 - field.x0)/6)`, 1a21:5c52). Loop `r = ui_wait(1)`: `r == 0`: hover highlight like `list_box_run` (the hovered row has `DS:B898 = -code`), caret blink (`edit_blink()` every 35 idle
passes), typed characters (`DS:1F3F > 0`) go to `edit_key()`; `r > 0` is not produced; `r < 0`: if `-r <= n` it is a row: new selection `sel = -r - 1`, `DS:B88E = sel`, selection frames redrawn, the slot's text copied into the field buffer
and the edit session restarted (`edit_begin`); otherwise (a button: -23 "Save Game", -24 "Forget it") `edit_end()`, `ui_pop(1)`, return `r`.

### 1a21:5c52 edit_begin
New (SR1 `0f38:5910` text editor, split into five functions). `void edit_begin(int x, int y, char *buf, int maxlen)`: `DS:882E = DS:AD2B; DS:AD2B = DS:AD28` (caret colour); `cursor_ctl(-3)`; stores `DS:8838 = x, 883A = y, 8836 = buf, 8832 = maxlen`;
draws `buf` at (x, y) (slot 3); caret position `DS:883C = DS:AD32` (x after the text), `DS:883E = DS:AD34`; draws the caret `vline(883C, 883E, 8)`, `DS:882C = 1` (caret visible); `cursor_ctl(-1)`; copies `(x, y, x+6*maxlen+1, y+8)` to the front page; `click_clear(); kbd_remap_restore(1);
kbd_text_mode_on(DS:2182 /*numeric only*/); DS:8840 = DS:1F3D; DS:1F3D = 1; DS:1F3F = 0 /*last key*/; DS:8835 = 0; DS:8830 = strlen(buf)` (current length).

### 1a21:5d5e edit_key
New. `void edit_key(void)`: handles the key in `DS:1F3F` (ASCII): accepted are Backspace (8) and 0x20..0x5A (upper case and punctuation only; a leading space is ignored: `DS:8830 == 0 && key == 0x20` → return); in numeric mode (`DS:2182`)
only '0'..'9' are accepted (else sound vector slot 15, `DS:BBBC`, and return). Erases the caret (`vline` in the background colour); Backspace: if the length > 0 removes the last character (`buf[--len] = 0`), refills the glyph cell
with the background (`fill_rect(x, y, width of the glyph, 8)`) and moves `DS:883C` left; other keys: if `len < maxlen` append, draw the character at the caret (slot 3), `DS:883C = DS:AD32`; if full or nothing to erase: beep (slot 15).
Redraws the caret (`vline` foreground colour, `DS:882C = 1`), copies the affected cells to the front page, `DS:1F3F = 0`.

### 1a21:5f16 edit_blink
New. `void edit_blink(void)`: toggles the caret: `DS:882C ^= 1`; draws `vline(DS:883C, DS:883E, 8)` in the caret colour (visible) or the background colour; copies the 1x8 strip to the front page.

### 1a21:5f90 edit_end
New. `void edit_end(void)`: `if (DS:882C) edit_blink();` (hide the caret) `DS:AD2B = DS:882E; click_clear(); DS:1F3D = DS:8840; kbd_text_mode_off(); kbd_hotspot_refresh();`.

### 1a21:5fb8 edit_run
New (SR1 `0f38:5910`). `int edit_run(int x, int y, char *buf, int maxlen)`: `edit_begin(...)`; loop: `r = ui_wait(0x23)`; `r == 0` (35-tick timeout): `edit_blink()`; `r == -22` ("Forget it"/Esc) **or** (numeric mode and r >= 0 and key == 'N' 0x4E):
`r = -22`, finish; `r < 0` (button) or key == Enter (13) or (numeric mode and key == 'O' 0x4F): in numeric mode with an empty buffer: beep (slot 15) and continue; else (`'O'` → `r = -24` "Offer") `edit_end(); return r` (Enter gives `r = 0`);
any other key (`DS:1F3F != 0`) → `edit_key()`.

### 1a21:6056 edit_number
= SR1 `edit_number` (0f38:5da7), §2 table; remap: `DS:8BA2` → `DS:B890` (the title/edit element), scheme `_g_scheme` → `DS:9DC6`, numeric flag `DS:09F2` → `DS:2182`; the field width is `(x1 - x0)/6` capped at 10; calls `edit_run`; `*v = atoi(buf)`
(`34b0:1688`); returns `r > 0 ? 0 : r` (0 = Enter, -22 never mind, -24 offer).

### 1a21:60d2 ui_edit_number
= SR1 `FUN_0f38_5e1e`: `int ui_edit_number(int screen, int *v)`: `ui_push(screen); buf[0] = 0; r = edit_number(v, buf); ui_pop(1); return r;`.

### 1a21:8268 pump_overlay
Changed (SR1 `pump_overlay` 0f38:872e). Gas-station only (location `DS:1BC8 == 3`), called from `car_draw` mode 4. `void pump_overlay(int x, int y0, int w, int h, FarPtr dst, int idx)`:
`d = x - (DS:AD5C & 0xFFF8)` (pump x relative to the car's aligned x); first the part of the "view" bitmap `DS:AD58/AD5A` (the car/pump picture saved by the last `car_draw`) that overlaps the pump is copied
into `dst` (masked blit slot 2) with rect `{w = min(w + d, w), h = ..., sx = max(d, 0), sy = 0, dx = max(-d, 0), dy = (DS:AD5E - y0)}`; then the pump slice `{w = DS:50F8[idx], h = 0x3A, sx = DS:5104[idx], 0, 0, 0}` of the pump sheet
`DS:B89A/B89C` (picture 0x3E8 = LIB2 #1000, loaded by the gas-station event) is blitted masked onto `dst`. Tables: `DS:50F8` = {8, 8, 16, 8, 8, 16} widths, `DS:5104` = {0, 8, 24, 40, 48, 56} source x, `DS:50F2` = {56, 176}
pump x, `DS:50FA` = {8, 16} pump widths. (SR1 decoded two pictures 0x4C8/0x449 into a 16x70 temp; SR2 slices one sheet; `idx` 1..3 = left pump frames, 4..6 = the same frames for the second bitmap.)

### 1a21:831c car_strip_setup
Changed (SR1 `car_strip_setup` 0f38:84be). `void car_strip_setup(FarPtr strip, FarPtr *out_bgsrc, int smoke_pic, FarPtr *out_bg, int top, int h, u16 *out_ov)` - builds the background strips of a driving car from the strip bitmap `DS:7906`
(a descriptor over VRAM page 2 rows `top-h..top`): copy the 18-byte descriptor (arena level 1), `planes += (top - h) * stride`, `height = h`, `size = h * DS:AD1E * stride`; `*out_bgsrc` = new bitmap
`(strip.w, h, masked = smoke_pic != 0)` filled by `bitmap_copy_into` (Tandy: width `DS:7914` [= strip width when 0], blit with rect `{DS:7914, h, DS:7912, 0, 0, 0}`); `*out_bg` = Tandy: 0; EGA: the shared descriptor itself;
others: a new bitmap `(w, h)` which is filled with the strip only for locations 3 and 6 (`DS:1BC8`); `*out_ov = 0`; with a smoke picture its mask rows are merged into the bgsrc mask and `*out_ov = 0x790A` (one overlay
rect `DS:790A` with `DS:7910 = bgsrc.h - 1`; SR1 had two rects at 5A10/5A18). `car_draw` now always passes `smoke_pic = 0`; only `carhop_anim` passes a picture (0x458).

### 1a21:8546 car_compose
Changed (SR1 `car_compose` 0f38:8866). `void car_compose(FarPtr *frames, int model, int flags, int driver, int *tyre_pics, int smoke, int n)` - **no sticker argument**. Tables: overlays `P = DS:BAAC + 10*model` (SR1 8DF0), layout `L = DS:9BF4 + 12*model`
(SR1 7682; same field meaning: `L0/L1` wheel x, `L2/L3` driver x/y, `L4` roof x, `L5/L6` scoop x/y, `L7` rear-bumper y, `L8/L9` front-bumper x/y, `L10/L11` **gas-cap point**). Steps:
`frames[0] = pic_get(MODELS[model].picture (+16), 1)`; `DS:AD62 = L10; DS:AD64 = L11` (SR1: the points table 818C and sticker position vars are gone); flag 2: `pic_blit_at(f0, P+6, 0, L7)`; flag 1: `pic_blit_at(f0, P+8, L8, L9)`;
flag 4: `pic_blit_at(f0, P+2, L4, 0)` and `roof` = first opaque row of the roof mask (`DS:9858`, row = byte index / stride); flag 8 and `model != 0x15`: `pic_draw_masked(f0, P+4, L5, L6, 1)`; (flag 0x20 is not used here). `driver != 0`:
picture `DS:AD74` composited behind at `(L2, L3 - h + roof + 1)`. Frames 1..n-1 are allocated `(f0.w + (driver==Tandy ? 0 : 8), f0.h, mask, level 1)`; filling: Tandy: copies of f0 (descriptor words +10/+12 copied); EGA: `frame[i] = shifted(frame[i-1], 2)`;
others: `frame[1] = shifted(f0, 2)`, `frame[2..3]` = copies of `frame[0..1]`. Wheels (`tyre_pics != 0`): for every frame `i`, `pic_get(tyre_pics[i], 2)`, `dy = frame.h - pic.h`, `sh = Tandy ? 0 : (EGA ? 2*i : 2*(i & 1))`
(Tandy also sets `DS:79E2 = i < n-1`), composite behind at `(L0 + sh, dy)` and `(L1 + sh, dy)`. `smoke != 0`: picture `0x472` (LIB2 #1138; SR1 0x49D) composited under the left and right opaque extents of f0 (same placement rule
`sh`). `tyre_pics` is a pointer to 4 picture ids (`TYRE_PICS[grade]`, `DS:4D30`).

### 1a21:89fa car_draw
Changed (SR1 `car_draw` 0f38:8e48, §4.17). `void car_draw(int mode, int model, unsigned flags, int driver, int y, int *tyre_pics)` - the sticker and smoke arguments are gone (`driver` = draw the driver; `y` = ground line, 0x9E in the garage).
Modes as in SR1 (1 garage→street, 2 leaves the gas station, 3 parked static, 4 drives in, 6 drives out, 7 static "See it", 8 static redraw after customising). Location tables are indexed by `LOC = DS:1BC8` (0..6:
1 garage, 2 Bob's street, 3 gas station, 5 newspaper, 6 Bob's drive-in): rows `DS:1BD0` = {0,20,26,25,0,0,20}, `YOFF` `DS:1BDE` = {0,1,8,5,0,0,14}, colour (EGA/VGA) `DS:1BFA` = {0,1,1,1,0,3,1}, colour (others) `DS:1BEC` = 0.
```c
ytop = y - 0x45; cursor_ctl(-4);
for (;;) {
   stat = mode == 3 || mode == 7 || mode == 8;  nfr = stat ? 1 : 4;
   pic_info(MODELS[model].picture, &w, &h, &masked);  DS:1C08 = YOFF = DS:1BDE[LOC];  colour = ega_or_vga ? DS:1BFA[LOC] : DS:1BEC[LOC];
   rows = stat ? h - YOFF : DS:1BD0[LOC];
   x = ((0x140 - w) / 2 + 4) & 0xFFF8;  if (LOC == 1 && ((0x120 - w) & 0xFFF8) < x) x = (0x120 - w) & 0xFFF8;      /* NEW: in the garage the car is moved left */
   xend = (mode == 2) ? 0x140 - x : x + w;
   if (DS:7916 != 2) {                                       /* DS:7916: Tandy/CGA two-pass state (SR1 DS:5A24) */
      if (mode == 4) { DS:4C58 = 1; strip bitmap DS:7906 over VRAM page 2 (EGA: bitmap_init + ega_latch_copy of rows ytop.. from the back page; others: bitmap_init + screen_save_rect(0, ytop, 0x140, ...)) }
      if (mode == 4) { if (LOC == 3) arena_release_levels(); else arena_reset_all(); if (LOC == 2 || LOC == 6) town_bg_save(); } else arena_release_levels();
      car_compose(frames, model, flags, driver, tyre_pics, 1, nfr);
      if (Tandy && !stat) { 1a21:80ec(frames, nfr, YOFF, rows, colour); arena_free_low(3); }
      if (!stat) { DS:7914/7912 = (DS:7916 == 1) ? (0x140, 0) : (xend, mode == 2 ? x : 0);  car_strip_setup(DS:7906, &bgsrc, 0, &bg, 0x46, h, &ov); }
   }
   DS:AD5C = x;  DS:AD5E = y - frames[0].h + 1;  DS:AD60 = frames[0].h - YOFF;  DS:AD62 += x;  DS:AD64 += DS:AD5E;      /* car position and gas-cap point (SR1 825E.., 826E..) */
   ax_x = (mode == 1 || mode == 4) ? x - xend : x;
   if (DS:7916 != 2) {  EGA: ega_vram_cache_strips(DS:AA3C, frames, nfr, YOFF, rows, bg_ptr, colour);  static modes 3/7/8: blit the frame to the back (3) or front (7,8) page,
                        mode 3 then present_back_page() (1a21:2888), modes 7/8 page_copy_rect(front -> back); goto cleanup; }
   anim = sprite_scene_setup(ax_x, y, frames, DS:AA3C, nfr, bg, bgsrc, ov, xend);  anim_script(anim, mode, xend);
   if (mode == 6) copy 8 bytes DS:95DE.. into the anim's dirty rect (+4);
   if (mode is 4, 1, 2 or 6) {
      DS:A808 = 0;  DS:B8D4 = (flags & 0x20) == 0;           /* NEW: 1 = the car has a muffler (engine sound variant) */
      DS:1BCA = 1;                                            /* animation running */
      if (!car_runnable(cur_car, 1)) sound_vec[10](1) /*DS:BBB4*/; else DS:A808 = 1;      /* a car that cannot run makes the chug/crank noise instead of its engine */
      if (DS:7916 != 2 && DS:A808) race_isr_enter();
      while (anim_step(anim) != 0) ;                          /* driver slot 9 (DS:A0D2) */
      if (DS:7916 != 1 && DS:A808) race_isr_leave();
      DS:AD58 = DS:1BCC; DS:AD5A = DS:1BCE;                   /* last frame (car view) for later overlays */
      if (!DS:A808) sound_vec[10](0);
      DS:A808 = 0; DS:1BCA = 0; copy the anim's 8-byte rect back to DS:95DE..95E4;
   }
   if (LOC == 3 && mode == 4) for (i = 0; i < 2; i++) { pump_overlay(DS:50F2[i], 0x7B, DS:50FA[i], 0x3A, dst_a[i], i + 1); pump_overlay(DS:50F2[i], 0x7B, DS:50FA[i], 0x3A, dst_b[i], i + 4); }   /* dst_a = DS:9A4A/9A4C + 4i, dst_b = DS:9ACC/9ACE + 4i */
   gfx_screen_mode(2);  if (driver != EGA) copy_page(front -> back) [slot 5, DS:A0C2];
   if (DS:7916 != 1) break;   mode = 2; DS:7916 = 2;           /* the second pass of a two-page driver */
}
cleanup: DS:7912 = DS:7914 = DS:1C08 = 0; arena_release_levels(); DS:7916 = 0; cursor_ctl(-2);
```
Differences to SR1: no sticker (`DS:0294`), no smoke ground shadow in the driving modes (smoke 0 for the strip), garage x clamp, muffler/runnable-dependent sound, engine-less cars do not start the race ISR, pump overlay rewritten, mode 3 ends with `present_back_page`.
The car position outputs are `DS:AD5C/AD5E/AD60` (SR1 825E/8260/8262) and the gas-cap point `DS:AD62/AD64`.

### 1a21:9216 walker_anim
Changed (SR1 `walker_anim` 0f38:975b). `void walker_anim(FarPtr pic, int sx, int sy, int w, int h, int dx, int y, int base, int unused)`: copies rows `y .. y+h-1` of `pic` (source sx, sy, width `w`) into the car strip `DS:7906` (rows `base - 0x45 ..`) at x `dx`,
clipped to the strip (`top = base - 0x45`, `bot = strip.h + top - 1`; `y0 = max(y, top)`, `y1 = min(y + h - 1, bot)`; blit `{w, y1-y0+1, sx, sy + y0 - y, dx, y0 - top}`, slot 0, flags 1). At Bob's (`DS:1BC8 == 6`) the 7 background columns of the table `DS:5070`
(14-byte records, width word at +0, x word at +4: (16,40) (16,96) (16,152) (16,208) (16,264) (32,0) (32,288)) are refreshed from the front page (`DS:AD10/12`) into the strip: blit `{w, strip.h, sx = x, sy = base - 0x45, dx = x, dy = 0}`. SR1: strip `DS:5A0A`, table `DS:533E`.

### 1a21:92f2 carhop_frames
Changed (SR1 `carhop_frames` 0f38:985c): `void carhop_frames(FarPtr *frames, int mode)`: table record `t = DS:7918 + 6*mode` = `{u16 picture, u16 columns, u16 count}` (mode 0: 1113, 6, 11; mode 1: 1114, 10, 10; SR1 `DS:5A26`); the sheet is cut into `count`
frames of `w/columns x h/2` (two rows when `columns != count`): after `columns` frames the source x restarts at 0 and y moves down one row.

### 1a21:939a carhop_strip
Changed (SR1 `carhop_strip` 0f38:990f): `void carhop_strip(FarPtr *d)`: size of picture **0x458** (SR1 0x474); descriptor over VRAM page 2 at row offset `0x46`, `bitmap_init(d, ..., w, h, driver==EGA ? -2 : -1)`; EGA: `ega_latch_copy` of the visible page's rows from **0x52** (SR1 0x44) with width 0x28 bytes into it; other drivers `screen_save_rect(0, 0x52, w, h, d)`.

### 1a21:945a carhop_anim
Changed (SR1 `carhop_anim` 0f38:99d9). `void carhop_anim(int mode, int opp)`: the car-hop walks in (mode 0) or out (mode 1) at Bob's drive-in. Constants that differ from SR1: strip rows base `0x52`, car-hop frame cell `0x28 x 0x34` (SR1 0x30 x 0x29), town background snapshot
`DS:95EE/95F0` (`town_bg_save`: 0x28x0x34 taken from page (0x68, 0x52)), start x `(mode == 0 ? 0xD8 : 0) + 0x68`, y 0x85, step script `DS:79C0` (mode 0) / `DS:79D0` (mode 1) copied into a local 4-record script that is installed as the anim's script (`anim+0x10`),
frame index tables `DS:7924` (mode 0, 77 bytes: 6,5,4,3,2,1 repeated, ending 7,8,9,10,11) and `DS:7972` (mode 1: 1..10 then 4..10 repeated), 0x4D = 77 steps, 5 ticks each (`DS:1F66/1F68` clock); each step `bitmap_make_shifted(frame[idx-1], anim.x & ~align, cell)`
then one `anim_step`. Final `walker_anim(bitmap, 0, 0, w, 0x34, 0x68, 0x52, 0xA8)`; non-EGA drivers copy the front page to the back page (slot 5). `opp != 0` composes the opponent's car (`DS:AD58/AD5A`, position `DS:AD5C/AD5E`) in front of her as in SR1.

### 1a21:98e2 town_bg_save
Changed (SR1 `town_bg_save` 0f38:9e5d): snapshot of the town background behind the car: `DS:95EE = arena_bitmap_alloc(0x28, 0x34, 0, 0)`, filled by blitting the back page rect `{0x28, 0x34, sx 0x68, sy 0x52}` (SR1: 0x30 x 0x29 from (0xC8, 0x44), pointer `DS:6CCC`).

### 1a21:9b26 arena_release_levels
New (arena, shared with video). `void arena_release_levels(void)`: `DS:A31E = 0` (low-arena level count); `DS:B7EC = DS:BC4C` (top of the permanent block); `DS:A7D8 = 0x3C` (high-arena level index); `DS:B864 = DS:A0FE`. Frees every temporary
allocation but keeps the permanent block (`1a21:9b0e` also clears `DS:BC4C`).

### 1a21:9b40 arena_free_low
New. `void arena_free_low(int n)`: `if (n == 0) DS:A31E = 0; else DS:A31E -= n;` (pop `n` levels of the low arena, allocated with `kind 1` of `arena_alloc` 1a21:99d2).

### 1a21:9b56 arena_free_high
New. `void arena_free_high(int n)`: `if (n == 0) { DS:A7D8 = 0x3C; DS:B864 = DS:A0FE; return; } DS:A7D8 += n; top = (DS:A7D8 == 0x3C) ? DS:A0FE : DS:B7EE[DS:A7D8] - 1; DS:B7EC[DS:A7D8] = top;`
(pops `n` levels of the high arena, allocated with `kind 2` of `arena_alloc`). In this spec `arena_free_high(1)` replaces SR1's `arena_pop(1)` for bitmaps allocated with kind 2.

### 1a21:a016 anim_ring_reverse
= SR1 `FUN_0f38_a5a5` (srport `anim_ring_reverse`, garage_draw.c): reverses the 4-frame ring for driving out to the left. DS remap / layout: the anim record fields `+0x0E` (x copy) → **`+0x0C`** (`param_1[6] = DS:B866`, SR1 `param_1[7] = DS:8B7C`);
the frame-record table `DS:77C0` (12 bytes per frame) → **`DS:9D20`** (`*(u16*)(DS:9D22 + 12i) = *(u16*)(*(u16*)(DS:9D20 + 12i) + 6)`, then `DS:9D24 + 12i = *(...DS:9D22 + 12i) + 8`); the ring links are the same (`+6` of the frame descriptors). The final `if (DS:B866 & 7) in {2,4,6}`
test is unchanged.

### 1a21:a0d4 anim_phase_set
= SR1 `FUN_0f38_a65d` (srport `anim_phase_set`): `void anim_phase_set(ANIM *a, int i, int count, int dx, int dy)`: phase record `i` (8 bytes) at `*(u16*)(a + 0x10) + 4*i`... as in SR1 but the script pointer lives at **`a + 0x10`** (SR1 `a + 0x12`):
`rec.count (u16 @+0) = count; rec.dx (s8 @+2) = dx; rec.dy (s8 @+3) = dy; rec.next (u16 @+4) = 0xFFFF; rec.b6 = rec.b7 = 0`.

### 1a21:a11e anim_phases
= SR1 `FUN_0f38_a6ad` (srport `anim_phases`), identical arithmetic: `anim_phases(a, mode, d1, d2, d3, dist, p1, p2)`: modes 1/4: `n2 = ((dist*p2/100)/d2) & ~1; n3 = (((100-p1-p2)*dist/100)/d3) & ~3; n1 = (dist - n3*d3 - n2*d2)/d1`; modes 2/6:
`n2 = |((dist*p2/100)/d2)| & ~1; n1 = |((dist*p1/100)/d1)| & ~3; n3 = |((dist - n1*|d1| - n2*|d2|)/d3)|`; then `anim_phase_set(a,0,n1+1,d1,0); (a,1,n2,d2,0); (a,2,n3,d3,0); (a,3,-1,0,0)`. The phase table pointer offset is `+0x10`.

### 1a21:a25e anim_script
= SR1 `FUN_0f38_a7ee` (srport `anim_script`): the frame script of a `car_draw` mode `anim_script(a, mode, dist)`: modes 1 and 4: `anim_phases(a, 1, 8, 4, 2, dist, p1, p2)` with `(p1, p2) = (0, 0x3C)` when `DS:1BC8 == 1` else `(0x19, 0x37)`; mode 2: `anim_phases(a, 2, 2, 4, 8, dist, 10, 0x3C)`;
mode 6: `anim_ring_reverse(a); anim_phases(a, 6, -2, -4, -4, dist, 0x28, 0x3C)`; other modes: nothing. `drive_dest` (SR1) → **`DS:1BC8`**.

### 1a21:b16e tc_wheel_strip
Changed (SR1 `tc_wheel_strip` 0f38:b632): hub picture **0x47C** (`DS:9634`, SR1 0x4C7); the strip of the car around the wheel is saved from the screen row **0x80** (SR1 0x78) with height `0xA2 - DS:79E4` (0x28 wide), blended with the car (`DS:96A0`) at `y = 0x9B - car.h - DS:79E4`;
`DS:9668` = car x (SR1 `DS:6DA0`); `DS:79E4` = y base (0 except Tandy). Otherwise the same two-step construction.

### 1a21:b358 tc_wheel_swap
Changed (SR1 `tc_wheel_swap` 0f38:b82c). `void tc_wheel_swap(int x, int *old_pics, int *new_pics)`: the hand picture is **0x47F** (SR1 0x4D2); wheel frames are fetched with `1a21:b0f6(pic, kind)` (a `pic_get` variant); vertical positions of the animations: lowering the old
wheel at `0x9A` (SR1 0x94), rolling away/in at **0xA1** (SR1 0x9B), raising the new wheel at `0xA1`; the hold-off marker `DS:9640 + 0xE == 0x85` (SR1 0x7D); phases: down `8` steps dy +1, roll away `(x + w)/2` steps dx -2, roll in `(x+w)/2 + 1` steps dx +2,
up `8` steps dy -1. Sound: vector slot 0 (`DS:BB8C`, arg 1/0, with `DS:1F7C = 8`) brackets each roll (SR1 `loop_idle_putt`), slot 14 (`DS:BBC4`) plays at the end; `arena_free_low(6)` frees the rolling frames (SR1 `arena_pop_low(6)`).

### 1a21:b9bc tire_change_anim
Changed (SR1 `tire_change_anim` 0f38:be93, §4.9). `void tire_change_anim(int *old_pics, int *new_pics)` (pointers to the 4 wheel picture ids = `TYRE_PICS[grade]`). Same story (jack comes up, car lifted 7, rear wheel swapped, front wheel swapped, car lowered 9, jack goes away) with these
differences: no sticker save/restore; `car_compose(frames, model, flags, 0, 0, 0, 1)`; jack pictures from the id table **`DS:79E6`** = {1021, 1020, 1022} (SR1 `DS:5AAA`); jack x **0x90** (SR1 0x88) and the jack background colour fill is gone; backdrop
`show_picture(0x3FA)` (the garage backdrop piece, SR1 0x408); shadow picture **0x472**, saved at y `0x9F - sh.h` (SR1 0x99 / 0x49D); the car background strip is saved to row `0x9F` (SR1 0x99); hub **0x47C**; car band saved from y **0x88** (0x1A rows, SR1 0x80 / 0x1C);
jack rise: `sprite_scene_setup(0x90, 0xB9, ...)`, phase **0x20** steps up (SR1 0x21), lower: `(0x90, 0x9A)`, 0x20 steps down; car raise at y **0x9E**, 7 steps up; car lowering at y **0x9A**, 9 steps down (SR1 0x98 / 0x94); the crank frames cycle through the 3 jack
pictures as before (`DS:95FC`). Sound / music: AdLib (`DS:0050 == 0`): `music_stop()` at the start; slot 6 (`DS:BBA4`) on/off around the jack moves with `DS:1F7C = 0x10`; slot 7 (`DS:BBA8`) pulses on each crank step of the car; slot 22 (`DS:BBE4`, arg 1) mutes during
the lift and (arg 0) after lowering; at the end `music_resume()` if `DS:5304`. End: `arena_release_levels()`, copy front page to back page (slot 5), `DS:B866 = car x`, `cursor_ctl(-2)`.

### 2925:2b26 under_car_slide_in
New (module vote: video). `void under_car_slide_in(void)`: builds the under-car animation: `bg` = extract {0x68,0x28,0x50,0x8F} from picture `0x3FA` (the garage backdrop);
composes the car with `car_compose(&frames, car_x_model(DS:AD76), 0, 0, &TYRE_PICS[DS:AD72], 0, 1)`; copies the composed car (shifted so that the car
sits at (0x50 - DS:AD5C, 0x8F - DS:AD5E)) and the picture `0x3FC` (row y limit) into a work bitmap; creates shifted copies of picture `0x4AE` (the jack,
`~DS:AD24 > 2` copies at pixel shifts 2, 4, ...); then loops `x` from 0x50 to 0x80 (step 2) and `y` from 0xAC upward (step -1 down to 0x90): each pass
draws the backdrop strip, the shifted jack at (x, y) and the car (masked), `wait_vretraces(2)`, and copies the dirty rectangle to the front page - the
car is lifted by the jack. Frees with `arena_free_low(...)`. (Pure animation; no game state.)

### 2925:2e60 under_car_draw
New (the symbol table calls it `video_options_menu2`; it has nothing to do with video options). `void under_car_draw(void)`: `pic = pic_get(0x4AF)` (under-car backdrop, kept in `DS:9718/971A` for erasing), drawn at (0x18,0x15) on the back page; then for the
fitted parts draws their pictures (masked, at absolute coordinates): exhaust manifold `DS:822A[grade]` {1203, 1204} at (200, 0x50); muffler `DS:822E[grade]`
{1205, 1206} at (0xA8, 0x5C); differential `DS:8232[grade]` {1200, 1201, 1202} at (0x50, 0x3F), where `grade` = catalogue byte `DS:4504 + 10*type`. Copies
(0x18,0x15)-(0x127,0xA0) to the front page.

### 2925:2ff8 under_car_part_pic
New (module vote: video). `void under_car_part_pic(int cat, int type)`: redraws one part after a change; `type < 0` erases it (restores the backdrop `DS:9718` rectangle). Rectangles
(backdrop coordinates, drawn at +0x18,+0x15): cat 5 {x 0xB0..0xF7, y 0x3B..0x4B}, cat 6 {x 0x90..0xA7, y 0x47..0x51}, cat 7 {x 0x38..0x4F, y 0x2A..0x3C}; the
picture is the one of the tables above (`type >= 0`), then the rectangle is copied to the front page.

### 2925:3174 under_car_menu
New (the RE_GUIDE calls it `stickers_menu_sr2`; it is garage code 14 "Under the Car" - SR2 has **no stickers**). `int under_car_menu(void)` (far): the exhaust
manifold / muffler / differential of the current car. Helpers `under_car_slide_in` (2925:2b26), `under_car_draw` (2925:2e60), `under_car_part_pic` (2925:2ff8).
```c
if (!cur_car) { msg_box(9); return 0; }
under_car_slide_in();  under_car_draw();
for (;;) {
   do {
      n0 = 0;  b = ui_dialog(34);                          /* screen 34: -2 Exhaust manifold, -3 Muffler, -4 Differential, -1 Done */
      if (b == -4) { cat = 7; none_msg = 118; slot = &car->differential(+0x20); }
      else if (b == -3) { cat = 6; none_msg = 117; slot = &car->muffler(+0x1E);
                          if (*slot) { DS:A61C[0] = "Remove current muffler" /*DS:8238*/; wear[0] = 0x80; n0 = 1; } }
      else if (b == -2) { cat = 5; none_msg = 116; slot = &car->exhaust_manifold(+0x1C); }
      else if (b == -1) { arena_free_high(1); return 2; }
      spares_collect(cat, nodes, wear, n0, &n);
      r = list_box(7, 0, DS:A61C, wear, n);               /* -3 Change, -4 Forget it */
   } while (r == -4);
   if (r == 0x3EA) { msg_box(none_msg); continue; }       /* "no spare exhaust manifolds / mufflers / differentials" */
   sel = (r == -3) ? DS:B88E : 0;
   if (cat == 6 && *slot && sel == 0) {                    /* "Remove current muffler" */
       under_car_part_pic(6, -1); part_uninstall(slot); continue; }
   part = nodes[sel];
   if ((CAT[part->type].make_mask(+5) & MODELS[car->model].specA & 0x0F) == 0) { msg_box(24 /*That won't fit!*/); continue; }
   under_car_part_pic(cat, part->type);
   part_install(spare_unlink(part), slot);
}
```
(The fit test is the same make-mask test as for transmissions; differentials/mufflers are "fits all" (mask 7).) The garage hot spot code 14 ("Under the
Car") returns 2 on Done, which makes `game_loop` redraw the garage with mode 2.


## Handoffs

**Claimed from other modules' lists** (the module vote of `port/sr2_modules.csv` put them elsewhere; they are garage code and are specified and listed in `garage_symbols.csv`; S3.7 must deduplicate if another spec also lists them):
`0001:3f5a sell_spare_parts` (vote: game_flow), `0001:72f2 car_info_sr2` (game_flow), `0001:7aec ignition_tune` (sound - it only contains engine-tone calls), `2925:3174 under_car_menu`, `2925:2b26 under_car_slide_in`, `2925:2e60 under_car_draw`
(symbol name `video_options_menu2` is wrong), `2925:2ff8 under_car_part_pic` (video), `1a21:27d2 msg_close` (video).

**In the garage list but better owned elsewhere** (still specified here so the coverage gate is met): `1a21:565c slot_list_edit` (save-game slot picker, game_flow `save_game_screen`), `1a21:9b26/9b40/9b56` arena helpers (video), `1a21:9216/92f2/939a/945a/98e2` Bob's car-hop helpers
(race `bob_drive_in`), `1a21:8268 pump_overlay` (gas-station event), `0001:4d90 message_at` (race results), `1a21:2018 dialog_msg`, `1a21:204c fatal_message`, `1a21:2d90 cursor_ctl` (platform / video), `0001:6796`-`68e2` (garage screen helpers).

**Functions of other modules that this spec calls** (not specified here): `event15_run` 2925:2104 and helpers `2925:1ab8-1f38, 20be` (the gas station, race), `event17_run` 2925:1970, `bob_drive_in` 0001:a626, `drive_to_event` 0001:8c68, `car_max_speed` 0001:c6f6 (race);
`hotspots_reset` 0001:0e74 (classifieds close), `kbd_hotspot_refresh` 0001:1c5c, `ui_wait` 0001:1417, `msg_box` 0001:3db4, `wait_ticks` 0001:3d80, `input_reset`, `click_clear`, `race_isr_enter/leave` (platform / game_flow);
`pay_or_complain` 1a21:1e50, `status_label` 1a21:1e86, `money_add` 1a21:1df4, `ui_menu` 1a21:4cf4, `ui_dialog` 1a21:4d26, `ui_edit` 1a21:610c, `rnd` 1a21:6180, `hot_data_load` 1a21:6e6a (game_flow / platform);
`draw_element` 1a21:44a8, `draw_screen` 1a21:473e, `ui_hit_test` 1a21:43dc, `demo_input_hook` 1a21:4806, `sprite_scene_setup`, `pic_get`, `pic_draw_masked`, `pic_blit_at`, `bitmap_*`, `screen_*`, `arena_alloc/bitmap_alloc`, `ega_*`, `1a21:80ec/ade2/ad48/b09e/b0f6` (video).

**Data given to other modules**: the CAR record (0x2A bytes) and part chains (order `+06,+08,+0A,+0C,+0E,+10,+12,+1C,+1E,+20`) are saved/loaded by `save_game_screen`/`load_game_screen` (FORMATS.md); the 128-byte ad block `DS:9B40` is written after the clock fields;
race reads `cur_car` (engine, manifold, carburettors, transmission, tyres, exhaust manifold, muffler, differential, ignition `+23`, gas `+24`, engine_link `+22`, bolts `+14..+1B`) through `car_max_speed`, the opponent records (`+0A` colour, `+0D` flags, `+0F`, `+10`) and `car_flags`;
`DS:1BC8` selects the location tables of `car_draw`; `DS:AD5C/AD5E/AD62/AD64` are the car / gas-cap positions used by the gas-station and Bob's events.

## Open questions

1. `bay_bolt_draw` and `bay_bolt_remove_anim` were checked against the disassembly (slot-11 slide arguments `(top, bottom, x, from, to, sprite, mask)`); `bay_part_in_anim`, `bay_part_out_anim`, `bay_connector_draw` and the transmission drawing helpers were read from the decompile, which garbles slot-11/slot-1 calls (the SR1 port notes say the same) - re-check their rectangle fields with `x86dis.py` while porting.
2. `classifieds_run` "Next page" restarts at the last row shown (`DS:1ECA`), i.e. with one row of overlap; this follows the code literally.
3. `classifieds_roll_ads`: if every id has a non-zero counter while ads are still wanted, `rnd(DS:9B86)` with 0 and the search loop would not end; not reachable in normal play (counters are only raised by +3 per purchase).
4. Sound vector slot roles (5, 9, 10, 13, 14, 17, 22 ...) are inferred from the call sites; the SR2 sound spec defines the slots. `kbd_hotspot_refresh` (0001:1c5c) is described from its use only.
5. `car_draw` Tandy / CGA / Hercules branches (`1a21:80ec`, second pass `DS:7916`) are summarised, not transcribed. `1a21:831c`'s unused `top` argument and the `DS:7912/7914` strip variables are only relevant to those drivers.
6. `car_new` manifold special case `grade == 8 && make == 4` can never trigger (make is 0..2); the stock cars never use grade 8, so the Ford supercharger gap (catalogue 49 is Chrysler's) is moot.
7. `MODEL.b6` (preview colour) is rolled by `hot_data_load`; its exact timing (new game vs program start) was not checked. The garage uses `+6` as the colour of a bought car.
8. The gas station (`event15_run`) and the tyre-change sound slots are outside this spec; `fatal_message`'s text-mode branch only uses DOS print calls.
9. Savegame: the exact position of the 128-byte block `DS:9B40` relative to `DS:1F72` is from FORMATS.md ("+1A2 0x80"); contents decoded here (week, ads, counters, pool, part row, prices).
