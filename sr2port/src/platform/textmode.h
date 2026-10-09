#pragma once
/* PORT: the 80x25 colour text mode (BIOS mode 3) of the start-up menus (video options, sound options) and
 * the DOS console calls they use: INT 10h AH=0 / AH=2, INT 21h AH=09h, kbhit / getch. Glyphs come from the
 * game's own 8x8 font (DS:86EA, the real mode 3 font is not in the program). Not game state: the text cells
 * live here, not in mem[]. */
#include "types.h"

void text_mode_set(void);                      /* INT 10h AX=0003h: clears the screen, cursor to (0,0), shows text */
void text_mode_leave(void);                    /* back to the graphics frame (vga_mode_0d takes over) */
void set_text_cursor(int col, int row);        /* 0001:0090 (INT 10h AH=2, page 0) */
void dos_print_line(const char *s, int indent);/* 0000:0000: indent spaces + s + "\n\r$" (INT 21h AH=09h) */
bool text_kbhit(void);                         /* 34b0:1762 */
int  text_getch(void);                         /* 34b0:1788: ASCII of the next key, blocking (pumps the host) */
