/*
 * hcrond - a cron with crontab in mysql
 *
 *  Copyright 2006 Martin Hradil <himdel@seznam.cz>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  The GNU General Public License can also be found in the file
 *  `COPYING' that comes with the hcrond source distribution.
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#define __USE_BSD
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <mysql/mysql.h>

#include "options.h"
#include "locks.h"
#include "logs.h"

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 64
#endif

#define Free(x) if (x)\
	free((x))		
#define Gree(x) Free(gclst->x)

#define LIMO_SEC "0-59"
#define LIMO_MIN "0-59"
#define LIMO_HOUR "0-23"
#define LIMO_DAY "1-31"
#define LIMO_MON "1-12"
#define LIMO_DOW "0-6"

extern char **environ;

typedef struct Jobs Jobs;
struct Jobs {
	int id;
	int lastrun, nextrun;
	char *sec, *min, *hour, *day, *mon, *dow;
  	int andor;
	int uid, gid, nice;
	char *cmd, *name;
	int runonce;
	Jobs *n, *gn;
};
Jobs *curlst = NULL, *gclst = NULL;

typedef struct JoQ JoQ;
struct JoQ {
	char *cmd; 
	JoQ *n;
};
JoQ *job_queue_end = NULL, *job_queue_start = NULL;
int jobs_running = 0, job_queue_count = 0;

void addQ(const char *cmd);	/* adds copy of cmd to the job queue */
char *getQ(int lock);		/* gets the same (lock? is a bool) */
void run_jobs(void);
void refresh(int sig);
void run_this(char *cmd);
void childcare(int sig);
void gnerun(Jobs *j);		/* updates j->nextrun */
int isin(const char *z, int v, const char *limo);	/* is v in s? (bool); limo = "0-59" */
void trans(char **in, const char *foo, const char *bar);	/* trans - (*in) =~ s/$foo/$bar/g; */



int
main(int argc, char **argv)
{
	optmain(argc, argv);
	
	openlog("hcrond", LOG_PERROR | LOG_PID, LOG_CRON);
	syslog(LOG_INFO, "start");

	/* pidfile check 
	 *
	 * fopen(pidfile, "r");
	 * todo but start-stop-daemon already handles it
	 */

/* extern int allow_root;
 * extern int allow_uidgid;
 * extern int force_uid;
 * extern int force_gid;
 * extern char *force_hostname;
 * extern int ignore_machine;
 * extern int allow_notnice;
 * extern char *force_shell;
 * extern int force_shell_die;
 */
	if (!debug) {
		switch (fork()) {
			case 0:
				setsid();
//				if (signal(SIGCHLD, SIG_IGN) == SIG_ERR) {
					/* FIXME use sigaction and handle return values */
//					errf("signal: %s\n", strerror(errno));
//					return 1;
//				}
//				break;
			case -1:
				errf("fork: %s\n", strerror(errno));
				return 1;
			default:
				return 0;
		}
	}

	/* schedule reloads */
	/* FIXME use sigaction and handle return */
	signal(SIGALRM, refresh);
	signal(SIGCHLD, childcare);
	
	/* TODO handle SIGHUP, etc */

	refresh(SIGALRM);
	run_jobs();

	return 0;
}


