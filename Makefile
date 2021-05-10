all:
	gcc -o term term.c font.c disp.c ansi.c -lSDL2 -O2
