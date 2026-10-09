/* Picture libraries LIB1 / LIB2 / LIB3 - platform.md 1a21:61e2-6be0, video.md pic_info / pic_get (file
 * layout: FORMATS.md "Picture libraries").
 *
 * Picture directory: segment DS:86E2 (0x3726 in the file), 0x22 bytes per picture (28-byte file record plus
 * the parked plane and the packed far pointer). One library handle is open at a time (lib_select); packed
 * pictures are loaded into far memory, into the tail of the destination bitmap (SR2 decodes in place) or
 * parked in one plane of the off-screen EGA memory (A000:7D00+, through the VGA register model).
 * Message boxes (1a21:204c) go through modules.fatal_message. The floppy-disk prompts cannot trigger in
 * the port (dos_getdrive reports C:), but their logic is kept. */
#include "platform/platform.h"
#include "platform/video.h"
#include "platform/vga.h"
#include "modules.h"

#include <string.h>

/* Picture directory entry fields (FORMATS.md; 0x22 bytes) */
#define PE_W       0x00
#define PE_H       0x02
#define PE_DEPTH   0x04
#define PE_MASKED  0x05
#define PE_TOKENS  0x06
#define PE_OFFSET  0x16    /* u32 from the end of the directory */
#define PE_SIZE    0x1A    /* u16 packed size (without the trailing BCh) */
#define PE_PLANE   0x1C    /* u8 parked plane, FFh = none */
#define PE_PACKED  0x1E    /* far packed data, NULL = not loaded */
#define PE_BYTES   0x22

/* 1a21:204c with a DGROUP string */
static void message_box(u16 s)
{
    modules.fatal_message(ds_str(s));
}

/* The message strings are reached through pointers in DGROUP (DS:4B66..4B70). */
static u16 msg(u16 slot) { return DSW(slot); }

/* 1a21:61e2 file_not_found: "File not found: " + name, box, exit. */
_Noreturn void file_not_found(const char *name)
{
    char *buf = ds_str(msg(DS_MSG_NOT_FOUND));
    strcpy(buf + 0x10, name);
    modules.fatal_message(buf);
    platform_exit();
}

/* 1a21:6220 bad_file_format */
_Noreturn void bad_file_format(void)
{
    message_box(msg(DS_msg_bad_format));
    platform_exit();
}

/* 1a21:6244 copy_all_files */
_Noreturn void copy_all_files(void)
{
    message_box(msg(DS_msg_copy_files));
    platform_exit();
}

/* 1a21:629a pic_index: LIB1 ids 1.. -> 0..3, LIB2 ids 1000.. -> 4..215, LIB3 ids 2000.. -> 216..298. */
s16 pic_index(s16 id)
{
    if (id < 1000) return (s16)(id - 1);
    if (id < 2000) return (s16)(id - 0x3E4);
    return (s16)(id - 0x6F8);
}

/* Entry of the picture at directory index i (imul 22h, low word). */
static FarPtr pic_entry(s16 i)
{
    return far_make(DSW(DS_picdir_seg), (u16)((s32)i * PE_BYTES));
}

/* 1a21:62c4 lib_select: makes the library holding id the open one (closing the previously selected one),
 * prompts for its disk until it opens (3 tries), checks the picture count. */
void lib_select(s16 id)
{
    s16 ix = pic_index(id);
    u16 expect, count_var, name;
    u8 disk;
    DSW(DS_prev_lib_handle) = DSW(DS_cur_lib_handle);
    if (ix < 4) {
        DSW(DS_cur_lib_handle) = DS_lib1_handle;
        count_var = DS_lib1_count; name = DSW(DS_lib1_path); expect = 4; disk = '1';
    } else if (ix < 0xD8) {
        DSW(DS_cur_lib_handle) = DS_lib2_handle;
        count_var = DS_lib2_count; name = DSW(DS_lib2_path); expect = 0xD4;
        disk = (DSS(DS_driver_id) == -2 || DSS(DS_driver_id) == -6) ? '2' : '3';
    } else {
        DSW(DS_cur_lib_handle) = DS_lib3_handle;
        count_var = DS_lib3_count; name = DSW(DS_lib3_path); expect = 0x53; disk = '4';
    }
    DSW(DS_cur_lib_count) = count_var;
    DSW(DS_cur_lib_name) = name;
    if (DSW(DSW(DS_cur_lib_handle)) != 0) return;

    if (DSW(DS_prev_lib_handle) != 0) {                          /* PORT: the original closes handle 0 on the first call */
        dos_close(DSW(DSW(DS_prev_lib_handle)));
        DSW(DSW(DS_prev_lib_handle)) = 0;
    }
    u16 m = msg(DS_msg_insert_disk);
    DSB(m + 0x0C) = disk;
    DSB(m + 0x19) = DSB(DSW(DS_cur_lib_name));
    s16 si = 0;
    while (dos_open(ds_str(DSW(DS_cur_lib_name)), 0, &DSW(DSW(DS_cur_lib_handle))) != 0) {
        if (si++ == 3) platform_exit();
        message_box(m);
    }
    u16 got = 0;
    if (dos_read(DSW(DSW(DS_cur_lib_handle)), ds_ptr(DSW(DS_cur_lib_count)), 2, &got) != 0 || got != 2 ||
        DSW(DSW(DS_cur_lib_count)) != expect)
        bad_file_format();
}

