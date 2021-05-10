#ifndef DISP_H
#define DISP_H
#include <stdbool.h>

#define WIDTH 80
#define HEIGHT 25

#define ATTR_INV 0x1
#define ATTR_BOLD 0x2

#define ATTR_FG_MASK 0x1C
#define ATTR_BG_MASK 0xE0

#define ATTR_FG(a) (((a)>>2)&0x7)
#define ATTR_BG(a) (((a)>>5)&0x7)
#define ATTR_DEFAULT 0x1C

extern unsigned char char_screen[WIDTH * HEIGHT];
extern unsigned char attr_screen[WIDTH * HEIGHT];
extern int cur_x;
extern int cur_y;
extern bool curs_on;

void attr_setfg(unsigned char *attr, int fg);
void attr_setbg(unsigned char *attr, int bg);

int display_init(void);
void display_redraw(void);
void display_update(void);
void display_quit(void);
void render_screen(void);
void scroll_down(int count);

#endif
