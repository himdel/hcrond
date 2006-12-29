/* rscrond - a mysql based cron replacement */
/* original code by logic.cz */

#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <mysql/mysql.h>
#define HOST_NAME_MAX 64

typedef enum {jobAt, jobHourly, jobDaily, jobWeekly, jobMonthly} TJob;
extern char **environ;

const char* TJobPeriod[] = {"at", "hourly", "daily", "weekly", "monhtly"};
const char* TSQLPeriod[] = {"NULL", "ADDDATE(NOW(), INTERVAL 1 HOUR)", "ADDDATE(NOW(), INTERVAL 1 DAY)", "ADDDATE(NOW(), INTERVAL 7 DAY)", "ADDDATE(NOW(), INTERVAL 1 MONTH)"};


void run_jobs(MYSQL *);
void sigchld_handler(int);
int getPeriod(MYSQL *, char *);
MYSQL_RES *run_query(MYSQL *, char *);


int
main(int argc, char **argv)
{
	char *host = "127.0.0.1";
	char *user = "mysql";
	char *pass = "";
	char *dbnm = "db";
	int port = 666;
	
	MYSQL *sql = mysql_init(NULL);
	if (!sql) {
		fprintf(stderr, "rscrond: mysql_init error TODO\n");
		return -1;
	}
	
	if (!mysql_real_connect(sql, host, user, pass, dbnm, port, NULL, 0)) {
		fprintf(stderr, "rscrond: mysql_real_connect error TODO\n");
		return -1;
	}
	
	switch (fork()) {
		case 0:
			setsid();
			struct sigaction sa;
			
			sa.sa_handler = sigchld_handler;
			sigemptyset(&sa.sa_mask);
			sa.sa_flags = SA_RESTART;
			if (sigaction(SIGCHLD, &sa, NULL) == -1) {
				perror("rscrond: sigaction: ");
				return -1;
			}

			for(;;) {
				run_jobs(sql);
				sleep(30);
			}
				
			mysql_close(sql);
			break;
		case -1:
			fprintf(stderr, "rscrond: fork error\n");
			return 1;
		default:
			;
	}
	return 0;
}


MYSQL_RES *
run_query(MYSQL *sql, char *query)
{
	if (mysql_query(sql, query)==0)
		return mysql_store_result(sql);
	else
		return NULL;
}


/*TODO*/
void
run_jobs(MYSQL *sql)
{
	int i;
	char *query = (char *) malloc(256);
	MYSQL_RES *result;
	MYSQL_ROW row;
	
	char hname[HOST_NAME_MAX + 1];
	if (gethostname(hname, HOST_NAME_MAX + 1))
		strcpy(hname, "localhost");

    // select
    sprintf(query, "SELECT * FROM rscrond WHERE next_run <= NOW() AND next_run!='0000-00-00 00:00:00' AND machine='%s'", hname);
    result = run_query(sql, query);
    // go, go, go ... :)
    if(result && result->row_count)
    {
        for(i=0; i<result->row_count; i++)
        {
            row = mysql_fetch_row(result);

	    // reschedule task
	    sprintf(query, "UPDATE rscrond SET next_run=%s WHERE pid=%s", TSQLPeriod[getPeriod(sql, row[4])], row[0]);
	    mysql_free_result(run_query(sql, query));
	    
	    if(!fork())
	    {
		// set privileges ...
		setuid(atoi(row[2]));
		setgid(atoi(row[3]));

		// smash it ... :)
		char *argv[4];
	        argv[0] = "sh";
		argv[1] = "-c";
		argv[2] = row[1];
		argv[3] = 0;
		execve("/bin/sh", argv, environ);
	    }
        }
    }
    // free result    
    mysql_free_result(result);
    
    // free memory
    free(query);
}


int
getPeriod(MYSQL *sql, char *period)
{
	int i;
	for(i = 0; i < 5; i++)
		if (strcmp(period, TJobPeriod[i]) == 0)
			return i;
	
	return -1;
}


void
sigchld_handler(int s)
{
	while (waitpid(-1, NULL, WNOHANG) > 0);
	/* TODO check and report */
}
