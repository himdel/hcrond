#include <syslog.h>

#define dbg(...) { if (debug) daemon_log(LOG_DEBUG, __VA_ARGS__); }
#define inf(...) daemon_log(LOG_INFO, __VA_ARGS__)
#define wrn(...) daemon_log(LOG_WARNING, __VA_ARGS__)
#define err(...) daemon_log(LOG_ERR, __VA_ARGS__)
