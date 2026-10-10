# Platform layer (SR2 delta): text-mode start-up menus, keyboard/joystick/mouse, memory, picture libraries, shape files, MSC 6.0 C runtime

Scope: platform: start-up menus (video selection), keyboard / joystick / mouse, memory pools and bitmap allocation, arena, picture libraries LIB1-3, `SR2_shp` loader, MSC 6.0 runtime.
SR2 segments: `0000:0000` (text print), `0001` (main game segment: 34 platform functions), `1a21` (library: palette vectors, picture libraries, mouse, arena, shapes), `320d`, `34b0` (runtime, 111 functions), `371c`-`3723` (static descriptors, indexed as code but data).
Counts (matcher classes, `port/sr2_sr1_matches.csv`): **181 functions = 31 same / 88 changed / 62 new**. Specified here: 30 by reference to SR1 (`= SR1 ...` lines, recompiled or only re-addressed), 24 with a described difference, 8 new with full text, 111 C-runtime rows, 7 data rows (static descriptors).
Builds on: `../StreetRod/port/spec/platform.md` (+ `video.md` for the drawing side of the helpers). Addresses are `SSSS:OOOO` as in the index (`0001:xxxx` is SR1 `0000:(xxxx+0x10)`-style main segment; Ghidra adds 0x1000 to the segment). Conventions of SR1 apply (MSC medium model, far call arguments from `[bp+6]`, near from `[bp+4]`, result in AX / DX:AX).


## Overview of what changed for the platform layer

* **Start-up** (`main` = 0001:07b0 is specified by game_flow): `cpu_speed_calibrate` -> switches `1`, `nomouse` (`DS:020E`), `demo` (`DS:B8D0`), `nouemem` (`DS:0210`), `e c v h t` (force mode 2 / 1 / 3 / 4 / 5 and skip the menu via `video_detect_menu(forced)`) ->
  `video_detect_menu` -> `adlib_detect`/`sound_options_menu` -> `mem_pools_init` -> `driver_select` -> `gfx_init` -> `arena_init` -> `platform_install` -> sound init -> `lib_open_all` -> `lib_read_dir(1)` -> picture list -> `lib_read_dir(2)` -> ...
  SR1's `auto` switch is gone. The MSC 6.0 start-up calls `main` with the normal `argc/argv/envp` (see `_astart`).
* **Text-mode menus** (`video_menu`, `sound_options_menu`) print through `dos_print_line(s, indent)` and `set_text_cursor(col, row)` (INT 10h AH=2) instead of SR1's unindented lines; `video_menu` has a new VGA line.
* **Interrupts**: INT 8 `timer_isr` 0001:2246 / `race_isr` 0001:2622 and the music ISR 2f1b:00cb (sound module), INT 9 `kbd_isr` 0001:3014 (here), INT 24h `crit_err_handler` 0001:191a (here), INT 0 divide-error handler in the runtime (34b0:00DB). Old vectors: INT 8 `DS:A2AC/A2AE`, INT 9 `DS:3BDC/3BDE`.
* **Picture libraries**: LIB1 (4 pictures, ids 1..4), LIB2 (212 pictures, ids 1000..), **LIB3 (83 pictures, ids 2000.., hard disk only)**; 28-byte file records, 0x22-byte memory entries at segment 0x3726 (`DS:86E2`); decoding happens **in place** inside the destination bitmap; `pic_unpack` has an escape for token bytes. Only one library file is open at a time.
* **Descriptors shrank from 0x30 to 0x12 bytes** (see video.md "Desc"); `bitmap_alloc/_make/_init` write the new layout.
* **Arena**: arena 1 (65000 bytes) as in SR1 plus **arena 2** (45000 bytes on VGA) for `arena_bitmap_alloc(pool -1)` and the `SR2_shp` shape sets.
* **Shapes**: new file `?:sr2_shp` (`shp_load_set`, `shp_read_block_a/b`).
* **Input**: keyboard ISR lost the demo gating; new `input_poll_any` (0001:8c2c); joystick and mouse code unchanged apart from addresses.
* **Runtime**: MSC 6.0 (SR1: 5.1). Only a few routines matter to the port: `_astart`/`_cinit` (start-up), `exit` family, `int86`/`int86x`/`intdos` (INT 10h, 21h, 33h), `_dos_*` file calls, `movedata`, `time`, `sprintf`, string helpers.

Interrupt vector / port summary used by the platform (for the SDL host layer):

| What | Where | Port replacement |
|---|---|---|
| INT 8 (72.8 Hz PIT ch0, divisor 0x4000) | `platform_install` (sound module) | host timer thread / frame tick |
| INT 9 (keyboard, port 60h/61h) | `kbd_install` 0001:3175, handler 0001:3014 | SDL key events -> `kbd_xlat` translation |
| INT 24h critical errors | `_harderr` 34b0:249e, handler 0001:191a | none |
| INT 33h mouse | `int86` in `mouse_*` | SDL mouse (relative motion) |
| port 201h joystick | `joy_read` 0001:31a6 | optional gamepad |
| INT 10h video/palette | `bios_set_mode`, `ega_set_palette`, text menu | VGA mode 0Dh emulation, `vga_pal[17]` |
| INT 21h files | `_dos_open/_dos_read/lib_seek` | host file I/O |


Regular DS remap blocks (SR1 range -> SR2 = SR1 + delta; the per-function "DS remap" lists in the sections below are the exceptions and the confirmation):

| SR1 range | delta | SR2 range | Content |
|---|---|---|---|
| `0250-0254` | -0x54 | `01FC-0200` | EGA/VGA detection flags |
| `05E2-0600` | +0x196E | `1F50-1F6E` | mouse state, tick counter `05F8 -> 1F66` |
| `4718-47EE` | -0xBF8 (`47CD-47D0`: -0xBF9) | `3B20-3BF6` | joystick, pointer, click records, keyboard tables |
| `5AEE-5AF2` | +0x1F3C | `7A2A-7A2E` | saved BIOS mode / equipment / EGA memory |
| `6C60-6C70` | (not regular) | `8844-884E` | library selection state: `6C60 -> 8844`, `6C68 -> 8846`, `6C6A -> 8848`, `6C6C -> 884A`, `6C6E -> 884C`, `6C70 -> 884E` |
| `4ED8-4EE8` | (not regular) | `4B50-4B6A` | library handles / messages (see Globals) |
| `8B02, 8B7A, 7B14, 7D80, 78DE, 8F00` | (not regular) | `B7EC, B864, A31E, A7D8, A0FE, BC4C` | arena state |


## Globals

DGROUP is `3E08` in SR2 (SR1 `3E96`). `DS:xxxx` = image `0x3E080 + xxxx`. "SR1 equivalent" = DS offset in the Street Rod 1 specs.

