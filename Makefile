CC=gcc
LD=gcc

CFLAGS=-std=c99 -pedantic -Wall -W
LDFLAGS=-lmysqlclient -ldaemon
LEX=flex

OBJS=options.o main.o prioq.o
GEND=options.c version.h

all: HAVE_libdaemon hcrond

HAVE_libdaemon:
	pkg-config libdaemon

hcrond: $(OBJS)
	$(LD) -o $@ $^ $(LDFLAGS)

main.o: main.c options.h logs.h compat.h config.h

options.o: options.c options.h version.h help.c compat.h

prioq.o: prioq.c prioq.h config.h

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
