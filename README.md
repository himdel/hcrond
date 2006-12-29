hcrond
======

A cron daemon with precision in seconds and crontab in a MySQL table.

-- this is a reconstructed git history of previously unversioned project

-- see README

-- hcrond 0.1-rc1 (~ Jun 31, 2006)
* has license gpl2
* empty changelog
* makefile using flex and libmysqlclient
* readme with opts and db table
* todo list with some bugs
* hcrond sysv initscript
* random locking

-- hcrond 0.1-rc3 (~ Aug 7, 2006)
* locking fixed - phthread & semaphore
* pidfile
* more debug info

-- hcrond-0.1 (~ Aug 8, 2006)
* uses --std=gnu99 because of sigaction
* more TODO
* uses sigaction, doesn't do SIGHUP
* some dbg stuff commented out, why?

-- hcrond-0.1.1 (2011-01-18)
* bugfix:  no jobs got run in January if mon="\*" and andor="&", also mon=1 meant February, not Jan" and andor="&", also mon="1" meant February, not January - fixed now, thanks to Philipp Schreiber for reporting it
* more TODO

-- see ChangeLog for >0.2

