/* game_flow: start-up chain (title sequence, title_and_setup), the random generator and the data files SR2_data and
 * hall_dat — port/spec/game_flow.md, FORMATS.md "SR2_DATA" (0001:3cc4, 1a21:0e82, 1a21:6180, 1a21:6e6a-71cc). */
#include "game/flow.h"
#include "game/race.h"

#include <stdlib.h>
#include <time.h>

#include "game/records.h"
#include "game/snd.h"
#include "game/ui.h"
#include "modules.h"
#include "platform/platform.h"
#include "platform/video.h"

/* 1a21:6180 rnd: a 32-bit LCG (SR1: Wichmann-Hill). n < 1: seed = time(NULL) (DS:4A84 / 4A86), returns -1. */
s16 rnd(s16 n)
{
    if (n < 1) {
        DSL(DS_rnd_seed) = getenv("SR_SEED") ? (u32)atol(getenv("SR_SEED")) : (u32)time(NULL);   /* PORT: SR_SEED = developer aid (reproducible runs) */
        return -1;
    }
    u32 seed = DSL(DS_rnd_seed) * 0x015A4E35u + 1;
    DSL(DS_rnd_seed) = seed;
    return (s16)(((seed >> 16) & 0x7FFF) % (u32)(u16)n);
}

/* 1a21:0e82 title_sequence: "California Dreams presents", the title, the credits (two waits on the credits) */
void title_sequence(void)
{
    u16 h = DSW(DS_pic_rows);
    if (DSS(DS_driver_id) == -2 || DSS(DS_driver_id) == -6) drv_pal_black();
    DSW(DS_pic_rows) = 200;
    /* (not VGA: cga_set_palette(0)) */
    show_picture_at(3, 0x28, 0x28, 0);                /* LIB1 #2 "California Dreams presents" */
    pic_free(3);
    drv_pal_normal();
    DSW(DS_g_mirror) = 0;
    show_picture(1, -1);                              /* LIB1 #0 title, drawn off screen */
    pic_free(1);
    wait_click_or_key(400);
    drv_pal_normal();                                 /* title visible */
    /* (not VGA: cga_set_palette(1)) */
    drv_blinds(desc_planes(g_front()), desc_planes(g_back()));
    show_picture(4, -1);                              /* LIB1 #3 credits, off screen */
    pic_free(4);
    wait_ticks(0x14);
    wait_click_or_key(400);
    drv_blinds(desc_planes(g_front()), desc_planes(g_back()));   /* credits visible */
    wait_ticks(0x14);
    wait_click_or_key(600);
    DSW(DS_pic_rows) = h;
    screen_fill_rect(0, 0, 0x140, 200, 0);
    drv_blinds(desc_planes(g_front()), desc_planes(g_back()));   /* black */
}

/* 0001:3cc4 title_and_setup: returns -1 (game_loop then runs the new game / load flow) */
s16 title_and_setup(void)
{
    gfx_screen_mode(0);
    sound_vec(18);                                    /* the title music (DS:BBD4, S5.4) */
    title_sequence();
    drv_pal_black();
    gfx_screen_mode(1);
    if (!DSW(DS_sw_demo)) {                           /* SR1: protection skipped; SR2 asks (never checks) */
        if (copy_protection() == 0) platform_exit();
        cursor_ctl(-4);
    }
    track_build_all(1);                                /* new road seed, stored at DS:A9F8 */
    DSB(DS_save_name) = 0;
    DSB(DS_player_name) = 0;
    DSW(DS_demo) = DSW(DS_sw_demo);
    if (!DSW(DS_title_resident)) {                    /* the start-up chain could not load the big pictures */
        lib_read_dir(2);
        if (DSW(DS_snd_device) == 0) modules.mus_songs_load();
        hot_data_load();
        hall_load();
        if (DSS(DS_driver_id) == -2) pic_park_list(DS_RESIDENT_PICS);
    }
    pic_load_list(DS_RESIDENT_PICS, 1);
    cursor_ctl(-2);
    return -1;
}

