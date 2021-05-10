#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "ansi.h"
#include "disp.h"

#define ESC_BUFLEN 1024
#define MAX_PARAMS 16
char esc_buf[ESC_BUFLEN];
int esc_i = 0;
bool inesc = false;
int params[MAX_PARAMS];
bool delay_scroll = false;
unsigned char curattr = ATTR_DEFAULT;

// Previously displayed character
char prevdisp = ' ';

void cur_up(int count) {
    if (count < 0) count = 1;
    cur_y -= count;
    if (cur_y < 0)
        cur_y = 0;
    delay_scroll = false;
}
void cur_down(int count) {
    if (count < 0) count = 1;
    cur_y += count;
    if (cur_y >= HEIGHT)
        cur_y = HEIGHT-1;
    delay_scroll = false;
}
void cur_left(int count) {
    if (count < 0) count = 1;
    cur_x -= count;
    if (cur_x < 0)
        cur_x = 0;
    delay_scroll = false;
}
void cur_right(int count) {
    if (count < 0) count = 1;
    cur_x += count;
    if (cur_x >= WIDTH)
        cur_x = WIDTH-1;
    delay_scroll = false;
}
void cur_xy(int x, int y) {
    if (x <= 0) x = 1;
    if (y <= 0) y = 1;
    cur_x = x - 1;
    cur_y = y - 1;
    //printf("Moved to x, y %d, %d\n", cur_x, cur_y);
    
    if (cur_x >= WIDTH)
        cur_x = WIDTH-1;
    if (cur_y >= HEIGHT)
        cur_y = HEIGHT-1;
    delay_scroll = false;
}

void erase(int d) 
{
    int cpos = cur_x + (cur_y * WIDTH);
    if (d <= 0) {
        // curor to end
        memset(char_screen + cpos, ' ', WIDTH*HEIGHT - cpos);
        memset(attr_screen + cpos, curattr, WIDTH*HEIGHT - cpos);
    } else if (d == 1) {
        memset(char_screen, ' ', cpos);
        memset(attr_screen, curattr, cpos);
    } else if (d == 2 || d == 3) {
        memset(char_screen, ' ', WIDTH*HEIGHT);
        memset(attr_screen, curattr, WIDTH*HEIGHT);
    }
}

void erase_line(int d)
{
    int cpos = cur_x + (cur_y * WIDTH);
    if (d <= 0) {
        memset(char_screen + cpos, ' ', WIDTH - cur_x);
        memset(attr_screen + cpos, curattr, WIDTH - cur_x);
    } else if (d == 1) {
        memset(char_screen + (cur_y * WIDTH), ' ', cur_x+1);
        memset(attr_screen + (cur_y * WIDTH), curattr, cur_x+1);
    } else if (d == 2) {
        memset(char_screen + (cur_y * WIDTH), ' ', WIDTH);
        memset(attr_screen + (cur_y * WIDTH), curattr, WIDTH);
    }
}
void scroll_up(int count)
{
    if (count < 0) count = 1;
    for (; count > 0; count--) {
        memmove(&char_screen[0], &char_screen[WIDTH], (HEIGHT-1)*WIDTH);
        memset(&char_screen[(HEIGHT-1)*WIDTH], ' ', WIDTH);
        
        memmove(&attr_screen[0], &attr_screen[WIDTH], (HEIGHT-1)*WIDTH);
        memset(&attr_screen[(HEIGHT-1)*WIDTH], curattr, WIDTH);
    }
}
void scroll_down(int count)
{
    if (count < 0) count = 1;
    for (; count > 0; count--) {
        memmove(&char_screen[WIDTH], &char_screen[0], (HEIGHT-1)*WIDTH);
        memset(char_screen, ' ', WIDTH);
        
        memmove(&attr_screen[WIDTH], &attr_screen[0], (HEIGHT-1)*WIDTH);
        memset(attr_screen, curattr, WIDTH);
    }
}

void insert_lines(int count)
{
    if (count < 0) count = 1;
    for (; count > 0; count--) {
        if (cur_y >= HEIGHT-1) {
            scroll_up(1);
        } else {
            memmove(&char_screen[(cur_y+1) * WIDTH], &char_screen[(cur_y) * WIDTH], (HEIGHT-cur_y-1)*WIDTH);
            memset(&char_screen[cur_y * WIDTH], ' ', WIDTH);
            
            memmove(&attr_screen[(cur_y+1) * WIDTH], &attr_screen[(cur_y) * WIDTH], (HEIGHT-cur_y-1)*WIDTH);
            memset(&attr_screen[cur_y * WIDTH], curattr, WIDTH);
        }
    }
}

