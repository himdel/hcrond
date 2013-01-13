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
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <mysql/mysql.h>
#include <libdaemon/dfork.h>
#include <libdaemon/dlog.h>
#include <libdaemon/dsignal.h>

#include "options.h"
#include "logs.h"
#include "compat.h"

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 64
#endif

#define Free(x) { if (x) free(x); x = NULL; }
#define Gree(x) Free(jlst->x)

/* for isin() */
#define LIMO_SEC "0-59"
#define LIMO_MIN "0-59"
#define LIMO_HOUR "0-23"
#define LIMO_DAY "1-31"
#define LIMO_MON "1-12"
#define LIMO_DOW "0-6"

extern char **environ;

/* job list - TODO tree */
typedef struct Jobs Jobs;
struct Jobs {
	int id;
	int lastrun, nextrun;
	char *sec, *min, *hour, *day, *mon, *dow;
  	int andor;
	int uid, gid, nic;
	char *cmd, *name;
	int runonce;
	Jobs *n;
};
Jobs *jlst = NULL;

/* jobs queue */
typedef struct JoQ JoQ;
struct JoQ {
	char *cmd; 
	int uid, gid, nic;
	JoQ *n;
};
JoQ *job_queue_end = NULL, *job_queue_start = NULL;
int jobs_running = 0, job_queue_count = 0;

int main(int argc, char **argv);
int run_jobs(void);
MYSQL *db_connect();
int db_update(MYSQL *sql, Jobs *j);
Jobs *refresh(Jobs *jlst, int load);
void run_this(const char *cmd, int uid, int gid, int nic);
void childcare(void);
void gnerun(Jobs *j);
int isin(const char *z, int v, const char *limo);
void trans(char **in, const char *foo, const char *bar);
void addQ(const char *cmd, int uid, int gid, int nic);
char *getQ(int *uid, int *gid, int *nic);
int queryf(MYSQL *sql, const char *fmt, ...);
int maybe_atoi(const char *s, int x);



/* main main */
int
main(int argc, char **argv)
{
	/* options */
	optmain(argc, argv);
	
	/* SIGINT running hcrond */
	if (kill) {
		err("do it yourself");
		return 1;
	}

	/* start log */
	daemon_log_ident = "hcrond";
	daemon_log_use = DAEMON_LOG_AUTO | (debug ? DAEMON_LOG_STDERR : 0);

	/* fork */
	if (!debug) {
		daemon_retval_init();
		switch (daemon_fork()) {
			case 0: /* now a proper little daemon child */
				daemon_retval_send(42);
				break;
			case -1: /* fork error */
				err("fork: %s", strerror(errno));
				daemon_retval_done();
				return 1;
			default: /* parent dies */
				if (daemon_retval_wait(16) != 42)
					wrn("didn't recieve proper notification from the daemon");
				return 0;
		}
	}
	inf(debug ? "start (debug = 1)" : "start");

	/** signals
	 *  - reloading from db is done on SIGALRM
	 *  - if jobs_running < max_jobs, SIGCHLD starts new tasks
	 */
	if (daemon_signal_init(SIGALRM, SIGCHLD, SIGINT, SIGQUIT, SIGHUP, 0) != 0) {
		err("can't establish signal handlers: %s", strerror(errno));
		inf("hcrond currently depends on signals");
		return 1;
	}
	atexit(daemon_signal_done);
	
	/* start looping - SIGALRM used to cause reload from db */
	int ret;
	raise(SIGALRM);
	ret = run_jobs();
	
	switch(ret) {
		case SIGINT:
			inf("exitting on SIGINT");
			break;
		case SIGQUIT:
			inf("exitting on SIGQUIT");
			break;
		case SIGHUP:
			inf("SIGHUP - restarting");
			break;
		default:
			inf("exitting on error");
	}

	if (ret == SIGHUP) {	/* reload, not die */
		daemon_signal_done();
		execvp(argv[0], argv);
		/* something's terribly wrong */
		err("sorry, can't reload, dying");
		_exit(1);
	}

	/** done by atexit:
	 *	daemon_signal_done();
	 */

	return 0;
}


/* run_jobs - main job loop - handles jobs and signals */
int
run_jobs(void)
{
	for (;;) {
		int sig = daemon_signal_next();
		if (sig < 0)
			wrn("daemon_signal_next: %d", sig);
		else if (sig > 0)
			switch(sig) {
				case SIGALRM:
					jlst = refresh(jlst, 1);
					alarm(reload);
					break;
				case SIGCHLD:
					childcare();
					break;
				default:	/* SIGINT, SIGQUIT, SIGHUP */
					refresh(jlst, 0);
					/* TODO the queue shouldn't be forgotten!!! */
					return sig;
			}
		
		/* TODO can select now if ((ttc > 0) || (max_jobs && (jobs_running >= max_jobs))) when have tree */

		Jobs *s = jlst;
		time_t tm = time(NULL);
		while (s) {
			if (s->runonce == 0) { /* -1 = eternal, 0 = no longer, n = n times yet */
				s = s->n;
				continue;
			}
			if ((tm >= s->nextrun) && (s->lastrun != tm)) {
				s->lastrun = tm;
				if (s->runonce > 0) {
					s->runonce--;
					dbg("job %s will run %d times yet", s->name, s->runonce);
				}
				run_this(s->cmd, s->uid, s->gid, s->nic);
				gnerun(s);
			}
			s = s->n;
		}
		// TODO use daemon_signal_fd with select instead (so can use ttc)
		usleep(1000000);
	}
}