/* ================================================================================ SR2_data */

/* 1a21:6e6a hot_data_load: the nine blocks of SR2_data (FORMATS.md) into DGROUP, then the text pool; the pointer fix-ups.
 * (The original advances the name pointer DS:14D8 while it copies "SR2_data"; afterwards it equals DS:14DA, the pointer of
 * "SR2_end". Called once.) */
void hot_data_load(void)
{
    u16 drive = DSW(DS_lib2_path);
    DSB(DS_data_path) = DSB(drive);
    DSB((u16)(DS_data_path + 1)) = DSB((u16)(drive + 1));
    u16 d = (u16)(DS_data_path + 2);
    for (;;) {
        u8 c = DSB(DSW(DS_data_name_ptr));
        DSW(DS_data_name_ptr)++;
        DSB(d++) = c;
        if (c == 0) break;
    }
    u16 fd;
    if (dos_open(ds_str(DS_data_path), 0, &fd) != 0) {
        modules.fatal_message(ds_str(DS_msg_cant_open));          /* "Can't open data file" */
        platform_exit();
    }
    DSW(DS_data_dst7) = 0x8E3C;
    for (u16 k = 0; k < 9; k++) {
        u16 n, size = DSW((u16)(DS_data_size + 2 * k));
        if (dos_read(fd, ds_ptr(DSW((u16)(DS_data_dst + 2 * k))), size, &n) != 0 || n != size) {
            modules.fatal_message(ds_str(DS_msg_data_read));      /* "Data disk read fail" */
            platform_exit();
        }
    }
    {
        u16 n, last = DSW(DS_text_pool_end);
        if (dos_read(fd, ds_ptr(DS_text_pool), (u16)(last + 0x50), &n) != 0 || last > n) {
            modules.fatal_message(ds_str(DS_msg_data_read));
            platform_exit();
        }
    }
    dos_close(fd);
    for (u16 k = 0; k < 6; k++) {                     /* road bitmaps: three near pointers into DS:8E3C */
        u16 p = (u16)(0x8E3C + 0x42 * k);
        DSW((u16)(DS_road_bitmaps + 14 * k)) = p;
        DSW((u16)(DS_road_bitmaps + 14 * k + 2)) = (u16)(p + 0x16);
        DSW((u16)(DS_road_bitmaps + 14 * k + 6)) = (u16)(p + 0x2C);
    }
    DSW(DS_police_sprite_set) = 0xAAE4;
    for (u16 i = 0; i < MODEL_COUNT; i++) {
        u16 m = (u16)(DS_MODELS + MODEL_REC * i);
        DSW((u16)(m + 0x0C)) = (u16)(DSW((u16)(DS_ad_text_tbl + 2 * i)) + DS_text_pool);   /* car ad text pointer */
        DSB((u16)(m + 6)) = (u8)rnd(5);                                                     /* random byte */
    }
    for (u16 i = 0; i < FLOW_PEOPLE_N; i++)
        DSW((u16)(DS_people + FLOW_PERSON_SIZE * i + 6)) = (u16)(DSW((u16)(DS_person_name_tbl + 2 * i)) + DS_text_pool);
    if (DSS(DS_cur_drive) < 3 && DSW(DS_floppy_drives) == 1) {     /* floppy install, one drive: is SR2_shp here? */
        DSB(DS_shp_name) = DSB(DS_data_path);
        if (dos_open(ds_str(DS_shp_name), 0, &fd) != 0) {
            DSW(DS_shp_missing) = 1;
            return;
        }
        dos_close(fd);
    }
}

/* 1a21:7052 data_disk_check: the drive letter of the data disk. Floppy: opens the data file, asking for disk 2 / 3 up to
 * three times; hard disk: '@' + drive ('C'). */
