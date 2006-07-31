#include <stdio.h>
#include <stdlib.h>
#include "options.h"

int
main(int argc, char **argv)
{
	optmain(argc, argv);

	printf("configfile = %s\n", configfile);
	printf("pidfile = %s\n", pidfile);
	printf("host = %s\n", host);
	printf("port = %d\n", port);
	printf("user = %s\n", user);
	printf("pass = %s\n", pass);
	printf("dbnm = %s\n", dbnm);
	printf("table = %s\n", table);
	printf("allow_root = %d\n", allow_root);
	printf("allow_uidgid = %d\n", allow_uidgid);
	printf("force_uid = %d\n", force_uid);
	printf("force_gid = %d\n", force_gid);
	printf("force_hostname = %s\n", force_hostname);
	printf("ignore_machine = %d\n", ignore_machine);
	printf("allow_notnice = %d\n", allow_notnice);
	printf("force_shell = %s\n", force_shell);
	printf("force_shell_die = %d\n", force_shell_die);

	return 0;
}
