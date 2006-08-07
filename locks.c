#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#define __USE_POSIX
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>

#include "locks.h"
#include "logs.h"

sem_t sem;

void
lock_init(void)
{
	if (sem_init(&sem, 0, 1) < 0)
		errf("sem_init");
}


void
lock_kill(void)
{
	sem_destroy(&sem);
}


void
lock_wait(void)
{
	while (sem_wait(&sem));
}


void
lock_unlock(void)
{
	sem_post(&sem);
}
