/* options.h for hcrond */

extern char *configfile;
extern char *pidfile;
extern char *host;
extern int port;
extern char *user;
extern char *pass;
extern char *dbnm;
extern char *table;
extern int allow_root;
extern int allow_uidgid;
extern int force_uid;
extern int force_gid;
extern char *force_hostname;
extern int ignore_machine;
extern int allow_notnice;
extern char *force_shell;
extern int force_shell_die;
extern int debug;
extern int reload;
extern int max_jobs;
extern int kill;

extern int optmain(int argc, char **argv);