/* 1a21:6432 lib_read_dir: the 28-byte records of LIB1 / LIB2 / LIB3 into the 0x22-byte directory entries
 * (parked plane FFh, packed pointer 0); returns the largest packed size (unsigned compare). */
s16 lib_read_dir(s16 lib)
{
    s16 base, count;
    if (lib == 1) { base = 0; count = 4; lib_select(1); }
    else if (lib == 2) { base = (s16)DSW(DS_lib1_count); count = 0xD4; lib_select(1000); }
    else { base = (s16)(DSW(DS_lib2_count) + DSW(DS_lib1_count)); count = 0x53; lib_select(2000); }
    u16 maxsz = 0;
    u16 pos = 2;                                                 /* file position of the record */
    for (s16 i = 0; i < count; i++, pos = (u16)(pos + 0x1C)) {
        FarPtr e = pic_entry((s16)(base + i));
        u16 got = 0;
        if (lib_seek(DSW(DSW(DS_cur_lib_handle)), (s32)(s16)pos) != 0 ||         /* cwd */
            dos_read(DSW(DSW(DS_cur_lib_handle)), e, 0x1C, &got) != 0 || got != 0x1C)
            bad_file_format();
        wr8(e.seg, (u16)(e.off + PE_PLANE), 0xFF);
        far_wr(e.seg, (u16)(e.off + PE_PACKED), far_make(0, 0));
        if (rd16(e.seg, (u16)(e.off + PE_SIZE)) > maxsz) maxsz = rd16(e.seg, (u16)(e.off + PE_SIZE));
    }
    return (s16)maxsz;
}

/* 1a21:652a pic_load: 0 no memory, 1 already loaded, 2 loaded into RAM (dst or far_alloc), 3 parked in one
 * plane of the off-screen EGA memory (read into a temporary top arena block, copied with movedata under
 * map mask = that plane). size + 1 bytes are read; both the first and the last must be BCh (floppy: retry
 * after a disk prompt; hard disk: "bad file format"). */
