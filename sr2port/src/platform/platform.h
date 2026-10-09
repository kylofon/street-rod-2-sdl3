#pragma once
/* Platform layer of SR.EXE: public API for the game code (game_flow, garage, race) and sound.
 *
 * Implements port/spec/platform.md: start-up (main 0001:07b0 up to the game loop, the text menus, driver
 * selection 0001:046e, video mode init 0001:0628), the timer ISRs 0001:2246 / 0001:2622, timer install /
 * restore / exit, the keyboard INT 9 0001:3014 with its tables, the hot-key remap, text mode, mouse (INT 33h
 * replacement), joystick, memory (DOS arena + far heap in mem[], bitmap descriptors, VRAM picture pools, the
 * two work arenas), files and the picture libraries LIB1 / LIB2 / LIB3.
 *
 * Conventions (PORTING.md): all game state lives in mem[] at its original address; functions are
 * named after port/symbols.csv and carry the original address in a comment; near pointers to DGROUP
 * data are u16 DS offsets, far pointers FarPtr. Integer widths as the original: int = s16/u16.
 * Near pointers the original takes to *stack* buffers (rects, token copies, out-parameters) are C
 * pointers in the port.
 *
 * Hooks into subsystems ported separately (race step, sound, the game_flow message box) are function
 * pointers in modules.h, wired up in modules.c. */
#include "mem.h"
#include "symbols.h"
#include "platform/ds_extra.h"

/* DGROUP string / buffer as a C pointer (mem[] is byte addressed, so this is always valid). */
static inline char *ds_str(u16 off) { return (char *)mp(DGROUP, off); }

/* ============================================================================================ startup */

/* PORT: 0001:070c cpu_speed_calibrate counts loop iterations during one BIOS tick; the port stores a
 * fixed "fast machine" value (any value >= 4 selects the same game logic, 1..3 the slow-machine paths). */
#define SR_CPU_SPEED 20

/* 0001:07b0 main, initialisation part: the switches ("1", nomouse, demo, nouemem, e c v h t; argv[0] is
 * the program name), the text-mode video / sound option menus, heap, driver vectors, gfx_init, arenas,
 * platform_install, sound init (hook), palette DS:1DA6, lib_open_all, slow flag, LIB1 directory,
 * cursor_init, gfx_screen_mode(0) and the resident pictures. Returns when main would call
 * wait_click_or_key(1) / game_loop 0001:4dd6; the port's main.c then calls game_main(). */
void platform_main_init(int argc, char **argv);

/* PORT: the DOS state the original finds at start-up: the memory arena (MCB chain in mem[]: program
 * block from the PSP at LOAD_SEG-10h to DGROUP+1000h as _astart leaves it, the rest free up to A000h)
 * and the BSS cleared. Called first by platform_main_init. */
void dos_init(void);

void driver_select(s16 mode);                 /* 0001:046e: mode 2/3 only (VGA path): DS:AD1C = -2, vectors */
void video_mode_init(s16 mode);               /* 0001:0628: mode 0Dh (vga_mode_0d), DS:0048 = 1 */
void slow_flag_set(void);                     /* 0001:079a: DS:ADD8 = (s16)DS:B7B4 < 4 */
void empty_stub(void);                        /* 0001:07ae */

/* ============================================================================ timer, exit, the ISRs */

/* 0001:19e8 platform_install: joystick detection + calibration (a gamepad connected now), mouse reset,
 * sound state (hook), PIT 4000h (72.8 Hz), INT 8 = timer_isr, INT 9 = kbd_isr (host keyboard),
 * input_reset (game_flow). */
void platform_install(void);
void timer_restore(void);                     /* 0001:189e: tick handler off, DS:1F4E = DS:0044 = 0 */
_Noreturn void platform_exit(void);           /* 0001:18ce (game_flow): "Quit demo" box, restore, exit */

void timer_isr(void);                         /* 0001:2246: normal INT 8 body, run once per host tick */
void race_isr(void);                          /* 0001:2622: platform part; sound and race step via hooks */
void race_isr_enter(void);                    /* tick handler = race_isr, mouse y shift 6 */
void race_isr_leave(void);                    /* tick handler = timer_isr, mouse shifts 1/1 */
s16  sign(s16 v);                             /* 0001:2222: -1 / 0 / 1 */

/* Tick counter DS:1F66 (u32, 72.8 Hz) and the 18.2 Hz counter DS:1F6E. */
static inline u32 ticks_now(void) { return DSL(DS_ticks); }
/* The original's signed 32-bit compare "deadline > now" (high word signed, low word unsigned). */
static inline bool tick_after(u32 a, u32 b) { return (s32)a > (s32)b; }