void
refresh(int sig)
{	/* remember not to kill stuff used by run_jobs, just mv to gclst */
/* TODO pass gclst and curlst and if entry in both, solve (so as not to forgot lastrun */
	if (sig != SIGALRM) {
		errf("sanity check failure in refresh\n");
		return;
	}

	alarm(reload);
	signal(SIGALRM, refresh);

	/* free stuff in gclst */
	while (gclst) {
		dbg("killing %s\n", gclst->name);
		Gree(sec);
		Gree(min);
		Gree(hour);
		Gree(day);
		Gree(mon);
		Gree(dow);
		Gree(cmd);
		Gree(name);

		Jobs *x = gclst;
		gclst = gclst->gn;
		free(x);
	}

	/* open db */
	MYSQL *sql = mysql_init(NULL);
	if (!sql) { 
		err("mysql_init");
		return;
	}
	if (!mysql_real_connect(sql, host, user, pass, dbnm, port, NULL, 0)) {
		err("mysql_real_connect");
		return;
	}
	dbg("connected to mysql\n");

	char query[256];	/* FIXME constants are evil, use snprint and malloc */

	/* update lastrun from whole curlst */
	Jobs *act;
	act = curlst;
	while (act) {
		sprintf(query, "UPDATE %s SET lastrun = '%d', runonce = '%d' WHERE id = %d", table, act->lastrun, act->runonce, act->id);
		if (mysql_query(sql, query) != 0) 
			err("mysql_query (UPDATE)");
		act = act->n;
	}

	/* delete jobs with run */
	sprintf(query, "DELETE FROM %s WHERE runonce = 0", table);
	if (mysql_query(sql, query) != 0) 
		err("mysql_query (DELETE)");

	/* curlst becomes new gclst; later ->n's are set to point to new curlst */
	gclst = curlst;
	curlst = NULL;

/* TODO fields by name - using this:
 *
 * 	unsigned int num_fields;
 * 	unsigned int i;
 * 	MYSQL_FIELD *fields;
 * 	
 * 	num_fields = mysql_num_fields(result);
 * 	fields = mysql_fetch_fields(result);
 *
 * 	for(i = 0; i < num_fields; i++)
 * 		printf("Field %u is %s\n", i, fields[i].name);
 */

	MYSQL_RES *res;
	MYSQL_ROW row;
	sprintf(query, "SELECT * FROM %s ORDER BY id", table);
	res = (mysql_query(sql, query) == 0) ? mysql_store_result(sql) : NULL;
	if (res)
		for (int i = 0; (row = mysql_fetch_row(res)); i++) {
			Jobs *nw;
			nw = malloc(sizeof(Jobs));
			if (!nw) { /* FIXME better */
				errf("malloc: %s\n", strerror(errno));
				return;
			}
			nw->id = atoi(row[0]);
			nw->lastrun = atoi(row[14]);
			scp(nw->sec, row[1]);
			scp(nw->min, row[2]);
			scp(nw->hour, row[3]);
			scp(nw->day, row[4]);
			scp(nw->mon, row[5]);
			scp(nw->dow, row[6]);
			nw->andor = (row[12] && (*(row[12]) == '&')) ? 1 : 0;
			scp(nw->name, row[11]);
			scp(nw->cmd, row[10]);
			nw->runonce = atoi(row[15]);

			/* FIXME get from locale maybe */
			trans(&nw->dow, "Sun", "0");
			trans(&nw->dow, "Mon", "1");
			trans(&nw->dow, "Tue", "2");
			trans(&nw->dow, "Wed", "3");
			trans(&nw->dow, "Thu", "4");
			trans(&nw->dow, "Fri", "5");
			trans(&nw->dow, "Sat", "6");
			if (strcmp(nw->dow, "7") == 0)
				*(nw->dow) = '0';

			trans(&nw->mon, "Jan", "1");
			trans(&nw->mon, "Feb", "2");
			trans(&nw->mon, "Mar", "3");
			trans(&nw->mon, "Apr", "4");
			trans(&nw->mon, "May", "5");
			trans(&nw->mon, "Jun", "6");
			trans(&nw->mon, "Jul", "7");
			trans(&nw->mon, "Aug", "8");
			trans(&nw->mon, "Sep", "9");
			trans(&nw->mon, "Oct", "10");
			trans(&nw->mon, "Nov", "11");
			trans(&nw->mon, "Dec", "12");

			trans(&nw->sec, "*", LIMO_SEC);
			trans(&nw->min, "*", LIMO_MIN);
			trans(&nw->hour, "*", LIMO_HOUR);
			trans(&nw->day, "*", LIMO_DAY);
			trans(&nw->mon, "*", LIMO_MON);
			trans(&nw->dow, "*", LIMO_DOW);
			
			dbg("job %s (%s): %s %s %s %s %s %s %c\n", nw->name, nw->cmd, nw->sec, nw->min, nw->hour, nw->day, nw->mon, nw->dow, nw->andor);

			gnerun(nw);	/* update nw->nextrun */

			nw->n = nw->gn = curlst;
			curlst = nw;
		}
	mysql_free_result(res);

	/* ->n's are set to point to the new curlst */
	Jobs *x;
	x = gclst;
	while (x) {
		x->n = curlst;
		x = x->gn;
	}
	
	/* close db */
	mysql_close(sql);
	dbg("disconnect");
}

/* run_this - TODO comment
 *	 called by
 *	  -run_jobs (scheduled for now)
 *	  -childcare (a job has ended so we can run another)
 */