| DS offset | type | name | meaning | SR1 equivalent |
|---|---|---|---|---|
| `DS:01FC` | u16 | `ega_present` | EGA/VGA card found by `ega_vga_detect` (1 = EGA BIOS answered, 2 = colour CRTC only) | `DS:0250` |
| `DS:01FE` | u16 | `ega_lowres_sw` | EGA info CL says switches 1/14 (set -> `ega_present = 1`) | `DS:0252` |
| `DS:0200` | u16 | `g_vga` | 1 = VGA (EGA driver with VGA split-screen); set by `video_detect_menu`/`driver_select` | `DS:0254` |
| `DS:020E` | u16 | `mouse_enabled` | 0 = `nomouse` switch (see `main`) | `DS:0262` |
| `DS:0210` | u16 | `no_ega_park` | 1 = `nouemem` switch: `pic_park_list` does nothing | `DS:0280` |
| `DS:0113` | char[] | `recommended_text` | "Recommended mode: #" | - |
| `DS:0127` | char[] | `video_menu_title` | "STREET ROD II:  Video Options" + 2 newlines (text menu strings `0127 / 0147 / 016F`, menu line table `DS:01A2..01AC`) | `DS:019B ..` |
| `DS:0202` | char[] | `str_Tandy` | "Tandy" (BIOS ROM signature, `0208` = "TANDY") | `DS:0256` |
| `DS:1D02` | u32 | `wait_last` | `wait_ticks` completion time of the previous call (DS:1D02/1D04) | - |
| `DS:1F50` | i16 | `mouse_dx` | scaled mouse delta x (`mouse_poll`) | `DS:05E2` |
| `DS:1F52` | i16 | `mouse_dy` | scaled mouse delta y | `DS:05E4` |
| `DS:1F54` | i16 | `mouse_acc_x` | mickey accumulator x | `DS:05E6` |
| `DS:1F56` | i16 | `mouse_acc_y` | mickey accumulator y | `DS:05E8` |
| `DS:1F58` | u16 | `mouse_shift_x` | mickey -> pixel shift x | `DS:05EA` |
| `DS:1F5A` | u16 | `mouse_shift_y` | mickey -> pixel shift y (changed by `race_isr_enter/leave`) | `DS:05EC` |
| `DS:1F5C` | u16 | `mouse_left` | left button (INT 33h AX=3) | `DS:05EE` |
| `DS:1F5E` | u16 | `mouse_right` | right button | `DS:05F0` |
| `DS:1F60` | u8 | `mouse_present` | non-zero when a mouse driver answered (`input_poll_any` polls it) | - |
| `DS:1F66` | u32 | `ticks` | tick counter (72.8 Hz timer ISR; DS:1F66/1F68) | `DS:05F8` |
| `DS:1FBE` | u8[] | `kbd_saved_pairs` | saved `{scan, old key}` pairs of the per-screen key remap, 0-terminated (`DS:1FBE` = first scan code, 0 = no remap) | `DS:0650 (+ flag DS:064E)` |
| `DS:3B20` | i16[4] | `joy_x` | joystick centre / axis words (`joy_read`, calibration) | `DS:4718` |
| `DS:3B24` | u8 | `joy_buttons` | joystick buttons (bits 4-5 of port 201h, inverted) | `DS:471C` |
| `DS:3B25` | u8 | `joy_present` | joystick detected / enabled | `DS:471D` |
| `DS:3B32` | i16 | `cursor_x` | pointer x (0..319) | `DS:472A` |
| `DS:3B34` | i16 | `cursor_y` | pointer y (0..199) | `DS:472C` |
| `DS:3B36` | i8 | `kbd_dir_x` | sum of held direction keys, x | `DS:472E` |
| `DS:3B37` | i8 | `kbd_dir_y` | sum of held direction keys, y | `DS:472F` |
| `DS:3B38` | i8[2] | `kbd_last_dir` | last direction pair | `DS:4730` |
| `DS:3B39` | u8 | `kbd_dir_aux` | cleared by `kbd_remap_restore` | `DS:4731` |
| `DS:3B3A` | u8 | `click_held` | bit 0 key, bit 1 joystick, bit 2 mouse button held | `DS:4732` |
| `DS:3B3B` | i8 | `cursor_visible` | pointer show/hide nesting counter (`cursor_ctl`) | `DS:4733` |
| `DS:3B3E` | u8 | `click_code` | pending primary click code (1 mouse, 0x1F key, joy buttons) | `DS:4736` |
| `DS:3B3F` | u8 | `click2_code` | pending secondary click code (0x1E) | `DS:4737` |
| `DS:3B40` | i16 | `click_x` | position of the pending click | `DS:4738` |
| `DS:3B42` | i16 | `click_y` |  | `DS:473A` |
| `DS:3B44` | i16 | `click2_x` | position of the secondary click | `DS:473C` |
| `DS:3B46` | i16 | `click2_y` |  | `DS:473E` |
| `DS:3B48` | i16 | `kbd_dir_lock` | interrupt-side lock byte (only cleared by the ISR in SR2) | `DS:4740` |
| `DS:3B4A` | u16 | `cursor_hot_x` | pointer hot spot x (set by `cursor_ctl`) | `DS:4742` |
| `DS:3B4C` | u16 | `cursor_hot_y` | pointer hot spot y | `DS:4744` |
| `DS:3B4E` | i16 | `race_saved_cursor_x` | saved pointer position (`cursor_init` clears) | `DS:4746` |
| `DS:3B50` | i16 | `race_saved_cursor_y` |  | `DS:4748` |
| `DS:3B6E` | u8[0x57] | `kbd_xlat` | scan code -> key code table (patched per screen); `+0x33/+0x34` = `,` `.` entries toggled by `kbd_text_mode_on/off` | `DS:4766` |
| `DS:3BD4` | u8 | `click_edge` | set with every primary click key press | `DS:47CD` |
| `DS:3BD5` | u8 | `sound_on` | toggled by Ctrl (scan 1Dh) | `DS:47CE` |
| `DS:3BD6` | u8 | `sound_gate` | sound routines run only when non-zero | `DS:47CF` |
| `DS:3BD7` | u8 | `key` | the one-byte key "queue" (`key_read` read-and-clear) | `DS:47D0` |
| `DS:3BDC` | far ptr | `old_int9` | previous INT 9 vector (`kbd_install`) | `DS:47D4/47D6` |
| `DS:3BE0` | u8[16] | `kbd_held_flags_a` | cleared by `kbd_remap_restore` | `DS:47D8` |
| `DS:3BF0` | u8[] | `kbd_held_flags` | held flags of the direction keys, indexed by (-k); also `joy_saved_sp` (word at `DS:3BF0`) | `DS:47E8` |
| `DS:3BF2` | u8[4] | `bios_mode_vars` | dead code (`bios_mode_*_set/apply`) | `DS:47EA..47ED` |
| `DS:3BF6` | u16 | `click_seen` | "any click / Esc" flag set by `kbd_isr`, `input_poll_any`, polled by wait loops | `DS:47EE` |
| `DS:4846` | near* (-> " Gear: ") | `status_label_text` | status label string used by `gfx_screen_mode(3)` | `DS:49DE` |
| `DS:4912` | u16[4] | `g_vram_pool_used` | bytes used in each of the four VRAM picture-cache pools; base far pointer `DS:490E:4910` (A000:0000), pools start at +0x7D00 | `DS:4C18 / 4C14` |
| `DS:4B50` | u16 | `lib1_handle` | LIB1 file handle (0 = closed) | `DS:4ED8` |
| `DS:4B52` | u16 | `lib2_handle` | LIB2 file handle | `DS:4EDA` |
| `DS:4B54` | u16 | `lib3_handle` | **LIB3 file handle (new)** | - |
| `DS:4B56` | char*[6] | `lib_names` | name pointers `?:lib1`, `?:lib1c`, `?:lib2`, `?:lib2c`, `?:lib3`, `?:lib3c` | `DS:4EDC..4EE2 (4 names)` |
| `DS:4B62` | u16 | `crit_err_off` | non-zero -> `crit_err_handler` returns at once; only ever cleared (by `lib_open_all`) | - |
| `DS:4B66` | char* | `msg_insert_disk` | "Insert disk ? into drive ?" (`[0xC]` disk number char, `[0x19]` drive letter patched) | `DS:4EE4` |
| `DS:4B68` | char* | `msg_copy_files` | "Copy all files to current directory" | - |
| `DS:4B6A` | char* | `msg_bad_format` | "Bad file format" | `DS:4EE8` |
| `DS:4B6C` | char* | `msg_not_found` | "File not found: " (+ name copied to offset 0x10 by `file_not_found`) | `DS:4EEC` |
| `DS:4B70` | char* | `msg_no_memory` | "Not enough memory to run Street Rod !" | `DS:4BEE` |
| `DS:4BBE` | u16 | `shp_handle` | `sr2_shp` file handle (0 = not open) | - |
| `DS:4BC0` | char[] | `shp_name` | "?:sr2_shp" (drive letter patched by `shp_load_set`) | - |
| `DS:4B72` | u8[16] | `run_counts` | `pic_unpack` run lengths of the 16 tokens | `DS:4EEE` |
| `DS:4B82` | u8[16] | `run_values` | `pic_unpack` run values (00 or FF) | `DS:4EFE` |
| `DS:79E0` | u16 | `arena2_used` | bytes already used in arena 2 by `arena_bitmap_alloc(pool -1)` | - |
| `DS:80A2` | u32+u16+u16[26] | `shp_directory` | `SR2_SHP` directory, 26 x 8 bytes `{u32 offset, u16 size, u16 0}` | - |
| `DS:81CE` | far ptr[3] | `shp_b_ptrs` | destination pointers of the format-B shape sets (index relative to `DS:81DA`) | - |
| `DS:81DA` | u16 | `shp_b_first` | first format-B set id (23); `DS:81DC` = count (3) | - |
| `DS:8172` | far ptr[23] | `shp_a_ptrs` | destination pointers of the format-A shape sets (index = set id) | - |
| `DS:8844` | u16 | `dgroup_seg` | DGROUP segment value (`get_ds`); `far_alloc` rejects blocks in it | `DS:6C60` |
| `DS:8846` | u16 | `cur_drive` | DOS drive at start-up (1 = A:, 3 = C:) | `DS:6C68` |
| `DS:8848` | u16* | `cur_lib_count` | pointer to the entry count of the current library (`DS:B8CA / B8D6 / B8DA`) | `DS:6C6A` |
| `DS:884A` | u16* | `cur_lib_handle` | pointer to the handle of the current library | `DS:6C6C` |
| `DS:884C` | char* | `cur_lib_name` | pointer to the path of the current library | `DS:6C6E` |
| `DS:884E` | u16* | `prev_lib_handle` | handle pointer of the previously selected library (closed on a switch) | `DS:6C70` |
| `DS:86E2` | u16 | `picdir_seg` | segment of the picture directory (0x3726); entries of 0x22 bytes | `DS:691C (seg 389B)` |
| `DS:849E` | u16 | `amblksiz` | C heap growth chunk (set 0x800 by `mem_pools_init`, 0x400 by `_myalloc`) | `DS:64A6` |
| `DS:95F2` | far ptr | `arena1` | 65000-byte work arena 1 | `DS:6CD0` |
| `DS:9E6C` | far ptr | `arena2` | **second arena (new)**: `DS:A9D6` bytes (45000 VGA); `arena_bitmap_alloc(-1)` and shape sets | - |
| `DS:9860` | u16 | `floppy_drives` | number of usable floppy drives (1 or 2) from `lib_open_all` | - |
| `DS:A0FE` | u16 | `arena_top` | end of the top (mode 3) region of arena 1 (65000) | `DS:78DE` |
| `DS:A31E` | u16 | `arena_low_depth` | depth of the low stack (mode 1) | `DS:7B14` |
| `DS:A7D8` | u16 | `arena_top_index` | index of the top stack (mode 2), starts 0x3C | `DS:7D80` |
| `DS:A9D6` | u16 | `arena2_size` | size of arena 2 (45000 / 62800 / 40000) | - |
| `DS:ADD6` | u16 | `road_mode` | shape-set list selector (0, 1, 2, 4, 8, 0x10) | - |
| `DS:ADD8` | u16 | `slow_machine` | 1 when `cpu_speed < 4` | `DS:82C6` |
| `DS:B7B4` | u16 | `cpu_speed` | `cpu_speed_calibrate` result (polls per 55 ms tick / 950, minimum 1) | `DS:8ACC` |
| `DS:B7EC` | u16[] | `arena_w` | arena allocation table W (W[0x3C] = `DS:B864`) | `DS:8B02` |
| `DS:BC4C` | u16 | `arena_perm` | permanent bump pointer of arena 1 | `DS:8F00` |
| `DS:B8CA` | u16 | `lib1_count` | LIB1 picture count (4) | `DS:8BC8` |
| `DS:B8D6` | u16 | `lib2_count` | LIB2 picture count (0xD4) | `DS:8DD2` |
| `DS:B8DA` | u16 | `lib3_count` | **LIB3 picture count (0x53, new)** | - |
| `DS:B8DE` | char* | `lib1_path` | pointer to the LIB1 path string (`?:lib1` / `?:lib1c`) | `DS:8DD4` |
| `DS:B8E2` | char* | `lib2_path` | LIB2 path string | `DS:8DD6` |
| `DS:B8E8` | char* | `lib3_path` | **LIB3 path string (new)** | - |
| `DS:B864` | u16 | `arena_w_top` | W[0x3C] | `DS:8B7A` |
| `DS:AD2D` | u8 | `ui_level` | 2 = after `cursor_init` (SR1: used by menus) | `DS:8247` |

## Functions

### 0000:0000 dos_print_line

`near void (char *s, int indent)`

Text-mode print helper (image 0x0000-0x009f; the Capstone index only sees its first 16 bytes as "0000:0000", the rest is not indexed).
SR1 `dos_print_line(s)` (0000:0000) gained an indent: builds a 0x74-byte stack buffer = `indent` spaces + `s` (strcat), appends `"\n\r$"`
and prints it with INT 21h AH=09h through `int86` (34b0:17b0). `indent <= 0` means no spaces. Used only by `video_menu` and `sound_options_menu`
(text mode 3 menus). Port: print to the host text console or skip (the menus are replaced by the launcher).

### 0001:0090 set_text_cursor

New. `near void set_text_cursor(int col, int row)` (near call, args at `[bp+4]`, `[bp+6]`). `int86(0x10, &in, &out)` (34b0:17b0) with `AH = 2` (set cursor position), `BH = 0` (page 0), `DL = (u8)col`, `DH = (u8)row`. Used by `video_menu` and `sound_options_menu` (text-mode menus, e.g. `set_text_cursor(0, 6)` before the prompt). Port: move the text-mode console cursor to (col, row).

### 0001:00c0 video_menu

Text-mode "Video Options" menu, rewritten (new layout, VGA entry). Same contract as SR1: `int video_menu(int recommended)` returns 1..5 for a
numeric key, 0 for any other key (= accept the recommendation), Esc calls `platform_exit()`. A recommendation outside 1..5 is forced to 1.
Steps: `int86(0x10, AX=0003h)` (80x25 text); build `"Recommended mode: #"` (DS:0113, 20 bytes copied) + `DS:[01A0 + 2*rec]` + 2 (skips the two leading spaces of the menu
line); `set_text_cursor(col=0,row=6)` = `0001:0090` (INT 10h AH=02h, page 0; this tiny helper is counted under game_flow);
`dos_print_line` (indent in parentheses): title DS:0127 `"STREET ROD II:  Video Options" + 2 newlines` (23), DS:0147 `"Use numeric keys to select graphic mode"` (18),
DS:016F `"or press any other key to accept recommendation" + newline` (15), then the six strings whose pointers are `DS:01A2..01AC` (indent 20): `1 - CGA`, `2 - EGA`,
`3 - VGA 320 x 200 x 16 colors` (**new**; SR1 had no VGA line), `4 - Hercules monochrome`, `5 - Tandy`, `Esc - Back to DOS`; a blank line (`DS:01A0`, indent 0);
the "Recommended mode" line (indent 13). Then `while (kbhit()) getch();` (34b0:1762 / 1788), `while (!kbhit());`, `k = getch() - '0'`; `k == -0x15` (Esc) -> `platform_exit()` (0001:18ce);
`1 <= k <= 5` -> k else 0. Port: the text menu is not needed (the launcher selects VGA); keep the return contract if the original flow is emulated.

### 0001:02ee vga_detect

= SR1 `vga_detect` (0000:018b), ../StreetRod/port/spec/platform.md §2.1; no DS references. INT 10h AX=1A00h: result 1 iff AL != 0 and BL == 8

### 0001:0330 bios_set_mode

= SR1 `bios_set_mode` (0000:01d1), ../StreetRod/port/spec/platform.md §2.1; no DS references

### 0001:034e cga_set_palette

= SR1 `cga_set_palette` (0000:01f2), ../StreetRod/port/spec/platform.md §2.1; DS remap: DS:8236 → DS:AD1C. CGA palette (INT 10h AH=0Bh BH=1), skipped on Hercules - not ported (VGA only)

### 0001:037c video_detect_menu

`int video_detect_menu(int forced)` (SR1: no argument). `forced` is the mode selected by the command-line switch (`e c v h t` -> 2 1 3 4 5, 0 = none, see `main`).
Detection is the SR1 logic: `ega_vga_detect()`; `if (DS:01FE) DS:01FC = 1;` (SR1 `DS:0252 / 0250`); mono equipment + (EGA present or `herc_detect()`) -> `DS:01FC = 0`, herc = 1;
`rec = DS:01FC ? (DS:7A2D == 3 ? 2 : 6) : 1` (`DS:7A2D` = SR1 `DS:5AF1`, EGA memory code); herc -> 4; `far_memmem("Tandy" DS:0202, F000:0000, 0xFFFF)` or `"TANDY"` (DS:0208) -> 5;
`vga_detect()` -> 3. **Difference:** the menu is shown only when `forced == 0` (`if (forced == 0) { c = video_menu(rec); forced = c ? c : rec; }`);
a forced mode skips the menu. Result: `forced == 1 && DS:01FC` -> 6; `forced == 3` -> `DS:0200 = 1` (VGA split-screen flag, SR1 `DS:0254`) and return 2; else return `forced`.
VGA path: returns 2 and sets `DS:0200 = 1` (port: always this).

### 0001:046e driver_select

`void driver_select(int mode)` - VGA path only. `switch (mode)`: mode 3 -> `DS:0200 = 1` then as mode 2; mode 2 -> `DS:AD1C = -2` (driver id, SR1 `DS:8236`),
BIOS equipment byte `0040:0010 = (b & 0xCF) | 0x20` (80x25 colour), call `ega_install_vectors` (2f32:111c, copies the 20-pointer table `DS:7B38` to `DS:A0AE`), return.
Modes 1/4/5/6 (CGA, Hercules, Tandy, EGA-64K -> CGA) build the static page descriptors with the new 0x12-byte layout (see video.md "Desc") and install `2719:01a6` (CGA/Herc) or
`2f32:0906` (Tandy) -> **not ported (VGA only)**. New: those branches call `1a21:b09e` (descriptor copy, see video.md) instead of SR1's `far_memset` of the page memory.
Jump table at image 0x0496 (cs:), 6 entries. The static descriptors (segment 371c, 3 x 0x12 bytes) are left as in the file on the VGA path.

### 0001:0628 video_mode_init

New (SR1 had this inline in `main`). `near void video_mode_init(int mode)` (arg `[bp+4]`), called by `main` right after `driver_select(mode)`; `mode` = the video choice of the start-up menu (1 CGA `c`, 2 EGA `e`, 3 VGA `v`, 4 Hercules `h`, 5 Tandy `t`; 6 behaves like 1). Switch on `mode - 1` (jump table at 0001:0644, 6 words; a value outside 1..6 does nothing but the final store):

