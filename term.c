#define _XOPEN_SOURCE 600
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <stdbool.h>

#include "disp.h"
#include "ansi.h"

#define BUF_SIZ 10240       // Max characters to read from PTY at once
 

int main(int argc, char **argv)
{
    int pty_master;
    int pty_slave;
    pid_t cpid;

    errno = 0;
    if ((pty_master = posix_openpt(O_RDWR | O_NOCTTY)) < 0) {
        fprintf(stderr, "Failed to allocate PTY: %s\n", strerror(errno));
        return 1;
    }

    printf("PTY: %s\n", ptsname(pty_master));
    
    if (grantpt(pty_master) < 0) {
        fprintf(stderr, "grantpt failed: %s\n", strerror(errno));
        return 1;
    }

    if (unlockpt(pty_master) < 0) {
        fprintf(stderr, "unlockpt failed: %s\n", strerror(errno));
        return 1;
    }

    if (ptsname(pty_master) == NULL) {
        fprintf(stderr, "No name for pts.\n");
        return 1;
    }

    if ((pty_slave = open(ptsname(pty_master), O_RDWR | O_NOCTTY)) < 0) {
        fprintf(stderr, "Failed to open slave pty: %s\n", strerror(errno));
        return 1;
    }

    // xterm-color   allows some applications like htop to work, and my bashrc to have color
    //               however some programs use xterm escape codes
    // ansi          A closer match to what my terminal supports, but applications seem to go
    //               beyond the standard and sometimes break with this TERM
    
    char *env[] = { "TERM=ansi", "LANG=C", "LINES=25", "COLUMNS=80", NULL };


    if ((cpid = fork()) < 0) {
        fprintf(stderr, "Failed to fork: %s\n", strerror(errno));
    }
    
    if (cpid == 0) {  // Child
        // Make the parent this processes controlling terminal. (New session)
        close(pty_master);
        setsid(); 

        if (ioctl(pty_slave, TIOCSCTTY, NULL) < 0) {
            fprintf(stderr, "ioctl TIOCSCTTY failed: %s\n", strerror(errno));
            return 1;
        }

        dup2(pty_slave, STDIN_FILENO);
        dup2(pty_slave, STDOUT_FILENO);
        dup2(pty_slave, STDERR_FILENO);
        close(pty_slave); 

        execle("/bin/bash", "-/bin/bash", NULL, env);
        return 1; // Never reached
    } 
    // Parent
    close(pty_slave);

    fd_set fds;

    struct timeval to;

    char buf[BUF_SIZ];

    display_init();
    
    SDL_Event e;
    int pos = 0;
    int dirty = 0;
    bool running = true;
    
    bool mod_ctrl = false;
    bool mod_meta = false;
    
    
    while (running) {
        if (SDL_PollEvent(&e)) {
            do {
                if (e.type == SDL_QUIT)
                    running = false;
                else if (e.type == SDL_TEXTINPUT) {
                    // For some reason when ctrl is held no TEXTINPUT events
                    // are fired when a letter is pressed.
                    // They do get fired here for some numbers and symbols though
                    
                    // Lazy for now, assume ASCII (ignore utf-8 escapes)
                    char ch = e.text.text[0];
                    if (ch >= 0 && ch < 0x80) {
                        if (mod_ctrl) {
                            ch &= 0x1F;
                        }
                        if (mod_meta)
                            write(pty_master, "\033", 1);
                        
                        write(pty_master, &ch, 1);
                    }
                } 
                else if (e.type == SDL_KEYDOWN) {
                    if (e.key.keysym.mod & KMOD_CTRL) {
                        // A complete hack, but it works to get the ascii here
                        char ch = (char)e.key.keysym.sym;
                        if (ch >= 'a' && ch <= 'z') {
                            ch = ch & 0x1F;
                            write(pty_master, &ch, 1);
                        }
                    } else {
                    switch (e.key.keysym.sym) {
                    case SDLK_RETURN:
                        write(pty_master, "\n", 1);
                        break;
                    case SDLK_BACKSPACE:
                        write(pty_master, "\b", 1);
                        break;
                    case SDLK_TAB:
                        write(pty_master, "\t", 1);
                        break;
                    case SDLK_UP:
                        write(pty_master, "\033[A", 3);
                        break;
                    case SDLK_DOWN:
                        write(pty_master, "\033[B", 3);
                        break;
                    case SDLK_LEFT:
                        write(pty_master, "\033[D", 3);
                        break;
                    case SDLK_RIGHT:
                        write(pty_master, "\033[C", 3);
                        break;
                    case SDLK_LALT:
                    case SDLK_RALT:
                        mod_meta = true;
                        break;
                    case SDLK_ESCAPE:
                        write(pty_master, "\033", 1);
                    }
                    }
                } else if (e.type == SDL_KEYUP) {
                    switch (e.key.keysym.sym) {
                    case SDLK_LALT:
                    case SDLK_RALT:
                        mod_meta = false;
                        break;
                    }
                } else if (e.type == SDL_USEREVENT) {
                    curs_on = !curs_on;
                    dirty++;
                }
            } while (SDL_PollEvent(&e));
        }
        
        to.tv_sec = 0;
        to.tv_usec = 1000; // 1ms
        FD_ZERO(&fds);
        FD_SET(pty_master, &fds);
        
        int count = select(pty_master+1, &fds, NULL, NULL, &to);
        ssize_t rcount = 0;
        
        if (count) {
            // Data to read
            if ((rcount = read(pty_master, buf, BUF_SIZ)) < 1) {
                // No data read, child exited.
                fprintf(stderr, "Nothing to read, exiting: %s\n", strerror(errno));
                return 2;
            }

            dirty = handle(buf, rcount);
        }
    
        int childstatus;
        int rval;
        if ((rval = waitpid(cpid, &childstatus, WNOHANG)) != 0) {
            if (rval < 0) {
                fprintf(stderr, "Error reading child status: %s\n", strerror(errno));
                running = false;
            }
            if (WIFEXITED(childstatus)) {
                fprintf(stderr, "Child exitied code %hhu\n", WEXITSTATUS(childstatus));
                running = false;
            }
            if (WIFSIGNALED(childstatus)) {
                fprintf(stderr, "Child terminated %hhu\n", WTERMSIG(childstatus));
                running = false;
            }
        }
        
        if (dirty) {
            dirty = 0;
            display_update();
        }

        display_redraw();
        SDL_Delay(5);
    }
    
    kill(cpid, SIGKILL); // Agressive for now. (Install SIGALRM handler to give child a second to SIGTERM then SIGKILL)
    display_quit();

    return 0;
}


