/* DOS and C runtime replacement: the DOS state at start-up, DOS memory (INT 21h 48h/49h/4Ah on an MCB
 * chain in mem[]), the far heap (_fmalloc 34b0:135d, _ffree 34b0:134a) and the DOS file calls (_dos_open &
 * co., intdos lseek 1a21:6256) - platform.md overview, "Arena", "lib_seek".
 *
 * DOS memory. The chain starts at the program's MCB (PORT: DOS itself, the environment block and the
 * memory below the PSP are not modelled): PSP at LOAD_SEG - 10h, its MCB right below; _astart shrinks
 * the program block to DGROUP + 1000h = HEAP_BOTTOM (INT 21h 4Ah); the rest up to HEAP_TOP (A000h) is
 * one free block. Each MCB: byte 0 'M' / 'Z', word 1 owner (0 = free), word 3 size in paragraphs.
 * 48h is first fit (DOS's default strategy), joining free neighbours while it walks; 4Ah grows a block
 * into the free blocks behind it or fails with error 8.
 *
 * Far heap. PORT: SR2's runtime is MSC 6.0 (heap code 34b0:1300-15be), different from SR1's MSC 5.1 that
 * the SR1 port transcribed register by register. Only the contract is kept: a far block outside DGROUP,
 * NULL when DOS memory is exhausted. Each _fmalloc is its own DOS block (offset 0004h in the segment,
 * where the MSC heap puts the first block), _ffree gives it back to the MCB chain. Which bytes the
 * allocator places where is not observable by the game.
 *
 * Files. DOS handles are host FILE pointers (PORT: an OS resource, not game state) in a 20-entry job
 * file table; 0..4 are the standard devices (not backed by files), new handles are the lowest free
 * entry as DOS hands them out (5, 6, ...). Names: the part after the last ':', '' or '/' is looked up
 * case-insensitively in the game directory (host_game_path). Errors set _doserrno DS:8295 and errno
 * DS:828A (PORT: errno = the DOS code; the runtime's _dosmaperr table is not used by the game). */
#include "platform/platform.h"
#include "platform/vga.h"
#include "host.h"

#include <stdio.h>
#include <string.h>

/* ======================================================================== C runtime variables */

#define PSP_SEG        ((u16)(LOAD_SEG - 0x10))
#define RT_ERRNO       0x828A  /* errno */
#define RT_DOSERRNO    0x8295  /* _doserrno (byte written by _dosmaperr, read as a word by lib_seek) */
#define BSS_START      0x8806  /* _astart zero-fills DS:8806 .. DS:BC4F */
#define BSS_END        0xBC50

/* 34b0:05c8 _dosmaperr (PORT: errno := the DOS error code). */
static void crt_dosmaperr(u16 ax)
{
    DSB(RT_DOSERRNO) = (u8)ax;
    DSW(RT_ERRNO) = (u16)(u8)ax;
}

/* ============================================================================== DOS memory */

#define MCB_FIRST ((u16)(PSP_SEG - 1))

static u8  mcb_type(u16 m)  { return rd8(m, 0); }
static u16 mcb_owner(u16 m) { return rd16(m, 1); }
static u16 mcb_size(u16 m)  { return rd16(m, 3); }
static bool mcb_valid(u16 m) { u8 t = mcb_type(m); return t == 'M' || t == 'Z'; }
static void mcb_set(u16 m, u8 type, u16 owner, u16 size)
{
    wr8(m, 0, type);
    wr16(m, 1, owner);
    wr16(m, 3, size);
}

/* Joins the free blocks that follow the block of MCB m into it (DOS does this while walking). */
static void mcb_join_free(u16 m)
{
    while (mcb_type(m) == 'M') {
        u16 n = (u16)(m + 1 + mcb_size(m));
        if (n >= HEAP_TOP || !mcb_valid(n) || mcb_owner(n) != 0) break;
        mcb_set(m, mcb_type(n), mcb_owner(m), (u16)(mcb_size(m) + 1 + mcb_size(n)));
    }
}

/* Cuts the block of MCB m to paras paragraphs; the rest becomes a free block. */
static void mcb_split(u16 m, u16 paras)
{
    u16 size = mcb_size(m);
    if (size <= paras) return;
    u16 n = (u16)(m + 1 + paras);
    mcb_set(n, mcb_type(m), 0, (u16)(size - paras - 1));
    mcb_set(m, 'M', mcb_owner(m), paras);
    mcb_join_free(n);
}

/* INT 21h AH=48h: segment of the new block, or 0 with *err = 8 (BX = largest block not modelled). */
static u16 dos_mem_alloc(u16 paras, u16 *err)
{
    u16 m = MCB_FIRST;
    for (;;) {
        if (!mcb_valid(m)) { *err = 7; return 0; }                  /* arena trashed */
        if (mcb_owner(m) == 0) {
            mcb_join_free(m);
            if (mcb_size(m) >= paras) {                              /* first fit */
                mcb_split(m, paras);
                wr16(m, 1, PSP_SEG);
                *err = 0;
                return (u16)(m + 1);
            }
        }
        if (mcb_type(m) == 'Z') break;
        m = (u16)(m + 1 + mcb_size(m));
        if (m >= HEAP_TOP) break;
    }
    *err = 8;
    return 0;
}