char data_disk_check(void)
{
    s16 tries = 0;
    u16 fd;
    if (DSS(DS_cur_drive) > 2) return (char)(DSS(DS_cur_drive) + 0x40);
    while (dos_open(ds_str(DS_data_path), 0, &fd) != 0) {
        if (++tries > 3) platform_exit();
        u16 prompt = DSW(DS_msg_insert_disk);
        DSB((u16)(prompt + 0x0C)) = (DSS(DS_driver_id) == -2 || DSS(DS_driver_id) == -6) ? '2' : '3';
        DSB((u16)(prompt + 0x19)) = DSB(DSW(DS_lib2_path));
        modules.fatal_message(ds_str(prompt));
    }
    dos_close(fd);
    lib_select(1000);
    return (char)DSB(DS_data_path);
}

/* ================================================================================ hall_dat */

/* 1a21:70fc hall_scramble: bit 7 set on the first 0xE6 bytes of the table */
void hall_scramble(void)
{
    for (u16 i = 0; i < 0xE6; i++) DSB((u16)(DS_hall + i)) |= 0x80;
}

/* 1a21:710c hall_unscramble */
void hall_unscramble(void)
{
    for (u16 i = 0; i < 0xE6; i++) DSB((u16)(DS_hall + i)) &= 0x7F;
}

/* 1a21:711c hall_load: u16 count, then count records of 0x17 bytes; a missing file is an empty hall */
void hall_load(void)
{
    u16 fd = 0, n;
    DSB(DS_hall_path) = DSB(DSW(DS_lib2_path));
    data_disk_check();
    if (dos_open(ds_str(DS_hall_path), 0, &fd) != 0 || fd == 0) {
        DSW(DS_hall_count) = 0;
        return;
    }
    if (dos_read(fd, ds_ptr(DS_hall_count), 2, &n) != 0 || n != 2) bad_file_format();
    for (s16 i = 0; DSS(DS_hall_count) > i; i++)
        if (dos_read(fd, ds_ptr((u16)(DS_hall + 0x17 * i)), 0x17, &n) != 0 || n != 0x17) bad_file_format();
    dos_close(fd);
    hall_unscramble();
}

/* 1a21:71cc hall_save. Failure: the "file not found" box (DS:4B6C), the file removed, the box again. */
void hall_save(void)
{
    u16 fd = 0, n;
    u16 err;
    hall_scramble();
    data_disk_check();
    err = dos_creat(ds_str(DS_hall_path), 0, &fd);
    if (DSS(DS_cur_drive) > 2) {                      /* hard disk */
        if (fd == 0 || err != 0) {
            modules.fatal_message(ds_str(DSW(DS_msg_not_found)));
            platform_exit();
        }
    } else {                                          /* floppy: up to three prompts */
        s16 tries = 0;
        DSB((u16)(DSW(DS_msg_insert_disk) + 0x0C)) = '1';
        DSB((u16)(DSW(DS_msg_insert_disk) + 0x19)) = DSB(DS_hall_path);
        while (!(fd != 0 && err == 0)) {
            if (tries == 3) platform_exit();
            modules.fatal_message(ds_str(DSW(DS_msg_bad_format)));
            err = dos_creat(ds_str(DS_hall_path), 0, &fd);
            tries++;
        }
    }
    bool ok = dos_write(fd, ds_ptr(DS_hall_count), 2, &n) == 0 && n == 2;
    for (s16 i = 0; ok && DSS(DS_hall_count) > i; i++)
        ok = dos_write(fd, ds_ptr((u16)(DS_hall + 0x17 * i)), 0x17, &n) == 0 && n == 0x17;
    if (ok) ok = dos_close(fd) == 0;
    if (!ok) {
        modules.fatal_message(ds_str(DSW(DS_msg_not_found)));
        dos_remove(ds_str(DS_hall_path));
        modules.fatal_message(ds_str(DSW(DS_msg_not_found)));
    }
    hall_unscramble();
}
