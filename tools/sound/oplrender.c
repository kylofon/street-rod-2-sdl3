/* oplrender IN.opl OUT.wav : renders an OPL write log with Nuked-OPL3.
   IN: repeated records {u32 sample_pos @49716 Hz, u16 reg (0x100 = second bank), u8 val} (7 bytes, LE).
   Output: 16-bit stereo WAV at 49716 Hz; tail = 1 s of release after the last write. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opl3.h"
#define RATE 49716
static void w32(FILE *f, uint32_t v) { fwrite(&v, 4, 1, f); }
static void w16(FILE *f, uint16_t v) { fwrite(&v, 2, 1, f); }
int main(int argc, char **argv) {
    if (argc < 3) return 2;
    FILE *in = fopen(argv[1], "rb");
    if (!in) return 1;
    fseek(in, 0, SEEK_END); long n = ftell(in) / 7; fseek(in, 0, SEEK_SET);
    unsigned char *r = malloc(n * 7 + 1);
    fread(r, 7, n, in); fclose(in);
    opl3_chip chip; OPL3_Reset(&chip, RATE);
    uint32_t last = n ? *(uint32_t *)(r + (n - 1) * 7) : 0, total = last + RATE;
    int16_t *pcm = malloc((size_t)total * 4);
    long i = 0;
    for (uint32_t t = 0; t < total; t++) {
        while (i < n && *(uint32_t *)(r + i * 7) <= t) {
            OPL3_WriteReg(&chip, *(uint16_t *)(r + i * 7 + 4), r[i * 7 + 6]); i++;
        }
        OPL3_Generate(&chip, pcm + t * 2);
    }
    FILE *o = fopen(argv[2], "wb");
    fwrite("RIFF", 1, 4, o); w32(o, 36 + total * 4); fwrite("WAVEfmt ", 1, 8, o);
    w32(o, 16); w16(o, 1); w16(o, 2); w32(o, RATE); w32(o, RATE * 4); w16(o, 4); w16(o, 16);
    fwrite("data", 1, 4, o); w32(o, total * 4); fwrite(pcm, 4, total, o); fclose(o);
    return 0;
}