void delete_lines(int count)
{
    if (count < 0) count = 1;
    for (; count > 0; count--) {
        if (cur_y >= HEIGHT-1) {
            erase_line(2);        
        } else {
            memmove(&char_screen[(cur_y) * WIDTH], &char_screen[(cur_y+1) * WIDTH], (HEIGHT-cur_y-1)*WIDTH);
            memset(&char_screen[(HEIGHT-1) * WIDTH], ' ', WIDTH);
            
            memmove(&attr_screen[(cur_y) * WIDTH], &attr_screen[(cur_y+1) * WIDTH], (HEIGHT-cur_y-1)*WIDTH);
            memset(&attr_screen[(HEIGHT-1) * WIDTH], curattr, WIDTH);
        }
    }
}

void insert_chars(int count)
{
    if (count < 0) count = 1;
    for (; count > 0; count--) {
        if (cur_x >= WIDTH-1) {
            char_screen[cur_x + (cur_y * WIDTH)] = ' ';
            attr_screen[cur_x + (cur_y * WIDTH)] = curattr;
        }
        memmove(&char_screen[cur_x + 1 + (cur_y * WIDTH)], &char_screen[cur_x + (cur_y * WIDTH)], WIDTH-cur_x-1);
        char_screen[cur_x + (cur_y * WIDTH)] = ' ';
        memmove(&attr_screen[cur_x + 1 + (cur_y * WIDTH)], &attr_screen[cur_x + (cur_y * WIDTH)], WIDTH-cur_x-1);
        attr_screen[cur_x + (cur_y * WIDTH)] = curattr;
    }
}

void delete_chars(int count)
{
    if (count < 0) count = 1;
    for (; count > 0; count--) {
        if (cur_x >= WIDTH-1) {
            char_screen[cur_x + (cur_y * WIDTH)] = ' ';
            attr_screen[cur_x + (cur_y * WIDTH)] = curattr;
        }
        memmove(&char_screen[cur_x + (cur_y * WIDTH)], &char_screen[cur_x + 1 + (cur_y * WIDTH)], WIDTH-cur_x-1);
        char_screen[WIDTH-1 + (cur_y * WIDTH)] = ' ';
        memmove(&attr_screen[cur_x  + (cur_y * WIDTH)], &attr_screen[cur_x + 1 + (cur_y * WIDTH)], WIDTH-cur_x-1);
        attr_screen[WIDTH-1 + (cur_y * WIDTH)] = curattr;
    }
}

void rept_prev(int count)
{
    if (count <= 0) count = 1;
    for (;count > 0; count--) {
        if (delay_scroll) {
            //printf("Doing delayed\n");
                cur_x = 0;
                cur_y++;
                if (cur_y >= HEIGHT) {
                    scroll_up(1);
                    cur_y--;
                }
            delay_scroll = false;
        }
        //printf("%c", buf[i]);
        char_screen[cur_x + (WIDTH * cur_y)] = prevdisp;
        
        attr_screen[cur_x + (WIDTH * cur_y)] = curattr;
        if (cur_x >= WIDTH-1) {
            delay_scroll = true;
            //printf("Delaying\n");
        }
        else {                    
            cur_x++;
        }
    }
}
bool in_os = false;
void doescape(void) 
{
    for (int i = 0; i < MAX_PARAMS; i++)
        params[i] = -1;
    int params_i = 0;
    int curnum = 0;
    //printf("In escape\n");
    esc_i = 0;
    char *b = esc_buf;
    switch (*b++) {
    case '[': // CSI sequence
        //printf("In CSI\n");
        curnum = -1;
        params_i = 0;
        while (*b != 0 && !(*b >= 0x40 && *b <= 0x7E)) {
            if (*b >= 0x30 && *b <= 0x39) { // Digit
                if (curnum < 0) 
                    curnum = 0;
                curnum *= 10;
                curnum += *b - 0x30;
                //printf("Digit '%c'\n", *b);
            } else if (*b == ';') {
                if (params_i < MAX_PARAMS) 
                    params[params_i++] = curnum;
                //printf("Pushed';' %d\n", curnum);
                curnum = -1;
            } else if (*b >= 0x20 && *b <= 0x2F) {
                // Intermediate bytes, what do they do here?
            } 
            b++;
        } 
        if (params_i < MAX_PARAMS) {
            params[params_i++] = curnum;
            //printf("Pushed'end' %d\n", curnum);
        }
        if (*b >= 0x40 && *b <= 0x7E) {
            //printf("final '%c'\n", *b);
            // Final byte, parse sequence
            switch (*b) {
            case '@': insert_chars(params[0]); break;
            case 'A': cur_up(params[0]); break;
            case 'B': cur_down(params[0]); break;
            case 'C': cur_right(params[0]); break;
            case 'D': cur_left(params[0]); break;
            case 'E': cur_x = 0; cur_down(params[0]); break;
            case 'F': cur_x = 0; cur_up(params[0]); break;
            case 'G': cur_xy(params[0], cur_y + 1); break;
            case 'H': cur_xy(params[1], params[0]); break;
            case 'J': erase(params[0]); break;
            case 'K': erase_line(params[0]); break;
            case 'L': insert_lines(params[0]); break;
            case 'M': delete_lines(params[0]); break;
            case 'P': delete_chars(params[0]); break;
            case 'S': scroll_up(params[0]); break;
            case 'T': scroll_down(params[0]); break;
            case 'f': cur_xy(params[1], params[0]); break;
            case 'd': cur_xy(cur_x + 1, params[0]); break;
            case 'b': rept_prev(params[0]); break;
            case 'm': /* SGR */ 
                for (int i = 0; i < params_i; i++) {
                    switch(params[i]) {
                    case -1:
                    case 0: // Reset
                        curattr = ATTR_DEFAULT; 
                        break;
                    case 1: // Bold
                        curattr |= ATTR_BOLD;
                        break;
                    case 7: // Reverse Video
                        curattr |= ATTR_INV;
                        printf("INV\n");
                        break;
                    case 10: // Default font
                        curattr &= 0xFC;
                        break;
                    case 39: // Default FG
                        attr_setfg(&curattr, ATTR_FG(ATTR_DEFAULT));
                        break;
                    case 49: // Default BG
                        attr_setbg(&curattr, ATTR_BG(ATTR_DEFAULT));
                        break;
                    default:
                        if (params[i] >= 30 && params[i] <= 37) {
                            attr_setfg(&curattr, params[i] - 30);
                        } else if (params[i] >= 40 && params[i] <= 47) {
                            attr_setbg(&curattr, params[i] - 40);
                        } else {
                            printf("Unhandled SGR: %d\n", params[i]);
                        }
                    }
                }
                break;
            case 'n': /* DSR */ break;
            default:
                printf("Unknown CSI sequence: '%s'\n", esc_buf);
            }
        }
        break;
    case 'N': // Single Shift Two
        break;
    case 'O': // Single Shift Three
        break;
    case 'P': // Device Control String
        break;
    case '\\': //String Terminator
        in_os = false;
        break;
    case ']': // OS Command
        // Capture until String Terminator or bell(xterm)
        in_os = true;
        break;
    case 'X': // Start of String
        break;
    case '^': // Privacy Message
        break;
    case '_': // Application Program Command
        break;
    case 'c': // Reset to initial state
        break;
    }
    
    inesc = false;
}


