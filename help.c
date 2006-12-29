/* help.c for hcrond */
/* when updating remember to update README too */

void
help()
{
	printf("Usage: hcrond [OPTION]...\n"
		"hcrond is a cron with crontab in mysql\n\n"
		"Mandatory arguments to long options are mandatory for short options too.\n"
		"Bool with no args defaults to true (true, 1, yes)\n"
		"  -c, --configfile\tconfigfile filename (/etc/hcrondrc)\n"
		"  -P, --pidfile\t\tpidfile (/var/run/hcrond.pid)  ## not implemented\n"
		"  -h, --host\t\taddress (IP or hostname) of server running mysql (127.0.0.1)\n"
		"  -o, --port\t\tthe port mysql listens to (666)\n"
		"  -u, --user\t\tusername (evil)\n"
		"  -p, --pass\t\tpassword (swordfish)\n"
		"  -d, --dbnm\t\tdatabase name (hcrond)\n"
		"  -t, --table\t\ttable name (crontab)\n"
		"  -R, --allow_root\tallow running cmd with uid=0 or gid=0 (bool:no)  ## not implemented\n"
		"  -g, --allow_uidgid\tallow running cmd with different uid && gid (bool:yes)  ## not implemented\n"
		"  -U, --force_uid\trun everything with this uid or username  ## not implemented\n"
		"  -G, --force_gid\trun everything with this gid or groupname  ## not implemented\n"
		"  -H, --force_hostname\tuse this hostname for comparison with `machine' field  ## not implemented\n"
		"  -i, --ignore_machine\tignore the `machine' field (bool:no)  ## not implemented - always true\n"
		"  -n, --allow_notnice\tallow negative nice values (bool:no) ## not implemented\n"
		"  -s, --force_shell\talways use this shell  ## not implemented\n"
		"  -D, --force_shell_die\tdie if forced shell different from `sh' (bool:yes)  ## not implemented\n"
		"  -r, --reload\t\treload from table every n secs (30)\n"
		"  -j, --max-jobs\t\trun only n jobs at a time; 0 means infinity (1)\n"
		"  -e, --debug\t\tdebug mode (no detach and more verbosity) (bool:no)\n"
		"  -k, --kill\t\tkill already running hcrond\n"
		"  --help\t\tdisplay this help and exit\n"
		"  -V, --version\t\toutput version information and exit\n\n"
		"Report bugs to <himdel@seznam.cz>.\n");
	exit(0);
}