| mode | action |
|---|---|
| 1, 6 (CGA) | `bios_set_mode(4)` (0001:0330); `cga_set_palette(1)` (0001:034e) |
| 2, 3 (EGA / VGA) | `bios_set_mode(0x0D)`; `cga_set_palette(1)` |
| 4 (Hercules) | `herc_graphics_on()` (0001:c93a); segment `s = (DS:AD1C == -4) ? 0xB000 : 0xB800`; zero the 32 KB `s:0000..7FFF` word by word; `outb(0x3B8, 0x2A)` (graphics, video on, page 0); `DS:0046 = 1; DS:0042 = 1` (Hercules-in-use flags; `DS:0042` is tested by the `2719` CGA/Hercules drawing helpers for a segment in `B000..B400`) |
| 5 (Tandy) | `bios_set_mode(9)`; `outb(0x3DF, 0xF6)` (Tandy CRT/CPU page register) |

Finally (all modes, also an out-of-range `mode`) `DS:0048 = 1` ("video mode was set": `platform_exit` then calls the driver restore + `video_restore`). Port (VGA only): only `bios_set_mode(0x0D)` + palette matters; set `DS:0048 = 1`.

### 0001:070c cpu_speed_calibrate

= SR1 `cpu_speed_calibrate` (0000:05c6), ../StreetRod/port/spec/platform.md §2.1; no DS references

### 0001:079a slow_flag_set

= SR1 `slow_flag_set` (0000:0659), ../StreetRod/port/spec/platform.md §2.1; DS remap: DS:82C6 → DS:ADD8; DS:8ACC → DS:B7B4. `DS:ADD8 = (DS:B7B4 < 4) ? 1 : 0` ("slow machine" flag)

### 0001:07ae empty_stub

New. `far void (void)`: a bare `retf` (1 byte + pad). `main` calls it first and `_astart` (34b0:0018) calls it before `_setargv`/`_cinit` (leftover hook of the C start-up). Port: no-op, can be omitted.

### 0001:0e74 hotspots_reset

= SR1 `hotspots_reset` (0000:0d58), ../StreetRod/port/spec/platform.md §2.2; DS remap: DS:051A → DS:1ECC; DS:051C → DS:1ECE; DS:051E → DS:1ED0. calls `arena_clear` (1a21:9940; SR1 called `arena_reset` 0f38:a054 = SR2 1a21:9b0e), zeroes DS:1ECC/1ECE/1ED0, `ui_pop(1)` (1a21:4cac)

### 0001:1708 click_clear

Changed (SR1 `click_clear`, 0000:0d78, ../StreetRod/port/spec/platform.md §2.2). `far void click_clear(void)`; DS remap: SR1 `DS:6C4A` -> `DS:881C`, `DS:8B92` -> `DS:B880`, `DS:8BAA` -> `DS:B898`, `DS:4736/4737` -> `DS:3B3E/3B3F`:
`DS:881C = 1` (pointer moved / redraw cursor); `DS:B880 = -1` (invalidates the cached hot-spot rectangle x1, so `ui_wait` re-runs `hotspot_find`); `DS:B898 = 32000` (hovered code = none); `DS:3B3E = DS:3B3F = 0` (pending primary / secondary click); `key_read()` (0001:3168, discards the key slot). Called at the end of `music_stop`/`music_resume` and by the menu / garage code after screen changes.

### 0001:189e timer_restore

Changed (SR1 `timer_restore`, 0000:0fb7, ../StreetRod/port/spec/platform.md §4.2). `far void timer_restore(void)`:
`cli; outb(0x43, 0x36); outb(0x40, 0); outb(0x40, 0)` (PIT ch0 divisor 65536 = 18.2 Hz); `_dos_setvect(8, DS:A2AE:A2AC)` (34b0:2474; the BIOS handler saved by `platform_install`); `sti`; `kbd_restore()` (0001:3194, restores INT 9); `DS:1F4E = 0` (ISRs installed, SR1 `DS:05E0`); `DS:0044 = 0` ("timer installed", tested by `platform_exit`). With AdLib the music ISR's own removal (sound slot 21) has restored INT 8 before; this routine restores the BIOS vector unconditionally. SR1 remap: `DS:7AAA` -> `DS:A2AC/A2AE`, `DS:05E0` -> `DS:1F4E`.

### 0001:191a crit_err_handler

INT 24h handler installed by `_harderr` (34b0:249e) in `platform_install`. SR1 behaviour (message box then `_hardresume(1 retry / 2 abort)`, abort -> `platform_exit`) with these differences:
(1) **new early return**: if `DS:4B62 != 0` the handler returns immediately without any message or `_hardresume` (flag set by `lib_open_all` while it probes for LIB3, see `lib_open_all`);
(2) the box is `fatal_message` (1a21:204c, SR1 0f38:23ce) with SR2 texts: errcode 0 (write protect) `DS:1F80` with the drive letter `'A'+AL` patched at `[5]`; errcode 2 (not ready) `DS:1F7E`
with the letter patched at `[strlen-2]`; errcode 9 (printer out of paper) `DS:1F8E`; deverror bit 15 set (`& 0x8000`) -> `DS:1FAE`, action forced to abort (2); otherwise `DS:1FA3` ("disk error").
Action code: 1 (retry) if `deverror & 0x8000` or `deverror & 0x1000`, else 2.

### 0001:19e8 platform_install

Changed (SR1 `platform_install`, 0000:1111, ../StreetRod/port/spec/platform.md §4.2). `far void platform_install(void)`, called once by `main` after `arena_init()`. The sound parts (song states `DS:9DEC..9E02`, `DS:ADD2`, `DS:9DB0`, PIT ch0 programming, saving the old INT 8 and the far-pointer tables) are in `sound.md` §5.1; the order below shows where they sit. Steps:

1. **Joystick detection** (as SR1): `outb(0x201, 0)` twice; `first = inb(0x201)`; poll `v = inb(0x201)` up to 65535 times (`n` starts at 1, wraps to 0) until `(v & 0xF3) == 0xF0` (A axes timed out, buttons up). If `DS:9730` (demo, SR1 `DS:0610`) is 0 **and** `n != 0` **and** `v != first`: `DS:3B25 = 1` (joystick present, SR1 `DS:471D`); `joy_read()` (0001:31a6) three times, summing `DS:3B20` (x) and `DS:3B22` (y); `DS:3B26 = sumx / 3`, `DS:3B28 = sumy / 3` (signed `idiv`, centre); thresholds `DS:3B2A = -(DS:3B26 >> 1)`, `DS:3B2C = DS:3B26 >> 1`, `DS:3B2E = -(DS:3B28 >> 1)`, `DS:3B30 = DS:3B28 >> 1` (arithmetic shifts). Otherwise `DS:3B25` stays 0. SR1 remap: `471D -> 3B25`, `4718/471A -> 3B20/3B22`, `471E/4720 -> 3B26/3B28`, `4722..4728 -> 3B2A..3B30`.
2. **Mouse**: `if (DS:020E /*0 = nomouse switch*/ && DS:9730 == 0) && mouse_reset() == -1` (1a21:769c): `DS:1F60 = 1` (mouse present, SR1 `DS:05F2`), `DS:1F54 = DS:1F56 = 0`, `DS:1F58 = DS:1F5A = 1`; else `DS:1F60 = 0`. (The demo and `nomouse` cases skip the driver call.)
3. Sound state init (`sound.md` §5.1, first block: `DS:9DF2 = 0`, song states, `DS:ADD2`, `DS:9DB0 = 0`).
4. `_harderr(crit_err_handler)` (34b0:249e with the far pointer to 0001:191a).
5. `cli`; PIT ch0 mode 3, divisor 0x4000 (`outb(0x43,0x36); outb(0x40,0); outb(0x40,0x40)`) = 72.82 Hz; `DS:A2AC/A2AE = _dos_getvect(8)` (34b0:241a), also stored at `2f1b:[0E..11]` (the segment is reached through `DS:86DA`); `_dos_setvect(8, race_isr)` and read back into `2f1b:[1A..1D]` (`DS:86DC`); `_dos_setvect(8, timer_isr)`, read back into `2f1b:[16..19]` and copied to `2f1b:[12..15]` (`DS:86DE`, `DS:86E0`; `sound.md` §5.1); `DS:1F7A = 0`; `sti`.
6. `if (DS:0050 == 0) adlib_garage_timbres_load()` (2c57:1394).
7. `kbd_install()` (0001:3175); `DS:1F4E = 1`; `outb(0x43, 0xB6)` (PIT ch2 mode 3 for the speaker); `input_reset()` (0001:17ea); `DS:0044 = 1` (timer installed; `platform_exit` calls `timer_restore` only when set). INT 8 = `timer_isr` on return.

SR1 differences: no `DS:8EF6..8EFC = 10,2,5,5` init, no `DS:6C46/6C48 = -1`, SR1's sound-channel default block (`DS:79B0..79E0`) is replaced by the song states, the demo test uses `DS:9730` and the mouse switch `DS:020E`, INT 8 is hooked three times to learn the ISR far pointers, the AdLib timbre load is new.

### 0001:1bea kbd_remap_restore

Same as SR1 `kbd_remap_restore` (clears the 16 bytes `DS:3BE0..3BEF` and `DS:3B36/3B37/3B38/3B39`; restores the translation table `DS:3B6E` from the saved pairs at `DS:1FBE`
(pairs `{scan, old}`, 0-terminated; all pairs, or only those with `old > 0x1F` when `keep_low != 0`; `DS:1FBE = 0` (= remap inactive) when `keep_low == 0`)) **except**: the function
no longer returns the "was remapped" value (SR1 returned `DS:064E`) and the remap-active test is gone (the loops start at `DS:1FBE` and stop at the first 0 scan code).
SR1 `DS:47D8 -> 3BE0, 4731 -> 3B39, 4730 -> 3B38, 472F -> 3B37, 472E -> 3B36, 4766 -> 3B6E, 0650 -> 1FBE`.

### 0001:1c5c kbd_hotspot_refresh

New. `far void kbd_hotspot_refresh(void)`: rebuilds the keyboard hot-key remap of the current menu (called by the garage code after hot spots moved, see `garage.md`). Steps:
1. `n = 1; p = DS:1FBE` (save list of pairs `{scan, old}`); `kbd_remap_restore(0)` (0001:1bea) first: restores the translation table `DS:3B6E` from the previous pairs and clears `DS:1FBE`.
2. If `DS:B87C` (menu depth) != 0: `DS:8820 = word DS:B872[DS:B87C]`; `el = DS:B894` (current element array of 0x12-byte `UIEL`s, `garage.md`). For every element up to and including the one with `flags & 2` (last): if `flags & 1` (active hot spot), for each of the two key bytes `k = el[0x10]` then `k = el[0x11]` (0xFF = none; if the first is 0xFF the second is skipped too): `p[0] = k; p[1] = DS:3B6E[k]` (save old entry); `DS:3B6E[k] = (u8)el->code - 0x32` (the key now yields the negative hot-key code that `ui_wait` maps back with `+0x32`); `p += 2; n++`.
3. Terminator (also when the menu depth is 0): `p[0] = 0x32; p[1] = DS:3BA0; DS:3BA0 = 0xA6; p[2] = 0` - scan code 0x32 (table entry `0x3B6E + 0x32 = 0x3BA0`, key `M`) is saved and remapped to 0xA6 (+0x32 = -0x28, the juke-box hot key); the list is 0-terminated. `n` is not returned (AX undefined).

### 0001:2222 sign

New. `far int sign(int v)` (`[bp+6]`): returns 0 for `v == 0`, 1 for `v > 0`, -1 for `v < 0`. Used by the demo cursor mover in `timer_isr`.

### 0001:2246 timer_isr