void
run_this(char *cmd)
{
	dbg("run_this(\"%s\")\n", cmd);
	if (max_jobs != 0) {
		if (jobs_running >= max_jobs) {
			dbg("queueing %s\n", cmd);
			addQ(cmd);
			return;
		} else {
			LOCKED(jobs_running++);
		}
	}
	switch (fork()) {
		case -1:
			errf("fork error\n");
			return;
		case 0:
			/* TODO handle uid, gid and nice*/
			/// setuid(uid);
			// setgid(gid);
			dbg("running %s\n", cmd);
			
			char *argv[4] = {"sh", "-c", cmd, 0};

			execve("/bin/sh", argv, environ);

			errf("something amiss\n");
			exit(1);
			break;
	}
}


void
run_jobs(void)
{
	int ttc = 0;
	for (;;) {
		Jobs *s;
		time_t tm;

		tm = time(NULL);
		s = curlst;
		if (s == NULL) {
			if (ttc < reload)
				ttc++;
		} else {
			ttc = 0;
		}
			
		while (s) {
			if (s->runonce == 0) { /* -1 = eternal, 0 = no longer, n = n times yet */
				s = s->n;
				continue;
			}
			if ((tm >= s->nextrun) && (s->lastrun != s->nextrun)) {
				s->lastrun = tm;
				if (s->runonce > 0) {
					s->runonce--;
					dbg("job %s will run %d times yet\n", s->name, s->runonce);
				}
				run_this(s->cmd);
				gnerun(s);
			}
			/* ttc - time to sleep */
			if (!ttc)
				ttc = s->nextrun - tm;
			else if (s->nextrun - tm < ttc)
				ttc = s->nextrun - tm;
			s = s->n;
		}
		dbg("sleeping for %d sec\n", ttc);
		usleep(ttc * 1000000);
	}
}
/* l8r:
 * char hname[HOST_NAME_MAX + 1];
 * if (gethostname(hname, HOST_NAME_MAX + 1))
 * 	strcpy(hname, "localhost");
 */


void
childcare(int sig)
{	
	if (sig != SIGCHLD)
		return;

	signal(SIGCHLD, childcare);
	
	int st, pid;
	pid = wait(&st);

	inf("%d is dead\n", pid);
	/* TODO syslog name and status */
	
	LOCKED(jobs_running--);

	char *cmd = NULL;
	LOCK;
	if (job_queue_count)
		cmd = getQ(0);
	UNLOCK;
	if (cmd) {
		run_this(cmd);
		free(cmd);
	}
}


/* gnerun - updates j->nextrun */
/* TODO add option timezone - expecting UTC */
/* TODO negative options */
void
gnerun(Jobs *j)
{
	if ((j == NULL) || (j->runonce == 0))
		return;

	time_t tm;
	struct tm *tim;

	/* +1 so we don't repeat a task n-times within 1 sec */
	tm = time(NULL);
	if (tm == j->lastrun)
		tm++;
	tim = gmtime(&tm);

nday:	while (!((j->andor && isin(j->day, tim->tm_mday, LIMO_DAY) && isin(j->mon, tim->tm_mon, LIMO_MON) && isin(j->dow, tim->tm_wday, LIMO_DOW)) || ((!j->andor) && ((isin(j->day, tim->tm_mday, LIMO_DAY) && isin(j->mon, tim->tm_mon, LIMO_MON)) || isin(j->dow, tim->tm_wday, LIMO_DOW))))) {
		tm += 24 * 3600;
		tm -= tim->tm_sec + 60 * tim->tm_min + 3600 * tim->tm_hour;
		tim = gmtime(&tm);
	}
nhour:	while (!isin(j->hour, tim->tm_hour, LIMO_HOUR)) {
		tm += 3600;
		tm -= tim->tm_sec + 60 * tim->tm_min;
		tim = gmtime(&tm);
		if (tim->tm_hour == 0)
			goto nday;
	}
nmin:	while (!isin(j->min, tim->tm_min, LIMO_MIN)) {
		tm += 60;
		tm -= tim->tm_sec;
		tim = gmtime(&tm);
		if (tim->tm_min == 0)
			goto nhour;
	}
	while (!isin(j->sec, tim->tm_sec, LIMO_SEC)) {
		tm++;
		tim = gmtime(&tm);
		if (tim->tm_sec == 0)
			goto nmin;
	}
	dbg("job %s will be run in %d sec (%04d-%02d-%02d %02d:%02d:%02d)\n", j->name, (int) (tm - time(NULL)), 1900 + tim->tm_year, tim->tm_mon, tim->tm_mday, tim->tm_hour, tim->tm_min, tim->tm_sec);
	j->nextrun = tm;
}


