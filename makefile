all: compakt

compakt: compakt.c *.h
	gcc -O3 -o compakt compakt.c -lSDL2 -lSDL2_image -lm -ljack -lportmidi -lpthread -lfftw3

run:
	./compakt

clean:
	rm compakt