Changed (SR1 `timer_isr`, 0000:1fe2, ../StreetRod/port/spec/platform.md §4.3). `void interrupt timer_isr(void)`: INT 8 at 72.82 Hz outside of driving (installed by `platform_install`, swapped for `race_isr` by `race_isr_enter`). Saves all registers, loads `DS` with the constant 0x3E08 (port: not needed), `cld`. Per tick, in order:
1. `DS:1F66:1F68 += 1` (u32 game tick counter, SR1 `DS:05F8`).
2. Sub-tick divider: `a = DS:1F76 (u8); DS:1F76--; if (a != 0) { outb(0x20, 0x20) /*EOI*/; sti; } else { pushf; call far [DS:A2AC] /*BIOS INT 8, does its own EOI*/; DS:1F76 = 3; DS:1F6E:1F70 += 1 /*u32 18.2 Hz counter, SR1 DS:0600*/; }` - BIOS handler on the first tick, then every 4th.
3. Sound (`sound.md` §5.2, speaker music tick §6.2): `slot[23]` (`[DS:BBE8]`) when `DS:3BD5 == 0 && DS:3BD6 != 0`; speaker song tick when `DS:0050 == 1` and playing; `slot[24]` (`[DS:BBEC]`) when `DS:0050 == 0 && DS:3BD5 != 0 && DS:3BD6 == 0`.
4. **Demo** (`DS:9730 != 0`; SR1 gated by `DS:0610` and also called `demo_step` here, now done by `ui_wait`): `if (DS:20EA < 0) goto done;` (`DS:20EA` = target x, -1 = none / arrived, -2 = pick next, see game_flow `demo_step`; `DS:B196` = target y). If `DS:3B32 != DS:20EA || DS:3B34 != DS:B196`: `DS:3B32 += sign(DS:20EA - DS:3B32); DS:3B34 += sign(DS:B196 - DS:3B34); DS:881C = 1`, done. On arrival: `DS:3B3E = DS:3B3F = 1` (inject primary and secondary click), `DS:3B40 = DS:20EA`, `DS:3B42 = DS:B196` (click position), `DS:20EA = -1`, done. No keyboard / joystick cursor movement in demo mode.
5. **Cursor motion** (not demo), only if `(i8)DS:3B3B > 0` (cursor shown): `cli; if (DS:2000) goto done; DS:2000 = 1 /*re-entrancy guard, SR1 DS:06E0*/; sti; dx = dy = 0;` if `DS:3B48 == 0` (keyboard ISR not mid-update): `dx = sgn((i8)DS:3B36)`, `dy = sgn((i8)DS:3B37)` (held direction keys); if `DS:3B25` (joystick): `joy_read()`, `x = DS:3B20`: `x < DS:3B2A` -> `dx = -1`, else `DS:3B2C < x` -> `dx = 1`; `y = DS:3B22`: `y < DS:3B2E` -> `dy = -1`, else `DS:3B30 < y` -> `dy = 1` (overrides the keys per axis). If `dx | dy`: `DS:3B32 += DS:3B52[dx]; DS:3B34 += DS:3B58[dy]` (word tables `{-1, 0, 1}` indexed -1..1, i.e. +-1 pixel) and `DS:881C = 1`. `DS:2000 = 0`. The mouse is not read here (`mouse_poll` runs from `ui_wait` / `input_poll_any`).
6. Restore registers, `iret`.
SR1 differences: no demo-step trigger, no speaker-gate close (replaced by the sound slots), demo cursor mover steps to an explicit target, joystick no longer tested against demo (demo exits earlier). DS remap: `05F8 -> 1F66`, `0608 -> 1F76`, `0600 -> 1F6E`, `06E0 -> 2000`, `4733 -> 3B3B`, `4740 -> 3B48`, `472E/472F -> 3B36/3B37`, `472A/472C -> 3B32/3B34`, `6C4A -> 881C`, `7AAA -> A2AC`.

### 0001:2622 race_isr

Changed (SR1 `race_isr`, 0000:238c, ../StreetRod/port/spec/platform.md §4.3 and §4.8). `void interrupt race_isr(void)`: INT 8 while driving (installed by `race_isr_enter`, removed by `race_isr_leave`, both in other modules). Same prologue as `timer_isr` (registers, `DS = 0x3E08`, `cld`; 5 byte locals zeroed). Order:
1. `DS:1F66:1F68 += 1` (u32 tick counter).
2. If `DS:1F7A != 0` (install in progress) step 3 is skipped.
3. Sound: `sound.md` §5.3 (`DS:3BD5 == 0` -> `slot[23]` if `DS:3BD6`; else `slot[24]` if `DS:3BD6 != DS:3BD5`, then speaker engine tone (`DS:0050 == 1`) or AdLib engine / siren / tyre effects (`DS:0050 == 0`); phase bit = `DS:1F76 & 1`).
4. Tick chain (same as `timer_isr`): `a = DS:1F76; DS:1F76--; if (a != 0) { outb(0x20, 0x20); sti; return; }` else `DS:1F6E:1F70 += 1; pushf; call far [DS:A2AC]` (BIOS INT 8, EOI by BIOS); `DS:1F76 = 3`. The sound phase of step 3 uses the value of `DS:1F76` before this decrement.
5. Physics gate (chained tick only, i.e. every 4th): return unless `DS:1F7A == 0 && DS:B7AE != 0 /*racing*/ && DS:1BCA == 0 /*no animation player*/`; `DS:1F77--` (SR1 `DS:0609`); if `(i8)DS:1F77 > 0` return; `DS:1F77 = 3` (physics runs every 12th tick = 6.07 Hz); `cli; if (DS:2008) return; DS:2008 = 1 /*busy guard, SR1 DS:06E8*/; sti`.
6. One car-control / physics step: `race.md` §3 (input block, autopilot, rpm / gear / wear, steering, `road_edge_collision` 0dcb:148a, `race_ctrl_reset` 0001:2610). Finally `DS:2008 = 0`.
7. Restore registers, `iret`.
SR1 differences: engine sound block rewritten (`sound.md` §5.3), `DS:1F7A` gate, physics half rewritten (`race.md` §3). DS remap: `05F8 -> 1F66`, `0608 -> 1F76`, `0609 -> 1F77`, `0600 -> 1F6E`, `06E8 -> 2008`, `8ACA -> B7AE`.

### 0001:2f6c bios_mode_b_set

= SR1 `bios_mode_b_set` (0000:2bad), ../StreetRod/port/spec/platform.md §2.2; DS remap: DS:47EC → DS:3BF2; DS:47ED → DS:3BF3. stores `DS:3BF2/3BF3` (dead, no callers)

### 0001:2f7f bios_mode_a_set

= SR1 `bios_mode_a_set` (0000:2b9a), ../StreetRod/port/spec/platform.md §2.2 (sibling of `bios_mode_b_set` 0001:2f6c); DS remap: DS:47EA -> DS:3BF4; DS:47EB -> DS:3BF5. `far void bios_mode_a_set(u8 equip, u8 mode)`: `DS:3BF4 = equip` (`[bp+6]`); `DS:3BF5 = mode` (`[bp+8]`). Dead (no callers); `bios_mode_a_apply` 0001:2fb4 reads the pair.

### 0001:2f92 bios_mode_b_apply

= SR1 `bios_mode_b_apply` (0000:2be2), ../StreetRod/port/spec/platform.md §2.2; DS remap: DS:47EC → DS:3BF2; DS:47ED → DS:3BF3. dead (no callers)

### 0001:2fb4 bios_mode_a_apply

= SR1 `bios_mode_a_apply` (0000:2bc0), ../StreetRod/port/spec/platform.md §2.2; DS remap: DS:47EA → DS:3BF4; DS:47EB → DS:3BF5. dead (no callers)

### 0001:2fd6 kbd_text_mode_on

= SR1 `kbd_text_mode_on` (0000:2c04), ../StreetRod/port/spec/platform.md §2.2; DS remap: DS:4732 → DS:3B3A; DS:4774 → DS:3B7C; DS:4799 → DS:3BA1; DS:479A → DS:3BA2; DS:479F → DS:3BA7

### 0001:2ffd kbd_text_mode_off

= SR1 `kbd_text_mode_off` (0000:2c2b), ../StreetRod/port/spec/platform.md §2.2; DS remap: DS:4774 → DS:3B7C; DS:4799 → DS:3BA1; DS:479A → DS:3BA2; DS:479F → DS:3BA7

### 0001:3014 kbd_isr

INT 9 handler = SR1 `kbd_isr` (spec §4.4) with the **demo gating removed**: SR1 ignored every key except Ctrl (1Dh/9Dh) and `M` (32h) while `DS:0610` (demo active) was non-zero and let Esc (01h) set `DS:0610 = 99`
(abort demo); SR2 has neither the check nor the Esc rule (the demo is aborted elsewhere, see game_flow). Everything else is identical: XT acknowledge via port 61h, E0/E1 ignored, index `sc & 0x7F` (> 0x55 -> 0x56, release ignored),
`k = DS:3B6E[i]`; `k < 0` (>= -30) direction keys use the (dx,dy) pairs at `DS:3B6E + k` (held flags `DS:3BF0 + k`, sum `DS:3B36/3B37`, last `DS:3B38`); `k == 0x1F` primary click
(`DS:3B3E = 0x1F`, position `DS:3B32/3B34` -> `DS:3B40/3B42`, `DS:3B3A |= 1`, `DS:3BF6 = 1` ("any click" flag, SR1 `DS:47EE`), `DS:3BD4 = 1`); `k == 0x1E` secondary click (`DS:3B3F`, `DS:3B44/3B46`);
`k == 0x1D` toggles `DS:3BD5` (sound on/off); otherwise the key slot `DS:3BD7 = k`. The lock byte `DS:3B48` (SR1 `DS:4740`) is only cleared, never set to -1 by the ISR. EOI `out 20h,20h`.

### 0001:3168 key_read

= SR1 `key_read` (0000:2db6), ../StreetRod/port/spec/platform.md §2.2; DS remap: DS:47D0 → DS:3BD7

### 0001:3175 kbd_install

= SR1 `kbd_install` (0000:2dc3), ../StreetRod/port/spec/platform.md §2.2; DS remap: DS:47D4 → DS:3BDC; DS:47D6 → DS:3BDE

### 0001:3194 kbd_restore

= SR1 `kbd_restore` (0000:2de2), ../StreetRod/port/spec/platform.md §2.2; DS remap: DS:47D4 → DS:3BDC; DS:47D6 → DS:3BDE

### 0001:31a6 joy_read

= SR1 `joy_read` (0000:2df4), ../StreetRod/port/spec/platform.md §2.2; DS remap: DS:4718 → DS:3B20; DS:471C → DS:3B24; DS:472A → DS:3B32; DS:472C → DS:3B34; DS:4732 → DS:3B3A; DS:4736 → DS:3B3E; DS:4738 → DS:3B40; DS:473A → DS:3B42; DS:47E8 → DS:3BF0. the `0x96` constants in the SR1 listing were the DGROUP segment (0x3E96 vs 0x3E08), not data

### 0001:399e get_ds

= SR1 `get_ds` (0000:36a8), ../StreetRod/port/spec/platform.md §2.4; no DS references

### 0001:39b4 mem_pools_init

= SR1 `mem_pools_init` (0000:36be), ../StreetRod/port/spec/platform.md §2.4; DS remap: DS:64A6 → DS:849E; DS:6C60 → DS:8844

### 0001:39cc far_free

New (SR1 called `_ffree` directly). `far void far_free(void far *p)`: `_ffree(p)` (34b0:134a) with the two argument words (`[bp+6]` offset, `[bp+8]` segment) re-pushed. Used by `pic_free`. Port: `free(p)`.

### 0001:39de far_alloc

= SR1 `far_alloc` (0000:3709), ../StreetRod/port/spec/platform.md §2.4; DS remap: DS:6C60 → DS:8844; DS:8236 → DS:AD1C. callees: `_fmalloc` = 34b0:135d, `_ffree` = 34b0:134a, `out_of_memory` = 1a21:6232 (message DS:4B70 "Not enough memory to run Street Rod !" + `platform_exit`; see Handoffs)

### 0001:3a42 bitmap_alloc

= SR1 `bitmap_alloc` (0000:376b, platform.md §2.4/4.9) with the **new 0x12-byte descriptor** (video.md "Desc"): header size 0x12 instead of 0x30 (allocation `0x12 + planes + mask + 2`; the extra 2 bytes are padding),
planes start at `d + 0x12`. Field offsets: w +0, h +2, size +4 (= stride*h*nplanes), planes far ptr +6/+8, mask far ptr +A/+C, stride +E, type/flag byte +0x10 (SR1: +0x24 planes, +0x28 mask, +0x2C stride, +0x2E type).
Logic unchanged: `stride = ((w-1) >> DS:AD20) + 1`, `plane = stride*h`, `size = plane * DS:AD1E`; flags bit0 = has mask, bit1 = mask only (planes pointer moves to the mask, planes = NULL);
`d` non-NULL -> initialise in place. SR1 `g_nplanes DS:8238 -> AD1E`, `g_px_shift DS:823A -> AD20`.

### 0001:3b70 bitmap_make

= SR1 `bitmap_make` (0000:3880) with the 0x12-byte descriptor: `far_alloc(0x12)`; fields w +0, h +2, size +4, planes ptr +6/+8 (arg1/arg2), mask ptr +A/+C (arg3/arg4), stride +E, type +0x10.
`size = ((w-1 >> DS:AD20) + 1) * DS:AD1E * h`. Returns NULL when the allocation fails. SR1 offsets +0x24/+0x28/+0x2C/+0x2E map to +6/+A/+E/+0x10.

### 0001:3bfa bitmap_init

= SR1 `bitmap_init` (0000:3904) with the 0x12-byte descriptor; SR2 prototype `(Desc far *d, void far *planes, void far *mask, int w, int h, int type)` (far pointers are 2 words each).
Same fields as `bitmap_make`; `size = stride * DS:AD1E * h`.

### 0001:8c2c input_poll_any

`near void (void)`

Polls every input device once for a "button or Esc/Enter" and records it in the click-seen flag `DS:3BF6` (SR1 `DS:47EE`). Steps: if `DS:1F60` (mouse enabled, SR1 `DS:0262`) -> `mouse_buttons()` (1a21:76d6) and
`DS:3BF6 |= DS:1F5C` (left button, SR1 `DS:05EE`); if `DS:3B25` (joystick present) -> `joy_read()` and `DS:3BF6 |= DS:3B24` (joy buttons); `c = key_read()` (0001:3168, clears the key slot); if `c == 0x1B` or `c == 0x1F`
-> `DS:3BF6 = 1`. Used by the race/event wait loops (`0001:8d9e`, `race_logic`).

### 0001:c8d2 herc_mem_alias

