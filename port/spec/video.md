# Video (SR2 delta): graphics library, driver vector tables, EGA/VGA blitters, text, palette, polygon fill (VGA 16-colour path only)

Scope: graphics library functions (descriptors, clipping, arena bitmaps, primitives, cursor, palette, picture helpers), the EGA/VGA driver and its hand-written helper segments. CGA / Hercules / Tandy driver functions get one line each: "not ported (VGA only)".
SR2 segments: `0001:3c60`, `0001:a4aa`, `1a21` (graphics library, ~85 functions), `26b3` (`pic_unpack`), `26c0` (cursor blits), `2719` (CGA/Hercules driver), `2f32` (EGA/VGA and Tandy driver), `3210`-`33e5` (hand-written helpers), `33f6`/`33f8` (**new polygon fill**), `3422`/`3458` (CGA/Tandy slot 18), `3488` (pixel decimation helpers).
Counts (matcher classes, `port/sr2_sr1_matches.csv`): **177 functions = 16 same / 121 changed / 40 new**. Specified here: 54 by reference to SR1 (`= SR1 ...` lines, recompiled or only re-addressed), 26 with a described difference, 15 new with full text, 76 "not ported (VGA only)" rows.
Builds on: `../StreetRod/port/spec/video.md` (EGA/VGA driver, blit helpers, primitives, palette, pointer). Addresses are `SSSS:OOOO` as in the index (`0001:xxxx` is SR1 `0000:(xxxx+0x10)`-style main segment; Ghidra adds 0x1000 to the segment). Conventions of SR1 apply (MSC medium model, far call arguments from `[bp+6]`, near from `[bp+4]`, result in AX / DX:AX).


## 1. Overview of the differences to SR1

* **Descriptor ("Desc") is 0x12 bytes** (SR1: 0x30). Every function that touched `+0x24 / +0x28 / +0x2C / +0x2E` uses `+6 / +0xA / +0xE / +0x10` now. The remap is mechanical: a porter reusing the SR1 C code only changes the struct.
* **Driver vector table has 20 slots** (SR1: 15) at `DS:A0AE` (SR1 `DS:78A2`): new slots 15..19 (15/16/19 = road renderer helpers in `0dcb`, **17 = set colour**, **18 = polygon fill**).
* **New**: polygon fill (slot 18, `33f8:01ca`), `ega_set_colour` (slot 17), `bitmap_clear`, `pic_get_padded`, `bitmap_blit_at` / `bitmap_blit_masked_at` wrappers, `arena_bitmap_alloc(pool -1)` (second arena), pixel decimation helper (slot 15).
* **Changed behaviour**: `pic_unpack` token escape; `pic_decode` decodes in place; `rect_subtract_edge` / `anim_restore_rect` flag polarity inverted; `ega_anim_step` per-event tile lists and `Anim` layout (one overlay); `ega_slide_sprite` extra `alt` frame; `sprite_hspan_clip` strictness; `cursor_*` bank of 3 shapes and race-page redirection; `ega_blit_shifted`/`shift_row`/`ega_put_shifted_row` re-implemented (same pixels); `gfx_screen_mode` loses mode 4; `gfx_init` initialises the UI colour variables; `screen_text` mirror condition; palettes (below).
* **Everything else** in the library is the SR1 code with new DS addresses and the new Desc offsets ("= SR1" lines).
* VGA behaviour is unchanged otherwise: BIOS mode 0Dh, planar 40 bytes per line, latched copies, pages at A000:0000 / 2000 / 4000 / 6000 (page 1 also reachable as A200:0000), VRAM picture cache from A000:7D00, split screen via CRTC line compare.

## 2. Desc (bitmap / page descriptor, 0x12 bytes)

```c
typedef struct {        /* SR1: 0x30 bytes; SR2: 0x12 bytes (+2 bytes allocation padding in bitmap_alloc) */
    u16 w;              /* +00 width in pixels (SR1 +00) */
    u16 h;              /* +02 height          (SR1 +02) */
    u16 size;           /* +04 bytes of all colour planes (= stride*h*nplanes); screen page 1F40h (SR1 +04) */
    FarPtr planes;      /* +06 plane 0; plane k at planes + k*stride*h (screen: A000:off, planes parallel)   (SR1 +24) */
    FarPtr mask;        /* +0A 1 bit = transparent; 0:0 = none                                                (SR1 +28) */
    u16 stride;         /* +0E bytes per row = ((w-1) >> DS:AD20) + 1; 28h for screen pages                    (SR1 +2C) */
    u8  type;           /* +10 FEh = EGA/VGA screen page (FDh CGA, FCh Hercules, FAh Tandy); memory bitmap: FFh (SR1 +2E) */
    u8  pad;            /* +11 */
} Desc;                 /* Rect = { i16 w, h, sx, sy, dx, dy } unchanged */
```

Static page descriptors (segment `371c`, 0x12 bytes each, listed as data rows below): page 0 `371c:0000` A000:0000, page 1 `371c:0012` A200:0000, page 2 template `371c:0024`, page 3 `3720:0000` (filled by `gfx_init`), plus `3723:0000` (race front buffer copy of page 0) and the scratch descriptor `3722:0002`.
Memory bitmaps are created by `bitmap_alloc` (0001:3a42): header 0x12 + padding 2, then the colour planes (plane after plane) and the mask (`stride*h`, 1 = transparent); `size` = all planes.
The picture directory entries (0x22 bytes, segment 0x3726) are not Descs; see platform.md `lib_read_dir`.

## 3. Driver vector table (20 far pointers, `DS:A0AE + 4*slot`)

| Slot | `DS:` | EGA/VGA (template `DS:7B38`) | CGA/Herc (`DS:7A40`) | Tandy (`DS:7ADC`) | Meaning |
|---|---|---|---|---|---|
| 0 | A0AE | `2f32:198a` ega_blit | `2719:11fe` | `2f32:04a2` | `blit(src, dst, rect, flags)` |
| 1 | A0B2 | `2f32:230f` ega_blit_shifted | `2719:1566` | `2f32:0788` | `blit_shifted(src, dst, rect)` |
| 2 | A0B6 | `2f32:1dd8` ega_blit_masked | `2719:1092` | `2f32:0364` | `blit_masked(src, dst, rect, mode)` |
| 3 | A0BA | `2f32:112e` ega_draw_text | `2719:01b8` | `2f32:0918` | `draw_text(dst, x, y, s)` |
| 4 | A0BE | `26c0:0175` ega_draw_cursor | `26c0:00c1` | `26c0:0002` | pointer blit with save-under |
| 5 | A0C2 | `2f32:17a4` ega_copy_page | `2719:0d02` | `2f32:0b80` | `copy_page(src, dst)` |
| 6 | A0C6 | `2f32:1571` ega_recolour_rect | `2719:0700` | `2f32:09da` | `recolour_rect(...)` |
| 7 | A0CA | `1a21:0da6` pal_black | `2719:0e7a` | `1a21:0da6` | screen off |
| 8 | A0CE | `1a21:0db8` pal_normal | `2719:0ea2` | `1a21:0db8` | screen on (palette) |
| 9 | A0D2 | `2f32:2754` ega_anim_step | `2719:17ca` | `2f32:0cbe` | `anim_step(anim)` |
| 10 | A0D6 | `2f32:17f5` ega_blinds | `2719:0d58` | `2f32:0b9c` | `blinds(dst, src)` |
| 11 | A0DA | `2f32:2ab2` ega_slide_sprite | `1a21:a2f2` | `1a21:a2f2` | `slide_sprite(top, bottom, x, from, to, sprite, alt)` |
| 12 | A0DE | `2f32:2540` ega_line | `2719:1c7c` | `3236:0038` | `line(x0, y0, x1, y1, colour)` |
| 13 | A0E2 | `0dcb:043c` (race_render) | `2719:1f19` | `3236:025f` | `hspan(x0, x1, y, colour)` |
| 14 | A0E6 | `0dcb:04e6` (race_render) | `2719:2040` | `3236:0307` | `fill_rows(y, colour)` |
| 15 | A0EA | `0dcb:b8c8` (race_render) | `0dcb:b9e0` | `0dcb:ba72` | shrink a picture (uses `3488:000e`) |
| 16 | A0EE | `0dcb:bd12` (race_render) | `0dcb:c09a` | `0dcb:c342` | road renderer helper |
| 17 | A0F2 | `2f32:1932` ega_set_colour | `2719:1c60` | `3236:0371` | set drawing colour (Set/Reset register) |
| 18 | A0F6 | `33f8:01ca` poly_fill | `3422:025d` | `3458:0228` | **polygon fill** |
| 19 | A0FA | `0dcb:0006` (race_render) | `0dcb:0108` | `0dcb:01da` | road renderer helper |

`driver_select` (platform.md) picks the table; `ega_install_vectors` copies the 20 pointers (0x28 words). VGA = driver id -2.

## 4. Palettes

* The hardware palette is only programmed through INT 10h (`ega_set_palette`: AX=1002h with 16 registers + overscan; `ega_set_palreg`: AX=1000h/1001h); values > 7 are stored `| 0x10`. Displayed colour of pixel value c = `ega_palette[(r & 7) | (r & 0x10 ? 8 : 0)]` with `r = vga_pal[c]` (200-line interpretation), as in SR1 (video.md §1.1/§4.10).
* **New in SR2**: the live table `DS:1DA6` is no longer fixed. It is loaded by `pal_load(src)` (1a21:198a) from constant tables and applied by `pal_normal` (slot 8) or `ega_set_palette(DS:1DA6)`:

| Table | Content (17 bytes: 16 registers + overscan) | Loaded by |
|---|---|---|
| `DS:1DA6` live | initially (file) `00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F 00` (identity); `main` applies it right after `gfx_init` | `main` (`ega_set_palette(DS:1DA6)`) |
| `DS:1DB8` | all 00 (black) | `pal_black` |
| `DS:1DC8` (and `1E2E`) | `00 00 08 07 0F 07 09 0B 03 02 04 0C 05 0A 06 0E 00` (the SR1 game palette) | race / event set-up (`0001:8d9e`) |
| `DS:1DDA` | `00 00 08 0D 0F 07 00 00 01 02 09 0C 05 0A 06 0E 00` | `game_over_menu`, `new_game_prompt`, `garage_screen`, `dancing_girl`, `jail`, `summer_over_screen`, `event15_run`, `pal_shadow_patch` |
| `DS:1DEC` | `00 01 02 03 04 05 06 07 08 09 00 0B 0C 0D 0E 0F 00` | `0001:a51c` (end-of-event picture) |
| `DS:1DFE` | `09 01 08 00 0F 07 0B 03 0E 02 04 0C 0D 0A 06 0E 00` (SR1 `DS:0462`) | `pal_shadow_swap` (exchanged with `DS:1DA6`) |

  Consequence for the port: the start-up screens (title, menus) are shown with the **identity** table until a screen calls `pal_load`; `tools/srlib.py --palette game` (`DS:1DC8`) does not describe those screens.
* Single register changes (`ega_set_palreg`) are used for colour cycling; `pal_shadow_patch` sets registers 6, 7, 8 to 09 01 03.

## 5. VGA memory map (unchanged from SR1)

`A000:0000` page 0 (visible, 8000 bytes per plane), `2000` page 1 back buffer (also `A200:0000`), `4000` page 2, `6000` page 3, `1F40..1F6F` pointer save-under (3 bytes x 16 rows, all planes through the latches), `4AF0` slide-sprite save area, `7D00..FBFF` per plane picture cache (`vram_pool_alloc`, base `DS:490E`).
Line compare (CRTC) split screen: `ega_set_split` unchanged (`line + 100` on VGA).

## 6. Regular DS remap blocks

