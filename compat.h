/* compat.h for hcrond */

extern void usleep(unsigned long usec);
extern char *strdup(const char *s);
extern int fileno(FILE *stream);
extern int nice(int incr);