= SR1 `herc_mem_alias` (0000:e56a), ../StreetRod/port/spec/platform.md §2.1; no DS references. Hercules detection helper - not ported (VGA only)

### 0001:c8ec herc_detect

= SR1 `herc_detect` (0000:e584), ../StreetRod/port/spec/platform.md §2.1; no DS references. Hercules detection - not ported (VGA only)

### 0001:c92b equip_is_mono

= SR1 `equip_is_mono` (0000:e5c3), ../StreetRod/port/spec/platform.md §2.1; no DS references. equipment word says monochrome adapter (Hercules detection) - not ported (VGA only)

### 0001:c93a herc_graphics_on

= SR1 `herc_graphics_on` (0000:e5d2), ../StreetRod/port/spec/platform.md §2.1; DS remap: DS:5ABC → DS:79F8; DS:5AD0 → DS:7A0C; DS:5AEC → DS:7A28. Hercules table copies from `DS:79F8` (CRTC words) / `DS:7A0C` (30-byte BIOS block) -- not ported (VGA only)

### 0001:c978 video_restore

= SR1 `video_restore` (0000:e610), ../StreetRod/port/spec/platform.md §2.1; DS remap: DS:5AEE → DS:7A2A; DS:5AF0 → DS:7A2C. restores the BIOS mode and equipment byte at exit (INT 10h AH=0); the port only needs to leave graphics mode

### 0001:c997 ega_vga_detect

= SR1 `ega_vga_detect` (0000:e62f), ../StreetRod/port/spec/platform.md §2.1; DS remap: DS:0250 → DS:01FC; DS:0252 → DS:01FE; DS:5AEE → DS:7A2A; DS:5AEF → DS:7A2B; DS:5AF0 → DS:7A2C; DS:5AF1 → DS:7A2D; DS:5AF2 → DS:7A2E. sets `DS:01FC` (EGA/VGA present, SR1 `0250`), `DS:01FE` (SR1 `0252`), `DS:7A2D` (EGA memory code, SR1 `5AF1`)

### 1a21:0da6 pal_black

Driver slot 7. `ega_set_palette(DS:1DB8)` (17 zero bytes: all registers black, overscan 0), then `DS:AD2F = 0` (palette "on" flag, SR1 `DS:8249`).
Same as SR1 `pal_black` (0f38:0b2d) except the table address (SR1 `DS:0452` -> `DS:1DB8`).

### 1a21:0db8 pal_normal

Driver slot 8. `ega_set_palette(DS:1DA6)` (the live 17-byte palette table), then `DS:AD2F = 1`. **Difference to SR1** (`ega_set_palette(DS:0440)`): the live table is a variable that
`pal_reg12_13` (1a21:198a) reloads from one of several constant tables (see video.md "Palettes"); its static initial content in the file is the identity `00 01 .. 0F, 00`, but `main` loads the game
palette `DS:1DC8` into it before the first `pal_normal`.

### 1a21:61e2 file_not_found

New. `far void file_not_found(char *name)` (noreturn). The message buffer `DS:[4B6E]` holds `"File not found: ????????"` (16-character prefix): `strcpy(msg + 0x10, name)` (NUL included); `fatal_message(msg)` (1a21:204c, boxed message + key wait); `platform_exit()` (0001:18ce). Callers: `snd_bank_load`, `mus_file_load`, `lib_open_all` (LIB1 on a floppy passes `name + 2`), the `SR2_shp` open in `shp_load_set`. Port: report "File not found: <name>" and exit.

### 1a21:6220 bad_file_format

`far void (void) noreturn`

`fatal_message(DS:4B6A)` ("Bad file format", box from 1a21:204c) then `platform_exit()` (0001:18ce). Replaces SR1's inline `message_box(DS:4EE8); platform_exit()` in the library loaders.

### 1a21:6232 out_of_memory

New. `far void out_of_memory(void)` (noreturn): `fatal_message(DS:[4B70])` ("Not enough memory to run Street Rod !"), then `platform_exit()`. Callers: `far_alloc`, `pic_get`, the shape / sound loaders.

### 1a21:6244 copy_all_files

`far void (void) noreturn`

`fatal_message(DS:4B68)` ("Copy all files to current directory") then `platform_exit()`. Used by `lib_open_all` when a library file is missing on a hard disk (SR1 had this string inline). Note: `1a21:6232` (between these two) is `out_of_memory`, see Handoffs.

### 1a21:6256 lib_seek

= SR1 `lib_seek` (0f38:6016), ../StreetRod/port/spec/platform.md §2.5; DS remap: DS:630D → DS:828A; DS:6318 → DS:8295. `lib_seek(handle, pos_lo, pos_hi)`: INT 21h AX=4200h through `intdos`; returns 0, or the DOS error code (`DS:8295`, also stored to `DS:828A`)

### 1a21:629a pic_index

`int pic_index(int id)` (near). Library-relative -> global directory index. SR1: `id >= 1000 ? id - 0x3DF : id - 1`, then a CGA/Hercules re-map through `DS:07C6[]`.
SR2: `id < 1000 ? id - 1` (LIB1: ids 1..4 -> 0..3); `1000 <= id < 2000 ? id - 0x3E4` (LIB2: ids 1000.. -> 4..215); `id >= 2000 ? id - 0x6F8` (**LIB3**: ids 2000.. -> 216..298).
The CGA/Hercules re-map table is gone (all drivers share one picture order).

### 1a21:62c4 lib_select

`void lib_select(int id)` (near). Makes the library that holds picture `id` the current one. **Differences to SR1** (spec §4.10 `lib_select`): three libraries chosen by the picture index
`ix = pic_index(id)`: `ix < 4` LIB1 (n = 1, handle ptr `DS:4B50`, count ptr `DS:B8CA`, name ptr `DS:B8DE`, expected count 4); `ix < 0xD8` LIB2 (n = 2, `DS:4B52`, `DS:B8D6`, `DS:B8E2`, expected 0xD4);
else LIB3 (n = 3, `DS:4B54`, `DS:B8DA`, `DS:B8E8`, expected 0x53). Globals set: `DS:884E` = previous handle ptr (`DS:884A`), `DS:884A` = handle ptr, `DS:8848` = count ptr, `DS:884C` = name ptr.
If `*DS:884A == 0` (library not open): `_dos_close(*DS:884E)`, `*DS:884E = 0` (only one library open at a time); prompt text `DS:4B66` ("Insert disk ? into drive ?"): `[0xC]` = '1' (LIB1), '4' (LIB3),
'2' (LIB2 on EGA/Tandy) or '3' (LIB2 otherwise), `[0x19]` = drive letter `*DS:884C`; `_dos_open(name, 0, handle)` retried up to 3 prompts (`fatal_message` = 1a21:204c, then `platform_exit` at the 4th failure);
then read the 2-byte count into `*DS:8848` and require `== expected` (SR1 checked `lib1 + lib2 == 0x115`); mismatch or read error -> `bad_file_format` (1a21:6220).

### 1a21:6432 lib_read_dir

`uint lib_read_dir(int lib)` returns the largest packed size (unsigned compare of the `+0x1A` word). **Differences to SR1**: 28-byte file records (FORMATS.md "Picture libraries") read into **0x22-byte memory
entries** at segment **0x3726** (`DS:86E2` holds the segment; SR1: 48-byte entries at 389b). `lib == 1`: base 0, count 4, `lib_select(1)`; `lib == 2`: base `DS:B8CA` (= 4), count 0xD4, `lib_select(1000)`;
`lib == 3` (**new**): base `DS:B8D6 + DS:B8CA` (= 0xD8), count 0x53, `lib_select(2000)`. Entry `i`: `lib_seek(handle, 2 + 28*i)` then `_dos_read(handle, entry, 0x1C)`; seek/read error or short read ->
`bad_file_format` (1a21:6220). Afterwards `entry[+0x1C] = 0xFF` (parked plane = none) and `entry[+0x1E..+0x21] = 0` (packed far pointer). SR1 also initialised the w/h/stride cache fields (`+22..+2E`), SR2 has no such fields.
Entry layout (0x22 bytes): `+0 w, +2 h, +4 depth (u8), +5 masked (u8), +6 tokens[16], +0x16 file offset (u32, relative to the end of the directory), +0x1A packed size (u16), +0x1C parked plane (u8, FF = none), +0x1E packed far ptr (off, seg)`.

### 1a21:652a pic_load

`int pic_load(int id, int to_vram, void far *dst)` (near; return 0 no memory, 1 already loaded, 2 loaded into RAM, 3 parked in EGA memory). SR1 logic (spec §4.10) with these differences:
entry = `pic_index(id) * 0x22` in segment `DS:86E2`; resident test = `*(u32*)(entry+0x1E) != 0`; `to_vram == 0`: buffer = `dst` or `far_alloc(size + 1, 0)` (size = `+0x1A`), plane byte `+0x1C = FF`;
`to_vram != 0`: `vram_pool_alloc(size, &plane)`; on success the temporary buffer is `arena_alloc(size + 1, 2)` and the plane byte is stored in `+0x1C`; on failure the packed ptr stays 0 (-> return 0).
File position = `*DS:8848 * 0x1C + 2 + entry.offset` (count of the current library times the 28-byte record size). Read `size + 1` bytes; accept when `buf[0] == 0xBC && buf[size] == 0xBC`;
parked pictures: after the read `movedata` the packed bytes into the VRAM plane, packed ptr = the VRAM address, `arena_pop(1)` (= 1a21:9b56), SEQ2 = 0Fh, return 3; otherwise return 2.
Failure path: hard disk (`DS:8846 > 2`) -> `bad_file_format`; floppy: up to 3 prompts (`DS:4B66`: `[0xC]` = byte 01h for LIB1 (SR1 bug kept: a raw 1, not '1'), '2'/'3' for LIB2; `[0x19]` = `*DS:884C`), `_dos_close`, reopen, seek again.

### 1a21:67e8 pic_free

`void pic_free(int id)`. Entry = `pic_index(id) * 0x22`; if the packed far pointer (`+0x1E/+0x20`) is non-null: `far_free(ptr)` (0001:39cc) and clear it. SR1: 48-byte entries, `_ffree` -> `far_free`.

### 1a21:683a pic_decode

`Desc far *pic_decode(int id, Desc far *dst)` (far; dst may be a ready-made bitmap or NULL). Differences to SR1 (§4.10): (1) hide cursor `cursor_ctl(-4)`; bitmap is always created with
`bitmap_alloc(w, h, masked, 1, dst)` (SR1: only when `DS:4F0E`); (2) **no shared decode buffer**: the packed data is read into the *tail of the destination bitmap itself*:
`tail = (mask ptr ? mask + stride*h : planes + size) - packed_size + 1`, i.e. unpacking runs in place, the compressed data sitting at the end of the target memory (`pic_load(id, 0, tail)`; EGA/VGA);
(3) token table copied from entry `+6` (16 bytes) onto the stack; parked picture: GC4 (read map) = entry `+0x1C`; source = packed ptr `+1`, count = size - 1;
(4) VGA (`DS:AD1C == -2`): if the bitmap has planes: `pic_unpack(src, planes, n, tokens, 16)` (26b3:000c) and, when a mask exists, `make_mask_plane(planes, mask, size)` (33e5:00b9); mask-only bitmap: unpack into the mask;
other drivers use the same `pic_unpack` (Tandy: 3210:005a/0120/023c, not ported); (5) afterwards SEQ2 = 0Fh if the picture was parked, and if `pic_load` returned 2 the entry's packed ptr is cleared; show cursor `cursor_ctl(-2)`.
SR1's `DS:7564` shared buffer and `DS:9AD4/9AD6` are gone.

### 1a21:6b50 pic_park_list

`int pic_park_list(int *ids)`: if `DS:0210` (`nouemem` switch, SR1 `DS:0280`) is 0, `pic_load(id, 1, NULL)` for every id of the 0-terminated list (EGA/VGA only; parks the pictures in video memory). Else nothing. = SR1 `pic_park_list` (0f38:6887); `pic_load` now takes a 4-word argument list (id, 1, 0, 0).

### 1a21:6b80 pic_load_list

= SR1 `pic_load_list` (0f38:68ca): `pic_load(id, 0, NULL)` for each id; counts successes; stops at the first failure unless `keep_going`. Only the `pic_load` calling convention changed.

### 1a21:6bc0 pic_free_list

= SR1 `pic_free_list` (0f38:6915): `pic_free(id)` for every id of the 0-terminated list.

### 1a21:6be0 lib_open_all

`void lib_open_all(int disks, int mode)`. SR1 logic (spec §4.10 `lib_open_all`) with these differences: (1) `DS:9860 = disks` (1 or 2 usable floppy drives; set from `_bios_equiplist` bits as in SR1: drive < 3, disks > 1, equipment bit 0 and bits 6-7);
(2) file names come from the pointer table `DS:4B56`..`4B60` (`"?:lib1"`, `"?:lib1c"`, `"?:lib2"`, `"?:lib2c"`, `"?:lib3"`, `"?:lib3c"`; `c` variants only for CGA/Hercules, i.e. `mode` not 2 and not 5), drive letter patched into byte 0;
handles `DS:4B50` (LIB1), `DS:4B52` (LIB2), `DS:4B54` (LIB3), counts `DS:B8CA / B8D6 / B8DA`, name pointers `DS:B8DE / B8E2 / B8E8`; (3) error boxes: LIB1 missing on a floppy -> `file_not_found(name + 2)` (1a21:61e2, text DS:4B6C/4B6E + name), on a hard disk -> `copy_all_files` (1a21:6244, text DS:4B68 "Copy all files to current directory") + exit;
(4) counts: LIB1 count is read without a check, LIB2 count must be 0xD4, **LIB3 is opened only on a hard disk** (`DS:8846 > 2`) and its count must be 0x53 (a missing LIB3 file -> `copy_all_files`); bad read -> `bad_file_format` (1a21:6220);
(5) the second-floppy retry loop of LIB2 ends with `DS:4B62 = 0` (the only write to that flag; `crit_err_handler` returns early when it is non-zero); (6) the four song file names `DS:7304 / 7310 / 731C / 7328` get the LIB2 drive letter in byte 0.