/* ======================================================================================= keyboard */

void kbd_isr(u8 sc);                          /* 0001:3014: one scan code byte (port 60h), XT set 1 */
s16  key_read(void);                          /* 0001:3168: read-and-clear the key slot DS:3BD7 (AL, 0-extended) */
void kbd_install(void);                       /* 0001:3175: host keyboard -> kbd_isr */
void kbd_restore(void);                       /* 0001:3194 */
/* PORT: A / Z during a race: +1 shift up, -1 shift down, 0 none; taking it clears it. Read by the
 * race physics step. Not in the original. */
s8   kbd_take_shift_dir(void);
void kbd_remap_restore(s16 keep_low);         /* 0001:1bea: undo the hot-key remap (saved pairs DS:1FBE) */
void kbd_hotspot_refresh(void);               /* 0001:1c5c: rebuild the hot-key remap of the current menu + 'M' */
void kbd_text_mode_on(s16 keep_space);        /* 0001:2fd6: Backspace, ',', '.', Space become keys */
void kbd_text_mode_off(void);                 /* 0001:2ffd */

/* ===================================================================== clicks, input polling */

void click_clear(void);                       /* 0001:1708 */
void input_poll_any(void);                    /* 0001:8c2c: mouse / joystick button or Esc/Enter -> DS:3BF6 */
/* game_flow (S5.2): input_reset 0001:17ea, ui_wait 0001:1d02, wait_ticks 0001:3d80, wait_click_or_key
 * 1a21:0dca. Until then game/flow_stub.c has the few the start-up chain needs. */
void input_reset(void);
void wait_click_or_key(s16 n);

/* ================================================================================ mouse, joystick */

s16  mouse_reset(void);                       /* 1a21:769c: -1 = mouse present (0 with nomouse / no host mouse) */
void mouse_buttons(void);                     /* 1a21:76d6: DS:1F5C = left, DS:1F5E = right (value 2) */
void mouse_motion(s16 *dx, s16 *dy);          /* 1a21:7708: mickeys since the last call */
void mouse_poll(void);                        /* 1a21:7738: DS:1F50/1F52 scaled delta, left button -> click */
void joy_read(void);                          /* 0001:31a6: DS:3B20/3B22, DS:3B24, click (gamepad) */

/* ========================================================================================= memory */

/* 0001:39de far_alloc: _fmalloc (far heap blocks in DOS memory inside mem[]; PORT: one DOS block per
 * allocation, see dos.c); blocks inside DGROUP are rejected. NULL on failure, or out_of_memory() if fatal. */
FarPtr far_alloc(u16 size, s16 fatal);
void   far_free(FarPtr p);                    /* 0001:39cc: _ffree */
_Noreturn void out_of_memory(void);           /* 1a21:6232: "Not enough memory to run Street Rod !" */
u16    get_ds(void);                          /* 0001:399e: DGROUP segment */
void   mem_pools_init(void);                  /* 0001:39b4: DS:849E = 800h, DS:8844 = DS, pools_init(1) */
void   pools_init(s16 full);                  /* 0001:3934 (game_flow): the car / part free lists */

/* The far heap (_fmalloc 34b0:135d / _ffree 34b0:134a) and DOS memory (INT 21h 48h/4Ah) in mem[]. */
FarPtr crt_fmalloc(u16 size);
void   crt_ffree(FarPtr p);

/* Bitmap descriptors (18 bytes, video.md "Desc"; accessors in video.h). */
FarPtr bitmap_alloc(s16 w, s16 h, s16 flags, s16 drv, FarPtr d);        /* 0001:3a42 (d NULL: far_alloc) */
FarPtr bitmap_make(FarPtr data, FarPtr mask, s16 w, s16 h, s16 drv);    /* 0001:3b70 */
void   bitmap_init(FarPtr d, FarPtr data, FarPtr mask, s16 w, s16 h, s16 drv); /* 0001:3bfa */
FarPtr vram_pool_alloc(u16 size, u8 *plane);  /* 0001:3c60: A000:7D00+ in one plane; NULL, *plane = FFh if full */

/* The 65000-byte work arena DS:95F2 with its four allocation modes (platform.md "Arena"):
 * 0 permanent bump, 1 low stack, 2 top stack (released by arena_pop), 3 top permanent. Arena 2
 * (DS:9E6C, 45000 bytes on VGA) is bumped by arena_bitmap_alloc(pool -1) and holds the SR2_SHP sets. */