s16 pic_load(s16 id, s16 to_vram, FarPtr dst)
{
    FarPtr e = pic_entry(pic_index(id));
    FarPtr vram = far_make(0, 0);
    u8 plane = 0;

    if (!far_is_null(far_rd(e.seg, (u16)(e.off + PE_PACKED)))) return 1;
    if (to_vram != 0) {
        vram = vram_pool_alloc(rd16(e.seg, (u16)(e.off + PE_SIZE)), &plane);
        FarPtr p = far_make(0, 0);
        if (!far_is_null(vram))
            p = arena_alloc((s16)(rd16(e.seg, (u16)(e.off + PE_SIZE)) + 1), 2);
        far_wr(e.seg, (u16)(e.off + PE_PACKED), p);
        wr8(e.seg, (u16)(e.off + PE_PLANE), plane);
    } else {
        FarPtr p = !far_is_null(dst) ? dst : far_alloc((u16)(rd16(e.seg, (u16)(e.off + PE_SIZE)) + 1), 0);
        far_wr(e.seg, (u16)(e.off + PE_PACKED), p);
        wr8(e.seg, (u16)(e.off + PE_PLANE), 0xFF);
    }
    if (far_is_null(far_rd(e.seg, (u16)(e.off + PE_PACKED)))) return 0;

    lib_select(id);
    /* (count * 1Ch + 2) sign-extended (imul low word, cwd) + the 32-bit offset */
    s32 pos = (s32)(s16)(u16)((s32)DSS(DSW(DS_cur_lib_count)) * 0x1C + 2);
    pos = (s32)((u32)pos + rd32(e.seg, (u16)(e.off + PE_OFFSET)));
    if (lib_seek(DSW(DSW(DS_cur_lib_handle)), pos) != 0) bad_file_format();
    if (rd8(e.seg, (u16)(e.off + PE_PLANE)) != 0xFF) {
        out(0x3C4, 2);
        out(0x3C5, (u8)(1u << (rd8(e.seg, (u16)(e.off + PE_PLANE)) & 0x1F)));   /* map mask: one plane */
    }
    s16 tries = 0;
    for (;;) {
        u16 n = (u16)(rd16(e.seg, (u16)(e.off + PE_SIZE)) + 1), got = 0;
        FarPtr b = far_rd(e.seg, (u16)(e.off + PE_PACKED));
        if (dos_read(DSW(DSW(DS_cur_lib_handle)), b, n, &got) != 0) bad_file_format();
        if (got != n) bad_file_format();
        if (rd8(b.seg, b.off) == 0xBC && rd8(b.seg, (u16)(b.off + n - 1)) == 0xBC) break;
        if (DSS(DS_cur_drive) > 2) bad_file_format();            /* hard disk: no retry */
        do {                                                     /* floppy: ask for the disk again */
            if (++tries > 3) platform_exit();
            u8 al;
            if (DSW(DSW(DS_cur_lib_handle)) == DSW(DS_lib1_handle)) al = 1;   /* sic: 01h, not '1' */
            else if (DSS(DS_driver_id) == -2 || DSS(DS_driver_id) == -6) al = '2';
            else al = '3';
            u16 m = msg(DS_msg_insert_disk);
            DSB(m + 0x0C) = al;
            DSB(m + 0x19) = DSB(DSW(DS_cur_lib_name));
            message_box(m);
            dos_close(DSW(DSW(DS_cur_lib_handle)));
        } while (dos_open(ds_str(DSW(DS_cur_lib_name)), 0, &DSW(DSW(DS_cur_lib_handle))) != 0);
        if (lib_seek(DSW(DSW(DS_cur_lib_handle)), pos) != 0) bad_file_format();
    }
    if (rd8(e.seg, (u16)(e.off + PE_PLANE)) == 0xFF) return 2;
    {
        FarPtr b = far_rd(e.seg, (u16)(e.off + PE_PACKED));
        vmovedata(b.seg, b.off, vram.seg, vram.off, rd16(e.seg, (u16)(e.off + PE_SIZE)));   /* to one plane */
        far_wr(e.seg, (u16)(e.off + PE_PACKED), vram);
        arena_pop(1);
        out(0x3C4, 2);
        out(0x3C5, 0x0F);
    }
    return 3;
}

/* 1a21:67e8 pic_free: far_free the packed copy, packed pointer = NULL. */
void pic_free(s16 id)
{
    FarPtr e = pic_entry(pic_index(id));
    FarPtr p = far_rd(e.seg, (u16)(e.off + PE_PACKED));
    if (!far_is_null(p)) {
        far_free(p);
        far_wr(e.seg, (u16)(e.off + PE_PACKED), far_make(0, 0));
    }
}

/* 1a21:683a pic_decode: hide the cursor, make dst a bitmap for the picture (bitmap_alloc(w, h, masked, 1,
 * dst)), read the packed data into the tail of that bitmap (unless resident or parked) and unpack in place.
 * A parked picture is read back through the read map select GC4 = its plane (left set, sic); the map mask
 * is restored to 0Fh. */