/* INT 21h AH=4Ah on the block at seg: 0 or the error (7 bad arena / 9 bad block, 8 not enough memory;
 * the block is left unchanged on failure). */
static u16 dos_mem_resize(u16 seg, u16 paras)
{
    u16 m = (u16)(seg - 1);
    if (seg < PSP_SEG || seg >= HEAP_TOP || !mcb_valid(m) || mcb_owner(m) == 0) return 9;
    u16 size = mcb_size(m);
    if (paras <= size) {
        mcb_split(m, paras);
        return 0;
    }
    /* grow: into the free blocks behind this one */
    u32 avail = size;
    u16 n = (u16)(m + 1 + size);
    u8 t = mcb_type(m);
    while (t == 'M' && n < HEAP_TOP && mcb_valid(n) && mcb_owner(n) == 0) {
        avail += 1u + mcb_size(n);
        t = mcb_type(n);
        n = (u16)(n + 1 + mcb_size(n));
    }
    if (avail < paras) return 8;
    mcb_join_free(m);
    mcb_split(m, paras);
    return 0;
}

/* (INT 21h AH=49h is never called: the MS C heap does not give memory back to DOS.) */

/* ================================================================== start-up (34b0:0018 _astart) */

/* PORT: dos_init - platform.h. The state _astart leaves: the memory chain with the program block ending at
 * DGROUP + 1000h, and the BSS zero-filled (DS:8806..BC4F; the file holds data up to DS:8810). */
void dos_init(void)
{
    /* The loader: program block = all conventional memory, PSP at LOAD_SEG - 10h. */
    memset(mp(PSP_SEG, 0), 0, 0x100);
    wr8(PSP_SEG, 0, 0xCD);
    wr8(PSP_SEG, 1, 0x20);                                       /* INT 20h */
    wr16(PSP_SEG, 2, HEAP_TOP);                                  /* top of memory */
    mcb_set(MCB_FIRST, 'Z', PSP_SEG, (u16)(HEAP_TOP - PSP_SEG));

    /* _astart: DGROUP gets min(top - DGROUP, 1000h) paragraphs; setblock shrinks the program block. */
    u16 si = (u16)(rd16(PSP_SEG, 2) - DGROUP);
    if (si >= 0x1000) si = 0x1000;
    si = (u16)(si + DGROUP);
    wr16(PSP_SEG, 2, si);
    dos_mem_resize(PSP_SEG, (u16)(si - PSP_SEG));
    memset(mp(DGROUP, BSS_START), 0, BSS_END - BSS_START);
}

/* ======================================================================== far heap (PORT, see top) */

/* 34b0:135d _fmalloc */
FarPtr crt_fmalloc(u16 size)
{
    u16 paras = (u16)(((u32)size + 4 + 0x0F) >> 4), err;
    if (size >= 0xFFF1) return far_make(0, 0);
    u16 seg = dos_mem_alloc(paras, &err);
    if (!seg) return far_make(0, 0);
    return far_make(seg, 4);
}

/* 34b0:134a _ffree */
void crt_ffree(FarPtr p)
{
    if (far_is_null(p)) return;
    u16 m = (u16)(p.seg - 1);
    if (p.seg < HEAP_BOTTOM || p.seg >= HEAP_TOP || !mcb_valid(m)) return;
    wr16(m, 1, 0);                                               /* owner 0: free (joined when walked) */
}

/* ================================================================================= DOS files */

#define DOS_MAX_HANDLES 20
#define DOS_STD_HANDLES 5

typedef enum { H_FREE, H_STD, H_FILE } HandleKind;
static struct { HandleKind kind; FILE *f; } dos_jft[DOS_MAX_HANDLES];
static bool dos_jft_ready;

static void dos_jft_init(void)
{
    if (dos_jft_ready) return;
    for (int i = 0; i < DOS_STD_HANDLES; i++) dos_jft[i].kind = H_STD;    /* stdin .. stdprn */
    dos_jft_ready = true;
}

static FILE *dos_file(u16 h)
{
    dos_jft_init();
    if (h >= DOS_MAX_HANDLES || dos_jft[h].kind != H_FILE) return NULL;
    return dos_jft[h].f;
}

/* PORT: the file name part of a DOS path ("C:lib1", ":HOTROD.SAV", "A:\X\NAME" -> "lib1" ...). */
static const char *dos_base_name(const char *s)
{
    const char *b = s;
    for (const char *p = s; *p; p++)
        if (*p == ':' || *p == '\\' || *p == '/') b = p + 1;
    return b;
}