void   arena_init(void);                      /* 1a21:995c */
void   arena_clear(void);                     /* 1a21:9940 */
void   arena_free_all(void);                  /* 1a21:99b0: nothing to do in the port */
FarPtr arena_alloc(s16 n, s16 mode);          /* 1a21:99d2 */
void   arena_reset(void);                     /* 1a21:9b0e */
void   arena_reset_stacks(void);              /* 1a21:9b26 */
void   arena_pop_low(s16 n);                  /* 1a21:9b40: n = 0 empties the low stack */
void   arena_pop(s16 n);                      /* 1a21:9b56: releases n top (mode 2) allocations; 0 = all */

/* ========================================================================== DOS files (INT 21h) */

/* The original's file calls (MS C _dos_* and intdos). Names are C strings as the game builds them
 * ("C:lib1", ":HOTROD.SAV"; DGROUP strings via ds_str()): a drive prefix "X:" and any directory are
 * dropped and the file is looked up case-insensitively in the game directory (host_game_path). Buffers are far
 * pointers into mem[]. Handles are small integers (5, 6, ... as DOS hands them out); 0 = none.
 * Return 0 on success, else a DOS error code (2 file not found, 5 access denied, 6 bad handle). */
u16  dos_open(const char *name, u16 mode, u16 *handle);            /* 1e16:2330 _dos_open (mode 0 read, 1 write, 2 r/w) */
u16  dos_creat(const char *name, u16 attr, u16 *handle);           /* 1e16:2305 _dos_creat (create / truncate) */
u16  dos_read(u16 handle, FarPtr buf, u16 n, u16 *got);    /* 1e16:2348 _dos_read */
u16  dos_write(u16 handle, FarPtr buf, u16 n, u16 *put);   /* 1e16:234f _dos_write */
u16  dos_close(u16 handle);                                /* 1e16:22f0 _dos_close */
u16  dos_remove(const char *name);                                /* 1e16:22de remove: 0 or -1 */
void dos_getdrive(u16 *drive);                             /* 1e16:2382 _dos_getdrive: PORT: always 3 (C:) */
s16  lib_seek(u16 handle, s32 pos);                        /* 0f38:6016: intdos AX=4200h; 0 or the error */

/* ================================================================================ picture libraries */

/* Picture directory: segment DS:86E2 (0x3726 in the file), 0x22 bytes per picture (FORMATS.md). Ids:
 * 1..4 = LIB1, 1000.. = LIB2 #(id-1000), 2000.. = LIB3 #(id-2000). */
s16  pic_index(s16 id);                       /* 1a21:629a */
void lib_select(s16 id);                      /* 1a21:62c4: one library open at a time */
s16  lib_read_dir(s16 lib);                   /* 1a21:6432: lib 1 / 2 / 3; returns the largest packed size */
s16  pic_load(s16 id, s16 to_vram, FarPtr dst);  /* 1a21:652a: 0 no memory, 1 loaded, 2 RAM, 3 parked */
void pic_free(s16 id);                        /* 1a21:67e8 */
FarPtr pic_decode(s16 id, FarPtr dst);        /* 1a21:683a: unpack into dst (re-initialised), in place */
void pic_info(s16 id, s16 *w, s16 *h, s16 *masked);   /* 1a21:6ac6 */
FarPtr pic_get(s16 id, s16 pool);             /* 1a21:6b04: arena bitmap + pic_decode (pool -1: arena 2) */
s16  pic_park_list(u16 ids);                  /* 1a21:6b50: 0-terminated u16 list at DS:ids; skipped with nouemem */
s16  pic_load_list(u16 ids, s16 keep_going);  /* 1a21:6b80: returns the number loaded */
void pic_free_list(u16 ids);                  /* 1a21:6bc0 */
void lib_open_all(s16 disks, s16 mode);       /* 1a21:6be0 */
_Noreturn void file_not_found(const char *name);   /* 1a21:61e2 */
_Noreturn void bad_file_format(void);         /* 1a21:6220 */
_Noreturn void copy_all_files(void);          /* 1a21:6244 */

/* ===================================================================================== game_flow data */

/* PORT: game_flow functions (S5.2) the start-up chain calls; game/flow_stub.c holds stand-ins. */
void hot_data_load(void);                     /* 1a21:6e6a */
void hall_load(void);                         /* 1a21:711c */
s16  rnd(s16 n);                              /* 1a21:6180: n <= 0 seeds from the clock, returns -1 */
