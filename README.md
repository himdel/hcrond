hcrond
======

A cron daemon with precision in seconds and crontab in a MySQL table.

 * table:
 * id = unique identifier
 * sec = 0..59
 * min = 0..59
 * hour = 0..23
 * day = 1..31
 * mon = 1..12
 * dow = 0..6 (sunday = 0, 7)
 * uid = EUID for cmd
 * gid = EGID for cmd
 * machine = hostname where to run (everywhere when null)
 * cmd = cmd for sh -c
 * name = task name
 * andor = ('&', '|') specifies whether to AND or OR the dow with day&mon
 * nice = nice value
 * lastrun = 0 (used to record time cmd was last run) (is INT!)
 * runonce = we run the job only N times and then delete it (-1 = infinite)
 *
 * standard cron syntax is supported, so "1-5/2" == "1,3,5" = 
 *  = "Mon,Wed,Fri" or "Jan,Mar,May" and * is every
 *
 * TODO negative numbers signify frequency (sec == -2 => run twice per sec)

    Usage: hcrond [OPTION]...
    hcrond is a cron with crontab in mysql

    Mandatory arguments to long options are mandatory for short options too.
    Bool with no args defaults to true (true, 1, yes)

 * -c, --configfile	configfile filename (/etc/hcrondrc)
 * -P, --pidfile		pidfile (/var/run/hcrond.pid)  ## not implemented
 * -h, --host		address (IP or hostname) of server running mysql (127.0.0.1)
 * -o, --port		the port mysql listens to (666)
 * -u, --user		username (evil)
 * -p, --pass		password (swordfish)
 * -d, --dbnm		database name (hcrond)
 * -t, --table		table name (crontab)
 * -R, --allow_root	allow running cmd with uid=0 or gid=0 (bool:no)  ## not implemented
 * -g, --allow_uidgid	allow running cmd with different uid && gid (bool:yes)  ## not implemented
 * -U, --force_uid	run everything with this uid or username  ## not implemented
 * -G, --force_gid	run everything with this gid or groupname  ## not implemented
 * -H, --force_hostname	use this hostname for comparison with `machine' field  ## not implemented
 * -i, --ignore_machine	ignore the `machine' field (bool:no)  ## not implemented - always true
 * -n, --allow_notnice	allow negative nice values (bool:no) ## not implemented
 * -s, --force_shell	always use this shell  ## not implemented
 * -D, --force_shell_die	die if forced shell different from `sh' (bool:yes)  ## not implemented
 * -j, --max-jobs	run only n jobs at a time; 0 means infinity (1)
 * -r, --reload		reload from table every n secs (30)
 * -e, --debug		debug mode (no detach and more verbosity) (bool:no)
 * --help		display this help and exit
 * -V, --version		output version information and exit

Report bugs to <himdel@seznam.cz>.

sql to create table:

    CREATE TABLE `hcrondtab` (
     `id` INT NOT NULL AUTO_INCREMENT PRIMARY KEY ,
     `sec` VARCHAR( 128 ) NULL DEFAULT '*',
     `min` VARCHAR( 128 ) NULL DEFAULT '*',
     `hour` VARCHAR( 128 ) NULL DEFAULT '*',
     `day` VARCHAR( 128 ) NULL DEFAULT '*',
     `mon` VARCHAR( 128 ) NULL DEFAULT '*',
     `dow` VARCHAR( 128 ) NULL DEFAULT '*',
     `uid` VARCHAR( 128 ) NULL ,
     `gid` VARCHAR( 128 ) NULL ,
     `machine` VARCHAR( 128 ) NULL ,
     `cmd` VARCHAR( 512 ) NOT NULL ,
     `name` VARCHAR( 128 ) NOT NULL ,
     `andor` ENUM( '&', '|' ) NOT NULL DEFAULT '|',
     `nice` INT NULL DEFAULT '0',
     `lastrun` INT NULL DEFAULT '0',
     `runonce` INT DEFAULT '-1' NOT NULL
    ) ENGINE = MYISAM ;


Please not that there is a v0.3 branch that contains more current versions, those were reported to segfault occassionally and it's been too long. The version I know people use is v0.1.1.


Copyright 2006-2011 Martin Hradil <himdel@seznam.cz>
Licensed under GPL the terms of which can be found in the file COPYING.