/* INT 21h 3Dh / 3Ch: 0 and *handle, or the DOS error (2 not found, 4 too many files, 5 denied). */
static u16 dos_open_file(const char *name, const char *fmode, bool create, u16 *handle)
{
    dos_jft_init();
    u16 h;
    for (h = 0; h < DOS_MAX_HANDLES && dos_jft[h].kind != H_FREE; h++) {}
    if (h >= DOS_MAX_HANDLES) return 4;
    const char *base = dos_base_name(name);
    if (!*base) return 2;
    char *path = host_game_path(base, create);
    if (!path) return 2;
    FILE *f = fopen(path, fmode);
    host_free(path);
    if (!f) return create ? 5 : 2;
    dos_jft[h].kind = H_FILE;
    dos_jft[h].f = f;
    *handle = h;
    return 0;
}

/* The runtime's error exit of the _dos_* calls (1e16:055a): _dosmaperr, return the DOS error. */
static u16 dos_ret(u16 err)
{
    if (err) crt_dosmaperr(err);
    return err;
}

/* 1e16:2330 _dos_open — platform.h. Mode 0 read, 1 write, 2 read/write (sharing bits ignored). */
u16 dos_open(const char *name, u16 mode, u16 *handle)
{
    u8 access = (u8)(mode & 7);
    if (access > 2) return dos_ret(0x0C);                        /* invalid access code */
    return dos_ret(dos_open_file(name, access == 0 ? "rb" : "r+b", false, handle));
}

/* 1e16:2305 _dos_creat — platform.h (INT 21h 3Ch: create or truncate, opened read/write). */
u16 dos_creat(const char *name, u16 attr, u16 *handle)
{
    (void)attr;
    return dos_ret(dos_open_file(name, "w+b", true, handle));
}

/* DOS 3Fh / 40h on mem[] from seg:off on (video memory through the register model). */
static u16 dos_xfer(FILE *f, FarPtr buf, u16 n, bool write)
{
    if (is_vram(buf.seg)) {
        u16 k = 0;
        for (; k < n; k++) {
            u16 off = (u16)(buf.off + k);
            if (write) {
                if (fputc(vrd(buf.seg, off), f) == EOF) break;
            } else {
                int c = fgetc(f);
                if (c == EOF) break;
                vwr(buf.seg, off, (u8)c);
            }
        }
        if (write) fflush(f);
        return k;
    }
    u32 at = far_lin(buf);
    if (at >= MEM_SIZE) return 0;
    if (n > MEM_SIZE - at) n = (u16)(MEM_SIZE - at);
    size_t k = write ? fwrite(mem + at, 1, n, f) : fread(mem + at, 1, n, f);
    if (write) fflush(f);
    return (u16)k;
}

/* 1e16:2348 _dos_read — platform.h */
u16 dos_read(u16 handle, FarPtr buf, u16 n, u16 *got)
{
    FILE *f = dos_file(handle);
    if (!f) return dos_ret(6);
    *got = dos_xfer(f, buf, n, false);
    return 0;
}

/* 1e16:234f _dos_write — platform.h */
u16 dos_write(u16 handle, FarPtr buf, u16 n, u16 *put)
{
    FILE *f = dos_file(handle);
    if (!f) return dos_ret(6);
    u16 k = dos_xfer(f, buf, n, true);
    if (k < n && ferror(f)) return dos_ret(5);
    *put = k;
    return 0;
}

/* 1e16:22f0 _dos_close — platform.h. Closing a standard handle (0..4) frees its entry, as DOS does. */
u16 dos_close(u16 handle)
{
    dos_jft_init();
    if (handle >= DOS_MAX_HANDLES || dos_jft[handle].kind == H_FREE) return dos_ret(6);
    if (dos_jft[handle].kind == H_FILE) fclose(dos_jft[handle].f);
    dos_jft[handle].kind = H_FREE;
    dos_jft[handle].f = NULL;
    return 0;
}

/* 1e16:22de remove — platform.h: 0, or -1 (1e16:0552 / 0567) with _dosmaperr. */
u16 dos_remove(const char *name)
{
    const char *base = dos_base_name(name);
    char *path = *base ? host_game_path(base, false) : NULL;
    int rc = path ? remove(path) : -1;
    if (path) host_free(path);
    if (rc != 0) {
        crt_dosmaperr(path ? 5 : 2);
        return 0xFFFF;
    }
    return 0;
}

/* 1e16:2382 _dos_getdrive — platform.h. PORT: always drive 3 (C:), so every floppy path is dead. */
void dos_getdrive(u16 *drive)
{
    *drive = 3;
}

/* 0f38:6016 lib_seek — platform.md §2.5: intdos(AX=4200h, BX=handle, CX:DX=pos); 0 on success, else
 * _doserrno DS:6318 (set by 1e16:0572 from AL) which is also copied to DS:630D. */
s16 lib_seek(u16 handle, s32 pos)
{
    FILE *f = dos_file(handle);
    if (f && fseek(f, (long)(u32)pos, SEEK_SET) == 0) return 0;
    crt_dosmaperr(f ? 0x19 : 6);                  /* 6 invalid handle; PORT: 19h if the host seek fails */
    DSW(RT_ERRNO) = DSW(RT_DOSERRNO);
    return DSS(RT_DOSERRNO);
}