/* db_connect - connect to the db */
MYSQL *
db_connect()
{
	MYSQL *sql = mysql_init(NULL);
	if (!sql) { 
		err("mysql_init: %s", mysql_error(sql));
		return NULL;
	}
	if (!mysql_real_connect(sql, host, user, pass, dbnm, port, NULL, 0)) {
		err("mysql_real_connect: %s", mysql_error(sql));
		mysql_close(sql);
		return NULL;
	}
	dbg("connected to mysql");
	return sql;
}


/* queryf - formatted query */
int
queryf(MYSQL *sql, const char *fmt, ...)
{
	int n;
	char *s = NULL, z[1];
	va_list ap;
	
	va_start(ap, fmt);
	n = vsnprintf(z, 1, fmt, ap);
	va_end(ap);
	
	s = malloc((n + 1) * sizeof(char));
	if (!s) {
		err("queryf: not enough memory for query");
		return -1;
	}
	
	va_start(ap, fmt);
	vsnprintf(s, n + 1, fmt, ap);
	va_end(ap);
	
	n = mysql_query(sql, s);
	free(s);
	return n;
}


/* db_update - update lastrun and runonce in the table, remove run_once == 0 */
int
db_update(MYSQL *sql, Jobs *j)
{
	int e = 0;

	while (j) {
		if (queryf(sql, "UPDATE %s SET lastrun = '%d', runonce = '%d' WHERE id = %d", table, j->lastrun, j->runonce, j->id) != 0) {
			err("mysql_query (UPDATE): %s", mysql_error(sql));
			e++;
		}
		j = j->n;
	}

	if (queryf(sql, "DELETE FROM %s WHERE runonce = 0", table) != 0) {
		err("mysql_query (DELETE): %s", mysql_error(sql));
		e++;
	}

	return e;
}

/* refresh - updates the db and if load, reloads */
Jobs *
refresh(Jobs *jlst, int load)
{	
	MYSQL *sql;

	/* connect */
	if (!(sql = db_connect())) {
		wrn("failed to connect to the db, using the old list");
	 	return jlst;
	}
	
	/* update lastrun and runonce in the table */
	if (jlst) {
		int n;
		if ((n = db_update(sql, jlst)) != 0)
			wrn("db_update failed %d times, your problem", n);
	}
	
	/* clean the list */
	while (jlst) {
		Gree(sec);
		Gree(min);
		Gree(hour);
		Gree(day);
		Gree(mon);
		Gree(dow);
		Gree(cmd);
		Gree(name);

		Jobs *x = jlst;
		jlst = x->n;
		free(x);
	}
	jlst = NULL;	/* no-op */

	if (!load) {	/* just updating (before death) */
		mysql_close(sql);
		return NULL;
	}

	/* load anew */
	MYSQL_RES *res;
	res = (queryf(sql, "SELECT * FROM %s ORDER BY id", table) == 0) ? mysql_store_result(sql) : NULL;
	if (res) {
		MYSQL_ROW row;
		for (int i = 0; (row = mysql_fetch_row(res)); i++) {
			Jobs *nw;
			nw = malloc(sizeof(Jobs));
			if (!nw) {
				err("malloc: %s", strerror(errno));
				break;
			}
			nw->id = atoi(row[0]);
			nw->lastrun = atoi(row[14]);
			nw->sec = strdup(row[1]);
			nw->min = strdup(row[2]);
			nw->hour = strdup(row[3]);
			nw->day = strdup(row[4]);
			nw->mon = strdup(row[5]);
			nw->dow = strdup(row[6]);
			nw->uid = maybe_atoi(row[7], -1);
			nw->gid = maybe_atoi(row[8], -1);
			nw->andor = (row[12] && (*(row[12]) == '&')) ? 1 : 0;
			nw->nic = maybe_atoi(row[13], 0);
			nw->name = strdup(row[11]);
			nw->cmd = strdup(row[10]);
			nw->runonce = atoi(row[15]);

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
			
			gnerun(nw);	/* updates nw->nextrun */
			nw->n = jlst;
			jlst = nw;
		}
		mysql_free_result(res);
	}
	
	/* close db */
	mysql_close(sql);
	return jlst;
}


/* run_this - runs or queues job
 *	 called by
 *	  -run_jobs (scheduled for now)
 *	  -childcare (a job has ended so we can run another)
 */