int handle(char *buf, ssize_t len)
{
    int dirty = 0;
    
    for (ssize_t i = 0; i < len; i++) {
        if (inesc) {
            esc_buf[esc_i++] = buf[i];
            if (esc_i >= ESC_BUFLEN-1 || (buf[i] >= 0x40 && buf[i] <= 0x7E && buf[i] != '[')) {
                esc_buf[esc_i] = 0;
                //printf("'%s' esc_i %d buf[i] %c", esc_buf, esc_i, buf[i]);
                // Terminate and do escape, cutoff any further chars
                doescape();
                dirty = 1;
            }
        } else if (in_os) {
            if (buf[i] == 0x07)
                in_os = false;
        } else {
            switch (buf[i]) {
            case 033:
                // Start of escape sequence
                inesc = true;
                break;
            case '\r':
                cur_x = 0;
                delay_scroll = false;
                //printf("CR\n");
                dirty++;
                break;
            case 0x07:
                 // Beep
                 break;
            case '\n':
                cur_y++;
                if (cur_y >= HEIGHT) {
                    scroll_up(1);
                    cur_y--;
                }
                dirty++;
                //printf("NL\n");
                break;
            case '\b':
                delay_scroll = false;
                cur_x--;
                if (cur_x < 0) {
                    if (cur_y > 0) {
                        cur_y--; cur_x = WIDTH-1;
                    } else {
                        cur_x = 0;
                    }
                }
                dirty++;
                break;
            case '\t':
                {
                    int prev_x = cur_x;
                    cur_x += 8;
                    cur_x = cur_x - (cur_x % 8);
                    int len = (cur_x > WIDTH) ? WIDTH-prev_x : cur_x-prev_x;
                    memset(&char_screen[prev_x + (WIDTH * cur_y)], ' ', len);
                    memset(&attr_screen[prev_x + (WIDTH * cur_y)], curattr, len);
                    if (cur_x >= WIDTH) {
                        cur_x = 0;
                        cur_y++;
                        if (cur_y >= HEIGHT) {
                            scroll_up(1);
                            cur_y--;
                        }
                    }
                }
                dirty++;
                break;
            default:
                if (delay_scroll) {
                    //printf("Doing delayed\n");
                        cur_x = 0;
                        cur_y++;
                        if (cur_y >= HEIGHT) {
                            scroll_up(1);
                            cur_y--;
                        }
                    delay_scroll = false;
                }
                //printf("%c", buf[i]);
                prevdisp = char_screen[cur_x + (WIDTH * cur_y)] = buf[i];
                
                attr_screen[cur_x + (WIDTH * cur_y)] = curattr;
                if (cur_x >= WIDTH-1) {
                    delay_scroll = true;
                    //printf("Delaying\n");
                }
                else {                    
                    cur_x++;
                }
                    
                dirty++;
            }
        }
    }
    return dirty;
}
