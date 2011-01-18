%option noyywrap
%{
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#define _GNU_SOURCE
#include <getopt.h>

#include "options.h"
#include "version.h"

char *configfile = NULL;
char *pidfile = NULL;
char *host = NULL;
int port = -1;
char *user = NULL;
char *pass = NULL;
char *dbnm = NULL;
char *table = NULL;
int allow_root = -1;
int allow_uidgid = -1;
int force_uid = -1;
int force_gid = -1;
char *force_hostname = NULL;
int ignore_machine = -1;
int allow_notnice = -1;
char *force_shell = NULL;
int force_shell_die = -1;
int debug = -1;
int reload = -1;
int max_jobs = -1;

#define sop(s,c) \
	if (!s) { \
		char *foo = yytext + c; \
		while ((*foo == ' ') || (*foo == '=') || (*foo == '"')) \
			foo++; \
		if (strlen(foo) >= 1) { \
			s = malloc(strlen(foo) + 1); \
			if (!s) { \
				perror("malloc"); \
				exit(1); \
			} \
			strcpy(s, foo); \
			char *bar = strchr(s, '"'); \
			if (bar) \
				*bar = 0; \
		} \
	}

#define iop(s,c) \
	if (s < 0) { \
		char *foo = yytext + c; \
		while ((*foo == ' ') || (*foo == '=') || (*foo == '"')) \
			foo++; \
 		if (strlen(foo) > 0) \
			s = atoi(foo); \
	}

#define bop(s,c) \
	if (s < 0) { \
		char *foo = yytext + c; \
		while ((*foo == ' ') || (*foo == '=') || (*foo == '"')) \
			foo++; \
		if (strlen(foo) > 0) \
	 		s = unbool(foo); \
	}

int
unbool(char *s)
{
	if (s == NULL)
		return 1;
	char *bar = strchr(s, '"');
	if (bar) 
		*bar = 0; 
	for (bar = s; *bar; bar++)
		if (isupper(*bar))
			*bar = tolower(*bar);

	if (!strcmp(s, "1"))
		return 1;
	if (!strcmp(s, "0"))
		return 0;
	if (!strcmp(s, "yes"))
		return 1;
	if (!strcmp(s, "no"))
		return 0;
	if (!strcmp(s, "true"))
		return 1;
	if (!strcmp(s, "false"))
		return 0;
	fprintf(stderr, "hcrond: bad bool %s\n - assuming default\n", s);
	return -1;
}

%}
%%
\#.*			/* comment */
configfile.*		/* ignored */	
help			/* ignored */
version			/* ignored */
pidfile.*		sop(pidfile, 7);
host.*			sop(host, 4);
port.*			iop(port, 4);
user.*			sop(user, 4);
pass.*			sop(pass, 4);
dbnm.*			sop(dbnm, 4);
table.*			sop(table, 5);
allow_root.*		bop(allow_root, 10);
allow_uidgid.*		bop(allow_uidgid, 12);
force_uid.*		iop(force_uid, 9);
force_gid.*		iop(force_gid, 9);
force_hostname.*	sop(force_hostname, 14);
ignore_machine.*	bop(ignore_machine, 14);
allow_notnice.*		bop(allow_notnice, 13);
force_shell[^_].*	sop(force_shell, 11);
force_shell_die.*	bop(force_shell_die, 15);
reload.*		iop(reload, 6);
debug.*			bop(debug, 5);
max_jobs.*		iop(max_jobs, 8);
\r?\n			/* eol ok */
[ \t]+			/* whitespace ignored */
.*			fprintf(stderr, "invalid input: %s\n", yytext);
%%
void
version()
{
	printf("hcrond %s\n" "Copyright (C) 2006-2011 Martin Hradil <himdel@seznam.cz>\n"
		"This is free software.  You may redistribute copies of it under the terms of\n"
		"the GNU General Public License <http://www.gnu.org/licenses/gpl.html>.\n"
		"There is NO WARRANTY, to the extent permitted by law.\n", VERSION);
	exit(0);
}

#include "help.c"

