#include "platform/textmode.h"

#include <string.h>

#include "host.h"
#include "mem.h"
#include "platform/ega.h"
#include "symbols.h"

#define COLS 80
#define ROWS 25
#define TEXT_W (COLS * 8)
#define TEXT_H (ROWS * 8)

static char cell[ROWS][COLS];
static int cur_col, cur_row;
static bool text_active, dirty;

/* Glyph of character c: 9 bytes (advance, 8 rows) at DS:86EA offset 6 + (c - 20h) * 9. */
static const u8 *glyph(u8 c)
{
    if (c < 0x20 || c > 0x7A) c = 0x20;
    return mp(DSW(DS_g_font_seg), (u16)(6 + (c - 0x20) * 9));
}

static bool compose(u32 *xrgb)
{
    if (!dirty) return false;
    dirty = false;
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) {
            const u8 *g = glyph((u8)cell[r][c]);
            for (int y = 0; y < 8; y++)
                for (int x = 0; x < 8; x++)
                    xrgb[(r * 8 + y) * TEXT_W + c * 8 + x] = (g[1 + y] >> (7 - x) & 1) ? 0xAAAAAA : 0x000000;
        }
    /* the cursor: an underline on the last scan line of the cell */
    if (cur_row < ROWS && cur_col < COLS)
        for (int x = 0; x < 8; x++) xrgb[(cur_row * 8 + 7) * TEXT_W + cur_col * 8 + x] = 0xAAAAAA;
    return true;
}

void text_mode_set(void)
{
    memset(cell, ' ', sizeof cell);
    cur_col = cur_row = 0;
    text_active = true;
    dirty = true;
    host_set_frame_source(compose, TEXT_W, TEXT_H);
}

void text_mode_leave(void)
{
    text_active = false;
    ega_show();                                    /* the graphics frame source comes back */
}

void set_text_cursor(int col, int row)
{
    cur_col = col < 0 ? 0 : col >= COLS ? COLS - 1 : col;
    cur_row = row < 0 ? 0 : row >= ROWS ? ROWS - 1 : row;
    dirty = true;
}

static void tty_char(char ch)
{
    if (ch == '\r') { cur_col = 0; return; }
    if (ch == '\n') {
        if (++cur_row >= ROWS) {
            memmove(cell[0], cell[1], sizeof cell - COLS);
            memset(cell[ROWS - 1], ' ', COLS);
            cur_row = ROWS - 1;
        }
        return;
    }
    cell[cur_row][cur_col] = ch;
    if (++cur_col >= COLS) { cur_col = 0; tty_char('\n'); }
}

void dos_print_line(const char *s, int indent)
{
    for (int i = 0; i < indent; i++) tty_char(' ');
    for (; *s; s++) tty_char(*s);
    tty_char('\n');
    tty_char('\r');
    dirty = true;
}

/* ---- keyboard: the XT scan code stream of the host (the game's INT 9 handler is not installed yet) */

static int key_queue[16];
static int key_head, key_count;

static void text_kbd(u8 sc)
{
    static const char row0[] = "1234567890";
    static const char rowq[] = "qwertyuiop";
    static const char rowa[] = "asdfghjkl";
    static const char rowz[] = "zxcvbnm";
    if (sc & 0x80 || sc >= 0xE0) return;             /* breaks and prefixes */
    int ch = 0xFF;                                   /* "some other key" */
    if (sc == 0x01) ch = 0x1B;
    else if (sc == 0x1C) ch = '\r';
    else if (sc == 0x39) ch = ' ';
    else if (sc >= 0x02 && sc <= 0x0B) ch = row0[sc - 0x02];
    else if (sc >= 0x10 && sc <= 0x19) ch = rowq[sc - 0x10];
    else if (sc >= 0x1E && sc <= 0x26) ch = rowa[sc - 0x1E];
    else if (sc >= 0x2C && sc <= 0x32) ch = rowz[sc - 0x2C];
    if (key_count < 16) key_queue[(key_head + key_count++) & 15] = ch;
}

bool text_kbhit(void)
{
    host_set_kbd_handler(text_kbd);
    host_pump();
    return key_count > 0;
}

int text_getch(void)
{
    host_set_kbd_handler(text_kbd);
    while (key_count == 0) host_pump();
    int ch = key_queue[key_head];
    key_head = (key_head + 1) & 15;
    key_count--;
    return ch;
}