int
isin(const char *z, int v, const char *limo)
{
	char *c, *s;
	int e = 0;
	
	if (!z)
		return 0;
	scp(s, z);
	
	do {
		c = s;
		s = strchr(s, ',');
		if (s == NULL) {
			e = 1;
		} else {
			*s = 0;
			s++;
		}
		/* got token in c; rest in s or e 1 */
		if (*c == '*') {
			warnf("internal error: unconverted asterisk found\n");
			return 1;
		}
		if (strchr(c, '-') == NULL) {
			if (atoi(c) == v) 
				return 1;
			else
				continue;
		}
		/* Interval */ 
		char *d, *e;
		int a, b, f;
		d = strchr(c, '-');
		*d = 0;
		d++;
		e = strchr(d, '/');
		if (e) {
			*e = 0;
			e++;
			f = atoi(e);
		}
		a = atoi(c);
		b = atoi(d);

/* to support intervals
 * {(a,b) | \forall a,b \elem \N0 | a > b }		(things like Fri-Mon)
 * to be unrolled to
 * (a, a + 1, ..., n - 1, n, 0, 1, ..., b - 1, b)	(things like 5,6,0,1))
 * we're using limo to define 0 and n by assuming it to be "0-n"
 */
		if ((b < a) && ((v >= a) || (v <= b))) {
			if (!e)
				return 1;
// FIXME check validity of string first (must be /^[0-9]+-[0-9]+$/)
			int limoa, limob;

			limoa = atoi(limo);
			limob = atoi(strchr(limo, '-') + 1);
			while ((a != v) && (a != b))
				a = ((a + f) % limob) + limoa;
			if ((a != v) && (a == b))
				a = ((a + f) % limob) + limoa;
			if (a == v)
				return 1;
		} else if ((v >= a) && (v <= b)) {
			if (!e) 
				return 1;
			while ((a != v) && (a <= b))
				a += f;
			if (a == v)
				return 1;
		}
	} while (!e);

	return 0;
}


/* trans - (*in) =~ s/$foo/$bar/g; */
void
trans(char **in, const char *foo, const char *bar)
{
	int l = 0;
	char *x, *y, *z;

	x = y = *in;

	while ((x = strstr(x, foo)))
		l++, x += strlen(foo);
	
	if (!l)
		return;
	
	z = x = malloc(strlen(*in) + (l * (strlen(bar) - strlen(foo))) + 1);
	if (!x) {
		errf("memory\n");
		return;
	}
	
	while(*y) {
		if (strstr(y, foo) == y) {
			*z = 0;
			strcat(z, bar);
			z += strlen(bar);
			y += strlen(foo);
		} else {
			*(z++) = *(y++);
		}
	}
	*z = 0;

	free(*in);
	*in = x;
}


/* addQ - adds copy of cmd to the job queue */
void
addQ(const char *cmd)
{
	JoQ *n;
	n = malloc(sizeof(JoQ));
	if (!n) {
		errf("memory error, forgetting %s !!\n", cmd);
		return;
	}
	n->cmd = malloc(strlen(cmd) + 1);
	if (!n->cmd) {
		errf("memory error, forgetting %s !!\n", cmd);
		free(n);
		return;
	}
	strcpy(n->cmd, cmd);
	n->n = NULL;
	LOCK;
	if (job_queue_end)
		job_queue_end->n = n;
	if (!job_queue_start)
		job_queue_start = n;
	job_queue_end = n;
	job_queue_count++;
	if (job_queue_count % 10 == 0)
		warnf("warning: queue contains %d items\n", job_queue_count);
	UNLOCK;
}


/* getQ - gets a cmd from the job queue (lock (bool) ?) */
char *
getQ(int lock)
{
	char *s = NULL;
	JoQ *b = NULL;
	if(lock)
		LOCK;
	if ((b = job_queue_start))
		job_queue_start = b->n;
	if (b->n == NULL)
		job_queue_end = NULL;
	job_queue_count--;
	if(lock)
		UNLOCK;
	s = b->cmd;
	free(b);
	return s;
}