int
optmain(int argc, char **argv)
{
	/* getopt */
        struct option lo[] = {
                { "configfile",		1,	NULL,   'c' },
                { "pidfile",		1,	NULL,   'P' },
                { "host",		1,	NULL,	'h' },
                { "port",		1,	NULL,	'o' },
                { "user",		1,	NULL,	'u' },
                { "pass",		1,	NULL,	'p' },
                { "dbnm",		1,	NULL,	'd' },
                { "table",		1,	NULL,	't' },
                { "allow_root",		2,	NULL,	'R' },
                { "allow_uidgid",	2,	NULL,	'g' },
                { "force_uid",		1,	NULL,	'U' },
                { "force_gid",		1,	NULL,	'G' },
                { "force_hostname",	1,	NULL,	'H' },
                { "ignore_machine",	2,	NULL,	'i' },
                { "allow_notnice",	2,	NULL,	'n' },
                { "force_shell",	1,	NULL,	's' },
                { "force_shell_die",	2,	NULL,	'D' },
                { "reload",		1,	NULL,	'r' },
                { "debug",		2,	NULL,	'e' },
		{ "max_jobs",		1,	NULL,	'j' },
                { "version",		0,	NULL,   'V' },
                { "help",		0,	NULL,   0 },
                { NULL,			0,	NULL,   0 }
        };
	
#define cp(s) \
 s = malloc(strlen(optarg) + 1); \
 if (!s) { \
  perror("malloc"); \
  exit(1); \
 } \
 strcpy(s, optarg);

	int c;
	while ((c = getopt_long(argc, argv, "c:P:h:o:u:p:d:t:R::g::U:G:H:i::n::s:D::r:e::j:V", lo, NULL)) != -1) {
                switch(c) {
			case 'c':
				cp(configfile);
				break;
			case 'P':
				cp(pidfile);
				break;
			case 'h':
				cp(host);
				break;
			case 'o':
				port = atoi(optarg);
				break;
			case 'u':
				cp(user);
				break;
			case 'p':
				cp(pass);
				break;
			case 'd':
				cp(dbnm);
				break;
			case 't':
				cp(table);
				break;
			case 'R':
				allow_root = unbool(optarg);
				break;
			case 'g':
				allow_uidgid = unbool(optarg);
				break;
			case 'U':
				force_uid = atoi(optarg);
				break;
			case 'G':
				force_gid = atoi(optarg);
				break;
			case 'H':
				cp(force_hostname);
				break;
			case 'i':
				ignore_machine = unbool(optarg);
				break;
			case 'n':
				allow_notnice = unbool(optarg);
				break;
			case 's':
				cp(force_shell);
				break;
			case 'D':
				force_shell_die = unbool(optarg);
				break;
			case 'j':
				max_jobs = atoi(optarg);
				break;
			case 'r':
				reload = atoi(optarg);
				break;
			case 'e':
				debug = unbool(optarg);
				break;
                        case 'V':
				version();
			default:
				help();
                }
        }

	if (configfile == NULL)
		configfile = "/etc/hcrondrc";

	FILE *f = fopen(configfile, "r");
	if (!f) {
		fprintf(stderr, "warning: config file %s doesn't exist\n", configfile);
	} else {
		yyin = f;
		/* sets only options not yet set; from $configfile */
		yylex();
		fclose(f);
	}
	/* defaults */
	if (!pidfile)
		pidfile = "/var/run/hcrond.pid";
	if (!host)
		host = "127.0.0.1";
	if (port < 0)
		port = 666;
	if (!user)
		user = "evil";
	if (!pass)
		pass = "swordfish";
	if (!dbnm)
		dbnm = "hcrond";
	if (!table)
		table = "crontab";
	if (allow_root < 0)
		allow_root = 0;
	if (allow_uidgid < 0)
		allow_uidgid = 1;
	if (ignore_machine < 0)
		ignore_machine = 0;
	if (allow_notnice < 0)
		allow_notnice = 0;
	if (force_shell_die < 0)
		force_shell_die = 1;
	if (reload < 0)
		reload = 30;
	if (debug < 0)
		debug = 0;
	if (max_jobs < 0)
		max_jobs = 1;
	/* force_{uid,gid,hostname,shell} default to NULL */

	return 0;
}