### 1a21:7304 shp_read_block_a

> **Secondary view (S3.7):** owned by `race_render.md`; on any conflict the owner's section wins.

`near far-ptr (int set, int far *dst)`

Loads shape set `set` (0..22, format A, FORMATS.md "SR2_SHP") from `?:sr2_shp`. `dst` = destination far pointer (arguments in the order `set, dst_off, dst_seg`). Steps:
(1) record the destination: `DS:8172[set*4] = dst_off`, `DS:8174[set*4] = dst_seg` (table of far pointers, SR1 none; stored at image DS offsets `-0x7E8E` / `-0x7E8C` = `0x8172 / 0x8174`); size = `DS:80A6[set*8]` (directory `DS:80A2 + set*8`: `{u32 file offset @+0, u16 size @+4, u16 0}`);
(2) if `DS:4BBE` (file handle) is 0: `_dos_open("?:sr2_shp" DS:4BC0, 0, &DS:4BBE)` (failure -> `file_not_found("?:sr2_shp")` 1a21:61e2);
(3) `lib_seek(DS:4BBE, offset_lo, offset_hi)` then `_dos_read(handle, dst, size)`, error / short read -> `bad_file_format`;
(4) fix-up pass over the loaded set (header `{u16 n_lods; u16 ptr_off; u16 ptr_seg}` at dst): `dst[1] = dst + 6` (pointer to the LOD records), `dst[2] = dst_seg`; span data starts at `p = dst + 6 + n_lods*16`; for LOD i (record at `dst+6+16*i`):
`rec[+0xC] = p`, `rec[+0xE] = dst_seg` (pointer to this LOD's span data), then for up to 6 (colour,count) pairs of the record while `count != 0`: `p += count * 3`.
Returns the next free address `dst + size` as a far pointer (DX:AX = seg:off) so the caller can chain sets.

### 1a21:7446 shp_read_block_b

> **Secondary view (S3.7):** owned by `race_render.md`; on any conflict the owner's section wins.

`near far-ptr (int idx, int far *dst)`

Loads shape sets of format B (mode `DS:ADD6 == 8`, sets 23..25; first id `DS:81DA`, count `DS:81DC`): `idx` is relative to `DS:81DA`. `DS:81CE[idx*4] = dst_off`, `DS:81D0[idx*4] = dst_seg`;
size = `DS:80A6[(idx + DS:81DA)*8]`; open/seek/read exactly as `shp_read_block_a`. Fix-up: header `{u16 n_groups; u16 lods_per_group (=7); u16 ptr_off; u16 ptr_seg; u32 group_ptr[n_groups]}`:
`dst[2] = dst + 8`, `dst[3] = dst_seg`; `p = dst + 8 + n_groups*4` (LOD record table start: group g at `p0 + g*lods_per_group*16`); for g in `0..n_groups-1`: `group_ptr[g] = (p, dst_seg)` with `p += lods_per_group*16`;
then span data pointer `q` = end of the record tables; for every group, for every LOD (16-byte records): `rec[+0xC] = q`, `rec[+0xE] = dst_seg`; for up to 6 (colour,count) pairs of the record (`rec[1 + 2*j]` = count byte of pair j) while `count != 0`: `q += count * 3`.
Returns `dst + size` (far, DX = seg).

### 1a21:75ea shp_load_set

> **Secondary view (S3.7):** owned by `race_render.md`; on any conflict the owner's section wins.

`far void (void)`

Loads the shape sets of the current road mode into the **second arena** at `DS:9E6C:9E6E + DS:79E0` (`DS:79E0` = bytes already used by `arena_bitmap_alloc` pool -1). Sets the drive letter of `DS:4BC0` ("?:sr2_shp") to `*DS:B8E8`'s first char
(LIB3 name drive; always the LIB data drive). `mode = DS:ADD6`: 8 -> format B: `shp_read_block_b(i, ptr)` for `i = 0 .. DS:81DC - 1`; 0x10 -> id list `DS:4C30`; 0 -> `DS:4BD6`; 1 -> `DS:4BEE`; 2 or 4 -> `DS:4C16`
(the id lists are `-1` terminated word arrays of shape-set numbers); for the lists: `shp_read_block_a(id, ptr)` per id, chaining the returned pointer. Finally if `(end - DS:9E6C) >= DS:A9D6` (arena size) -> `out_of_memory` (1a21:6232).

### 1a21:769c mouse_reset

= SR1 `mouse_reset` (0f38:799a), ../StreetRod/port/spec/platform.md §2.3; no DS references. returns 0 when the INT 33h vector (`0000:00CC`) is null, else INT 33h AX=0

### 1a21:76d6 mouse_buttons

= SR1 `mouse_buttons` (0f38:79d7), ../StreetRod/port/spec/platform.md §2.3; DS remap: DS:05EE → DS:1F5C; DS:05F0 → DS:1F5E. INT 33h AX=3; `DS:1F5C` = left button (SR1 `DS:05EE`), `DS:1F5E` = right (SR1 `05F0`)

### 1a21:7708 mouse_motion

= SR1 `mouse_motion` (0f38:7a14), ../StreetRod/port/spec/platform.md §2.3; no DS references. INT 33h AX=0Bh mickeys

### 1a21:7738 mouse_poll

= SR1 `mouse_poll` (0f38:7a47), ../StreetRod/port/spec/platform.md §2.3; DS remap: DS:05E2 → DS:1F50; DS:05E4 → DS:1F52; DS:05E6 → DS:1F54; DS:05E8 → DS:1F56; DS:05EA → DS:1F58; DS:05EC → DS:1F5A; DS:05EE → DS:1F5C; DS:472A → DS:3B32; DS:472C → DS:3B34; DS:4732 → DS:3B3A; DS:4736 → DS:3B3E; DS:4738 → DS:3B40; DS:473A → DS:3B42. the decompiler shows the tail as "set bit 4 then clear it"; the machine code skips the clear when the button is down (same as SR1). Click record: `DS:3B3E = 1`, position copied from `DS:3B32/3B34` to `DS:3B40/3B42`, `DS:3B3A` bit 2 = left button held

### 1a21:9940 arena_clear

= SR1 `arena_clear` (0f38:9ebc): resets the work-arena allocator (SR1 also zeroed the far pointer `DS:0294`, which SR2 no longer has): `DS:A31E = 0` (low-stack depth, SR1 `DS:7B14`), `DS:BC4C = 0` (permanent bump pointer, SR1 `DS:8F00`), `DS:B7EC = 0` (W[0]), `DS:A7D8 = 0x3C` (top-stack index, SR1 `DS:7D80`),
`DS:A0FE = 65000` (end of the top region, SR1 `DS:78DE`), `DS:B864 = 65000` (W[0x3C], SR1 `DS:8B7A`). The array `W` is now at `DS:B7EC` (SR1 `DS:8B02`).

### 1a21:995c arena_init

Allocates **two** arenas: arena 1 `DS:95F2:95F4 = far_alloc(65000, fatal)` (SR1 `DS:6CD0`; the Tandy special case `8000:0000` is gone), arena 2 (**new**, used by `arena_bitmap_alloc(..., pool -1)` and `shp_load_set`)
`DS:9E6C:9E6E = far_alloc(DS:A9D6, fatal)` with `DS:A9D6 = 45000` (EGA/VGA, driver -2), `0xF550` = 62800 (Tandy) or 40000 (other); then `arena_clear()`. Both blocks must lie outside DGROUP (`far_alloc`).

### 1a21:99b0 arena_free_all

`far void (void)`

Called first by `platform_exit` (replaces SR1's `arena_low_shrink`): `_ffree(DS:95F2:95F4)` and `_ffree(DS:9E6C:9E6E)` (34b0:134a), i.e. releases both arenas before the program ends. Port: nothing to do (host frees memory at exit).

### 1a21:99d2 arena_alloc

`void far *arena_alloc(int n, int mode)` on arena 1 (`DS:95F2:95F4`), same four modes as SR1 (spec §4.9): mode 0 permanent bump (`off += DS:BC4C; DS:BC4C += n; DS:B7EC = DS:BC4C`); mode 1 low stack (`off += W[DS:A31E]; W[DS:A31E + 1] = W[DS:A31E] + n; DS:A31E++`);
mode 2 top stack (`W[DS:A7D8] -= n; off += W[DS:A7D8]; W[DS:A7D8 - 1] = W[DS:A7D8] - 1; DS:A7D8--`); mode 3 (`n = DS:A0FE - n; DS:A0FE = n - 1; off += n; DS:B864 = DS:A0FE`); other modes return the arena start.
Remap: `W` = `DS:B7EC` (SR1 `DS:8B02`); `DS:A31E` (7B14), `DS:BC4C` (8F00), `DS:A7D8` (7D80), `DS:A0FE` (78DE), `DS:B864` (8B7A).
The pops are `1a21:9b40 arena_pop_low(k)` (`DS:A31E -= k`, 0 = all), `1a21:9b56 arena_pop(k)` (top stack, SR1 0f38:a0b8), `1a21:9b0e arena_reset` (SR1 a054), `1a21:9b26 arena_reset_stacks` (SR1 a07e) (see Handoffs).

### 320d:0004 far_memmem

= SR1 `far_memmem` (2460:0002), ../StreetRod/port/spec/platform.md §2.1; no DS references. `far_memmem(pat, buf, len)`: scans `buf[len]` for the NUL-terminated `pat` (hand-written asm, es:di scan); used for "Tandy"/"TANDY" at F000:0000

### 34b0:0018 _astart

Program entry (file CS:IP = 34b0:0018; EXEPACK-unpacked image). **Differences to SR1's `_astart` (1e16:001e)** (MSC 6.0 start-up): DOS version check (`INT 21h AH=30h`, AL < 2 -> return to DOS via PSP:0); sizes the program from PSP:2 (top of memory) and sets `DS:8254` (end of stack),
`DS:825A..8260` (stack/heap bounds); **zero-fills the BSS `DS:8806..BC4F`** (`rep stosb`); optional hook `[DS:86B4]`; calls the empty stub `0001:07ae` (a bare `retf`), `_setargv` (34b0:0308) and `_cinit` (34b0:0110); then
`main(argc = [DS:82AD], argv = [DS:82AF], envp = [DS:82B1])` = `0001:07b0` (image 0000:07c0), and `exit(main's result)` (34b0:01d5). **Port:** SDL3 `main(argc, argv)` calls the game's `main` directly; the BSS is zero (C static storage); nothing else is observable.
`DS:0002` (PSP top of memory) is the only PSP field read; the environment segment (PSP:2Ch) is read by `_setargv`/`_cinit` (`_C_FILE_INFO`).

### 34b0:0110 _cinit

C initialisation (changed vs SR1 only in MSC 6 details): `INT 21h AH=35h` saves the INT 0 vector (`DS:827C:827E`) and `INT 21h AX=2500h` installs the **divide-error handler `34b0:00DB`** (prints "Run-time error R6003 - integer divide by zero" via `_FF_MSGBANNER/_NMSG_WRITE(3)` and exits with 0xFF) - the only interrupt vector the runtime hooks;
floating-point/emulator init through `[DS:86C6]` when `DS:86C8 != 0` (not used by the game); scans the environment block for `_C_FILE_INFO=` (inherited handle flags -> `DS:8299`). **Port:** nothing (division by zero is a host error; the game never divides by zero on purpose).

### 34b0:01d5 exit

C `exit(status)`: runs the terminator tables (`_initterm` x4 around the optional `[DS:86BC]` stdio flush hook), `_nullcheck` (NULL-pointer segment checksum, see 02e4), `_ctermsub`, then `INT 21h AH=4Ch`. host C library `exit` (the game exits with codes 0 and 5 through `platform_exit`).

### 34b0:01dc _exit

C `_exit(status)`: same as `exit` without the stdio flush hook. host C library `_exit`.

### 34b0:01e4 _cexit

C `_cexit()`: terminator work of `exit` without terminating the process. host C library (never called by the game).

### 34b0:01ee _c_exit

C `_c_exit()`: quick variant (no flush hook, no terminate). Not used by the game.

### 34b0:0258 _ctermsub

Final terminator step: `[DS:86C6](...)` floating-point cleanup if `DS:86C8 != 0`; `INT 21h` (restore INT 0 vector); `INT 21h` (close stdaux/handle flag `DS:82BA`). No port equivalent (host process exit).

### 34b0:0285 _initterm

Calls the function pointers of an initialiser/terminator table from the end downwards (`SI`..`DI`), skipping null entries. No port equivalent.

### 34b0:0298 _FF_MSGBANNER

Writes the run-time error banner "\r\nrun-time error " (`_NMSG_WRITE(0xFC)`), an optional hook `[DS:82C0]`, then `_NMSG_WRITE(0xFF)`. Error path only.

### 34b0:02c0 _chkstk

Stack-overflow check at function entry (`AX` = frame size): compares with `DS:82C8`; overflow -> `[DS:82C4]` hook or `_stkover` (34b0:00EB, run-time error R6000). **Not ported** (host stack).

### 34b0:02e4 _nullcheck

Checksums the first 0x42 bytes of DGROUP (the C 6.0 copyright header, XOR must be 0x55); mismatch -> "R6001 null pointer assignment" via `_NMSG_WRITE(1)` at exit. Diagnostic only.

### 34b0:0308 _setargv

Command-line / environment parser (DOS: PSP:81h, environment segment PSP:2Ch): fills `argc = DS:82AD`, `argv = DS:82AF`, `envp = DS:82B1`, handling quotes and backslash rules of MSC 6; the DOS version byte goes to `DS:8292`. **Port:** use the host `argc/argv`.
The game's switches are matched against these strings in `main` (see the `main` spec of game_flow): `1`, `nomouse`, `demo`, `nouemem`, `e`, `c`, `v`, `h`, `t`.

### 34b0:049a _setenvp

Builds the `envp` pointer table from the environment block (the part of `_setargv` that follows it; no callers - reached by fall-through). Host: `environ`.

### 34b0:0518 _NMSG_TEXT

Looks up a run-time error message by number in the table at `DS:872C` (`{u16 id, char text[]}` ... id -1). Error path only.

### 34b0:0543 _NMSG_WRITE

Writes the error text for a message number to stderr (`INT 21h AH=40h`); optional hook `[DS:86B8]` when `DS:86B6 == D6D6`. Error path only.

### 34b0:057a _myalloc

Internal allocation with `_amblksiz` forced to 0x400 (`DS:849E`): `_nmalloc(size)`; on failure prints the heap-overflow error (R6009 "not enough space for environment"/R6008) and aborts. Used during start-up (`_setargv`).

### 34b0:05c2 _maperror

Tiny wrapper: `_dosmaperr()` (DOS error in AX -> errno). Host: errno handling.

### 34b0:05c8 _dosmaperr

Maps a DOS error code (AX) to `errno` (`DS:8295`) using the table at `DS:82CE`; also stores `_doserrno` (`DS:828A`). The game reads the numeric result only through `lib_seek`/`_dos_*` return values.

### 34b0:05f6 fwrite

C `fwrite(buf, size, count, stream)` (buffered, handles the line-buffered `_stbuf` temporary buffers). host C library `fwrite`. Used only by `puts`.

### 34b0:06fc _flsbuf

C `_flsbuf(ch, stream)`: flush the stream buffer and store one character (allocates `_getbuf` on first use). host C library internal of stdio.

### 34b0:07e0 _getbuf

Allocates the 512-byte stream buffer (`_nmalloc(0x200)`) or falls back to the one-byte buffer inside the FILE. stdio internal.

### 34b0:0824 _stbuf

Gives stdout/stderr/stdprn a temporary 512-byte buffer for the duration of one `printf`/`puts` call. stdio internal.

### 34b0:0897 _ftbuf

Flushes and releases the temporary buffer of `_stbuf`. stdio internal.

### 34b0:08d6 fflush

C `fflush(stream)` (NULL = `_flushall`). host C library `fflush`.

### 34b0:0952 _flushall

C `_flushall()`: flush every open stream (FILE table `DS:82E4..8424`, 8 bytes... entries of 0xC bytes). host C library `fflush(NULL)`.

### 34b0:09ae _output

The printf formatting engine of `sprintf` (state-table driven; handles %d %u %x %X %o %c %s %ld %lu %lx and field width/precision/flags; floating point is not linked). Called only by `sprintf`. host C library `vsnprintf`/`sprintf`: the game uses simple formats (`%d`, `%s`, `%c`, `%ld`, `%02d`, `%-5d` style).

### 34b0:0db9 _output_getint

`_output` helper: fetch the next `int` argument from the va_list (`[BP+10]`). No port equivalent needed (host printf).

### 34b0:0dc1 _output_getlong

`_output` helper: fetch the next `long`/far pointer (2 words). No port equivalent needed (host printf).

### 34b0:0dcd _output_getarg

`_output` helper: fetch an `int` or a `long` argument depending on the `l` flag (`[BP-4] & 0x20`). No port equivalent needed (host printf).

### 34b0:0dea _output_putc

`_output` helper: store one character in the destination FILE (`--cnt < 0 ? _flsbuf : *ptr++ = c`). No port equivalent needed (host printf).

### 34b0:0e14 _output_pad

`_output` helper: write `CX` pad characters (field width). No port equivalent needed (host printf).

### 34b0:0e32 _output_write

`_output` helper: write `CX` characters of a string. No port equivalent needed (host printf).

### 34b0:0e4e _output_cltoa

`_output` helper: convert an unsigned long to digits in the given radix (`DX:AX / CX`, adds `A`-offset for hex letters). No port equivalent needed (host printf).

### 34b0:0e86 lseek

C `lseek(handle, offset, whence)` via DOS `INT 21h AH=42h`; handle table at `DS:8299` (flag byte per handle), `DS:8297` = handle count. host C library `lseek`/`fseek` (the game uses `lib_seek` = `intdos` AX=4200h instead).

### 34b0:0f00 _open_stub

Fragment of `open` (no callers; reached only by fall-through): DOS 3+ sharing-mode flag handling before `INT 21h AH=3Dh/3Ch`. Internal.

### 34b0:0f1a open

C `open(path, flags, mode)`: `INT 21h` 3Dh/3Ch/6Ch with O_CREAT (0x100), O_TEXT/O_BINARY (0x4000/0x8000) handling and handle-table update. host C library `open` (the game uses `_dos_open`/`_dos_creat` instead; called from `remove`-adjacent code only).

### 34b0:10ab _nop_stub

Empty function (`retf` only).

### 34b0:10bc read

C `read(handle, buf, n)` (text-mode CR/LF translation, ^Z handling). host C library `read` (the game uses `_dos_read`).

### 34b0:11a6 write

C `write(handle, buf, n)` (text-mode LF -> CR/LF expansion in 0x80-byte chunks via `34b0:125a`). Used by `fflush`/stdout. host C library `write`.

### 34b0:125a _write_chunk

Helper of `write`: `INT 21h AH=40h` for one chunk; error -> `_maperror`. Internal.

### 34b0:12e6 stackavail

C `stackavail()`: bytes left between SP and the stack limit `DS:82C8`. host C library not needed (never called except by `_chkstk` paths).

### 34b0:12fa _nmalloc_stub

Thunk into the near-heap allocator (`34b0:1321`), same code. Internal heap.

### 34b0:1300 _heap_mark_free

Internal heap: marks a block free by setting bit 0 of its header word (called by `_nfree`). Internal.

### 34b0:1321 _nmalloc

C `_nmalloc(size)` (near heap, tries `_heap_search` then `_heap_grow`). host C library `malloc` (the game calls it only indirectly through `_myalloc`/`_fmalloc`).

### 34b0:134a _ffree

C `_ffree(void far *p)`: sets bit 0 of the header word at `p - 2` (free). host C library `free`. Callers: `far_alloc` (reject), `arena_free_all`.

### 34b0:135d _fmalloc

C `_fmalloc(size)`: far-heap allocation; walks the heap segments (`DS:8492..849A`), `_heap_search` (34b0:1444) per segment, grows with DOS `INT 21h AH=48h` (`_heap_new_seg`). host C library `malloc` (see `far_alloc`: blocks whose segment equals DGROUP, `DS:8844`, are rejected by the game).

### 34b0:13da _heap_init_seg

Initialises a new far-heap segment header (size, free list, end marker FFFEh). Internal heap.

### 34b0:140e _heap_link_seg

Links a new heap segment into the segment chain. Internal heap.

### 34b0:1444 _heap_search

First-fit search of a heap segment for a block of `CX` bytes (coalescing free neighbours). Internal heap.

### 34b0:14c0 _heap_grow

Grows the far heap: `_heap_find_end` + DOS `INT 21h AH=4Ah` resize / `AH=48h` new segment, chunk `DS:849E`. Internal heap.

### 34b0:154c _heap_resize_seg

`INT 21h AH=4Ah` wrapper that resizes a heap segment and records its new size. Internal heap.

### 34b0:159d _heap_find_end

Walks a heap segment to its end marker (FFFEh). Internal heap.

### 34b0:15be _heap_new_seg

`INT 21h AH=48h` allocation of a new heap segment, links it (`_heap_init_seg`/`_heap_link_seg`). Internal heap.

### 34b0:1602 strlen

C `strlen`. host C library `strlen`.

### 34b0:161e strncpy

C `strncpy(dst, src, n)` (zero-pads). host C library `strncpy`.

### 34b0:1646 strncmp

C `strncmp(a, b, n)`. host C library `strncmp`.

### 34b0:1680 atoi

Alias entry of `atol` (`34b0:1688`) returning `int`. host C library `atoi`.

### 34b0:1684 atoi_nul

Second alias entry of `atol`. host C library `atoi`.

### 34b0:1688 atol

C `atol(s)`: skips blanks/tabs, optional sign, decimal digits. host C library `atol`.

### 34b0:16dc itoa

C `itoa(value, buf, radix)`. host C library `snprintf("%d")`/own itoa (radix 10 used by the game).

### 34b0:16f8 ltoa_stub

Entry that jumps into the shared integer-to-string converter `34b0:2336` (digit loop, sign for radix 10). Internal to `itoa`/`ltoa`.

### 34b0:1702 getenv

C `getenv(name)` over `DS:82B1`. host C library `getenv` (used for "TZ" by `tzset`).

### 34b0:1762 kbhit

C `kbhit()`: DOS `INT 21h AH=0Bh`/pending char byte `DS:85C1`; used by `video_menu`/`sound_options_menu` only. host C library not needed (text menus).

### 34b0:1788 getch

C `getch()`: DOS `INT 21h AH=07h` (or the pending scan byte `DS:85C0`). host C library not needed (text menus).

### 34b0:17b0 int86

C `int86(intno, &inregs, &outregs)`: builds `CD nn / CB` code on the stack and calls it; marshals AX BX CX DX SI DI (+ carry flag). The game uses it for INT 10h (video modes, text, cursor), INT 21h (print), INT 33h (mouse). **Port:** emulate only the used calls: INT 10h (`bios_set_mode`, `vga_detect`, text menu) - the SDL launcher replaces them; INT 33h -> SDL mouse (`mouse_reset/buttons/motion`).

### 34b0:1830 puts

C `puts(s)`: `fwrite` + "\n" to stdout (`DS:82EC`). Used by error paths only.

### 34b0:18aa sprintf

C `sprintf(buf, fmt, ...)` through `_output` (string FILE at `DS:971C`, flags 0x42). host C library `sprintf`. Used 15 times (numbers into message buffers).

### 34b0:1904 cprintf

C `cprintf(fmt, ...)` (console output through `_putch`). Not used on the VGA path.

### 34b0:192c _cprint

The formatting engine of `cprintf` (a second copy of `_output`, table at `DS:85C2`, sink = `putch`). Not used on the VGA path.

### 34b0:1d37 _cprint_getint

`_cprint` helper (as `_output_getint`). Not used on the VGA path.

### 34b0:1d3f _cprint_getlong

`_cprint` helper (as `_output_getlong`). Not used on the VGA path.

### 34b0:1d4b _cprint_getarg

`_cprint` helper (as `_output_getarg`). Not used on the VGA path.

### 34b0:1d68 _cprint_putc

`_cprint` helper: `putch(c)`. Not used on the VGA path.

### 34b0:1d7f _cprint_pad

`_cprint` helper (pad). Not used on the VGA path.

### 34b0:1d9d _cprint_write

`_cprint` helper (write string). Not used on the VGA path.

### 34b0:1db9 _cprint_cltoa

`_cprint` helper (long to digits). Not used on the VGA path.

### 34b0:1df0 int86x

C `int86x(intno, &in, &out, &segregs)` (like `int86` plus ES/DS from SREGS). Used by `ega_set_palette`/`ega_set_palreg` (INT 10h AX=1000h/1001h/1002h). **Port:** record the palette register writes (`vga_pal[17]`, see video.md "Palettes").

### 34b0:1e86 intdos

C `intdos(&in, &out)` = `int86(0x21, ...)` without the opcode patch. Used by `lib_seek` (AX=4200h). host C library `lseek`/`fseek`.

### 34b0:1ece movedata

C `movedata(srcseg, srcoff, dstseg, dstoff, n)` (`rep movsb`). host C library `memcpy` between port memory spaces (video planes <-> RAM). Used 11 times: picture loading, `far_memcpy`, EGA plane copies.

### 34b0:1eec segread

C `segread(&sregs)`: CS, DS, ES, SS into the structure. Used by `get_ds` (0001:399e) to learn DGROUP; port: constant.

### 34b0:1f04 putch

C `putch(c)`: DOS console output `INT 21h AH=02h`. Used by `cprintf` only.

### 34b0:1f22 time

C `time(&t)`: DOS date (`AH=2Ah`) and time (`AH=2Ch`) converted with `_loctotime`/`__tzset`; reads midnight rollover. host C library `time(NULL)`. Used to seed `rnd`.

### 34b0:1f76 __tzset_once

Calls `tzset` once (`DS:9724` guard). Internal.

### 34b0:1f88 tzset

C `tzset()`: parses the `TZ` environment variable (std name, offset, dst name; seconds in `DS:8664`) via `getenv`. host C library `tzset` (no TZ set on a typical DOS box: no effect).

### 34b0:2030 _isindst

C runtime `_isindst(tm*)`: daylight-saving test for a broken-down time (US rules, tables at `DS:79C0..`). Internal to `time`.

### 34b0:20fa _loctotime

Converts a DOS date/time (year, month, day, h, m, s) to a time_t using `_aFlmul`, the day-of-year tables and `tzset` results. Internal to `time`.

### 34b0:21fc strrchr

C `strrchr(s, c)` (hand-written: `repne scasb` to the end, then scan back). host C library `strrchr`. Callers search for `$` and `.`.

### 34b0:2226 stricmp

C `stricmp(a, b)` (case-insensitive). host C library `strcasecmp`. Used by `main` to match the switches (`nomouse`, `demo`, `nouemem`).

### 34b0:2268 strcspn

C `strcspn(s, reject)` (32-byte bitmap of the reject set). host C library `strcspn`.

### 34b0:22c2 memmove

C `memmove(dst, src, n)` (overlap-safe). host C library `memmove`.

### 34b0:230a memcpy

C `memcpy(dst, src, n)` - **note:** SR1 symbols named the same routine `memset`; the code copies from the second argument (`movsb/movsw`), it is `memcpy`. host C library `memcpy`.

### 34b0:2396 srand

C `srand(seed)`: `DS:86AA:86AC = seed:0`. Not used by the game (own `rnd`).

### 34b0:23a8 rand

C `rand()`: `seed = seed * 0x000343FD + 0x00269EC3; return (seed >> 16) & 0x7FFF`. Not used by the game (`rnd` is the Wichmann-Hill generator, see game_flow).

### 34b0:23d0 remove

C `remove(path)` = DOS `INT 21h AH=41h`; error -> `_dosmaperr`. host C library `remove`. Used by `hall_save` failure path.

### 34b0:23de _bios_equiplist

`INT 11h` (equipment list). Port: return a constant with bit 0 clear (no floppies needed) or the value that `lib_open_all` expects for a hard-disk run.

### 34b0:23e2 _dos_close

`INT 21h AH=3Eh`; returns 0 or the DOS error. host C library `close`/`fclose`.

### 34b0:23f0 _dos_creatnew_stub

Fragment in front of `_dos_creat` (no callers): store handle. Internal.

### 34b0:23f7 _dos_creat

`_dos_creat(path, attr, &handle)` = `INT 21h AH=3Ch`. host C library `open(path, O_CREAT|O_TRUNC|O_WRONLY)` (save games, `hall_dat`).

### 34b0:241a _dos_getvect

`_dos_getvect(n)` = `INT 21h AH=35h` (returns the old vector in ES:BX). Used by `platform_install` for INT 8/9. **Port:** none (the ISRs become host timer/keyboard callbacks).

### 34b0:242c _dos_open

`_dos_open(path, mode, &handle)` = `INT 21h AH=3Dh`. host C library `open(path, O_RDONLY)` / `fopen` (library files, `SR2_data`, `SR2_shp`, sound banks). 12 callers.

### 34b0:2444 _dos_read

`_dos_read(handle, buf far, n, &got)` = `INT 21h AH=3Fh`; `[DS:86B6] == D6D6` hook; returns 0 or error. host C library `read`/`fread`. 14 callers.

### 34b0:244b _dos_write

`_dos_write(handle, buf far, n, &written)` = `INT 21h AH=40h`. host C library `write`/`fwrite`. 4 callers (`hall_save`, save games).

### 34b0:2474 _dos_setvect

`_dos_setvect(n, handler)` = `INT 21h AH=25h`. Used for INT 8 / INT 9 (`platform_install`, `timer_restore`, `kbd_restore`, `race_isr_enter/leave`, `mus_timer_install`). **Port:** replace by registering the host timer/keyboard callbacks.

### 34b0:248a _dos_getdrive

`_dos_getdrive(&drive)`: `INT 21h AH=19h`, stores 1 = A:. Port: constant 3 (C:) = hard-disk layout (`lib_open_all` then also opens LIB3).

### 34b0:249e _harderr

`_harderr(handler)`: stores the handler (`DS:86AE:86B0`) and sets INT 24h to a stub in the runtime that saves SS:SP/the device header and calls the user handler = `crit_err_handler`. Port: no INT 24h; file errors come from the host (map to the `fatal_message` paths).

### 34b0:24ed _hardresume

`_hardresume(action)`: restores the registers saved by the INT 24h stub and returns action (0 ignore, 1 retry, 2 abort) to DOS. Port: n/a.

### 34b0:24f6 _aFldiv

Compiler helper: signed 32/32 division `DX:AX = a / b` (long). C `/` on `long`. host C library `/`.

### 34b0:2590 _aFlmul

Compiler helper: 32x32 -> 32 multiplication. C `*` on `long`.

### 34b0:25c2 _aFlrem

Compiler helper: signed 32/32 remainder. C `%` on `long`.

### 34b0:2662 _aFaldiv

Compiler helper: `*pl /= l` (long /= long) through `_aFldiv`. C `/=` on `long`.

### 34b0:2682 _aFalmul

Compiler helper: `*pl *= l` (long *= long) through `_aFlmul`. C `*=` on `long`.

### 371c:0000 page0_desc

Data, not code (the indexer disassembled it). 18-byte Desc ({u16 w, u16 h, u16 size, far planes, far mask, u16 stride, u8 type, u8 pad}); see video.md "Desc". Value: `w=320 (0x140), h=200 (0xC8), size=0x1F40, planes=A000:0000, mask=0:0, stride=0x28, type=0xFE` (EGA/VGA screen page 0 = visible page). `main` passes `371c:0000` as `front` to `gfx_init`; `gfx_init` changes `type` to FDh for CGA/Hercules/Tandy via `driver_select`.

### 371c:0012 page1_desc

Data, not code (the indexer disassembled it). 18-byte Desc ({u16 w, u16 h, u16 size, far planes, far mask, u16 stride, u8 type, u8 pad}); see video.md "Desc". Value: as page 0 but `planes = A200:0000` (the back buffer page of the menus, CRTC start 0x2000). `back` argument of `gfx_init`.

### 371c:0024 page2_desc

Data, not code (the indexer disassembled it). 18-byte Desc ({u16 w, u16 h, u16 size, far planes, far mask, u16 stride, u8 type, u8 pad}); see video.md "Desc". Template: `w=320, h=200, size=0x4000, planes=A000:2000, mask=0, stride=0x50, type=0xFD` (CGA default). `gfx_init` (VGA) overwrites it with a copy of page 1 and `planes = A000:4000` (race frame buffer B); `DS:9A8A` points at it.

### 3720:0000 page3_desc

Data, not code (the indexer disassembled it). 18-byte Desc ({u16 w, u16 h, u16 size, far planes, far mask, u16 stride, u8 type, u8 pad}); see video.md "Desc". Zero-filled in the file. `gfx_init` (VGA) fills it with a copy of page 1 and `planes = A000:6000` (page 3); `DS:9A8E` points at it.

### 3722:0000 scratch_pad

Data, not code: 2 zero bytes in front of `scratch_desc` (3722:0002).

### 3722:0002 scratch_desc

Data, not code (the indexer disassembled it). 18-byte Desc ({u16 w, u16 h, u16 size, far planes, far mask, u16 stride, u8 type, u8 pad}); see video.md "Desc". Zero-filled in the file; used by the road renderer / event code (`DS:9AD8 = 3722:0002`, `DS:9DC8:9DCA`) as a scratch descriptor that is initialised at run time.

### 3723:0000 race_page_desc

Data, not code (the indexer disassembled it). 18-byte Desc ({u16 w, u16 h, u16 size, far planes, far mask, u16 stride, u8 type, u8 pad}); see video.md "Desc". Value: `w=320, h=200, size=0x1F40, planes=A000:0000, mask=0, stride=0x28, type=0xFE` - a second copy of page 0 used as the race front-buffer descriptor on VGA (`DS:9BC2:9BC4 = 3723:0000`, set by `0dcb` race set-up).

## Handoffs

Functions of the platform list that belong to another module (not specified here):

| Address | Belongs to | What it is |
|---|---|---|
| `1a21:473e` | game_flow (UI) | **dialog item list walker**: calls `1a21:44a8` for every 0x12-byte item until the item whose flag byte has bit 1 set; grows the bounding box `DS:8824/8826/8828/882A` (min x, min y, max x, max y of the items). Part of the new dialog/menu item system (`1a21:44a8` draws one item: types by `flags & 0xFFD8` = 8/0x10/0x40 button frames, 0x80 / 0x100 labels, 0x200/0x400 text lines, 0x800 hot spots; colours from the scheme `DS:9DC6 + 5*level`). |

Functions in **other** modules' lists that the platform spec owns (the module vote went wrong; please do not duplicate the symbol rows):

| Address | Listed under | Really | SR1 |
|---|---|---|---|
| `0001:0090` | game_flow | `set_text_cursor(col, row)`: `INT 10h AH=02h`, BH = 0, DL = col, DH = row (called by `video_menu`, `sound_options_menu`) | new |
| `0001:39cc` | game_flow | `far_free(p)`: `_ffree(p)` (34b0:134a) | `0000:36d6` |
| `1a21:6232` | sound | `out_of_memory()`: `fatal_message(DS:4B70)`, `platform_exit()` | `0000:36e9` |
| `1a21:9b0e` | race | `arena_reset()`: `DS:A31E = 0, DS:BC4C = 0, DS:B7EC = 0, DS:A7D8 = 0x3C, DS:B864 = DS:A0FE` (SR1 also cleared `DS:0294`) | `0f38:a054` |
| `1a21:9b26` | garage | `arena_reset_stacks()`: `DS:A31E = 0, DS:B7EC = DS:BC4C, DS:A7D8 = 0x3C, DS:B864 = DS:A0FE` (keeps the permanent bump area) | `0f38:a07e` |
| `1a21:9b40` | garage | `arena_pop_low(k)`: `if (k == 0) DS:A31E = 0; DS:A31E -= k` | `0f38:a09e` |
| `1a21:9b56` | garage | `arena_pop(k)`: `k == 0`: `DS:A7D8 = 0x3C; DS:B864 = DS:A0FE`; else `DS:A7D8 += k; W[DS:A7D8] = (DS:A7D8 == 0x3C) ? DS:A0FE : W[DS:A7D8 + 1] - 1` (`W` = `DS:B7EC`; `W[0x3C]` = `DS:B864`) - same as SR1 | `0f38:a0b8` |

SR1 platform functions whose SR2 counterparts are listed under other modules (their SR1 sections in `../StreetRod/port/spec/platform.md` apply there): `main` 0001:07b0, `click_clear` 0001:1708, `music_stop` 1728, `music_resume` 1784, `input_reset` 17ea, `timer_restore` 189e, `platform_exit` 18ce, `platform_install` 19e8, `ui_wait` 1d02, `tune_start` 2002, `tune_rewind` 208a,
`race_isr_enter` 20a0, `race_isr_leave` 2176, `timer_isr` 2246, `race_ctrl_reset` 2610, `race_isr` 2622, `wait_click_or_key` 1a21:0dca, `hot_data_load` 1a21:6e6a, `data_disk_check` 7052, `hall_scramble` 70fc, `hall_unscramble` 710c, `hall_load` 711c, `hall_save` 71cc.
Also note for game_flow: `platform_exit` calls `arena_free_all` (1a21:99b0) first, `fatal_message` = 1a21:204c, and `crit_err_handler`/`lib_*` use `bad_file_format` (1a21:6220).

Data passed to other modules: `DS:3B3E..3B46` click records and `DS:3B32/3B34` pointer position (ui_wait), `DS:3BF6` click-seen flag (race wait loops), `DS:B7B4` cpu speed (race/video detail), `DS:86E2` picture directory segment, the libraries' count/handle variables (`hot_data_load`/`data_disk_check` re-select LIB2).

## Open questions

* `DS:4B62` is only ever written with 0 (`lib_open_all`); the early return in `crit_err_handler` is therefore dead in the shipped code. A save-game or copy-protection path might set it; no writer was found.
* `dos_print_line` (image 0x0000-0x009F) is not an indexed function (the index sees a 16-byte stub `0000:0000` and nothing up to `0001:0090`); the symbol row uses the address `0000:0000` as a placeholder for the whole routine.
* Where `DS:81DF` (word added to the polygon pixel address) gets a non-zero value: no writer found; keep it 0.
* `kbd_remap_restore` no longer returns the previous remap state; check game_flow callers (`kbd_remap_screen`) for a use of the return value.
* The semantic of `DS:3BD6` (SR1 `47CF`, sound gate) is shared with the sound module; only listed here for completeness.
* Runtime names: `memcpy` (SR1 seed name `memset`), `strrchr`, `strncmp`, `strncpy`, `strcspn`, `atol` are identified from the code, not from a library match; heap helper names are descriptive only.

