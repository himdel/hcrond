/* prioq.c - a priority queue implementation */
/* (c) Martin Hradil, 2007 - license: GPL */

#include <stdbool.h>
#include "config.h"
#include "prioq.h"

#ifndef PRIOQ_DEF_SIZ
#define PRIOQ_DEF_SIZ 0
#endif

/*
 * typedef struct PrioItm PrioItm;
 * struct PrioItm {
 * 	int prio;
 * 	PRIOQ_DATA data;
 * };
 * 
 * typedef struct PrioQ PrioQ;
 * struct PrioQ {
 * 	int count;
 * 	int size;
 * 	PrioItm queue[];
 * };
 */

/* constructors */
PrioQ
pq_new(void)
{
	return pq_newa(PRIOQ_DEF_SIZ);
}

PrioQ
pq_newa(int n)
{
	PrioQ pq;
	pq.count = 0;
	pq.size = ((n > 0) && (pq.queue = malloc(n * sizeof(PrioItm)))) ? n : 0;
	return pq;
}

/* destructor */
void
pq_delete(PrioQ pq)
{
	if ((pq.size > 0) && pq.queue)
		free(pq.queue);
}

/* queue ops */
bool
pq_push(PrioQ *pq, int p, PRIOQ_DATA d)
{
	PrioItm i;
	i.prio = p;
	i.data = d;
	return pq_pushi(pq, i);
}

bool
pq_pushi(PrioQ *pq, PrioItm i)
{
	if ((!pq) || (!pq->queue))
		return false;
	// TODO
}

PRIOQ_DATA
pq_pop(PrioQ *pq)
{
	return pg_popi(pq).data;
}

PrioItm 
pq_popi(PrioQ *pq)
{
	// TODO
}

PRIOQ_DATA
pq_top(PrioQ *pq)
{
	return pg_topi(pq).data;
}

PrioItm 
pq_topi(PrioQ *pq)
{
	if ((pq == NULL) || (pq->queue == NULL) || (pq->count =< 0)) {
		PrioItm x;
		x.priority = -1;
		x.data = PRIOQ_DATA_NULL;
		return x;
	}
	return *(pq->queue);
}
