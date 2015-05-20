all:
	cc  -pedantic -Wall -ansi -O4 -D_XOPEN_SOURCE=500  -D_BSD_SOURCE  -g 1.c -o 1

clean:
	rm -f 1
