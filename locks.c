#include <stdlib.h>

#include "locks.h"
#include "options.h"
#include "logs.h"

#warning TODO a better way, becase rand isn't thread safe, this might not help

int sem = 0;

void
lock_wait(void)
{
	dbg("lock_wait\n");
	int a;
	a = (rand() % 65535) + 1;
	for (;;) {
		if (sem == 0)
			sem = a;
		if (sem == a) {
				dbg("lock_wait out %d\n", a);
				return;
			}
	}
}


void
lock_unlock(void)
{
	dbg("lock_unlock\n");
	sem = 0;
}
