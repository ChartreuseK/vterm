#include <SDL2/SDL.h>
#include <stdint.h>
#include "font.h"
#include "disp.h"

#define FONT_WIDTH 8
#define FONT_HEIGHT 8
#define FONT_VSCALE 2         // Vertical scaling of font (scanlines)
#define DISP_WIDTH (WIDTH*FONT_WIDTH)
#define DISP_HEIGHT (HEIGHT*FONT_HEIGHT*FONT_VSCALE)
#define FONT_CHARS 256
#define BLINK_DELAY 1000


#define COLOR_WHITE 0xFFFFFFFF
#define COLOR_GREY  0xFF7F7F7F
#define COLOR_BLACK 0xFF000000



bool curs_on = false;

int cur_x, cur_y;


SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture *texture = NULL;
SDL_TimerID blink_timer_id;

// raw_screen contains RGBA values for each pixel
uint32_t raw_screen[DISP_WIDTH * DISP_HEIGHT];
// Characters that make up screen
unsigned char char_screen[WIDTH * HEIGHT];
unsigned char attr_screen[WIDTH * HEIGHT];

uint32_t colors[16] = {
    0xFF000000, // Black
    0xFFAA0000, // Red
    0xFF00AA00, // Green
    0xFFAA5500, // Yellow/Brown
    0xFF0000AA, // Blue
    0xFFAA00AA, // Magenta
    0xFF00AAAA, // Cyan
    0xFFAAAAAA, // White
    0xFF555555, // Brights 
    0xFFFF5555,
    0xFF55FF55,
    0xFFFFFF55,
    0xFF5555FF,
    0xFFFF55FF,
    0xFF55FFFF,
    
    0xFFFFFFFF
};

Uint32 blink_timer(Uint32 interval, void *param);

void attr_setfg(unsigned char *attr, int fg)
{
    *attr &= ~ATTR_FG_MASK;
    *attr |= (fg & 7) << 2;
}
void attr_setbg(unsigned char *attr, int bg)
{
    *attr &= ~ATTR_BG_MASK;
    *attr |= (bg & 7) << 5;
}

int display_init(void)
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
    window = SDL_CreateWindow("VTerm",
                SDL_WINDOWPOS_UNDEFINED,
                SDL_WINDOWPOS_UNDEFINED,
                DISP_WIDTH, DISP_HEIGHT,
                0); // SDL_WINDOW_RESIZABLE
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    //SDL_RenderSetIntegerScale(renderer, true);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, 0); // Nearest Neighbour.  "linear" for linear
    SDL_RenderSetLogicalSize(renderer,DISP_WIDTH,DISP_HEIGHT);

    texture = SDL_CreateTexture(renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            DISP_WIDTH,DISP_HEIGHT);
            
    memset(char_screen, ' ', WIDTH*HEIGHT);
    memset(attr_screen, ATTR_DEFAULT, WIDTH*HEIGHT);
    render_screen();
    
    display_update();

    blink_timer_id = SDL_AddTimer(BLINK_DELAY, blink_timer, NULL);
    
    SDL_StartTextInput();
    
    return 0;
}

void display_redraw(void)
{

    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

void display_update(void)
{
    render_screen();
    SDL_UpdateTexture(texture, NULL, &raw_screen, DISP_WIDTH*sizeof(uint32_t));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

void display_quit(void)
{
    SDL_DestroyWindow(window);
    SDL_Quit();
}

/**
 * Render character screen to raw screen
 *****/
void render_screen(void)
{
    int iscur = 0;
    for (int row = 0; row < HEIGHT; row++) {
        for (int col = 0; col < WIDTH; col++) {
            unsigned char ch = char_screen[row * WIDTH + col];

            curs_on = true; // Disable blink for now
            iscur = (row == cur_y && col == cur_x && curs_on);
            unsigned char attr = attr_screen[col + row*WIDTH];
            uint32_t fg = colors[ ATTR_FG(attr) +8];
            uint32_t fg_dim = ((colors[ ATTR_FG(attr) + 8 ] >> 1) & 0x7F7F7F7F) | 0xFF000000;
            uint32_t fg_low = colors[ ATTR_FG(attr)];
            uint32_t bg = colors[ ATTR_BG(attr) ];
            uint32_t bg_dim = ((colors[ ATTR_BG(attr)] >> 1) & 0x7F7F7F7F) | 0xFF000000;
            
                //ch = 0xDB; // Solid block
            // Draw the lines of the font
            for (int y = 0; y < FONT_HEIGHT; y++) {
                unsigned char line = font[ch + FONT_CHARS*y];
                int prevbit = 0;
                for (int x = 0; x < FONT_WIDTH; x++) {
                    int bit = (line & 0x80);
                    if (attr & ATTR_BOLD)
                        bit |= prevbit;
                    prevbit = (line & 0x80);
                    if (attr & ATTR_INV || iscur) {
                        raw_screen[ (row*FONT_HEIGHT + y)*FONT_VSCALE*DISP_WIDTH
                                   + col*FONT_WIDTH
                                   + x] = bit ? bg : fg;
                        if (FONT_VSCALE > 1) {
                        raw_screen[ (row*FONT_HEIGHT + y)*FONT_VSCALE*DISP_WIDTH + DISP_WIDTH
                                   + col*FONT_WIDTH
                                   + x] = bit ? bg_dim : (attr & ATTR_BOLD) ? fg_low : fg_dim;
                        }
                    } else {
                        raw_screen[ (row*FONT_HEIGHT + y)*FONT_VSCALE*DISP_WIDTH
                                   + col*FONT_WIDTH
                                   + x] = bit ? fg : bg;
                        if (FONT_VSCALE > 1) {
                        raw_screen[ (row*FONT_HEIGHT + y)*FONT_VSCALE*DISP_WIDTH + DISP_WIDTH
                                   + col*FONT_WIDTH
                                   + x] = bit ? (attr & ATTR_BOLD) ? fg_low : fg_dim : bg_dim;
                        }
                    }
                    line <<= 1;
                }
                
            }
        }
    }
}




    

Uint32 blink_timer(Uint32 interval, void *param) 
{
    SDL_Event event;
    SDL_UserEvent uevent;
    
    uevent.type = SDL_USEREVENT;
    uevent.code = 0;
    uevent.data1 = NULL;
    uevent.data2 = NULL;
    
    event.type = SDL_USEREVENT;
    event.user = uevent;
    
    SDL_PushEvent(&event);
    
    return interval;
}