FarPtr pic_decode(s16 id, FarPtr dst)
{
    cursor_ctl(-4);                                              /* hide */
    FarPtr e = pic_entry(pic_index(id));
    dst = bitmap_alloc((s16)rd16(e.seg, (u16)(e.off + PE_W)), (s16)rd16(e.seg, (u16)(e.off + PE_H)),
                       (s16)rd8(e.seg, (u16)(e.off + PE_MASKED)), 1, dst);
    FarPtr data = desc_planes(dst), mask = desc_mask(dst);
    u16 size = rd16(e.seg, (u16)(e.off + PE_SIZE));
    FarPtr tail;
    if (!far_is_null(mask)) tail = far_make(mask.seg, (u16)(mask.off + desc_stride(dst) * desc_h(dst)));
    else tail = far_make(data.seg, (u16)(data.off + desc_size(dst)));
    tail.off = (u16)(tail.off - size + 1);
    s16 r = pic_load(id, 0, tail);
    u8 tokens[16];                                               /* the stack copy */
    for (u16 i = 0; i < 16; i++) tokens[i] = rd8(e.seg, (u16)(e.off + PE_TOKENS + i));
    if (rd8(e.seg, (u16)(e.off + PE_PLANE)) != 0xFF) {
        out(0x3CE, 4);
        out(0x3CF, rd8(e.seg, (u16)(e.off + PE_PLANE)));         /* read map select: the parked plane */
    }
    FarPtr src = far_rd(e.seg, (u16)(e.off + PE_PACKED));
    src.off++;                                                   /* skip the leading BCh */
    u16 n = (u16)(size - 1);
    if (DSS(DS_driver_id) == -2) {
        if (far_is_null(data)) {
            pic_unpack(src, mask, n, tokens, 0x10);
        } else {
            pic_unpack(src, data, n, tokens, 0x10);
            if (!far_is_null(mask)) make_mask_plane(data, mask, desc_size(dst));
        }
    } else {
        /* PORT: the Tandy decoders (3210:005a / 0120 / 023c) and the CGA picture order are not ported. */
        pic_unpack(src, !far_is_null(data) ? data : mask, n, tokens, 0x10);
    }
    if (rd8(e.seg, (u16)(e.off + PE_PLANE)) != 0xFF) {
        out(0x3C4, 2);
        out(0x3C5, 0x0F);
    }
    if (r == 2) far_wr(e.seg, (u16)(e.off + PE_PACKED), far_make(0, 0));   /* the bitmap's own tail */
    cursor_ctl(-2);                                              /* show */
    return dst;
}

/* 1a21:6ac6 pic_info */
void pic_info(s16 id, s16 *w, s16 *h, s16 *masked)
{
    FarPtr e = pic_entry(pic_index(id));
    *w = (s16)rd16(e.seg, (u16)(e.off + PE_W));
    *h = (s16)rd16(e.seg, (u16)(e.off + PE_H));
    *masked = (s16)rd8(e.seg, (u16)(e.off + PE_MASKED));
}

/* 1a21:6b04 pic_get: an arena bitmap (1a21:9a68; pool -1 = the second arena) with the picture unpacked. */
FarPtr pic_get(s16 id, s16 pool)
{
    s16 w, h, m;
    pic_info(id, &w, &h, &m);
    FarPtr b = arena_bitmap_alloc(w, h, m, pool);
    pic_decode(id, b);
    return b;
}

/* 1a21:b0f6 pic_get_padded: a masked bitmap w + 8 wide with the picture at (0,0), 8 transparent columns. */
FarPtr pic_get_padded(s16 id, s16 pool)
{
    FarPtr p = pic_get(id, 2);
    FarPtr d = arena_bitmap_alloc((s16)(desc_w(p) + 8), (s16)desc_h(p), 1, pool);
    bitmap_clear(d);
    bitmap_blit_at(d, p, 0, 0, 3);
    arena_pop(1);
    return d;
}

/* 1a21:6b50 pic_park_list: parks a 0-terminated id list in EGA memory; returns 1. */
s16 pic_park_list(u16 ids)
{
    if (DSW(DS_no_ega_park) != 0) return 1;
    for (u16 di = ids; DSW(di) != 0; di = (u16)(di + 2))
        pic_load((s16)DSW(di), 1, far_make(0, 0));
    return 1;
}

/* 1a21:6b80 pic_load_list: loads into RAM; counts successes, stops at the first failure unless
 * keep_going. */
s16 pic_load_list(u16 ids, s16 keep_going)
{
    s16 si = 0;
    for (;;) {
        u16 bx = ids;
        ids = (u16)(ids + 2);
        s16 id = (s16)DSW(bx);
        if (id == 0) return si;
        if (pic_load(id, 0, far_make(0, 0)) != 0) si++;
        else if (keep_going == 0) return si;
    }
}

/* 1a21:6bc0 pic_free_list */
void pic_free_list(u16 ids)
{
    for (u16 di = ids; DSW(di) != 0; di = (u16)(di + 2))
        pic_free((s16)DSW(di));
}

/* _bios_equiplist (INT 11h). PORT: one floppy drive (bit 0, bits 6-7 = 0); only reached on a floppy
 * drive, which the port never reports. */
