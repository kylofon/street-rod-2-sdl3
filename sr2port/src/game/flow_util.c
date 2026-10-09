/* game_flow: MS C runtime string helpers on DGROUP memory (strcpy, strcmp, strchr, itoa, atoi ...) as the
 * flow code of SR.EXE uses them inline (repne scasb / movsw) or through the 34b0 runtime (atoi 34b0:1680,
 * itoa 34b0:16dc, strchr 34b0:21fc, memmove). All arguments are DS offsets; the strings stay in mem[]. */
#include "game/flow.h"

#include "platform/platform.h"

void fl_strcpy(u16 dst, u16 src)
{
    u16 n = 0;
    while (DSB((u16)(src + n)) != 0) n++;
    for (u16 i = 0; i <= n; i++) DSB((u16)(dst + i)) = DSB((u16)(src + i));   /* forward copy (movsw) */
}

void fl_strncpy(u16 dst, u16 src, u16 n)
{
    u16 i = 0;
    for (; i < n && DSB((u16)(src + i)) != 0; i++) DSB((u16)(dst + i)) = DSB((u16)(src + i));
    for (; i < n; i++) DSB((u16)(dst + i)) = 0;
}

void fl_strcat(u16 dst, u16 src)
{
    while (DSB(dst) != 0) dst++;
    fl_strcpy(dst, src);
}

s16 fl_strcmp(u16 a, u16 b)
{
    for (;; a++, b++) {
        u8 x = DSB(a), y = DSB(b);
        if (x != y) return x < y ? -1 : 1;
        if (x == 0) return 0;
    }
}

u16 fl_strlen(u16 s)
{
    u16 n = 0;
    while (DSB((u16)(s + n)) != 0) n++;
    return n;
}

/* strchr: the NUL terminator matches c == 0 */
u16 fl_strchr(u16 s, u8 c)
{
    for (;; s++) {
        if (DSB(s) == c) return s;
        if (DSB(s) == 0) return 0;
    }
}

/* 34b0:1680 atoi: white space, optional sign, decimal digits; 16-bit arithmetic */
s16 fl_atoi(u16 s)
{
    while (DSB(s) == ' ' || DSB(s) == '\t') s++;
    u8 sign = DSB(s);
    if (sign == '-' || sign == '+') s++;
    u16 v = 0;
    while (DSB(s) >= '0' && DSB(s) <= '9') v = (u16)(v * 10 + (DSB(s++) - '0'));
    return (s16)(sign == '-' ? (u16)-v : v);
}

/* 34b0:16dc itoa(v, dst, 10): '-' for negative values in radix 10 */
void fl_itoa(s16 v, u16 dst)
{
    char tmp[8];
    int n = 0;
    u16 u = (u16)v;
    if (v < 0) { DSB(dst++) = '-'; u = (u16)-v; }
    do { tmp[n++] = (char)('0' + u % 10); u /= 10; } while (u != 0);
    while (n > 0) DSB(dst++) = (u8)tmp[--n];
    DSB(dst) = 0;
}

void fl_memmove(u16 dst, u16 src, u16 n)
{
    if (dst < src) for (u16 i = 0; i < n; i++) DSB((u16)(dst + i)) = DSB((u16)(src + i));
    else           for (u16 i = n; i > 0; i--) DSB((u16)(dst + i - 1)) = DSB((u16)(src + i - 1));
}

/* The save-file name as 0001:58b2 / 59e2 / 5d82 build it: name[0] = the data drive letter, 8 bytes
 * of the template (":HOTROD\0") after it, itoa(n) over its NUL, then strcat ".SAV" (the 5 bytes
 * after the template). Returns buf. */
u16 fl_save_name(u16 buf, u16 tmpl, s16 n)
{
    DSB(buf) = (u8)data_disk_check();
    for (u16 i = 0; i < 8; i++) DSB((u16)(buf + 1 + i)) = DSB((u16)(tmpl + i));
    fl_itoa(n, (u16)(buf + 8));
    fl_strcat(buf, (u16)(tmpl + 8));
    return buf;
}
