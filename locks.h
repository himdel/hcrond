#define LOCKED(x) { lock_wait(); x; lock_unlock(); }
#define LOCK lock_wait()
#define UNLOCK lock_unlock()

/*
#define LOCKED(x) { dbg("LOCKED_IN (line %d)\n", __LINE__); lock_wait(); x; lock_unlock(); dbg("LOCKED_OUT"); }
#define LOCK	{ dbg("LOCK_WAIT_IN (line %d)\n", __LINE__); lock_wait(); dbg("LOCK_WAIT_OUT"); }
#define UNLOCK	{ dbg("LOCK_UNLOCK_IN (line %d)\n", __LINE__); lock_unlock(); dbg("LOCK_UNLOCK_OUT"); }
*/
extern void lock_wait();
extern void lock_unlock();
extern void lock_init();
extern void lock_kill();