| SR1 range | delta | SR2 range | Content |
|---|---|---|---|
| `7316-7322` | +0x276C | `9A82-9A8E` | page table (4 far pointers) |
| `7AEC-7B12` | +0x27CE | `A2BA-A2E0` | pointer (cursor) state |
| `78A2-78DA` | +0x280C | `A0AE-A0E6` | driver vector table (SR2 continues to `A0FA`, slots 15..19 new) |
| `7D18-7D50` | +0x282A..+0x28BA | `A544-A60A` | animation scene state (see `sprite_scene_setup`) |
| `822A-8258` | +0x2AE6 | `AD10-AD3E` | graphics state: front/back page, mirror, text colours, status line |
| `825A-8262` | +0x2AFE | `AD58-AD60` | view rectangle (12 words `AD40-AD56` of UI colours were inserted before it) |
| `0440, 0452, 0462` | +0x1966 / +0x1966 / +0x199C | `1DA6, 1DB8, 1DFE` | palettes |
| `04BE-04C2` | +0x1982 | `1E40-1E44` | `show_picture_at` origin / height |
| `5C0C, 5C15, 5C1E` | +0x1F7C | `7B88, 7B91, 7B9A` | blind offsets, masks |


## Globals

DGROUP is `3E08` in SR2 (SR1 `3E96`). `DS:xxxx` = image `0x3E080 + xxxx`. "SR1 equivalent" = DS offset in the Street Rod 1 specs.

