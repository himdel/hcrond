CC=gcc
LD=gcc

# gnu99 note: sorry, I can't compile main.c (sigaction stuff) with -std=c99
#	 don't know why. So I use gnu99. If you don't use gcc, try c99 and
#	 propably don't forget to uncomment __USE_POSIX in locks.c
CFLAGS=-std=gnu99 -pedantic -Wall -W
LDFLAGS=-lmysqlclient -lpthread
LEX=flex

OBJS=options.o main.o locks.o
GEND=options.c version.h

hcrond: $(OBJS)
	$(LD) -o $@ $^ $(LDFLAGS)

main.o: main.c options.h locks.h logs.h

options.o: options.c options.h version.h help.c

locks.o: locks.c locks.h logs.h options.h

options.c: options.l

version.h: VERSION
	echo \#define VERSION \"`cat $<`\" > $@

install: hcrond
	cp -ai etc /
	cp -i hcrond /usr/bin

tgz: clean
	scripts/tgz

clean:
	rm -f hcrond $(OBJS) $(GEND)