void
run_this(const char *cmd, int uid, int gid, int nic)
{
	if (max_jobs && (jobs_running >= max_jobs)) {
		dbg("queueing %s", cmd);
		addQ(cmd, uid, gid, nic);
		return;
	}

	/* really running it now */
	switch (fork()) {
		case -1:
			err("fork error, job %s not run, queueing", cmd);
			addQ(cmd, uid, gid, nic);
			return;
		case 0:
			if (allow_uidgid && (uid != -1))
				if (setuid(uid))
					wrn("setuid: can't set uid to %d: %s", uid, strerror(errno));
			if (allow_uidgid && (gid != -1))
				if (setgid(gid))
					wrn("setgid: can't set gid to %d: %s", uid, strerror(errno));
			if ((nic > 0) || ((nic < 0) && allow_notnice)) {
				errno = 0;
				if ((nice(nic) == -1) && errno)
					wrn("nice: can't renice by %d: %s", nic, strerror(errno));
			}
			dbg("running %s", cmd);
			
			char *argv[4] = {"sh", "-c", cmd, 0};	/* compiler warns about discarding constness of cmd but we are forked so who cares */

			execve("/bin/sh", argv, environ);

			err("something amiss");
			exit(1);
			break;
		default:
			jobs_running++;
	}
}


/* handles children */
void
childcare(void)
{	
	int st, pid;
	pid = waitpid(-1, &st, WNOHANG);
	
	if (pid == -1) {
		dbg("waitpid error, weird");
	} else if (pid == 0) {
		dbg("no child has terminated yet a SIGCHILD was delivered");
	} else {
		inf("%d is dead, status %d", pid, WEXITSTATUS(st));
		jobs_running--;
	
		char *cmd = NULL;
		int uid, gid, nic = 0;
		uid = gid = -1;
		
		if (job_queue_count)
			cmd = getQ(&uid, &gid, &nic);
		if (cmd) {
			run_this(cmd, uid, gid, nic);
			free(cmd);	/* malloced by addQ */
		}
	}
}


/* gnerun - updates j->nextrun */
void
gnerun(Jobs *j)
{
	if ((j == NULL) || (j->runonce == 0))
		return;

	time_t tm, tmo;
	struct tm *tim;

	/* +1 so we don't repeat a task n-times within 1 sec */
	tm = tmo = time(NULL);
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
	j->nextrun = tm;
}


/* is {v \elem *z}? */
int
isin(const char *z, int v, const char *limo)
{
	char *c, *s;
	int e = 0;
	
	if (!z)
		return 0;
	s = strdup(z);
	if (!s)
		return 0;
	
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
			wrn("internal error: unconverted asterisk found");
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
	if ((!in) || (!(*in)) || (!foo)) {
		dbg("trans called with a NULL <in>, <*in> or <foo> argument");
		return;
	}

	int l = 0;
	char *x, *y, *z;

	x = y = *in;
	while ((x = strstr(x, foo)))
		l++, x += strlen(foo);
	
	if (!l)	/* no $foo, so no change */
		return;
	
	z = x = malloc(strlen(y) + (l * ((bar ? strlen(bar) : 0) - strlen(foo))) + 1);
	if (!z) {
		err("memory");
		return;
	}
	
	while (*y) {
		if (strstr(y, foo) == y) {
			*z = 0;
			if (bar) {
				strcat(z, bar);
				z += strlen(bar);
			}
			y += strlen(foo);
		} else {
			*(z++) = *(y++);
		}
	}
	*z = 0;

	free(*in);
	*in = x;
}


/* addQ - adds a copy of cmd to the job queue */
void
addQ(const char *cmd, int uid, int gid, int nic)
{
	JoQ *n;
	n = malloc(sizeof(JoQ));
	if (!n) {
		err("memory error, forgetting %s !!", cmd);
		return;
	}
	n->cmd = malloc(strlen(cmd) + 1);
	if (!n->cmd) {
		err("memory error, forgetting %s !!", cmd);
		free(n);
		return;
	}
	strcpy(n->cmd, cmd);
	n->uid = uid;
	n->gid = gid;
	n->nic = nic;
	n->n = NULL;
	if (job_queue_end)
		job_queue_end->n = n;
	if (!job_queue_start)
		job_queue_start = n;
	job_queue_end = n;
	job_queue_count++;
	if (job_queue_count % 10 == 0)
		wrn("queue contains %d items", job_queue_count);
}


/* getQ - gets a cmd from the job queue and remove it */
char *
getQ(int *uid, int *gid, int *nic)
{
	char *s = NULL;
	JoQ *b = NULL;
	if ((b = job_queue_start)) {
		job_queue_start = b->n;
		job_queue_count--;
		s = b->cmd;
		if (uid)
			*uid = b->uid;
		if (gid)
			*gid = b->gid;
		if (nic)
			*nic = b->nic;
	}
	if ((!b) || (b->n == NULL))
		job_queue_end = NULL;
	Free(b);
	return s;
}


/* maybe_atoi - (s =~ /^-?[0-9]+$/) && atoi(s) || x */
int
maybe_atoi(const char *s, int x)
{
	if (!s)
		return x;
	const char *z = s;
	while (*z) {
		if (!strchr("-0123456789", *z))
			return x;
		z++;
	}
	return atoi(s);
}