| DS offset | type | name | meaning | SR1 equivalent |
|---|---|---|---|---|
| `DS:AD0C` | far ptr | `race_front` | race-mode front page (set by the road renderer; used by `cursor_redraw` when `racing`) | - |
| `DS:ADA2` | far ptr | `race_back` | race-mode back page (VGA) | - |
| `DS:AD10` | far Desc* | `g_front` | visible page descriptor (DS:AD10 off, AD12 seg) | `DS:822A` |
| `DS:AD14` | far Desc* | `g_back` | back page descriptor drawn into (AD14, AD16) | `DS:822E` |
| `DS:AD18` | u16 | `g_aux_a` | cleared by `gfx_init` | `DS:8232` |
| `DS:AD1A` | u16 | `g_aux_b` | cleared by `gfx_init` | `DS:8234` |
| `DS:AD1C` | i16 | `driver_id` | -2 EGA/VGA, -4 Hercules, -6 Tandy (CGA: type byte FDh in the page descriptor, id stays whatever `driver_select` set) | `DS:8236` |
| `DS:AD1E` | u16 | `g_nplanes` | 4 on EGA/VGA, 1 on CGA/Herc/Tandy | `DS:8238` |
| `DS:AD20` | u16 | `g_px_shift` | pixel -> byte shift (3 on EGA/VGA) | `DS:823A` |
| `DS:AD22` | u16 | `g_px_factor` | 1 (EGA/VGA), 2 (CGA), 4 (Tandy) | `DS:823C` |
| `DS:AD24` | i16 | `g_x_align` | x alignment mask: 0xFFF8 on EGA/VGA | `DS:823E` |
| `DS:AD26` | u16 | `g_mirror` | 1 = also copy drawing from the back page to the front page (double buffering) | `DS:8240` |
| `DS:AD28` | u8 | `g_text_fg` | text foreground (scheme colours); `AD29` = background | `DS:8242 / 8243` |
| `DS:AD2A` | u8 | `g_fill_col` | fill colour; `AD2B` = line colour | `DS:8244 / 8245` |
| `DS:AD2C` | u8 | `g_frame_style` | 0 none, 1 single, 2 double, 3 solid | `DS:8246` |
| `DS:AD2D` | u8 | `g_ui_level` | UI level (1 after `gfx_init`, 2 after `cursor_init`) | `DS:8247` |
| `DS:AD2E` | u8 | `g_overscan` | overscan (border) register value for `ega_set_palette` | `DS:8248` |
| `DS:AD2F` | u8 | `g_pal_visible` | 0 after `pal_black`, 1 after `pal_normal` | `DS:8249` |
| `DS:AD32` | i16 | `g_text_end_x` | end x of the last `draw_text` (AD34 = y) | `DS:824C / 824E` |
| `DS:AD36` | u16 | `status_w` | status line parameters set by `status_init` (AD36 = 0x58, AD38 = y + 1, AD3A = 0xA0, AD3C, AD3E) | `DS:8254.. 8258` |
| `DS:AD40` | u16[12] | `ui_colours` | **new**: UI palette indices set by `gfx_init` (`AD40` .. `AD56`; see `gfx_init`) | `DS:8266 / 8268 / 7644..` |
| `DS:AD58` | u16* | `view_rect_ptr` | pointer to the view rect; `AD5C/AD5E/AD60` its corner coordinates (`rect_clip_to_view`) | `DS:825A..8262` |
| `DS:ADD8` | u16 | `slow_machine` | see platform.md | `DS:82C6` |
| `DS:9A82` | far Desc*[4] | `g_page` | page table: `9A82` page 0, `9A86` page 1, `9A8A` page 2, `9A8E` page 3 | `DS:7316..7322` |
| `DS:9BC0` | u16 | `g_draw_off` | offset part of the line/fill target (`fill_rect`) | `DS:767C` |
| `DS:9DC2` | u16 | `g_draw_seg` | segment of the line/fill/poly target (`DS:9DC2`) | `DS:787A` |
| `DS:9DC6` | u8* | `g_scheme` | colour scheme table in use: `0x1D06` (EGA/VGA/Tandy) or `0x1D56` (CGA); scheme n = 5 bytes at `+5n`... status scheme 11 at `+0x37` | `DS:787E (0x3A0 / 0x3F0)` |
| `DS:A0AE` | far ptr[20] | `drv_vec` | **driver vector table, 20 slots** (see below); `A0CE` = slot 8, `A0DE` = slot 12, `A0F6` = slot 18 | `DS:78A2 (15 slots)` |
| `DS:7A40` | far ptr[20] | `vec_cga` | CGA/Hercules driver template | `DS:5B04` |
| `DS:7ADC` | far ptr[20] | `vec_tandy` | Tandy driver template | `DS:5B8A` |
| `DS:7B38` | far ptr[20] | `vec_ega` | EGA/VGA driver template | `DS:5BD0` |
| `DS:7B88` | u16[5] | `g_blinds_rows` | `ega_blinds` pass offsets `0 50 A0 28 78` | `DS:5C0C` |
| `DS:7B91` | u8[9] | `g_lmask` | left masks `00 80 C0 E0 F0 F8 FC FE FF`; `DS:7B99` = right-mask view | `DS:5C15` |
| `DS:86EA` | u16 | `g_font_seg` | font segment (26E6); glyph `c` at offset `c*9 - 0x11A` | `DS:691A (seg 0E9F)` |
| `DS:1C48` | far Desc* | `g_saved_back` | pages remembered by `gfx_screen_mode(0)` (`1C48` back, `1C4C` front) | `DS:02E6 / 02EA` |
| `DS:1D06` | u8[] | `scheme_ega` | colour scheme table (5 bytes per scheme) for EGA/VGA; `DS:1D56` = CGA table | `DS:03A0 / 03F0` |
| `DS:1DA6` | u8[17] | `pal_live` | live palette register table (16 + overscan); loaded by `pal_load`, applied by `pal_normal` | `DS:0440` |
| `DS:1DB8` | u8[17] | `pal_black_tab` | all zero | `DS:0452` |
| `DS:1DC8` | u8[17] | `pal_game` | game palette `00 00 08 07 0F 07 09 0B 03 02 04 0C 05 0A 06 0E 00` (= SR1 `DS:0440`); `DS:1E2E` copy | `DS:0440` |
| `DS:1DDA` | u8[17] | `pal_menu` | palette `00 00 08 0D 0F 07 00 00 01 02 09 0C 05 0A 06 0E 00` used by menus/garage/jail (`pal_load(0x1DDA)`) | - |
| `DS:1DEC` | u8[17] | `pal_identity2` | identity with register 10 = 0 (`FUN_0001_a51c`) | - |
| `DS:1DFE` | u8[17] | `pal_alt` | alternate palette `09 01 08 00 0F 07 0B 03 0E 02 04 0C 0D 0A 06 0E 00` (swapped by `pal_shadow_swap`) | `DS:0462` |
| `DS:1E1E` | near*[] | `gear_labels` | gear label string pointers (`gear_label`) | `DS:0482` |
| `DS:1E40` | i16 | `pic_origin_x` | `show_picture_at` origin x, `1E42` y, `1E44` clip/clear height (200) | `DS:04BE / 04C0 / 04C2` |
| `DS:1BC8` | i16 | `event_id` | current event / trip id (2 Bob's, 3 event 15, 4/6 other); selects tile lists in `ega_anim_step` | `DS:0284` |
| `DS:1BCC` | u16 | `anim_w` | sprite w/h cache of `sprite_anim_advance` (`1BCE` = h) | `DS:0288 / 028A` |
| `DS:1C08` | i16 | `g_anim_yoff` | sprite height reduction used by the animation code | `DS:02D0` |
| `DS:50CC` | u16[12] | `tiles6` | 6 tile pairs `{x, w}` for event 6 animations; `DS:50E4` = 3 pairs for event 15 | `DS:539A` |
| `DS:A2BA` | far Desc* | `g_ptr_save` | pointer save-under descriptor (`A2BA`); cursor state block `A2BA..A2E0` | `DS:7AEC` |
| `DS:A2D8` | i8 | `cursor_state` | 0 off, 1 on, <0 restoring | `DS:7B0A` |
| `DS:A544` | Anim* | `scene_ptr` | sprite scene header (`sprite_scene_setup` return) with fields `A546..A556` | `DS:7D18` |
| `DS:A5FC` | PageNode[2] | `page_nodes` | animation page ring (16-byte nodes) | `DS:7D42` |
| `DS:9D1A` | sprite rec[] | `scene_sprites` | 12-byte sprite records of the scene | `DS:77BA` |
| `DS:977E` | u8[18*n] | `strip_descs` | car strip descriptors (non-VGA) | `DS:6E68` |
| `DS:B7AE` | u8 | `racing` | non-zero while a race / event screen is active (redirects the pointer to the race pages) | `DS:8ACA` |
| `DS:B87C` | u16 | `gfx_aux` | cleared by `gfx_init` | - |

## Functions

### 0001:3c60 vram_pool_alloc

= SR1 `vram_pool_alloc` (0000:3967), ../StreetRod/port/spec/platform.md §2.4; DS remap: DS:4C14 → DS:490E; DS:4C16 → DS:4910; DS:4C18 → DS:4912. video-memory base far pointer `DS:490E:4910` (A000:0000), used counters `DS:4912` x4 (SR1 `DS:4C14` / `DS:4C18`); result = base + 0x7D00 + `used[k]`, plane k = first with `used[k] + size < 32000`, `*plane = FF` when none

### 0001:a4aa dancer_blit

NOT SR1 `bitmap_blit_at` (the matcher paired it by mistake). `void dancer_blit(Desc far *dst)`: calls driver slot 2 (`blit_masked`, `[DS:A0B6]`) with source = the bitmap whose far pointer is `DS:AAFC:AAFE`,
destination `dst`, rect `{w=0x18, h=0x2C, sx=0, sy=0, dx=0x122, dy=0x57}` and mode 1: draws a 24x44 sprite at (290, 87). `DS:AAFC` is filled by `0001:a4ea` (`pic_get(0x47D, 2)`, LIB2 picture 0x47D) for the
Bob's Drive-In dancer scene (`dancing_girl` 0001:a588).

### 1a21:1048 gfx_screen_mode

`uint gfx_screen_mode(int n)`. SR1 (`0f38:0dbd`, video.md §4.8) with these differences: **case 4 is gone** (n = 0..3 only); globals remapped (front page `DS:AD10`, back page `DS:AD14`, mirror flag `DS:AD26`, driver id `DS:AD1C`,
saved pair `DS:1C4C:1C4E` (front) / `DS:1C48:1C4A` (back); SR1 `DS:02EA / 02E6`). n = 0: remember the pair if not yet saved (`if !(DS:1C4C|DS:1C4E)`), `DS:004C = 1`. n = 1: restore both pages from the saved pair;
VGA: CRTC 13h = 14h, `ega_set_split(-1)`, CRTC 0Ch/0Dh = 0; mirror flag = 1; `status_init(0xBE)` (SR1 called `status_init(0xBE)` only for n = 1). n = 2: VGA: CRTC start address = 0; mirror = 1. n = 3 (single buffer): both pages = the saved
front, mirror = 0, `status_init(VGA ? 0x5A : 0xBE)` and, **new**, `if (DS:B7AE) status_label(DS:4846)` (SR1: `if (DS:8ACA) status_label(DS:49DE)`; `DS:B7AE` is the "race mode" byte, see Globals).

### 1a21:17cc far_memset

= SR1 `far_memset` (0f38:1640), ../StreetRod/port/spec/video.md §2.3; no DS references. `far_memset(p, c, n)`: byte fill (loop, not `rep stos`)

### 1a21:17ee clip_rect

= SR1 `clip_rect` (0f38:1680), ../StreetRod/port/spec/video.md §2.3; no DS references. Desc offsets: stride +0xE, h +2 (SR1 +0x2C / +2); the pixel->byte shift is `DS:AD20`; returns 1 when the clipped w and h are > 0

### 1a21:18e2 wait_vretraces

= SR1 `wait_vretraces` (0f38:177d), ../StreetRod/port/spec/video.md §2.3; DS remap: DS:8236 → DS:AD1C. waits `n` frames: Hercules (`DS:AD1C == -4`) 3BAh bit 7 high then low; otherwise 3DAh bit 3 low then high (SR1 identical). Port: `host_wait_vretrace()` x n

### 1a21:1924 wait_ticks

`far void (int ticks)`

NOT SR1 `font_string_width` (the matcher/symbol file named it so by mistake; the real text-width function is `1a21:1954`, see Handoffs). A metronome delay: `deadline = (i32)DS:1D02 + ticks` (32-bit, `DS:1D02/1D04` = completion time of the previous call);
busy-wait until the 32-bit tick counter `DS:1F66/1F68` (72.8 Hz, SR1 `DS:05F8`) is `>= deadline` (signed compare of the high word, then unsigned low word); then `DS:1D02/1D04 = DS:1F66/1F68` (the counter value read after the wait).
Calls: `wait_ticks(8)`, `wait_ticks(0x2EE)` (title / dancer scenes). The first call after start-up waits against `DS:1D02 = 0`, i.e. returns at once. Port: `wait until host_ticks() >= last + n` and store the new `last`.

### 1a21:198a pal_load

Was believed to be SR1 `pal_reg12_13` (`ega_set_palreg(0x0C, 0x0D)`). SR2: `void pal_load(const u8 *src17)` copies **17 bytes** `src17[0..16]` into the live palette table `DS:1DA6` (only the shadow; the hardware is updated by the next
`pal_normal` (driver slot 8) or `ega_set_palette`). Source tables used by callers: `DS:1DC8` (game default, = SR1 `DS:0440`), `DS:1DDA`, `DS:1DEC`, `DS:1E2E` (copy of the default) - see video.md "Palettes".

### 1a21:19aa pal_shadow_patch

SR1 `pal_shadow_patch` (0f38:181c) set only `DS:0446..0448 = 09 01 03`. SR2: `pal_load(DS:1DDA)` first (table `00 00 08 0D 0F 07 00 00 01 02 09 0C 05 0A 06 0E 00`), then `DS:1DAC = 9, DS:1DAD = 1, DS:1DAE = 3`
(registers 6, 7, 8 of the live table).

### 1a21:19c6 pal_shadow_swap

= SR1 `pal_shadow_swap` (0f38:182c), ../StreetRod/port/spec/video.md §4.9; DS remap: DS:0440 → DS:1DA6; DS:0462 → DS:1DFE. swaps the 17 bytes `DS:1DA6` <-> `DS:1DFE` (SR1 `DS:0440` <-> `DS:0462`)

### 1a21:19f0 gfx_init

`void gfx_init(Desc far *front, Desc far *back)`; `main` calls `gfx_init(371c:0000, 371c:0012)`. SR1 logic (video.md §4.8) with these differences:
(1) `DS:AD10:AD12 = front`, `DS:AD14:AD16 = back`, `DS:AD18 = DS:AD1A = 0` (SR1 `822A/822E/8232/8234`); `DS:AD26 = 1` (mirror, SR1 `8240`); `DS:AD1C = (i8)front->type` (+0x10).
(2) mode tables: Tandy (-6): `AD1E = 1, AD20 = 1, AD22 = 4, AD24 = 0xFFFE, DS:9DC6 = 0x1D06`; EGA/VGA (-2): `AD1E = 4, AD20 = 3, AD22 = 1, AD24 = 0xFFF8, 9DC6 = 0x1D06`, CRTC 0Ch = 0, 0Dh = 0; CGA/Hercules: `1, 2, 2, 0xFFFC, 9DC6 = 0x1D56`, `front->type = 0xFD`.
(SR1 `8238 -> AD1E` planes, `823A -> AD20` pixel->byte shift, `823C -> AD22`, `823E -> AD24` x alignment mask; scheme pointer `787E -> 9DC6`, values 0x03A0/0x03F0 -> 0x1D06/0x1D56.)
(3) text colours: `DS:AD28 = *(u16*)(scheme + 5)`, `DS:AD2A = *(u16*)(scheme + 7)`, `DS:AD2C = *(u8*)(scheme + 9)` (SR1 `memcpy(DS:8242, scheme + 5, 5)`: fg `AD28`, bg `AD29`, fill `AD2A`, line `AD2B`, frame style `AD2C`).
(4) **new UI colour variables** set to constants (VGA/EGA values; CGA overrides the first four): `DS:AD40 = 0xB, AD42 = 0xE, AD44 = 4, AD46 = 5, AD48 = 2, AD4A = 4, AD4C = 5, AD4E = 2, AD50 = 0, AD52 = 0xF, AD54 = 5, AD56 = 0`
(CGA/Hercules: `AD40 = 1, AD42 = 0, AD44 = 1, AD46 = 3`). They replace SR1's `DS:8266/8268` and the 5-entry word cache at `DS:75EC..7618` (not initialised any more); SR1's call `0f38:4bff` (menu reset) is gone.
(5) `(*DS:A0CE)()` = driver slot 8 (`pal_normal`); `DS:B87C = 0`; `DS:AD2D = 1` (UI level, SR1 `8247`).
(6) page table (SR1 `DS:7316/731A/731E/7322`): `DS:9A82 = front`, `DS:9A86 = back`, `DS:9A8A = 371c:0024`, `DS:9A8E = 3720:0000`; VGA: page 2 descriptor (copy of `back`, 9 words) with planes = `A000:4000`, page 3 (`3720:0000`, copy of `back`) with planes = `A000:6000`.
Static descriptors: see video.md "Desc" (segment 371c: page 0 `A000:0000`, page 1 `A200:0000`, page 2 template, 18 bytes each).

### 1a21:1b9a ega_set_palreg

= SR1 `ega_set_palreg` (0f38:1f4b), ../StreetRod/port/spec/video.md §1.1; no DS references. `ega_set_palreg(reg, value)`: value > 7 -> `| 0x10`; stores into the live table `DS:1DA6[reg]`; waits for 3DAh bit 3; INT 10h AX=1000h (reg < 16) / 1001h (overscan); same as SR1

### 1a21:1bee ega_set_palette

= SR1 `ega_set_palette` (0f38:1fa4), ../StreetRod/port/spec/video.md §1.1; DS remap: DS:8248 → DS:AD2E. `ega_set_palette(u8 *p16)`: builds 17 bytes (values > 7 | 0x10, overscan byte `DS:AD2E`) and INT 10h AX=1002h through `int86x`; no retrace wait; same as SR1

### 1a21:1c5a status_init

= SR1 `status_init` (0f38:2022), ../StreetRod/port/spec/video.md §4.9; DS remap: SR1 DS:822A..8258 -> DS:AD10..AD3E (+0x2AE6): front AD10/AD12, back AD14/AD16, mirror AD26 (8240), text colours AD28/AD2A/AD2C (8242/8244/8246), 8250 -> AD36, 8252 -> AD38, 8254 -> AD3A, 8256 -> AD3C; scheme 787E -> 9DC6. `status_init(y)`: `DS:AD38 = y + 1, DS:AD3A = 0xA0, DS:AD36 = 0x58, DS:AD3C = 0`; text colours from scheme 11 (`scheme + 0x37`: `DS:AD28/AD2A/AD2C`); `fill_rect(back, 0, y, 320, 10)`; if mirror: `page_copy_rect(back, front, 0, y, 319, y + 9)`

### 1a21:1f5e gear_label

= SR1 `gear_label` (0f38:22e9), ../StreetRod/port/spec/video.md §4.9; DS remap: DS:0482 -> DS:1E1E; DS:8256 -> DS:AD3C; DS:8258 -> DS:AD3E. `gear_label(i)`: `DS:AD3C = 1` temporarily, `status_print(DS:AD3E, *(u16*)(DS:1E1E + 2*i), 8)`; label pointer table at `DS:1E1E` (SR1 `DS:0482`)

### 1a21:1f8e screen_fill_rect

= SR1 `screen_fill_rect` (0f38:231a), ../StreetRod/port/spec/video.md §4.9; DS remap: DS:822A → DS:AD10; DS:822C → DS:AD12; DS:822E → DS:AD14; DS:8230 → DS:AD16; DS:8240 → DS:AD26; DS:8244 → DS:AD2A. HIDE = `cursor_ctl(-3)` (1a21:2d90), `DS:AD2A = c`, `fill_rect(DS:AD14, x, y, w, h)`, mirror via `page_copy_rect` if `DS:AD26`, restore fill colour, SHOW `cursor_ctl(-1)`

### 1a21:21be show_picture

`void show_picture(int id, int clear_colour)` = SR1 `show_picture` (0f38:2554) except: the picture is fetched with `pic_get(id, 1)` (**low stack**, freed with `arena_pop_low(1)` = 1a21:9b40; SR1 used pool 2 / `arena_pop(1)`), the clear height and the clip height use `DS:1E44`
(origin offset `DS:1E40/1E42`, SR1 `DS:04BE/04C0/04C2`), the picture is drawn with driver slot 0 at (0,0)->(DS:1E40, DS:1E42), then origin reset to 0. HIDE/SHOW = `cursor_ctl(-3 / -1)`.

### 1a21:22a8 show_picture_at

= SR1 `show_picture_at` (0f38:2638); DS remap: DS:04BE → DS:1E40; DS:04C0 → DS:1E42. `show_picture_at(id, x, y, clear)`: `DS:1E40 = x, DS:1E42 = y`, then `show_picture(id, clear)`

### 1a21:22c6 screen_text

`void screen_text(int x, int y, char *s)` = SR1 `screen_text` (0f38:2656) with one extra condition: the mirror copy `page_copy_rect(back, front, x, y, DS:AD32 - 1, y + 7)` is done only if `DS:AD26 != 0 && x < DS:AD32` (`DS:AD32` = end x of the text just drawn, SR1 `DS:824C`; `DS:AD34` = end y).

### 1a21:231c screen_hline

= SR1 `screen_hline` (0f38:26a4), ../StreetRod/port/spec/video.md §4.9; DS remap: DS:822A → DS:AD10; DS:822C → DS:AD12; DS:822E → DS:AD14; DS:8230 → DS:AD16; DS:8240 → DS:AD26

### 1a21:236e screen_save_rect

`Desc far *screen_save_rect(int x, int y, int w, int h, Desc far *dst)` = SR1 `screen_save_rect` (0f38:26f2) with the 0x12-byte descriptor: x aligned with `DS:AD24`, right edge `(~AD24 + w + x) & AD24` clamped to 320, bottom clamped to 200;
`dst == NULL` -> `arena_bitmap_alloc(w', h', 0, 2)` else `bitmap_alloc(w', h', dst has mask, -1, dst)` (mask pointer test: `+0xC` / `+0xA`); copy via driver slot 0 (`[DS:A0AE]`, flags 1) from `DS:AD14` with cursor hidden.

### 1a21:246c screen_put_bitmap

= SR1 `screen_put_bitmap` (0f38:27ef), ../StreetRod/port/spec/video.md §4.9; DS remap: DS:822E → DS:AD14; DS:8230 → DS:AD16. `screen_put_bitmap(bmp, x, y)`: rect `{w, h, 0, 0, x & DS:AD24, y}`, driver slot 0 flags 1 to `DS:AD14`, cursor hidden

### 1a21:24d8 screen_put_bitmap_mirror

= SR1 `screen_put_bitmap_mirror` (0f38:2853); DS remap: DS:822A → DS:AD10; DS:822C → DS:AD12; DS:822E → DS:AD14; DS:8230 → DS:AD16; DS:8240 → DS:AD26. `screen_put_bitmap_mirror`: `screen_put_bitmap` then mirror rect `x .. x+w-1, y .. y+h-1` if `DS:AD26`

### 1a21:28dc cursor_shape_offset

= SR1 `cursor_shape_offset` (0f38:2b1c), ../StreetRod/port/spec/video.md §4.9; no DS references. `cursor_shape_offset(n) = n * DS:A2D4` (bytes per pre-shifted shape bank row group = stride << 4)

### 1a21:28e8 cursor_init

= SR1 `cursor_init` (0f38:2b28, video.md §4.10/4.7) with a **smaller bank**: the pointer picture (LIB1 picture 2, `pic_get(2, 2)`) now has **3 shapes of 16 rows (24 x 48)** instead of 4 (24 x 64): bank bitmap `bitmap_alloc(24 << AD20, 0x30, 1, -1, mem)`,
plane stride 0x480 (SR1 0x600), memory = `AD1E * 0x480 + 0x4A4` bytes (+ `(0x18 >> AD20) * 0x10` for non-VGA); the bank is cleared with `bitmap_clear` (1a21:b09e); mask at `bank.mask + 0x480`, save-under descriptor built with `bitmap_init(DS:A2BA...)`
(VGA: planes `A000:1F40`, type FE; others: bank + 0x492). `pic_get` failure -> `out_of_memory` (1a21:6232). The 8 (`~DS:AD24`) pre-shifted copies are made with `bitmap_make_shifted` as before (rect `{24, 0x30, 0, 0, 24*k, 0}` into the bank, flags 3), temporaries via `arena_bitmap_alloc(24, 0x30, 1, 1)` / `arena_pop_low(1)`.
Afterwards `arena_reset()` (1a21:9b0e; SR1 `0f38:a054`), `DS:A2CA..A2D0 = bank planes/mask pointers`, VGA: the mask is inverted word by word (`size >> 3` words), `DS:A2D4 = stride << 4`, `DS:A2D6 = 1` (shape), `DS:A2D2 = cursor_shape_offset(1)`, `DS:A2DE = -1`,
cursor position `DS:3B32 = 0x9E, DS:3B34 = 0x80`, `DS:3B4E = DS:3B50 = 0`, `DS:AD2D = 2`. Cursor state remap (SR1 -> SR2): `7AEC -> A2BA` (save desc), `7AF0 -> A2BE` (save w) ... `7B12 -> A2E0` (constant delta +27CE), `472A/472C -> 3B32/3B34`.

### 1a21:2b44 cursor_redraw

`void cursor_redraw(int x, int y)` = SR1 `cursor_redraw` (0f38:2e8b, spec §4.10) plus the **race-mode page pair**: if `DS:B7AE != 0` (race / event screen active) the pointer is drawn into `{front, back} = VGA: (DS:AD0C:AD0E, DS:ADA2:ADA4), else (DS:AD10:AD12, DS:AD0C:AD0E)`
instead of `(DS:AD10, DS:AD14)`, `y` is clamped to 0x54 and the mirror flag `DS:AD26` is forced to 1 for the call (restored at the end). Everything else (save-under restore with `ega_latch_copy_to40`, driver slot 4 draw, dirty rect union, mirror copy through `page_copy_rect`) is unchanged;
the restore on VGA now passes the page-B pointer arithmetic `planes + (x_save >> 3) + y_save * 0x28`. State variables remapped as in `cursor_init`.

### 1a21:3e84 fill_rect

= SR1 `fill_rect` (0f38:4302), ../StreetRod/port/spec/video.md §3; DS remap: DS:767C → DS:9BC0; DS:787A → DS:9DC2; DS:8244 → DS:AD2A. `fill_rect(d, x, y, w, h)`: `DS:9DC2/9BC0 = d.planes.seg/off` (line target, SR1 `DS:787A/767C`), `h` times driver slot 12 (`[DS:A0DE]`, line) in colour `DS:AD2A`; w/h clipped to d

### 1a21:3f43 vline

= SR1 `vline` (0f38:439c), ../StreetRod/port/spec/video.md §3; DS remap: DS:767C → DS:9BC0; DS:787A → DS:9DC2; DS:8245 → DS:AD2B. `vline`: slot 12 in colour `DS:AD2B`

### 1a21:3fba hline

= SR1 `hline` (0f38:43ff), ../StreetRod/port/spec/video.md §4.9; DS remap: DS:8244 → DS:AD2A; DS:8245 → DS:AD2B. `hline(d,x,y,w) = fill_rect(d,x,y,w,1)` with `DS:AD2A` temporarily = `DS:AD2B`

### 1a21:3ff6 draw_frame3

= SR1 `draw_frame3` (0f38:4432), ../StreetRod/port/spec/video.md §3; DS remap: DS:8244 → DS:AD2A; DS:8245 → DS:AD2B; DS:8246 → DS:AD2C. `draw_frame3`: style `DS:AD2C` (0 none, 1 single, 2 double, 3 solid); same sequence of fill_rect/vline/hline as SR1

### 1a21:41f0 draw_box1

= SR1 `draw_box1` (0f38:45d7), ../StreetRod/port/spec/video.md §4.9; no DS references. `draw_box1`

### 1a21:6ac6 pic_info

`void pic_info(int id, int *w, int *h, int *masked)` = SR1 `pic_info` (0f38:67ff) with the 0x22-byte entries of segment `DS:86E2`: `*w = entry+0`, `*h = entry+2`, `*masked = (u8) entry+5` (SR1 stride-48 entries, flag at +6).

### 1a21:6b04 pic_get

`Desc far *pic_get(int id, int pool)` = SR1 `pic_get`: `pic_info`, then `arena_bitmap_alloc(w, h, masked, pool)` (**pool -1 = second arena** `DS:9E6C`, 0..3 = arena 1 modes), then `pic_decode(id, bitmap)`.

### 1a21:7f5c ega_blit_4planes

= SR1 `ega_blit_4planes` (0f38:8126), ../StreetRod/port/spec/video.md §4.9; no DS references. `ega_blit_4planes`: per plane SEQ2 = 1<<p, GC4 = p, `rows_mask_or` (3345:000c); the row stride argument is the descriptor stride (0x28 on screen)

### 1a21:7fc6 ega_vram_cache_strips

= SR1 `ega_vram_cache_strips` (0f38:819a), ../StreetRod/port/spec/video.md §3; DS remap: DS:731E → DS:9A8A. `ega_vram_cache_strips`: caches car strips in VRAM page `DS:9A8A` from row 46h; strip copies with `ega_save_rect` (destination stride 0x28)

### 1a21:80ec strip_cache_ram

Non-VGA car strip cache: builds RAM strip bitmaps in the 18-byte records at DS:977E from a car description (EGA/VGA use ega_vram_cache_strips); called only when DS:AD1C is not -2. not ported (VGA only).

### 1a21:9a68 arena_bitmap_alloc

`Desc far *arena_bitmap_alloc(int w, int h, int flags, int pool)` = SR1 `arena_bitmap_alloc` (0f38:9fce) with the 0x12-byte descriptor: size = `0x14 + (flags ? plane : 0) + ((flags & 2) ? 0 : plane * DS:AD1E)` (`plane = (((w-1) >> DS:AD20) + 1) * h`);
`pool == -1` (**new**) allocates from the second arena: `p = DS:9E6C + DS:79E0; DS:79E0 += size`, segment `DS:9E6E` (a bump region that is never popped; `shp_load_set` appends the shape sets behind it); otherwise `p = arena_alloc(size, pool)` in arena 1.
Then `d->planes = (p + 0x12, seg)` and `bitmap_alloc(w, h, flags, 0xFFFF, d)` (drv byte FF, no allocation).

### 1a21:9bec rect_intersect

= SR1 `rect_intersect` (0f38:a0fa), ../StreetRod/port/spec/video.md §4.7; DS remap: DS:823E → DS:AD24. `rect_intersect(a, b, out)`: {x0,y0,x1,y1}; x snapped with `DS:AD24`; NULL when empty

### 1a21:9c74 rect_clip_to_view

= SR1 `rect_clip_to_view` (0f38:a198), ../StreetRod/port/spec/video.md §4.9; DS remap: DS:825A → DS:AD58; DS:825E → DS:AD5C; DS:8260 → DS:AD5E; DS:8262 → DS:AD60. `rect_clip_to_view`: view rect `DS:AD5C/AD5E/...` (SR1 `DS:825A..8262`)

### 1a21:9ccc rect_clip_make

= SR1 `rect_clip_make` (0f38:a1ee), ../StreetRod/port/spec/video.md §1.3; DS remap: DS:823E → DS:AD24. `rect_clip_make`

### 1a21:9d38 sprite_scene_setup

`Anim *sprite_scene_setup(int x, int y, Desc far **sprites, Desc far *bgbufs, int n, int a, int b, Desc far *bg, int c, int d)`: SR1 `sprite_scene_setup` (0f38:a273, spec §4.9 "sprite_anim_*") with these differences:
**stores one parameter fewer** (SR1 kept a second overlay rect in `DS:7D24`), scene state in new addresses: `DS:A550 = x`, `DS:A552 = y + 1 - sprites[0]->h` (SR1: `y - (h - 1)`), `DS:A546/A548 = a, b`, `DS:A54A/A54C/A54E` = the three following parameters;
per-sprite 12-byte records at `DS:9D1A + 12*i` (word 0/1 = the sprite descriptor far pointer taken from `sprites[i]`; word 2 = background/save buffer pointer: 0 if `bgbufs == NULL`, else VGA `bgbufs + 8*i`, other drivers `DS:977E + 18*i`; word 3 = pointer into the record table; the last words cache the two words `*(rec[3] + 6)` and `*(rec[4] + 8)` = the sprite's plane pointer words), terminated by a record whose first word is `DS:9D1A`;
page ring at `DS:A5FC` (16-byte `PageNode`s: page far ptr `+0`, last rect `+4..+0xA` = {x0, y0, x1, y1} (initialised to `{0, 0, -1, -1}` = empty), `+0xC` page number for `ega_show_page` (the node index), `+0xE` next node (ring); VGA builds two nodes from the page table `DS:9A82/9A86`, otherwise one from `DS:AD10`); the nodes are linked in a ring; node pointer `DS:A556`, scene pointer `DS:A544`, callback id `DS:A554 = 0x9B16` (SR1 `0x764E`).
Returns `&DS:A544`. (Layout inferred from the code; the fields that the garage/event code reads are listed in their specs.) The scene header is cleared through `rect_clip_make(sprite.w, sprite.h, A550, A552, page.w, page.h, &A600)`.

### 1a21:9f42 rect_subtract_edge

`Rect4 *rect_subtract_edge(Rect4 *a, Rect4 *b, Rect4 *out, int vertical)` = SR1 `rect_subtract_edge` (0f38:a4b9) with the **flag polarity inverted**: SR2 `vertical != 0` takes the y-edge branch (compare `a.y0 / a.y1` with `b.y0 / b.y1`), `vertical == 0` the x-edge branch;
SR1 took the y-edge branch for 0. Result handling unchanged: `out = b` minus the part covered by `a`; if the result is empty, `out = {1000, 1000, -1, -1}` and NULL is returned (non-empty: returns `out`). Callers pass the frame mode byte (`frame[3]` in SR2) accordingly.

### 1a21:a2f2 slide_sprite_generic

`far void (int top, int bottom, int x, int from, int to, Desc far *sprite, ...)`

Driver slot 11 for CGA and Tandy only (EGA/VGA use `ega_slide_sprite`, slot 11 = 2f32:2ab2) - **not ported (VGA only)**. Matches SR1 `0f38:a87e` (which was never named): a software slide built on `screen_save_rect`, `sprite_scene_setup`, `anim_phase_set` and driver slot 9.

### 1a21:a4bc sprite_anim_advance

= SR1 `sprite_anim_advance` (0f38:aa37) with the **new `Anim` layout** (below) and renamed globals: `DS:B866 = anim.x` (SR1 `DS:8B7C`), `DS:1BCC/1BCE = sprite->w/h` (SR1 `DS:0288/028A`). Steps unchanged: `--*frame.count`; if zero, `frame += 4`; if `*frame != -1`: `x += (i8) frame[2]`, `y += (i8) frame[3]`, and if `|(i8) frame[2]|` is non-zero,
`sprite = *(sprite + ((|dx| & ~2) >> 1) + 6)` (next frame descriptor pointer in the sprite list). Frame record: `{u16 count, i8 dx, i8 dy}`, list terminated by `count == -1`. SR1's `(|dx| >> 1) & 0xFFFE` and SR2's `(|dx| & 0xFFFD) >> 1` are the same value.
`Anim` (SR2): `+00 sprite (near ptr to far ptr)`, `+02 bgsrc far`, `+06 bg far`, `+0A overlay (one Rect4 ptr; SR1 had two at +0A/+0C)`, `+0C x`, `+0E y`, `+10 frame ptr`, `+12 PageNode ptr`.

### 1a21:a528 sprite_hspan_clip

`int sprite_hspan_clip(int x, int w, Anim *a, Rect *out)` = SR1 `sprite_hspan_clip` (0f38:aac2) with `anim.x` at `+0xC` (SR1 `+0xE`), alignment `DS:AD24` and **one strictness change**: the test is `(sprite_x_aligned < x + w) && (x < sprite_x_aligned + sprite.w)`
(SR1: `x <= sprite_x_aligned + sprite.w`). Result unchanged: `out.sx = max(x - sprite_x, 0)`, `out.dx = max(x, sprite_x)`, `out.w = ((min(x + w, sprite_x + sprite.w) + ~AD24) & AD24) - out.dx`; returns 1 / 0.

### 1a21:a59c bitmap_copy_into

= SR1 `bitmap_copy_into` (0f38:ab50), ../StreetRod/port/spec/video.md §4.9; no DS references. `bitmap_copy_into(src, dst, flags)`: `bitmap_alloc` header like src, then full copy through driver slot 0 (`[DS:A0AE]`)

### 1a21:a624 bitmap_opaque_extent

= SR1 `bitmap_opaque_extent` (0f38:abc2), ../StreetRod/port/spec/video.md §4.9; DS remap: DS:823C → DS:AD22. `bitmap_opaque_extent`: stride at +0xE, mask ptr +0xA/+0xC; shift `DS:AD22`

### 1a21:a758 bitmap_make_shifted

= SR1 `bitmap_make_shifted` (0f38:acbf), ../StreetRod/port/spec/video.md §4.9; DS remap: DS:823A → DS:AD20. `bitmap_make_shifted(src, shift, dst)`: copy shifted right by `shift & 7` pixels (width +8), mask padded with 1; planes loop over `DS:AD1E` planes; Desc offsets as above

### 1a21:a9ac bitmap_blit_masked_shift

= SR1 `bitmap_blit_masked_shift` (0f38:af50), ../StreetRod/port/spec/video.md §4.9; DS remap: DS:823E → DS:AD24. `bitmap_blit_masked_shift(src, dst, rect, and_mask)`: pre-shift with `bitmap_make_shifted` when `rect.sx & 7`, driver slot 2 (`[DS:A0B6]`); the temporary is allocated with `arena_bitmap_alloc(w + 8, h, mask, 2)` and freed with `arena_pop(1)` = 1a21:9b56

### 1a21:ab52 bitmap_composite_behind

= SR1 `bitmap_composite_behind` (0f38:b0f2), ../StreetRod/port/spec/video.md §4.9; DS remap: DS:5AA6 → DS:79E2; DS:823A → DS:AD20; DS:823E → DS:AD24. `bitmap_composite_behind(src, dst, x, y)`: per plane `dst |= ~smask & src & dmask`, then `dmask &= smask` unless `DS:79E2` (SR1 `DS:5AA6`)

### 1a21:ad48 bitmap_blit_at

`far void (Desc far *dst, Desc far *src, int x, int y, int flags)`

= what SR1 `bitmap_blit_at` (0f38:b31f) did (the earlier match to 0001:a4aa was wrong): driver slot 0 (`[DS:A0AE]`, `blit`) with source `src` (arguments in order `dst_off, dst_seg, src_off(near ptr to far), src_seg, x, y, flags`), rect `{src.w, src.h, 0, 0, x, y}`.

### 1a21:ad92 pic_blit_at

= SR1 `pic_blit_at` (0f38:b360): `Desc *p = pic_get(id, 2); bitmap_blit_at(dst, p, x, y, flags 3); DS:985A:9858 = p; arena_pop(1)`. SR2 calls `bitmap_blit_at` 1a21:ad48 and `arena_pop` 1a21:9b56; the picture pointer cache is at `DS:9858` (far).

### 1a21:ade2 bitmap_blit_masked_at

`far void (Desc far *dst, Desc far *src, int x, int y, int flags)`

Same as `bitmap_blit_at` but through driver slot 2 (`[DS:A0B6]`, `blit_masked`, mode = `flags`): rect `{src.w, src.h, 0, 0, x, y}`. Used for sprites with a mask (stickers, car parts).

### 1a21:ae2c pic_draw_masked

= SR1 `pic_draw_masked` (0f38:b3b4), ../StreetRod/port/spec/video.md §4.9; no DS references. `pic_draw_masked(d, id, x, y, flag)`: temporary `pic_get` + `bitmap_blit_masked_shift` + `arena_pop(1)` (1a21:9b56)

### 1a21:ae94 mask_and_rect

= SR1 `mask_and_rect` (0f38:b41b); no DS references. `mask_and_rect`: ANDs the source mask into the destination mask over the clipped rect (mask ptr +0xA/+0xC, stride +0xE)

### 1a21:af7a far_memcpy

= SR1 `far_memcpy` (0f38:b502), ../StreetRod/port/spec/video.md §4.9; no DS references. `far_memcpy(dst, src, n)` -> `movedata`

### 1a21:af96 bitmap_extract

= SR1 `bitmap_extract` (0f38:b51e), ../StreetRod/port/spec/video.md §4.9; no DS references. `bitmap_extract(src, rect, pool)`: new bitmap (+mask if src has one) from a sub-rect via slot 0 flags 3

### 1a21:b028 page_copy_rect

Changed (SR1 `page_copy_rect`, 0f38:b5aa, ../StreetRod/port/spec/video.md §4.7); DS remap: DS:823E -> DS:AD24; DS:78A2 -> DS:A0AE. `far void page_copy_rect(Desc far *src, Desc far *dst, int x0, int y0, int x1, int y1)` (args `[bp+6]`/`[bp+8]` src, `[bp+0A]`/`[bp+0C]` dst, then x0, y0, x1, y1; inclusive rectangle). Difference to SR1: only the upper clamps remain (`x1 > 0x13F -> 0x13F`, `y1 > 0xC7 -> 0xC7`); the SR1 lower clamps (`x0 < 0`, `x1 < 0`) are gone. `m = DS:AD24`; rect `{ w = ((~m + x1 + 1) & m) - (x0 & m), h = y1 - y0 + 1, sx = x0, sy = y0, dx = x0, dy = y0 }`; then `[DS:A0AE](src, dst, &rect, 1)` (driver slot 0, flags 1). Callers: game_flow / garage / race screens (`page_copy_rect(back, front, ...)`).

### 1a21:b09e bitmap_clear

`far void (Desc far *d)`

Clears a bitmap: `far_memset(d->planes, 0, d->size)` and, if the bitmap has a mask, `far_memset(d->mask, 0xFF, d->size / DS:AD1E)` (size of one plane; mask 1 = transparent). New (SR1 relied on the allocator not clearing).
Also called by `driver_select` and `cursor_init`.

### 1a21:b0f6 pic_get_padded

`far Desc far * (int id, int pool)`

Returns a masked bitmap `arena_bitmap_alloc(w + 8, h, 1, pool)` containing picture `id` at (0,0) with 8 extra transparent columns: `p = pic_get(id, 2)`; `d = arena_bitmap_alloc(p.w + 8, p.h, 1, pool)`; `bitmap_clear(d)` (planes 0, mask FF);
`bitmap_blit_at(d, p, 0, 0, 3)`; `arena_pop(1)` (frees `p`). Result is ready for sub-byte shifting. Callers (2925 car-compose code) pass pool 1 (low stack).

### 26b3:000c pic_unpack

`void pic_unpack(u8 far *src, u8 far *dst, int n, u8 *tokens, int ntok)` (far, hand-written; SR1 `0e92:0006` returned a count in AX, SR2 returns nothing). Input = `n` bytes of packed data (without the leading BCh). Tables in DGROUP: `DS:4B72` run **counts** (16 bytes), `DS:4B82` run **values** (16 bytes) (SR1 `DS:4EEE / 4EFE`).
Setup: 256-byte reverse map `rev[tokens[i] & 0xFF] = i + 1` (i < ntok = 16, zero-filled first); `esc0 = tokens[0]`, `esc1 = tokens[1]`.
Loop while `n > 0`: `b = *src++; n--`; **if `b == 0x00 || b == 0xFF`**: `c = *src++; n--`; `c != 0` -> store `c + 1` copies of `b`; **`c == 0` (new in SR2)** -> store ONE byte: `esc0` for `b == 0x00`, `esc1` for `b == 0xFF` (the two token bytes that would otherwise stand for the useless runs 1x00 / 1xFF - the packer escapes a literal token byte as `00 00` / `FF 00`);
else if `rev[b] != 0` -> run `k = rev[b] - 1`: store `counts[k]` copies of `values[k]`; else store `b`. (`tools/srlib.py` still decodes `00 00` as `00` and `FF 00` as `FF`: 13 / 5 / 6 occurrences in LIB1 / LIB2 / LIB3, so a handful of bytes differ.)

### 26c0:0002 tdy_draw_cursor

Tandy slot 4 (cursor blit). not ported (VGA only).

### 26c0:00c1 cga_draw_cursor

CGA/Hercules slot 4 (cursor blit). not ported (VGA only).

### 26c0:0175 ega_draw_cursor

`void ega_draw_cursor(FarPtr img, FarPtr mask, FarPtr save, FarPtr scr, int xb, int y, int wb, int rows)` slot 4 (hand-written). = SR1 `ega_draw_cursor` (0e6c:0175, video.md §4.7) except the **plane stride of the pointer bank**: the image planes are `0x480` apart
(24 bytes x 48 rows; SR1 `0x600`), i.e. plane offsets `s, s + 0x480, s + 0x900, s + 0xD80` (SR1 `s, +0x600, +0xC00, +0x1200`). Everything else (save-under into VRAM `A000:1F40` through the latches, row pitch 0x28, SEQ2/GC8 sequence) is identical.

### 2719:000a cga_helper_000a

CGA/Hercules pixel helper (shared with the Tandy blits). not ported (VGA only).

### 2719:003a cga_helper_003a

CGA/Hercules pixel helper (shared with the Tandy blits). not ported (VGA only).

### 2719:00c2 cga_helper_00c2

CGA/Hercules pixel helper (shared with the Tandy blits). not ported (VGA only).

### 2719:01a6 cga_install_vectors

Copies the 20-pointer table DS:7A40 to DS:A0AE (CGA/Hercules driver select). not ported (VGA only).

### 2719:01b8 cga_draw_text

CGA/Hercules slot 3 (draw_text). not ported (VGA only).

### 2719:0700 cga_recolour_rect

CGA/Hercules slot 6 (recolour_rect). not ported (VGA only).

### 2719:0d02 cga_copy_page

CGA/Hercules slot 5 (copy_page). not ported (VGA only).

### 2719:0d58 cga_blinds

CGA/Hercules slot 10 (blinds). not ported (VGA only).

### 2719:0e58 cga_helper_0e58

CGA/Hercules helper (new in SR2). not ported (VGA only).

### 2719:0e7a cga_pal_black

CGA/Hercules slot 7 (screen off). not ported (VGA only).

### 2719:0ea2 cga_pal_normal

CGA/Hercules slot 8 (screen on). not ported (VGA only).

### 2719:0eca cga_helper_0eca

CGA/Hercules blit helper. not ported (VGA only).

### 2719:0fbb cga_helper_0fbb

CGA/Hercules blit helper. not ported (VGA only).

### 2719:1092 cga_blit_masked

CGA/Hercules slot 2 (blit_masked). not ported (VGA only).

### 2719:115e cga_helper_115e

CGA/Hercules helper (new in SR2). not ported (VGA only).

### 2719:11fe cga_blit

CGA/Hercules slot 0 (blit). not ported (VGA only).

### 2719:1384 cga_helper_1384

CGA/Hercules helper (new in SR2). not ported (VGA only).

### 2719:138c cga_helper_138c

CGA/Hercules helper (new in SR2). not ported (VGA only).

### 2719:13e6 cga_helper_13e6

CGA/Hercules helper. not ported (VGA only).

### 2719:14a8 cga_helper_14a8

CGA/Hercules helper. not ported (VGA only).

### 2719:1566 cga_blit_shifted

CGA/Hercules slot 1 (blit_shifted). not ported (VGA only).

### 2719:1702 cga_helper_1702

CGA/Hercules anim helper (new in SR2). not ported (VGA only).

### 2719:176c cga_helper_176c

CGA/Hercules anim helper (new in SR2). not ported (VGA only).

### 2719:17ca cga_anim_step

CGA/Hercules slot 9 (anim_step; also event tiles 1BC8 == 3/6). not ported (VGA only).

### 2719:1c38 cga_pixel_addr

CGA/Hercules pixel address helper. not ported (VGA only).

### 2719:1c60 cga_set_colour

CGA/Hercules slot 17 (set colour). not ported (VGA only).

### 2719:1c7c cga_line

CGA/Hercules slot 12 (line). not ported (VGA only).

### 2719:1f19 cga_hspan

CGA/Hercules slot 13 (hspan). not ported (VGA only).

### 2719:2040 cga_fill_rows

CGA/Hercules slot 14 (fill_rows). not ported (VGA only).

### 2719:20bd cga_helper_20bd

CGA/Hercules helper (1 instruction: ret). not ported (VGA only).

### 2f32:0002 ega_wait_vretrace

= SR1 `ega_wait_vretrace` (21a0:0006), ../StreetRod/port/spec/video.md §2.1; no DS references. hand-written: wait while 3DAh bit 0 set, then until bit 3 set (no decompile; identical bytes to SR1)

### 2f32:0010 ega_set_split

= SR1 `ega_set_split` (21a0:0014), ../StreetRod/port/spec/video.md §2.1; DS remap: DS:0254 → DS:0200. `ega_set_split(line)`: CRTC line compare (waits for retrace first); -1 = off; VGA adds 100; hand-written, identical code

### 2f32:00a8 tdy_helper_00a8

Tandy blit helper. not ported (VGA only).

### 2f32:019d tdy_helper_019d

Tandy blit helper. not ported (VGA only).

### 2f32:022a tdy_helper_022a

Tandy blit helper. not ported (VGA only).

### 2f32:0364 tdy_blit_masked

Tandy slot 2 (blit_masked). not ported (VGA only).

### 2f32:04a2 tdy_blit

Tandy slot 0 (blit). not ported (VGA only).

### 2f32:0672 tdy_helper_0672

Tandy shift helper. not ported (VGA only).

### 2f32:0700 tdy_helper_0700

Tandy shift helper. not ported (VGA only).

### 2f32:0788 tdy_blit_shifted

Tandy slot 1 (blit_shifted). not ported (VGA only).

### 2f32:0906 tdy_install_vectors

Copies the 20-pointer table DS:7ADC to DS:A0AE (Tandy driver select). not ported (VGA only).

### 2f32:0918 tdy_draw_text

Tandy slot 3 (draw_text). not ported (VGA only).

### 2f32:09da tdy_recolour_rect

Tandy slot 6 (recolour_rect). not ported (VGA only).

### 2f32:0b80 tdy_copy_page

Tandy slot 5 (copy_page). not ported (VGA only).

### 2f32:0b9c tdy_blinds

Tandy slot 10 (blinds). not ported (VGA only).

### 2f32:0c56 tdy_helper_0c56

Tandy anim helper. not ported (VGA only).

### 2f32:0cbe tdy_anim_step

Tandy slot 9 (anim_step). not ported (VGA only).

### 2f32:111c ega_install_vectors

= SR1 `ega_install_vectors` (21a0:1128), ../StreetRod/port/spec/video.md §2.1; DS remap: DS:5BD0 → DS:7B38; DS:78A2 → DS:A0AE. `ega_install_vectors`: copies **20** far pointers (`rep movsw` with CX = 0x28 words = 80 bytes) from `DS:7B38` to the driver vector table `DS:A0AE` (SR1: 15 pointers, 0x1E words, `DS:5BD0` -> `DS:78A2`). The table is listed in video.md "Driver vector table"

### 2f32:112e ega_draw_text

= SR1 `ega_draw_text` (21a0:113a), ../StreetRod/port/spec/video.md §4.5; DS remap: DS:8242 → DS:AD28; DS:8243 → DS:AD29; DS:824C → DS:AD32; DS:824E → DS:AD34. `ega_draw_text(d, x, y, s)` slot 3: font data now at segment 26e6 (`DS:86EA`), glyph `c` at offset `c*9 - 0x11A` (SR1 `c*9 - 0x120` in segment `DS:691A`; the font bytes are identical, only the base moved by 6); colours `DS:AD28` (fg), `DS:AD29` (bg); end x/y -> `DS:AD32/AD34` (SR1 `DS:824C/824E`); stride 0x28

### 2f32:1571 ega_recolour_rect

= SR1 `ega_recolour_rect` (21a0:1508), ../StreetRod/port/spec/video.md §2.1; no DS references. `ega_recolour_rect` slot 6; colour-compare read mode 1, write mode 2; Desc offsets +6/+8 (planes) and +0xE (stride)

### 2f32:17a4 ega_copy_page

= SR1 `ega_copy_page` (21a0:16dc), ../StreetRod/port/spec/video.md §4.4; no DS references. `ega_copy_page` slot 5: latched copy of 8000 bytes (`movedata` count 0x1F40)

### 2f32:17f5 ega_blinds

= SR1 `ega_blinds` (21a0:1722), ../StreetRod/port/spec/video.md §4.4; DS remap: DS:05F8 → DS:1F66; DS:05FA → DS:1F68; DS:5C0C → DS:7B88. `ega_blinds` slot 10: 5-pass interleaved latched page copy, >= 3 ticks per pass; pass offsets table `DS:7B88` (SR1 `DS:5C0C`); row length 0x28

### 2f32:1932 ega_set_colour

`far void (int colour)`

Driver slot 17 (EGA/VGA). `out 3CEh, 0; out 3CFh, colour` = **Graphics Controller Set/Reset register (index 0) := colour** (4-bit colour for the planes enabled in index 1). Used by the road renderer before the span primitives of slots 13/14 and the polygon fill (slot 18).
CGA equivalent `2719:1c60`, Tandy `3236:0371`. Port: `gc.set_reset = colour & 0x0F`.

### 2f32:1956 ega_show_page

= SR1 `ega_show_page` (21a0:182e), ../StreetRod/port/spec/video.md §2.1; no DS references. `ega_show_page(page)`: CRTC start address = page * 0x2000 (regs 0Ch/0Dh), then wait for vertical retrace

### 2f32:198a ega_blit

= SR1 `ega_blit` (21a0:1862), ../StreetRod/port/spec/video.md §4.4; no DS references. `ega_blit(src, dst, rect, flags)` slot 0; the unrolled loops of the decompile are rewritten but the algorithm (clip_rect, `ega_latch_copy` for screen->screen, per-plane `movedata` otherwise, flags bit0 planes / bit1 mask) is the same; row length constant 0x28

### 2f32:1bf6 ega_sprite_over_bg

= SR1 `ega_sprite_over_bg` (21a0:1ae9), ../StreetRod/port/spec/video.md §4.4; no DS references. `ega_sprite_over_bg`: per plane `dst = (bg & mask) | sprite`, optional latched tail rows; calls `rows_composite` (32a5:0002)

### 2f32:1dd8 ega_blit_masked

= SR1 `ega_blit_masked` (21a0:1cd5), ../StreetRod/port/spec/video.md §4.4; no DS references. `ega_blit_masked(src, dst, rect, mode)` slot 2: mode 0 -> `rows_mask_blend` (33e5:0002), mode != 0 -> `rows_mask_or` (3345:000c); destination stride 0x28 on screen pages (type FEh)

### 2f32:1f32 ega_save_rect

= SR1 `ega_save_rect` (21a0:1e2d), ../StreetRod/port/spec/video.md §4.4; no DS references. `ega_save_rect(src, w, h, dst, src_stride)`: latched copy into a packed VRAM buffer

### 2f32:1fbf shift_row

`void shift_row(u8 *row, int s, int n)` (near). Result as SR1 (shift the n-byte row right by `s` bits for `s > 0`, left by `-s` for `s < 0`, carrying bits across bytes; `s == 0` or `n <= 0`: nothing) but **re-implemented**:
right shift: `m = DS:7B91[s]` (left masks `00 80 C0 E0 F0 F8 FC FE FF`, SR1 `DS:5C15`), `carry = 0`; for i in 0..n-1: `w = row[i] << (8 - s)` (16-bit); `row[i] = ((w >> 8) & ~m) | carry; carry = (u8) w`; left shift (`s' = -s`, last byte first): `m = DS:7B91[8 - s']`, `carry = 0`; for i = n-1 down to 0: `v = rol8(row[i], s')`; `row[i] = (v & m) | carry; carry = v & ~m`.
Equivalent bit-for-bit to the SR1 rol/ror form for any (s, n). **The caller's length changed**: `ega_blit_shifted` passes `n = max(src_bytes, dst_bytes)` (below), SR1 passed `(w + 7) / 8`.

### 2f32:2068 ega_put_shifted_row

`void ega_put_shifted_row(FarPtr d, u8 *buf4, int w, int bit)` (near; **no `shift` parameter**, the caller shifts first). Same write sequence as SR1 (spec §4.4): `buf4` holds 4 plane rows of 0x28 bytes (`buf+0x00/0x28/0x50/0x78`); if `bit != 0`: left partial byte with mask `m = ~DS:7B91[bit]` (and `& DS:7B91[bit + w]` when `bit + w < 8`), GC8 = m, for k in 0..3: SEQ2 = 1<<k, dummy read of `*d` (latch load), write `buf[k] & m`; `d++`; `w -= 8 - bit`;
then GC8 = FFh and `w / 8` full bytes (4 planes each, `d++` after plane 3); then `w % 8 > 0`: mask `DS:7B91[w]`, 4 planes, **no** `d++`; finally GC8 = FFh, SEQ2 = 0Fh. The masks table is `DS:7B91` (SR1 `DS:5C15`).

### 2f32:230f ega_blit_shifted

`void ega_blit_shifted(Desc far *src, Desc far *dst, Rect *r)` slot 1 (unclipped memory -> screen blit at any x). Same as SR1 (§4.4) with these differences: Desc offsets (planes +6/+8, stride +0xE, size +4);
`src_bytes = ((sx + w - 1) >> 3) - (sx >> 3) + 1` and `dst_bytes = ((dx + w - 1) >> 3) - (dx >> 3) + 1` (abs-value idiv by 8, i.e. truncating division; SR1 computed only `src_bytes`); per row: copy `src_bytes` bytes of each of the 4 planes (`plane k` at `+ (size >> 2) * k`) into `buf[k*0x28 + i]`;
`shift = (dx & 7) - (sx & 7)`; if `shift != 0`: `shift_row(buf + 0x28*k, shift, max(src_bytes, dst_bytes))` for k = 0..3; then `ega_put_shifted_row(dst_ptr, buf, w, dx & 7)`; advance `src += src.stride`, `dst += dst.stride`; GC5 = 0 (write mode 0) set once at the start. (SR1 shifted `(w+7)/8` bytes inside the put routine.)

### 2f32:2540 ega_line

= SR1 `ega_line` (21a0:23bc), ../StreetRod/port/spec/video.md §2.1; DS remap: DS:5C1E → DS:7B9A. `ega_line(x0, y0, x1, y1, colour)` slot 12: Bresenham with set/reset into segment `DS:9DC2` (SR1 `DS:787A`), byte stride 0x28; hand-written, identical code (the decompile shows different jump-table names only)

### 2f32:26b2 ega_pixel_addr

= SR1 `ega_pixel_addr` (21a0:252e), ../StreetRod/port/spec/video.md §2.1; DS remap: DS:787A → DS:9DC2. `BX = x, AX = y -> BX = (x >> 3) + y*0x28, ES = DS:9DC2, CL = 7 - (x & 7), AH = 1`

### 2f32:26ec anim_restore_rect

`void anim_restore_rect(Rect4 *r, Desc far *page, Desc far *bgsrc, int mode)` (near). As SR1 (`21a0:2568`) but the **`mode` polarity is inverted**: SR2 `mode == 0` -> `back = r->y0 * 0x28` (vertical background origin); `mode != 0` -> `back = r->x0 >> 3`
(SR1: mode 0 -> x). Then `ega_latch_copy({bgsrc.planes.seg, bgsrc.planes.off - back}, page.planes, (x1>>3)-(x0>>3)+1, y1-y0+1, x0>>3, y0, page.stride)`. Desc offsets +6/+8 (planes), +0xE (stride).

### 2f32:2754 ega_anim_step

`int ega_anim_step(Anim *a)` slot 9. Structure as SR1 (video.md §4.7) with these differences: (1) **`Anim` layout** (sprite +0, bgsrc +2, bg +6, overlay +0xA (single), x +0xC, y +0xE, frame +0x10, node +0x12); the frame record is `{u16 count, i8 dx, i8 dy}` and its `mode` byte is `frame[3]`
in this version's `rect_subtract_edge(&box, &node->rect, &dirty, mode)` call (polarity inverted, see 1a21:9f42 / anim_restore_rect); (2) the sprite height reduction is `DS:1C08` (SR1 `DS:02D0`); (3) the per-tile loop is selected by the **event id `DS:1BC8`** (SR1 `DS:0284 == 6`):
`1BC8 == 6`: 6 tiles from `DS:50CC` (pairs `{x, w}`), `sprite_hspan_clip(tile.x, tile.w, a, &r)` then `ega_sprite_over_bg(sprite, node.page, bg, tail, &r)` for each hit; **`1BC8 == 3` (new, event 15)**: 3 tiles from `DS:50E4` with the same call, and after each tile `event15_overlay(node.page, i)` (`2925:1cae`, i = 0,1,2) plus a final `event15_overlay(node.page, 3)`;
otherwise a single `ega_sprite_over_bg` with tail `{w = sprite.w, 0, x...}`. (4) only **one** overlay rect (`a->overlay`) is intersected (`rect_intersect`) and redrawn with `ega_blit_masked(bg, page, &r, 0)`; (5) the end-of-animation branch (`*frame < 0`): `ega_latch_copy(node.page.planes, next.page.planes, 0x28, 0xBE, 0, 0, 0x28)` and if `node.show` copy the rect of `next.next` and `ega_show_page(0)`; returns 0 / 1.
`sprite_anim_advance` (1a21:a4bc) is called before `ega_show_page(node.show)`.

### 2f32:2ab2 ega_slide_sprite

`void ega_slide_sprite(int top, int bottom, int x, int from, int to, Desc far *sprite, Desc far *alt)` slot 11 - one more parameter than SR1 (`21a0:28ec`): `alt` (may be NULL) is a second frame of the same size; when non-NULL the sprite **alternates every frame** between `sprite` and `alt`
(starting with `sprite`; planes pointer `+6/+8` and mask pointer `+0xA` swapped). Other differences: the page is `DS:AD14` (back) and the mirror test `DS:AD26` (SR1 `DS:822E/8240`); the old optional overlay blit through slot 2 (`DS:0792`/`DS:70AC`) in the loop is gone; per frame: restore the window with `ega_latch_copy_to40`,
draw the 4 planes with `rows_mask_or` (GC4 / SEQ2 per plane), SEQ2 = 0Fh, and, if mirror: `ega_wait_vretrace(); ega_latch_copy(back -> front, ...); ega_wait_vretrace()`. Save area still `back.planes + 0x2AF0` (A000:4AF0 in page 2). The make_mask_plane call at the start is gone (masks are prepared by the caller).

### 3210:0000 tdy_pic_helper_0000

Tandy picture decoder helper. not ported (VGA only).

### 3210:005a tdy_pic_unpack_a

Tandy picture decoder (planes). not ported (VGA only).

### 3210:0120 tdy_pic_unpack_b

Tandy picture decoder (mask only). not ported (VGA only).

### 3210:01ab tdy_pic_helper_01ab

Tandy picture decoder helper. not ported (VGA only).

### 3210:01f4 tdy_pic_helper_01f4

Tandy picture decoder helper. not ported (VGA only).

### 3210:023c tdy_pic_make_mask

Tandy mask builder. not ported (VGA only).

### 3236:000c tdy_pixel_addr

Tandy pixel address helper. not ported (VGA only).

### 3236:0038 tdy_line

Tandy slot 12 (line). not ported (VGA only).

### 3236:025f tdy_hspan

Tandy slot 13 (hspan). not ported (VGA only).

### 3236:0307 tdy_fill_rows

Tandy slot 14 (fill_rows). not ported (VGA only).

### 3236:0371 tdy_set_colour

Tandy slot 17 (set colour). not ported (VGA only).

### 326e:000a tdy_blit_helper_000a

Tandy blit helper. not ported (VGA only).

### 326e:00d3 tdy_blit_helper_00d3

Tandy blit helper. not ported (VGA only).

### 3288:000c tdy_blit_helper_000c

Tandy blit helper. not ported (VGA only).

### 3288:004f tdy_blit_helper_004f

Tandy blit helper. not ported (VGA only).

### 3298:0000 ega_latch_copy

= SR1 `ega_latch_copy` (24e7:0008), ../StreetRod/port/spec/video.md §4.4; no DS references. `ega_latch_copy(src, dst, wb, h, xb, y, stride)`: write mode 1 latched copy; identical hand-written code

### 3298:0083 ega_latch_copy_to40

= SR1 `ega_latch_copy_to40` (24e7:008b), ../StreetRod/port/spec/video.md §4.4; no DS references. `ega_latch_copy_to40(src, wb, h, dst, sstride)`: destination stride 0x28; identical code (the 7 extra instructions in the diff are the epilogue that the SR1 function-boundary guess cut off: `out 3CEh,5 / out 3CFh,0` resets write mode 0)

### 32a5:0002 rows_composite

= SR1 `rows_composite` (24f4:000a), ../StreetRod/port/spec/video.md §4.4; no DS references. `(mask & bg) | src` per word, unrolled; same code, the `cs:` working variables moved from `cs:[0x22..0x28]` to `cs:[0x1A..0x20]` and the jump base from 0x91 to 0x89 (the SR1/SR2 instruction counts differ only because the SR1 index extended the function over the next one)

### 3345:000c rows_mask_or

= SR1 `rows_mask_or` (2595:0004), ../StreetRod/port/spec/video.md §4.4; no DS references. `(mask & dst) | src` per word, unrolled, right to left; same code (dispatch constants 0x535/0x84 -> 0x53D/0x8C)

### 33e5:0002 rows_mask_blend

= SR1 `rows_mask_blend` (2634:000a), ../StreetRod/port/spec/video.md §4.4; no DS references. `(dst & mask) | (~mask & src)` per word then byte; identical

### 33e5:00b9 make_mask_plane

= SR1 `make_mask_plane` (2634:00c1), ../StreetRod/port/spec/video.md §2.2; no DS references. mask = ~(p0|p1|p2|p3) over `total/4` bytes; identical algorithm (a `loop` epilogue added by the index)

### 33f6:0000 poly_pixel_addr

`near asm (BX = x, AX = y) -> BX, ES, CL, AH`

Helper of the polygon fill (slot 18). `CL = BL; DX:AX = y * DS:81DE` (byte stride, 0x28); `BX = (x >> 3) + AX + word [DS:81DF]` (that word is 0: it overlays the high byte of the stride; a leftover per-page offset); `ES = DS:9DC2` (line/fill target segment, SR1 `DS:787A`);
`CL = (x & 7) ^ 7`; `AH = 1`; `retf` (the callers do `shl ah, cl` to get the pixel mask 0x80 >> (x & 7)). The equivalent EGA helper for lines is `ega_pixel_addr` (2f32:26b2).

### 33f8:0060 poly_edge_setup

`near asm (DI = Edge*)`

Initialises one Bresenham edge walker. `Edge` (0x14 bytes): `+0 vram offset, +2 pixel mask (u8, 0x80 = leftmost pixel of the byte), +4 error, +6 d1, +8 d2, +A remaining steps, +C current vertex ptr, +E next vertex ptr, +10 step routine, +12 next-vertex routine`.
`dx = next.x - cur.x`, `dy = next.y - cur.y` (read through `[DI+0xC]` / `[DI+0xE]`; vertices are `{i16 x, i16 y}`; edges are walked top to bottom so `dy >= 0`). Four cases (`a = |dx|`): `dx >= 0`: if `dy <= dx` (x-major): `count = dx; err = dx - 2dy; d1 = -2dy; d2 = 2dx - 2dy; step = 33f8:0004`;
else (y-major) `count = dy; err = dy - 2dx; d1 = -2dx; d2 = 2dy - 2dx; step = 33f8:001b`. `dx < 0` (`a = -dx`): x-major (`dy <= a`): `count = a; err = a - 2dy; d1 = -2dy; d2 = 2a - 2dy; step = 33f8:0049`; y-major: `count = dy; err = dy - 2a; d1 = -2a; d2 = 2dy - 2a; step = 33f8:0032`.
Step routines (hand-written, `BX` = offset, `AH` = mask, `CX` = error, `DI` = edge; return CF = 1 when the scanline changed): `0004` (x-major, right): `x++` (`ror ah,1`; carry -> `bx++`); if `err < 0`: `bx += DS:81DE` (y++), `err += d2`, CF = 1; else `err += d1`, CF = 0.
`0049` (x-major, left): `x--` (`rol ah,1`; carry -> `bx--`); same error rule. `001b` (y-major, right): `bx += stride` first; if `err < 0`: `x++`, `err += d2` else `err += d1`; CF = 1 always. `0032` (y-major, left): `bx += stride`; if `err < 0`: `x--`, `err += d2` else `err += d1`; CF = 1.

### 33f8:0100 poly_edge_advance

`near asm (DI = Edge*) -> CF`

Walks one edge until the scanline changes. Loads `CX = err, AH = mask, BX = offset` from the edge; loop: `--count`; if `count < 0` -> edge ended: `if (--frame.remaining < 0) { CF = 1; return; }` (frame word `[BP+0x2A]`), else store the state, `cur = next` (`[DI+0xC] = [DI+0xE]`), `next = [DI+0x12]()` (next/previous vertex routine), `poly_edge_setup(DI)`, continue;
otherwise `[DI+0x10]()` (step routine); CF = 1 (scanline changed) -> store `CX, BX, AH` back, **plot the new pixel** (`out 3CEh, (mask << 8) | 8` = GC bit mask; `or es:[bx], al` read-modify-write which, with Set/Reset enabled, writes the Set/Reset colour into the masked pixel), return CF = 0;
CF = 0 -> plot the pixel (same `out` + `or es:[bx], al`) and keep stepping. So every pixel of the edge is plotted (x-major runs included), and the routine returns with the walker on the next scanline.

### 33f8:014a poly_next_vertex

`near asm (SI = vertex ptr) -> SI`

`SI = (SI == [BP+0x28]) ? [BP+0x32] : SI + 4` - next vertex of the polygon with wrap (`[BP+0x28]` = address of the last vertex, `[BP+0x32]` = first).

### 33f8:0157 poly_prev_vertex

`near asm (SI = vertex ptr) -> SI`

`SI = (SI == [BP+0x32]) ? [BP+0x28] : SI - 4` - previous vertex with wrap.

### 33f8:0164 poly_span_fill

`near asm (SI, DI = Edge*)`

Fills the pixels strictly between the two edge walkers on the current scanline. Order the edges so that `DI` is the left one (`[DI] < [SI]`, equal offsets: the one with the larger mask byte `[DI+2]`, since 0x80 is leftmost). Left start: `bh = mask_left - 1` (pixels right of the left edge pixel inside its byte); `di = offset_left`; if `bh` becomes 0, `di++` and `bh = 0xFF`.
Right end: `cx = offset_right - di`; `bl = (-mask_right) << 1` (pixels left of the right edge pixel); if that is 0, `bl = 0xFF` and `cx--`. If `cx < 0` nothing to fill. Set/Reset colour: GC index 0 := `[BP+0x36]` (third argument of `poly_fill`). Then with GC index 8 (bit mask): if `bh != 0xFF`: if `cx <= 0` { `bl &= bh` } else { mask `bh`, `movsb` (write one byte with the latches/Set-Reset), `cx--` };
mask `0xFF` and `rep movsb` for `cx` bytes (bytes copy es:[si] -> es:[di], i.e. a read+write with Set/Reset replacing the data); mask `bl`, `movsb` (right partial byte).

### 33f8:01ca poly_fill

`far void (Point near *pts, int n, int fill_colour, int edge_colour)  /* driver slot 18, [DS:A0F6] */`

**New in SR2** (used only by the road renderer in `0dcb`, e.g. `0dcb:4294`): fills a convex polygon of `n` vertices (`pts[i] = {i16 x, i16 y}`, near pointer into SS/DS, any winding) directly in video memory through the Graphics Controller Set/Reset path. Arguments at `[BP+0x32]` = pts, `[BP+0x34]` = n, `[BP+0x36]` = fill colour, `[BP+0x38]` = edge colour (callers pass the same colour twice). Stack frame 0x2C bytes: `Edge A @ BP+0`, `Edge B @ BP+0x14`, `[BP+0x28]` = &pts[n-1], `[BP+0x2A]` = remaining vertex transitions.
```
if (n <= 0) return;  last = &pts[n-1];
top = vertex with the smallest y (first one on ties);          // loop over all n vertices
(offset, mask) = poly_pixel_addr(top.x, top.y); mask = 1 << ((x&7)^7) = 0x80 >> (x & 7); A.off = B.off = offset; A.mask = B.mask = mask;
A.cur = B.cur = top; A.nextfn = poly_prev_vertex (33f8:0157); B.nextfn = poly_next_vertex (33f8:014a);
A.next = prev(top); B.next = next(top);                         // two walkers go around the polygon in opposite directions
poly_edge_setup(A); poly_edge_setup(B); remaining = n - 2;
GC: index 1 (enable Set/Reset) := 0Fh;  GC index 0 (Set/Reset) := edge_colour; plot the top pixel (GC index 8 (bit mask) := mask; OR into es:[offset]);
loop {
    GC index 0 := edge_colour;                                      // re-armed at the start of every iteration (poly_span_fill changes it)
    if (poly_edge_advance(A)) { poly_edge_advance(B); break; }     // CF=1: out of vertices
    if (poly_edge_advance(B)) { poly_edge_advance(A); break; }
    poly_span_fill(A, B);                                          // interior of this scanline, colour fill_colour
}
GC: index 0 := 0; index 1 := 0 (Set/Reset disabled); index 3 := 0 (rotate/function); index 8 := FFh.   // restore
```
Edge pixels are plotted with `edge_colour`, interior spans with `fill_colour` (the callers always pass equal colours). Port: rasterise the same Bresenham edges (formulas in `poly_edge_setup`) and spans, writing planar pixels with the colour; the draw target is segment `DS:9DC2` with 0x28-byte rows (the visible/back VGA page selected by the caller).

### 3422:009e cga_poly_edge_setup

CGA/Hercules polygon edge setup (slot 18 helper). not ported (VGA only).

### 3422:013e cga_poly_edge_advance

CGA/Hercules polygon edge advance (slot 18 helper). not ported (VGA only).

### 3422:01aa cga_poly_next_vertex

CGA/Hercules polygon vertex step (slot 18 helper). not ported (VGA only).

### 3422:01b7 cga_poly_prev_vertex

CGA/Hercules polygon vertex step (slot 18 helper). not ported (VGA only).

### 3422:01c4 cga_poly_span_fill

CGA/Hercules polygon span fill (slot 18 helper). not ported (VGA only).

### 3422:025d cga_poly_fill

CGA/Hercules slot 18 (poly_fill). not ported (VGA only).

### 3458:00a6 tdy_poly_edge_setup

Tandy polygon edge setup (slot 18 helper). not ported (VGA only).

### 3458:0146 tdy_poly_edge_advance

Tandy polygon edge advance (slot 18 helper). not ported (VGA only).

### 3458:01a0 tdy_poly_next_vertex

Tandy polygon vertex step (slot 18 helper). not ported (VGA only).

### 3458:01ad tdy_poly_prev_vertex

Tandy polygon vertex step (slot 18 helper). not ported (VGA only).

### 3458:0228 tdy_poly_fill

Tandy slot 18 (poly_fill). not ported (VGA only).

### 3488:000e pixel_decimate_planes

`far void (src far, int wb, int h, dst far, u8 row_pat, u8 col_pat, u8 bits)`

Helper of race_render slot 15 (`0dcb:b8c8`, "shrink picture"): copies a 4-plane bitmap keeping only the pixel rows and columns selected by two 8-bit periodic patterns. Arguments (bp+6..): `src` (far, 4 planes one after another, `wb` bytes per row, `h` rows), `dst` (far, zero-filled, packed output), `row_pat` (`[BP+0x12]`), `col_pat` (`[BP+0x14]`),
`bits` (`[BP+0x16]` = popcount(col_pat)). For plane p = 0..3: for row r = 0..h-1 with pattern bit `0x80 >> (r & 7)` (rotated right once per row; `[BP-4]`): if `(row_pat & bit) == 0` skip the source row (`src += wb`) else the row is kept: for each source byte b (wb of them): compact the bits of `b` whose column bit is set in `col_pat` (scan the 8 columns MSB first: `bl |= al` for kept set bits, shifting out the dropped ones), producing `bits` bits;
append them MSB-first to the output bit stream (`dl` = bit position: `es:[di] |= bl >> dl; dl += bits; if dl == 8 { di++; dl = 0 } else if dl > 8 { di++; es:[di] = bl << (bits + 8 - dl); dl &= 7 }`); at the end of the row `if (dl != 0) di++`. Output rows are therefore byte-aligned, `ceil(wb*bits/8)` bytes each, planes consecutive. `3488:00bf` / `3488:01f3` are the CGA / Tandy variants of the same helper.

### 3488:00bf cga_pixel_decimate_planes

CGA/Hercules variant of 3488:000e (slot 15 helper). not ported (VGA only).

### 3488:01f3 tdy_pixel_decimate_planes

Tandy variant of 3488:000e (slot 15 helper). not ported (VGA only).

## Handoffs

Functions of the video list that belong to another module (not specified here):

| Address | Belongs to | What it is |
|---|---|---|
| `1a21:27d2` | game_flow (UI) | = SR1 `msgbox_hide` (0f38:2adf): if `DS:1E7C:1E7E` holds a saved background bitmap, `screen_put_bitmap_mirror(saved, *(DS:881A+4), *(DS:881A+6))`, `arena_pop(1)`, clear it; **new**: when `racing` (`DS:B7AE`) it first redirects `DS:AD10/AD14` to the race page pair (as `cursor_redraw`) and forces `DS:AD26 = 1`, restoring all afterwards; clears `DS:1F4C`. |
| `1a21:44a8` | game_flow (UI) | **dialog item renderer** (new): draws one 0x12-byte item `{u16 flags, text ptr, colour scheme, ... x0, y0, x1, y1}` (button frames `fill_rect` + `draw_frame3`, centred labels via `1a21:1954`, text slots, hot-spot registration `DS:B888`/`B88A`/`B890`/`B892`). Walked by `1a21:473e`. |
| `2925:2b26`, `2925:2e60`, `2925:2ff8` | garage | **not** video: they are the car-assembly / engine-bay picture routines of the new event code (picture ids 0x3FA, 0x3FC, 0x4AE, 0x4AF; `car_compose`, `bitmap_extract`, `wait_vretraces`, `page_copy_rect`); `2e60` was mis-named `video_options_menu2` (it draws the engine bay overlay pictures at (0x18,0x15), (200,0x50), (0xA8,0x5C), (0x50,0x3F)). |
| `2925:3174` | garage | `stickers_menu_sr2` (stickers screen). |

Functions listed under other modules that the video spec owns (do not duplicate the symbol rows):

| Address | Listed under | Really | SR1 |
|---|---|---|---|
| `1a21:1954` | garage | `font_string_width(char *s)`: sum of the glyph advance bytes `byte [s[i]*9 - 0x11A]` in the font segment `DS:86EA`; (the symbol `font_string_width` on 1a21:1924 is wrong, see `wait_ticks`) | `0f38:17c8` |
| `1a21:2d90` | garage | `cursor_ctl(op)`: unchanged logic (op >= 0 shape: hot spot from `DS:200C/200E[op]` -> `DS:3B4A/3B4C`, `cursor_shape_offset`, `cursor_redraw`, return; -1/-2 show, -3/-4 hide with the counter `DS:3B3B`, state `DS:A2D8`) | `0f38:2d7e` |
| `1a21:b028` | game_flow | `page_copy_rect(src, dst, x0, y0, x1, y1)`: unchanged logic with `DS:AD24` mask; calls slot 0 flags 1 | `0f38:b5aa` |
| `2925:1cae` | race/game_flow | `event15_overlay(page, i)` called by `ega_anim_step` when `DS:1BC8 == 3` (draws per-tile overlay `i`; `i = 3` after the loop) | - |

Data handed over: the driver vector table `DS:A0AE` (slots called by race_render / garage / game_flow: slot users are the `lcall [DS:A0xx]` sites), `DS:AD10/AD14` page descriptors, `DS:AD26` mirror flag, palette tables, `DS:1BC8` event id.

## Open questions

* `3488:000e` (slot 15 helper): the role of the two pattern bytes (rows/columns kept) is inferred from the code and the single caller `0dcb:b8c8`; confirm in the race_render spec.
* The `Anim` frame record `mode` byte (`frame[3]` in `rect_subtract_edge`/`anim_restore_rect` calls) and its polarity change are consistent in the three functions but the animation scripts (garage/event tables) were not checked.
* `shift_row`: bit-exact equivalence with SR1 for left shifts was reasoned from the code, not tested.
* The title screen is drawn with the identity palette; whether any screen between `main` and the first `pal_load` relies on `DS:1DC8` is not checked.
* `DS:81DE`/`81DF` (stride byte used by the polygon fill and its stray high byte) - see platform.md.
* `DS:B87C` (cleared by `gfx_init`) and `DS:AD18/AD1A` have no reader in the video code.

