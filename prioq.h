/* prioq.h - a priority queue implementation */
/* (c) Martin Hradil, 2007 - license: GPL */

#ifndef __PRIOQ_H__
#define __PRIOQ_H__

#include <stdbool.h>
#include "config.h"

#ifndef PRIOQ_DATA
#define PRIOQ_DATA void *
#endif

#ifndef PRIOQ_DATA_NULL
#define PRIOQ_DATA NULL
#endif

typedef struct PrioItm PrioItm;
struct PrioItm {
	int prio;
	PRIOQ_DATA data;
};

typedef struct PrioQ PrioQ;
struct PrioQ {
	int count;
	int size;
	PrioItm queue[];
};

/* constructors */
PrioQ pq_new(void);
PrioQ pq_newa(int);

/* destructor */
void pq_delete(PrioQ);

/* queue ops */
bool pq_push(PrioQ *, int, PRIOQ_DATA);
bool pq_pushi(PrioQ *, PrioItm);
PRIOQ_DATA pq_pop(PrioQ *);
PrioItm pq_popi(PrioQ *);
PRIOQ_DATA pq_top(PrioQ *);
PrioItm pq_topi(PrioQ *);

#endif	/* __PRIOQ_H__ */
