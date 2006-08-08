#include <syslog.h>

/* note: never run syslog in anything called from a sighandler - hangs */

#define err(s) syslog(LOG_ERR, "hcrond: " s ": %s\n", mysql_error(sql))
#define errf(...) syslog(LOG_ERR, "hcrond: " __VA_ARGS__)
#define warnf(...) syslog(LOG_WARNING, "hcrond: " __VA_ARGS__)
#define inf(...) syslog(LOG_INFO, "hcrond: " __VA_ARGS__)
#define scp(d, s) { (d) = malloc(strlen(s) * sizeof(char) + 1); if ((d)) strcpy((d), (s)); }
#define dbg(...) { if(debug) syslog(LOG_DEBUG, "hcrond: " __VA_ARGS__); }
