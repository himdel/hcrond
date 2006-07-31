CC=gcc
LD=gcc
CFLAGS=-std=c99 -pedantic -Wall -W
LDFLAGS=-lmysqlclient
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