static u16 bios_equiplist(void)
{
    return 0x0001;
}

/* 1a21:6be0 lib_open_all: the current drive, one or two floppy drives, LIB1, LIB2 and (hard disk only)
 * LIB3 ("?:lib1" / "?:lib2" / "?:lib3", the "c" variants for modes other than 2 and 5), their picture
 * counts; the four song file names get the LIB2 drive letter. */
void lib_open_all(s16 disks, s16 mode)
{
    dos_getdrive(&DSW(DS_cur_drive));
    if (DSS(DS_cur_drive) < 3 && disks > 1) {
        u16 eq = bios_equiplist();
        disks = ((eq & 1) && (eq & 0xC0)) ? 2 : 1;
    } else {
        disks = 1;
    }
    DSW(DS_floppy_drives) = (u16)disks;
    bool cga = !(mode == 2 || mode == 5);
    u16 n1 = DSW((u16)(DS_lib_names + (cga ? 2 : 0)));
    DSW(DS_lib1_path) = n1;
    DSB(n1) = (u8)(DSB(DS_cur_drive) + 0x40);
    u16 err = dos_open(ds_str(n1), 0, &DSW(DS_lib1_handle));
    if (err != 0 || DSW(DS_lib1_handle) == 0) {
        if (DSS(DS_cur_drive) > 2) copy_all_files();
        file_not_found(ds_str((u16)(DSW(DS_lib1_path) + 2)));
    }
    u16 got = 0;
    if (dos_read(DSW(DS_lib1_handle), ds_ptr(DS_lib1_count), 2, &got) != 0 || got != 2) bad_file_format();
    u16 n2 = DSW((u16)(DS_lib_names + 4 + (cga ? 2 : 0)));
    DSW(DS_lib2_path) = n2;
    DSB(n2) = DSB(DSW(DS_lib1_path));
    if (dos_open(ds_str(n2), 0, &DSW(DS_lib2_handle)) != 0 || DSW(DS_lib2_handle) == 0) {
        if (DSS(DS_cur_drive) > 2) {
            copy_all_files();
        } else if (disks == 1) {
            DSW(DS_lib2_handle) = 0;                             /* single drive: asked for later */
        } else {
            DSB(DSW(DS_lib2_path)) = (DSW(DS_cur_drive) == 1) ? 'B' : 'A';   /* the other floppy drive */
            err = dos_open(ds_str(DSW(DS_lib2_path)), 0, &DSW(DS_lib2_handle));
            s16 si = 0;
            while (err != 0 || DSW(DS_lib2_handle) == 0) {
                if (si == 3) platform_exit();
                u16 m = msg(DS_msg_insert_disk);
                DSB(m + 0x0C) = (mode == 2 || mode == 5) ? '2' : '3';
                DSB(m + 0x19) = DSB(DSW(DS_lib2_path));
                message_box(m);
                err = dos_open(ds_str(DSW(DS_lib2_path)), 0, &DSW(DS_lib2_handle));
                si++;
            }
            DSW(DS_crit_err_off) = 0;                            /* the only write to that flag */
        }
    }
    if (DSW(DS_lib2_handle) != 0) {
        if (dos_read(DSW(DS_lib2_handle), ds_ptr(DS_lib2_count), 2, &got) != 0 || got != 2 ||
            DSW(DS_lib2_count) != 0xD4)
            bad_file_format();
    }
    if (DSS(DS_cur_drive) > 2) {                                 /* LIB3: hard disk only */
        u16 n3 = DSW((u16)(DS_lib_names + 8 + (cga ? 2 : 0)));
        DSW(DS_lib3_path) = n3;
        DSB(n3) = DSB(DSW(DS_lib1_path));
        if (dos_open(ds_str(n3), 0, &DSW(DS_lib3_handle)) != 0 || DSW(DS_lib3_handle) == 0) copy_all_files();
        if (dos_read(DSW(DS_lib3_handle), ds_ptr(DS_lib3_count), 2, &got) != 0 || got != 2 ||
            DSW(DS_lib3_count) != 0x53)
            bad_file_format();
    }
    /* the four song file names get the LIB2 drive letter */
    DSB(DS_song_name0) = DSB(DSW(DS_lib2_path));
    DSB(DS_song_name1) = DSB(DSW(DS_lib2_path));
    DSB(DS_song_name2) = DSB(DSW(DS_lib2_path));
    DSB(DS_song_name3) = DSB(DSW(DS_lib2_path));
}
